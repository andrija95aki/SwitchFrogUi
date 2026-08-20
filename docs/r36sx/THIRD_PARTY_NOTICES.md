# Third-party notices

TreeFrogUI is a compilation. Every component retains its upstream license.
Review the repository [license](../../LICENSE.md) and the license files shipped
with release components.

## Upstream projects

- TreeFrogUI: https://github.com/tzubertowski/treefrog-ui
- FrogUI: https://github.com/tzubertowski/FrogUI
- TreeFrogUI PicoArch: https://github.com/tzubertowski/TreeFrogUI_picoarch
- TreeFrogUI PCSX4ALL: https://github.com/tzubertowski/TreeFrogUI_pcsx4all
- Rockbox: https://www.rockbox.org/

## Project contributions

- Original TreeFrogUI developer and maintainer: Tomasz Zubertowski
  (`tzubertowski`, also known as Proszty).
- Original FrogUI contributors: Tomasz Zubertowski, Desoxyn, and Q_ta.
- SwitchFrogUI R36SX fork direction, device testing, feature design, and
  publishing: Andrija (`@andrija95aki`).
- Development assistance: OpenAI Codex.
- Hardware observations, testing, and issue reports: the R36SX and TreeFrogUI
  communities.

The FrogUI-derived frontend is CC BY-NC-SA 4.0. PicoArch, libretro cores,
Rockbox, PCSX4ALL, SDL, libpng, zlib, and other components retain their own
licenses. Release packages include the corresponding notices and source links.

## Development and runtime resources

- libretro and its emulator-core projects: core-specific GPL, LGPL, BSD, MIT,
  MAME, and other upstream licenses; see `CORE-SOURCES.md`.
- PicoArch by neonloop and the TreeFrogUI port: BSD 3-Clause for the wrapper;
  vendored libpicofe portions retain GPL v2+, LGPL v2.1+, or MAME terms.
- TreeFrogUI PCSX4ALL port: its upstream open-source notices apply.
- DuckStation CHTDB: optional source for locally generated PS1 GameShark
  lists. Database scripts are MIT; individual codes remain owned by their
  original authors. No cheat database is bundled in the public OS overlay.
- Rockbox: GNU GPL v2 or later.
- SDL 1.2: GNU LGPL v2.1.
- FFmpeg/libffplayer and HCRTOS headers/APIs: their respective upstream and
  component licenses apply; proprietary H.OS firmware is not redistributed.
- libpng and zlib: the libpng and zlib licenses respectively.
- Duktape 2.7.0 by the Duktape authors: MIT. The official unmodified
  amalgamated source and license are vendored in
  `apps/jsdev/third_party/duktape`; it powers the JSDev runtime.
- stb_image and stb_truetype by Sean Barrett and contributors: public-domain or
  MIT dual-use terms offered by upstream.
- SF3000-RE by goph-R: hardware research and boot-format reference material.
- Zig, LLVM/Clang, and GNU development tools: used to build the project under
  their respective open-source licenses; the toolchains are not shipped in the
  SD-card overlay.
- Art Book Next by Anthony Caccese: CC BY-NC-SA 4.0 artwork resources retained
  from upstream TreeFrogUI.
- GamePocket by AbFarid: SIL Open Font License 1.1.
- monogram by datagoblin: CC0 1.0.

## UI fonts

The additional Atkinson Hyperlegible, Audiowide, Bungee, Chakra Petch,
Quantico, Rajdhani, Righteous, Share Tech Mono, Space Mono, and Tomorrow fonts
come from the official [Google Fonts repository](https://github.com/google/fonts)
and are distributed under the SIL Open Font License 1.1. Each family's complete
`OFL.txt` is included under `frogui/fonts/licenses/`.

## UI interaction sounds

The Bright, Soft, and Cyber sound packs use selected clips from Kenney's
[UI Audio](https://kenney.nl/assets/ui-audio) pack, released under Creative
Commons CC0. The original notice is included as `frogui/sounds/KENNEY-CC0.txt`.
The pack names describe their use in SwitchFrogUI; they are not Nintendo or
Sony recordings and are not affiliated with either company.

## Included sample recording

`SwitchFrogUI Sample - Mozart - Piano Sonata No. 14.ogg`

- Composition: Wolfgang Amadeus Mozart, Piano Sonata No. 14 in C minor, K. 457
- Performance: La Pianista, recorded 2010-06-09
- Source: https://commons.wikimedia.org/wiki/File:Mozart_-_Piano_Sonata_No._14.ogg
- Composition status: public domain
- Performance license: CC0 1.0 Universal Public Domain Dedication
- License: https://creativecommons.org/publicdomain/zero/1.0/
- Original audio SHA-256:
  `80148604C5A2F528F6FCF318D475A94AFF38266AD626C583CFAEA8ABCC101C98`

The recording is included only as a Rockbox functionality test. No attribution
is legally required by CC0, but provenance is recorded here.
