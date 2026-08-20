/*
 * JSDev API Showcase
 * Demonstrates every API provided by SwitchFrogUI JSDev 1.0.
 * Move with the D-pad. A/B/X/Y play different waveforms. START resets
 * persistent state. SELECT toggles the help panel.
 */

var g = JSDev.graphics;
var input = JSDev.input;
var audio = JSDev.audio;
var storage = JSDev.storage;
var x = storage.get("x", 320);
var y = storage.get("y", 250);
var visits = storage.get("visits", 0) + 1;
var help = true;
var pulse = 0;
var lastButton = "NONE";
var message = "Persistent launch count: " + visits;

storage.set("visits", visits);
console.log("Showcase started", "launch", visits);

function savePosition() {
    storage.set("x", Math.round(x));
    storage.set("y", Math.round(y));
}

JSDev.on("load", function () {
    audio.beep(660, 80, 0.18, "sine");
    setTimeout(function () { audio.beep(880, 100, 0.16, "triangle"); }, 100);
});

JSDev.on("buttondown", function (button) {
    lastButton = button;
    if (button === "A") audio.beep(523, 140, 0.22, "square");
    if (button === "B") audio.beep(392, 140, 0.22, "sine");
    if (button === "X") audio.beep(659, 140, 0.22, "triangle");
    if (button === "Y") audio.beep(220, 140, 0.18, "noise");
    if (button === "SELECT") help = !help;
    if (button === "START") {
        audio.stopAll();
        storage.remove("x");
        storage.remove("y");
        x = 320; y = 250;
        message = "Saved position reset";
    }
});

JSDev.on("buttonup", function (button) {
    lastButton = button + " released";
});

JSDev.on("update", function (dt) {
    var speed = 180 * dt;
    if (input.isDown("LEFT")) x -= speed;
    if (input.isDown("RIGHT")) x += speed;
    if (input.isDown("UP")) y -= speed;
    if (input.isDown("DOWN")) y += speed;
    x = Math.max(24, Math.min(g.width - 24, x));
    y = Math.max(145, Math.min(g.height - 25, y));
    pulse = (Math.sin(JSDev.time() / 240) + 1) * 0.5;
});

var saveTimer = setInterval(savePosition, 1000);
var canceledTimer = setTimeout(function () { message = "This should never run"; }, 5000);
clearTimeout(canceledTimer);

JSDev.on("draw", function () {
    g.clear("#07111f");

    /* Lines, filled/unfilled rectangles, circles, pixels, colors and text. */
    for (var row = 0; row < g.height; row += 24) {
        var shade = 18 + Math.floor(row / g.height * 34);
        g.rect(0, row, g.width, 24, (shade << 16) | ((shade + 12) << 8) | (shade + 30), true);
    }
    g.text(22, 14, "JSDev API Showcase", "#70e8ff", 28);
    g.text(24, 52, message, "white", 16);
    g.text(24, 76, "Last event: " + lastButton, "#ffd35a", 16);
    g.line(20, 108, 620, 108, "#35536e");
    g.rect(24, 126, 130, 62, "#765dff", true);
    g.rect(170, 126, 130, 62, "#70e8ff", false);
    g.circle(370, 157, 31, "#ff5f91", true);
    g.circle(465, 157, 31, "#ffd35a", false);
    for (var i = 0; i < 24; i++) g.pixel(530 + i * 3, 145 + (i % 4) * 7, "#72f1a5");

    var radius = 18 + Math.floor(pulse * 8);
    g.circle(Math.round(x), Math.round(y), radius, "#70e8ff", true);
    g.circle(Math.round(x), Math.round(y), radius + 5, "white", false);
    g.text(Math.round(x) - 7, Math.round(y) - 13, "JS", "#07111f", 15);

    if (help) {
        g.rect(18, 370, 604, 92, "#101b32", true);
        g.rect(18, 370, 604, 92, "#586d92", false);
        g.text(30, 378, "D-pad move  A/B/X/Y sounds  SELECT help  START reset", "white", 15);
        g.text(30, 406, "Position saves every second and returns after restart.", "#a8bad4", 14);
        g.text(30, 432, "Open this .js file in Text Editor to experiment.", "#70e8ff", 14);
    }
});

JSDev.on("unload", function () {
    clearInterval(saveTimer);
    savePosition();
});
