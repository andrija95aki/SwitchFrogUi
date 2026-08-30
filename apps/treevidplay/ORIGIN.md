# Merged video-player upstream provenance

This directory preserves an unchanged copy of TreeFrogUI's standalone hardware
video player as the decoder/timing baseline for SwitchFrogUI's single merged
`Videos` application.

- Project: <https://github.com/tzubertowski/TreeFrogUI>
- Release/tag: `v1.2.0_b`
- Source commit: `21d7abf4303715583bf536237cff8533d343d24c`
- Source file: `apps/video_player/video_player.c`
- Vendored source SHA-256:
  `7DF2C1888C68AA93882FA812D0DCACB781ACFD94441DBA211449F641D3B9BCC0`
- Official release runtime SHA-256:
  `0347E4BF1B26FBDA5CA964E88B9CC57DCB10008E316F2992B933430E7AFAFAEC`
- Upstream author: Tomasz Zubertowski (`tzubertowski` / `proszty`)
- License: see `UPSTREAM-LICENSE.md`; the TreeFrogUI frontend creative code
  is distributed under CC BY-NC-SA 4.0 and all component licenses remain in
  force.

`upstream_video_player.c`, `UPSTREAM-README.md`, and
`UPSTREAM-LICENSE.md` are copied verbatim from that pinned commit.

The active merged implementation is `../video_player.c`. It retains the
upstream 256-byte zeroed player-init block, audio-master clock, direct I2SO
output, layer order, message loop, playlist handling and startup watchdog.
SwitchFrogUI adds its safe local SRT/WebVTT renderer, automatic exact-basename
subtitle selection, persistent 100 ms timing offsets, Fit/Fill/Stretch/Original
display modes, pause menu, broader file extensions and persistent 180-degree
rotation. It is built by the official SF3000 GNU SDK workflow; Zig is not used
for this H.OS executable ABI.
