# TreeVidPlay upstream provenance

TreeVidPlay is an unchanged copy of TreeFrogUI's standalone hardware video
player, exposed as a second SwitchFrogUI Home application so the existing
SwitchFrogUI `Videos` player remains available.

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
`UPSTREAM-LICENSE.md` are copied verbatim from that pinned commit. The R36SX
build stages the exact official `v1.2.0_b` runtime rather than rebuilding it
with Zig because H.OS 1.2 requires the vendor GNU executable ABI. The only
SwitchFrogUI-specific parts are the Home/browser integration and launch
wrapper outside this directory.
