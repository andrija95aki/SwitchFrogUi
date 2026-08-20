# SwitchFrogUI for R36SX

This branch is an R36SX v2.7 / H.OS 1.2-focused fork of
[TreeFrogUI](https://github.com/tzubertowski/treefrog-ui). It keeps the original
TreeFrogUI and FrogUI attribution and license terms, and adds the console-style
launcher, media applications, device workarounds, and stability fixes developed
for this hardware.

The repository contains source, reproducible patches, build scripts, and
documentation. It deliberately contains **no commercial ROMs, console BIOS
images, save data, play history, or personal card logs**.

## Start here

- [Download the ROM-free R36SX SD-card overlay](https://github.com/andrija95aki/SwitchFrogUi/releases/latest)
- [Download the latest SwitchFrogUI package](https://github.com/andrija95aki/SwitchFrogUi/releases/latest)
- [Browse the buildable source branch](https://github.com/andrija95aki/SwitchFrogUi/tree/r36sx-source-build)
- [Install the prebuilt R36SX overlay](docs/r36sx/INSTALL.md)
- [Build from source](docs/r36sx/BUILDING.md)
- [Features and fixes](docs/r36sx/FEATURES.md)

This branch also carries the compatible, low-disruption parts of upstream
TreeFrogUI 1.0.12: ten credited optional platform icon packs, optional friendly
platform names, scroll-position indicators, saved-action confirmations,
GameSwitcher screenshot caching, Home-selection restoration, and the
`cubevol` volume-key fix. The existing Switch-style Home, themes, static UI,
R36SX display correction, and emulator changes remain the defaults.

The Home screen also includes an Ebooks application backed by TreeFrogUI's
MuPDF reader. Doom, Heretic, and Hexen have separate optional library folders,
and additional installed routes cover FBNeo, MAME 2003-Plus, Beetle Lynx,
Snes9x 2010, Vectrex, Odyssey 2, and Videopac. Twelve four-colour gradient
themes extend the theme gallery without changing the selected default.
- [Annotated hardware screenshot gallery](docs/r36sx/SCREENSHOTS.md)
- [Third-party software and sample-audio notices](docs/r36sx/THIRD_PARTY_NOTICES.md)

<p align="center">
  <img src="docs/r36sx/screenshots/home-last-played-games.jpeg" width="760" alt="SwitchFrogUI Home screen on R36SX">
</p>

The gallery contains photographs of the build running on real R36SX v2.7
hardware, including a side-by-side display-glitch correction comparison.

## Supported target

- R36SX motherboard v2.7
- H.OS 1.2 stock card layout
- 640x480 display
- MIPS32r2 little-endian, hard-float
- Linux 4.4 / glibc 2.19-compatible runtime

Other TreeFrogUI devices remain supported by upstream, but this branch and its
release package are intentionally scoped to R36SX.

## Important distribution note

The downloadable package is a ROM-free **SwitchFrogUI overlay** for a legitimate
stock H.OS 1.2 card. It is not a redistributable H.OS firmware image. Start from
your own working stock card, then merge the release files onto it. This avoids
redistributing proprietary firmware while still providing a copy-and-boot
SwitchFrogUI installation. SwitchFrogUI remains based on TreeFrogUI and retains
the upstream project attribution and licence terms.
