#ifndef SWITCHFROG_MENU_POWER_H
#define SWITCHFROG_MENU_POWER_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>

/* Only the frontend uses this policy. No guessed clocks/voltages, no changes
 * to thermal limits, no powersave fallback that could starve terminal tasks.
 * R36SX has one CPU policy; absent sysfs support is a harmless no-op. */
typedef struct {
    char root[256], governor[64], minimum[64];
    int changed;
} MenuCpuPolicy;

static int menu_cpu_read(const char *root, const char *name, char *value, size_t size) {
    char path[320];
    snprintf(path, sizeof(path), "%s/%s", root, name);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int ok = fgets(value, size, f) != NULL;
    fclose(f);
    if (ok) value[strcspn(value, "\r\n")] = 0;
    return ok && value[0];
}

static int menu_cpu_write(const char *root, const char *name, const char *value) {
    char path[320];
    snprintf(path, sizeof(path), "%s/%s", root, name);
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    int ok = fprintf(f, "%s\n", value) > 0;
    if (fclose(f) != 0) ok = 0;
    return ok;
}

static int menu_cpu_restore(MenuCpuPolicy *p) {
    if (!p->changed) return 1;
    int a = menu_cpu_write(p->root, "scaling_min_freq", p->minimum);
    int b = menu_cpu_write(p->root, "scaling_governor", p->governor);
    if (a && b) p->changed = 0;
    return a && b;
}

static int menu_cpu_enter(MenuCpuPolicy *p, const char *root) {
    char available[512], lowest[64];
    const char *chosen = NULL;
    const char *candidates[] = {"schedutil", "ondemand", "conservative"};
    memset(p, 0, sizeof(*p));
    snprintf(p->root, sizeof(p->root), "%s", root);
    if (!menu_cpu_read(root, "scaling_governor", p->governor, sizeof(p->governor)) ||
        !menu_cpu_read(root, "scaling_min_freq", p->minimum, sizeof(p->minimum)) ||
        !menu_cpu_read(root, "cpuinfo_min_freq", lowest, sizeof(lowest)) ||
        !menu_cpu_read(root, "scaling_available_governors", available, sizeof(available))) return 0;
    if (strspn(lowest, "0123456789") != strlen(lowest)) return 0;
    for (unsigned i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
        const char *found = strstr(available, candidates[i]);
        size_t len = strlen(candidates[i]);
        if (found && (found == available || found[-1] == ' ' || found[-1] == '\t') &&
            (found[len] == 0 || found[len] == ' ' || found[len] == '\t')) {
            chosen = candidates[i]; break;
        }
    }
    if (!chosen) return 0;
    p->changed = 1;
    if (!menu_cpu_write(root, "scaling_governor", chosen) ||
        !menu_cpu_write(root, "scaling_min_freq", lowest)) {
        menu_cpu_restore(p);
        return 0;
    }
    return 1;
}

/* Keep the existing 60 Hz input/debounce/audio/timeout semantics, but sleep
 * instead of relying on repeated vendor framebuffer writes for pacing. */
static int64_t menu_tick_delay(int64_t now, int64_t previous) {
    const int64_t period = 16666667;
    int64_t elapsed = now - previous;
    return previous && elapsed >= 0 && elapsed < period ? period - elapsed : 0;
}

static void menu_tick_wait(int64_t *previous) {
    struct timespec now, delay;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        delay.tv_sec = 0; delay.tv_nsec = 16666667;
    } else {
        int64_t ns = (int64_t)now.tv_sec * 1000000000 + now.tv_nsec;
        delay.tv_sec = 0; delay.tv_nsec = (long)menu_tick_delay(ns, *previous);
    }
    while (delay.tv_nsec && nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0)
        *previous = (int64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}

static int menu_present_needed(int can_duplicate, int blanked, int redraw, unsigned *age) {
    if (blanked && can_duplicate) { *age = 0; return 0; }
    /* One keepalive/second while lit also recovers externally changed planes. */
    if (!can_duplicate || redraw || ++*age >= 60) { *age = 0; return 1; }
    return 0;
}
#endif
