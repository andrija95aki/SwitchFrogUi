#!/bin/sh
set -u
runtime=/mnt/sdcard/cubegm/dsperate
save_root=/mnt/sdcard/frogui/dsperate
mkdir -p "$save_root" || exit 1
[ "$#" -eq 1 ] && [ -f "$1" ] || exit 1
# This upstream interpreter build is experimental. JIT and native code remain
# off; no-audio matches the audited upstream SF3000 package's supported path.
export DS_HCGE=1 DS_HCGE_DIAG=0 DS_MIPS_JIT=0 DS_MIPS_NATIVE=0
log=/dev/null
[ -f /mnt/sdcard/log.txt ] && log=/mnt/sdcard/log.txt
cd "$save_root" || exit 1
env HOME="$save_root" "$runtime/lib/ld.so.1" \
    --library-path "$runtime/lib:/mnt/sdcard/cubegm/usr/lib:/mnt/sdcard/cubegm/lib" \
    "$runtime/dsperate" "$1" --no-mic --no-audio --interp >>"$log" 2>&1
result=$?
sync
exit "$result"
