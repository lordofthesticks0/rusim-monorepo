# Display Demo Video (`video-demo/`)

A slow walkthrough video of the `display-v9` LVGL touch UI, written in Rust with
[fframes](https://github.com/dmtrKovalenko/fframes). The video is code: every
frame is an SVG tree built by `video-demo/src/lib.rs`, rendered on the GPU (Skia
over Vulkan) and encoded by FFmpeg into an H.264 `.mp4`.

The footage is the real UI. The screens are snapshots taken by the LVGL PC
simulator ([display-sim.md](display-sim.md)), so fonts, colours and layout are
the ones the ESP32-S3 draws on the panel.

## Layout

| Path | Contents |
|---|---|
| `video-demo/src/lib.rs` | The video: storyboard table `BEATS`, camera, spotlight, touch marker, captions |
| `video-demo/src/main.rs` | Command line (`fframes::cli`): render, preview, frame, strip, inspect |
| `video-demo/media/ui-*.png` | Simulator snapshots (800 x 480), converted from BMP |
| `video-demo/media/sfx-*.wav` | Sound effects, synthesised with FFmpeg (see Sound) |
| `video-demo/media/Montserrat-*.otf` | Caption font. LVGL's built-in fonts are Montserrat, so captions match the panel. SIL Open Font License, copied from Fedora's `julietaula-montserrat-fonts` |
| `video-demo/Cargo.toml` | Dependencies, pinned to fframes 1.4.0. `fframes_skia_renderer` has the `cli` feature for `--renderer skia-cpu` |
| `video-demo/.cargo/config.toml` | Sets `FFMPEG_FORCE_BUILD=1` (see Build) |
| `video-demo/hyperframes/` | A separate HyperFrames project (the dashboard demo video), not part of the Cargo build. See [dashboard-demo-video.md](dashboard-demo-video.md) |
| `video-demo/hyperframes-mobile/` | The same HyperFrames video on the dashboard's phone layout. See [dashboard-demo-video.md](dashboard-demo-video.md) |

Everything inside the `media/` folder is compiled into the binary.

## How the video works

- **One continuous shot.** The device (a drawn bezel around the snapshot) never
  cuts away. A camera glides between parts of the screen. The camera is a
  keyframe animation of `(x, y, zoom)`: `x, y` is the display pixel placed at
  the centre of the stage.
- **Beats.** Each entry of `BEATS` is one step: which snapshot is on the panel,
  how long it stays, which rectangle is in focus, the zoom, the caption, and an
  optional gesture at the end. `VideoDemoVideo::new` turns the table into all
  animations. Changing one beat's `seconds` shifts everything after it.
- **Two zoom levels.** `WIDE` (1.0) shows the whole device with margins; the
  spotlight points at the subject. `CLOSE` (1.4) is for small targets (tabs,
  link chip, start button, modal): the device is clearly larger than the frame,
  so the crop reads as a close-up. Zooms between about 1.05 and 1.3 clip the
  device edge by a few pixels, which looks like a mistake, so they are not used.
  The camera follows the focus fully up and down but only 30 % sideways
  (`PAN_X`), so the device never hugs one edge of the frame.
- **Spotlight.** Everything outside the focus rectangle is dimmed. The hole is a
  rounded rectangle cut out of a panel-sized path (`fill-rule="evenodd"`). It
  glides with the camera and stays 2 px inside the panel so its outline is never
  cut. Before a tap it lifts while the finger comes in, so the tapped element is
  not in the dimmed part; before a hold it stays, because the held button is the
  focus.
- **Touch marker.** A `Tap` or `Hold` gesture draws a fingertip that slides in,
  presses and sends out a ripple. A `Hold` also fills a ring over 2 s, the real
  `UI_HOLD_TO_CONFIRM_MS`. The next beat's snapshot appears when the gesture
  commits.
- **Captions.** Each beat is an fframes `Scene` that draws only its caption
  (chapter label, one line, one detail line). The chapter label stays on screen
  while consecutive beats share it.
- **Timing.** Camera moves take 1.8 s with ease-in-out. Snapshots cross-fade over
  0.9 s. During a hold the zoom creeps by 2.5 %, so the picture is never frozen.
  The real panel switches screens instantly (no LVGL animations, see
  [display-docs.md](display-docs.md) §15.1); the cross-fade is a video choice.

- **Flat backdrop.** The background is one solid colour (`STAGE`, `#0e1319`)
  and the device has no blurred drop shadow. A soft gradient or blur on a dark
  background spans only a few brightness levels, so H.264 turns it into visible
  bands and blocks. The bezel's 1 px outline separates the device from the
  backdrop instead.

All coordinates in `BEATS` are display pixels (the 800 x 480 panel), read off
the snapshots.

## Storyboard

| Scene | Snapshot | Shows |
|---|---|---|
| Title | setup | Title card |
| Login (x2) | setup | QR login; the display never sees credentials |
| Overview (x4) | home | Tabs, run card, status card, link chip and bell; tap Data |
| Data (x4) | data picker, node, bottle | Six nodes, bottle temperatures, five sensors, chart |
| Control (x5) | control, modal | Steppers, 2 s hold switches, hold to start, start-blocked modal, tap Info |
| Info (x2) | info | Link, database, run, queue, feed, UI load; tap Home |
| Running | home running | Progress while a run is active |
| Sleep | home running | Backlight off after 60 s; a touch wakes it |
| End | home running | Fade out |

`06-bottle-missing` is not used: the simulator's preview data draws a trend
chart for the absent bottle 4, which the real sensor feed never does.

## Sound

The video has sound effects only (no music or voice-over). `cues()` in
`lib.rs` builds the list from `BEATS` with the same timing functions the
animation uses (`touch_times`, `GLIDE`, the backlight seconds), so changing a
beat's length moves its sounds with it. `Video::audio` turns each cue into an
`AudioTrack`.

| File | Plays | Gain |
|---|---|---|
| `sfx-swell.wav` | Soft chord as the device fades in (0.3 s) and out (3.6 s before the end) | -7 / -10 dB |
| `sfx-whoosh.wav` | Under every camera glide; its loudest point lands on the glide's fastest point | -16 dB |
| `sfx-tap.wav` | 0.1 s after the fingertip presses (taps and the hold) | -3 dB |
| `sfx-hold.wav` | Rising tone during the 2 s hold, while the ring fills | -10 dB |
| `sfx-error.wav` | Two falling notes when the hold ends on the "start blocked" modal | -5 dB |
| `sfx-sleep.wav` | Falling tone while the backlight turns off | -8 dB |
| `sfx-wake.wav` | Rising tone when the backlight comes back | -8 dB |

Taps are panned left or right by the touch's x position on the panel
(`pan_for`), at most 40 %. Embedded audio is mono, so pan is the only stereo
effect.

The files were generated with FFmpeg's `aevalsrc` (sine tones with
exponential decay) and `anoisesrc` (pink noise for the whoosh), 48 kHz mono
16-bit WAV. Example, the tap:

```bash
ffmpeg -f lavfi -i "aevalsrc='0.55*sin(2*PI*1500*t)*exp(-t*110)+0.35*sin(2*PI*520*t)*exp(-t*45)':d=0.15:s=48000" \
  -ar 48000 -ac 1 -c:a pcm_s16le sfx-tap.wav
```

They are placeholders: replace a file with a recorded or licensed sound of the
same name to change it. Check levels with `cargo run --release -- audio analyze`
(true peak must stay below -1 dBTP; it is -5.3 dBTP now) and `audio at <time>`
to see what plays at a moment. Integrated loudness is low (about -27 LUFS)
because the track is mostly silence between effects.

## Commands

Run inside `video-demo/`. Keep `--release`; debug builds render many times
slower.

```bash
cargo run --release -- timeline              # scenes and their seconds
cargo run --release -- preview 0s            # real-time window (space, h/l, j/k, q)
cargo run --release -- strip Overview -n 12  # contact sheet -> strip.png
cargo run --release -- frame Data@3s         # full-size PNG -> frames/
cargo run --release -- inspect              # automatic checks
cargo run --release -- render --renderer skia-cpu -o out.mp4   # final video
```

The final video is `video-demo/out.mp4` (git-ignored): 1920 x 1080, 30 fps,
H.264, 176.5 s, 5295 frames, with an AAC track holding the sound effects.

`--renderer skia-cpu` draws with Skia on the CPU: the same pictures as the GPU,
about 16 frames per second here (about 5 minutes for the whole video). The
GPU (Vulkan) path is about 4x faster, but on this laptop the Intel `xe` kernel
driver timed out a job and reset the GPU 70 % into a full render ("device
lost"). Without `--renderer` the GPU is used, which is fine for `preview`,
`frame` and short ranges. fframes 1.4.0 has no `--pops` check.

Scenes are addressed by name: `Overview` is the first Overview beat,
`Overview[2]` the third, `Control@50%` the middle of the first Control beat.

## Refreshing the snapshots

After a UI change, retake the snapshots and copy them in:

```bash
pio run -e display-sim
SDL_VIDEODRIVER=dummy SIM_SHOTS=/tmp/shots .pio/build/display-sim/program
for f in /tmp/shots/*.bmp; do magick "$f" "video-demo/media/ui-$(basename "${f%.bmp}").png"; done
rm video-demo/media/ui-06-bottle-missing.png
```

If a widget moved, update the focus rectangles and tap points in `BEATS`.

## Build

The first build takes several minutes because FFmpeg is compiled from source.
Two fixes are in place:

| Problem | Fix |
|---|---|
| `fframes` 1.4.0 accepts any `fframes-media` 1.4.x, but 1.4.1 renamed a type, so the build fails with `type alias takes 0 lifetime arguments` | `Cargo.toml` pins `fframes-media` and `fframes-media-dir-macro` to `=1.4.0` |
| The prebuilt FFmpeg is linked against x264 ABI 163; Fedora 44 ships ABI 165, so linking fails with `undefined symbol: x264_encoder_open_163` | `.cargo/config.toml` sets `FFMPEG_FORCE_BUILD=1`, so FFmpeg is built against the system x264. Needs `nasm` and `x264-devel` |

If FFmpeg was ever built from the prebuilt download in the same `target/`
folder, run `cargo clean --release -p ffmpeg-sys-fframes` once: the build script
skips compiling when `libavutil.a` already exists in its output folder.

The `vulkan` backend needs a working Vulkan driver (Intel Mesa was used).

## Limitations

- The snapshots show the simulator's state: no WT32 link ("No WT32"), database
  not ready, 5 missing readings, and the `DEBUG: bypass login` button on the
  login screen (`IS_DEBUG` build). The captions describe what is on screen.
- The bezel is drawn, not a photo of the Sunton board.
- The sound effects are synthesised placeholders, and there is no music.
- `media/` is git-ignored, so the snapshots, fonts and sound files are not in
  the repository.

---
Updated at 2026-11-26-17:30.
