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
#include <hcuapi/snd.h>

extern unsigned char fontdata8x8[64 * 16];

#define KEY_SELECT (1u << 0)
#define KEY_START  (1u << 3)
#define KEY_UP     (1u << 4)
#define KEY_RIGHT  (1u << 5)
#define KEY_DOWN   (1u << 6)
#define KEY_LEFT   (1u << 7)
#define KEY_L      (1u << 10)
#define KEY_R      (1u << 11)
#define KEY_X      (1u << 12)
#define KEY_A      (1u << 13)
#define KEY_B      (1u << 14)
#define KEY_Y      (1u << 15)
#define MAX_EXTERNAL_SUBS 16
#define SUBTITLE_OFFSET_FILE "/mnt/sdcard/frogui/video_subtitle_offsets.txt"
#define SUBTITLE_OFFSET_TMP  "/mnt/sdcard/frogui/video_subtitle_offsets.tmp"
#define SUBTITLE_OFFSET_LIMIT_MS 60000

/* H.OS's Linux 4.4 dynamic loader crashes before main() when a Zig/LLD-linked
 * executable has libffplayer.so in DT_NEEDED.  Stock rkgame loads its media
 * stack after process startup, so do the same: keep the executable dependent
 * only on libc/libdl and resolve the vendor player API explicitly.  Besides
 * avoiding the loader fault, this gives the log an exact dlopen/dlsym stage. */
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
static int (*fp_hcplayer_get_audio_streams_count)(void *);
static int (*fp_hcplayer_get_subtitle_streams_count)(void *);
static int (*fp_hcplayer_get_cur_video_stream_info)(void *, HCPlayerVideoInfo *);
static int (*fp_hcplayer_change_audio_track)(void *, int);
static int (*fp_hcplayer_change_subtitle_track)(void *, int);
static int (*fp_hcplayer_set_speed_rate)(void *, float);
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
#define hcplayer_get_audio_streams_count fp_hcplayer_get_audio_streams_count
#define hcplayer_get_subtitle_streams_count fp_hcplayer_get_subtitle_streams_count
#define hcplayer_get_cur_video_stream_info fp_hcplayer_get_cur_video_stream_info
#define hcplayer_change_audio_track fp_hcplayer_change_audio_track
#define hcplayer_change_subtitle_track fp_hcplayer_change_subtitle_track
#define hcplayer_set_speed_rate fp_hcplayer_set_speed_rate
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
    LOAD_PLAYER_SYMBOL(hcplayer_get_audio_streams_count);
    LOAD_PLAYER_SYMBOL(hcplayer_get_subtitle_streams_count);
    LOAD_PLAYER_SYMBOL(hcplayer_get_cur_video_stream_info);
    LOAD_PLAYER_SYMBOL(hcplayer_change_audio_track);
    LOAD_PLAYER_SYMBOL(hcplayer_change_subtitle_track);
    LOAD_PLAYER_SYMBOL(hcplayer_set_speed_rate);
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
    size_t size;
    int width, height, stride;
} Overlay;

static Overlay osd = {-1, NULL, 0, 0, 0, 0};
static pthread_mutex_t subtitle_lock = PTHREAD_MUTEX_INITIALIZER;
static char subtitle_text[1024];
static bool subtitle_visible;
static int64_t subtitle_start_ms;
static int64_t subtitle_end_ms;
static volatile int64_t player_position_ms;
static int subtitle_offset_ms;

static int64_t clock_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
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

static void overlay_clear(void) {
    if (osd.pixels) memset(osd.pixels, 0, osd.size);
}

static void overlay_close(void) {
    if (osd.pixels) {
        overlay_clear();
        munmap(osd.pixels, osd.size);
    }
    if (osd.fd >= 0) close(osd.fd);
    osd.fd = -1; osd.pixels = NULL; osd.size = 0;
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
    osd.pixels = mmap(NULL, osd.size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, osd.fd, 0);
    if (osd.pixels == MAP_FAILED) { osd.pixels = NULL; overlay_close(); return; }
    overlay_clear();
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!osd.pixels || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x + w > osd.width) w = osd.width - x;
    if (y + h > osd.height) h = osd.height - y;
    for (int yy = 0; yy < h; yy++) {
        uint32_t *row = osd.pixels + (y + yy) * osd.stride + x;
        for (int xx = 0; xx < w; xx++) row[xx] = color;
    }
}

static void draw_text(int x, int y, int scale, const char *text, uint32_t color) {
    if (!osd.pixels || !text) return;
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

static const char *ass_dialogue_text(const char *ass) {
    const char *p = ass; int commas = 0;
    while (*p && commas < 9) if (*p++ == ',') commas++;
    return commas == 9 ? p : ass;
}

static void copy_subtitle_text(char *dst, size_t n, const char *src) {
    size_t j = 0;
    if (!src || !n) return;
    for (size_t i = 0; src[i] && j + 1 < n; i++) {
        if (src[i] == '{') { while (src[i] && src[i] != '}') i++; continue; }
        if (src[i] == '\\' && (src[i + 1] == 'N' || src[i + 1] == 'n')) {
            dst[j++] = '\n'; i++; continue;
        }
        if (src[i] != '\r') dst[j++] = src[i];
    }
    dst[j] = '\0';
}

static void player_callback(HCPlayerCallbackType type, void *user_data, void *val) {
    (void)user_data;
    pthread_mutex_lock(&subtitle_lock);
    if (type == HCPLAYER_CALLBACK_SUBTITLE_OFF) {
        /* Keep a positive-delay cue alive for the same delay after ffplayer's
         * nominal OFF event. Timestamped cues normally expire by themselves. */
        if (subtitle_end_ms <= subtitle_start_ms)
            subtitle_end_ms = player_position_ms;
    } else if (type == HCPLAYER_CALLBACK_SUBTITLE_ON && val) {
        AVSubtitle *sub = (AVSubtitle *)val;
        subtitle_text[0] = '\0';
        for (unsigned i = 0; i < sub->num_rects && !subtitle_text[0]; i++) {
            AVSubtitleRect *r = sub->rects[i];
            if (!r) continue;
            if (r->type == SUBTITLE_TEXT && r->text)
                copy_subtitle_text(subtitle_text, sizeof(subtitle_text), r->text);
            else if (r->type == SUBTITLE_ASS && r->ass)
                copy_subtitle_text(subtitle_text, sizeof(subtitle_text), ass_dialogue_text(r->ass));
        }
        subtitle_visible = subtitle_text[0] != '\0';
        if (subtitle_visible) {
            int64_t now = player_position_ms;
            int64_t start = sub->pts == INT64_MIN ? now : sub->pts / 1000;
            start += sub->start_display_time;
            /* Broken/missing container PTS values are common in loose subtitle
             * files. Fall back to the decoder's current presentation point. */
            if (start < now - 300000 || start > now + 300000) start = now;
            int64_t duration = (int64_t)sub->end_display_time - sub->start_display_time;
            if (duration <= 0 || duration > 600000) duration = 5000;
            subtitle_start_ms = start;
            subtitle_end_ms = start + duration;
        }
    }
    pthread_mutex_unlock(&subtitle_lock);
}

static bool extension_is_subtitle(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && (!strcasecmp(dot, ".srt") || !strcasecmp(dot, ".ass") ||
                   !strcasecmp(dot, ".ssa") || !strcasecmp(dot, ".sub") ||
                   !strcasecmp(dot, ".idx") || !strcasecmp(dot, ".vtt") ||
                   !strcasecmp(dot, ".smi") || !strcasecmp(dot, ".sami"));
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
    while (count < capacity && (entry = readdir(dir)) != NULL) {
        if (strncasecmp(entry->d_name, prefix, prefix_len) != 0 ||
            (entry->d_name[prefix_len] != '.' && entry->d_name[prefix_len] != '\0') ||
            !extension_is_subtitle(entry->d_name)) continue;
        size_t needed = strlen(directory) + strlen(entry->d_name) + 2;
        uris[count] = malloc(needed);
        if (!uris[count]) break;
        snprintf(uris[count], needed, "%s/%s", directory, entry->d_name);
        count++;
    }
    closedir(dir);
    return count;
}

static int volume_get(void) {
    int fd = open("/dev/sndC0i2so", O_RDWR); uint8_t value = 50;
    if (fd >= 0) { ioctl(fd, SND_IOCTL_GET_VOLUME, &value); close(fd); }
    return value > 100 ? 100 : value;
}

static void volume_set(int value) {
    uint8_t v = (uint8_t)(value < 0 ? 0 : value > 100 ? 100 : value);
    int fd = open("/dev/sndC0i2so", O_RDWR);
    if (fd >= 0) { ioctl(fd, SND_IOCTL_SET_VOLUME, &v); close(fd); }
}

static const char *scale_name(int mode) {
    static const char *names[] = {"FIT", "FILL", "STRETCH", "ORIGINAL"};
    return names[mode & 3];
}

static void apply_display_mode(void *player, int mode, int rotation, int vw, int vh) {
    const int sw = 1920, sh = 1080;
    if (rotation & 1) { int t = vw; vw = vh; vh = t; }
    if (vw <= 0 || vh <= 0) { vw = sw; vh = sh; }
    struct vdec_dis_rect rect = {{0, 0, (uint16_t)vw, (uint16_t)vh},
                                 {0, 0, (uint16_t)sw, (uint16_t)sh}};
    if (mode == 0) { /* fit */
        int w = sw, h = w * vh / vw;
        if (h > sh) { h = sh; w = h * vw / vh; }
        rect.dst_rect.x = (uint16_t)((sw - w) / 2); rect.dst_rect.y = (uint16_t)((sh - h) / 2);
        rect.dst_rect.w = (uint16_t)w; rect.dst_rect.h = (uint16_t)h;
    } else if (mode == 1) { /* crop source to fill */
        if ((int64_t)vw * sh > (int64_t)vh * sw) {
            int w = vh * sw / sh; rect.src_rect.x = (uint16_t)((vw - w) / 2); rect.src_rect.w = (uint16_t)w;
        } else {
            int h = vw * sh / sw; rect.src_rect.y = (uint16_t)((vh - h) / 2); rect.src_rect.h = (uint16_t)h;
        }
    } else if (mode == 3) { /* one source pixel per output coordinate */
        int w = vw > sw ? sw : vw, h = vh > sh ? sh : vh;
        rect.dst_rect.x = (uint16_t)((sw - w) / 2); rect.dst_rect.y = (uint16_t)((sh - h) / 2);
        rect.dst_rect.w = (uint16_t)w; rect.dst_rect.h = (uint16_t)h;
        rect.src_rect.x = (uint16_t)((vw - w) / 2); rect.src_rect.y = (uint16_t)((vh - h) / 2);
        rect.src_rect.w = (uint16_t)w; rect.src_rect.h = (uint16_t)h;
    }
    hcplayer_set_display_rect(player, &rect);
}

static void render_overlay(void *player, bool paused, float speed, int rotation,
                           int scale_mode, int volume, int subtitle_track,
                           int subtitle_count, int offset_ms,
                           int64_t controls_until) {
    if (!osd.pixels) return;
    overlay_clear();
    int text_scale = osd.width >= 900 ? 2 : 1;
    int64_t pos = hcplayer_get_position(player), dur = hcplayer_get_duration(player);
    player_position_ms = pos;
    pthread_mutex_lock(&subtitle_lock);
    if (subtitle_visible && subtitle_track >= 0 &&
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
        int y = osd.height - box_h - (clock_ms() < controls_until ? 95 * text_scale : 18 * text_scale);
        fill_rect(12 * text_scale, y, osd.width - 24 * text_scale, box_h, 0xB0000000u);
        draw_text(20 * text_scale, y + 6 * text_scale, text_scale, wrapped, 0xFFFFFFFFu);
    }
    pthread_mutex_unlock(&subtitle_lock);
    if (clock_ms() >= controls_until) return;
    int panel_h = 101 * text_scale, y = osd.height - panel_h;
    fill_rect(0, y, osd.width, panel_h, 0xD0101520u);
    char p[16], d[16], line[256]; format_time(p, sizeof p, pos); format_time(d, sizeof d, dur);
    snprintf(line, sizeof line, "%s %s/%s %.0fX R%d %s V%d S%s%d/%d D%+dMS",
             paused ? "PAUSED" : "PLAY", p, d, speed, rotation * 90, scale_name(scale_mode), volume,
             subtitle_track < 0 ? "OFF " : "", subtitle_track < 0 ? 0 : subtitle_track + 1, subtitle_count,
             offset_ms);
    draw_text(10 * text_scale, y + 7 * text_scale, text_scale, line, 0xFFFFFFFFu);
    int bar_x = 10 * text_scale, bar_y = y + 20 * text_scale, bar_w = osd.width - 20 * text_scale;
    fill_rect(bar_x, bar_y, bar_w, 3 * text_scale, 0xFF596273u);
    if (dur > 0) fill_rect(bar_x, bar_y, (int)((int64_t)bar_w * pos / dur), 3 * text_scale, 0xFF48D8FFu);
    draw_text(10 * text_scale, y + 31 * text_scale, text_scale,
              "A PAUSE   LEFT/RIGHT SEEK 10S   L/R SEEK 60S   B EXIT", 0xFFEAF0F7u);
    draw_text(10 * text_scale, y + 44 * text_scale, text_scale,
              "X SPEED   Y ROTATE   START SCALE   SELECT SUBTITLES", 0xFFEAF0F7u);
    draw_text(10 * text_scale, y + 57 * text_scale, text_scale,
              "UP/DOWN VOLUME   SELECT+Y AUDIO TRACK", 0xFFEAF0F7u);
    draw_text(10 * text_scale, y + 70 * text_scale, text_scale,
              "HOLD SELECT + LEFT/RIGHT: SUB DELAY -/+100MS", 0xFFEAF0F7u);
}

int main(int argc, char **argv) {
    fprintf(stderr, "video_player: entered main argc=%d\n", argc);
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
    args.uri = argv[1]; args.msg_id = msgid; args.sync_type = HCPLAYER_AUDIO_MASTER;
    args.quick_mode = true; args.snd_devs = AUDDEV_DEFAULT; args.rotate_enable = true;
    args.callback = player_callback; args.ext_subtitle_stream_num = external_count;
    args.ext_sub_uris = external_count ? external_subs : NULL;
    fprintf(stderr, "video_player: creating decoder\n");
    void *player = hcplayer_create(&args);
    if (!player) {
        fprintf(stderr, "video_player: could not open %s\n", argv[1]);
        hcplayer_deinit(); msgctl(msgid, IPC_RMID, NULL);
        for (int i = 0; i < external_count; i++) free(external_subs[i]);
        unload_ffplayer(); return 4;
    }

    fprintf(stderr, "video_player: decoder created; opening overlay\n");
    overlay_open();
    volatile uint32_t *keys = attach_keys();
    uint32_t previous = keys ? (*keys & 0xffffu) : 0;
    bool paused = false, done = false, ready = false;
    float speeds[] = {1.0f, 2.0f, 4.0f, 8.0f}; int speed_index = 0;
    int rotation = 0, scale_mode = 0, video_w = 1920, video_h = 1080;
    int volume = volume_get(), subtitle_track = -1, subtitle_count = 0, audio_track = 0;
    subtitle_offset_ms = subtitle_offset_load(argv[1]);
    bool select_chord_used = false;
    int64_t controls_until = clock_ms() + 8000, last_overlay = 0;
    hcplayer_play(player);
    fprintf(stderr, "video_player: playing %s (%d sidecar subtitles)\n", argv[1], external_count);

    while (!done) {
        HCPlayerMsg msg;
        while (msgrcv(msgid, &msg, sizeof(msg) - sizeof(msg.type), 0, IPC_NOWAIT) >= 0) {
            if (terminal_message(msg.type)) done = true;
            if (msg.type == HCPLAYER_MSG_STATE_READY) {
                HCPlayerVideoInfo info; memset(&info, 0, sizeof(info));
                if (hcplayer_get_cur_video_stream_info(player, &info) == 0) { video_w = info.width; video_h = info.height; }
                subtitle_count = hcplayer_get_subtitle_streams_count(player);
                if (subtitle_count > 0 && external_count > 0 && hcplayer_change_subtitle_track(player, 0) == 0)
                    subtitle_track = 0;
                apply_display_mode(player, scale_mode, rotation, video_w, video_h);
                ready = true;
            }
        }

        uint32_t raw = keys ? (*keys & 0xffffu) : 0;
        uint32_t pressed = raw & ~previous;
        uint32_t released = previous & ~raw;
        previous = raw;
        if (pressed & KEY_SELECT) select_chord_used = false;
        if (pressed) controls_until = clock_ms() + 4000;
        if ((raw & (KEY_START | KEY_SELECT)) == (KEY_START | KEY_SELECT) || (pressed & KEY_B)) {
            if (raw & KEY_SELECT) select_chord_used = true;
            done = true;
        } else if ((raw & KEY_SELECT) && (pressed & KEY_Y)) {
            select_chord_used = true;
            int count = hcplayer_get_audio_streams_count(player);
            if (count > 0) { audio_track = (audio_track + 1) % count; hcplayer_change_audio_track(player, audio_track); }
        } else if ((raw & KEY_SELECT) && (pressed & (KEY_LEFT | KEY_RIGHT))) {
            select_chord_used = true;
            subtitle_offset_ms += (pressed & KEY_RIGHT) ? 100 : -100;
            if (subtitle_offset_ms < -SUBTITLE_OFFSET_LIMIT_MS) subtitle_offset_ms = -SUBTITLE_OFFSET_LIMIT_MS;
            if (subtitle_offset_ms >  SUBTITLE_OFFSET_LIMIT_MS) subtitle_offset_ms =  SUBTITLE_OFFSET_LIMIT_MS;
            subtitle_offset_save(argv[1], subtitle_offset_ms);
            fprintf(stderr, "video_player: subtitle delay %+d ms saved for %s\n",
                    subtitle_offset_ms, argv[1]);
        } else if (pressed & KEY_A) {
            if (paused) hcplayer_resume(player); else hcplayer_pause(player); paused = !paused;
        } else if (pressed & (KEY_LEFT | KEY_RIGHT | KEY_L | KEY_R)) {
            int delta = (pressed & (KEY_R | KEY_L)) ? 60000 : 10000;
            if (pressed & (KEY_LEFT | KEY_L)) delta = -delta;
            int64_t target = hcplayer_get_position(player) + delta, duration = hcplayer_get_duration(player);
            if (target < 0) target = 0; if (duration > 0 && target > duration) target = duration;
            hcplayer_seek(player, target);
        } else if (pressed & KEY_X) {
            speed_index = (speed_index + 1) % 4; hcplayer_set_speed_rate(player, speeds[speed_index]);
        } else if (pressed & KEY_Y) {
            rotation = (rotation + 1) & 3; hcplayer_change_rotate_type(player, (rotate_type_e)rotation);
            if (ready) apply_display_mode(player, scale_mode, rotation, video_w, video_h);
        } else if (pressed & KEY_START) {
            scale_mode = (scale_mode + 1) & 3;
            if (ready) apply_display_mode(player, scale_mode, rotation, video_w, video_h);
        } else if ((released & KEY_SELECT) && !select_chord_used) {
            subtitle_count = hcplayer_get_subtitle_streams_count(player);
            subtitle_track++;
            if (subtitle_track >= subtitle_count) subtitle_track = -1;
            hcplayer_change_subtitle_track(player, subtitle_track);
        } else if (pressed & (KEY_UP | KEY_DOWN)) {
            volume += (pressed & KEY_UP) ? 5 : -5;
            if (volume < 0) volume = 0; if (volume > 100) volume = 100; volume_set(volume);
        }
        int64_t now = clock_ms();
        if (now - last_overlay >= 100) {
            render_overlay(player, paused, speeds[speed_index], rotation, scale_mode,
                           volume, subtitle_track, subtitle_count, subtitle_offset_ms,
                           controls_until);
            last_overlay = now;
        }
        usleep(16000);
    }

    overlay_close(); hcplayer_stop2(player, true, true); hcplayer_deinit();
    if (keys) shmdt((const void *)keys); msgctl(msgid, IPC_RMID, NULL);
    for (int i = 0; i < external_count; i++) free(external_subs[i]);
    unload_ffplayer();
    return 0;
}
