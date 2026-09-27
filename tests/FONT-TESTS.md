# Font resilience tests

After `scripts/Prepare-R36SXSources.ps1` applies the FrogUI patch, compile the
test with a native C compiler, for example:

```sh
cc -O2 tests/font_resilience_test.c -lm -o font-resilience-test
cd frogui
../font-resilience-test
```

Run from `frogui/` so the normal font loader finds the bundled `fonts/` files.
The test expects the twelve distributed faces to be installed there (the
R36SX build assets supply them). It checks missing/short font reads, failed
allocation, preservation of the current face, repeat-load caching, no-heap
fallback text, scaled keyboard-style labels and unchanged main-menu metrics.
It performs 720 draw cycles across 12 faces and three user font sizes. It does
not simulate SD hardware or reproduce every physical-device failure.

`apps/font_fallback.h` is checked in so ordinary builds need no generation step.
To regenerate its CC0 ASCII raster from the bundled monogram face:

```sh
cc -O2 tests/make_font_fallback.c -lm -o make-font-fallback
./make-font-fallback frogui/fonts/monogram.ttf apps/font_fallback.h
```

Keep the generated font's datagoblin/CC0 provenance notice. No proprietary
fonts, runtime downloads, or SD-card files are required by this fallback.
