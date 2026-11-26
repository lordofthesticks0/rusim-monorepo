# Review: RuSim dashboard demo, mobile UI

Film: storyline A on the dashboard's phone layout, built from `SPEC.md`. 141.6 s, 1920 x 1080,
30 fps, sound effects only. Not rendered yet: waiting for approval of the Studio preview.

## What the person asked for (2026-11-26)

"The same video, but the mobile version", with the same pacing and a similar outro, rendered
as a landscape video showing the mobile UI.

- Same: every beat time of the desktop second cut (holds, camera moves, captions, CSV sheet,
  clicks as taps), the caption band and its three entrances, the zoom through the RuSim mark
  and the end card with the QR code (the same file).
- Changed for the phone: the dashboard is recorded at 390 x 844 CSS px with touch input and
  shown as a phone in the middle of the landscape frame. The desktop camera pans became
  scrolls inside the screen, at the same times. The cursor became a touch dot.

## Creative calls

- The phone is the captured screen in a plain black bezel: no status bar, notch or brand, so
  nothing on it is invented. It sits in the middle of the stage, under the same caption band.
- The touch dot appears only where a tap happens (lands 0.4 s before, lifts 0.3 s after), so
  it does not read as a mouse. Scrolls show no finger.
- Below 860 px the app hides its sidebar in a drawer, so each change of view is two taps:
  the menu, then the view. The menu taps fall in the camera moves before the desktop film's
  nav clicks, so those clicks keep their times. The ending opens the drawer once more: it
  shows the three views of the claim "Watch it live. Look back. Compare." and holds the RuSim
  mark the camera zooms through, at the same screen position as the desktop sidebar's.
- Monitoring: the grid and the five sensor cards do not fit one zoomed view on a phone, so
  the camera pulls back while the page scrolls 219 px, the chamber is tapped with both in
  view (the values tick in), then the camera moves to the sensors.
- Captions 1 and 9 now say "from your phone" and "Tap any point"; the rest are unchanged.
- History opens Experiment 4 by name, as the desktop film did. A new Experiment 5 went live
  today and now heads the list; it is visible in the list and in Monitoring.

## Measured, and only looked at

- `check`: passes, no errors.
- Seam walk -> end card: the frames at 130.567 and 130.6 s are identical (0 pixels differ).
- Clip -> still handovers, frame pair across each handover vs. a pair inside the clip
  (pixels off by more than 8 levels): drawer 0.04 %, menu clips 0.00 %, chamber select
  0.30 %, Results 0.16 %, first Compare add 0.01 %, ending drawer 0.35 % (camera creeping).
  Chamber tap 1.04 % and second Compare add 0.70 %: compression on the amber stripes and the
  chart colours; only 0.15 % and 0.09 % differ by more than 24 levels, nothing moves.
- Chrome draws its blue tap highlight on the tapped element for exactly one recorded frame
  (frame 0 of each tap clip). Shown, it is a 33 ms flicker, so each tap clip starts one frame
  late and the still before it holds that frame.
- The QR code decodes to `https://unpadrumensim.netlify.app` from the final frame, also at
  640 x 360 (zxing-cpp).
- QA frames from `SPEC.md`, the beat, seam and phone review sheets were looked at.
- Sound: the desktop film's sounds at the same times, plus five taps (35.7, 60.0, 95.4,
  101.3, 124.4). Nobody has heard it; loudness not measured (`npx hyperframes audio`).

## Weakest shots, and what I would change

- Wide shots (0–4.4, 16.4–21.2, 25.6–29.2, 121.2–128.8): a portrait phone in a landscape frame
  leaves wide empty ground on both sides, and the UI text is small (scale 0.98). They are
  short establishing beats; captions carry them.
- 104.3: the first series is added while its chart is below the fold, so its draw is not
  seen; the scroll after it shows the line already drawn. The second add (111.1) shows the
  draw.
- 13.0–14.6: the "Verifying token" still is a separate take (see below), so its field has
  lost focus while the typed still before it is focused. It matches a real tap on the button.

## Warnings kept on purpose

- `timeline_track_too_dense` (36 timed elements) and `composition_file_too_large`: one
  continuous take, so one scene.
- `nested_media_start_basis_ambiguous` (11 videos): times are local to the walk scene, which
  starts at 0.

## Facts to keep in mind

- The recording froze the "Verifying token" label at the start of its 180 ms fade, so the
  first take showed an empty blue button. The still was re-taken (unfrozen, 450 ms after the
  tap) and `capture.ts` now moves the animations 400 ms on before that still. The desktop
  film's `login-verifying.png` has the same empty button.
- Monitoring shows live data of Experiment 5, recorded 2026-11-26 at 17:13 local time. A new
  recording shows newer values.
- The end-card URL and the QR code point at the Netlify site (the 2026-10-01 deploy). The
  mobile layout in this film is `master`.
- `*.mp4` and `*.wav` are not tracked (`video-demo/.gitignore`): the clips come back from
  `capture/capture.ts`, the sounds from `bash capture/build-sfx.sh`.
