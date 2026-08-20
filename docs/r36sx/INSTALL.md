# Install on R36SX v2.7 / H.OS 1.2

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
