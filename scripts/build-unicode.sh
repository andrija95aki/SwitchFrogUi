#!/usr/bin/env bash
set -euo pipefail
repo=$(pwd)
stage="$repo/artifacts/upstream"
cd "$RUNNER_TEMP/switchfrog-apps"
git clone --filter=blob:none https://github.com/harfbuzz/harfbuzz.git harfbuzz
git -C harfbuzz checkout d2e24847f679922c3d77a4c48f9030489d61875d
git clone --filter=blob:none https://github.com/Tehreer/SheenBidi.git SheenBidi
git -C SheenBidi checkout 9c048a32d2131f67608e85ccccf9079cc46d6e42
inc="-Iharfbuzz/src -ISheenBidi/Headers -ISheenBidi/Source"
arch="-EL -mips32r2 -mfp32 -mhard-float -fPIC -G0 --sysroot=$SF_SYSROOT"
hbdefs="-DHB_MINI -DHB_LEAN -DHB_NO_CDECL -DHB_NO_SETLOCALE -DHB_NO_OT_FONT -DHB_NO_FALLBACK_SHAPE -DHAVE_FREETYPE"
"${SF_PREFIX}g++" $arch -Os -std=c++11 -fno-exceptions -fno-rtti $hbdefs $inc \
  -I"$SF_SYSROOT/usr/include/freetype2" -c harfbuzz/src/harfbuzz.cc -o hb.o
"$SF_GCC" $arch -Os $inc -DSB_CONFIG_UNITY -c SheenBidi/Source/SheenBidi.c -o sb.o
"$SF_GCC" $arch -O2 $inc -I"$SF_SYSROOT/usr/include/freetype2" \
  -c "$repo/apps/unicode_text.c" -o unicode.o
"${SF_PREFIX}g++" $arch -shared unicode.o hb.o sb.o -L"$SF_SYSROOT/usr/lib" \
  -Wl,--no-undefined -lfreetype -lm -lpthread -static-libstdc++ -static-libgcc \
  -o "$stage/libswitchfrog_text.so"
"${SF_PREFIX}strip" "$stage/libswitchfrog_text.so"
cp harfbuzz/COPYING "$stage/HarfBuzz-LICENSE.txt"
cp SheenBidi/LICENSE "$stage/SheenBidi-LICENSE.txt"
"${SF_PREFIX}readelf" -d "$stage/libswitchfrog_text.so" >> "$stage/elf-dependencies.txt"
sha256sum "$stage/libswitchfrog_text.so" >> "$stage/SHA256SUMS.txt"
# Same production renderer exercised natively: RTL, Unicode, clipping, malformed
# UTF-8 and repeated sizes. Target hardware visual acceptance remains separate.
sudo apt-get update -qq
sudo apt-get install -y -qq libfreetype6-dev
hostft=$(pkg-config --cflags --libs freetype2)
g++ -O2 -std=c++11 $hbdefs $inc $(pkg-config --cflags freetype2) -c harfbuzz/src/harfbuzz.cc -o host-hb.o
gcc -O2 $inc -DSB_CONFIG_UNITY -c SheenBidi/Source/SheenBidi.c -o host-sb.o
gcc -O2 $inc $(pkg-config --cflags freetype2) -DSF_FONT_ROOT=\"$repo/assets/upstream-160/fonts\" \
  -c "$repo/apps/unicode_text.c" -o host-unicode.o
gcc -O2 -c "$repo/tests/unicode_text_test.c" -o host-test.o
g++ host-test.o host-unicode.o host-hb.o host-sb.o $hostft -lpthread -o unicode-test
./unicode-test
