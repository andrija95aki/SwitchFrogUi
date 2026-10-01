#!/bin/sh
# Never export the raw FAT block device or switch away from an active OTG disk.
if awk '$2 == "/media/hdd" { found=1 } END { exit !found }' /proc/mounts; then
    printf '%s\n' 'Remove OTG storage before starting USB Mode.' >&2
    exit 1
fi
exec /mnt/sdcard/cubegm/usb_mode.sh mtp
