# SwitchFrogUI 1.4.1 for R36SX

## A note from Andrija

A big thank you to YouTubers **SjslTech** and **MartStratIV** for their reviews
of SwitchFrogUI, and to
everyone who has tried the OS! I really appreciate your time, feedback and
patience.

I'm sorry for the bugs in the release and the frustration they've caused.
SwitchFrogUI is a hobby project, and these days I spend more time developing
the OS than actually playing games on it! Your testing and bug reports help
me make it better. Thank you for giving it a try and being part of the project.

## Installation

Complete, ROM-free SD-card packages consolidating the controls/save-state update,
licensed BIOS alternatives and menu idle-power work. Extract **all contents
directly to an empty FAT32 card root**, including hidden Rockbox files. No
separate stock installation is needed. Keep your existing card as a backup.

| Download | Compatibility |
| --- | --- |
| `SwitchFrogUI-1.4.1-R36SX-v2.7.zip` | R36SX motherboard v2.7; working H.OS 1.2 base |
| `SwitchFrogUI-1.4.1-R36SX-v2.6.zip` | Experimental motherboard v2.6 target; untested |

## New in 1.4.1

- Sleep-based 60 Hz menu pacing; unchanged frames are reused instead of copying
  the full screen continuously, with no presents while blanked on supported hosts.
- Dynamic CPU governor/hardware-minimum clock in the frontend where supported;
  exact previous settings restored before handing off to games/apps. Maximum
  clocks, voltage, thermal limits and emulator/video runtimes are unchanged.
- Static boot graphic identifies SwitchFrogUI Version 1.4.1.

## Included from today's controls and packaging update

- List-based button remapping: B can be mapped without exiting, conflicts swap,
  and physical A/B/D-pad navigate the page independently of current mappings.
  Reset All or hold physical **Start + L1 + R1** for about two seconds to recover.
- Game Details save-slot selection (Up/Down, then A). Launch PS1 games once to
  register disc IDs for existing states. Quick Resume loads the last successfully
  saved state; an explicit slot wins. New saves record order independently of
  broken device clocks; old states with tied dates need manual slot selection.
- MENU/FN or SELECT + L1 saves; +R1 loads. SELECT + START opens emulator menus.
  PicoArch screenshot: MENU + L2; fast-forward: MENU + R2. Requires core state support.
- On-screen keyboard fits the display. Removed the Game Switcher settings toggle.
- Fresh-install Ocean Depth Gradient; emulator/platform folders included even
  when empty, with `ROM-FOLDERS.md` explaining where games belong.
- Optional PS1 OpenBIOS, gpSP/ReGBA GBA firmware and SameBoy GB/GBC boot ROMs,
  licenses, provenance/source and `BIOS-GUIDE.md`. Not activated automatically;
  instructions explain adding legally obtained original BIOSes where needed.

## Package contents

Complete boot/rootfs files, emulator cores, launcher, Rockbox, video/ebook tools
and app data. Samples: SwitchFrogUI test video/subtitles, stock H.OS `sample.mp4`,
Mozart song, three stock photos, World English Bible and JSDev demos.

No game ROMs, proprietary BIOS dumps, saves, favourites, user settings/history
or other private media. `MD/dummy.md` is required bootstrap data, not a game.
Both ZIPs include empty platform folders, hidden `.rockbox` data, a complete
SHA-256 manifest and companion archive checksum.

## Testing caveats

Some components/features are imported from **TreeFrogUI and have not been
thoroughly tested**. Andrija has only been able to test development builds on
**R36SX v2.7; v2.6 is experimental and is not guaranteed to work**. These numbers
are motherboard revisions, not H.OS versions.

New controls, firmware alternatives and menu-power changes still require
physical-device testing. The idle-frame regression test reduces 600 simulated
presents to 10 over ten seconds; actual battery life and temperature improvement
have not been measured. CPU frequency savings depend on firmware support.
Retest boot, menu sounds/input, timeout and power wake, game/video handoff,
remapping recovery and save/load/resume. Release 1.4.0 remains available for rollback.

Credits: TreeFrogUI/FrogUI by Tomasz Zubertowski (Proszty) and contributors;
SwitchFrogUI by Andrija and contributors listed in About/project notices.
All components retain their original licenses.

[Install and controls](https://github.com/andrija95aki/SwitchFrogUi/blob/r36sx-source-build/docs/r36sx/INSTALL.md)
| [Changelog](https://github.com/andrija95aki/SwitchFrogUi/blob/r36sx-source-build/release-notes.md)
| [Build from source](https://github.com/andrija95aki/SwitchFrogUi/blob/r36sx-source-build/docs/r36sx/BUILDING.md)
