# Review: RuSim dashboard demo

Film: storyline A, "One chamber, start to finish" (`STORYLINES.md`), built from `SPEC.md`.
141.6 s, 1920 x 1080, 30 fps, sound effects only. Not rendered yet: waiting for approval
of the Studio preview.

## Second cut (2026-11-26)

The person found the first cut (72.2 s) a little fast and asked for everything to take
twice as long, except the outro, plus a QR code at the end.

- Doubled: every start time, hold, camera move, cursor move, caption (time on screen and
  entrance speed), the typing (82 -> 164 ms a character), the CSV sheet and its line ticks
  (70 -> 140 ms). The walk scene goes from 66.2 to 130.6 s.
- Kept at real speed, on purpose: the app's own recorded animations (hand-off, value
  ticks, chart and series draws, page swaps), its quick responses (0.10–0.14 s fades for
  a tooltip or a select change) and the click press. Slowed down, the dashboard would look
  sluggish; the stills after each clip now hold longer instead.
- Unchanged, as asked: the zoom through the mark (1.8 s) and the end card up to the URL.
- The login page has drifting bubbles, so its clip was re-recorded longer (480 frames,
  16 s) with `ONLY=login bun run capture.ts`. Its first 8.4 s match the old take pixel
  for pixel; nothing else was re-recorded.
- New: holds longer than 3 s creep in by 2.5% (as in the fframes display video), so the
  picture never freezes; the last one pivots on the RuSim mark, so the mark is in frame
  when the zoom through it starts. Without them `check` flagged five dead stretches of
  3.1–4.5 s.
- New QR beat after the URL: the lockup steps 260 px left, a white card opens from its
  middle line (like the wordmark), a blue scan line uncovers the code, "Scan to open the
  dashboard" (variable `qrLabel`), then a 5 s hold. A soft whoosh (volume 0.3) marks the
  card. The code is `assets/qr.svg` from `capture/build-qr.py` (segno, version 3-M).
- Fixed in passing: the typing cover used `steps(16)` over 16 x 82 ms, but GSAP's
  `steps(16)` holds 17 values, so dots came every 77 ms against 82 ms ticks. The duration
  is now 17 x 164 ms, so each dot lands on its tick.

## Creative calls

- Storyline A was the person's pick. B ("Four questions at the bench") and C ("The silent
  chamber") are in `STORYLINES.md` and were not built.
- The film follows Chamber 3. It is the one chamber that exists both in the Monitoring
  Preview data (24 chambers) and in the stored experiments (chambers 1–4).
- Monitoring runs on the app's built-in Preview data (`DEV_FORCE_LIVE_EXPERIMENT = 1`), and
  its "Preview" banner stays visible, because no experiment is live. Its chart is a straight
  synthetic line, so the chart beat uses Experiment 4 (stored readings) instead.
- Every screen is the real app on `master` (local dev server, device 0 token), captured by
  `capture/capture.ts`. The app's own motion (login curve, hand-off, value ticks, chart and
  series draws, nav pill, view swaps) is recorded frame by frame, not redrawn.
- Drawn on top of the captures: the cursor, a white cover that reveals the 17 typed dots of
  the real "typed" still one at a time, the CSV sheet (its text is the real downloaded
  file), the RuSim mark (the app's own BrandMark SVG) and the QR card.
- Captions sit in a bottom band, as in the fframes display video, in the app's font (Chivo).
  No music, by request: clicks, typing ticks, CSV line ticks, two whooshes and one swell,
  all from `video-demo/media/` (three tap pitches rotate so repeats differ).

## Measured, and only looked at

- `check`: passes, no errors.
- The walk-to-end-card seam: the frames at 130.567 and 130.6 s are identical (0 pixels
  differ; the mark alone at 960, 460).
- Clip-to-still handovers: same files and same relative timing as the first cut, where
  they differed in at most 0.21% of pixels. At the hand-off (16.4 s), where the camera is
  moving, the frame pair across the handover differs as much as two frames inside the
  clip (1.58% vs 1.62%), so the change is camera motion only.
- The QR code decodes to `https://unpadrumensim.netlify.app` from the rendered final frame
  (zxing-cpp), also after scaling it down to 640 x 360.
- QA frames from `SPEC.md`, the review sheets, the seam sheet and the phone sheet were
  looked at.
- Sound: placed from measured peaks (tap 0 s, whoosh 0.762 s, swell 0.706 s), one frame
  after each picture. Nobody has heard it. The rendered file's loudness is not measured yet
  (`npx hyperframes audio <render> --json` after the render).

## Weakest shot, and what I would change

- 14.6–16.4 s, sign-in hand-off: the real app blanks the login before the dashboard fades
  in, so there is a brief flash of empty ground while the camera is still on the form.
  With more time: start the pull-back during "Verifying token", or record the hand-off
  with a longer lead-in.
- The finished CSV sheet holds about 7 s and caption 12 stays 16.4 s; at double pace they
  are the longest stretches without a camera move. If the cut feels slow there, they are the first to trim.
- P2: when a clip hands over to its still, the hover tint under the cursor disappears
  (29.1, 105.6 and 112.4 s), because the stills were taken with the mouse parked.

## Warnings kept on purpose

- `timeline_track_too_dense` and `composition_file_too_large` (walk.html, 32 timed
  elements): the film is one continuous camera take, so it is one scene.
- `nested_media_start_basis_ambiguous` (8 videos): the times are local to the walk scene,
  and that scene starts at 0, so local and global time agree.

## Facts to keep in mind

- The end-card URL (variable `url` in `compositions/endcard.html`) and the QR code point at
  the Netlify site, which still serves the 2026-10-01 deploy, an older look than this
  video. Changing the URL means running `capture/build-qr.py <url>` too.
- `video-demo/.gitignore` ignores `*.mp4` and `*.wav`, so the recorded clips and the
  sounds are not tracked. They come back from `capture/capture.ts` and
  `capture/build-sfx.sh` (from `video-demo/media/`).
