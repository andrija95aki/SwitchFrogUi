#!/bin/sh
# H.OS hardware video-player wrapper for SwitchFrogUI.
export LD_LIBRARY_PATH=/mnt/sdcard/rootfs/usr/lib:/mnt/sdcard/cubegm/usr/lib:/usr/lib:$LD_LIBRARY_PATH
if [ -f /mnt/sdcard/log.txt ]; then
    echo "=== video_player: $1 ===" > /mnt/sdcard/video_player.log
    exec /mnt/sdcard/cubegm/video_player "$1" >> /mnt/sdcard/video_player.log 2>&1
fi
exec /mnt/sdcard/cubegm/video_player "$1"
