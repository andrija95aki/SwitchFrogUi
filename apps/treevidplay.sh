#!/bin/sh
# Exact upstream TreeFrogUI v1.2.0_b player, isolated from SwitchFrogUI's
# display-fix preload because libffplayer owns the hardware video plane.
export LD_LIBRARY_PATH=/mnt/sdcard/rootfs/usr/lib:/mnt/sdcard/cubegm/usr/lib:/usr/lib:$LD_LIBRARY_PATH
unset LD_PRELOAD
unset TF_R36SX_DISPLAYFIX_PRELOADED

if [ -f /mnt/sdcard/log.txt ]; then
    [ -f /mnt/sdcard/treevidplay.log ] &&
        mv /mnt/sdcard/treevidplay.log /mnt/sdcard/treevidplay.log.prev
    echo "=== TreeVidPlay: $1 ===" > /mnt/sdcard/treevidplay.log
    exec /mnt/sdcard/cubegm/treevidplay "$1" >> /mnt/sdcard/treevidplay.log 2>&1
fi

exec /mnt/sdcard/cubegm/treevidplay "$1"
