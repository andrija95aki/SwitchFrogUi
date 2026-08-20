# R36SX features and fixes

## Home and game library

- Console-style horizontal Home screen with three direct-launch last-played
  games, Recent, Favourites, non-empty platform cards, Music, Videos, Ebooks,
  Files, Terminal, Text Editor, and Settings.
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
- All UI animations removed for responsiveness; B consistently returns to the
  previous menu.
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
- Terminal Home card runs the card's real BusyBox `ash` shell in a persistent
  child process with a clean text-only, mixed-case Space Mono framebuffer view.
  It starts in `/mnt/sdcard`, never triggers the automatic screen timeout,
  stores the latest 32 commands, recalls them with Up/Down, and closes cleanly
  with B/Escape.
- Text Editor Home card starts in `roms/Ebook` and browses nested folders. X
  creates a file with any extension, A opens it, Y renames it, Ctrl+S saves,
  F2 renames while editing, and B/Escape saves modified text before closing.
  Files are capped at 128 KiB to avoid loading a ROM or other large binary into
  memory on this constrained device.
- The redundant Mini Linux shortcut page was removed; it only duplicated the
  existing Terminal, Files, and System Information entries and did not provide
  a second distribution or desktop stack.

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

- Start+Select opens the in-game menu without the stock parent menu forcing a
  return to Home.
- PS1 video scaling now changes the actual presented geometry on the R36SX
  instead of routing two labels to the same 4:3 full-panel output. Available
  modes are 4:3 Fill, Raw Pixel Fit, Integer, Native 1x, Overscan 110%, and
  16:9 Letterbox, and Stretch; changes are visible immediately and persist
  when saved. Accurate GPU blending is enabled by default and live GPU setting
  changes reach the active renderer. Exit first returns the vendor display
  engine to a stable frame geometry to avoid freezing the resumed UI.
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

- Whole-card file browser with audio dispatch to Rockbox and video dispatch to
  the hardware player.
- Permanent Ebooks Home card opens a clean browser rooted at `roms/Ebook` and
  shows EPUB, MOBI, PDF, FB2, CBZ, and XPS documents. It launches the official
  TreeFrogUI MuPDF reader, supports custom fonts and per-book progress, and
  does not mix documents into game recents.
- Rockbox opens at the card root, uses intuitive A-confirm/B-back mapping,
  follows the OS master volume, and inherits the display correction.
- Dedicated Videos browser that navigates real folders without retriggering its
  Home action.
- The hardware player blanks FrogUI's retained framebuffer before playback so
  the decoder's main video plane is visible, then draws controls through a
  single-copy off-screen overlay to prevent progress-bar flicker.
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
  FFmpeg/H.OS hardware player always runs without that correction.
- A tiny launcher built with the official SF3000 GNU SDK starts without a
  `DT_NEEDED` dependency on the vendor media stack, then loads the Zig-built
  player module and stock H.OS `libffplayer.so` API with `dlopen`/`dlsym`.
  This avoids the H.OS pre-`main()` SIGFPE seen with Zig/LLD executables while
  retaining the stock hardware decoder, direct-file launch, controls and
  subtitles.
- File and video list browsers use a clean theme background instead of carrying
  over the artwork banner from the previously selected Home card.
