## SwitchFrogUI 1.4.1 - Menu idle power and consolidated full-card release

- Promotes the local menu-power update into source and both complete SD-card
  packages. Menu timing sleeps at 60 Hz rather than relying on framebuffer
  submissions; input, sound, debounce and timeout semantics are preserved.
- Reuses unchanged frames, with one visible keepalive per second and no
  framebuffer presents while blanked when the host supports frame duplication.
  Input, redraw and wake submit immediately. Other hosts retain full presents.
- Uses a supported dynamic CPU governor and hardware minimum only while the
  frontend is active; restores the previous governor/minimum before games/apps.
  Missing CPUFreq support is a no-op. No voltage, maximum-clock or thermal edits.
- Retains the 1.4.0 controls, save-state launching, keyboard fix, removed Game
  Switcher toggle, empty platform directories, and licensed open BIOS package.
- Static boot graphic now reads SwitchFrogUI Version 1.4.1. Fresh installs keep
  Ocean Depth Gradient. User card settings, saves and favourites are preserved.
- Video/emulator runtime code and suspend/wake helpers are unchanged. The
  regression test simulates 10 static-frame presents instead of 600 over ten
  seconds; this is not a measured battery-life or temperature improvement.
- Physical checks remain: boot, inputs/sounds, screen timeout/power wake,
  game/app handoff, save/load/resume and idle warmth. Some TreeFrogUI imports
  remain insufficiently tested; only v2.7 has been tested during development,
  and v2.6 remains experimental with no compatibility guarantee.
- Both ZIPs include complete boot files and apps, approved samples, empty ROM
  folders, and optional open firmware with licenses/source/provenance. No game
  ROMs, proprietary BIOS dumps, private media, saves or personal settings.

## SwitchFrogUI 1.4.0 - Controls, save-state launching and full SD-card releases

- 2026-09-27 package refresh: optional, licensed PS1/GBA/GB/GBC firmware
  alternatives plus BIOS-GUIDE.md, provenance, licenses and GBA BIOS source.
  Proprietary BIOS dumps remain excluded; existing BIOS selections are kept.
- Removed the Game Switcher toggle from Settings without changing existing
  Recent presentation preferences.
- Both full-card ZIPs preserve empty platform/emulator folders, with a
  `ROM-FOLDERS.md` guide explaining where to put games. No ROMs are included.
- Replaced the button-capture wizard with logical-button and physical-button
  lists. B can be reassigned without leaving the page. Conflicting assignments
  swap to prevent duplicates. This page uses fixed physical A/B and D-pad.
- Added Reset All to Defaults, recovery by holding physical Start + L1 + R1
  for about two seconds in SwitchFrogUI, repair of invalid old maps, and atomic
  mapping-file replacement.
- Game Details has a scrollable save-slot list: Up/Down selects a slot; A
  launches and loads it. PCSX4ALL records each game's disc ID on launch, so
  existing PS1 states become discoverable after opening the game once.
- Quick Resume loads the last successfully saved state on opening
  a game in PicoArch or PCSX4ALL. Explicit slot selection takes priority.
  Battery saves and memory cards are separate from emulator save states.
  Successful saves now write an atomic last-slot marker, which takes priority
  over dates because some R36SX clocks give all saves the same 1979 timestamp.
  Old untracked states fall back to dates; use explicit selection if tied.
- MENU/FN + L1 saves and MENU/FN + R1 loads the selected state slot in PicoArch
  and PCSX4ALL; SELECT also works as modifier. Actions fire once per hold.
  SELECT + START still opens the emulator menu. PicoArch fast-forward moves
  to MENU + R2 and screenshot to MENU + L2. Cores lacking serialization support
  cannot offer save states.
- On-screen keyboard rows fit the available panel width instead of extending
  beyond the 640x480 screen.
- Fresh installs use Ocean Depth Gradient. The static boot graphic shows
  SwitchFrogUI Version 1.4.0 instead of an H.OS version.
- Two complete card-root ZIPs target R36SX motherboard v2.6 and v2.7, including
  boot files, emulators and app runtimes, including Rockbox's hidden `.rockbox`.
  Extract directly to an empty FAT32 card; no separate stock copy is needed.
- No games, proprietary console BIOS dumps, saves, favourites, history, custom settings
  or custom media. Samples: SwitchFrogUI video/subtitles, stock H.OS
  `sample.mp4`, Mozart recording, three stock-card photos, World English Bible
  and JSDev API Showcase.

### Testing and compatibility

Some features and emulators are imported from TreeFrogUI and have not been
thoroughly tested. Andrija has only been able to test development builds on
R36SX v2.7; **v2.6 is experimental and is not guaranteed to work**. New 1.4.0
controls/save-state changes still need physical-device testing. The v2.6/v2.7
labels refer to motherboard revisions, not H.OS version numbers.

The v2.7 package retains the working H.OS 1.2 boot base. v2.6 uses the upstream
R36SX v2.6 minimal stock backup with SwitchFrogUI and its driver fallback.
Retained fixes include 1.3.7 hardware display-layer video sizing and directory
locks, the merged player/subtitle offset, file manager, photo viewer and
earlier emulator fixes.

## SwitchFrogUI 1.3.7 safe video zoom and directory locks

- Video Fit, Fill, Stretch and Original modes now use the hardware MAIN-layer
  `DIS_SET_ZOOM` ioctl with next-frame activation. They no longer call
  `hcplayer_set_display_rect()`, which could restart or stall the proprietary
  H.OS decoder several seconds after a size change.
- Added persistent directory access locks to every SwitchFrogUI browser. Rename
  one directory to the special name `locked` to register the lock without
  changing its real name. Attempting to enter it displays `Directory
  inaccessible.`; Up, Up, Down, Down, Left, Right, Left, Right unlocks it.
- Locking attempts to set the FAT Hidden attribute and unlocking clears it.
  This is a convenience/privacy feature, not encryption; visibility of FAT
  hidden entries on desktop Linux and macOS depends on their mount/file-manager
  settings.

## SwitchFrogUI 1.3.6 pause-menu resize synchronization

- Video sizing changes made in the pause menu are now collected without
  touching the proprietary display pipeline while the decoder is paused.
- Only the final selected Fit, Fill, Stretch or Original mode is applied, 250
  ms after playback has resumed. This avoids H.OS implicitly restarting the
  decoder behind the still-open pause menu and prevents the subsequent double-
  resume freeze.
- Added explicit pause, resume and deferred-resize trace points for reliable
  physical-device diagnosis without recurring SD-card writes.

## SwitchFrogUI 1.3.5 upstream-startup restoration

### 2026-08-30

- A one-minute Porco Rosso retest proved the external watchdog was armed but
  could not run once the proprietary decoder wedged, so recovery after the
  fault is not reliable on this single-core H.OS environment.
- Restored TreeVidPlay's proven startup ordering: no fb0 memory write and no
  `hcplayer_set_display_rect()` call before the first decoded frame.
- Defer the active-page black clear until video is confirmed running. Sizing
  modes remain in the pause menu and are applied only after first frame when
  the user explicitly changes one, avoiding unproven startup-side mutations.
- Removed the shell heartbeat supervisor after the device trace proved it
  could not be scheduled during the proprietary decoder fault. Playback once
  again uses the simple foreground handoff of the proven upstream player.

## SwitchFrogUI 1.3.4 video startup hotfix

### 2026-08-30

- Corrected the 1.3.3 black-background implementation after a Porco Rosso
  device trace reached READY but blocked before its first decoded frame.
- Clear only the active fb0 scanout page. The earlier implementation cleared
  all seven virtual SF3000 framebuffer pages and could disturb display pages
  retained by the hardware pipeline.
- Reduced external hard-block recovery from 18 to 12 seconds and synchronously
  record watchdog arming/timeouts in `log.txt`, making recovery behavior
  auditable even after a forced power-off.
- If a decoder process remains stuck after TERM and KILL, release the wrapper
  itself so zhijack is no longer held indefinitely waiting for the player.

## SwitchFrogUI 1.3.3 video freeze-recovery update

### 2026-08-30

- Added an external heartbeat supervisor around the proprietary H.OS decoder.
  If `libffplayer` blocks inside an open, playback or shutdown call for 18
  seconds, the stuck process is terminated and zhijack can restore FrogUI.
- Added an internal 15-second playback-progress watchdog for files which open
  and display a first frame but stop advancing their media timestamp.
- Added explicit lifecycle logging before decoder shutdown so a future device
  log distinguishes playback stalls from cleanup stalls.
- Clear the retired frontend framebuffer to opaque black before starting the
  video plane. Letterbox and pillarbox areas no longer show the file browser.
- The heartbeat is stored only in `/tmp` and updated at most twice per second,
  so it does not write to the SD card or burden smooth video playback.

## SwitchFrogUI 1.3.2 merged video-player update

### 2026-08-30

- Consolidated the temporary Videos/TreeVidPlay pair into one `Videos` Home
  card and one direct GNU-SDK executable.
- Retained TreeFrogUI v1.2.0_b's device-proven 256-byte zeroed initialization,
  audio-master/I2SO timing, decoder lifecycle, playlist and seek behavior.
- Ported Fit, Fill, Stretch and Original sizing, a six-row pause menu,
  exact-basename SRT/WebVTT subtitles and persistent 100 ms timing offsets.
- Kept subtitle parsing outside `libffplayer` because H.OS 1.2's external
  subtitle decoder is unstable. Subtitle-only playback redraws occur only when
  the active cue changes, protecting high-bitrate playback performance.
- Preserved playback-mode selection, previous/next video, 10/60-second seeks,
  persistent FN+L1+R1 rotation, startup watchdog and exact browser return.
- The official SF3000 GNU workflow compiled the merged MIPS32r2 executable
  successfully in run `33315346123`.

## SwitchFrogUI 1.3.1 TreeVidPlay update

### 2026-08-30

- Added a separate `TreeVidPlay` Home card backed by the exact official
  TreeFrogUI v1.2.0_b hardware video-player runtime and pinned matching source.
- Preserved the existing `Videos` player and its subtitle/pause/scaling
  features as an independent fallback; Files continues to open videos with it.
- TreeVidPlay gets a whole-card video-only browser, dedicated logging, and exact
  directory/file selection restoration after normal completion or manual exit.
- Added release-hash enforcement, staged-card/release-package integration,
  source provenance, license attribution, and build documentation.

Physical R36SX testing is required to confirm the upstream player's claimed
high-bitrate/1080p60 behavior on each source codec and profile.

## SwitchFrogUI 1.3.0 development update

This source update ports the eight compatible groups selected from newer
TreeFrogUI work while retaining SwitchFrogUI's Home layout and R36SX behavior.

### 2026-08-28 device hotfix

- Added a first-class `gbc` platform route so Game Boy and Game Boy Color can
  appear as separate Home cards while both continue to use Gambatte.
- Added repeatable English-only GB/GBC/GBA/SNES import, payload de-duplication,
  metadata generation, artwork matching and libretro cheat staging tools.
- Added PS1 media auditing guidance used to reject unsupported package files
  without discarding structurally valid PBP, BIN, IMG or ISO games.

- Restored the proven H.OS 1.2 public player-init profile after device logs
  isolated a crash inside `hcplayer_create()` to the newer guessed ABI block.
- Fixed valid top-down BMP screenshots (`640x-480`) being rejected by the
  Photo viewer before `stb_image` could normalize and decode them.
- Virtual keyboards now remain hidden when a text prompt opens and are shown
  or hidden explicitly with Select, with or without a USB keyboard attached.

- Rebased the H.OS video startup sequence on the stock-compatible player:
  vendor layer order, audio-master clocking, panel geometry, stable SDK init,
  startup watchdog, subtitles, pause controls, rotation and exact browser return.
- Added PicoArch runtime audio recovery. The pause menu flushes stale samples,
  resume starts from fresh core audio, and AUDDEC/I2SO failures are reinitialized
  on the owning audio thread instead of leaving games silent until reboot.
- Removed full-panel CPU scaling from forced-ratio and integer paths, added
  hardware-assisted envelopes, preserved portrait/vector aspect ratios, improved
  long core-option text, and repaired PCE/VICE input edge cases.
- Expanded Files with type/size metadata, recursive folder-size calculation,
  and explicit Keep both / Skip / Replace collision choices. Existing confirmed
  copy, cut, paste, recursive directory and multi-select operations remain.
- Corrected the Rockbox framebuffer colour conversion in the reproducible patch
  and let last-played cards use gameplay/save-state captures when available.
- Added Play Activity totals and two credited optional artwork packs from the
  newer upstream line: Art Book NextUI and Nao Black.
- Added visible version, source commit and build date in About, Hardware
  Information, the libretro core and the staged-card build information file.
- Added a guarded offline updater. It rejects unsafe archive paths and protected
  user files, validates SHA-256 before installation, backs up replaced files,
  uses same-directory atomic replacements, and never packages ROMs or saves.

The video player, FrogUI core and PicoArch sources compile successfully for the
MIPS32r2/H.OS target. Device testing remains required for hardware decoding,
vendor audio recovery and direct display scaling.

---

> [!IMPORTANT]
> v1.0.10_b gives the UI a **fresh look** (bigger bold font, custom wallpaper, a proper battery icon), makes **big ROM folders load fast**, reworks the **Settings menu**, adds an **aspect-ratio picker** and a PS1 **Hi-Res Fix**, keeps **volume control working in games**, and fixes **PS1 hi-res freezing/blacking out**, **positional PS1 buttons**, **automatic PS1 BIOS**, and the **lingering battery icon**.
> 
> 📋 **[Submit Anonymous Feedback (Google Forms)](https://docs.google.com/forms/d/e/1FAIpQLSfM-y2_UnERrjScqkSfkRSEfBPJ79rDwDo3GwuYWXxpkFTp4Q/viewform?usp=header)**

---

## What's New in v1.0.10_b

Now it's pretty, too.

- **🎨 New look.** Bigger, bolder text (the same clean font MinUI/NextUI use) with more breathing room, so the menus are easier to read on the little screen. The PCSX4ALL (PS1) menus now match too - same font, theme colours, and rounded selection, rendered crisply at full resolution. Same layout, less squinting.
- **🎨 Battery Colour Mode.** New Settings → Appearance toggle: instead of a fill bar, the battery shows a single colour dot - green (70-100%), blue (30-70%), red (0-30%). Minimal, at a glance.
- **🔋 A real battery icon.** The frontend and the in-game menu now show a proper battery indicator (with a charging bolt) in place of the stock one - reads the actual charge level, turns red when it's nearly time to plug in. It draining is still your problem.
- **🖼️ Custom wallpaper.** Settings → Appearance → **Wallpaper**: drop images in `frogui/wallpapers/` and pick one to use across every screen, instead of the per-system art. **Wallpaper Fit** offers Windows-style Fill / Fit / Stretch / Center / Tile. Make it yours; play nothing, beautifully.
- **⚡ Big ROM folders load fast.** Folders with thousands of games used to crawl when opening and scrolling. The listing is sorted properly now (not the old slow way) and cached between visits, so a folder you've seen before opens instantly. Add or remove a game and it refreshes on its own. There's a Folder Cache toggle in Settings if you ever want it off. More time to not decide what to play.
- **🗂️ Settings menu, reorganized.** Options are grouped under headers (Appearance, Library, Gameplay, System) and indented so you can actually find things. New toggles: **Hide Extensions** (drop the `.gb`/`.gba` clutter from names) and **Background Images** (turn off the per-system art for a plain background). Same settings, less squinting.
- **📐 Aspect ratio picker.** New single control in the in-game Video menu: Integer, Native (what the core wants), 4:3, 16:9, 3:2, 5:4, 8:7, 16:10, or Fill. Replaces the old Screen size toggle - one list, per game. Integer and Native stay exact and free; a forced ratio reshapes in a single pass only when you pick one. Now you can get the picture wrong in more precise ways. (PS1's non-square modes like 256-wide games are aspect-corrected too, no longer squished.)
- **🔊 Volume control survives games.** The frontend used to kill and restart the volume daemon to redraw its little on-screen icon, which quietly took your volume buttons with it. It's left alone now - the icon comes back on its own, and the buttons keep working. Turn it down; it won't help.
- **🩹 PS1 hi-res games stop freezing and blacking out.** Colin McRae Rally, Worms Armageddon and friends flip into a tall 480-line video mode that the display driver quietly chokes on - black screen or a frozen picture while the game plays on underneath. Those frames are scaled back down to something the driver can actually show now, on every device. The cars still understeer into the scenery, but you get to watch.
- **🎮 PS1 buttons now match their positions.** Cross (confirm) is the south button, Circle east, Square west, Triangle north - so what the game tells you to press lines up with where it is on the pad. Only applies to fresh setups; if you'd already remapped, your choice is kept. Press south to confirm the things you'll regret.
- **💿 PS1 BIOS just works now.** Drop any real BIOS (`scph*.bin`) into `cubegm/bios/` and PCSX4ALL finds it and switches HLE off on its own - no menu ritual, no exact filename. If you'd already set one by hand, it's left alone. One less thing to get wrong before the disappointment starts.
- **🔋 The battery icon stops haunting your games.** It used to linger over the screen after a game launched, reminding you the clock is running down on the battery and on everything else. It gets wiped now, over and over, so you don't have to think about it.

*Everything else already shipped in v1.0.8. The Nearest filter still messes up the menu sometimes. It is what it is.*

**Updating:** copy `cubegm/` and `frogui/` over your card, then copy your device's `install_first/<device>/` folder again. ROMs, saves, and settings are untouched.

---

*Overview, features, install guide, troubleshooting, and porting info live in the [README](README.md).*
