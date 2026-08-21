# PlayStation 1

← [back to README](../../README.md)

PS1 has **two cores**. `PS`/`ps1`/`psx` folders prefer the standalone **PCSX4ALL** if its binary is present, falling back to the libretro **pcsx_rearmed** core otherwise; the `ps1r` folder always uses pcsx_rearmed directly (lightrec JIT).

**Use a real BIOS.** Without one, both cores fall back to HLE, which causes **graphical glitches, worse performance, and broken/hanging memory-card saves** (e.g. Harvest Moon). Drop **any** PS1 BIOS in **`cubegm/bios/`** and both cores find it:

| Core | ROM folder | BIOS |
|------|-----------|------|
| **PCSX4ALL** | `PS` | auto-detects any `scph*.bin` in `cubegm/bios/` (legacy `cubegm/cores/.pcsx4all/` still works) |
| **pcsx_rearmed** (lightrec) | `ps1r` | any `scph*.bin` in `cubegm/bios/` |

Filenames are case-insensitive; `scph1001.bin`, `scph5501.bin`, `scph7001.bin`, etc. all work. One file in `cubegm/bios/` covers both cores.

For the best compatibility, install one BIOS for each game region rather than
letting every disc use the first file found. The expected normalized names are
`scph5500.bin` (Japan), `scph5501.bin` (North America), and `scph5502.bin`
(Europe/PAL). `scripts/Install-PS1Bioses.ps1` identifies known dumps by SHA-1,
rejects known bad images, creates the normalized aliases, and writes minimal
per-game PCSX4ALL overrides from the disc-serial report. It never downloads,
commits, renames, or deletes a user's BIOS dump. Example:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Install-PS1Bioses.ps1 `
  -SourceDirectory 'E:\PS1 BIOSES' -CardRoot 'E:\'
```

The script writes `cubegm/bios/PS1-BIOS-INSTALL.txt` and
`PS1-GAME-BIOS-MAP.csv` so every decision can be audited. BIOS images are
copyrighted console firmware and are intentionally excluded from source and
release packages.

> [!NOTE]
> **PCSX4ALL now uses the BIOS automatically.** If a valid BIOS is present it
> switches HLE off on its own at launch - no menu steps. The old "turn HLE off
> in Core Settings" dance is gone; it was only needed because the file wasn't
> being found. If you ever want to force HLE, delete the BIOS from `cubegm/bios/`.

> [!IMPORTANT]
> **BIOS-less compatibility mode:** the bundled PCSX4ALL HLE cannot safely
> complete every asynchronous memory-card request. Without a real BIOS it now
> presents empty card slots, preventing games such as Spyro from hanging at
> `Accessing memory card`; emulator save states still work. Supplying your own
> valid 512 KiB BIOS automatically enables both persistent `.mcr` cards again.

For missing shadows, flat lighting, or opaque effects, open **Start+Select ->
GPU Accuracy / Shadows** and choose **Restore Accurate Defaults**, then save it
globally or for that game. This enables Lighting and Blending, disables the
less-accurate Fast Lighting shortcut, and enables PS1 dithering. Changes apply
to the running game immediately.

**Speed toggles:** for heavy 3D games (e.g. Tekken 3) that don't run full speed, open the PCSX4ALL menu with **`START + SELECT`** (`SELECT + L1` also remains available) and turn on **Pixel Skip** and/or **Interlace** - they trade a little image quality for a real speed boost.

### Hi-Res Fix (for games that freeze or go black)

A few games switch into a **hi-resolution video mode** (480 lines) that the
display driver can't present directly - the screen freezes or goes black while
the game keeps running underneath. Known cases: **Colin McRae Rally 2.0**,
**Worms Armageddon**.

If a game does that, open the PCSX4ALL menu (**`START + SELECT`**) and turn **Hi-Res
Fix** to **On**. It scales those hi-res frames down to something the driver can
show, at a small per-frame CPU cost.

It's **Off by default** because most games never use hi-res mode and the fix
isn't free - leaving it off keeps every normal game running at full speed. Only
flip it on for the specific games that need it (the setting is saved per the
PCSX4ALL config, so it persists once set).

PCSX4ALL is a standalone emulator with its own menu and hotkeys, separate from the shared in-game shortcuts.

### Cheats

PCSX4ALL loads ePSXe-style GameShark lists by the disc serial from
`cubegm/cores/.pcsx4all/cheats/`. Open its in-game menu and select **Cheats**;
all installed codes start disabled.

`scripts/Install-PS1Cheats.ps1` scans the ROM library for actual disc serials
and converts compatible fixed-value entries from the open-source
[DuckStation CHTDB](https://github.com/duckstation/chtdb). It deliberately
rejects parameter placeholders and opcode families this PCSX4ALL engine does
not implement. The generated CSV report identifies games for which the
database has no safe compatible entry; codes are never borrowed from another
region or revision.
