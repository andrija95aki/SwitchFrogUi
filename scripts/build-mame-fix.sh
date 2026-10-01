#!/usr/bin/env bash
set -euo pipefail
repo=$(pwd)
stage="$repo/artifacts/upstream"
cd "$RUNNER_TEMP/switchfrog-apps"
git clone --filter=blob:none https://github.com/libretro/mame2000-libretro.git mame2000
git -C mame2000 checkout 760ba0269dcb3f6f19297c553562f81a8a73dc1c
git -C mame2000 apply "$repo/patches/mame2000-load-failure.patch"
make -C mame2000 -j2 platform=unix CC="gcc -fcommon -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types -Wno-error=int-conversion"
gcc -O2 "$repo/tests/mame2000_bad_load_test.c" -ldl -o mame-bad-load-test
timeout 30 ./mame-bad-load-test "$PWD/mame2000/mame2000_libretro.so"
make -C mame2000 clean
make -C mame2000 -j2 platform=unix IS_X86=0 \
  CC="$SF_GCC -EL -mips32r2 -mfp32 -mhard-float -fPIC --sysroot=$SF_SYSROOT" \
  AR="${SF_PREFIX}ar" \
  LDFLAGS="-shared -EL --sysroot=$SF_SYSROOT -Wl,--no-undefined"
cp mame2000/mame2000_libretro.so "$stage/mame2000_libretro.so"
"${SF_PREFIX}strip" "$stage/mame2000_libretro.so"
git -C mame2000 rev-parse HEAD > "$stage/mame2000-source.txt"
sha256sum "$stage/mame2000_libretro.so" >> "$stage/SHA256SUMS.txt"
