#!/bin/sh
# Merged TreeFrogUI/SwitchFrogUI H.OS hardware video-player wrapper.
export LD_LIBRARY_PATH=/mnt/sdcard/rootfs/usr/lib:/mnt/sdcard/cubegm/usr/lib:/usr/lib:$LD_LIBRARY_PATH
# FrogUI/emulators use this optional preload to compensate a faulty R36SX
# panel.  The hardware video player uses libffplayer's own display pipeline;
# injecting the disp_frame shim there crashes the dynamic loader before main().
# Keep the player isolated and let it own the video plane directly.
unset LD_PRELOAD
unset TF_R36SX_DISPLAYFIX_PRELOADED

# libffplayer is proprietary and a bad stream can occasionally block inside a
# decoder call. Supervise the child from outside the library. The player
# updates this /tmp heartbeat every loop; eighteen unchanged seconds means the
# process is wedged, so terminate it and let zhijack relaunch FrogUI.
heartbeat="/tmp/switchfrog-video.$$.heartbeat"
export SWITCHFROG_VIDEO_HEARTBEAT="$heartbeat"
rm -f "$heartbeat"

logfile=""
if [ -f /mnt/sdcard/log.txt ]; then
    logfile=/mnt/sdcard/video_player.log
    [ -f "$logfile" ] && mv "$logfile" "$logfile.prev"
    echo "=== video_player: $1 ===" > "$logfile"
    /mnt/sdcard/cubegm/video_player "$1" >> "$logfile" 2>&1 &
else
    /mnt/sdcard/cubegm/video_player "$1" &
fi
player_pid=$!

(
    previous=""
    stale=0
    while kill -0 "$player_pid" 2>/dev/null; do
        current=""
        [ -f "$heartbeat" ] && current=$(cat "$heartbeat" 2>/dev/null)
        if [ -n "$current" ] && [ "$current" != "$previous" ]; then
            previous="$current"
            stale=0
        else
            stale=$((stale + 1))
        fi
        if [ "$stale" -ge 18 ]; then
            if [ -n "$logfile" ]; then
                echo "video_player watchdog: heartbeat stalled; terminating pid $player_pid" >> "$logfile"
            fi
            kill -TERM "$player_pid" 2>/dev/null
            sleep 2
            kill -KILL "$player_pid" 2>/dev/null
            exit 0
        fi
        sleep 1
    done
) &
watchdog_pid=$!

wait "$player_pid"
result=$?
kill "$watchdog_pid" 2>/dev/null
wait "$watchdog_pid" 2>/dev/null
rm -f "$heartbeat"
if [ -n "$logfile" ]; then
    echo "video_player exit=$result" >> "$logfile"
fi
exit "$result"
