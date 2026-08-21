# What SwitchFrogUI adds

This page separates the complete R36SX feature set into two useful comparisons:

1. everything the SwitchFrogUI card adds over the stock R36SX H.OS 1.2 experience;
2. the work this branch adds over the pinned upstream TreeFrogUI/FrogUI base.

SwitchFrogUI remains a TreeFrogUI fork. Features brought forward from newer
upstream releases are credited as integrations, while fork-specific work is
identified as SwitchFrogUI work. The downloadable overlay contains no commercial
ROMs, console BIOS files, personal saves, or play history.

---

## Added over stock H.OS 1.2

### Home screen and game library

- Switch-style horizontal Home screen designed for the 640x480 R36SX display.
- Three large, direct-launch cards for the most recently played games.
- Dedicated Recent and Favourites collections.
- Automatic platform cards for every detected platform containing at least one ROM.
- Platform artwork contained inside cards instead of being stretched or cropped.
- Box-art game grids with square, antialiased cards.
- Condensed 12-game text-list mode for large libraries.
- L1/R1 fast page navigation and wrap-around navigation in cards, grids, and lists.
- Search inside platform game collections.
- Favourite stars and save-data badges on game cards.
- Game detail panel with full title, artwork, total play time, and save-state play time.
- ROM discovery across nested folders and H.OS FAT32 directory metadata quirks.
- Artwork discovery in `.res`, `images`, `Imgs`, `media`, and `boxart` layouts.
- Optional friendly platform names without renaming ROM directories.
- Return to the exact originating Home card after leaving a game or application.
- Selected-card artwork-colour halo that remains visible with dark artwork.

### Appearance and interaction

- 62 colour themes, including bright palettes, two-stop gradients, and four-colour gradients.
- Dedicated visual theme gallery with live colour samples.
- Optional persistent solid background colours.
- 12 bundled readable, playful, modern, monospace, and cyber-style fonts.
- Discovery of user-supplied TTF and OTF fonts.
- Adjustable UI font size from 70% to 130%.
- Ten optional credited platform icon packs.
- Cog-wheel Settings artwork and modern slider-based numerical settings.
- Configurable Off, Classic, Bright, Soft, and Cyber UI sound packs.
- Static, animation-free rendering for better responsiveness on the MIPS SoC.
- Consistent B/Escape back behavior and first-to-last wrap-around navigation.
- Scroll-position indicators and saved-setting confirmation messages.
- Static SwitchFrogUI boot branding that identifies the H.OS 1.2 base.

### Music, video, books, and files

- Rockbox Music card with whole-card browsing and OS master-volume integration.
- Corrected Rockbox gamepad mapping with intuitive A-confirm and B-back behavior.
- Hardware-decoded Videos application using the stock H.OS media stack safely.
- Video Fit, Fill, Stretch, and Original scaling modes.
- Simple pause menu, 10-second seeking, pause/resume, and clean exit controls.
- Automatic same-basename SRT and WebVTT subtitle loading.
- Subtitle enable/disable and persistent timing adjustment in exact 100 ms steps.
- Whole-card Files browser rooted at `/mnt/sdcard`.
- Automatic dispatch of audio files to Rockbox and video files to the video player.
- Direct internal photo viewing for PNG, JPEG, BMP, GIF, TGA, PSD, PNM-family,
  and PIC files, with panel-fit rendering, transparency, L1/R1 rotation, and B-back.
- Automatic dispatch of text, Markdown, logs, configuration, source, subtitle,
  cue, and playlist files to Text Editor.
- Ebooks application supporting EPUB, MOBI, PDF, FB2, CBZ, and XPS.
- Three public-domain English EPUB test books with source and hash records.
- Every application file picker begins at the SD-card root and uses the same
  bounded parent-folder/return-to-Home behavior for B.
- File/video lists clear stale game artwork instead of retaining a residual banner.

### Productivity and development tools

- Real BusyBox `ash` Terminal card starting at `/mnt/sdcard`.
- Larger text-only Space Mono terminal display with persistent 32-command history.
- Up/Down terminal command recall and no automatic timeout during terminal use.
- Text Editor that creates, opens, edits, renames, and saves arbitrary filenames.
- Standard keyboard navigation, selection, clipboard, undo/redo, search, indentation,
  word movement, Page Up/Down, Home/End, and save/open/new shortcuts.
- 128 KiB editor safety cap to avoid accidentally loading ROMs as text.
- JSDev offline JavaScript game-development runtime.
- JSDev 640x480 RGB565 graphics, antialiased text, animation callbacks, gamepad
  events, held-button input, timers, logging, four synthesized waveforms, and
  isolated persistent JSON storage.
- Editable JSDev starter project and bundled full-API showcase.

### Hardware, input, and system tools

- Persistent fix for the R36SX left 110-pixel display-band manufacturing fault.
- Display correction inherited by FrogUI, games, emulator menus, and Rockbox.
- Persistent FN + L1 + R1 180-degree screen flip across the UI, supported apps,
  emulators, video, controls, and subtitles.
- Short power press turns off/wakes the display; long press remains safe shutdown.
- Configurable 10/20/30/60-second UI screen timeout and two-minute sleep request.
- Safe framebuffer reconstruction after wake instead of a black resume screen.
- Perceptual 21-step master-volume curve with useful gradual changes.
- External USB-keyboard control of the UI and games.
- Editable mapping for all 14 gamepad controls, shared with FrogUI, libretro,
  PCSX4ALL gameplay, and both emulator menu paths.
- Live keyboard input tester showing device, held keys, modifiers, and key events.
- Hardware Information page for board, model, SoC, CPU, memory, display, kernel,
  serial availability, firmware, and SD-card capacity.
- Live I/O diagnostics for both SD/MMC slots, USB host/OTG, mass storage,
  keyboards, mice, gamepads, input nodes, audio/headphone output, framebuffers,
  serial/HID, and network interfaces.
- Shareable diagnostics report saved to `frogui/io_diagnostics.txt`.

### Emulation and stability

- Around 75 emulator cores compared with the stock firmware's small core set,
  covering consoles, handhelds, arcade systems, home computers, and game engines.
- Configurable per-core palettes, display behavior, input, performance, and
  system-specific options.
- In-game save states, Quick Resume, and optional automatic save/load behavior.
- Optional GameSwitcher with cached box art or exit screenshots.
- Optional start-directly-in-Recent behavior.
- Start+Select reliably opens the in-game menu instead of returning to Home.
- PCSX4ALL Start+Select patch while retaining Select+L1 compatibility.
- Correct PS1 live scaling with 4:3 Fill, Raw Pixel Fit, Integer, Native 1x,
  Overscan 110%, 16:9 Letterbox, and true full-screen Stretch.
- Correct PS1 transparency/blending defaults and live renderer updates.
- Native PCSX GPU Accuracy / Shadows menu with persistent live Lighting, Fast
  Lighting, Blending, and Dithering controls; legacy v1 configs load correctly.
- BIOS-less PCSX safe mode avoids HLE memory-card detection hangs while keeping
  save states; user-supplied real BIOS files restore both persistent cards.
- Stable display geometry restoration when leaving PCSX4ALL.
- Reduced PCSX4ALL menu flicker through double-buffered final presentation.
- Duplicate PS1 history/favourite routes collapsed by ROM path.
- Persistent favourite, play-time, save-state, and last-played metadata.
- Separate Doom, Heretic, and Hexen library routes using PrBoom.
- Additional FBNeo, MAME 2003-Plus, Beetle Lynx, Snes9x 2010, Vectrex,
  Odyssey 2, and Videopac routes.
- Included redistributable `vecx` and `o2em` cores; no copyrighted BIOS or IWAD data.
- Static dirty-frame rendering, cached artwork colour sampling, cached display maps,
  faster sorting, and bounded GameSwitcher image caching.
- Clean process handoff before standalone media launches to release display/audio resources.
- Tiny GNU-SDK video launcher plus dynamically loaded player module to avoid the
  H.OS loader crash encountered with a direct vendor-library dependency.

### Packaging and documentation

- Reproducible R36SX source build and patch preparation scripts.
- Copy-and-boot `card-files` staging for a legitimate stock H.OS 1.2 card.
- ROM-free public overlay with allow-list packaging and credential/content audits.
- Checksums, rollback binaries, install/build guides, feature notes, core sources,
  third-party notices, JSDev guide, and annotated hardware screenshots.

---

## Added over the pinned TreeFrogUI/FrogUI base

### SwitchFrogUI interface

- Replaced the text-forward launcher presentation with the horizontal console Home.
- Added three direct last-played game cards, permanent utility cards, and automatic
  non-empty platform cards in a deliberate Home ordering.
- Added box-art grids, condensed lists, collection search, page jumps, detail panels,
  favourite/save badges, full-name display, and save-state play-time records.
- Added wrap-around navigation throughout Home, settings, grids, lists, help pages,
  core selection, and pagination.
- Added persistent Home-card restoration by identity across frontend restarts.
- Added the selected-card artwork-colour halo and contained platform logo layout.
- Removed UI animations and residual list artwork for performance and clarity.
- Added modern settings rows, sliders, cog-wheel artwork, theme gallery, background
  selection, 12 bundled fonts, font sizing, extra gradients, and sound packs.

### New applications

- Added Rockbox as the Music experience with corrected controls and OS volume behavior.
- Added the H.OS hardware-decoded Videos application and its pause/scaling/subtitle UI.
- Added the whole-card Files application with media and textual-file dispatch.
- Added the internal Photo viewer and image-file dispatch from Files.
- Added the Terminal and full keyboard-driven Text Editor.
- Added the source-built Duktape-powered JSDev runtime and showcase project.
- Added a permanent Ebooks card, root-based picker, and public-domain samples.
- Removed the experimental Mini Linux shortcut because it duplicated existing tools.

### R36SX-specific engineering

- Added the optional left-band display correction and applied it beyond the frontend.
- Added persistent global 180-degree rotation through FN + L1 + R1.
- Repaired short-power display blanking, delayed sleep, and black-screen resume.
- Added the perceptual volume curve and preserved the stock volume service correctly.
- Added device model/system information and live hardware/I/O diagnostics.
- Added editable USB-keyboard-to-gamepad translation and the keyboard event tester.
- Added H.OS FAT32 ROM/platform discovery compatibility and R36SX display fast paths.

### Emulator and media fixes

- Reworked PS1 menu input, scaling, transparency defaults, exit stability, duplicate
  routes, and full-panel Stretch behavior.
- Reduced emulator-menu flicker while preserving readable PicoArch menu text.
- Added Doom-family and extra emulator routes plus the missing redistributable cores.
- Added safe standalone-app process handoff and the H.OS-compatible video bootstrap.
- Added crash-safe local subtitle parsing and persistent per-video subtitle offsets.

### Compatible newer-upstream integrations

The following were integrated from newer TreeFrogUI 1.0.12 work with minimal
changes and retain their upstream credit:

- ten optional platform icon packs;
- optional friendly platform names;
- contained/non-stretched icon-pack layout;
- scroll indicators and saved-action confirmations;
- GameSwitcher screenshot caching;
- Home-selection restoration support; and
- the `cubevol` volume-key fix.

### Build, release, and project work

- Added the R36SX v2.7/H.OS 1.2 build branch, device-specific patches, and
  reproducible cross-build automation.
- Added ROM-safe release assembly, audit rules, hashes, rollback policy, and
  plug-and-play dual-target deployment workflow.
- Added SwitchFrogUI branding while preserving TreeFrogUI/FrogUI attribution,
  licenses, original developer credit, and upstream source links.
- Added R36SX-specific installation, build, feature, JSDev, screenshot,
  third-party, and hardware-debugging documentation.

For implementation-level notes, see [R36SX features and fixes](FEATURES.md),
[build instructions](BUILDING.md), and the [third-party notices](THIRD_PARTY_NOTICES.md).
