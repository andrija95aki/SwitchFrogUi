#!/bin/sh
# Merged TreeFrogUI/SwitchFrogUI H.OS hardware video-player wrapper.
export LD_LIBRARY_PATH=/mnt/sdcard/rootfs/usr/lib:/mnt/sdcard/cubegm/usr/lib:/usr/lib:$LD_LIBRARY_PATH
# FrogUI/emulators use this optional preload to compensate a faulty R36SX
# panel.  The hardware video player uses libffplayer's own display pipeline;
# injecting the disp_frame shim there crashes the dynamic loader before main().
# Keep the player isolated and let it own the video plane directly.
unset LD_PRELOAD
unset TF_R36SX_DISPLAYFIX_PRELOADED

if [ -f /mnt/sdcard/log.txt ]; then
    logfile="/mnt/sdcard/video_player.log"
    [ -f "$logfile" ] && mv "$logfile" "$logfile.prev"
    echo "=== video_player: $1 ===" > "$logfile"
    echo "video_player: foreground upstream startup" >> "$logfile"
    /mnt/sdcard/cubegm/video_player "$1" >> "$logfile" 2>&1
    result=$?
    echo "video_player exit=$result" >> "$logfile"
    exit "$result"
fi
exec /mnt/sdcard/cubegm/video_player "$1"
