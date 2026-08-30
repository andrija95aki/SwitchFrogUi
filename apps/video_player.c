/* SwitchFrogUI merged hardware video player.
 *
 * Playback/decoder lifecycle is based on TreeFrogUI v1.2.0_b. The pause
 * menu, fit modes, safe local SRT/WebVTT renderer, persistent 100 ms subtitle
 * timing and 180-degree display toggle are SwitchFrogUI additions. */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/fb.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <jpeglib.h>
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
#include <hcuapi/dis.h>

extern unsigned char fontdata8x8[64 * 16];

#define THEME_FILE "/mnt/sdcard/cubegm/skin/skin.txt"
#define DEVICE_FILE "/tmp/tfdevice.env"
#define KEYMAP_FILE "/mnt/sdcard/frogui/keymap.txt"
#define MAX_PATH_LEN 512
#define SUBTITLE_OFFSET_FILE "/mnt/sdcard/frogui/video_subtitle_offsets.txt"
#define SUBTITLE_OFFSET_TMP  "/mnt/sdcard/frogui/video_subtitle_offsets.tmp"
#define SUBTITLE_OFFSET_LIMIT_MS 60000
#define VIDEO_SETTINGS_FILE "/mnt/sdcard/frogui/video_player.txt"
#define VIDEO_SETTINGS_TMP  "/mnt/sdcard/frogui/video_player.tmp"
#define ROTATION_FILE "/mnt/sdcard/frogui/screen_rotation.cfg"
#define ROTATION_TMP  "/mnt/sdcard/frogui/screen_rotation.tmp"
#define PLAYBACK_STALL_MS 15000

enum {
    BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_X, BTN_Y,
    BTN_L1, BTN_R1, BTN_START, BTN_SELECT, BTN_FN, BTN_COUNT
};
static int key_bits[BTN_COUNT] = { 4, 6, 7, 5, 13, 14, 12, 15, 10, 11, 3, 0, 16 };
static const char *key_names[BTN_COUNT] = {
    "UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y",
    "L1", "R1", "START", "SELECT", "FN"
};

typedef struct {
    int fd;
    unsigned char *mem;
    size_t mem_len;
    int fb_w, fb_h, pitch, bytespp;
    struct fb_var_screeninfo vi;
    int logical_w, logical_h;
    int rotation;
    uint32_t *canvas;
} Overlay;

typedef struct {
    uint32_t text;
    uint32_t accent;
    uint32_t selected_text;
    uint32_t background;
    bool background_enabled;
} Theme;

typedef enum { PLAY_SEQUENTIAL, PLAY_REPEAT, PLAY_RANDOM } PlaybackMode;
typedef enum { SCALE_FIT, SCALE_FILL, SCALE_STRETCH, SCALE_ORIGINAL, SCALE_COUNT } ScaleMode;

typedef struct {
    unsigned char *rgb;
    int width;
    int height;
    char temp_path[MAX_PATH_LEN];
} CoverState;

typedef struct {
    int64_t start_ms;
    int64_t end_ms;
    char text[1024];
} TimedCue;

static TimedCue *subtitle_cues;
static int subtitle_cue_count;
static int subtitle_cue_cursor;
static bool screen_rotation_180;

static volatile sig_atomic_t quit_requested;
static bool is_audio_path(const char *path);
static bool is_media_path(const char *path);

static void log_step(const char *step) {
    FILE *probe = fopen("/mnt/sdcard/log.txt", "r");
    if (!probe) return;
    fclose(probe);
    FILE *f = fopen("/mnt/sdcard/log.txt", "a");
    if (!f) return;
    fprintf(f, "VIDEO_PLAYER: %s\n", step);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
}

static void log_message(long type, int val) {
    char line[96];
    snprintf(line, sizeof(line), "message type=%ld val=%d", type, val);
    log_step(line);
}

static void configure_video_layer(void) {
    int fd = open("/dev/dis", O_RDWR);
    if (fd < 0) { log_step("cannot open /dev/dis"); return; }
    /* Exact blend order used by the stock R36SX hcprojector:
     * GMA-S (fb1 HUD) > MAIN (decoded video) > GMA-F (fb0 UI) > AUX. */
    struct dis_layer_blend_order order;
    memset(&order, 0, sizeof(order));
    order.distype = DIS_TYPE_HD;
    order.main_layer = 2;
    order.auxp_layer = 0;
    order.gmas_layer = 3;
    order.gmaf_layer = 1;
    int rc = ioctl(fd, DIS_SET_LAYER_ORDER, &order);
    close(fd);
    log_step(rc == 0 ? "video layer placed above fb0" : "video layer order ioctl failed");
}

/* FrogUI renders its browser into fb0 and then exits for the standalone
 * player. The decoded MAIN plane does not cover letterbox/pillarbox areas, so
 * stale browser pixels remain visible around the movie. Clear only the active
 * fb0 scanout page, and only after the decoder has produced its first frame.
 * No display-memory write is allowed in the proven upstream startup sequence.
 * FrogUI redraws the active page when zhijack relaunches it. */
static void clear_frontend_background(void) {
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) { log_step("cannot open fb0 for black background"); return; }
    struct fb_var_screeninfo vi;
    struct fb_fix_screeninfo fi;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) < 0 ||
        ioctl(fd, FBIOGET_FSCREENINFO, &fi) < 0 || fi.smem_len == 0 ||
        fi.line_length == 0 || vi.xres == 0 || vi.yres == 0) {
        close(fd);
        log_step("cannot query fb0 for black background");
        return;
    }
    size_t map_bytes = fi.smem_len;
    unsigned bytespp = vi.bits_per_pixel / 8;
    if (!bytespp || (size_t)vi.xres > SIZE_MAX / bytespp ||
        (size_t)vi.xoffset > SIZE_MAX / bytespp) {
        close(fd);
        log_step("invalid fb0 geometry for black background");
        return;
    }
    unsigned char *mem = mmap(NULL, map_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) {
        close(fd);
        log_step("cannot map fb0 for black background");
        return;
    }
    uint32_t opaque_black = 0;
    if (vi.transp.length && vi.transp.length < 32)
        opaque_black = ((1u << vi.transp.length) - 1u) << vi.transp.offset;
    size_t row_bytes = (size_t)vi.xres * bytespp;
    for (uint32_t y = 0; y < vi.yres; y++) {
        size_t virtual_y = (size_t)vi.yoffset + y;
        if (virtual_y > SIZE_MAX / (size_t)fi.line_length) break;
        size_t offset = virtual_y * (size_t)fi.line_length +
                        (size_t)vi.xoffset * bytespp;
        if (offset > map_bytes || row_bytes > map_bytes - offset) break;
        unsigned char *row = mem + offset;
        if (bytespp == 4) {
            uint32_t *pixels = (uint32_t *)row;
            for (uint32_t x = 0; x < vi.xres; x++) pixels[x] = opaque_black;
        } else if (bytespp == 2) {
            uint16_t value = (uint16_t)opaque_black;
            uint16_t *pixels = (uint16_t *)row;
            for (uint32_t x = 0; x < vi.xres; x++) pixels[x] = value;
        } else {
            memset(row, 0, row_bytes);
        }
    }
    munmap(mem, map_bytes);
    close(fd);
    char message[128];
    snprintf(message, sizeof(message),
             "active frontend page cleared to black (%ux%u at %u,%u)",
             vi.xres, vi.yres, vi.xoffset, vi.yoffset);
    log_step(message);
}

static int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void on_signal(int sig) {
    (void)sig;
    quit_requested = 1;
}

static void read_device_geometry(int *w, int *h, int *rotation) {
    FILE *f = fopen(DEVICE_FILE, "r");
    char line[128], key[64], value[64];
    *w = 640;
    *h = 480;
    *rotation = 0;
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%63[^=]=%63s", key, value) != 2) continue;
        if (!strcmp(key, "TF_PANEL_W")) *w = atoi(value);
        else if (!strcmp(key, "TF_PANEL_H")) *h = atoi(value);
        else if (!strcmp(key, "TF_ROTATE")) *rotation = atoi(value);
    }
    fclose(f);
    if (*w < 320 || *w > 1920) *w = 640;
    if (*h < 240 || *h > 1080) *h = 480;
    if (*rotation != 90 && *rotation != 180 && *rotation != 270) *rotation = 0;
}

static uint32_t parse_rgb(const char *s, uint32_t fallback) {
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 0);
    return end != s ? (uint32_t)(v & 0xFFFFFFu) : fallback;
}

static Theme load_theme(void) {
    Theme t = { 0xF4F4F4, 0xFFFFFF, 0x101010, 0x101010, true };
    FILE *f = fopen(THEME_FILE, "r");
    char line[128], key[64], value[64];
    if (!f) return t;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%63[^=]=%63s", key, value) != 2) continue;
        if (!strcmp(key, "text_color")) t.text = parse_rgb(value, t.text);
        else if (!strcmp(key, "selection_color")) t.accent = parse_rgb(value, t.accent);
        else if (!strcmp(key, "sel_text_color")) t.selected_text = parse_rgb(value, t.selected_text);
        else if (!strcmp(key, "background_color")) t.background = parse_rgb(value, t.background);
    }
    fclose(f);
    f = fopen("/mnt/sdcard/frogui/music_player.txt", "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%63[^=]=%63s", key, value) != 2) continue;
            if (!strcmp(key, "background")) t.background_enabled = !strcasecmp(value, "on");
            else if (!strcmp(key, "background_color")) t.background = parse_rgb(value, t.background);
        }
        fclose(f);
    }
    return t;
}

static PlaybackMode load_playback_mode(void) {
    FILE *f = fopen("/mnt/sdcard/frogui/music_player.txt", "r");
    char line[128], key[64], value[64];
    PlaybackMode mode = PLAY_SEQUENTIAL;
    if (!f) return mode;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%63[^=]=%63s", key, value) != 2) continue;
        if (!strcmp(key, "playback")) {
            if (!strcasecmp(value, "repeat") || !strcasecmp(value, "loop")) mode = PLAY_REPEAT;
            else if (!strcasecmp(value, "random") || !strcasecmp(value, "shuffle")) mode = PLAY_RANDOM;
        }
    }
    fclose(f);
    return mode;
}

static void save_playback_mode(PlaybackMode mode) {
    FILE *f = fopen("/mnt/sdcard/frogui/music_player.txt", "a");
    if (!f) return;
    fprintf(f, "playback=%s\n", mode == PLAY_REPEAT ? "loop" : mode == PLAY_RANDOM ? "random" : "sequential");
    fclose(f);
}

static const char *playback_name(PlaybackMode mode) {
    return mode == PLAY_REPEAT ? "LOOP" :
           mode == PLAY_RANDOM ? "RANDOM" : "SEQUENTIAL";
}

static const char *scale_name(ScaleMode mode) {
    static const char *names[SCALE_COUNT] = { "FIT", "FILL", "STRETCH", "ORIGINAL" };
    return names[(int)mode >= 0 && mode < SCALE_COUNT ? (int)mode : 0];
}

static ScaleMode load_scale_mode(void) {
    FILE *f = fopen(VIDEO_SETTINGS_FILE, "r");
    char line[128], key[64], value[64];
    int mode = SCALE_FIT;
    if (!f) return SCALE_FIT;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2 && !strcmp(key, "scale"))
            mode = atoi(value);
    }
    fclose(f);
    return mode >= 0 && mode < SCALE_COUNT ? (ScaleMode)mode : SCALE_FIT;
}

static void save_scale_mode(ScaleMode mode) {
    mkdir("/mnt/sdcard/frogui", 0777);
    FILE *f = fopen(VIDEO_SETTINGS_TMP, "w");
    if (!f) return;
    fprintf(f, "scale=%d\n", (int)mode);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    if (rename(VIDEO_SETTINGS_TMP, VIDEO_SETTINGS_FILE) == 0) sync();
    else unlink(VIDEO_SETTINGS_TMP);
}

static void screen_rotation_load(void) {
    char value[24] = {0};
    FILE *f = fopen(ROTATION_FILE, "r");
    screen_rotation_180 = false;
    if (!f) return;
    if (fgets(value, sizeof(value), f)) screen_rotation_180 = atoi(value) == 180;
    fclose(f);
}

static void screen_rotation_save(void) {
    FILE *f = fopen(ROTATION_TMP, "w");
    if (!f) return;
    fprintf(f, "%d\n", screen_rotation_180 ? 180 : 0);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    if (rename(ROTATION_TMP, ROTATION_FILE) == 0) sync();
    else unlink(ROTATION_TMP);
}

static int effective_rotation(int panel_rotation) {
    return (panel_rotation + (screen_rotation_180 ? 180 : 0)) % 360;
}

static void load_keymap(void) {
    FILE *f = fopen(KEYMAP_FILE, "r");
    char line[64], name[32];
    int bit;
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%31[^=]=%d", name, &bit) != 2) continue;
        for (int i = 0; i < BTN_COUNT; i++)
            if (!strcmp(name, key_names[i]) && bit >= 0 && bit < 32) key_bits[i] = bit;
    }
    fclose(f);
}

static volatile uint32_t *open_keys(void) {
    key_t key = ftok("/tmp/joy_key", 'a');
    if (key == (key_t)-1) return NULL;
    int id = shmget(key, 4, 0666);
    if (id < 0) return NULL;
    void *p = shmat(id, NULL, 0);
    return p == (void *)-1 ? NULL : (volatile uint32_t *)p;
}

static uint32_t logical_keys(volatile uint32_t *raw) {
    uint32_t in = raw ? (*raw & 0x1FFFFu) : 0;
    uint32_t out = 0;
    for (int i = 0; i < BTN_COUNT; i++)
        if (in & (1u << key_bits[i])) out |= 1u << i;
    return out;
}

static int overlay_open(Overlay *o) {
    memset(o, 0, sizeof(*o));
    o->fd = -1;
    read_device_geometry(&o->logical_w, &o->logical_h, &o->rotation);
    o->fd = open("/dev/fb1", O_RDWR);
    if (o->fd < 0) return -1;
    struct fb_fix_screeninfo fi;
    if (ioctl(o->fd, FBIOGET_VSCREENINFO, &o->vi) < 0 ||
        ioctl(o->fd, FBIOGET_FSCREENINFO, &fi) < 0 || fi.smem_len == 0) goto fail;
    o->fb_w = o->vi.xres;
    o->fb_h = o->vi.yres;
    o->pitch = fi.line_length;
    o->bytespp = o->vi.bits_per_pixel / 8;
    o->mem_len = fi.smem_len;
    if (o->bytespp != 4 && o->bytespp != 2) goto fail;
    o->mem = mmap(NULL, o->mem_len, PROT_READ | PROT_WRITE, MAP_SHARED, o->fd, 0);
    if (o->mem == MAP_FAILED) { o->mem = NULL; goto fail; }
    o->canvas = calloc((size_t)o->logical_w * o->logical_h, sizeof(*o->canvas));
    if (!o->canvas) goto fail;
    /* fb1 is portrait-shaped on the portrait-mounted SF panels. Its memory
     * must receive the same clockwise transform as the decoded MAIN layer.
     * Trust the boot profile, but only rotate when the fb geometry agrees. */
    if (!(o->fb_w < o->fb_h && o->logical_w > o->logical_h)) o->rotation = 0;
    return 0;
fail:
    if (o->mem) munmap(o->mem, o->mem_len);
    if (o->fd >= 0) close(o->fd);
    free(o->canvas);
    memset(o, 0, sizeof(*o));
    o->fd = -1;
    return -1;
}

static void overlay_clear(Overlay *o) {
    if (o->mem) memset(o->mem, 0, o->mem_len);
    if (o->canvas) memset(o->canvas, 0, (size_t)o->logical_w * o->logical_h * 4);
}

static void overlay_close(Overlay *o) {
    overlay_clear(o);
    if (o->mem) munmap(o->mem, o->mem_len);
    if (o->fd >= 0) close(o->fd);
    free(o->canvas);
    memset(o, 0, sizeof(*o));
    o->fd = -1;
}

static uint32_t scale_channel(uint32_t c, const struct fb_bitfield *b) {
    if (!b->length) return 0;
    uint32_t max = (1u << b->length) - 1u;
    return ((c * max + 127u) / 255u) << b->offset;
}

static uint32_t pack_fb(const Overlay *o, uint32_t argb) {
    uint32_t a = argb >> 24, r = (argb >> 16) & 255, g = (argb >> 8) & 255, b = argb & 255;
    return scale_channel(r, &o->vi.red) | scale_channel(g, &o->vi.green) |
           scale_channel(b, &o->vi.blue) | scale_channel(a, &o->vi.transp);
}

static void overlay_present(Overlay *o) {
    if (!o->mem || !o->canvas) return;
    for (int fy = 0; fy < o->fb_h; fy++) {
        unsigned char *row = o->mem + (size_t)(fy + o->vi.yoffset) * o->pitch;
        for (int fx = 0; fx < o->fb_w; fx++) {
            int lx, ly;
            if (o->rotation == 90) {
                lx = fy * o->logical_w / o->fb_h;
                ly = o->logical_h - 1 - fx * o->logical_h / o->fb_w;
            } else if (o->rotation == 180) {
                lx = o->logical_w - 1 - fx * o->logical_w / o->fb_w;
                ly = o->logical_h - 1 - fy * o->logical_h / o->fb_h;
            } else if (o->rotation == 270) {
                lx = o->logical_w - 1 - fy * o->logical_w / o->fb_h;
                ly = fx * o->logical_h / o->fb_w;
            } else {
                lx = fx * o->logical_w / o->fb_w;
                ly = fy * o->logical_h / o->fb_h;
            }
            uint32_t p = pack_fb(o, o->canvas[(size_t)ly * o->logical_w + lx]);
            if (o->bytespp == 4) ((uint32_t *)row)[fx + o->vi.xoffset] = p;
            else ((uint16_t *)row)[fx + o->vi.xoffset] = (uint16_t)p;
        }
    }
}

static uint32_t argb(unsigned a, uint32_t rgb) { return (a << 24) | (rgb & 0xFFFFFFu); }

static void rect(Overlay *o, int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > o->logical_w) w = o->logical_w - x;
    if (y + h > o->logical_h) h = o->logical_h - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) o->canvas[(size_t)yy * o->logical_w + xx] = color;
}

static void round_rect(Overlay *o, int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r < 1) { rect(o, x, y, w, h, color); return; }
    rect(o, x + r, y, w - 2 * r, h, color);
    rect(o, x, y + r, w, h - 2 * r, color);
    for (int yy = 0; yy < r; yy++) for (int xx = 0; xx < r; xx++) {
        int dx = r - xx, dy = r - yy;
        if (dx * dx + dy * dy <= r * r) {
            rect(o, x + xx, y + yy, 1, 1, color);
            rect(o, x + w - 1 - xx, y + yy, 1, 1, color);
            rect(o, x + xx, y + h - 1 - yy, 1, 1, color);
            rect(o, x + w - 1 - xx, y + h - 1 - yy, 1, 1, color);
        }
    }
}

static int text_width(const char *s, int scale) { return (int)strlen(s) * 8 * scale; }

static void text_draw(Overlay *o, int x, int y, const char *s, int scale, uint32_t color, int max_w) {
    int start = x;
    for (; *s && x + 8 * scale <= start + max_w; s++, x += 8 * scale) {
        unsigned char c = (unsigned char)*s;
        if (c >= 128) c = '?';
        for (int row = 0; row < 8; row++) {
            unsigned char bits = fontdata8x8[c * 8 + row];
            for (int col = 0; col < 8; col++)
                if (bits & (0x80u >> col)) rect(o, x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

static void format_time(int64_t ms, char *out, size_t n) {
    if (ms < 0) ms = 0;
    long sec = (long)(ms / 1000);
    if (sec >= 3600) snprintf(out, n, "%ld:%02ld:%02ld", sec / 3600, (sec / 60) % 60, sec % 60);
    else snprintf(out, n, "%ld:%02ld", sec / 60, sec % 60);
}

static const char *display_name(const char *path, char *out, size_t n) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    snprintf(out, n, "%s", base);
    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';
    return out;
}

static void folder_name(const char *path, char *out, size_t n) {
    char parent[MAX_PATH_LEN];
    snprintf(parent, sizeof(parent), "%s", path);
    char *slash = strrchr(parent, '/');
    if (!slash || slash == parent) { snprintf(out, n, "Music"); return; }
    *slash = '\0';
    slash = strrchr(parent, '/');
    snprintf(out, n, "%s", slash ? slash + 1 : parent);
}

static int subtitle_offset_load(const char *video_path) {
    FILE *f = fopen(SUBTITLE_OFFSET_FILE, "r");
    char line[1200];
    int result = 0;
    if (!f) return 0;
    while (fgets(line, sizeof(line), f)) {
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab++ = '\0';
        tab[strcspn(tab, "\r\n")] = '\0';
        if (!strcmp(tab, video_path)) result = atoi(line);
    }
    fclose(f);
    if (result < -SUBTITLE_OFFSET_LIMIT_MS) result = -SUBTITLE_OFFSET_LIMIT_MS;
    if (result > SUBTITLE_OFFSET_LIMIT_MS) result = SUBTITLE_OFFSET_LIMIT_MS;
    return result - result % 100;
}

static void subtitle_offset_save(const char *video_path, int offset_ms) {
    mkdir("/mnt/sdcard/frogui", 0777);
    FILE *src = fopen(SUBTITLE_OFFSET_FILE, "r");
    FILE *dst = fopen(SUBTITLE_OFFSET_TMP, "w");
    char line[1200];
    if (!dst) { if (src) fclose(src); return; }
    while (src && fgets(line, sizeof(line), src)) {
        char copy[1200];
        snprintf(copy, sizeof(copy), "%s", line);
        char *tab = strchr(copy, '\t');
        if (tab) {
            *tab++ = '\0';
            tab[strcspn(tab, "\r\n")] = '\0';
            if (!strcmp(tab, video_path)) continue;
        }
        fputs(line, dst);
    }
    if (src) fclose(src);
    if (offset_ms) fprintf(dst, "%d\t%s\n", offset_ms, video_path);
    fflush(dst);
    fsync(fileno(dst));
    fclose(dst);
    if (rename(SUBTITLE_OFFSET_TMP, SUBTITLE_OFFSET_FILE) == 0) sync();
    else unlink(SUBTITLE_OFFSET_TMP);
}

static bool subtitle_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && (!strcasecmp(dot, ".srt") || !strcasecmp(dot, ".vtt"));
}

/* Only an exact same-basename SRT/VTT is selected automatically. This avoids
 * guessing a language or loading an unrelated subtitle file. */
static bool find_sidecar_subtitle(const char *video, char *out, size_t out_size) {
    char directory[MAX_PATH_LEN], prefix[256], selected[256] = "";
    const char *slash = strrchr(video, '/');
    const char *base = slash ? slash + 1 : video;
    size_t dir_len = slash ? (size_t)(slash - video) : 1;
    if (dir_len >= sizeof(directory)) return false;
    if (slash) { memcpy(directory, video, dir_len); directory[dir_len] = '\0'; }
    else snprintf(directory, sizeof(directory), ".");
    snprintf(prefix, sizeof(prefix), "%s", base);
    char *dot = strrchr(prefix, '.');
    if (dot) *dot = '\0';
    size_t prefix_len = strlen(prefix);
    DIR *dir = opendir(directory);
    if (!dir) return false;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || !subtitle_extension(entry->d_name) ||
            (size_t)(ext - entry->d_name) != prefix_len ||
            strncasecmp(entry->d_name, prefix, prefix_len)) continue;
        if (!selected[0] || !strcasecmp(ext, ".srt")) {
            snprintf(selected, sizeof(selected), "%s", entry->d_name);
            if (!strcasecmp(ext, ".srt")) break;
        }
    }
    closedir(dir);
    if (!selected[0]) return false;
    return snprintf(out, out_size, "%s/%s", directory, selected) < (int)out_size;
}

static void subtitle_text_clean(char *dst, size_t n, const char *src) {
    size_t j = 0;
    if (!n) return;
    for (size_t i = 0; src && src[i] && j + 1 < n; i++) {
        if (src[i] == '{') { while (src[i] && src[i] != '}') i++; continue; }
        if (src[i] == '<') { while (src[i] && src[i] != '>') i++; continue; }
        if (src[i] == '\\' && (src[i + 1] == 'N' || src[i + 1] == 'n')) {
            dst[j++] = '\n'; i++; continue;
        }
        if (src[i] != '\r') dst[j++] = src[i];
    }
    dst[j] = '\0';
}

static int64_t subtitle_time_parse(const char *text) {
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

static void subtitles_clear(void) {
    free(subtitle_cues);
    subtitle_cues = NULL;
    subtitle_cue_count = 0;
    subtitle_cue_cursor = 0;
}

/* H.OS 1.2 crashes when external subtitle streams are passed into ffplayer.
 * Parse timed text locally and render it on fb1 instead. */
static int subtitles_load(const char *path) {
    subtitles_clear();
    if (!path || !subtitle_extension(path)) return 0;
    FILE *f = fopen(path, "r");
    char line[1200];
    if (!f) return 0;
    while (subtitle_cue_count < 4096 && fgets(line, sizeof(line), f)) {
        char *arrow = strstr(line, "-->");
        if (!arrow) continue;
        *arrow = '\0';
        int64_t start = subtitle_time_parse(line);
        int64_t end = subtitle_time_parse(arrow + 3);
        if (start < 0 || end <= start) continue;
        char joined[1024] = {0};
        size_t used = 0;
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (!line[0]) break;
            char clean[1024] = {0};
            subtitle_text_clean(clean, sizeof(clean), line);
            size_t length = strlen(clean);
            if (!length) continue;
            if (used && used + 1 < sizeof(joined)) joined[used++] = '\n';
            if (length > sizeof(joined) - used - 1) length = sizeof(joined) - used - 1;
            memcpy(joined + used, clean, length);
            used += length;
            joined[used] = '\0';
        }
        if (!joined[0]) continue;
        TimedCue *next = realloc(subtitle_cues,
            (size_t)(subtitle_cue_count + 1) * sizeof(*subtitle_cues));
        if (!next) break;
        subtitle_cues = next;
        TimedCue *cue = &subtitle_cues[subtitle_cue_count++];
        cue->start_ms = start;
        cue->end_ms = end;
        snprintf(cue->text, sizeof(cue->text), "%s", joined);
    }
    fclose(f);
    char message[96];
    snprintf(message, sizeof(message), "loaded %d local subtitle cues", subtitle_cue_count);
    log_step(message);
    return subtitle_cue_count;
}

static int subtitle_at(int64_t position_ms, int offset_ms, bool enabled) {
    if (!enabled || !subtitle_cues || subtitle_cue_count <= 0) return -1;
    int64_t cue_time = position_ms - offset_ms;
    if (subtitle_cue_cursor >= subtitle_cue_count ||
        cue_time < subtitle_cues[subtitle_cue_cursor].start_ms)
        subtitle_cue_cursor = 0;
    while (subtitle_cue_cursor < subtitle_cue_count &&
           cue_time >= subtitle_cues[subtitle_cue_cursor].end_ms)
        subtitle_cue_cursor++;
    if (subtitle_cue_cursor < subtitle_cue_count &&
        cue_time >= subtitle_cues[subtitle_cue_cursor].start_ms)
        return subtitle_cue_cursor;
    return -1;
}

typedef struct {
    char title[128];
    char artist[128];
    char album[128];
} TrackInfo;

static uint32_t id3_be32(const unsigned char *p);
static uint32_t id3_syncsafe(const unsigned char *p);

static void trim_field(char *s, size_t n) {
    s[n - 1] = '\0';
    size_t len = strlen(s);
    while (len && (s[len - 1] == ' ' || s[len - 1] == '\0')) s[--len] = '\0';
}

static void decode_id3_text(const unsigned char *data, size_t size, char *out, size_t out_size) {
    if (!size || out_size < 2) return;
    unsigned encoding = data[0];
    data++; size--;
    size_t used = 0;
    if (encoding == 1 || encoding == 2) {
        bool little_endian = encoding == 1 && size >= 2 && data[0] == 0xFF && data[1] == 0xFE;
        if (encoding == 1 && size >= 2 && ((data[0] == 0xFF && data[1] == 0xFE) ||
                                           (data[0] == 0xFE && data[1] == 0xFF))) {
            data += 2; size -= 2;
        }
        for (size_t i = 0; i + 1 < size && used + 1 < out_size; i += 2) {
            unsigned value = little_endian ? (unsigned)data[i] | ((unsigned)data[i + 1] << 8)
                                           : ((unsigned)data[i] << 8) | data[i + 1];
            if (!value) break;
            out[used++] = value >= 32 && value < 127 ? (char)value : '?';
        }
    } else {
        while (used < size && used + 1 < out_size && data[used]) {
            unsigned char value = data[used];
            out[used] = value >= 32 || value >= 0x80 ? (char)value : ' ';
            used++;
        }
    }
    out[used] = '\0';
    trim_field(out, out_size);
}

static void load_id3v2_metadata(FILE *f, TrackInfo *info) {
    unsigned char header[10];
    rewind(f);
    if (fread(header, 1, sizeof(header), f) != sizeof(header) || memcmp(header, "ID3", 3)) return;
    int version = header[3];
    if (version < 3 || version > 4) return;
    uint32_t tag_size = id3_syncsafe(header + 6);
    unsigned char *tag = malloc(tag_size);
    if (!tag || fread(tag, 1, tag_size, f) != tag_size) { free(tag); return; }
    size_t pos = 0;
    while (pos + 10 <= tag_size) {
        unsigned char *frame = tag + pos;
        if (!frame[0] || !frame[1] || !frame[2] || !frame[3]) break;
        uint32_t frame_size = version == 4 ? id3_syncsafe(frame + 4) : id3_be32(frame + 4);
        pos += 10;
        if (!frame_size || frame_size > tag_size - pos) break;
        if (!memcmp(frame, "TIT2", 4)) decode_id3_text(tag + pos, frame_size, info->title, sizeof(info->title));
        else if (!memcmp(frame, "TPE1", 4)) decode_id3_text(tag + pos, frame_size, info->artist, sizeof(info->artist));
        else if (!memcmp(frame, "TALB", 4)) decode_id3_text(tag + pos, frame_size, info->album, sizeof(info->album));
        pos += frame_size;
    }
    free(tag);
}

static void load_track_info(const char *path, TrackInfo *info) {
    memset(info, 0, sizeof(*info));
    FILE *f = fopen(path, "rb");
    if (!f) { display_name(path, info->title, sizeof(info->title)); return; }
    load_id3v2_metadata(f, info);
    if (fseek(f, -128, SEEK_END) == 0) {
        unsigned char tag[128];
        if (fread(tag, 1, sizeof(tag), f) == sizeof(tag) && !memcmp(tag, "TAG", 3)) {
            if (!info->title[0]) { memcpy(info->title, tag + 3, 30); trim_field(info->title, sizeof(info->title)); }
            if (!info->artist[0]) { memcpy(info->artist, tag + 33, 30); trim_field(info->artist, sizeof(info->artist)); }
            if (!info->album[0]) { memcpy(info->album, tag + 63, 30); trim_field(info->album, sizeof(info->album)); }
        }
    }
    fclose(f);
    if (!info->title[0]) display_name(path, info->title, sizeof(info->title));
    if (!info->album[0]) folder_name(path, info->album, sizeof(info->album));
}

static bool is_audio_path(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    return !strcasecmp(ext, ".mp3") || !strcasecmp(ext, ".m4a") ||
           !strcasecmp(ext, ".aac") || !strcasecmp(ext, ".wav") ||
           !strcasecmp(ext, ".flac") || !strcasecmp(ext, ".ogg") ||
           !strcasecmp(ext, ".opus");
}

static bool is_media_path(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    if (is_audio_path(path)) return true;
    static const char *const video_exts[] = {
        ".mp4", ".m4v", ".mkv", ".avi", ".mov", ".qt",
        ".mpg", ".mpeg", ".mpe", ".m1v", ".m2v", ".mpv",
        ".ts", ".mts", ".m2ts", ".trp", ".tp", ".vob",
        ".webm", ".flv", ".f4v", ".3gp", ".3g2",
        ".wmv", ".asf", ".ogv", ".ogm", ".rm", ".rmvb",
        ".divx", ".xvid", ".mxf", ".nut", ".dv", ".amv",
        ".mjpeg", ".mjpg", ".m3u8", ".h264", ".264",
        ".h265", ".hevc", ".vc1", ".av1", ".y4m", NULL
    };
    for (int i = 0; video_exts[i]; i++)
        if (!strcasecmp(ext, video_exts[i])) return true;
    return false;
}

static uint32_t id3_be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint32_t id3_syncsafe(const unsigned char *p) {
    return ((uint32_t)(p[0] & 0x7F) << 21) | ((uint32_t)(p[1] & 0x7F) << 14) |
           ((uint32_t)(p[2] & 0x7F) << 7) | (p[3] & 0x7F);
}

/* Extract standard ID3v2 APIC artwork for the hardware image decoder. */
static bool extract_embedded_cover(const char *media_path, char *out, size_t out_size) {
    FILE *f = fopen(media_path, "rb");
    unsigned char header[10];
    if (!f || fread(header, 1, sizeof(header), f) != sizeof(header) || memcmp(header, "ID3", 3) != 0) {
        if (f) fclose(f);
        log_step("no ID3v2 tag for cover");
        return false;
    }
    int version = header[3];
    uint32_t tag_size = id3_syncsafe(header + 6);
    unsigned char *tag = malloc(tag_size);
    if (!tag || fread(tag, 1, tag_size, f) != tag_size) { free(tag); fclose(f); return false; }
    fclose(f);
    size_t pos = 0;
    while (pos + 10 <= tag_size) {
        unsigned char *frame = tag + pos;
        if (!frame[0] || !frame[1] || !frame[2] || !frame[3]) break;
        uint32_t frame_size = version >= 4 ? id3_syncsafe(frame + 4) : id3_be32(frame + 4);
        pos += 10;
        if (frame_size > tag_size - pos) break;
        if (!memcmp(frame, "APIC", 4) && frame_size > 5) {
            unsigned char *data = tag + pos;
            size_t i = 1;
            while (i < frame_size && data[i]) i++;
            if (i + 2 < frame_size) {
                i += 2; /* MIME terminator and picture type */
                /* APIC descriptions use the frame's declared text encoding.
                 * Latin-1 and UTF-8 have a one-byte terminator; UTF-16 and
                 * UTF-16BE have a two-byte terminator. */
                if (data[0] == 0 || data[0] == 3) {
                    while (i < frame_size && data[i]) i++;
                    if (i < frame_size) i++;
                } else {
                    while (i + 1 < frame_size && (data[i] || data[i + 1])) i += 2;
                    if (i + 1 < frame_size) i += 2;
                }
                if (i < frame_size) {
                    snprintf(out, out_size, "/tmp/treefrog-cover-%ld.jpg", (long)getpid());
                    FILE *cover = fopen(out, "wb");
                    if (cover) {
                        bool ok = fwrite(data + i, 1, frame_size - i, cover) == frame_size - i;
                        fclose(cover); free(tag);
                        log_step(ok ? "APIC cover extracted" : "APIC cover write failed");
                        return ok;
                    }
                }
            }
        }
        pos += frame_size;
    }
    free(tag);
    log_step("ID3v2 tag has no APIC frame");
    return false;
}

typedef struct { struct jpeg_error_mgr base; jmp_buf jump; } JpegError;

static void jpeg_fail(j_common_ptr cinfo) {
    JpegError *error = (JpegError *)cinfo->err;
    longjmp(error->jump, 1);
}

static bool cover_decode(CoverState *cover, const char *path, int target_side) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    struct jpeg_decompress_struct decoder;
    JpegError error;
    memset(&decoder, 0, sizeof(decoder));
    decoder.err = jpeg_std_error(&error.base);
    error.base.error_exit = jpeg_fail;
    if (setjmp(error.jump)) {
        jpeg_destroy_decompress(&decoder);
        fclose(f);
        free(cover->rgb);
        cover->rgb = NULL;
        log_step("cover JPEG decode failed");
        return false;
    }
    jpeg_create_decompress(&decoder);
    jpeg_stdio_src(&decoder, f);
    jpeg_read_header(&decoder, TRUE);
    while (decoder.scale_denom < 8 &&
           (decoder.image_width / decoder.scale_denom > (unsigned)target_side * 2 ||
            decoder.image_height / decoder.scale_denom > (unsigned)target_side * 2))
        decoder.scale_denom *= 2;
    decoder.out_color_space = JCS_RGB;
    jpeg_start_decompress(&decoder);
    cover->width = (int)decoder.output_width;
    cover->height = (int)decoder.output_height;
    size_t stride = (size_t)cover->width * 3;
    cover->rgb = malloc(stride * (size_t)cover->height);
    if (!cover->rgb) longjmp(error.jump, 1);
    while (decoder.output_scanline < decoder.output_height) {
        JSAMPROW row = cover->rgb + (size_t)decoder.output_scanline * stride;
        jpeg_read_scanlines(&decoder, &row, 1);
    }
    jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    fclose(f);
    log_step("cover decoded into HUD framebuffer");
    return true;
}

static void cover_prepare(CoverState *cover, const char *media_path, int target_side) {
    memset(cover, 0, sizeof(*cover));
    if (extract_embedded_cover(media_path, cover->temp_path, sizeof(cover->temp_path)))
        cover_decode(cover, cover->temp_path, target_side);
}

static void cover_stop(CoverState *cover) {
    free(cover->rgb);
    if (cover->temp_path[0]) unlink(cover->temp_path);
    memset(cover, 0, sizeof(*cover));
}

static void draw_cover(Overlay *o, const CoverState *cover, int panel_y) {
    if (!cover || !cover->rgb || cover->width <= 0 || cover->height <= 0) return;
    int margin = o->logical_w / 28;
    int side = o->logical_w * 44 / 100;
    int available = panel_y - margin * 2;
    if (side > available) side = available;
    if (side <= 0) return;
    int x0 = (o->logical_w - side) / 2, y0 = margin;
    int radius = side / 18;
    for (int y = 0; y < side; y++) {
        for (int x = 0; x < side; x++) {
            int cx = x < radius ? radius - x : x >= side - radius ? x - (side - radius - 1) : 0;
            int cy = y < radius ? radius - y : y >= side - radius ? y - (side - radius - 1) : 0;
            if (cx && cy && cx * cx + cy * cy > radius * radius) continue;
            int sx = x * cover->width / side, sy = y * cover->height / side;
            const unsigned char *p = cover->rgb + ((size_t)sy * cover->width + sx) * 3;
            o->canvas[(size_t)(y0 + y) * o->logical_w + x0 + x] =
                0xFF000000u | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
        }
    }
}

static int audio_path_compare(const void *a, const void *b) {
    return strcasecmp((const char *)a, (const char *)b);
}

static bool playlist_neighbor(const char *path, int delta, char *out, size_t out_size) {
    const char *slash = strrchr(path, '/');
    if (!slash) return false;
    char dir_path[MAX_PATH_LEN];
    size_t dir_len = (size_t)(slash - path);
    if (!dir_len || dir_len >= sizeof(dir_path)) return false;
    memcpy(dir_path, path, dir_len); dir_path[dir_len] = '\0';
    char tracks[256][MAX_PATH_LEN]; size_t count = 0;
    DIR *dp = opendir(dir_path); if (!dp) return false;
    struct dirent *e;
    while ((e = readdir(dp)) && count < 256) {
        if (e->d_name[0] == '.') continue;
        char candidate[MAX_PATH_LEN];
        snprintf(candidate, sizeof(candidate), "%s/%s", dir_path, e->d_name);
        struct stat st;
        if (is_media_path(candidate) && stat(candidate, &st) == 0 && S_ISREG(st.st_mode))
            snprintf(tracks[count++], sizeof(tracks[0]), "%s", candidate);
    }
    closedir(dp);
    if (count < 2) return false;
    qsort(tracks, count, sizeof(tracks[0]), audio_path_compare);
    size_t index = count;
    for (size_t i = 0; i < count; i++) if (!strcasecmp(tracks[i], path)) { index = i; break; }
    if (index == count) return false;
    size_t next = (size_t)((index + count + delta) % count);
    snprintf(out, out_size, "%s", tracks[next]);
    return true;
}

static bool first_media_in_folder(const char *folder, char *out, size_t out_size) {
    char tracks[256][MAX_PATH_LEN]; size_t count = 0;
    DIR *dp = opendir(folder); if (!dp) return false;
    struct dirent *e;
    while ((e = readdir(dp)) && count < 256) {
        if (e->d_name[0] == '.') continue;
        char candidate[MAX_PATH_LEN];
        snprintf(candidate, sizeof(candidate), "%s/%s", folder, e->d_name);
        struct stat st;
        if (is_media_path(candidate) && stat(candidate, &st) == 0 && S_ISREG(st.st_mode))
            snprintf(tracks[count++], sizeof(tracks[0]), "%s", candidate);
    }
    closedir(dp);
    if (!count) return false;
    qsort(tracks, count, sizeof(tracks[0]), audio_path_compare);
    snprintf(out, out_size, "%s", tracks[0]);
    return true;
}

static bool random_media_neighbor(const char *path, char *out, size_t out_size) {
    const char *slash = strrchr(path, '/');
    if (!slash) return false;
    char dir_path[MAX_PATH_LEN];
    size_t dir_len = (size_t)(slash - path);
    if (!dir_len || dir_len >= sizeof(dir_path)) return false;
    memcpy(dir_path, path, dir_len); dir_path[dir_len] = '\0';
    char tracks[256][MAX_PATH_LEN]; size_t count = 0;
    DIR *dp = opendir(dir_path); if (!dp) return false;
    struct dirent *e;
    while ((e = readdir(dp)) && count < 256) {
        if (e->d_name[0] == '.') continue;
        char candidate[MAX_PATH_LEN];
        snprintf(candidate, sizeof(candidate), "%s/%s", dir_path, e->d_name);
        struct stat st;
        if (is_media_path(candidate) && stat(candidate, &st) == 0 && S_ISREG(st.st_mode) && strcasecmp(candidate, path))
            snprintf(tracks[count++], sizeof(tracks[0]), "%s", candidate);
    }
    closedir(dp);
    if (!count) return false;
    snprintf(out, out_size, "%s", tracks[(size_t)rand() % count]);
    return true;
}

static void apply_display_mode(void *player, ScaleMode mode, int video_w, int video_h,
                               int panel_w, int panel_h) {
    /* The H.OS display-rectangle ABI uses the projector's normalized
     * 1920x1080 coordinate space, even on a 640x480 panel. */
    const int norm_w = 1920, norm_h = 1080;
    int px = 0, py = 0, pw = panel_w, ph = panel_h;
    int sx = 0, sy = 0, sw = norm_w, sh = norm_h;
    if (video_w <= 0 || video_h <= 0) { video_w = panel_w; video_h = panel_h; }
    if (mode == SCALE_FIT) {
        pw = panel_w;
        ph = (int)((int64_t)pw * video_h / video_w);
        if (ph > panel_h) {
            ph = panel_h;
            pw = (int)((int64_t)ph * video_w / video_h);
        }
        px = (panel_w - pw) / 2;
        py = (panel_h - ph) / 2;
    } else if (mode == SCALE_FILL) {
        if ((int64_t)video_w * panel_h > (int64_t)video_h * panel_w) {
            sw = (int)((int64_t)norm_h * panel_w / panel_h);
            sx = (norm_w - sw) / 2;
        } else {
            sh = (int)((int64_t)norm_w * panel_h / panel_w);
            sy = (norm_h - sh) / 2;
        }
    } else if (mode == SCALE_ORIGINAL) {
        pw = video_w;
        ph = video_h;
        if (pw > panel_w || ph > panel_h) {
            if ((int64_t)pw * panel_h > (int64_t)ph * panel_w) {
                ph = (int)((int64_t)panel_w * ph / pw);
                pw = panel_w;
            } else {
                pw = (int)((int64_t)panel_h * pw / ph);
                ph = panel_h;
            }
        }
        px = (panel_w - pw) / 2;
        py = (panel_h - ph) / 2;
    }
    struct vdec_dis_rect rect = {
        {(uint16_t)sx, (uint16_t)sy, (uint16_t)sw, (uint16_t)sh},
        {(uint16_t)((int64_t)px * norm_w / panel_w),
         (uint16_t)((int64_t)py * norm_h / panel_h),
         (uint16_t)((int64_t)pw * norm_w / panel_w),
         (uint16_t)((int64_t)ph * norm_h / panel_h)}
    };
    int rc = hcplayer_set_display_rect(player, &rect);
    char message[160];
    snprintf(message, sizeof(message),
             "scale=%s video=%dx%d panel=%dx%d dst=%dx%d+%d+%d rc=%d",
             scale_name(mode), video_w, video_h, panel_w, panel_h,
             pw, ph, px, py, rc);
    log_step(message);
}

static int subtitle_wrap(const char *text, char lines[4][160], int max_chars) {
    int count = 0;
    const char *cursor = text;
    while (cursor && *cursor && count < 4) {
        while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r') cursor++;
        if (!*cursor) break;
        size_t remaining = strcspn(cursor, "\r");
        const char *newline = strchr(cursor, '\n');
        size_t take = strlen(cursor);
        if (newline && (size_t)(newline - cursor) < take) take = (size_t)(newline - cursor);
        if (take > (size_t)max_chars) {
            take = (size_t)max_chars;
            size_t split = take;
            while (split > 0 && cursor[split] != ' ') split--;
            if (split > (size_t)max_chars / 2) take = split;
        }
        if (take > remaining) take = remaining;
        if (take >= sizeof(lines[0])) take = sizeof(lines[0]) - 1;
        memcpy(lines[count], cursor, take);
        lines[count][take] = '\0';
        while (take && lines[count][take - 1] == ' ') lines[count][--take] = '\0';
        if (take) count++;
        cursor += take;
        while (*cursor == ' ') cursor++;
        if (*cursor == '\n' || *cursor == '\r') cursor++;
    }
    return count;
}

static void draw_subtitle(Overlay *o, const char *subtitle, int bottom_y) {
    if (!subtitle || !*subtitle) return;
    int scale = o->logical_w >= 800 ? 2 : 1;
    int max_chars = (o->logical_w - 80) / (8 * scale);
    if (max_chars < 20) max_chars = 20;
    char lines[4][160] = {{0}};
    int count = subtitle_wrap(subtitle, lines, max_chars);
    if (!count) return;
    int line_h = 11 * scale;
    int box_h = count * line_h + 16 * scale;
    int y = bottom_y - box_h;
    if (y < 8) y = 8;
    round_rect(o, 28, y, o->logical_w - 56, box_h, 10,
               argb(210, 0x080B10));
    for (int i = 0; i < count; i++) {
        int width = text_width(lines[i], scale);
        text_draw(o, (o->logical_w - width) / 2,
                  y + 8 * scale + i * line_h, lines[i], scale,
                  argb(255, 0xFFFFFF), o->logical_w - 72);
    }
}

static void draw_pause_menu(Overlay *o, Theme theme, int64_t pos, int64_t duration,
                            int selected, ScaleMode scale_mode,
                            bool subtitle_available, bool subtitles_enabled,
                            int subtitle_offset_ms, PlaybackMode playback) {
    if (!o->canvas) return;
    memset(o->canvas, 0, (size_t)o->logical_w * o->logical_h * sizeof(*o->canvas));
    int x = 40, y = 45, w = o->logical_w - 80, h = o->logical_h - 90;
    if (w < 360) { x = 18; w = o->logical_w - 36; }
    round_rect(o, x, y, w, h, 18, argb(242, 0x101620));
    rect(o, x + 14, y, w - 28, 4, argb(255, theme.accent));
    text_draw(o, x + 24, y + 19, "VIDEO PAUSED", 2, argb(255, theme.text), w - 48);

    char time_line[64], left[24], right[24];
    format_time(pos, left, sizeof(left));
    format_time(duration, right, sizeof(right));
    snprintf(time_line, sizeof(time_line), "%s / %s", left, right);
    text_draw(o, x + 24, y + 48, time_line, 1, argb(255, 0xB9C7D8), w - 48);
    int bar_x = x + 24, bar_y = y + 67, bar_w = w - 48;
    round_rect(o, bar_x, bar_y, bar_w, 6, 3, argb(255, 0x3A4655));
    int fill = duration > 0 ? (int)((int64_t)bar_w * pos / duration) : 0;
    if (fill > bar_w) fill = bar_w;
    if (fill > 0) round_rect(o, bar_x, bar_y, fill, 6, 3, argb(255, theme.accent));

    static const char *labels[] = {
        "RESUME", "VIDEO SIZE", "SUBTITLES", "SUBTITLE TIMING",
        "PLAYBACK MODE", "EXIT VIDEO"
    };
    int row_y = y + 91;
    int step = (h - 132) / 6;
    if (step < 34) step = 34;
    for (int i = 0; i < 6; i++) {
        int yy = row_y + i * step;
        if (i == selected)
            round_rect(o, x + 14, yy - 9, w - 28, step - 4, 8,
                       argb(255, 0x263748));
        text_draw(o, x + 25, yy, i == selected ? ">" : " ", 1,
                  argb(255, theme.accent), 16);
        text_draw(o, x + 45, yy, labels[i], 1, argb(255, theme.text), w - 190);
        const char *value = NULL;
        char value_buffer[32];
        if (i == 1) value = scale_name(scale_mode);
        else if (i == 2) value = !subtitle_available ? "NOT FOUND" :
                                 subtitles_enabled ? "AUTO" : "OFF";
        else if (i == 3) {
            snprintf(value_buffer, sizeof(value_buffer), "%+d MS", subtitle_offset_ms);
            value = value_buffer;
        } else if (i == 4) value = playback_name(playback);
        if (value) {
            int value_w = text_width(value, 1);
            text_draw(o, x + w - 24 - value_w, yy, value, 1,
                      argb(255, subtitle_available || i != 2 ? theme.accent : 0x7D8792),
                      value_w);
        }
    }
    text_draw(o, x + 24, y + h - 23,
              "UP/DOWN MOVE  LEFT/RIGHT CHANGE  A OK  B RESUME", 1,
              argb(255, 0xB9C7D8), w - 48);
    overlay_present(o);
}

static void draw_subtitle_only(Overlay *o, const char *subtitle) {
    if (!o->canvas) return;
    memset(o->canvas, 0, (size_t)o->logical_w * o->logical_h * sizeof(*o->canvas));
    draw_subtitle(o, subtitle, o->logical_h - 18);
    overlay_present(o);
}

static void draw_hud(Overlay *o, Theme t, const TrackInfo *info, int64_t pos, int64_t duration,
                     bool paused, PlaybackMode playback, const char *status,
                     const CoverState *cover, bool opaque_background,
                     const char *subtitle) {
    if (!o->canvas) return;
    uint32_t clear = opaque_background ? argb(255, t.background) : 0;
    for (size_t i = 0; i < (size_t)o->logical_w * o->logical_h; i++) o->canvas[i] = clear;
    int scale = o->logical_w >= 800 ? 2 : 1;
    int margin = o->logical_w / 28;
    int panel_h = scale == 2 ? 136 : 112;
    int panel_y = o->logical_h - panel_h - margin;
    draw_cover(o, cover, panel_y);
    round_rect(o, margin, panel_y, o->logical_w - margin * 2, panel_h, 24, argb(232, 0x101010));

    char title[256], byline[256], times[64];
    snprintf(title, sizeof(title), "%s", info->title);
    if (info->artist[0] && info->album[0])
        snprintf(byline, sizeof(byline), "%s  |  %s", info->artist, info->album);
    else if (info->artist[0]) snprintf(byline, sizeof(byline), "%s", info->artist);
    else snprintf(byline, sizeof(byline), "%s", info->album);
    format_time(pos, times, sizeof(times));
    size_t used = strlen(times);
    snprintf(times + used, sizeof(times) - used, " / ");
    format_time(duration, times + strlen(times), sizeof(times) - strlen(times));

    int left = margin + 18;
    int right = o->logical_w - margin - 18;
    const char *mode_label = playback == PLAY_REPEAT ? "LOOP" : playback == PLAY_RANDOM ? "RANDOM" : "SEQUENTIAL";
    text_draw(o, left, panel_y - 20, mode_label, scale, argb(255, t.accent), right - left);
    text_draw(o, left, panel_y + 12, title, scale, argb(255, t.text), right - left);
    int tw = text_width(times, scale);
    text_draw(o, right - tw, panel_y + 10, times, scale, argb(255, t.text), tw);

    if (byline[0])
        text_draw(o, left, panel_y + (scale == 2 ? 38 : 30), byline, scale,
                  argb(255, t.accent), right - left);

    int bar_y = panel_y + (scale == 2 ? 68 : 54);
    round_rect(o, left, bar_y, right - left, 10, 5, argb(255, 0x3A3A3A));
    int fill = duration > 0 ? (int)((right - left) * pos / duration) : 0;
    if (fill > right - left) fill = right - left;
    if (fill > 0) round_rect(o, left, bar_y, fill, 10, 5, argb(255, t.accent));

    const char *help = "A PAUSE   X/Y TRACK   SELECT MODE   B BACK";
    text_draw(o, left, bar_y + 26, help, scale, argb(190, t.text), right - left);
    int mode_width = text_width(mode_label, scale) + 20;
    round_rect(o, left, panel_y - 38, mode_width, 26, 13, argb(235, t.accent));
    text_draw(o, left + 10, panel_y - 31, mode_label, scale, argb(255, t.selected_text), mode_width - 20);
    if (status && *status) {
        int sw = text_width(status, scale) + 24;
        round_rect(o, (o->logical_w - sw) / 2, panel_y - 46, sw, 34, 10, argb(245, t.accent));
        text_draw(o, (o->logical_w - text_width(status, scale)) / 2, panel_y - 38,
                  status, scale, argb(255, t.selected_text), sw);
    } else if (paused) {
        const char *label = "PAUSED";
        int sw = text_width(label, scale) + 24;
        round_rect(o, (o->logical_w - sw) / 2, panel_y - 46, sw, 34, 10, argb(245, t.accent));
        text_draw(o, (o->logical_w - text_width(label, scale)) / 2, panel_y - 38,
                  label, scale, argb(255, t.selected_text), sw);
    }
    draw_subtitle(o, subtitle, panel_y - 48);
    overlay_present(o);
}

static bool player_error(long type, bool audio_only) {
    return type == HCPLAYER_MSG_OPEN_FILE_FAILED || type == HCPLAYER_MSG_UNSUPPORT_FORMAT ||
           (!audio_only && (type == HCPLAYER_MSG_UNSUPPORT_ALL_VIDEO ||
                            type == HCPLAYER_MSG_VIDEO_DECODE_ERR)) ||
           type == HCPLAYER_MSG_ERR_UNDEFINED || type == HCPLAYER_MSG_READ_TIMEOUT;
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    char folder_entry[MAX_PATH_LEN];
    struct stat launch_stat;
    if (stat(argv[1], &launch_stat) == 0 && S_ISDIR(launch_stat.st_mode)) {
        if (!first_media_in_folder(argv[1], folder_entry, sizeof(folder_entry))) return 2;
        argv[1] = folder_entry;
    }
    bool audio_only = is_audio_path(argv[1]);
    log_step("process started");
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    load_keymap();
    screen_rotation_load();
    Theme theme = load_theme();
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    PlaybackMode playback = load_playback_mode();
    ScaleMode scale_mode = load_scale_mode();
    char subtitle_path[MAX_PATH_LEN] = "";
    bool subtitle_available = !audio_only &&
        find_sidecar_subtitle(argv[1], subtitle_path, sizeof(subtitle_path)) &&
        subtitles_load(subtitle_path) > 0;
    bool subtitles_enabled = subtitle_available;
    int subtitle_offset_ms = subtitle_offset_load(argv[1]);
    TrackInfo track_info;
    load_track_info(argv[1], &track_info);
    bool restart_requested = false;
    char restart_path[MAX_PATH_LEN] = "";
    volatile uint32_t *raw_keys = open_keys();
    Overlay overlay;
    bool have_overlay = overlay_open(&overlay) == 0;
    int panel_rotation = 0, panel_w = 0, panel_h = 0;
    read_device_geometry(&panel_w, &panel_h, &panel_rotation);
    if (have_overlay && screen_rotation_180)
        overlay.rotation = (overlay.rotation + 180) % 360;
    char geometry_log[128];
    snprintf(geometry_log, sizeof(geometry_log),
             "panel=%dx%d rotate=%d fb1=%dx%d",
             panel_w, panel_h, panel_rotation,
             have_overlay ? overlay.fb_w : 0, have_overlay ? overlay.fb_h : 0);
    log_step(geometry_log);
    log_step(have_overlay ? "fb1 overlay ready" : "fb1 overlay unavailable");
    if (have_overlay) overlay_clear(&overlay);

    int msg_id = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
    if (msg_id < 0) msg_id = -1;
    log_step("calling hcplayer_init");
    if (hcplayer_init(LOG_WARNING) != 0) {
        log_step("hcplayer_init failed");
        if (have_overlay) draw_hud(&overlay, theme, &track_info, 0, 0, true, playback,
                                   "PLAYER INIT FAILED", NULL,
                                   audio_only && theme.background_enabled, NULL);
        sleep(3);
        goto done;
    }
    log_step("hcplayer_init returned");

    /* The R36SX/SF3000 stock player passes a 248-byte init block, while the
     * public SDK header describes an older 224-byte prefix. The field offsets
     * in that prefix match, but passing only sizeof(HCPlayerInitArgs) leaves
     * the firmware's appended fields as random stack data. Keep the known
     * prefix typed and guarantee the complete firmware block is zeroed. */
    union {
        long double alignment;
        unsigned char raw[256];
    } args_storage;
    memset(&args_storage, 0, sizeof(args_storage));
    HCPlayerInitArgs *args = (HCPlayerInitArgs *)args_storage.raw;
    args->uri = argv[1];
    args->msg_id = msg_id;
    args->sync_type = HCPLAYER_AUDIO_MASTER;
    int video_rotation = effective_rotation(panel_rotation);
    if (video_rotation) {
        args->rotate_enable = true;
        args->rotate_type = (rotate_type_e)(video_rotation / 90);
    }
    /* Match the stock hcprojector player setup. quick_mode and audsink are
     * intentionally left disabled; the firmware's audio-master path owns the
     * decoder/I2SO devices directly. */
    /* Audio must never own the decoded-video plane. Album art is decoded once
     * and composited into fb1 with the HUD, avoiding two hardware players
     * racing to display the attached picture. */
    args->bg_disable = true;
    args->play_attached_file = 0;
    args->snd_devs = AUDDEV_I2SO;

    log_step("calling hcplayer_create");
    void *player = hcplayer_create(args);
    if (!player) {
        if (have_overlay) draw_hud(&overlay, theme, &track_info, 0, 0, true, playback,
                                   audio_only ? "CANNOT OPEN AUDIO" : "CANNOT OPEN VIDEO", NULL,
                                   audio_only && theme.background_enabled, NULL);
        sleep(3);
        hcplayer_deinit();
        goto done;
    }
    log_step("hcplayer_create returned");
    configure_video_layer();
    hcplayer_play(player);
    CoverState cover = {0};
    if (audio_only && have_overlay)
        cover_prepare(&cover, argv[1], overlay.logical_w * 44 / 100);
    log_step("playback started");

    uint32_t previous = logical_keys(raw_keys);
    /* Seed edge detection with the launch button's current state. There is no
     * need to wait for every raw bit to clear: a stale cubevol bit could make
     * that guard permanent, whereas seeding prevents the A press leaking in. */
    log_step("entering playback loop");
    bool paused = false, eos = false;
    bool ready = false;
    bool rotation_latched = false;
    int menu_index = 0;
    int video_w = panel_w, video_h = panel_h;
    int64_t fatal_at = 0;
    int64_t playback_started_at = now_ms();
    int64_t last_progress_at = playback_started_at;
    int64_t last_progress_pos = -1;
    int64_t hud_until = playback_started_at + 4500;
    int64_t last_draw = 0;
    bool first_frame = false;
    bool background_cleared = false;
    bool display_mode_pending = false;
    int64_t display_mode_apply_at = 0;
    bool overlay_has_content = false;
    bool last_controls_visible = true;
    int last_subtitle = -2;
    const char *status = NULL;

    while (!quit_requested && !eos) {
        HCPlayerMsg msg;
        if (msg_id >= 0) while (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT) >= 0) {
            log_message(msg.type, msg.val);
            if (msg.type == HCPLAYER_MSG_STATE_EOS) eos = true;
            else if (msg.type == HCPLAYER_MSG_STATE_READY) {
                HCPlayerVideoInfo info;
                memset(&info, 0, sizeof(info));
                if (hcplayer_get_cur_video_stream_info(player, &info) == 0 &&
                    info.width > 0 && info.height > 0) {
                    video_w = info.width;
                    video_h = info.height;
                }
                ready = true;
            }
            else if (msg.type == HCPLAYER_MSG_FIRST_VIDEO_FRAME_DECODED ||
                     msg.type == HCPLAYER_MSG_FIRST_VIDEO_FRAME_SHOWED) first_frame = true;
            else if (player_error(msg.type, audio_only)) {
                status = audio_only ? "UNSUPPORTED AUDIO" : "UNSUPPORTED VIDEO";
                fatal_at = now_ms() + 3500;
                hud_until = fatal_at;
            }
        }

        uint32_t keys = logical_keys(raw_keys);
        uint32_t pressed = keys & ~previous;
        previous = keys;
        int64_t pos = hcplayer_get_position(player);
        int64_t duration = hcplayer_get_duration(player);
        int64_t now = now_ms();

        /* Never strand the user on a broken firmware decode path. Position
         * advancement is also accepted because some library builds omit the
         * first-frame messages. */
        if (pos > 250) first_frame = true;
        if (!audio_only && !ready && pos > 250) {
            HCPlayerVideoInfo info;
            memset(&info, 0, sizeof(info));
            if (hcplayer_get_cur_video_stream_info(player, &info) == 0 &&
                info.width > 0 && info.height > 0) {
                video_w = info.width;
                video_h = info.height;
            }
            ready = true;
        }
        if (!audio_only && first_frame && !background_cleared) {
            clear_frontend_background();
            background_cleared = true;
            log_step("upstream startup complete; manual sizing now enabled");
        }
        if (!first_frame && now - playback_started_at > 10000) {
            log_step(audio_only ? "startup watchdog: no audio progress" :
                                  "startup watchdog: no video frame");
            if (have_overlay)
                draw_hud(&overlay, theme, &track_info, pos, duration, true, playback,
                         audio_only ? "AUDIO START FAILED" : "VIDEO START FAILED", &cover,
                         audio_only && theme.background_enabled, NULL);
            sleep(2);
            break;
        }
        if (paused) {
            last_progress_at = now;
            last_progress_pos = pos;
        } else if (first_frame && pos >= 0) {
            if (last_progress_pos < 0 || pos > last_progress_pos + 100 ||
                pos + 1000 < last_progress_pos) {
                last_progress_pos = pos;
                last_progress_at = now;
            } else if (now - last_progress_at > PLAYBACK_STALL_MS) {
                log_step("playback watchdog: timestamp stopped advancing");
                if (have_overlay)
                    draw_hud(&overlay, theme, &track_info, pos, duration, true, playback,
                             "VIDEO PLAYBACK STALLED", &cover, !audio_only, NULL);
                sleep(2);
                break;
            }
        }

        bool rotation_chord = (keys & ((1u << BTN_FN) | (1u << BTN_L1) |
                                                   (1u << BTN_R1))) ==
                              ((1u << BTN_FN) | (1u << BTN_L1) | (1u << BTN_R1));
        if (rotation_chord && !rotation_latched) {
            screen_rotation_180 = !screen_rotation_180;
            screen_rotation_save();
            int rotation = effective_rotation(panel_rotation);
            hcplayer_change_rotate_type(player, (rotate_type_e)(rotation / 90));
            if (have_overlay) overlay.rotation = (overlay.rotation + 180) % 360;
            if (first_frame && !audio_only)
                apply_display_mode(player, scale_mode, video_w, video_h,
                                   panel_w, panel_h);
            rotation_latched = true;
            status = screen_rotation_180 ? "ROTATED 180" : "ROTATION NORMAL";
            hud_until = now + 2500;
        } else if (!rotation_chord) {
            rotation_latched = false;
        }

        bool exit_now = false;
        if (rotation_chord) {
            /* Consume the shoulders while the rotation chord is held. */
        } else if (paused) {
            if (pressed & (1u << BTN_B)) {
                log_step("pause menu: resume requested");
                hcplayer_resume(player);
                log_step("pause menu: resume returned");
                paused = false;
                if (display_mode_pending) {
                    /* H.OS restarts its decoder from inside
                     * hcplayer_set_display_rect(). Calling it while paused
                     * therefore desynchronizes our pause state and a later
                     * hcplayer_resume() can wedge the vendor player. Apply one
                     * final rectangle only after normal playback has resumed. */
                    display_mode_apply_at = now + 250;
                    log_step("scale change queued after resume");
                }
                status = NULL;
                hud_until = now + 3500;
            } else if (pressed & (1u << BTN_UP)) {
                menu_index = (menu_index + 5) % 6;
            } else if (pressed & (1u << BTN_DOWN)) {
                menu_index = (menu_index + 1) % 6;
            } else if (pressed & ((1u << BTN_LEFT) | (1u << BTN_RIGHT))) {
                int direction = (pressed & (1u << BTN_RIGHT)) ? 1 : -1;
                if (menu_index == 1 && !audio_only) {
                    scale_mode = (ScaleMode)(((int)scale_mode + SCALE_COUNT + direction) % SCALE_COUNT);
                    save_scale_mode(scale_mode);
                    display_mode_pending = first_frame;
                    log_step("pause menu: scale selection changed; apply deferred");
                } else if (menu_index == 2 && subtitle_available) {
                    subtitles_enabled = !subtitles_enabled;
                } else if (menu_index == 3 && subtitle_available) {
                    subtitle_offset_ms += direction * 100;
                    if (subtitle_offset_ms < -SUBTITLE_OFFSET_LIMIT_MS)
                        subtitle_offset_ms = -SUBTITLE_OFFSET_LIMIT_MS;
                    if (subtitle_offset_ms > SUBTITLE_OFFSET_LIMIT_MS)
                        subtitle_offset_ms = SUBTITLE_OFFSET_LIMIT_MS;
                    subtitle_offset_save(argv[1], subtitle_offset_ms);
                } else if (menu_index == 4) {
                    playback = (PlaybackMode)(((int)playback + 3 + direction) % 3);
                    save_playback_mode(playback);
                }
            } else if (pressed & ((1u << BTN_A) | (1u << BTN_START))) {
                if (menu_index == 0) {
                    log_step("pause menu: resume requested");
                    hcplayer_resume(player);
                    log_step("pause menu: resume returned");
                    paused = false;
                    if (display_mode_pending) {
                        display_mode_apply_at = now + 250;
                        log_step("scale change queued after resume");
                    }
                    status = NULL;
                    hud_until = now + 3500;
                } else if (menu_index == 1 && !audio_only) {
                    scale_mode = (ScaleMode)(((int)scale_mode + 1) % SCALE_COUNT);
                    save_scale_mode(scale_mode);
                    display_mode_pending = first_frame;
                    log_step("pause menu: scale selection changed; apply deferred");
                } else if (menu_index == 2 && subtitle_available) {
                    subtitles_enabled = !subtitles_enabled;
                } else if (menu_index == 3 && subtitle_available) {
                    subtitle_offset_ms = 0;
                    subtitle_offset_save(argv[1], subtitle_offset_ms);
                } else if (menu_index == 4) {
                    playback = (PlaybackMode)(((int)playback + 1) % 3);
                    save_playback_mode(playback);
                } else if (menu_index == 5) {
                    exit_now = true;
                }
            }
        } else if (pressed & (1u << BTN_B)) {
            exit_now = true;
        } else if (pressed & (1u << BTN_SELECT)) {
            playback = (PlaybackMode)(((int)playback + 1) % 3);
            save_playback_mode(playback);
            status = playback_name(playback);
            hud_until = now + 2500;
        } else if ((pressed & (1u << BTN_X)) &&
                   playlist_neighbor(argv[1], -1, restart_path, sizeof(restart_path))) {
            restart_requested = true;
            exit_now = true;
        } else if ((pressed & (1u << BTN_Y)) &&
                   playlist_neighbor(argv[1], 1, restart_path, sizeof(restart_path))) {
            restart_requested = true;
            exit_now = true;
        } else if (pressed & ((1u << BTN_A) | (1u << BTN_START))) {
            log_step("pause menu: pause requested");
            hcplayer_pause(player);
            log_step("pause menu: pause returned");
            paused = true;
            menu_index = 0;
            status = NULL;
        } else {
            int64_t jump = 0;
            if (pressed & (1u << BTN_LEFT)) jump = -10000;
            if (pressed & (1u << BTN_RIGHT)) jump = 10000;
            if (pressed & (1u << BTN_L1)) jump = -60000;
            if (pressed & (1u << BTN_R1)) jump = 60000;
            if (jump) {
                int64_t target = pos + jump;
                if (target < 0) target = 0;
                if (duration > 0 && target > duration - 250) target = duration - 250;
                hcplayer_seek(player, target);
                last_progress_pos = target;
                last_progress_at = now;
                status = jump > 0 ? "SEEK FORWARD" : "SEEK BACK";
                hud_until = now + 2500;
            }
        }
        if (exit_now) break;

        if (!paused && display_mode_pending && display_mode_apply_at > 0 &&
            now >= display_mode_apply_at) {
            log_step("applying deferred scale change during playback");
            apply_display_mode(player, scale_mode, video_w, video_h,
                               panel_w, panel_h);
            display_mode_pending = false;
            display_mode_apply_at = 0;
            last_progress_pos = pos;
            last_progress_at = now;
        }

        int cue_index = subtitle_at(pos, subtitle_offset_ms,
                                    subtitle_available && subtitles_enabled && !paused);
        const char *subtitle = cue_index >= 0 ? subtitle_cues[cue_index].text : NULL;
        bool controls_visible = now < hud_until;
        if (have_overlay && paused && now - last_draw >= 100) {
            draw_pause_menu(&overlay, theme, pos, duration, menu_index, scale_mode,
                            subtitle_available, subtitles_enabled,
                            subtitle_offset_ms, playback);
            overlay_has_content = true;
            last_draw = now;
        } else if (have_overlay && audio_only && now - last_draw >= 1000) {
            draw_hud(&overlay, theme, &track_info, pos, duration, false, playback,
                     status, &cover, theme.background_enabled, NULL);
            overlay_has_content = true;
            last_draw = now;
        } else if (have_overlay && !paused && !audio_only && controls_visible &&
                   now - last_draw >= 200) {
            draw_hud(&overlay, theme, &track_info, pos, duration, false, playback,
                     status, &cover, false, subtitle);
            overlay_has_content = true;
            last_draw = now;
        } else if (have_overlay && !paused && !audio_only && !controls_visible &&
                   (last_controls_visible || cue_index != last_subtitle)) {
            if (subtitle) {
                draw_subtitle_only(&overlay, subtitle);
                overlay_has_content = true;
            } else if (overlay_has_content) {
                overlay_clear(&overlay);
                overlay_has_content = false;
            }
            last_draw = now;
        }
        last_controls_visible = controls_visible;
        last_subtitle = cue_index;
        if (duration > 1000 && pos >= duration - 150 && !paused) {
            if (playback == PLAY_REPEAT) {
                snprintf(restart_path, sizeof(restart_path), "%s", argv[1]);
                restart_requested = true;
            } else if (playback == PLAY_RANDOM) {
                if (random_media_neighbor(argv[1], restart_path, sizeof(restart_path))) restart_requested = true;
            } else if (playlist_neighbor(argv[1], 1, restart_path, sizeof(restart_path))) {
                restart_requested = true;
            }
            eos = true;
        }
        if (fatal_at && now >= fatal_at) break;
        usleep(20000);
    }

    log_step("stopping playback");
    hcplayer_stop2(player, true, true);
    cover_stop(&cover);
    log_step("playback stopped");
    hcplayer_deinit();

done:
    subtitles_clear();
    if (msg_id >= 0) msgctl(msg_id, IPC_RMID, NULL);
    if (raw_keys) shmdt((void *)raw_keys);
    if (have_overlay) overlay_close(&overlay);
    if (restart_requested && restart_path[0]) {
        execl(argv[0], argv[0], restart_path, (char *)NULL);
        return 127;
    }
    return 0;
}
