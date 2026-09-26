# Install SwitchFrogUI 1.4.1 on R36SX

Choose the ZIP for the revision printed on your motherboard:

- `SwitchFrogUI-1.4.1-R36SX-v2.7.zip`: working H.OS 1.2 boot base.
- `SwitchFrogUI-1.4.1-R36SX-v2.6.zip`: experimental, not hardware-tested or guaranteed.

Only v2.7 has been tested during development. Some imported TreeFrogUI features
have not been thoroughly tested; new controls and menu-power changes also need
device testing. See [menu power notes](MENU-POWER.md); battery savings are not
yet measured.

## Full-package installation (1.4.0 and later)

1. Keep your existing card as a backup and prepare an empty FAT32 SD card.
2. Extract **all ZIP contents directly to its root**, including `rootfs`,
   `cubegm`, `frogui`, `roms`, and `MD`. Do not add an enclosing folder.
3. Include hidden files: `roms/rockbox/.rockbox` is required by Music.
4. Safely eject and boot. The logo shows SwitchFrogUI Version 1.4.1 and the
   default theme is Ocean Depth Gradient.

No separate stock download is required. Do not overwrite this package with a
different board's stock files. Games and proprietary console BIOS dumps are not included;
add your own to the appropriate `roms/<platform>` and emulator BIOS directories.
Empty platform folders are already included. Read `ROM-FOLDERS.md` in the card
root for the folder guide; `cores.md` lists systems and requirements.
The MD dummy/filelist files are required bootstrap data, not games.
Optional open-source BIOS alternatives are under `cubegm/bios/open-source/`.
Read the card-root `BIOS-GUIDE.md` (or [online guide](BIOS-GUIDE.md)) for setup,
compatibility warnings and exact locations for your own original firmware.

Samples are under Videos, Music, Photo, Ebooks and roms/JSDev.

## New controls

- Button Remap: select a logical button, then a physical one. Conflicts swap.
  The page always uses physical A/B and D-pad, regardless of remapping.
- Reset: use Reset All to Defaults or hold physical **Start + L1 + R1** for
  about two seconds anywhere in SwitchFrogUI.
- Game Details: Up/Down selects a saved slot; A launches and loads it. Open
  PS1 games once so PCSX4ALL can register their disc IDs for the save list.
- Quick Resume loads the newest state. Explicit slot choice takes priority.
  Battery saves and memory cards remain managed inside each game.
  New saves record their order independently of the clock. Old saves with
  identical dates need explicit selection until a new save establishes order.
- MENU/FN + L1 saves; MENU/FN + R1 loads. SELECT also works as modifier.
- SELECT + START opens the emulator menu. PicoArch uses MENU + L2 for
  screenshots and MENU + R2 for fast-forward when enabled.
- SELECT toggles the on-screen keyboard. Cores without state serialization
  cannot support emulator save/load shortcuts.

Retest FIT playback, B remapping, reset, keyboard edges, save/load and resume
on your device. Restore your card backup to roll back.

---

The following instructions apply **only to older overlay releases**, not the
complete 1.4.x ZIPs described above.

## What you need

- A working R36SX v2.7 stock H.OS 1.2 SD card or a backup of one.
- A FAT32 SD card with enough room for the stock system and your own games.
- The `SwitchFrogUI-R36SX-HOS-1.2.zip` release asset.

The release contains no ROMs or console BIOS files. The included Mozart sample
is a CC0/public-domain performance for testing Rockbox.

## Install

1. Back up the original card.
2. Copy the stock H.OS 1.2 card to the new FAT32 card.
3. Extract the release ZIP.
4. Copy everything inside its `SD_ROOT` folder to the root of the new card.
5. Merge folders and allow the SwitchFrogUI files to overwrite matching paths.
6. Safely eject the card and boot the R36SX.

Do not remove the release's root-level `MD/dummy.md` or `MD/filelist.csv` while
cleaning ROMs. They are tiny stock-launcher autorun bootstrap files, not games;
without them the SwitchFrogUI hijack never starts and boot stops at a black
screen before diagnostic logging begins.

Do not copy a `roms` or `bios` folder from an untrusted download. Add only ROMs
and BIOS files that you are legally entitled to use.

## First test

- The static boot screen should show a gamepad and `SwitchFrogUI`.
- Home should show Music, Videos, Files, and Settings even with no games.
- Music opens Rockbox at the SD-card root. The sample recording is under
  `Music/SwitchFrogUI Samples/`.
- Videos opens the hardware video browser. Add your own supported video to any
  card folder and select it with A.
- Add games under `roms/<platform>/`; non-empty platform cards appear after a
  restart.

To enable diagnostic logging, create an empty `log.txt` at the card root and
reboot. Delete it again after testing to avoid unnecessary SD-card writes.

## Safe rollback

Because installation starts from your own stock card, rollback is simply a
restore of your backup. Long-press power shutdown remains handled by H.OS.
