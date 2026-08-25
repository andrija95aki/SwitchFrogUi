# R36SX features and fixes

For a reader-friendly comparison against both stock H.OS 1.2 and the pinned
TreeFrogUI/FrogUI base, see [What SwitchFrogUI adds](ADDITIONS.md).

## Home and game library

- Console-style horizontal Home screen with three direct-launch last-played
  games, Recent, Favourites, non-empty platform cards, Music, Videos, Ebooks,
  Files, Terminal, Text Editor, JSDev, and Settings.
- Favourites uses a warm yellow identity card and Recent uses vivid purple;
  opening Recent clears the Home artwork instead of retaining a stale banner.
- Non-empty platforms are discovered correctly on the H.OS FAT32 `stat()` ABI.
- Platform collections use box-art grids with metadata-driven category tabs:
  `ALL` is first, followed by the available categories for that platform.
  L2/R2 changes tabs without moving or renaming a ROM. START switches to a
  condensed list and L1/R1 changes pages quickly.
- In browsers with more than five pages, Select+Start opens direct page entry
  (L2 remains the shortcut in utility lists without category tabs). Type a
  page number on an external keyboard or use the on-screen numeric keypad.
- Search across a platform collection, smaller antialiased titles, square
  antialiased cards, favourites stars, and save-data badges.
- Game details show the full title, artwork, category, short description, total
  play time, and save-state playtime records before launch when the optional
  sibling `.metadata.tsv` database is present.
- Home, grids, lists, settings, help pages, the core picker, and page buttons
  wrap from the first item to the last and back again.
- Existing artwork is found in `.res`, `images`, `Imgs`, `media`, and `boxart`
  layouts, including nested ROM folders.
- Ten credited TreeFrogUI 1.0.12 / Onion platform-icon packs are selectable in
  Settings. Pack logos are contained without stretching or cropping.
  `Existing artwork` remains the default cover-fill layout, and missing pack
  images fall back to the existing per-platform art and built-in controller.
- Optional friendly platform names expand short folder codes such as `snes`
  and `ps1` without renaming ROM directories or changing core detection.
- Returning from a platform, game, Recent, Favourites, utility, or Settings
  restores the originating Home card by identity instead of jumping to the
  first item. The position survives standalone-app/frontend process restarts
  and recent-game reordering.
- Thin seven-pixel artwork-colour halo around the selected Home card, alpha-
  simulated by blending into the actual rendered background. Dark artwork is
  brightened for the halo so it cannot become a black opaque shadow.

## Appearance and settings

- Sixty-two colour schemes: the original palette, bright R36SX additions, ten
  two-stop gradients, and twelve new four-colour gradients. Gradient rendering
  remains scanline-based and only runs when an event causes a redraw.
- Dedicated theme gallery with each theme name beside live background,
  selection, and accent-colour samples.
- Optional persistent solid background colour palette.
- Twelve bundled font choices spanning hyper-legible, modern, playful,
  monospace, and cyber styles, plus custom TTF/OTF discovery.
- Cog-wheel Settings artwork and sliders for font size (70-130%), brightness,
  timeout, and master volume.
- Configurable Off/Classic/Bright/Soft/Cyber interaction-sound packs. The three
  sampled packs use Kenney CC0 UI audio and fall back safely to generated tones.
- All UI animations removed for responsiveness. Every Files, Videos, Ebooks,
  Text Editor, and JSDev selector now shares one SD-root-bounded Back routine,
  so B consistently returns to the parent folder and then the originating Home
  card even when a path contains a trailing slash or cannot be reopened.
- Finishing a video or closing playback returns to the same Videos/Files
  directory with the launching filename still highlighted. The handoff is
  persisted by path and filename, so it also survives FrogUI's required
  standalone-process restart.
- Long settings, core, theme, information, diagnostics, help, and file lists
  include a compact position indicator. Saved settings, themes, button maps,
  core overrides, and favourite changes show a short confirmation.
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
- Live keyboard input tester inside Inputs & I/O Diagnostics. Press Y to open it,
  then use a connected USB keyboard to inspect the active Linux event device,
  held keys, modifier state, and recent key-down/key-up events. X clears the
  history and controller B always returns to the diagnostics page.
- Persistent editable USB-keyboard-to-gamepad mapping shared by FrogUI,
  PicoArch/libretro cores, standalone PCSX4ALL gameplay, and both emulator menu
  paths. Defaults: arrows=D-pad, Enter=A, Escape=B, Space=X, Left Shift=Y,
  Q/E=L1/R1, 1/3=L2/R2, Backspace=Select, and Right Shift=Start. Every one of
  the fourteen gamepad controls can be rebound in Settings and changes are
  reloaded without modifying the stock `joy_key` shared-memory writer.
- Text entry automatically opens a controller-operated virtual keyboard when
  no USB keyboard is detected. It covers game search, page entry, file and
  folder naming, Text Editor prompts and editing, and Terminal commands.
  Select shows or hides it at any time; L1 switches letters/symbols, R1 changes
  case, A types, Y deletes, and Start confirms or enters a new line. USB
  keyboard hot-plug is detected while the prompt remains open.
- Terminal Home card runs the card's real BusyBox `ash` shell in a persistent
  child process with a clean text-only, mixed-case Space Mono framebuffer view
  enlarged to 115% for the 640x480 panel.
  It starts in `/mnt/sdcard`, never triggers the automatic screen timeout,
  stores the latest 32 commands, recalls them with Up/Down, and closes cleanly
  with B/Escape.
- Text Editor Home card starts at `/mnt/sdcard` and browses nested folders. X
  creates a file with any extension, A opens it, Y renames it, Ctrl+S saves,
  F2 renames while editing, and B/Escape saves modified text before closing.
  Files are capped at 128 KiB to avoid loading a ROM or other large binary into
  memory on this constrained device. Standard keyboard editing includes
  Page Up/Down, Home/End and Ctrl+Home/End, Ctrl+word movement, Shift selection,
  Ctrl+A/C/X/V, eight-level Ctrl+Z/Y undo/redo, Ctrl+F and F3 find-next,
  Ctrl+N new, Ctrl+O browser, Ctrl+Backspace/Delete, Tab, and Shift+Tab.
- JSDev is a permanent Home card and a source-built libretro JavaScript
  runtime. Its browser starts at `/mnt/sdcard`, runs `.js` projects with A, creates a starter
  project with X, edits with Y, and renames with Select. The API provides
  graphics/text, 60 Hz animation, press/release and held gamepad input,
  synthesized sound, timers, logs, and isolated persistent JSON storage. A
  commented full-API showcase is included; see `docs/r36sx/JSDEV.md`.
- The redundant Mini Linux shortcut page was removed; it only duplicated the
  existing Terminal, Files, and System Information entries and did not provide
  a second distribution or desktop stack.
- Video browsing recognizes FLV plus a broad set of common and legacy
  containers and elementary streams: MP4/M4V, MKV, AVI/DivX/Xvid, MOV/QuickTime,
  MPEG, transport streams, VOB, WebM, F4V, 3GP/3G2, WMV/ASF, Ogg video,
  RealMedia, MXF, NUT, DV, AMV, Motion JPEG, HLS playlists, H.264/H.265/VC-1,
  AV1, and Y4M. Actual decoding remains limited to codecs implemented by the
  card's stock H.OS hardware `libffplayer`; an unsupported stream exits cleanly
  back to its selected file and records the reason in `video_player.log`.

## R36SX hardware fixes

- Persistent `R36SX Display glitch fix` for the left 110-pixel band with the
  final one-pixel vertical correction.
- The same optional correction is inherited by games, PCSX4ALL menus, Rockbox,
  and other standalone applications without double-applying it in FrogUI.
- Holding FN + L1 + R1 flips the final 640x480 output by 180 degrees; repeating
  the chord restores it. The state persists in `frogui/screen_rotation.cfg` and
  covers FrogUI, terminal, editor, Rockbox, supported emulators, and the native
  video player (including its controls and subtitles).
- Double-buffered panel presentation reduces PCSX4ALL menu text flicker.
- Short power press is bridged from H.OS `cubevol` to FrogUI, blanks/restores
  the display, and preserves the separate long-press shutdown path.
- Configurable 10/20/30/60-second UI screen timeout. Once blanked, the panel
  stays off indefinitely and wakes only for a real button or power-key event;
  the unreliable R36SX suspend-to-RAM timer is never entered.
  Terminal and active text-edit/name-entry screens are exempt from idle blanking.
- Perceptual master-volume curve gives useful gradual control across all 21
  volume positions instead of concentrating the change in the final levels.
- FrogUI leaves the H.OS `cubevol` service running when the display frontend is
  restored, preventing volume-down input from being reset between presses.
- Static/dirty-frame rendering, cached colour sampling and display maps, and
  faster sorting reduce unnecessary CPU work.
- The GameSwitcher retains the three most recently viewed artwork/screenshot
  images in a bounded cache, avoiding repeated SD reads and image decoding.

## Emulator fixes

- GBA output now drains in 800-frame chunks matching the device's real 48 kHz
  audio clock (the previous 735-frame/44.1 kHz timing caused periodic
  underruns). A three-frame prefill absorbs scheduler jitter, PicoArch reports
  real ring-buffer occupancy to gpSP, and adaptive frameskip starts only below
  30% with a one-frame interval. `scripts/Tune-GbaAudio.ps1` applies the stable
  profile to existing per-game configs while retaining gpSP DRC and leaving
  `.sav`/save-state files untouched.
- Start+Select opens the in-game menu without the stock parent menu forcing a
  return to Home.
- PS1 video scaling now changes the actual presented geometry on the R36SX
  instead of routing two labels to the same 4:3 full-panel output. Available
  modes are 4:3 Fill, Raw Pixel Fit, Integer, Native 1x, Overscan 110%, and
  16:9 Letterbox, and Stretch; changes are visible immediately and persist
  when saved. Accurate GPU blending is enabled by default and live GPU setting
  changes reach the active renderer. Exit first returns the vendor display
  engine to a stable frame geometry to avoid freezing the resumed UI.
- The old translated-key PCSX GPU screen is replaced by a native
  `GPU Accuracy / Shadows` page. Lighting, Fast Lighting, Blending, and
  Dithering change the live UNAI renderer and can be saved globally or per
  game. Accurate defaults use lighting and blending with Fast Lighting off;
  legacy version-1 configuration files are now loaded instead of discarded.
- A native `Performance / FPS` page adds Accurate, Balanced, Fast, and Maximum
  FPS profiles plus independent Auto/Off/1-3 frame skip, CPU speedhack,
  Fast Lighting, Pixel Skip, Interlace, and FPS-display controls. Accurate is
  still the default; aggressive settings are opt-in and can be saved per game.
  Every profile keeps lighting and blending enabled to protect shadows and
  semi-transparent effects.
- If no user-supplied real BIOS is available, PCSX4ALL reports empty memory-card
  slots instead of advertising HLE card operations that can hang Spyro and
  other games at `Accessing memory card`. Save states remain available. A legal
  512 KiB BIOS in `cubegm/bios` automatically restores both persistent cards.
- PCSX explicitly signals panel-fill to both the proprietary driver and the
  optional R36SX display-fix shim. Stretch therefore occupies all 640x480
  pixels instead of being re-letterboxed by the correction layer.
- Checksum-guarded PCSX4ALL patch maps Start+Select to its native menu while
  retaining Select+L1.
- Duplicate PS1 history/favourite routes are collapsed by ROM path so the slow
  alternate core is not shown as a second copy of the same game.
- Save-state markers and playtime metadata survive restarts.
- Separate optional `doom`, `heretic`, and `hexen` folders use the current
  PrBoom core's automatic IWAD detection. No copyrighted IWAD game data is
  bundled; users place their legally obtained `.wad` files in those folders.
- Additional folder routes expose FBNeo, MAME 2003-Plus, Beetle Lynx, Snes9x
  2010, Vectrex, Odyssey 2, and Videopac. The previously absent official
  `vecx` and `o2em` core binaries are now included in source-build staging.
  Vectrex needs no BIOS; Odyssey 2/Videopac requires a legally obtained
  `o2rom.bin` in `cubegm/bios`, which is not bundled.

## Music, files, and video

- Files now has a controller-friendly X popup for Copy, Cut, Paste, Rename,
  Delete, New Folder, and Clear Clipboard. It is drawn over a dimmed view of the
  current directory like a desktop context menu. Y marks/unmarks items and
  Select marks/clears the current directory, so copy, cut, and delete can act
  on multiple files and complete directory trees. Every mutation requires an
  explicit A/Yes confirmation; B always cancels. Name collisions receive a
  `- Copy` suffix, and a compact COPY/CUT label shows the active batch.
- Whole-card file browser with audio dispatch to Rockbox, video dispatch to the
  hardware player, an internal Photo viewer, and direct Text Editor dispatch
  for common text, Markdown, log, config, source-code, subtitle, and playlist
  extensions. Photos support PNG, JPEG, BMP, GIF, TGA, PSD, PNM/PPM/PGM, and
  PIC, preserve transparent artwork against a checkerboard, wrap through the
  current folder with Left/Right, rotate with L1/R1, zoom to 12×, and pan with
  the D-pad while zoomed; B returns to the exact source folder.
- Entering a folder records its highlighted row and scroll offset. Pressing B
  restores the folder itself at the same position in its parent instead of
  resetting the browser to the first row; the behavior is shared by Files,
  Videos, Ebooks, Text Editor, and JSDev selectors.
- Permanent Ebooks Home card opens a clean browser at `/mnt/sdcard` and
  shows EPUB, MOBI, PDF, FB2, CBZ, and XPS documents. It launches the official
  TreeFrogUI MuPDF reader, supports custom fonts and per-book progress, and
  does not mix documents into game recents.
- The build and public overlay include three public-domain English EPUB tests
  under `Ebooks/SwitchFrogUI Samples`: World English Bible (WEB), Rodwell's
  translation of the Qur'an, and the 1917 JPS Tanakh. Sources and hashes ship
  beside the files.
- Rockbox opens at the card root, uses intuitive A-confirm/B-back mapping,
  follows the OS master volume, and inherits the display correction.
- Rockbox defaults to the OneBit VFD Winamp-style skin; its Shortcuts menu
  includes the bundled FFT spectrum visualizer.
- Game Details exposes an explicit Emulator action that persists a per-game
  core override; the same selector is available from game grids with Select.
- PCSX4ALL's in-game menu exposes save-state slots 1–10 instead of hardcoding
  every save/load action to slot 1.
- Dedicated Videos browser that navigates real folders without retriggering its
  Home action.
- The hardware player blanks FrogUI's retained framebuffer before playback so
  the decoder's main video plane is visible, then draws controls through a
  single-copy off-screen overlay to prevent progress-bar flicker.
- Local-file playback keeps the validated pre-tuning H.OS quick-mode timing and
  original controls/subtitle overlay schedule. Video is the presentation-clock
  master, matching HiChip's simple SF2000 hardware-video path; this prevents
  the clean audio clock from repeatedly correcting 23.976 fps H.264 movies with
  B-frame composition timestamps. Experimental packet buffering, custom
  late-frame thresholds, RAM-only logging, and OSD blanking remain disabled.
- Hardware-decoded video playback computes layout against the R36SX panel's
  real 640x480 shape, then maps it to the stock projector decoder's normalized
  1920x1080 rectangle ABI. This prevents playback being trapped in a tiny
  hardware-plane square. Fit, Fill, Stretch, and Original modes are available
  from the pause menu.
- Playback controls stay deliberately small: A or Start opens the pause menu,
  Left/Right seeks 10 seconds, and B exits. Volume remains under the OS master
  controls; the player does not override it or expose audio-track selection.
- Crash-safe local SRT and WebVTT parsing automatically loads at most one
  sidecar whose complete basename exactly matches the video. This avoids the
  H.OS 1.2 external-subtitle decoder ABI that crashes the vendor playback
  thread.
- Subtitle enable/disable and per-video timing offsets in exact 100 ms steps
  are located in the pause menu; timing offsets persist across reboots.
- The static SwitchFrogUI boot screen identifies the installed base as H.OS 1.2.
- Standalone media launching uses a clean boot-loop handoff: PicoArch exits
  first, allowing the kernel to close its framebuffer, HCGE, `/dev/dis`, and
  audio resources before a fresh application process starts. The video path
  explicitly clears the emulator-only display-fix preload and marker, so the
  FFmpeg/H.OS hardware player always runs without that correction. Its native
  rotation path still honors the shared FN + L1 + R1 orientation state.
- A tiny launcher built with the official SF3000 GNU SDK starts without a
  `DT_NEEDED` dependency on the vendor media stack, then loads the Zig-built
  player module and stock H.OS `libffplayer.so` API with `dlopen`/`dlsym`.
  This avoids the H.OS pre-`main()` SIGFPE seen with Zig/LLD executables while
  retaining the stock hardware decoder, direct-file launch, controls and
  subtitles.
- File and video list browsers use a clean theme background instead of carrying
  over the artwork banner from the previously selected Home card.
