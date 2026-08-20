#!/bin/sh
# H.OS hardware video-player wrapper for SwitchFrogUI.
export LD_LIBRARY_PATH=/mnt/sdcard/rootfs/usr/lib:/mnt/sdcard/cubegm/usr/lib:/usr/lib:$LD_LIBRARY_PATH
# FrogUI/emulators use this optional preload to compensate a faulty R36SX
# panel.  The hardware video player uses libffplayer's own display pipeline;
# injecting the disp_frame shim there crashes the dynamic loader before main().
# Keep the player isolated and let it own the video plane directly.
unset LD_PRELOAD
unset TF_R36SX_DISPLAYFIX_PRELOADED
if [ -f /mnt/sdcard/log.txt ]; then
    [ -f /mnt/sdcard/video_player.log ] &&
        mv /mnt/sdcard/video_player.log /mnt/sdcard/video_player.log.prev
    echo "=== video_player: $1 ===" > /mnt/sdcard/video_player.log
    exec /mnt/sdcard/cubegm/video_player "$1" >> /mnt/sdcard/video_player.log 2>&1
fi
exec /mnt/sdcard/cubegm/video_player "$1"
