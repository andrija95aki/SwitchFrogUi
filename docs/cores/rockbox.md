# Music player (Rockbox)

← [back to README](../../README.md)

`rockbox` runs the full [Rockbox](https://www.rockbox.org/) jukebox as a standalone app (currently **R36SX**). It plays MP3, FLAC, OGG Vorbis, and the rest of Rockbox's codecs, with themes, fonts, playlists, and a proper now-playing screen.

The Music card opens Rockbox at `/mnt/sdcard`, so music can live anywhere on
the card. It renders at native 320×240 and the driver scales it to fill the
panel, so standard Rockbox themes look correct.

**Bundled themes** (apply via **Settings → Theme Settings → Browse Theme Files**): **OneBit VFD**, **iVideo**, **crowPod**, **CrazyBitMono**, **Snappy**, **SNAZZPKT**, plus stock **cabbiev2**.

**Default skin:** **OneBit VFD**, a compact Winamp-style dark player with VFD
typography and live stereo peak meters. **Spectrum Visualizer** in Rockbox's
Shortcuts menu launches the bundled FFT visualizer while audio is playing.

**More themes:** grab any **iPod Video (320×240)** theme from **[themes.rockbox.org](https://themes.rockbox.org/index.php?target=ipodvideo)** (our screen matches that target exactly). Unzip the theme's `.rockbox/` contents into `roms/rockbox/.rockbox/` - the standard Rockbox folder (so `themes/`, `wps/`, `fonts/`, `icons/` land in `roms/rockbox/.rockbox/themes/` etc.) - then apply it via **Settings → Theme Settings**.

**Controls (now-playing screen):**
- **A** - play / pause (**hold** to stop)
- **LEFT / RIGHT** - previous / next track (**hold** to seek within a track)
- **UP / DOWN** - volume
- **B** - back to the file browser (keeps playing)
- **START** - main menu (**hold** for the context menu)
- **SELECT + START** - quit back to TreeFrogUI

**Controls (menus and file browser):**
- **UP / DOWN** - move through items
- **A** - confirm / open
- **B** - return to the previous screen
- **LEFT / RIGHT** - decrease / increase a setting

On R36SX, Rockbox also follows TreeFrogUI's persistent **R36SX Display glitch
fix** setting. Exit and reopen Rockbox after changing the setting.
