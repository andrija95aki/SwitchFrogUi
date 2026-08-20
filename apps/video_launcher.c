#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

typedef int (*video_main_fn)(int argc, char **argv);

/*
 * Keep the H.OS executable deliberately tiny.  Large Zig/LLD-linked MIPS
 * executables fault in the H.OS 1.2 loader/runtime before main(), while the
 * same code works reliably as a shared object (the FrogUI core and display
 * shim use that route too).  The full player therefore lives in a module and
 * this process does only the late dlopen/dlsym handoff.
 */
int main(int argc, char **argv) {
    const char *module_path = "/mnt/sdcard/cubegm/video_player_impl.so";
    fprintf(stderr, "video_launcher: entered main argc=%d\n", argc);

    void *module = dlopen(module_path, RTLD_NOW | RTLD_LOCAL);
    if (!module) {
        fprintf(stderr, "video_launcher: dlopen %s failed: %s\n",
                module_path, dlerror());
        return 20;
    }

    dlerror();
    video_main_fn run = (video_main_fn)dlsym(module, "switchfrog_video_main");
    const char *error = dlerror();
    if (error || !run) {
        fprintf(stderr, "video_launcher: player entry missing: %s\n",
                error ? error : "unknown error");
        dlclose(module);
        return 21;
    }

    fprintf(stderr, "video_launcher: implementation loaded\n");
    int result = run(argc, argv);
    dlclose(module);
    return result;
}
