#!/bin/sh
# Keep the fork's video player independent from the optional audio frontend.
case "$1" in
    *.mp3|*.MP3|*.flac|*.FLAC|*.ogg|*.OGG|*.wav|*.WAV|*.m4a|*.M4A|*.aac|*.AAC|*.wma|*.WMA|*.opus|*.OPUS) ;;
    *) exit 2 ;;
esac
export LD_LIBRARY_PATH=/mnt/sdcard/cubegm/lib:/mnt/sdcard/cubegm/usr/lib
exec /mnt/sdcard/cubegm/music_lite "$1"
