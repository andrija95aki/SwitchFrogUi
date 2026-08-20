# JSDev runtime

JSDev is SwitchFrogUI's small JavaScript game-development runtime for R36SX.
It is a libretro core so JavaScript projects use the existing PicoArch video,
audio, input, scaling, keyboard mapping, display-fix, and in-game-menu paths.

The runtime embeds Duktape 2.7.0. The unmodified official amalgamated sources
are in `third_party/duktape` with their MIT license. The pinned release archive
has official MD5 `b3200b02ab80125b694bae887d7c1ca6`.

Projects are normal `.js` files in `roms/JSDev`. Persistent data is isolated by
script name under `roms/JSDev/.data`; storage keys are sanitized before use.
Scripts are limited to 1 MiB and storage values to 256 KiB to suit the device.
