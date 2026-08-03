#!/bin/sh
# Build libemu_tfhijack.so — the SF3500 boot-hijack libretro core.
# Output: libemu_tfhijack.so  → copy to SD cubegm/cores/
set -e
cd "$(dirname "$0")"

MIPS="$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot/opt/ext-toolchain/bin/mips-mti-linux-gnu-"
SYSROOT="$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot/mipsel-buildroot-linux-gnu/sysroot"
CC="${MIPS}gcc"

[ -x "${CC}" ] || { echo "toolchain not found: ${CC}"; exit 1; }

CFLAGS="-mips32r2 -march=mips32r2 -mtune=74kc -mfp32 -mhard-float -mlong-calls -EL \
        --sysroot=$SYSROOT -fPIC -G0 -Wall -O2 -DNDEBUG"

# Normal libc-linked shared lib (rkgame's process has libc). proper PIC relocs.
$CC $CFLAGS -shared -Wl,--gc-sections -EL --sysroot="$SYSROOT" \
    -o libemu_tfhijack.so tfhijack.c

"${MIPS}strip" libemu_tfhijack.so 2>/dev/null || true
ls -la libemu_tfhijack.so
file libemu_tfhijack.so

# nosleep — ptrace live-patcher that NOPs cubevol's sleep-arm stores in RAM
# First address becomes FrogUI's tap event; remaining unsafe sleep stores are
# NOPed. Dynamic + stripped (libc is on-device); persistent watcher mode.
$CC -mips32r2 -EL -O2 --sysroot="$SYSROOT" -o nosleep nosleep.c
"${MIPS}strip" nosleep 2>/dev/null || true
echo "built nosleep: $(ls -la nosleep | awk '{print $5}') bytes"

# Optional R36SX panel compensation. Loaded only for games when the matching
# persistent FrogUI setting is enabled; normal devices never load this shim.
$CC $CFLAGS -shared -Wl,--gc-sections -EL --sysroot="$SYSROOT" \
    -o r36sx_displayfix.so ../apps/r36sx_displayfix.c -ldl
"${MIPS}strip" r36sx_displayfix.so 2>/dev/null || true
echo "built r36sx_displayfix.so: $(ls -la r36sx_displayfix.so | awk '{print $5}') bytes"

echo "=== exported retro_* symbols ==="
"${MIPS}readelf" --dyn-syms libemu_tfhijack.so 2>/dev/null \
    | awk '$4=="FUNC"{print $8}' | grep '^retro_' | sort | tr '\n' ' '; echo
