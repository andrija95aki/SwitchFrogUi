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
- [Complete stock H.OS and TreeFrogUI comparison](https://github.com/andrija95aki/SwitchFrogUi/blob/r36sx-source-build/docs/r36sx/ADDITIONS.md)
- [Create JavaScript games with JSDev](docs/r36sx/JSDEV.md)

## What this fork adds

| Compared with | Highlights |
| --- | --- |
| **Stock H.OS 1.2** | Switch-style Home and game library, broad emulator/core collection, save and resume tools, Rockbox, hardware video with subtitles, Ebooks, Files, Terminal, Text Editor, JSDev, themes/fonts/sounds, keyboard mapping, diagnostics, and R36SX display/power/volume fixes. |
| **Pinned TreeFrogUI/FrogUI** | New horizontal Home and game views, utility applications, R36SX-specific display and resume engineering, PS1 repairs, global keyboard support, modern settings/themes, hardware diagnostics, reproducible build scripts, and an audited ROM-free release workflow. |

The [complete additions page](https://github.com/andrija95aki/SwitchFrogUi/blob/r36sx-source-build/docs/r36sx/ADDITIONS.md) expands both comparisons
into categorized, line-by-line feature lists with upstream integrations clearly
credited.

This branch also carries the compatible, low-disruption parts of upstream
TreeFrogUI 1.0.12: ten credited optional platform icon packs, optional friendly
platform names, contained/non-stretched icon-pack logos, scroll-position
indicators, saved-action confirmations,
GameSwitcher screenshot caching, Home-selection restoration, and the
`cubevol` volume-key fix. The existing Switch-style Home, themes, static UI,
R36SX display correction, and emulator changes remain the defaults.

The Home screen also includes an Ebooks application backed by TreeFrogUI's
MuPDF reader. Doom, Heretic, and Hexen have separate optional library folders,
and additional installed routes cover FBNeo, MAME 2003-Plus, Beetle Lynx,
Snes9x 2010, Vectrex, Odyssey 2, and Videopac. Twelve four-colour gradient
themes extend the theme gallery without changing the selected default.

External USB keyboards are usable throughout the launcher and emulators via a
persistent, fully editable keyboard-to-gamepad map. Home additionally exposes a
real BusyBox `ash` terminal and a keyboard-driven **Text Editor** whose picker
starts at the SD-card root. The terminal uses a larger monospace text-only screen,
persistent Up/Down command history, and disables automatic screen timeout while
open. The Files app opens common text, Markdown, log, config, source, subtitle,
and playlist formats directly in the editor, which can create, edit, rename,
and save files with any extension. The earlier Mini Linux shortcut shell was removed because its cards
only duplicated Terminal, Files, and System Information already available in
the main interface.

Text Editor keyboard controls follow desktop conventions: arrows and
Page Up/Down navigate, Home/End target a line, Ctrl+Home/End target the whole
document, Ctrl+Left/Right moves by word, and Shift extends a selection. Ctrl+A,
Ctrl+C/X/V, Ctrl+Z/Y (or Ctrl+Shift+Z), Ctrl+S, Ctrl+F with F3, Ctrl+N, Ctrl+O,
Ctrl+Backspace/Delete, Tab, and Shift+Tab are supported.

Home also includes **JSDev**, an offline JavaScript game-development runtime.
Its project browser creates, edits, renames, and runs `.js` files from
any folder, starting at the SD-card root; the bundled demo remains in
`roms/JSDev`. The API supplies RGB565 graphics and antialiased text, 60 Hz
animation callbacks, gamepad events and held-button input, four synthesized
sound waveforms, timers, logging, and per-project JSON storage. A complete
`JSDev API Showcase.js` demo is bundled.

All file-selection applications now open at `/mnt/sdcard`. Three public-domain
English test ebooks are bundled under `Ebooks/SwitchFrogUI Samples`: the World
English Bible, Rodwell Qur'an, and 1917 JPS Tanakh. Holding FN + L1 + R1 flips
the complete display 180 degrees in the UI, terminal, editor, media apps, and
emulators; repeating the chord restores normal orientation, and the state is
saved across app transitions and reboots.

- [Annotated hardware screenshot gallery](docs/r36sx/SCREENSHOTS.md)
- [Third-party software and sample-media notices](docs/r36sx/THIRD_PARTY_NOTICES.md)

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
