# PCSX4ALL Start+Select menu patch

Target: the MIPS32 little-endian `pcsx4all` build dated 2026-07-30, original
SHA-256 `AC67AF3B8D0D1C35B0BFFAD810FF791D6CB71738F0D3A832CF992CDF53688433`.

The emulator's input loop treated Start+Select as a clean exit while its menu
used Select+L1. TreeFrogUI therefore restarted normally (`rc=0`), which looked
like an in-game-menu crash. Two instructions are changed:

| Virtual address | Original | Patched | Purpose |
|---|---|---|---|
| `0x0040A7DC` | `bne a2,zero,0x0040AC90` | `bne a2,zero,0x0040A7FC` | Route Start+Select to the existing menu path instead of the exit path. |
| `0x0040A828` | `andi v0,v0,0x0400` | `andi v0,v0,0x0408` | Wait for both L1 and Start to be released before presenting the menu. |

Apply with `apps/patch-pcsx4all-hotkey.ps1`. The script is checksum-guarded and
also verifies the original instruction bytes before writing.
