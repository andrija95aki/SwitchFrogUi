#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#endif
static int64_t test_now = 1000000000;
static int sleep_calls, interrupt_once;
static int test_clock(int id, struct timespec *ts) {
    (void)id; ts->tv_sec = test_now / 1000000000; ts->tv_nsec = test_now % 1000000000;
    return 0;
}
static int test_sleep(const struct timespec *req, struct timespec *rem) {
    long ns = req->tv_nsec;
    sleep_calls++;
    if (interrupt_once) {
        interrupt_once = 0; test_now += ns / 2;
        rem->tv_sec = 0; rem->tv_nsec = ns - ns / 2;
        errno = EINTR; return -1;
    }
    test_now += ns; return 0;
}
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
#define clock_gettime test_clock
#define nanosleep test_sleep
#include "../apps/menu_power.h"

static void expect(const char *root, const char *name, const char *wanted) {
    char value[128]; assert(menu_cpu_read(root, name, value, sizeof(value)));
    assert(strcmp(value, wanted) == 0);
}
int main(int argc, char **argv) {
    assert(argc == 2); /* caller-created, disposable test directory */
    const char *root = argv[1];
    MenuCpuPolicy p;
    assert(!menu_cpu_enter(&p, root)); /* missing sysfs */
    assert(menu_cpu_restore(&p));
    assert(menu_cpu_write(root, "scaling_governor", "performance"));
    assert(menu_cpu_write(root, "scaling_min_freq", "1000000"));
    assert(menu_cpu_write(root, "cpuinfo_min_freq", "300000"));
    assert(menu_cpu_write(root, "scaling_max_freq", "1000000"));
    assert(menu_cpu_write(root, "scaling_available_governors", "performance powersave"));
    assert(!menu_cpu_enter(&p, root)); /* never force fixed low-clock powersave */
    expect(root, "scaling_min_freq", "1000000");
    assert(menu_cpu_write(root, "scaling_available_governors", "performance conservative ondemand"));
    assert(menu_cpu_enter(&p, root));
    expect(root, "scaling_governor", "ondemand");
    expect(root, "scaling_min_freq", "300000");
    expect(root, "scaling_max_freq", "1000000");
    assert(menu_cpu_restore(&p));
    expect(root, "scaling_governor", "performance");
    expect(root, "scaling_min_freq", "1000000");
    assert(menu_cpu_write(root, "scaling_available_governors", "schedutil ondemand"));
    assert(menu_cpu_enter(&p, root));
    expect(root, "scaling_governor", "schedutil");
    assert(menu_cpu_restore(&p));
    assert(menu_cpu_write(root, "cpuinfo_min_freq", "invalid"));
    assert(!menu_cpu_enter(&p, root));
    expect(root, "scaling_min_freq", "1000000");

    unsigned age = 0; int presents = 0;
    for (int i = 0; i < 600; i++) presents += menu_present_needed(1, 0, 0, &age);
    assert(presents == 10); /* 10 seconds static: 600 presents -> 10 */
    for (int i = 0; i < 600; i++) assert(!menu_present_needed(1, 1, 1, &age));
    assert(menu_present_needed(1, 0, 1, &age)); /* wake/input immediate */
    assert(menu_present_needed(0, 0, 0, &age)); /* non-duplicate frontend */
    assert(menu_tick_delay(1000000000, 0) == 0);
    assert(menu_tick_delay(1010000000, 1000000000) == 6666667);
    assert(menu_tick_delay(2000000000, 1000000000) == 0); /* no catch-up spin */
    int64_t previous = 0;
    menu_tick_wait(&previous); assert(sleep_calls == 0);
    interrupt_once = 1;
    menu_tick_wait(&previous); assert(sleep_calls == 2);
    assert(previous == 1016666667);
    puts("PASS: dynamic governor/restore, missing-policy fallback, 60Hz sleeping, EINTR, idle/blank/wake presentation.");
    return 0;
}
