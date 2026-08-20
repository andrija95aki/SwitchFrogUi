# R36SX features and fixes

## Home and game library

- Console-style horizontal Home screen with three direct-launch last-played
  games, Recent, Favourites, non-empty platform cards, Music, Videos, Files,
  and Settings.
- Non-empty platforms are discovered correctly on the H.OS FAT32 `stat()` ABI.
- Platform collections use box-art grids; START switches to a condensed
  12-title list and L1/R1 changes pages quickly.
- Search across a platform collection, smaller antialiased titles, square
  antialiased cards, favourites stars, and save-data badges.
- Game details show the full title, artwork, total play time, and save-state
  playtime records before launch.
- Home, grids, lists, settings, help pages, the core picker, and page buttons
  wrap from the first item to the last and back again.
- Existing artwork is found in `.res`, `images`, `Imgs`, `media`, and `boxart`
  layouts, including nested ROM folders.
- Static 15-pixel artwork-colour halo around the selected Home card, rendered
  below neighbouring cards with no continuous animation cost.

## Appearance and settings

- Fifty colour schemes: the original palette, ten bright R36SX additions, and
  ten low-cost vertical-gradient themes.
- Dedicated theme gallery with each theme name beside live background,
  selection, and accent-colour samples.
- Optional persistent solid background colour palette.
- Twelve bundled font choices spanning hyper-legible, modern, playful,
  monospace, and cyber styles, plus custom TTF/OTF discovery.
- Cog-wheel Settings artwork and sliders for font size (70-130%), brightness,
  timeout, and master volume.
- Configurable Off/Classic/Bright/Soft/Cyber interaction-sound packs. The three
  sampled packs use Kenney CC0 UI audio and fall back safely to generated tones.
- All UI animations removed for responsiveness; B consistently returns to the
  previous menu.
- In-app Controls & Shortcuts reference.
- Scrollable About & Contributions page crediting the SwitchFrogUI fork,
  original TreeFrogUI/FrogUI developers, open-source resources, and license
  families used by the runtime and build process.
- Hardware Information page with detected model, board, firmware, SoC, CPU,
  memory, display, kernel, serial availability, and SD-card capacity.
- Live I/O Diagnostics page for both SD/MMC interfaces, USB host/OTG buses and
  connected devices, USB mass storage, keyboards, mice, gamepads, Linux input
  event nodes, audio/PCM and headphone output, framebuffers, serial/HID and
  network interfaces. Insert a device or second card and press A/X to re-probe;
  the complete shareable result is saved as `frogui/io_diagnostics.txt`.

## R36SX hardware fixes

- Persistent `R36SX Display glitch fix` for the left 110-pixel band with the
  final one-pixel vertical correction.
- The same optional correction is inherited by games, PCSX4ALL menus, Rockbox,
  and other standalone applications without double-applying it in FrogUI.
- Double-buffered panel presentation reduces PCSX4ALL menu text flicker.
- Short power press is bridged from H.OS `cubevol` to FrogUI, blanks/restores
  the display, and preserves the separate long-press shutdown path.
- Configurable 10/20/30/60-second UI screen timeout; two minutes of blank time
  can request sleep and the frontend rebuilds the display after resume.
- Perceptual master-volume curve gives useful gradual control across all 21
  volume positions instead of concentrating the change in the final levels.
- Static/dirty-frame rendering, cached colour sampling and display maps, and
  faster sorting reduce unnecessary CPU work.

## Emulator fixes

- Start+Select opens the in-game menu without the stock parent menu forcing a
  return to Home.
- PS1 video scaling now changes the actual presented geometry on the R36SX
  instead of routing two labels to the same 4:3 full-panel output. Available
  modes are 4:3 Fill, Raw Pixel Fit, Integer, Native 1x, Overscan 110%, and
  16:9 Letterbox; changes are visible immediately and persist when saved.
- Checksum-guarded PCSX4ALL patch maps Start+Select to its native menu while
  retaining Select+L1.
- Duplicate PS1 history/favourite routes are collapsed by ROM path so the slow
  alternate core is not shown as a second copy of the same game.
- Save-state markers and playtime metadata survive restarts.

## Music, files, and video

- Whole-card file browser with audio dispatch to Rockbox and video dispatch to
  the hardware player.
- Rockbox opens at the card root, uses intuitive A-confirm/B-back mapping,
  follows the OS master volume, and inherits the display correction.
- Dedicated Videos browser that navigates real folders without retriggering its
  Home action.
- Hardware-decoded video playback with pause, 10/60-second seek, 1x/2x/4x/8x
  speed, 0/90/180/270-degree rotation, Fit/Fill/Stretch/Original scaling,
  volume, audio-track and subtitle-track selection.
- Sidecar SRT/ASS/SSA/SUB/IDX/VTT/SMI/SAMI subtitle discovery, including
  language suffixes.
- Per-video subtitle timing offsets in exact 100 ms steps, persisted across
  reboots.
- Standalone media launching uses a clean boot-loop handoff: PicoArch exits
  first, allowing the kernel to close its framebuffer, HCGE, `/dev/dis`, and
  audio resources before a fresh application process starts. The video path
  explicitly clears the emulator-only display-fix preload and marker, so the
  FFmpeg/H.OS hardware player always runs without that correction.
- A tiny launcher built with the official SF3000 GNU SDK starts without a
  `DT_NEEDED` dependency on the vendor media stack, then loads the Zig-built
  player module and stock H.OS `libffplayer.so` API with `dlopen`/`dlsym`.
  This avoids the H.OS pre-`main()` SIGFPE seen with Zig/LLD executables while
  retaining the stock hardware decoder, direct-file launch, controls and
  subtitles.
- File and video list browsers use a clean theme background instead of carrying
  over the artwork banner from the previously selected Home card.
