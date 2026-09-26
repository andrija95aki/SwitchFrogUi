# SwitchFrogUI 1.4.0 for R36SX

Two complete SD-card ZIPs are attached. Pick the revision printed on your
motherboard and extract **all contents directly to an empty FAT32 card root**.
No separate stock installation is needed. Include hidden files, especially
`roms/rockbox/.rockbox`.

| Package | Compatibility |
| --- | --- |
| `SwitchFrogUI-1.4.0-R36SX-v2.7.zip` | R36SX v2.7, working H.OS 1.2 boot base |
| `SwitchFrogUI-1.4.0-R36SX-v2.6.zip` | Experimental R36SX v2.6 boot base; untested |

## Changes

- **Button mapping:** choose a logical button from a list, then choose its
  physical button. Conflicts swap instead of creating duplicates. Remapping B
  no longer exits the page. Physical A/B and D-pad always navigate this page.
- **Recovery:** Reset All to Defaults, or hold physical **Start + L1 + R1**
  for about two seconds in SwitchFrogUI. Invalid old maps recover to defaults.
- **Select a save:** use Up/Down on Game Details to select an existing emulator
  state, then A to launch and load it. PCSX4ALL needs one launch per game to
  register the disc ID used by its existing save filenames.
- **Quick Resume:** opening a game loads the newest saved state. Explicit
  slot selection takes priority. Battery saves/memory cards remain in-game data.
  The last successfully saved slot is recorded independently of the device
  clock. Older untracked saves use their dates; tied dates cannot establish
  their original save order: select the intended slot manually and save again
  to establish a reliable order for subsequent launches.
- **Shortcuts:** MENU/FN + L1 saves; MENU/FN + R1 loads the selected slot in
  PicoArch and PCSX4ALL. SELECT is also a modifier. SELECT + START opens the
  emulator menu. PicoArch screenshot moves to MENU + L2; fast-forward to
  MENU + R2. Save states require emulator serialization support.
- **Keyboard:** all on-screen key rows fit within the display width.
- **Settings:** removed the Game Switcher toggle; existing Recent presentation
  preferences remain compatible.
- **Presentation:** Ocean Depth Gradient is the fresh-install theme; the
  static boot graphic shows SwitchFrogUI Version 1.4.0.
- Retains the working 1.3.7 video sizing fix and persistent directory locks,
  media apps, subtitle offset, file management, photo viewer and JSDev.

## Included and excluded

Included: board boot files, launcher, emulator cores, Rockbox and its data,
video/ebook tools, the SwitchFrogUI test video/subtitles and stock `sample.mp4`,
the Mozart song, three stock-card photos, World English Bible, and JSDev demos.
Empty platform/emulator ROM folders are included, with `ROM-FOLDERS.md`
explaining where to put games. Platform cards appear after supported games
are added.

### Open firmware package refresh (2026-09-27)

Both full-card ZIPs now include optional PCSX-Redux OpenBIOS (PS1), the gpSP/ReGBA
GBA replacement, and SameBoy GB/GBC boot ROMs. Licenses, GBA BIOS source, pinned
provenance and `BIOS-GUIDE.md` are included. These alternatives are not enabled
automatically; existing working BIOS selections should be preserved. They are
not guaranteed substitutes for original firmware and need physical-device tests.
The runtime binaries remain the published 1.4.0 build; the separate local
menu-power test build has not been promoted into these ZIPs.

Excluded: game ROMs, proprietary console BIOS dumps, user saves, favourites, history,
custom settings, other media and custom content. `MD/dummy.md` is a tiny required
boot hook, not a game. Supply your own BIOS for systems that require one.

## Testing notes

Some features and components are imported from TreeFrogUI and **have not been
thoroughly tested**. I have only been able to test development builds on
**R36SX v2.7** and **cannot guarantee operation on v2.6**. New 1.4.0 changes
still require physical-device testing; the v2.6 label is an experimental target,
not a certification. 2.6/2.7 are board revisions, not H.OS version numbers.

The GNU cross-build and remapping regression checks passed. Packages are
audited for excluded content, required boot/app files, hidden Rockbox data,
portable archive paths, and per-file SHA-256 hashes. Each ZIP has a companion
`.sha256` file and contains its own full checksum manifest.

Credits: TreeFrogUI/FrogUI by Tomasz Zubertowski (Proszty) and contributors;
SwitchFrogUI by Andrija and the contributors listed in About and project notices.
All components retain their original licenses/terms.

[Installation and controls](https://github.com/andrija95aki/SwitchFrogUi/blob/r36sx-source-build/docs/r36sx/INSTALL.md)
| [Complete changelog](https://github.com/andrija95aki/SwitchFrogUi/blob/r36sx-source-build/release-notes.md)
| [Build instructions](https://github.com/andrija95aki/SwitchFrogUi/blob/r36sx-source-build/docs/r36sx/BUILDING.md)
