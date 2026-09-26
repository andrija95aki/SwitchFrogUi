# SwitchFrogUI BIOS guide

The release contains open-source alternatives only, not Sony/Nintendo/Sega or
other proprietary BIOS dumps. Use your own lawfully obtained files for systems
that need them. Do not upload those files to this project's repository.

## Where files go

The system/BIOS directory for this build is **`cubegm/bios/` at the SD-card root**
(`/mnt/sdcard/cubegm/bios` on the device), not a RetroArch `system/` directory.
For example, Windows drive E uses `E:\cubegm\bios\gba_bios.bin`.
Extract individual firmware files unless a core requires a ZIP. Preserve exact
names/case, avoid doubled extensions such as `.bin.bin`, and restart the game
after changing firmware. Back up your existing BIOS, settings and saves first.

## Included open-source alternatives

All four binaries, licenses, provenance and GBA BIOS source are under
`cubegm/bios/open-source/`. They are **optional and not automatically activated**:
this preserves working installations and avoids silently changing save-state
compatibility. These combinations still need physical R36SX testing.

| Platform | Included file (relative to open-source/) | How to try it |
| --- | --- | --- |
| PlayStation | `ps1/openbios.bin` | Copy to `cubegm/bios/openbios.bin`; select it as described below. MIT-licensed PCSX-Redux OpenBIOS; experimental with this port. |
| GBA | `gba/open_gba_bios.bin` | Copy to `cubegm/bios/gba_bios.bin`, only after backing up any existing file. GPL-2.0 gpSP/ReGBA replacement; some games differ from original BIOS. |
| Game Boy | `sameboy/dmg_boot.bin` | For cores using this name, copy to `cubegm/bios/dmg_boot.bin` and enable their boot-ROM option. mGBA instead calls it `gb_bios.bin`. |
| Game Boy Color | `sameboy/cgb_boot.bin` | Copy to `cubegm/bios/cgb_boot.bin` and enable the core's boot-ROM option. mGBA instead calls it `gbc_bios.bin`. |

Do not overwrite a known-working original BIOS just to use an alternative.
Normal GB/GBC, NES cartridge, SNES cartridge and Mega Drive cartridge play
usually needs no external BIOS. GBA gpSP has an open replacement fallback;
mGBA can emulate BIOS services (disable its "Use BIOS file if found" option).
Select a different emulator from Game Details / SELECT where available.
See [gpSP](https://docs.libretro.com/library/gpsp/),
[mGBA](https://docs.libretro.com/library/mgba/) and
[Gearboy](https://docs.libretro.com/library/gearboy/) documentation.

### Selecting PS1 firmware

The default `roms/ps1` launcher uses **PCSX4ALL**, not PCSX ReARMed.
With the device off, edit `cubegm/cores/.pcsx4all/pcsx4all.cfg` (create the
file if absent). Change/add these lines without deleting other settings:

```text
BiosDir /mnt/sdcard/cubegm/bios
Bios openbios.bin
HLE 0
```

Per-game profiles in the same directory can override the global choice. Apply
the same three lines to the affected game's existing `.cfg` profile if needed;
do not delete it or change memory-card/save paths. For an original BIOS, replace
`openbios.bin` above with its actual filename, such as `scph5501.bin`.
New installs also auto-detect a 512 KiB BIOS placed directly in `cubegm/bios`,
preferring `scph*` names; an existing valid configured choice takes precedence.

PCSX4ALL's BIOS-free HLE fallback is less compatible. This build disables
memory cards in that fallback to avoid known card-access hangs; save states
remain available. OpenBIOS is not a promise to fix every game's card behaviour.
Disable Quick Resume and cold-start when switching BIOS/core; do not use an old
save state to test a new BIOS. Back up memory cards and test saving/loading too.

`roms/ps1r` / the ReARMed alternative has different detection rules. Its
documented original filenames include `scph5501.bin`, `scph1001.bin`,
`scph7001.bin`, `scph101.bin` and `PSXONPSP660.bin`. OpenBIOS is not automatically
selected there, and this package does not disguise it under a Sony filename.
See [PCSX ReARMed](https://docs.libretro.com/library/pcsx_rearmed/).

## Adding original firmware (not bundled)

These paths are relative to the card root. Only install files for systems you
use. Match the core and firmware version; renaming an unrelated file is not a
conversion. Other hardware/system variants may require additional files.

| System / core | Destination / required names |
| --- | --- |
| PS1 / PCSX4ALL | `cubegm/bios/scph5501.bin` (US), `scph5500.bin` (Japan), or a valid PAL BIOS such as `scph5502.bin`; set the actual name in the profile above. |
| GBA | `cubegm/bios/gba_bios.bin` (16,384 bytes). |
| Famicom Disk System / FCEUmm | `cubegm/bios/disksys.rom`; regular NES cartridges do not need it. [Core guide](https://docs.libretro.com/library/fceumm/) |
| Sega CD / Genesis Plus GX | `cubegm/bios/bios_CD_U.bin`, `bios_CD_E.bin`, `bios_CD_J.bin` for the matching disc region. [Core guide](https://docs.libretro.com/library/genesis_plus_gx/) |
| PC Engine CD / Beetle PCE Fast | `cubegm/bios/syscard3.pce`; HuCard games normally do not need it. [Core guide](https://docs.libretro.com/library/beetle_pce_fast/) |
| PC-FX | `cubegm/bios/pcfx.rom`. [Core guide](https://docs.libretro.com/library/beetle_pc_fx/) |
| Atari Lynx / Handy | `cubegm/bios/lynxboot.img`. [Core guide](https://docs.libretro.com/library/handy/) |
| ColecoVision / Gearcoleco | `cubegm/bios/colecovision.rom`. [Core guide](https://docs.libretro.com/library/gearcoleco/) |
| Intellivision / FreeIntv | `cubegm/bios/exec.bin` and `cubegm/bios/grom.bin`. [Core guide](https://docs.libretro.com/library/freeintv/) |
| Odyssey2 / O2EM | `cubegm/bios/o2rom.bin`; other machine modes need their own firmware. [Core guide](https://docs.libretro.com/library/o2em/) |
| MSX / blueMSX | Preserve the complete `Machines/` and `Databases/` directory trees inside `cubegm/bios/`; do not flatten them. [Core guide](https://docs.libretro.com/library/bluemsx/) |
| C64 / this VICE port | `cubegm/bios/vice/kernal`, `basic`, `chargen`; disk emulation may also need `dos1541`, `dos1571`, `dos1581`. Use this port's `cores.md` instructions, not assumptions about newer VICE versions that embed ROMs. |
| Neo Geo / PGM / FBNeo or FBA | Keep `neogeo.zip` / `pgm.zip` zipped beside the games in `roms/neogeo/`, `roms/fbneo/` or the actual arcade ROM folder. Use BIOS sets matching the selected core/ROM-set version; not every arcade game needs them. [FBNeo guide](https://docs.libretro.com/library/fbneo/) |

For Atari computers, Amiga, PC-88, X68000 and other less-tested imported cores,
follow the exact core listed in `cores.md` and its upstream firmware guide.
The [Libretro BIOS information hub](https://docs.libretro.com/library/bios/)
links the system-specific filenames/checksums. Substitute `cubegm/bios/` for
the frontend's system directory, retaining any required subdirectories.
Do not assume a core is usable merely because its folder is included.

### Validate and troubleshoot

1. Verify the file against the selected core's documented size and checksum.
   In PowerShell: `Get-FileHash -Algorithm MD5 'E:\cubegm\bios\gba_bios.bin'`.
   Nintendo's original GBA BIOS MD5 is `a860e8c0b6d573d191e4ec7db1b1e4f6`;
   the included open replacement intentionally has a different hash.
2. Safely eject, restart the device and cold-launch one game without Quick
   Resume. Check both normal play and saving/loading before broader use.
3. If it fails, restore your previous BIOS/configuration. Never fix a BIOS
   failure by deleting saves, memory cards or favourites.

No complete, verified open replacement set for every shipped console was
identified. Public packages therefore keep proprietary firmware excluded and
document alternatives rather than claiming universal BIOS compatibility.
