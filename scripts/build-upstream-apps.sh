#!/usr/bin/env bash
# Pinned optional apps, built using the same GNU SDK as the fork's runtimes.
set -euo pipefail
: "${SF_GCC:?}" "${SF_PREFIX:?}" "${SF_SYSROOT:?}" "${RUNNER_TEMP:?}"
repo=$(pwd)
stage="$repo/artifacts/upstream"
mkdir -p "$stage" "$RUNNER_TEMP/switchfrog-apps"
cd "$RUNNER_TEMP/switchfrog-apps"
git clone --filter=blob:none https://github.com/tzubertowski/TreeFrogUI_ebook_reader.git ebook
git -C ebook checkout e962703cddbdcaaf33f72206c23d8005e4a81696
git clone --depth 1 --branch 1.24.10 --recurse-submodules --shallow-submodules https://github.com/ArtifexSoftware/mupdf.git mupdf
gcc -I mupdf/include -ffunction-sections -fdata-sections ebook/test_reader.c -Wl,--gc-sections -o test-reader
./test-reader
arch="-EL -mips32r2 -march=mips32r2 -mtune=74kc -mfp32 -mhard-float --sysroot=$SF_SYSROOT"
make -C mupdf -j2 build=release HAVE_X11=no HAVE_GLUT=no HAVE_CURL=no USE_TESSERACT=no HAVE_OBJCOPY=no \
  OS=Linux CC="$SF_GCC" CXX="${SF_PREFIX}g++" AR="${SF_PREFIX}ar" LD="${SF_PREFIX}ld" \
  XCFLAGS="$arch" XCXXFLAGS="$arch" libs
"$SF_GCC" $arch -O2 -I mupdf/include -I ebook/port_sf3000 -c ebook/reader.c -o reader.o
"$SF_GCC" $arch -O2 -I ebook/port_sf3000 -c ebook/port_sf3000/hwdisp.c -o hwdisp.o
"${SF_PREFIX}g++" $arch reader.o hwdisp.o mupdf/build/release/libmupdf.a mupdf/build/release/libmupdf-third.a \
  -static-libstdc++ -static-libgcc -lm -ldl -lpthread -Wl,--gc-sections -o "$stage/ebook"
"$SF_GCC" $arch -mlong-calls -G0 -Os -DNDEBUG "$repo/apps/music_lite/upstream_player.c" \
  "$RUNNER_TEMP/pcsx4all/src/port/sf3000/fonts.c" -L"$SF_SYSROOT/usr/lib" \
  -Wl,--gc-sections -lffplayer -ljpeg -lpthread -o "$stage/music_lite"
"$SF_GCC" $arch -Os "$repo/apps/usb_mode/usb_exit_watcher.c" -o "$stage/usb_exit_watcher"
for app in ebook music_lite usb_exit_watcher; do "${SF_PREFIX}strip" "$stage/$app"; done
git -C ebook rev-parse HEAD > "$stage/ebook-source.txt"
git -C mupdf rev-parse HEAD > "$stage/mupdf-source.txt"
sha256sum "$stage/ebook" "$stage/music_lite" "$stage/usb_exit_watcher" > "$stage/SHA256SUMS.txt"
