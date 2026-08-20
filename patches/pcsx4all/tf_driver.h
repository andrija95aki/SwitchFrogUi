/* Device-correct proprietary driver path from /tmp/tfdevice.env.
 * Adapted from TreeFrogUI_ebook_reader's SF3000 port. */
#ifndef TF_DRIVER_H
#define TF_DRIVER_H

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static inline const char *tf_driver_path(void) {
    static char path[128];
    if (path[0]) return path;

    int fd = open("/tmp/tfdevice.env", O_RDONLY);
    if (fd >= 0) {
        char buf[512];
        int n = (int)read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            char *p;
            buf[n] = '\0';
            p = strstr(buf, "TF_DRIVER=");
            while (p && p != buf && p[-1] != '\n')
                p = strstr(p + 1, "TF_DRIVER=");
            if (p) {
                char *e;
                int len;
                p += 10;
                e = p;
                while (*e && *e != '\r' && *e != '\n') e++;
                len = (int)(e - p);
                if (len > 0 && len < (int)sizeof(path)) {
                    memcpy(path, p, len);
                    path[len] = '\0';
                }
            }
        }
    }
    if (!path[0]) strcpy(path, "/mnt/sdcard/cubegm/driver.so");
    return path;
}

#endif
