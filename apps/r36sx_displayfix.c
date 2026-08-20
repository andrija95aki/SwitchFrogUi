#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * R36SX final-frame transform shim.
 *
 * This library is inherited by FrogUI, PicoArch, PCSX4ALL, Rockbox, and other
 * applications that resolve H.OS's proprietary video_driver_disp_frame entry
 * point. It conditionally compensates the left-panel manufacturing fault and
 * also provides a persistent 180-degree display flip. The latter is toggled by
 * holding the hardware FN + L1 + R1 chord. No stock binary is replaced.
 */

#define PANEL_W 640
#define PANEL_H 480
#define BAD_BAND_W 110
#define BAD_BAND_Y_SHIFT 1
#define SETTINGS_FILE "/mnt/sdcard/frogui/settings.txt"
#define ROTATION_FILE "/mnt/sdcard/frogui/screen_rotation.cfg"
#define ROTATION_TMP  "/mnt/sdcard/frogui/screen_rotation.tmp"
#define KEY_FN (1u << 16)
#define KEY_L1 (1u << 10)
#define KEY_R1 (1u << 11)
#define ROTATE_CHORD (KEY_FN | KEY_L1 | KEY_R1)

typedef int (*disp_fn)(void *src, int w, int h, int pitch);
typedef void (*aspect_fn)(int mode);
typedef void *(*dlsym_fn)(void *handle, const char *name);

static dlsym_fn real_dlsym;
static disp_fn real_disp;
static aspect_fn real_aspect;
static int aspect_fit = 1; /* picoarch's R36SX driver: 0=fill, 1=aspect-fit */
static uint16_t *panel_buffers[2];
static int panel_index;
static int xmap[PANEL_W];
static int ymap[PANEL_H];
static int map_w = -1, map_h = -1, map_fit = -1;
static int process_is_pcsx = -1;
static volatile uint32_t *joy_keys;
static int joy_attach_attempted;
static int rotation_180 = -1;
static int rotation_latched;
static int display_fix_enabled;
static unsigned settings_poll;

static int is_pcsx_process(void) {
    if (process_is_pcsx < 0) {
        char name[64] = {0};
        FILE *f = fopen("/proc/self/comm", "r");
        if (f) {
            fgets(name, sizeof(name), f);
            fclose(f);
        }
        process_is_pcsx = strstr(name, "pcsx4all") ? 1 : 0;
    }
    return process_is_pcsx;
}

static int file_value_is_on(const char *path, const char *key) {
    char line[160];
    size_t key_len = strlen(key);
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    while (fgets(line, sizeof(line), file)) {
        if (!strncmp(line, key, key_len) && line[key_len] == '=' &&
            !strncmp(line + key_len + 1, "on", 2)) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

static void refresh_display_fix(void) {
    if (settings_poll++ % 15 == 0)
        display_fix_enabled = file_value_is_on(SETTINGS_FILE,
                                               "r36sx_display_glitch_fix");
}

static void rotation_load(void) {
    char value[24] = {0};
    FILE *file;
    if (rotation_180 >= 0) return;
    rotation_180 = 0;
    file = fopen(ROTATION_FILE, "rb");
    if (!file) return;
    if (fgets(value, sizeof(value), file))
        rotation_180 = atoi(value) == 180;
    fclose(file);
}

static void rotation_save(void) {
    FILE *file = fopen(ROTATION_TMP, "wb");
    if (!file) return;
    fprintf(file, "%d\n", rotation_180 ? 180 : 0);
    fflush(file);
    fsync(fileno(file));
    fclose(file);
    rename(ROTATION_TMP, ROTATION_FILE);
}

static void attach_joy_keys(void) {
    key_t key;
    int id;
    void *address;
    if (joy_keys || joy_attach_attempted) return;
    joy_attach_attempted = 1;
    key = ftok("/tmp/joy_key", 'a');
    if (key == (key_t)-1) return;
    id = shmget(key, sizeof(uint32_t), 0666);
    if (id < 0) return;
    address = shmat(id, NULL, 0);
    if (address != (void *)-1)
        joy_keys = (volatile uint32_t *)address;
}

static void poll_rotation_chord(void) {
    uint32_t raw;
    rotation_load();
    attach_joy_keys();
    if (!joy_keys) return;
    raw = *joy_keys;
    if ((raw & ROTATE_CHORD) == ROTATE_CHORD) {
        if (!rotation_latched) {
            rotation_180 = !rotation_180;
            rotation_save();
            rotation_latched = 1;
            fprintf(stderr, "SwitchFrogUI display rotation: %s\n",
                    rotation_180 ? "180 degrees" : "normal");
        }
    } else {
        rotation_latched = 0;
    }
}

static void resolve_real_dlsym(void) {
    if (!real_dlsym)
        real_dlsym = (dlsym_fn)dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.0");
}

static void fixed_aspect(int mode) {
    aspect_fit = mode ? 1 : 0;
    if (real_aspect) real_aspect(mode);
}

static int fixed_disp(void *pixels, int w, int h, int pitch) {
    if (!real_disp || !pixels || w <= 0 || h <= 0 || pitch < w * 2)
        return real_disp ? real_disp(pixels, w, h, pitch) : -1;

    poll_rotation_chord();
    refresh_display_fix();
    if (!display_fix_enabled && !rotation_180)
        return real_disp(pixels, w, h, pitch);

    /* Preserve the vendor's crisp native PicoArch menu only when it does not
     * need rotating. Game frames and PCSX4ALL's panel menu still use the final
     * compositor, as does every frame while upside down. */
    if (!rotation_180 && !is_pcsx_process() && w == 320 && h == 240 && pitch >= 640)
        return real_disp(pixels, w, h, pitch);

    if (!panel_buffers[0]) {
        panel_buffers[0] = (uint16_t *)malloc(PANEL_W * PANEL_H * sizeof(uint16_t));
        panel_buffers[1] = (uint16_t *)malloc(PANEL_W * PANEL_H * sizeof(uint16_t));
        fprintf(stderr, "SwitchFrogUI final display transform: active\n");
    }
    if (!panel_buffers[0] || !panel_buffers[1])
        return real_disp(pixels, w, h, pitch);

    uint16_t *out = panel_buffers[panel_index];
    panel_index ^= 1;
    const uint8_t *src_bytes = (const uint8_t *)pixels;

    int dw = PANEL_W, dh = PANEL_H, ox = 0, oy = 0;
    if (aspect_fit) {
        dh = h * PANEL_W / w;
        if (dh > PANEL_H) {
            dh = PANEL_H;
            dw = w * PANEL_H / h;
        }
        ox = (PANEL_W - dw) / 2;
        oy = (PANEL_H - dh) / 2;
    }

    if (w != map_w || h != map_h || aspect_fit != map_fit) {
        for (int x = 0; x < PANEL_W; x++)
            xmap[x] = (x < ox || x >= ox + dw) ? -1 : (x - ox) * w / dw;
        for (int y = 0; y < PANEL_H; y++)
            ymap[y] = (y < oy || y >= oy + dh) ? -1 : (y - oy) * h / dh;
        map_w = w; map_h = h; map_fit = aspect_fit;
    }

    if (!rotation_180 && display_fix_enabled && w == PANEL_W && h == PANEL_H &&
        pitch == PANEL_W * 2 && dw == PANEL_W && dh == PANEL_H) {
        const uint16_t *src = (const uint16_t *)pixels;
        for (int y = 0; y < PANEL_H; y++) {
            uint16_t *dst = out + (size_t)y * PANEL_W;
            int band_y = (y - BAD_BAND_Y_SHIFT + PANEL_H) % PANEL_H;
            memcpy(dst, src + (size_t)band_y * PANEL_W,
                   BAD_BAND_W * sizeof(uint16_t));
            memcpy(dst + BAD_BAND_W, src + (size_t)y * PANEL_W + BAD_BAND_W,
                   (PANEL_W - BAD_BAND_W) * sizeof(uint16_t));
        }
        return real_disp(out, PANEL_W, PANEL_H, PANEL_W * 2);
    }

    for (int y = 0; y < PANEL_H; y++) {
        uint16_t *dst = out + (size_t)y * PANEL_W;
        int corrected_y = display_fix_enabled ?
            (y - BAD_BAND_Y_SHIFT + PANEL_H) % PANEL_H : y;
        for (int x = 0; x < PANEL_W; x++) {
            int logical_x = rotation_180 ? PANEL_W - 1 - x : x;
            int logical_y = (display_fix_enabled && x < BAD_BAND_W) ? corrected_y : y;
            if (rotation_180) logical_y = PANEL_H - 1 - logical_y;
            int sx = xmap[logical_x];
            int sy = ymap[logical_y];
            if (sx < 0 || sy < 0) {
                dst[x] = 0;
                continue;
            }
            const uint16_t *srow = (const uint16_t *)(src_bytes + (size_t)sy * pitch);
            dst[x] = srow[sx];
        }
    }

    return real_disp(out, PANEL_W, PANEL_H, PANEL_W * 2);
}

void *dlsym(void *handle, const char *name) {
    resolve_real_dlsym();
    if (!real_dlsym) return NULL;

    if (strcmp(name, "video_driver_disp_frame") == 0) {
        real_disp = (disp_fn)real_dlsym(handle, name);
        return real_disp ? (void *)fixed_disp : NULL;
    }
    if (strcmp(name, "fbdev_video_aspect_ratio") == 0) {
        real_aspect = (aspect_fn)real_dlsym(handle, name);
        return real_aspect ? (void *)fixed_aspect : NULL;
    }
    return real_dlsym(handle, name);
}
