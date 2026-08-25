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
    # Keep decoder diagnostics in RAM while the movie is playing. Writing the
    # log to the same SD card that supplies a high-bitrate video can cause
    # avoidable FAT/flash latency spikes. Publish it after the decoder closes.
    video_log_tmp=/tmp/switchfrog_video_player.log
    echo "=== video_player: $1 ===" > "$video_log_tmp"
    /mnt/sdcard/cubegm/video_player "$1" >> "$video_log_tmp" 2>&1
    video_player_rc=$?
    [ -f /mnt/sdcard/video_player.log ] &&
        mv /mnt/sdcard/video_player.log /mnt/sdcard/video_player.log.prev
    cp "$video_log_tmp" /mnt/sdcard/video_player.log
    rm -f "$video_log_tmp"
    exit "$video_player_rc"
fi
exec /mnt/sdcard/cubegm/video_player "$1"
