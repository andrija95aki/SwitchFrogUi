#!/usr/bin/env bash
set -euo pipefail
repo=$(pwd)
stage="$repo/artifacts/upstream"
cd "$RUNNER_TEMP/switchfrog-apps"
git clone --filter=blob:none https://github.com/Synaps33/FroggyKVM.git froggykvm
git -C froggykvm checkout fceb877c686c86ea5f28ec61d82940f2d0b40411
git -C froggykvm apply "$repo/patches/froggykvm-sf3000.patch"
make -C froggykvm -j2 platform=sf3000 \
  CC="$SF_GCC --sysroot=$SF_SYSROOT" CXX="${SF_PREFIX}g++ --sysroot=$SF_SYSROOT" \
  LDFLAGS="-EL -mips32r2 -mfp32 -mhard-float --sysroot=$SF_SYSROOT -Wl,--no-undefined -lm -lz -lpthread -static-libstdc++ -static-libgcc"
cp froggykvm/j2me_libretro.so "$stage/j2me_libretro.so"
"${SF_PREFIX}strip" "$stage/j2me_libretro.so"
git -C froggykvm rev-parse HEAD > "$stage/j2me-source.txt"
"${SF_PREFIX}readelf" -d "$stage/j2me_libretro.so" >> "$stage/elf-dependencies.txt"
sha256sum "$stage/j2me_libretro.so" >> "$stage/SHA256SUMS.txt"
