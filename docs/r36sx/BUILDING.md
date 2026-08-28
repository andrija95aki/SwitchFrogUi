# Build the R36SX branch from source

This branch pins the upstream source revisions used during development and
stores the R36SX changes as reviewable patches. Third-party emulator cores,
Rockbox, and PCSX4ALL retain their own upstream projects and licenses.

## Prerequisites

- Windows 10/11 PowerShell 5.1 or newer
- Git
- Zig/Clang with the `mipsel-linux-gnueabihf.2.19` target
- A local copy of your stock H.OS 1.2 card for link-time libraries only
- SDL 1.2.15, libpng 1.6.37, and zlib 1.2.11 headers
- An HCRTOS source checkout for `ffplayer.h` and FFmpeg headers
- A `TreeFrogUI_pcsx4all` source checkout for the video-player bitmap font
- GNU runtime artifacts produced by the included **Build R36SX GNU runtimes**
  GitHub Actions workflow

No stock firmware, ROM, or BIOS file is committed to this repository.

## 1. Clone and prepare pinned sources

```powershell
git clone --recurse-submodules https://github.com/andrija95aki/SwitchFrogUi.git
cd treefrog-ui
git switch r36sx-source-build
powershell -ExecutionPolicy Bypass -File scripts/Prepare-R36SXSources.ps1
```

The preparation script:

- checks FrogUI commit `6c74b5cceeaf98c8d1ef0f12e78b4259e4f65df1`;
- applies `patches/r36sx-frogui.patch`;
- clones TreeFrogUI PicoArch at commit `f8ff5ba`;
- applies `patches/r36sx-picoarch.patch`;
- installs the portable MIPS syscall wrappers used by the Zig cross-link.

It is idempotent and refuses to reset or overwrite an unexpected checkout.

## 2. Build

Run `.github/workflows/build-r36sx-gnu.yml` on GitHub first and download its
`switchfrogui-r36sx-gnu-runtimes` artifact. It uses the official SF3000 GNU SDK
and pinned PCSX4ALL sources to build the launcher and emulator. This separation
is required because Zig/LLD-linked MIPS executables fault before `main()` in the
tested H.OS 1.2 loader, while Zig-built shared modules work correctly.

Download the official `TreeFrogUI_v1.0.15_a.zip` asset from the
[TreeFrogUI v1.0.15 release](https://github.com/tzubertowski/treefrog-ui/releases/tag/v1.0.15).
The build extracts the AGPL MuPDF ebook runtime, the two missing GPL cores, and
the colour-corrected Rockbox runtime from that archive. It applies the guarded
SwitchFrogUI A-confirm/B-back binary mapping to that exact known Rockbox build;
unknown hashes are rejected. Corresponding sources and licenses remain linked
in the project documentation.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Build-R36SX.ps1 `
  -StockCardRoot 'D:\' `
  -HcrtosRoot 'C:\src\hcrtos' `
  -Pcsx4allRoot 'C:\src\TreeFrogUI_pcsx4all' `
  -DependencyRoot "$env:LOCALAPPDATA\Temp\treefrog-deps" `
  -ZigPath 'C:\tools\zig\zig.exe' `
  -GnuRuntimeRoot 'C:\builds\switchfrogui-r36sx-gnu-runtimes' `
  -UpstreamReleaseArchive 'C:\downloads\TreeFrogUI_v1.0.15_a.zip'
```

Outputs are written to `.r36sx-build/out`:

- `frogui_libretro.so`
- `jsdev_libretro.so` (Duktape-powered JavaScript game runtime)
- `video_player`
- `video_player_impl.so`
- `pcsx4all`
- `picoarch` and `picoarch_hi` (official GNU-SDK builds with USB keyboard input)
- `ebook` (official MuPDF reader from the v1.0.12 archive)
- `o2em_libretro.so` and `vecx_libretro.so`
- `libemu_tfhijack.so`
- `nosleep`
- `r36sx_displayfix.so`

`out/card-files/` is also produced with the cores, JSDev API showcase, player, PicoArch pair,
PCSX4ALL, the default editable keyboard map, ebook
reader, missing Odyssey 2/Vectrex cores, twelve UI fonts, their OFL notices,
icon packs, and the CC0 sound packs already arranged in their final SD-card
paths. Merge that directory onto a test card after the base SwitchFrogUI
overlay is installed.

## Offline update package

Create a ROM-free, checksum-verified update from a staged `card-files` tree:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/New-OfflineUpdate.ps1 `
  -CardFilesRoot '.r36sx-build/out/card-files' `
  -OutputArchive '.r36sx-build/out/SwitchFrogUI-update.tar.gz'
```

Copy `SwitchFrogUI-update.tar.gz` to the card root and choose
**Settings -> Offline Update**. The device validates every payload checksum,
rejects path traversal and protected settings/history/favourites files, saves
replaced files under `SwitchFrogUI Backups/`, then installs with same-directory
atomic renames. Results are recorded in `SwitchFrogUI Update.log`.

Pass `-BuildPicoarch` to also compile `picoarch` and `picoarch_hi`. The portable
Zig PicoArch build passes static ABI checks but raised SIGFPE on the tested v2.7
device; release packages therefore use GNU-toolchain executable builds. This
limitation does not apply to the FrogUI core, video-player shared module, or the
small R36SX helper applications.

## 3. Generate the boot bitmap

```powershell
powershell -ExecutionPolicy Bypass -File scripts/make_switchfrogui_boot_source.ps1

powershell -ExecutionPolicy Bypass -File scripts/make_r36sx_boot_logo.ps1 `
  -SourcePng assets/switchfrogui-boot.png `
  -OutputBmp .r36sx-build/out/xgame-logo-r36sx.bmp
```

The output is an uncompressed 640x480, 32-bit, top-down BMP accepted by H.OS.

## 4. Create a maintainer release

Release assembly is intentionally allow-list based. Point it at a locally
tested card tree; it copies only required runtime files and rejects ROM, BIOS,
save, history, and log patterns.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/New-R36SXRelease.ps1 `
  -TestedCardRoot 'C:\TreeFrogUI\TESTCARD' `
  -OutputDirectory '.\dist'
```

Review `dist/SHA256SUMS.txt` and `dist/PUBLISH-AUDIT.txt` before attaching the
ZIP to a GitHub Release.

## 5. Curate an existing Nintendo library

`Curate-EnglishNintendoRoms.ps1` checks GBA, NES, and SNES ZIP contents against
the current Libretro No-Intro metadata. It recognizes headered NES/SNES dumps,
uses the GBA cartridge region byte as a fallback, renames verified English
titles, writes `.metadata.tsv` category/description data for the game-details
panel, and moves verified non-English titles to a recoverable `Quarantine`
folder. Ambiguous ROMs are reported and left untouched. ROMs referenced by
favourites, recent/play-time history, battery saves, save states, or screenshots
are marked `Protected` and always remain active, regardless of language.

Run a report first, review its CSV, then explicitly apply it:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Curate-EnglishNintendoRoms.ps1 `
  -CardRoot 'E:\' `
  -DatabaseRoot 'C:\src\libretro-database\metadat' `
  -ReportDirectory '.\reports'

powershell -ExecutionPolicy Bypass -File scripts/Curate-EnglishNintendoRoms.ps1 `
  -CardRoot 'E:\' `
  -DatabaseRoot 'C:\src\libretro-database\metadat' `
  -ReportDirectory '.\reports' `
  -Apply
```

Artwork, PicoArch save/screenshot basenames, favourites, recent paths, and
play-time records follow verified renames. The database checkout is not bundled;
obtain it from <https://github.com/libretro/libretro-database>.
