# SwitchFrogUI 1.5.0-dev: selective TreeFrogUI integration

Development/test build, not a public release. Based on TreeFrogUI **v1.6.0_c**
(a2219113), a prerelease. Existing SwitchFrogUI Home, video fixes, display
correction, file tools, power policy and default Ocean Depth theme are retained.
Your saved choices are not reset. No games, media or commercial BIOSes are added.

## What changed

| Area | Integration |
| --- | --- |
| Library speed | Persistent checked directory/filter snapshots and negative artwork cache; new files/covers invalidate them. Settings → Library → Rebuild library cache is the manual recovery option. |
| Controls | Fifteenth remappable FN button; keyboard default F1; safe migration of older maps. Existing physical Start+L1+R1 recovery and Menu+L1 save / Menu+R1 load remain. |
| FN shortcut | Optional FN → Settings from Home and FN-alone → libretro game menu. The game shortcut fires on release; FN chords do not accidentally open the menu. Native PCSX4ALL and standalone apps retain their own controls. |
| Settings | Remembered collapsible groups and per-platform extension filters. Filters change visibility, never delete ROMs or disc tracks. Standalone BIN games remain allowed. |
| Audio | Live cubevol master-volume synchronization where the persistentmem ABI is readable; retained perceptual software curve, 48 kHz buffer/prefill and driver handoff. Bounded underrun diagnostics, not per-frame logging. |
| Books | Pinned updated reader: deferred/checked atomic progress, hidden `.positions` sidecars with legacy migration, page/status fixes and bounded MuPDF cache. |
| Battery | Stock indicator remains default. Optional extra LOW/MID/HIGH estimate is explicitly uncalibrated, not an accurate percentage or runtime prediction. |
| OTG | Settings → Library → ROM source. Detects an actual `/media/hdd` mount with `roms` or `ROMS`, otherwise falls back to SD. Covers, favourites, locks and saved launch paths recognize both roots. |
| USB Mode | Settings → System → USB Mode (MTP). Explicit confirmation, dedicated app lifecycle, B exit. Raw mass-storage export is disabled. |
| Java | `roms/j2me`: FroggyKVM JAR and matching-name JAD/JAR pairs; runtime classes, editable game/phone-key mappings and built-in MIDI synthesis. |
| PS1 alternative | QPSX is an experimental **per-game choice**, not the new default. Game Details → X selects an emulator. Native PCSX4ALL remains preferred. |
| Nintendo DS | `roms/nds`: experimental DSperate standalone runtime. Interpreter forced; JIT/native code and audio disabled, matching the audited upstream SF3000 path. |
| Languages | Ten upstream packs, English fallback and translated fork-specific labels (Polish/French additions). Other packs retain English for new fork labels until contributed translations are available. |
| Text | Optional FreeType/HarfBuzz/SheenBidi Unicode and bidirectional UI rendering, DejaVu and WenQuanYi fallbacks. Existing ASCII renderer, transactional font loading and no-heap keyboard fallback remain intact. This does not turn the terminal/editor into a full Unicode terminal. |
| Appearance | Twelve additional Allium palettes and Nunito. Existing themes, sizes and gradients remain available. |
| On-screen keyboard | L2 toggles one-shot Ctrl, R2 Alt in Terminal/Text Editor. No extra keyboard rows; Select still explicitly shows/hides it. |
| Music | Settings → System → Music player selects Rockbox or Lightweight. One Music card; lightweight backend adds metadata, album art and sequential/repeat/random playback. Rockbox remains default. |
| Compatibility reports | Game Details → L2: community PS1 reports matched to recorded disc ID/region; launch through PCSX4ALL once to record an ID. R36HD reports are not R36SX performance guarantees. |
| Core audit | gpSP target, PCSX-ReARMed lightrec and Frodo autoload patches were already identical to the reviewed upstream copies: retain those working cores. MAME2000's load-failure guard is the new targeted delta. |
| Scaling | Existing nine hardware modes already cover the proposed additions, including Fill/Stretch; no duplicate modes or scaler replacement. |

### On-screen modifier shortcuts

- Text Editor Ctrl: **A** select all, **C/X/V** clipboard, **Z/Y** undo/redo,
  **S** save, **F** find. Alt **B/F** moves a word backward/forward.
- Terminal Ctrl **U** clears the input line, **W** deletes the last word,
  **L** clears displayed output. Alt **P/N** recalls previous/next history;
  Alt + Y/Delete deletes a word. The buffered shell is not a PTY: these do not
  claim job-control Ctrl+C/Alt shortcuts for arbitrary interactive programs.
- Modifiers clear after a key or when hiding the keyboard. They are unavailable
  in filename/search fields, where control bytes would be inappropriate.

### Java controls and MIDI

D-pad = 2/8/4/6; A = 5/action, B = 3, X = 0, Y = 1; L1/R1 = phone softkeys;
Select = `*`, Start = `#`. Select+D-pad sends arrows; Start+face buttons provides
diagonals. Edit `config/j2me/default.cfg` or a game's adjacent `.cfg` to customize.
JAD support requires a same-basename local JAR; no network download is performed.
The Java runtime uses `/saves/j2me` on the SD, including for an OTG game.

No Roland SC-55 SoundFont is redistributed. The built-in synthesizer works
without one. An optional SoundFont you have rights to use can be placed at
`cubegm/bios/j2me.sf2`. No Java games are included.

### Experimental emulator separation

QPSX states/frontend options: `picoarch/qpsx-<ROM-parent>/`.
QPSX internal configuration: `picoarch/qpsx-config/`.
QPSX memory cards: `picoarch/qpsx-memcards/`.
BIOS lookup uses the existing `cubegm/bios/` directory; no BIOS is replaced.
Do not exchange save states between QPSX, ReARMed and native PCSX4ALL.
DSperate uses its own `frogui/dsperate/` app-data directory and bundled loader.
DS layouts/touch/exit and real save persistence require on-device acceptance;
no full-speed DS or PS1 performance improvement is promised.

### USB / storage precautions

Only connect MTP to a trusted computer. The upstream responder is experimental;
its uploaded-name hardening is still an upstream caveat. It grants read/write
access to the **whole SD tree**, including directories locked in the UI. Folder
locks are not encryption or a PC-access security boundary. Finish transfers,
close/eject the PC connection, then press B. Interrupted transfers can be partial.
An OTG disk must be removed before USB device mode; never unplug active ROM
storage during a game. Reconnect and the recovered module's kernel/board ABI
need physical testing. This does not add second-SD-slot support.

## Validation and acceptance

Automated checks cover input-map migration/uniqueness/recovery, cache corruption
and invalidation, filter persistence/BIN visibility, locale fallback, disc-ID
CSV matching, state-slot history, menu idle policy, 720 font/keyboard cycles,
48 terminal layouts and 360 Unicode/RTL draw/measure/clipping cycles.
GNU target builds and ELF dependency checks do not prove device compatibility.

On R36SX v2.7 test: cold boot; 10/20/30/60-second blanking and power tap; language
switching and OSK; GBA/SNES/PS1 audio/menu/exit; old favourites and saves; ebook
progress; Music backend switching; SD/OTG reconnect; trusted-PC MTP transfer/B
exit; Java and DS games; QPSX cold boot and **separate** saves. Keep a backup.
The existing native-PCSX4ALL Quick Resume instability is **not fixed by this
integration**; keep Quick Resume disabled if it produces black video/garbled audio.
No physical v2.6 verification has been performed.

## Sources / reproducibility / redistribution

- Integration branch: `integration/treefrog-1.6-20261001` in
  [SwitchFrogUi](https://github.com/andrija95aki/SwitchFrogUi).
- [TreeFrogUI v1.6.0_c](https://github.com/tzubertowski/TreeFrogUI/releases/tag/v1.6.0_c)
  and FrogUI `a23ece7e1ec066c46997084cb70c06e588ef62f5` provide the reviewed deltas,
  DS package, runtime classes and MTP responder/module. Those payloads are not
  presented as newly source-built SwitchFrog components.
- Ebook `e962703cddbdcaaf33f72206c23d8005e4a81696`, MuPDF 1.24.10:
  [reader](https://github.com/tzubertowski/TreeFrogUI_ebook_reader),
  [MuPDF](https://github.com/ArtifexSoftware/mupdf). MuPDF is AGPL/commercial;
  preserve corresponding-source/license obligations when redistributing.
- [QPSX](https://github.com/angree/sf2000-qpsx-playstation-emulator)
  `9b4e4581bcc4022b2e9084b50e1231130387d104`, GPL-family inherited notices.
- [FroggyKVM](https://github.com/Synaps33/FroggyKVM)
  `fceb877c686c86ea5f28ec61d82940f2d0b40411`; PSPKVM/Sun/FluidLite and included
  third-party notices also apply. The root LICENSE still names QPSX: it is not
  sufficient alone to describe the Java runtime's licensing.
- [DSperate SF3000 package](https://github.com/tzubertowski/TreeFrogUI_DSperate/releases/tag/v1.15.2-rc.1),
  retains upstream runtime libraries; requires a full corresponding-source audit
  before a new public release, as do the recovered MTP module/runtime classes.
- [MAME2000](https://github.com/libretro/mame2000-libretro)
  `760ba0269dcb3f6f19297c553562f81a8a73dc1c`: matching coroutine-era patch baseline,
  not the much newer re-entrant emulator rewrite. Original MAME noncommercial
  and component licenses apply.
- [HarfBuzz](https://github.com/harfbuzz/harfbuzz)
  `d2e24847f679922c3d77a4c48f9030489d61875d` (MIT-style);
  [SheenBidi](https://github.com/Tehreer/SheenBidi)
  `9c048a32d2131f67608e85ccccf9079cc46d6e42` (Apache 2.0);
  FreeType (FTL/GPL option), using the existing stock-compatible shared library.
- [DejaVu 2.37](https://github.com/dejavu-fonts/dejavu-fonts/releases/tag/version_2_37)
  (Bitstream Vera/DejaVu license); Nunito (SIL OFL); TreeFrogUnicode is an upstream
  WenQuanYi-derived fallback (GPL with font exception). License copies accompany
  the fonts and shaping module. Existing artwork/theme noncommercial terms remain.

Use `scripts/Build-R36SX.ps1` for the fork UI/patch assembly and the
`Build R36SX GNU runtimes` workflow on this integration branch for GNU apps.
Pinned app builds live in `scripts/build-upstream-apps.sh`, `build-unicode.sh`,
`build-mame-fix.sh` and `build-j2me.sh`. Do not overlay an upstream release ZIP
onto SwitchFrogUI or publish this private test payload as a completed audited
release. Main/source-build branches and public releases are unchanged.

Deployment uses an explicit runtime allowlist, backs up replaced files and
verifies SHA-256 hashes on TESTCARD and E:. Per-target backup manifests record
which paths were new versus replaced. Roll back replaced files from that
target's backup; remove only manifest-listed new runtime files if necessary.
