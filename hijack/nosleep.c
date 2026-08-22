/*
 * nosleep - live-patch the running cubevol to disable sleep.
 *
 * SF3500 verifies cubevol on disk at boot (hash/signature), so a byte-patched
 * file shows "sdcard is damaged". Instead we leave the on-disk binary pristine
 * (passes verification, loads, runs) and NOP the two sleep-arm instructions in
 * the LIVE process image via ptrace. cubevol is ET_EXEC (non-PIE), so link
 * address == runtime address - the caller passes absolute text addresses.
 *
 * cubevol is respawned during a session (FrogUI restarts it for the battery/
 * volume OSD), which re-arms sleep, so this runs as a persistent WATCHER:
 * launched once by zhijack, it re-patches whenever a fresh cubevol appears.
 *
 * Usage:
 *   nosleep -w <hexaddr> [<hexaddr> ...]   watch: patch every new cubevol pid
 *   nosleep <pid> <hexaddr> [...]          one-shot: patch a specific pid
 *
 * The three R36-class sleep-arm stores are redirected to separate unused,
 * aligned words in cubevol's data segment; the watcher turns activity on any
 * path into /tmp/frogui_power_event for FrogUI. This preserves cubevol's
 * separate long-press shutdown path.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdint.h>

#define POWER_MARKER_COUNT 3
static const unsigned long power_marker_addr[POWER_MARKER_COUNT] = {
    0x0044bb1cUL, 0x0044bb28UL, 0x0044bb2cUL
};
#define POWER_EVENT_FILE  "/tmp/frogui_power_event"
#define POWER_MARKER_SENTINEL 0x53464750u /* "SFGP": detects stores of 0 too */
/* R36SX address order is 0x406d24, 0x40701c, 0x406b50. Redirect every known
 * sleep-arm store to its own unused data word. The previous bridge redirected
 * only the first path, but this firmware reaches a different path for some
 * short taps, so the event was never published. */
static const unsigned long power_marker_insn[POWER_MARKER_COUNT] = {
    0xac62bb1cUL, /* sw v0,0xbb1c(v1) */
    0xae02bb28UL, /* sw v0,0xbb28(s0) */
    0xac44bb2cUL  /* sw a0,0xbb2c(v0) */
};

typedef struct {
    pid_t pid;
    int mem_fd;
} WatchedPid;

static unsigned power_event_serial;

static void publish_power_event(void)
{
    char text[32];
    int n = snprintf(text, sizeof(text), "%u\n", ++power_event_serial);
    int fd = open(POWER_EVENT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        (void)!write(fd, text, (size_t)n);
        close(fd);
    }
}

static int patch_pid(pid_t pid, int naddr, char **addrs, int *mem_fd_out)
{
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) < 0) { perror("nosleep: attach"); return -1; }
    int status;
    if (waitpid(pid, &status, 0) < 0) { ptrace(PTRACE_DETACH, pid, 0, 0); return -1; }

    int rc = 0;
    for (int i = 0; i < naddr; i++) {
        unsigned long addr = strtoul(addrs[i], NULL, 0);
        unsigned long replacement = i < POWER_MARKER_COUNT ? power_marker_insn[i] : 0;
        /* POKETEXT writes one word (4 bytes on MIPS32). */
        if (ptrace(PTRACE_POKETEXT, pid, (void *)addr,
                   (void *)replacement) < 0) {
            fprintf(stderr, "nosleep: POKETEXT @0x%lx failed\n", addr);
            rc = -1;
        } else {
            printf("nosleep: %s %d @0x%lx (pid %d)\n",
                   i < POWER_MARKER_COUNT ? "POWER EVENT PATH" : "NOP",
                   i + 1, addr, (int)pid);
        }
    }
    if (mem_fd_out) {
        char mem_path[64];
        snprintf(mem_path, sizeof(mem_path), "/proc/%d/mem", (int)pid);
        /* Open while ptrace access is unquestionably authorised, then retain
         * the fd after detach for non-stopping marker polling. */
        *mem_fd_out = open(mem_path, O_RDWR);
        if (*mem_fd_out < 0) perror("nosleep: open process memory");
        else {
            uint32_t marker = POWER_MARKER_SENTINEL;
            for (int i = 0; i < POWER_MARKER_COUNT; i++)
                (void)!pwrite(*mem_fd_out, &marker, sizeof(marker),
                              (off_t)power_marker_addr[i]);
        }
    }
    ptrace(PTRACE_DETACH, pid, 0, 0);
    fflush(stdout);
    return rc;
}

/* Collect all pids whose comm == name (/proc/<pid>/comm). Returns count. */
static int find_pids(const char *name, pid_t *out, int max)
{
    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *e;
    int n = 0;
    while ((e = readdir(d)) && n < max) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char path[64], comm[64];
        snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        if (fgets(comm, sizeof(comm), f)) {
            comm[strcspn(comm, "\n")] = 0;
            if (strcmp(comm, name) == 0) out[n++] = (pid_t)atoi(e->d_name);
        }
        fclose(f);
    }
    closedir(d);
    return n;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "-w") == 0) {
        /* watch mode: patch EVERY cubevol instance (there are two on these
         * devices), and any respawn, forever. Track already-patched pids so we
         * don't re-ptrace (and briefly pause) a process every cycle. Text pages
         * are shared+COW, so each process must be poked individually. */
        #define MAXPIDS 16
        WatchedPid watched[MAXPIDS]; int nwatched = 0;
        int scan_ticks = 0;
        unlink(POWER_EVENT_FILE);
        for (;;) {
            if (scan_ticks-- <= 0) {
                pid_t pids[MAXPIDS];
                int n = find_pids("cubevol", pids, MAXPIDS);
                /* Forget exited instances and close their proc-memory handles. */
                for (int i = 0; i < nwatched; ) {
                    int alive = 0;
                    for (int j = 0; j < n; j++)
                        if (pids[j] == watched[i].pid) { alive = 1; break; }
                    if (!alive) {
                        if (watched[i].mem_fd >= 0) close(watched[i].mem_fd);
                        watched[i] = watched[--nwatched];
                    } else i++;
                }
                for (int j = 0; j < n; j++) {
                    int seen = 0;
                    for (int i = 0; i < nwatched; i++)
                        if (watched[i].pid == pids[j]) { seen = 1; break; }
                    if (!seen && nwatched < MAXPIDS) {
                        int mem_fd = -1;
                        if (patch_pid(pids[j], argc - 2, &argv[2], &mem_fd) == 0) {
                            watched[nwatched].pid = pids[j];
                            watched[nwatched].mem_fd = mem_fd;
                            nwatched++;
                        }
                    }
                }
                scan_ticks = 5; /* patch cubevol respawns within about 100 ms */
            }

            /* A tap executes one of the redirected stores. Its value is not
             * stable across cubevol paths/firmware revisions and can be zero,
             * so detect a change from a nonzero sentinel instead of testing
             * `marker != 0`. Polling proc memory does not stop cubevol. */
            int emitted = 0;
            for (int i = 0; i < nwatched; i++) {
                for (int m = 0; m < POWER_MARKER_COUNT; m++) {
                    uint32_t marker = 0;
                    if (watched[i].mem_fd >= 0 &&
                        pread(watched[i].mem_fd, &marker, sizeof(marker),
                              (off_t)power_marker_addr[m]) == (ssize_t)sizeof(marker) &&
                        marker != POWER_MARKER_SENTINEL) {
                        uint32_t sentinel = POWER_MARKER_SENTINEL;
                        (void)!pwrite(watched[i].mem_fd, &sentinel, sizeof(sentinel),
                                      (off_t)power_marker_addr[m]);
                        printf("nosleep: power tap path %d value=0x%x pid=%d\n",
                               m + 1, marker, (int)watched[i].pid);
                        fflush(stdout);
                        emitted = 1;
                    }
                }
            }
            if (emitted) publish_power_event();
            usleep(10000);
        }
        return 0;   /* unreachable */
    }

    if (argc >= 3) {
        pid_t pid = (pid_t)strtol(argv[1], NULL, 0);
        if (pid <= 0) { fprintf(stderr, "nosleep: bad pid\n"); return 2; }
        return patch_pid(pid, argc - 2, &argv[2], NULL) == 0 ? 0 : 1;
    }

    fprintf(stderr, "usage: %s -w <hexaddr>...   |   %s <pid> <hexaddr>...\n", argv[0], argv[0]);
    return 2;
}
