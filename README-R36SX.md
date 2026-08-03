# TreeFrogUI Contributions for R36SX

This branch is an R36SX v2.7 / H.OS 1.2-focused fork of
[TreeFrogUI](https://github.com/tzubertowski/treefrog-ui). It keeps the original
TreeFrogUI and FrogUI attribution and license terms, and adds the console-style
launcher, media applications, device workarounds, and stability fixes developed
for this hardware.

The repository contains source, reproducible patches, build scripts, and
documentation. It deliberately contains **no commercial ROMs, console BIOS
images, save data, play history, or personal card logs**.

## Start here

- [Download the ROM-free R36SX SD-card overlay](https://github.com/andrija95aki/treefrog-ui/releases/latest)
- [Direct ZIP download](https://github.com/andrija95aki/treefrog-ui/releases/download/r36sx-v1.0.0/TreeFrogUI-Contributions-R36SX-HOS-1.2.zip)
- [Browse the buildable source branch](https://github.com/andrija95aki/treefrog-ui/tree/r36sx-source-build)
- [Install the prebuilt R36SX overlay](docs/r36sx/INSTALL.md)
- [Build from source](docs/r36sx/BUILDING.md)
- [Features and fixes](docs/r36sx/FEATURES.md)
- [Third-party software and sample-audio notices](docs/r36sx/THIRD_PARTY_NOTICES.md)

Screenshots will be added after the device build has completed its final
hardware test pass.

## Supported target

- R36SX motherboard v2.7
- H.OS 1.2 stock card layout
- 640x480 display
- MIPS32r2 little-endian, hard-float
- Linux 4.4 / glibc 2.19-compatible runtime

Other TreeFrogUI devices remain supported by upstream, but this branch and its
release package are intentionally scoped to R36SX.

## Important distribution note

The downloadable package is a ROM-free **TreeFrogUI overlay** for a legitimate
stock H.OS 1.2 card. It is not a redistributable H.OS firmware image. Start from
your own working stock card, then merge the release files onto it. This avoids
redistributing proprietary firmware while still providing a copy-and-boot
TreeFrogUI installation.
