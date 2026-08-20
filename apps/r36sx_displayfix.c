#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Optional R36SX display shim.
 *
 * This library is preloaded on the initial frontend process when FrogUI's
 * persistent r36sx_display_glitch_fix setting is on. Picoarch's exec-chained
 * games and standalone emulators inherit it. Picoarch/PCSX4ALL resolve the
 * proprietary driver's presentation entry point with dlsym(), so intercept
 * that resolution and return a final-panel compositor. The two output buffers
 * also keep the asynchronously scanned PCSX menu from reading a buffer while
 * its text is being redrawn. No executable or stock driver is replaced, and
 * the normal path is untouched while the setting is off.
 */

#define PANEL_W 640
#define PANEL_H 480
#define BAD_BAND_W 110
#define BAD_BAND_Y_SHIFT 1

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

    /* PicoArch draws its native in-game menu into a 320x240 RGB565 surface.
     * Passing that UI through the final-panel resampler makes its small bitmap
     * glyphs disappear on the SNES path.  The vendor driver already handles
     * this exact menu geometry correctly, so leave only these UI frames alone;
     * game frames and PCSX4ALL's panel-native menu remain corrected. */
    if (w == 320 && h == 240 && pitch >= 640)
        return real_disp(pixels, w, h, pitch);

    if (!panel_buffers[0]) {
        panel_buffers[0] = (uint16_t *)malloc(PANEL_W * PANEL_H * sizeof(uint16_t));
        panel_buffers[1] = (uint16_t *)malloc(PANEL_W * PANEL_H * sizeof(uint16_t));
        fprintf(stderr, "R36SX Display glitch fix: active (left 110px, vertical -1px panel fault)\n");
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

    /* Cache the panel-to-source maps. Geometry changes are rare; avoiding two
     * integer divisions per output pixel materially reduces MIPS CPU cost. */
    if (w != map_w || h != map_h || aspect_fit != map_fit) {
        for (int x = 0; x < PANEL_W; x++)
            xmap[x] = (x < ox || x >= ox + dw) ? -1 : (x - ox) * w / dw;
        for (int y = 0; y < PANEL_H; y++)
            ymap[y] = (y < oy || y >= oy + dh) ? -1 : (y - oy) * h / dh;
        map_w = w; map_h = h; map_fit = aspect_fit;
    }

    /* Panel-native frames need only two row copies, including the compensated
     * band. This is the common menu/high-resolution fast path. */
    if (w == PANEL_W && h == PANEL_H && pitch == PANEL_W * 2 &&
        dw == PANEL_W && dh == PANEL_H) {
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
        int normal_sy = ymap[y];
        int band_sy = ymap[(y - BAD_BAND_Y_SHIFT + PANEL_H) % PANEL_H];
        for (int x = 0; x < PANEL_W; x++) {
            int sx = xmap[x];
            int sy = (x < BAD_BAND_W) ? band_sy : normal_sy;
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
