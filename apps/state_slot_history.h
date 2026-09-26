#ifndef SWITCHFROG_STATE_SLOT_HISTORY_H
#define SWITCHFROG_STATE_SLOT_HISTORY_H
#include <stdio.h>
#include <unistd.h>

/* Device clocks may reset at boot; record successful save order explicitly. */
static int sf_state_slot_read(const char *path, int first, int last) {
    int slot = -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (fscanf(f, "%d", &slot) != 1 || slot < first || slot > last) slot = -1;
    fclose(f);
    return slot;
}
static int sf_state_slot_write(const char *path, int slot) {
    char temporary[2048];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int)sizeof(temporary)) return -1;
    FILE *f = fopen(temporary, "w");
    if (!f) return -1;
    int failed = fprintf(f, "%d\n", slot) < 0;
    if (fflush(f) || fsync(fileno(f))) failed = 1;
    if (fclose(f)) failed = 1;
    if (!failed && rename(temporary, path) == 0) return 0;
    remove(temporary);
    return -1;
}
#endif
