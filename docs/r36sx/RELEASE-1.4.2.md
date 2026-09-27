# SwitchFrogUI 1.4.2 for R36SX

## Fixed in this release

- **Terminal:** the active command line stays above the on-screen keyboard and
  button hints. Output uses the remaining space without overlapping the prompt.
- **Disappearing text protection:** font replacement retains the working font
  if the replacement cannot be read, allocated or validated. Repeated settings
  application no longer reloads the same font. Keyboard label scaling no longer
  mutates shared font scale. A built-in lightweight font keeps text visible when
  font files or glyph allocations are unavailable.
- Static boot graphic and build identification updated to **1.4.2**.

Host checks passed for 12 fonts, three sizes, 720 keyboard-style render cycles,
font-loading/allocation failures, fallback rendering, and 48 terminal layouts.
Please retest keyboard opening/closing in search, Terminal and the editor on
your device. The intermittent hardware failure has not been reproduced here;
these checks cover the identified font-loss paths, not a hardware guarantee.

## Downloads and installation

| Package | Compatibility |
| --- | --- |
| `SwitchFrogUI-1.4.2-R36SX-v2.7.zip` | R36SX motherboard v2.7; H.OS 1.2 boot base |
| `SwitchFrogUI-1.4.2-R36SX-v2.6.zip` | Experimental motherboard v2.6; untested |

Extract **all files directly to the root of an empty FAT32 card**, including
hidden Rockbox files. These are complete card packages: no separate stock
download is needed. Keep the previous card as a backup; 1.4.1 remains available.
Use the package matching the revision printed on your motherboard.

Includes emulators and empty platform folders, Rockbox, video and ebook apps,
JSDev demos, test videos/subtitles and stock `sample.mp4`, Mozart music, three
stock photos and the World English Bible. Fresh installs use Ocean Depth
Gradient. Optional open BIOS alternatives include licenses and setup guidance.

No game ROMs, proprietary BIOS dumps, user saves, favourites, personal settings
or private media are included. `MD/dummy.md` is required bootstrap data, not a
game. Each ZIP has a full payload SHA-256 manifest and companion archive checksum.

## Known issues and testing caveats

**Quick Resume is not fixed in this release.** It can make emulators unstable;
disable it in Settings if you encounter black video, bad audio or freezes.
A separate emulator restore fix is planned. Video/emulator binaries are unchanged.

Some features are imported from **TreeFrogUI** and have not been thoroughly
tested. Andrija has only tested development builds on **R36SX v2.7**;
**v2.6 is experimental and is not guaranteed to work**. The new UI fixes still
need physical-device confirmation on both boards.

## A note from Andrija

Thank you to YouTubers **SjslTech** and **MartStratIV** for reviewing SwitchFrogUI,
and to everyone who has tried the OS. I'm sorry for the bugs and frustration.
SwitchFrogUI is a hobby project; I spend more time developing the OS than
actually playing games on it! Your testing, feedback and patience mean a lot.

Thanks to TreeFrogUI/FrogUI developer Tomasz Zubertowski (Proszty), upstream
contributors and everyone credited in About and the third-party notices.
All components retain their original licenses.

[Installation](https://github.com/andrija95aki/SwitchFrogUi/blob/r36sx-source-build/docs/r36sx/INSTALL.md)
| [Full changelog](https://github.com/andrija95aki/SwitchFrogUi/blob/r36sx-source-build/release-notes.md)
| [Build from source](https://github.com/andrija95aki/SwitchFrogUi/blob/r36sx-source-build/docs/r36sx/BUILDING.md)
