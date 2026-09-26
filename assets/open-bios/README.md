# Redistributable firmware alternatives

These are optional alternatives, not original console dumps. They are not
automatically enabled and must not replace a user's working BIOS/configuration.
See the card-root `BIOS-GUIDE.md` for setup and compatibility warnings.

## PlayStation: PCSX-Redux OpenBIOS (MIT)

- Author: PCSX-Redux contributors. License: `licenses/OpenBIOS.txt`.
- Binary distributor: https://github.com/RetroPortingToolKit/psxrecomp
  commit `71336b44d20d58ff56a376d6a96df9af21f455e6`, `bios/openbios.bin`.
- The distributor's build record is retained in `ps1/BUILD-PROVENANCE.toml`.
  It identifies PCSX-Redux source `55fbf0468345acfbe5512628a3be5c0cfe3f22e7`
  and uC-sdk `69e06871824e2d62069487a7426ded09090ceb69`. The license notice
  includes the SDK attribution. No modification was made to the binary.
- Source/build instructions:
  https://github.com/grumpycoders/pcsx-redux/tree/55fbf0468345acfbe5512628a3be5c0cfe3f22e7/src/mips/openbios
- Current source mirror: https://github.com/pcsx-redux/nugget/tree/main/openbios
- Experimental on this PCSX4ALL port; bundled does not mean device-tested.

## Game Boy Advance: gpSP / ReGBA replacement (GPL-2.0)

- Written by Normmatt and the VBA/VBA-M team, distributed by libretro/gpSP.
- Source/binary revision: `5819380c2ffb0900219d700a382ee68c464ebb99`.
  https://github.com/libretro/gpsp/tree/5819380c2ffb0900219d700a382ee68c464ebb99/bios
- License and corresponding BIOS source are supplied in `gpsp-source/`,
  including the upstream Makefile. Set DEVKITARM/DEVKITPRO to your devkitARM
  installation with libtonc, then run make in `gpsp-source/bios/`.
  The toolchain/library are build dependencies, not bundled here.
- This is the replacement distributed by gpSP, NOT the similarly named
  Normmatt/gba_bios repository that disassembles Nintendo's original ROM.
- Some games may still require an original dump; newer gpSP builds already
  embed this alternative and can run without an external BIOS file.

## Game Boy / Game Boy Color: SameBoy boot ROMs (Expat/MIT)

- Author: Lior Halphon / SameBoy contributors.
- Unmodified `dmg_boot.bin` and `cgb_boot.bin` extracted from the official
  `sameboy_winsdl_v1.0.3.zip` release. License: `licenses/SameBoy.txt`.
- https://github.com/LIJI32/SameBoy/releases/tag/v1.0.3
- Source/build instructions: https://github.com/LIJI32/SameBoy/tree/v1.0.3/BootROMs
- Optional boot animation/initialization alternatives. Most included GB/GBC
  cores also work with boot ROMs disabled; importing these is not required.

Exact firmware hashes are pinned in `scripts/Install-OpenBios.ps1` in the
SwitchFrogUI source; full-card ZIP manifests verify every distributed file.
