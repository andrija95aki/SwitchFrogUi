# Menu power saving (SwitchFrogUI 1.4.1)

The launcher previously requested the performance CPU governor and pinned the
minimum clock to the hardware maximum. FrogUI also re-submitted an unchanged
full-screen framebuffer on every tick, including while the backlight was off.
The card logs inspected did not demonstrate continuous idle logging; normal
launcher diagnostics are already opt-in via a root `log.txt` file.

The new frontend preserves 60 Hz input, debounce, sound and timeout behaviour,
but sleeps on a monotonic clock rather than depending on repeated display
submissions for pacing. Unchanged screens use libretro duplicate-frame
callbacks, with one visible keepalive per second and no presents while blanked.
Input/redraw/wake still submits a frame immediately. Frontends without duplicate
support retain the existing presentation path.

Where CPUFreq provides schedutil, ondemand or conservative, the frontend uses
that dynamic governor and the hardware-reported minimum frequency. It restores
the exact previous governor/minimum when unloaded, before PicoArch exec-chains
the game/app. Maximum clocks, voltages and thermal limits are not changed.
Missing CPUFreq or unsupported governors are safe no-ops, so CPU-frequency
savings depend on firmware support. See the [kernel CPUFreq documentation](https://docs.kernel.org/admin-guide/pm/cpufreq.html).

The video player, emulator runtimes, suspend/wake mechanism and game performance
settings are unchanged. A simulated ten-second static screen makes 10 presents
instead of 600, but this is not a measured battery-life or temperature result.

Device test: reboot, leave Home untouched for several minutes, check screen
timeout stays off, wake using a button and power-tap, test menu sounds, remapping
hold/repeat, terminal output, then launch/exit a game and a video. Compare idle
warmth/battery drain under the same brightness and charging conditions. CPU
clock/readings and actual temperatures cannot be measured from the SD card alone.

Included in the 1.4.1 full-card packages. The previous 1.4.0 release remains
available for rollback. Power savings and firmware compatibility still require
the physical-device checks above.
