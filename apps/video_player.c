/* SwitchFrogUI hardware video player for H.OS / R36SX. */
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <ffplayer.h>

extern unsigned char fontdata8x8[64 * 16];

#define KEY_SELECT (1u << 0)
#define KEY_START  (1u << 3)
#define KEY_UP     (1u << 4)
#define KEY_RIGHT  (1u << 5)
#define KEY_DOWN   (1u << 6)
#define KEY_LEFT   (1u << 7)
#define KEY_L1     (1u << 10)
#define KEY_R1     (1u << 11)
#define KEY_A      (1u << 13)
#define KEY_B      (1u << 14)
#define KEY_FN     (1u << 16)
#define ROTATE_CHORD (KEY_FN | KEY_L1 | KEY_R1)
#define MAX_EXTERNAL_SUBS 16
#define SUBTITLE_OFFSET_FILE "/mnt/sdcard/frogui/video_subtitle_offsets.txt"
#define SUBTITLE_OFFSET_TMP  "/mnt/sdcard/frogui/video_subtitle_offsets.tmp"
#define SUBTITLE_OFFSET_LIMIT_MS 60000
#define ROTATION_FILE "/mnt/sdcard/frogui/screen_rotation.cfg"
#define ROTATION_TMP  "/mnt/sdcard/frogui/screen_rotation.tmp"

/* H.OS's Linux 4.4 dynamic loader crashes before application code when the
 * vendor player is a load-time dependency.  Keep this module dependent only
 * on the standard runtime and resolve the stock media API explicitly after
 * the tiny launcher has entered main().  The log then identifies the exact
 * dlopen/dlsym boundary if the firmware rejects a vendor component. */
static void *ffplayer_library;
static int (*fp_hcplayer_init)(HCPlayerLogLevel);
static void (*fp_hcplayer_deinit)(void);
static void *(*fp_hcplayer_create)(HCPlayerInitArgs *);
static void (*fp_hcplayer_stop2)(void *, bool, bool);
static void (*fp_hcplayer_play)(void *);
static void (*fp_hcplayer_pause)(void *);
static void (*fp_hcplayer_resume)(void *);
static int (*fp_hcplayer_seek)(void *, int64_t);
static int64_t (*fp_hcplayer_get_duration)(void *);
static int64_t (*fp_hcplayer_get_position)(void *);
static int (*fp_hcplayer_get_cur_video_stream_info)(void *, HCPlayerVideoInfo *);
static int (*fp_hcplayer_set_display_rect)(void *, struct vdec_dis_rect *);
static int (*fp_hcplayer_change_rotate_type)(void *, rotate_type_e);

#define hcplayer_init fp_hcplayer_init
#define hcplayer_deinit fp_hcplayer_deinit
#define hcplayer_create fp_hcplayer_create
#define hcplayer_stop2 fp_hcplayer_stop2
#define hcplayer_play fp_hcplayer_play
#define hcplayer_pause fp_hcplayer_pause
#define hcplayer_resume fp_hcplayer_resume
#define hcplayer_seek fp_hcplayer_seek
#define hcplayer_get_duration fp_hcplayer_get_duration
#define hcplayer_get_position fp_hcplayer_get_position
#define hcplayer_get_cur_video_stream_info fp_hcplayer_get_cur_video_stream_info
#define hcplayer_set_display_rect fp_hcplayer_set_display_rect
#define hcplayer_change_rotate_type fp_hcplayer_change_rotate_type

static bool load_ffplayer(void) {
    const char *path = "/mnt/sdcard/rootfs/usr/lib/libffplayer.so";
    fprintf(stderr, "video_player: dlopen %s\n", path);
    dlerror();
    ffplayer_library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!ffplayer_library) {
        fprintf(stderr, "video_player: dlopen failed: %s\n", dlerror());
        return false;
    }
#define LOAD_PLAYER_SYMBOL(name) do { \
    *(void **)(&fp_##name) = dlsym(ffplayer_library, #name); \
    if (!fp_##name) { \
        fprintf(stderr, "video_player: missing %s: %s\n", #name, dlerror()); \
        dlclose(ffplayer_library); ffplayer_library = NULL; return false; \
    } \
} while (0)
    LOAD_PLAYER_SYMBOL(hcplayer_init);
    LOAD_PLAYER_SYMBOL(hcplayer_deinit);
    LOAD_PLAYER_SYMBOL(hcplayer_create);
    LOAD_PLAYER_SYMBOL(hcplayer_stop2);
    LOAD_PLAYER_SYMBOL(hcplayer_play);
    LOAD_PLAYER_SYMBOL(hcplayer_pause);
    LOAD_PLAYER_SYMBOL(hcplayer_resume);
    LOAD_PLAYER_SYMBOL(hcplayer_seek);
    LOAD_PLAYER_SYMBOL(hcplayer_get_duration);
    LOAD_PLAYER_SYMBOL(hcplayer_get_position);
    LOAD_PLAYER_SYMBOL(hcplayer_get_cur_video_stream_info);
    LOAD_PLAYER_SYMBOL(hcplayer_set_display_rect);
    LOAD_PLAYER_SYMBOL(hcplayer_change_rotate_type);
#undef LOAD_PLAYER_SYMBOL
    fprintf(stderr, "video_player: vendor player API loaded\n");
    return true;
}

static void unload_ffplayer(void) {
    if (ffplayer_library) dlclose(ffplayer_library);
    ffplayer_library = NULL;
}

typedef struct {
    int fd;
    uint32_t *pixels;
    uint32_t *back;
    size_t size, frame_size;
    int width, height, stride;
} Overlay;

static Overlay osd = {-1, NULL, NULL, 0, 0, 0, 0, 0};
static pthread_mutex_t subtitle_lock = PTHREAD_MUTEX_INITIALIZER;
static char subtitle_text[1024];
static bool subtitle_visible;
static int64_t subtitle_start_ms;
static int64_t subtitle_end_ms;
static int subtitle_offset_ms;
static bool screen_rotation_180;
static bool osd_scanout_enabled;

typedef struct {
    int64_t start_ms, end_ms;
    char text[1024];
} TimedCue;

static TimedCue *local_cues;
static int local_cue_count;
static int local_cue_index;

static int64_t clock_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void screen_rotation_load(void) {
    char value[24] = {0};
    FILE *file = fopen(ROTATION_FILE, "rb");
    screen_rotation_180 = false;
    if (!file) return;
    if (fgets(value, sizeof(value), file))
        screen_rotation_180 = atoi(value) == 180;
    fclose(file);
}

static void screen_rotation_save(void) {
    FILE *file = fopen(ROTATION_TMP, "wb");
    if (!file) return;
    fprintf(file, "%d\n", screen_rotation_180 ? 180 : 0);
    fflush(file);
    fsync(fileno(file));
    fclose(file);
    rename(ROTATION_TMP, ROTATION_FILE);
}

static volatile uint32_t *attach_keys(void) {
    key_t key = ftok("/tmp/joy_key", 'a');
    if (key == (key_t)-1) return NULL;
    int id = shmget(key, 4, 0666);
    if (id < 0) return NULL;
    void *p = shmat(id, NULL, 0);
    return p == (void *)-1 ? NULL : (volatile uint32_t *)p;
}

static bool terminal_message(long type) {
    return type == HCPLAYER_MSG_STATE_EOS ||
           type == HCPLAYER_MSG_STATE_TRICK_EOS ||
           type == HCPLAYER_MSG_OPEN_FILE_FAILED ||
           type == HCPLAYER_MSG_UNSUPPORT_FORMAT ||
           type == HCPLAYER_MSG_ERR_UNDEFINED;
}

/* H.OS's stock video UI blanks its framebuffer so the decoder's MAIN video
 * plane is visible underneath the transparent controls layer.  FrogUI exits
 * cleanly before this process starts, but its final file-list frame remains in
 * scanout until explicitly blanked. */
static void hide_frontend_framebuffer(void) {
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "video_player: could not open fb0 for blanking: %s\n",
                strerror(errno));
        return;
    }
    int result = ioctl(fd, FBIOBLANK, FB_BLANK_NORMAL);
    fprintf(stderr, "video_player: fb0 blank result=%d errno=%d\n",
            result, result < 0 ? errno : 0);
    close(fd);
}

static void overlay_clear(void) {
    if (osd.back) memset(osd.back, 0, osd.frame_size);
}

static void overlay_set_scanout(bool visible) {
    if (osd.fd < 0 || osd_scanout_enabled == visible) return;
    int request = visible ? FB_BLANK_UNBLANK : FB_BLANK_NORMAL;
    int result = ioctl(osd.fd, FBIOBLANK, request);
    if (result == 0) osd_scanout_enabled = visible;
    fprintf(stderr, "video_player: OSD scanout %s result=%d errno=%d\n",
            visible ? "on" : "off", result, result < 0 ? errno : 0);
}

static void overlay_present(void) {
    if (!osd.pixels || !osd.back) return;
    if (!screen_rotation_180) {
        memcpy(osd.pixels, osd.back, osd.frame_size);
        return;
    }
    memset(osd.pixels, 0, osd.frame_size);
    for (int y = 0; y < osd.height; y++)
        for (int x = 0; x < osd.width; x++)
            osd.pixels[(size_t)(osd.height - 1 - y) * osd.stride +
                       (osd.width - 1 - x)] =
                osd.back[(size_t)y * osd.stride + x];
}

static void overlay_close(void) {
    if (osd.pixels) {
        memset(osd.pixels, 0, osd.frame_size);
        overlay_set_scanout(false);
        munmap(osd.pixels, osd.size);
    }
    free(osd.back);
    if (osd.fd >= 0) close(osd.fd);
    osd.fd = -1; osd.pixels = NULL; osd.back = NULL;
    osd.size = 0; osd.frame_size = 0;
    osd_scanout_enabled = false;
}

static void overlay_open(void) {
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    memset(&fix, 0, sizeof(fix)); memset(&var, 0, sizeof(var));
    osd.fd = open("/dev/fb1", O_RDWR);
    if (osd.fd < 0 || ioctl(osd.fd, FBIOGET_FSCREENINFO, &fix) < 0 ||
        ioctl(osd.fd, FBIOGET_VSCREENINFO, &var) < 0 ||
        var.bits_per_pixel != 32 || fix.smem_len == 0) {
        overlay_close(); return;
    }
    osd.size = fix.smem_len;
    osd.width = (int)var.xres; osd.height = (int)var.yres;
    osd.stride = (int)fix.line_length / 4;
    osd.frame_size = (size_t)fix.line_length * osd.height;
    if (osd.frame_size > osd.size) osd.frame_size = osd.size;
    osd.pixels = mmap(NULL, osd.size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, osd.fd, 0);
    if (osd.pixels == MAP_FAILED) { osd.pixels = NULL; overlay_close(); return; }
    osd.back = calloc(1, osd.frame_size);
    if (!osd.back) { overlay_close(); return; }
    memset(osd.pixels, 0, osd.frame_size);
    overlay_clear();
    /* A transparent OSD still consumes a full 32-bit scanout layer. Keep the
     * layer physically blanked until controls or a subtitle are visible. */
    osd_scanout_enabled = true; /* force the first transition ioctl */
    overlay_set_scanout(false);
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!osd.back || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x + w > osd.width) w = osd.width - x;
    if (y + h > osd.height) h = osd.height - y;
    for (int yy = 0; yy < h; yy++) {
        uint32_t *row = osd.back + (y + yy) * osd.stride + x;
        for (int xx = 0; xx < w; xx++) row[xx] = color;
    }
}

static void draw_text(int x, int y, int scale, const char *text, uint32_t color) {
    if (!osd.back || !text) return;
    int origin = x;
    for (; *text; text++) {
        unsigned ch = (unsigned char)*text;
        if (ch == '\n') { x = origin; y += 10 * scale; continue; }
        if (ch < 32 || ch > 127) ch = '?';
        const unsigned char *glyph = &fontdata8x8[ch * 8];
        for (int gy = 0; gy < 8; gy++) for (int gx = 0; gx < 8; gx++)
            if (glyph[gy] & (0x80u >> gx))
                fill_rect(x + gx * scale, y + gy * scale, scale, scale, color);
        x += 8 * scale;
        if (x + 8 * scale >= osd.width) { x = origin; y += 10 * scale; }
    }
}

static void format_time(char *out, size_t n, int64_t ms) {
    if (ms < 0) ms = 0;
    int sec = (int)(ms / 1000);
    snprintf(out, n, "%d:%02d:%02d", sec / 3600, (sec / 60) % 60, sec % 60);
}

static int subtitle_offset_load(const char *video_path) {
    FILE *f = fopen(SUBTITLE_OFFSET_FILE, "r");
    if (!f) return 0;
    char line[1200]; int result = 0;
    while (fgets(line, sizeof(line), f)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab++ = '\0'; tab[strcspn(tab, "\r\n")] = '\0';
        if (!strcmp(tab, video_path)) result = atoi(line);
    }
    fclose(f);
    if (result < -SUBTITLE_OFFSET_LIMIT_MS) result = -SUBTITLE_OFFSET_LIMIT_MS;
    if (result >  SUBTITLE_OFFSET_LIMIT_MS) result =  SUBTITLE_OFFSET_LIMIT_MS;
    return result - result % 100;
}

static void subtitle_offset_save(const char *video_path, int offset_ms) {
    mkdir("/mnt/sdcard/frogui", 0777);
    FILE *src = fopen(SUBTITLE_OFFSET_FILE, "r");
    FILE *dst = fopen(SUBTITLE_OFFSET_TMP, "w");
    if (!dst) { if (src) fclose(src); return; }
    char line[1200];
    while (src && fgets(line, sizeof(line), src)) {
        char copy[1200];
        strncpy(copy, line, sizeof(copy) - 1); copy[sizeof(copy) - 1] = '\0';
        char *tab = strchr(copy, '\t');
        if (tab) {
            *tab++ = '\0'; tab[strcspn(tab, "\r\n")] = '\0';
            if (!strcmp(tab, video_path)) continue;
        }
        fputs(line, dst);
    }
    if (src) fclose(src);
    /* Zero is the default, so removing its row keeps the file compact. */
    if (offset_ms != 0) fprintf(dst, "%d\t%s\n", offset_ms, video_path);
    fflush(dst); fsync(fileno(dst)); fclose(dst);
    if (rename(SUBTITLE_OFFSET_TMP, SUBTITLE_OFFSET_FILE) == 0) sync();
    else unlink(SUBTITLE_OFFSET_TMP);
}

static void copy_subtitle_text(char *dst, size_t n, const char *src) {
    size_t j = 0;
    if (!src || !n) return;
    for (size_t i = 0; src[i] && j + 1 < n; i++) {
        if (src[i] == '{') { while (src[i] && src[i] != '}') i++; continue; }
        if (src[i] == '<') { while (src[i] && src[i] != '>') i++; continue; }
        if (src[i] == '\\' && (src[i + 1] == 'N' || src[i + 1] == 'n')) {
            dst[j++] = '\n'; i++; continue;
        }
        if (src[i] != '\r') dst[j++] = src[i];
    }
    dst[j] = '\0';
}

static bool extension_is_subtitle(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && (!strcasecmp(dot, ".srt") || !strcasecmp(dot, ".vtt"));
}

static int find_sidecar_subtitles(const char *video, char **uris, int capacity) {
    char directory[512], prefix[256];
    const char *slash = strrchr(video, '/');
    const char *base = slash ? slash + 1 : video;
    size_t dir_len = slash ? (size_t)(slash - video) : 1;
    if (dir_len >= sizeof(directory)) return 0;
    if (slash) { memcpy(directory, video, dir_len); directory[dir_len] = '\0'; }
    else strcpy(directory, ".");
    strncpy(prefix, base, sizeof(prefix) - 1); prefix[sizeof(prefix) - 1] = '\0';
    char *dot = strrchr(prefix, '.'); if (dot) *dot = '\0';
    size_t prefix_len = strlen(prefix);
    DIR *dir = opendir(directory); if (!dir) return 0;
    int count = 0; struct dirent *entry;
    char selected[512] = {0};
    while (capacity > 0 && (entry = readdir(dir)) != NULL) {
        const char *extension = strrchr(entry->d_name, '.');
        if (!extension || !extension_is_subtitle(entry->d_name) ||
            (size_t)(extension - entry->d_name) != prefix_len ||
            strncasecmp(entry->d_name, prefix, prefix_len) != 0) continue;
        /* Accept only Video.srt/Video.vtt, never Video.en.srt.  Prefer SRT if
         * both exact-name formats happen to exist. */
        if (!selected[0] || !strcasecmp(extension, ".srt")) {
            strncpy(selected, entry->d_name, sizeof(selected) - 1);
            selected[sizeof(selected) - 1] = '\0';
            if (!strcasecmp(extension, ".srt")) break;
        }
    }
    if (selected[0]) {
        size_t needed = strlen(directory) + strlen(selected) + 2;
        uris[count] = malloc(needed);
        if (uris[count]) {
            snprintf(uris[count], needed, "%s/%s", directory, selected);
            count++;
        }
    }
    closedir(dir);
    return count;
}

static void local_subtitles_clear(void) {
    free(local_cues);
    local_cues = NULL;
    local_cue_count = 0;
    local_cue_index = 0;
    pthread_mutex_lock(&subtitle_lock);
    subtitle_text[0] = '\0';
    subtitle_visible = false;
    pthread_mutex_unlock(&subtitle_lock);
}

static int64_t parse_subtitle_time(const char *text) {
    int h = 0, m = 0, s = 0, ms = 0;
    char separator = 0;
    while (*text == ' ' || *text == '\t') text++;
    if (sscanf(text, "%d:%d:%d%c%d", &h, &m, &s, &separator, &ms) == 5 &&
        (separator == ',' || separator == '.'))
        return ((int64_t)h * 3600 + m * 60 + s) * 1000 + ms;
    h = 0;
    if (sscanf(text, "%d:%d%c%d", &m, &s, &separator, &ms) == 4 &&
        (separator == ',' || separator == '.'))
        return ((int64_t)m * 60 + s) * 1000 + ms;
    return -1;
}

/* Parse loose SRT/WebVTT text ourselves.  Passing an external subtitle URI to
 * the stock H.OS 1.2 ffplayer causes a decoder-thread SIGSEGV on this device;
 * local timed text keeps subtitles and persistent offset control without
 * entering that incompatible ABI path. */
static int local_subtitles_load(const char *path) {
    local_subtitles_clear();
    if (!path) return 0;
    const char *dot = strrchr(path, '.');
    if (!dot || (strcasecmp(dot, ".srt") && strcasecmp(dot, ".vtt"))) {
        fprintf(stderr, "video_player: safe subtitle parser does not support %s\n", path);
        return 0;
    }
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[1200];
    while (fgets(line, sizeof(line), f)) {
        char *arrow = strstr(line, "-->");
        if (!arrow) continue;
        *arrow = '\0';
        int64_t start = parse_subtitle_time(line);
        int64_t end = parse_subtitle_time(arrow + 3);
        if (start < 0 || end <= start) continue;

        char joined[1024] = {0};
        size_t used = 0;
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (!line[0]) break;
            char clean[1024] = {0};
            copy_subtitle_text(clean, sizeof(clean), line);
            size_t length = strlen(clean);
            if (!length) continue;
            if (used && used + 1 < sizeof(joined)) joined[used++] = '\n';
            if (length > sizeof(joined) - used - 1)
                length = sizeof(joined) - used - 1;
            memcpy(joined + used, clean, length);
            used += length;
            joined[used] = '\0';
        }
        if (!joined[0]) continue;
        TimedCue *next = realloc(local_cues,
            (size_t)(local_cue_count + 1) * sizeof(*local_cues));
        if (!next) break;
        local_cues = next;
        TimedCue *cue = &local_cues[local_cue_count++];
        cue->start_ms = start; cue->end_ms = end;
        strncpy(cue->text, joined, sizeof(cue->text) - 1);
        cue->text[sizeof(cue->text) - 1] = '\0';
    }
    fclose(f);
    fprintf(stderr, "video_player: locally parsed %d cues from %s\n",
            local_cue_count, path);
    return local_cue_count;
}

static void local_subtitles_update(int64_t position_ms, int offset_ms, bool enabled) {
    int64_t cue_time = position_ms - offset_ms;
    pthread_mutex_lock(&subtitle_lock);
    if (!enabled || !local_cues || local_cue_count <= 0) {
        subtitle_visible = false;
        pthread_mutex_unlock(&subtitle_lock);
        return;
    }
    if (local_cue_index >= local_cue_count ||
        cue_time < local_cues[local_cue_index].start_ms)
        local_cue_index = 0;
    while (local_cue_index < local_cue_count &&
           cue_time >= local_cues[local_cue_index].end_ms)
        local_cue_index++;
    if (local_cue_index < local_cue_count &&
        cue_time >= local_cues[local_cue_index].start_ms) {
        TimedCue *cue = &local_cues[local_cue_index];
        strncpy(subtitle_text, cue->text, sizeof(subtitle_text) - 1);
        subtitle_text[sizeof(subtitle_text) - 1] = '\0';
        subtitle_start_ms = cue->start_ms;
        subtitle_end_ms = cue->end_ms;
        subtitle_visible = true;
    } else {
        subtitle_visible = false;
    }
    pthread_mutex_unlock(&subtitle_lock);
}

static const char *scale_name(int mode) {
    static const char *names[] = {"FIT", "FILL", "STRETCH", "ORIGINAL"};
    return names[mode & 3];
}

static void apply_display_mode(void *player, int mode, int vw, int vh) {
    /* hcplayer's vdec rectangle ABI is the projector's normalized 1920x1080
     * coordinate space, not physical panel pixels.  Compute the user-visible
     * geometry against the R36SX's 640x480 panel, then map it to that ABI.
     * Passing 640x480 directly makes the decoder allocate only the upper-right
     * fraction of its hardware plane (roughly the 100px square seen on device). */
    const int sw = 640, sh = 480;
    const int nw = 1920, nh = 1080;
    int px = 0, py = 0, pw = sw, ph = sh;
    int sx = 0, sy = 0, snw = nw, snh = nh;
    if (vw <= 0 || vh <= 0) { vw = sw; vh = sh; }
    if (mode == 0) { /* fit */
        pw = sw; ph = pw * vh / vw;
        if (ph > sh) { ph = sh; pw = ph * vw / vh; }
        px = (sw - pw) / 2; py = (sh - ph) / 2;
    } else if (mode == 1) { /* crop source to fill */
        if ((int64_t)vw * sh > (int64_t)vh * sw) {
            snw = (int)((int64_t)nh * sw / sh);
            sx = (nw - snw) / 2;
        } else {
            snh = (int)((int64_t)nw * sh / sw);
            sy = (nh - snh) / 2;
        }
    } else if (mode == 3) { /* one source pixel per output coordinate */
        pw = vw; ph = vh;
        if (pw > sw || ph > sh) {
            if ((int64_t)pw * sh > (int64_t)ph * sw) {
                ph = (int)((int64_t)sw * ph / pw); pw = sw;
            } else {
                pw = (int)((int64_t)sh * pw / ph); ph = sh;
            }
        }
        px = (sw - pw) / 2; py = (sh - ph) / 2;
    }
    struct vdec_dis_rect rect = {
        {(uint16_t)sx, (uint16_t)sy, (uint16_t)snw, (uint16_t)snh},
        {(uint16_t)((int64_t)px * nw / sw),
         (uint16_t)((int64_t)py * nh / sh),
         (uint16_t)((int64_t)pw * nw / sw),
         (uint16_t)((int64_t)ph * nh / sh)}
    };
    hcplayer_set_display_rect(player, &rect);
    fprintf(stderr, "video_player: display mode=%s video=%dx%d physical=%dx%d+%d+%d normalized=%dx%d+%d+%d src=%dx%d+%d+%d\n",
            scale_name(mode), vw, vh, pw, ph, px, py,
            rect.dst_rect.w, rect.dst_rect.h, rect.dst_rect.x, rect.dst_rect.y,
            rect.src_rect.w, rect.src_rect.h, rect.src_rect.x, rect.src_rect.y);
}

static bool render_overlay(void *player, bool paused, int menu_index,
                           int scale_mode, bool subtitle_available,
                           bool subtitles_enabled, int offset_ms,
                           int64_t controls_until) {
    if (!osd.pixels || !osd.back) return false;
    overlay_clear();
    bool content_visible = false;
    int text_scale = osd.width >= 900 ? 2 : 1;
    int64_t render_time = clock_ms();
    bool controls_visible = render_time < controls_until;
    int64_t pos = hcplayer_get_position(player);
    int64_t dur = (paused || controls_visible) ?
                  hcplayer_get_duration(player) : 0;
    local_subtitles_update(pos, offset_ms, subtitles_enabled);
    pthread_mutex_lock(&subtitle_lock);
    if (!paused && subtitle_visible && subtitles_enabled &&
        pos >= subtitle_start_ms + offset_ms &&
        pos < subtitle_end_ms + offset_ms) {
        char local[sizeof(subtitle_text)];
        strncpy(local, subtitle_text, sizeof(local) - 1); local[sizeof(local) - 1] = '\0';
        int chars = osd.width / (8 * text_scale) - 6;
        if (chars < 20) chars = 20;
        char wrapped[1200]; int col = 0, j = 0;
        for (int i = 0; local[i] && j + 2 < (int)sizeof(wrapped); i++) {
            if ((col >= chars && local[i] == ' ') || local[i] == '\n') { wrapped[j++] = '\n'; col = 0; }
            else { wrapped[j++] = local[i]; col++; }
        }
        wrapped[j] = '\0';
        int lines = 1; for (int i = 0; wrapped[i]; i++) if (wrapped[i] == '\n') lines++;
        int box_h = lines * 10 * text_scale + 12 * text_scale;
        int y = osd.height - box_h - (controls_visible ? 52 * text_scale : 18 * text_scale);
        fill_rect(12 * text_scale, y, osd.width - 24 * text_scale, box_h, 0xB0000000u);
        draw_text(20 * text_scale, y + 6 * text_scale, text_scale, wrapped, 0xFFFFFFFFu);
        content_visible = true;
    }
    pthread_mutex_unlock(&subtitle_lock);
    if (paused) {
        int panel_w = osd.width >= 540 ? 500 : osd.width - 36;
        int panel_h = 330;
        int x = (osd.width - panel_w) / 2;
        int y = (osd.height - panel_h) / 2;
        fill_rect(x, y, panel_w, panel_h, 0xE0101520u);
        fill_rect(x, y, panel_w, 4, 0xFF48D8FFu);
        draw_text(x + 22, y + 20, 2, "PAUSED", 0xFFFFFFFFu);

        char p[16], d[16], line[96];
        format_time(p, sizeof p, pos); format_time(d, sizeof d, dur);
        snprintf(line, sizeof line, "%s / %s", p, d);
        draw_text(x + 22, y + 52, 1, line, 0xFFB9C7D8u);
        int bar_x = x + 22, bar_y = y + 68, bar_w = panel_w - 44;
        fill_rect(bar_x, bar_y, bar_w, 4, 0xFF596273u);
        if (dur > 0) fill_rect(bar_x, bar_y,
            (int)((int64_t)bar_w * pos / dur), 4, 0xFF48D8FFu);

        const char *labels[] = {"RESUME", "VIDEO SIZE", "SUBTITLES",
                                "SUBTITLE TIMING", "EXIT VIDEO"};
        for (int i = 0; i < 5; i++) {
            int row_y = y + 91 + i * 38;
            if (i == menu_index) fill_rect(x + 14, row_y - 8, panel_w - 28, 31, 0xFF263748u);
            draw_text(x + 24, row_y, 1, i == menu_index ? ">" : " ", 0xFF48D8FFu);
            draw_text(x + 42, row_y, 1, labels[i], 0xFFFFFFFFu);
            if (i == 1) draw_text(x + panel_w - 118, row_y, 1,
                                  scale_name(scale_mode), 0xFF70E1FFu);
            if (i == 2) draw_text(x + panel_w - 118, row_y, 1,
                                  !subtitle_available ? "NOT FOUND" :
                                  subtitles_enabled ? "AUTO" : "OFF",
                                  subtitle_available ? 0xFF70E1FFu : 0xFF8893A0u);
            if (i == 3) {
                snprintf(line, sizeof line, "%+d MS", offset_ms);
                draw_text(x + panel_w - 118, row_y, 1, line,
                          subtitle_available ? 0xFF70E1FFu : 0xFF8893A0u);
            }
        }
        draw_text(x + 22, y + panel_h - 23, 1,
                  "UP/DOWN MOVE  LEFT/RIGHT CHANGE  A OK  B RESUME",
                  0xFFB9C7D8u);
        overlay_present();
        overlay_set_scanout(true);
        return true;
    }
    if (!controls_visible) {
        overlay_present();
        overlay_set_scanout(content_visible);
        return content_visible;
    }
    int panel_h = 52 * text_scale, y = osd.height - panel_h;
    fill_rect(0, y, osd.width, panel_h, 0xD0101520u);
    char p[16], d[16], line[256]; format_time(p, sizeof p, pos); format_time(d, sizeof d, dur);
    snprintf(line, sizeof line, "PLAY  %s / %s", p, d);
    draw_text(10 * text_scale, y + 7 * text_scale, text_scale, line, 0xFFFFFFFFu);
    int bar_x = 10 * text_scale, bar_y = y + 20 * text_scale, bar_w = osd.width - 20 * text_scale;
    fill_rect(bar_x, bar_y, bar_w, 3 * text_scale, 0xFF596273u);
    if (dur > 0) fill_rect(bar_x, bar_y, (int)((int64_t)bar_w * pos / dur), 3 * text_scale, 0xFF48D8FFu);
    draw_text(10 * text_scale, y + 31 * text_scale, text_scale,
              "A/START MENU   LEFT/RIGHT SEEK   B EXIT", 0xFFEAF0F7u);
    overlay_present();
    overlay_set_scanout(true);
    return true;
}

int switchfrog_video_main(int argc, char **argv) {
    fprintf(stderr, "video_player: entered implementation argc=%d\n", argc);
    if (argc < 2 || !argv[1][0]) { fprintf(stderr, "Usage: %s VIDEO_FILE\n", argv[0]); return 2; }
    fprintf(stderr, "video_player: input=%s\n", argv[1]);
    if (!load_ffplayer()) return 5;
    int msgid = msgget((key_t)0x54465650, 0666 | IPC_CREAT);
    if (msgid < 0) { perror("video_player: msgget"); unload_ffplayer(); return 3; }

    char *external_subs[MAX_EXTERNAL_SUBS] = {0};
    int external_count = find_sidecar_subtitles(argv[1], external_subs, MAX_EXTERNAL_SUBS);
    fprintf(stderr, "video_player: sidecars=%d; initializing hcplayer\n", external_count);
    hcplayer_init(LOG_WARNING);
    HCPlayerInitArgs args; memset(&args, 0, sizeof(args));
    screen_rotation_load();
    args.uri = argv[1]; args.msg_id = msgid; args.sync_type = HCPLAYER_AUDIO_MASTER;
    /* Audio master is the only tested mode that preserves the file timebase.
     * Quick mode is needed to present frame one, but its default three-frame
     * correction threshold visibly skips reordered 23.976 fps movies. Reserve
     * dropping for an emergency backlog of roughly five seconds at 24 fps. */
    args.quick_mode = true;
    args.qm_drop_thresh = 120;
    args.buffering_enable = false;
    args.snd_devs = AUDDEV_DEFAULT; args.rotate_enable = true;
    args.rotate_type = screen_rotation_180 ? ROTATE_TYPE_180 : ROTATE_TYPE_0;
    /* External subtitle decode in this H.OS libffplayer build crashes its
     * decoder thread. Sidecars are parsed and rendered locally below. */
    args.callback = NULL; args.ext_subtitle_stream_num = 0;
    args.ext_sub_uris = NULL;
    fprintf(stderr, "video_player: creating decoder (audio-master, quick=1 drop=120 buffer=0)\n");
    void *player = hcplayer_create(&args);
    if (!player) {
        fprintf(stderr, "video_player: could not open %s\n", argv[1]);
        hcplayer_deinit(); msgctl(msgid, IPC_RMID, NULL);
        for (int i = 0; i < external_count; i++) free(external_subs[i]);
        unload_ffplayer(); return 4;
    }

    fprintf(stderr, "video_player: decoder created; opening overlay\n");
    overlay_open();
    hide_frontend_framebuffer();
    volatile uint32_t *keys = attach_keys();
    uint32_t previous = keys ? (*keys & 0x1ffffu) : 0;
    bool paused = false, done = false, ready = false;
    bool rotation_latched = false;
    int menu_index = 0, scale_mode = 0, video_w = 640, video_h = 480;
    bool subtitle_available = false, subtitles_enabled = false;
    subtitle_offset_ms = subtitle_offset_load(argv[1]);
    if (external_count > 0 && local_subtitles_load(external_subs[0]) > 0)
        subtitle_available = subtitles_enabled = true;
    /* Normal playback starts with the transparent OSD scanout physically idle.
     * Input reveals controls and subtitle cues enable it only while needed. */
    int64_t controls_until = 0, last_overlay = 0;
    bool overlay_visible = false;
    hcplayer_play(player);
    fprintf(stderr, "video_player: playing %s (%d sidecar subtitles)\n", argv[1], external_count);

    while (!done) {
        HCPlayerMsg msg;
        while (msgrcv(msgid, &msg, sizeof(msg) - sizeof(msg.type), 0, IPC_NOWAIT) >= 0) {
            if (terminal_message(msg.type)) {
                fprintf(stderr, "video_player: terminal message type=%ld\n", msg.type);
                done = true;
            }
            if (msg.type == HCPLAYER_MSG_STATE_READY) {
                HCPlayerVideoInfo info; memset(&info, 0, sizeof(info));
                if (hcplayer_get_cur_video_stream_info(player, &info) == 0) { video_w = info.width; video_h = info.height; }
                apply_display_mode(player, scale_mode, video_w, video_h);
                ready = true;
                fprintf(stderr, "video_player: ready video=%dx%d subtitles=%d\n",
                        video_w, video_h, subtitle_available ? 1 : 0);
            }
        }

        uint32_t raw = keys ? (*keys & 0x1ffffu) : 0;
        uint32_t pressed = raw & ~previous;
        previous = raw;
        if (pressed) controls_until = clock_ms() + 4000;
        bool rotation_chord = (raw & ROTATE_CHORD) == ROTATE_CHORD;
        if (rotation_chord && !rotation_latched) {
            screen_rotation_180 = !screen_rotation_180;
            screen_rotation_save();
            hcplayer_change_rotate_type(player, screen_rotation_180 ?
                                        ROTATE_TYPE_180 : ROTATE_TYPE_0);
            if (ready) apply_display_mode(player, scale_mode, video_w, video_h);
            rotation_latched = true;
            controls_until = clock_ms() + 4000;
            fprintf(stderr, "video_player: display rotation %s\n",
                    screen_rotation_180 ? "180 degrees" : "normal");
        } else if (!rotation_chord) {
            rotation_latched = false;
        }
        if (rotation_chord) {
            /* The chord is consumed so its shoulder buttons cannot also act. */
        } else if ((raw & (KEY_START | KEY_SELECT)) == (KEY_START | KEY_SELECT) || (pressed & KEY_B)) {
            if (paused && (pressed & KEY_B) &&
                (raw & (KEY_START | KEY_SELECT)) != (KEY_START | KEY_SELECT)) {
                hcplayer_resume(player); paused = false;
            } else {
                done = true;
            }
        } else if (paused) {
            if (pressed & KEY_UP) menu_index = (menu_index + 4) % 5;
            else if (pressed & KEY_DOWN) menu_index = (menu_index + 1) % 5;
            else if ((pressed & (KEY_LEFT | KEY_RIGHT)) && menu_index == 1) {
                scale_mode = (scale_mode + ((pressed & KEY_RIGHT) ? 1 : 3)) & 3;
                if (ready) apply_display_mode(player, scale_mode, video_w, video_h);
            } else if ((pressed & (KEY_LEFT | KEY_RIGHT)) && menu_index == 2 && subtitle_available) {
                subtitles_enabled = !subtitles_enabled;
            } else if ((pressed & (KEY_LEFT | KEY_RIGHT)) && menu_index == 3 && subtitle_available) {
                subtitle_offset_ms += (pressed & KEY_RIGHT) ? 100 : -100;
                if (subtitle_offset_ms < -SUBTITLE_OFFSET_LIMIT_MS) subtitle_offset_ms = -SUBTITLE_OFFSET_LIMIT_MS;
                if (subtitle_offset_ms >  SUBTITLE_OFFSET_LIMIT_MS) subtitle_offset_ms =  SUBTITLE_OFFSET_LIMIT_MS;
                subtitle_offset_save(argv[1], subtitle_offset_ms);
                fprintf(stderr, "video_player: subtitle delay %+d ms saved for %s\n",
                        subtitle_offset_ms, argv[1]);
            } else if (pressed & KEY_A) {
                if (menu_index == 0) { hcplayer_resume(player); paused = false; }
                else if (menu_index == 1) {
                    scale_mode = (scale_mode + 1) & 3;
                    if (ready) apply_display_mode(player, scale_mode, video_w, video_h);
                } else if (menu_index == 2 && subtitle_available) {
                    subtitles_enabled = !subtitles_enabled;
                } else if (menu_index == 3 && subtitle_available) {
                    subtitle_offset_ms = 0;
                    subtitle_offset_save(argv[1], subtitle_offset_ms);
                } else if (menu_index == 4) done = true;
            }
        } else if (pressed & (KEY_A | KEY_START)) {
            hcplayer_pause(player); paused = true; menu_index = 0;
        } else if (pressed & (KEY_LEFT | KEY_RIGHT)) {
            int64_t target = hcplayer_get_position(player) +
                ((pressed & KEY_RIGHT) ? 10000 : -10000);
            int64_t duration = hcplayer_get_duration(player);
            if (target < 0) target = 0;
            if (duration > 0 && target > duration) target = duration;
            hcplayer_seek(player, target);
        }
        int64_t now = clock_ms();
        bool overlay_needed = paused || now < controls_until ||
                              (subtitle_available && subtitles_enabled);
        int overlay_interval = paused ? 100 :
                               (now < controls_until ? 250 : 200);
        if (overlay_needed && now - last_overlay >= overlay_interval) {
            overlay_visible = render_overlay(player, paused, menu_index, scale_mode,
                                             subtitle_available, subtitles_enabled,
                                             subtitle_offset_ms, controls_until);
            last_overlay = now;
        } else if (!overlay_needed && overlay_visible) {
            overlay_clear();
            overlay_present();
            overlay_set_scanout(false);
            overlay_visible = false;
        }
        usleep(16000);
    }

    overlay_close(); hcplayer_stop2(player, true, true); hcplayer_deinit();
    local_subtitles_clear();
    if (keys) shmdt((const void *)keys); msgctl(msgid, IPC_RMID, NULL);
    for (int i = 0; i < external_count; i++) free(external_subs[i]);
    unload_ffplayer();
    return 0;
}
