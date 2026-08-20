#!/bin/sh
# TreeFrogUI launch wrapper for Rockbox. Rockbox data stays in roms/rockbox,
# while its file browser starts at the SD root and selected audio paths pass in.
export HOME=/mnt/sdcard/roms/rockbox
export SDL_VIDEODRIVER=dummy
export SDL_AUDIODRIVER=dummy
export LD_LIBRARY_PATH=/mnt/sdcard/cubegm/lib:/mnt/sdcard/cubegm/usr/lib:$LD_LIBRARY_PATH

# Standalone Rockbox is reached by picoarch's direct exec path, so it does not
# pass through zhijack.sh's per-game preload branch. Apply the same persistent
# R36SX panel compensation here when the user enabled it in FrogUI Settings.
DISPLAYFIX=/mnt/sdcard/cubegm/r36sx_displayfix.so
if [ -f "$DISPLAYFIX" ] && [ "${TF_R36SX_DISPLAYFIX_PRELOADED:-0}" != 1 ]; then
    export TF_R36SX_DISPLAYFIX_PRELOADED=1
    export LD_PRELOAD="$DISPLAYFIX${LD_PRELOAD:+:$LD_PRELOAD}"
fi

mkdir -p "$HOME/.config/rockbox.org"

# Rockbox has its own dB volume setting and does not consume sndgain.txt.
# Translate FrogUI's perceptual master level at launch so music follows the
# same OS-wide control as emulators. Rockbox can still be adjusted in-session.
MASTER_VOLUME=$(sed -n 's/^volume=//p' /mnt/sdcard/frogui/settings.txt 2>/dev/null | tail -n 1)
case "$MASTER_VOLUME" in
    ''|*[!0-9]*) MASTER_VOLUME=50 ;;
esac
[ "$MASTER_VOLUME" -gt 100 ] && MASTER_VOLUME=100
MASTER_VOLUME=$(( (MASTER_VOLUME + 2) / 5 * 5 ))
case "$MASTER_VOLUME" in
    0)   ROCKBOX_DB=-74 ;;
    5)   ROCKBOX_DB=-40 ;;
    10)  ROCKBOX_DB=-34 ;;
    15)  ROCKBOX_DB=-30 ;;
    20)  ROCKBOX_DB=-28 ;;
    25)  ROCKBOX_DB=-26 ;;
    30)  ROCKBOX_DB=-24 ;;
    35)  ROCKBOX_DB=-23 ;;
    40)  ROCKBOX_DB=-22 ;;
    45)  ROCKBOX_DB=-20 ;;
    50)  ROCKBOX_DB=-18 ;;
    55)  ROCKBOX_DB=-16 ;;
    60)  ROCKBOX_DB=-14 ;;
    65)  ROCKBOX_DB=-12 ;;
    70)  ROCKBOX_DB=-10 ;;
    75)  ROCKBOX_DB=-8 ;;
    80)  ROCKBOX_DB=-6 ;;
    85)  ROCKBOX_DB=-4 ;;
    90)  ROCKBOX_DB=-2 ;;
    95)  ROCKBOX_DB=-1 ;;
    100) ROCKBOX_DB=0 ;;
    *)   ROCKBOX_DB=-18 ;;
esac
ROCKBOX_CONFIG="$HOME/.rockbox/config.cfg"
mkdir -p "$HOME/.rockbox"
if [ -f "$ROCKBOX_CONFIG" ]; then
    if grep -q '^volume:' "$ROCKBOX_CONFIG"; then
        sed "s/^volume:.*/volume: $ROCKBOX_DB/" "$ROCKBOX_CONFIG" > "$ROCKBOX_CONFIG.tmp"
    else
        cp "$ROCKBOX_CONFIG" "$ROCKBOX_CONFIG.tmp" &&
            printf 'volume: %s\n' "$ROCKBOX_DB" >> "$ROCKBOX_CONFIG.tmp"
    fi
    [ -f "$ROCKBOX_CONFIG.tmp" ] &&
        mv "$ROCKBOX_CONFIG.tmp" "$ROCKBOX_CONFIG"
else
    printf 'volume: %s\n' "$ROCKBOX_DB" > "$ROCKBOX_CONFIG"
fi

cd "$HOME" || exit 1
if [ -f /mnt/sdcard/log.txt ]; then
    echo "=== rockbox.sh: HOME=$HOME file=$1 ===" > /mnt/sdcard/rockbox.log
    if [ -n "$1" ] && [ "$1" != "/mnt/sdcard" ]; then
        exec /mnt/sdcard/cubegm/rockbox "$1" >> /mnt/sdcard/rockbox.log 2>&1
    fi
    exec /mnt/sdcard/cubegm/rockbox >> /mnt/sdcard/rockbox.log 2>&1
fi
if [ -n "$1" ] && [ "$1" != "/mnt/sdcard" ]; then
    exec /mnt/sdcard/cubegm/rockbox "$1"
fi
exec /mnt/sdcard/cubegm/rockbox
