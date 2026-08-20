# JSDev JavaScript game development

JSDev is a permanent SwitchFrogUI Home card for creating and running small
JavaScript games directly on an R36SX. Projects are ordinary `.js` files under
`roms/JSDev`; they can be copied between the card and a computer without a
special project format.

## Project browser

- **A** runs the selected project.
- **X** creates and opens a new project containing a working starter game.
- **Y** edits the selected project in SwitchFrogUI Text Editor.
- **Select** renames the selected project.
- **B** returns to the previous folder or Home.

The built-in `JSDev API Showcase.js` is a runnable, commented demonstration of
the complete API. PicoArch supplies the in-game menu and honors the system's
keyboard-to-gamepad mapping, display-glitch fix, scaling, and return position.

## Runtime model

Code at the top level runs once. Register frame and input handlers with
`JSDev.on(event, callback)`:

```javascript
JSDev.on("update", function (dt) {
    // dt is elapsed time in seconds (normally 1/60).
});

JSDev.on("draw", function () {
    // Draw the current frame.
});

JSDev.on("buttondown", function (button) {});
JSDev.on("buttonup", function (button) {});
JSDev.on("load", function () {});
JSDev.on("unload", function () {});
```

Button names are `UP`, `DOWN`, `LEFT`, `RIGHT`, `A`, `B`, `X`, `Y`, `L1`,
`R1`, `L2`, `R2`, `L3`, `R3`, `START`, and `SELECT`.

## API reference

### Graphics

The framebuffer is RGB565 at 640×480. Colors may be `0xRRGGBB`, `"#RRGGBB"`,
or one of `white`, `black`, `red`, `green`, `blue`, `yellow`, `cyan`, and
`magenta`.

```javascript
var g = JSDev.graphics;
g.width; g.height;
g.clear(color);
g.pixel(x, y, color);
g.line(x1, y1, x2, y2, color);
g.rect(x, y, width, height, color, filled);
g.circle(centerX, centerY, radius, color, filled);
g.text(x, y, text, color, pixelSize); // 8–72 px
```

Drawing persists until it is cleared or overwritten. A typical game clears in
its `draw` handler before drawing the new frame.

### Input and time

```javascript
JSDev.input.isDown("A"); // true while held
JSDev.time();            // milliseconds since launch

var id = setTimeout(function () {}, 250);
var repeating = setInterval(function () {}, 1000);
clearTimeout(id);
clearInterval(repeating);
```

Movement should multiply speed by `dt` so it remains frame-rate independent.

### Sound

```javascript
JSDev.audio.beep(frequencyHz, durationMs, volume, waveform);
JSDev.audio.stopAll();
```

Volume is from 0 to 1 and is still affected by the normal OS volume. Supported
waveforms are `square`, `sine`, `triangle`, and `noise`; up to eight sounds can
overlap.

### Persistent storage

```javascript
var score = JSDev.storage.get("high-score", 0);
JSDev.storage.set("high-score", score);
JSDev.storage.remove("high-score");
```

Values use JSON, so numbers, booleans, strings, arrays, plain objects, and null
are supported. Data is isolated per script below `roms/JSDev/.data`. Storage
keys are sanitized, scripts are capped at 1 MiB, and stored values at 256 KiB.

### Logging and errors

`console.log(...)` appends to that project's `.data/.../console.log`. Uncaught
JavaScript errors are shown in a readable on-screen panel instead of crashing
PicoArch; edit the script and relaunch it after correcting the error.

## JavaScript compatibility

JSDev embeds Duktape 2.7.0 and is designed around portable ES5 syntax. Use
`var` and `function` in projects for best compatibility. The runtime and the
unmodified engine sources build with the rest of the R36SX source tree.
