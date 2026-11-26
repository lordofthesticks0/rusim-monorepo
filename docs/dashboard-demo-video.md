# Dashboard Demo Videos (`video-demo/hyperframes/`, `video-demo/hyperframes-mobile/`)

Two versions of one film. `video-demo/hyperframes/` shows the desktop layout.
`video-demo/hyperframes-mobile/` shows the phone layout with the same times
([Mobile version](#mobile-version-video-demohyperframes-mobile)). Everything up to that section
describes the desktop project; the mobile project works the same way unless that section says
otherwise.

A 142-second (141.6 s) walkthrough video of the dashboard (`dashboard/`), made with
[HyperFrames](https://hyperframes.heygen.com). HyperFrames renders HTML to video:
the film is ordinary HTML, CSS and a GSAP timeline, and the renderer seeks the
paused timeline frame by frame, so every render is identical.

It is a separate project from the Rust/fframes display video in the same folder
([video-demo.md](video-demo.md)). It is not part of the Cargo build.

The footage is the real app. Every screen is a capture of the dashboard on `master`
running locally, signed in as development device 0. The app's own animations are
recorded frame by frame, not redrawn.

## Layout

| Path | Contents |
|---|---|
| `index.html` | Root composition: 141.6 s, the two scene hosts and every sound effect |
| `compositions/walk.html` | Scene 1 (0–130.6 s): one continuous camera take through the app |
| `compositions/endcard.html` | Scene 2 (130.6–141.6 s): the RuSim mark, product and lab lines, the URL, then the QR code |
| `capture/capture.ts` | Records the app: stills, frame-exact clips, element positions, the CSV |
| `capture/build-sfx.sh` | Rebuilds `assets/sfx/` from the fframes project's sounds |
| `capture/build-qr.py` | Writes `assets/qr.svg`, the QR code of the end card |
| `assets/screens/*.png` | Stills of each UI state, 3840 px wide (full page height) |
| `assets/clips/*.mp4` | The app's own animations, recorded at 30 fps |
| `assets/capture.json` | Element rectangles in page CSS pixels; the film's camera targets come from it |
| `assets/data/` | The real CSV the app exported (Experiment 4) and the one-colour Unpad emblem |
| `assets/qr.svg` | QR code for `https://unpadrumensim.netlify.app` (version 3-M, 29 x 29 modules) |
| `assets/fonts/` | Chivo and Chivo Mono (variable, Latin), the dashboard's own fonts |
| `assets/sfx/` | Click, whoosh and swell from `video-demo/media/`, plus typing and tick tracks built from the click |
| `STORYLINES.md` | The three storylines offered; A was chosen |
| `SPEC.md` | The film as numbers: look, motion, every shot with its time |
| `REVIEW.md` | What was checked, the weakest shot, warnings kept on purpose |

## Running it

Run the HyperFrames CLI through the installed skill's launcher, from
`video-demo/hyperframes/`:

```bash
L=~/.claude/skills/hyperframes/scripts/plugin-cli.mjs
node $L preview --background   # Studio preview in the browser
node $L check                  # contract, layout, motion and contrast checks
node $L snapshot --at 12.5     # a PNG of one moment
node $L render                 # the MP4
```

To record the screens again (after a UI change in `dashboard/`):

```bash
cd dashboard && bun run dev                 # terminal 1: the dev server on :5173
cd video-demo/hyperframes/capture
bun install && bun run capture.ts           # terminal 2: about 10 minutes
```

`ONLY=login bun run capture.ts` records only the login clip (a few minutes) and keeps
every other asset; it was used when the login scene became longer.

The script signs in with the development token of device 0 (`RUSIM_DEVEL_TOKEN`,
committed in `firmware/network-master/main.cpp`; set `RUSIM_DEMO_TOKEN` to use
another). It opens `http://localhost:5173/` (`DASHBOARD_URL` overrides it; Vite
listens on the IPv6 `localhost`, so `127.0.0.1` does not answer). It uses the
cached Chrome headless shell from Puppeteer (`CHROME_PATH` overrides it). It
overwrites `assets/screens`, `assets/clips`, `assets/data` and
`assets/capture.json`. When element positions move, the camera and cursor targets
in `walk.html` must be updated from the new `capture.json`.

To make the QR code for another address (uv installs `segno` on the fly):

```bash
uv run video-demo/hyperframes/capture/build-qr.py https://example.org
```

Change the end card's `url` variable to match.

## How the screens are recorded

The browser window is 1440 x 810 CSS pixels at a device pixel ratio of 8/3, so
each screenshot is 3840 x 2160. The film can zoom in to about 2.7x and stay sharp.

A screenshot at 3840 x 2160 takes about a second, far longer than the app's
animations (140–900 ms). Screenshots taken while an animation runs would show
random moments of it. So time is frozen during a recording:

1. CSS animations and transitions are frozen through the Chrome DevTools Protocol
   (`Animation.setPlaybackRate` to 0).
2. JavaScript timers are paused with Playwright's fake clock (`page.clock`). The app
   uses a timer to end its entrance animation, so the timers must move with the frames.
3. The interaction runs (a click, a select).
4. For each frame, the clock advances 1/30 s and every animation on the page is set
   to its exact time (`animation.currentTime`). Then the screenshot is taken.

Because of this, a recording is repeatable: the 480-frame login clip matches the
earlier 252-frame take pixel for pixel over their common 8.4 s.

The frames are encoded to H.264 with FFmpeg. After each recording, a still of the
settled state is taken, and the film switches from the clip to the still on the
clip's last frame. They differ in at most 0.2% of pixels.

The script also saves every target's rectangle (`capture.json`), downloads the CSV
through the app's own button, and copies the BrandMark SVG.

## How the film is built

- **Pace.** The second cut takes every time of the first cut (72.2 s) twice as long,
  up to the zoom through the mark: holds, camera and cursor moves, captions, typing and
  the CSV sheet. The app's recorded animations, its quick responses (0.10–0.14 s) and
  the click press stay at real speed, so the dashboard does not look slow; the still
  after each clip holds longer instead. The zoom through the mark and the end card keep
  their first-cut speed.
- **Camera.** `#walk-rig` holds CSS variables: `--cx`, `--cy` (the page point placed
  at the stage centre, 960 x 460) and `--k` (the zoom). Plain CSS turns them into
  transforms (`translate(960px, 460px) scale(var(--k))` around
  `translate(-cx, -cy)`). GSAP tweens only the variables, so a camera move is one
  tween and the chosen point travels in a straight line.
- **Creep.** A hold longer than 3 s zooms in by 2.5% (`creep()`), so the picture never
  freezes. The last one zooms about the sidebar's RuSim mark, so the mark stays in frame
  for the zoom through it.
- **Cursor.** Its position (`--px`, `--py`) is in page coordinates, so it stays on its
  target while the camera moves. Its scale is `var(--press) / var(--k)`, so it keeps
  the same on-screen size (54 px) at every zoom. A click is `--press` dipping to 0.86.
- **Layers.** Stills (`<img>`) and clips (`<video>`) sit in page coordinates inside
  the rig, each shown in its own time window (`data-start`, `data-duration`). Region
  clips (History) only cover the part of the page that changes.
- **Typing.** The "typed" still shows 17 dots; a white bar over them shrinks one dot
  every 164 ms, timed with the typing ticks. GSAP's `steps(16)` holds 17 values, each
  for 1/17 of the tween, so the tween lasts 17 x 164 ms.
- **CSV sheet.** The Download CSV button grows into a sheet showing the first 12 lines
  of the real export. Chamber 3 rows are tinted.
- **Captions.** One line at a time in a band under the stage (y 920–1080), Chivo 40 px.
  Three entrances rotate: per-word blur, per-word grey to ink, whole line.
- **End card.** The zoom ends on the sidebar's RuSim mark; a vector copy of the mark
  takes over, so the last frame of `walk` and the first of `endcard` are identical.
  After the URL types in, the whole lockup (`#end-stack`) moves 260 px left and the QR
  card opens beside it; a blue line runs down the code as it is uncovered. The QR code
  has 10 px modules and a 40 px quiet zone; it decodes from the final frame even at
  640 x 360. `product`, `institution`, `url` and `qrLabel` are composition variables
  of the end card.
- **Sound.** No music. Clicks, typing and CSV line ticks, two whooshes and one swell.
  Each starts one frame after its picture. The swell and whooshes are offset by their
  measured peaks (0.706 s, 0.762 s).

Every tween is a `fromTo` with explicit start values, so seeking to any frame gives
the same picture. In `walk.html` they also set `immediateRender: false`, because many
tweens act on the same element and only the first may apply its start state early.

## What is on screen

- Monitoring shows the app's Preview data (experiment 999) with its "Preview" banner,
  because `DEV_FORCE_LIVE_EXPERIMENT` is 1 in `dashboard/src/monitoring.ts` and no
  experiment is live. Chambers 07 and 19 are the Preview's silent chambers.
- History and Compare show the stored experiments of device 0 (Experiment 4, Chamber 3;
  Compare adds Experiment 2, Chamber 3).
- The end-card URL and the QR code point at the Netlify site, which serves the
  2026-10-01 deploy, not `master`.

## Mobile version (`video-demo/hyperframes-mobile/`)

The same film (141.6 s, 1920 x 1080, landscape) on the dashboard's phone layout. Every caption,
camera move and click of the desktop version happens at the same second; clicks become taps.
Captions 1 and 9 say "from your phone" and "Tap any point". The end card is the same file.

| Path | Contents |
|---|---|
| `index.html` | Root composition: the two scenes and the sounds (18 taps) |
| `compositions/walk.html` | Scene 1 (0–130.6 s): the phone, its scroll, the camera and the touch dot |
| `compositions/endcard.html` | Scene 2: a copy of the desktop end card |
| `capture/capture.ts` | Records the app at phone size |
| `capture/build-sfx.sh`, `capture/build-qr.py` | As in the desktop project, writing into this project |
| `SPEC.md`, `REVIEW.md` | The film as numbers; what was checked |

**How the phone is recorded.** `capture.ts` opens a 390 x 844 CSS px viewport with touch input
(Playwright's `isMobile` and `hasTouch`) at a device pixel ratio of 4, so each screen is
1560 x 3376 pixels. Taps use `locator.tap()`, which sends touch events. Below 860 px the
dashboard hides its sidebar in a drawer behind the menu button, so the script opens the
drawer before each change of view and records it. Every clip is one whole screen, recorded
at a scroll position the script chooses from element positions (for example, the chamber
grid and all five sensor cards on one screen). `capture.json` stores these positions under
`scroll` and in each clip's `page.y`.

**How the phone is shown.** The film draws the captured screen with 26 px corners inside a
plain 12 px black bezel. All stills are full-page screenshots, placed in page coordinates
inside `#walk-page`. The CSS variable `--sy` moves that layer up, which scrolls the page
inside the screen, and the screen (`overflow: hidden`) cuts off the rest. The camera rig is
the same as on desktop, but `--cx` and `--cy` are points on the phone screen. While a clip
plays, `--sy` equals the scroll position it was recorded at, so the clip fills the screen
exactly.

**Taps.** A 64 px dot marks each tap: it lands 0.4 s before the press, presses like the
cursor did, and lifts 0.3 s after. It never travels, so it does not look like a mouse. Scrolls
show no finger. Chrome draws a blue tap highlight on the tapped element, but in a frame-by-frame
recording it lasts exactly one frame, which looks like a flicker. Each tap clip therefore starts
one frame after its tap (`data-media-start="0.0334"`), and the still before it holds that
frame.

**Data.** The desktop film used Experiment 4 as the newest run. Since then Experiment 5 went
live, so Monitoring shows its live readings instead of the Preview data, and it heads the
history list. The script opens Experiment 4 by name, so History, the CSV and Compare show the
same data as the desktop film.

To record it again (dev server running, as above):

```bash
cd video-demo/hyperframes-mobile/capture
bun install && bun run capture.ts           # about 8 minutes
bash build-sfx.sh                           # the sounds, from video-demo/media/
```

The preview, checks and render use the same commands as above, run from
`video-demo/hyperframes-mobile/`.

## Files and Git

`video-demo/.gitignore` ignores `*.mp4` and `*.wav` in every subfolder, so
`assets/clips/` and `assets/sfx/` are not tracked. The clips come back from
`capture.ts`. The sounds are rebuilt from `video-demo/media/` (also not tracked) by
`bash capture/build-sfx.sh`. `assets/qr.svg` is tracked (3 KB).

`video-demo/hyperframes/.gitignore` ignores `.hyperframes/` (the CLI's local state:
snapshots, check reports, preview state and the undo history `video-state.json`, so
`hyperframes history` only reaches back on this machine), `renders/` (rendered videos
and their metadata) and `.capture-frames/` (temporary screenshots). The mobile project
has the same `.gitignore`; its screenshots, `capture.json` and the CSV are tracked as in the
desktop project.

---
Updated at 2026-11-26-17:30.
