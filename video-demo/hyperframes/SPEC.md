# SPEC: RuSim dashboard demo (storyline A, "One chamber, start to finish")

GOAL:    The viewer has watched one operator sign in, find a silent chamber, follow
         Chamber 3 from its live values into a finished run, export it, and compare it
         across runs, knows the dashboard's three views by name, and can open the site
         from the QR code on the last frame.
OUTPUT:  HyperFrames, 1920 x 1080, 30 fps. No voiceover, no music (person's choice):
         sound effects only.
TIME:    141.6 s x 30 = 4248 frames. Scene `walk` 0–130.6 s (one continuous take),
         scene `endcard` 130.6–141.6 s.
PACE:    Second cut (person's request): every time in `walk` up to the zoom (0–128.8 s)
         is twice the first cut's. At real speed: the app's recorded animations, its quick
         responses (0.10–0.14 s) and the click press. Original speed: the zoom through
         the mark and the end card up to the URL; the QR beat after it is new.

LOOK:    Medium: the real app, captured at 3840 x 2160 (1440 x 810 CSS px at 8/3 density).
         Ground #f7f7f5 (the app's own --ground), flat, no texture, no vignette, no grain.
         Ink #000000, muted #666662, edge #e3e3de. Accent #2563eb only on: the app's own
         pixels, the brand dot of the mark, the end-card URL, the QR scan line. Amber
         #b86200 only on the word "amber" in caption 4 (the app's disconnected colour).
         Display/body face: Chivo (variable 400–700, assets/fonts), mono: Chivo Mono.
         Captions: Chivo 500, 40 px, tracking -0.01em, left edge x = 160, centred on
         y = 996 inside a caption band y 920–1080 (ground fill, 1px #e3e3de top edge).
         Claim line: Chivo 500, 48 px. Depth: none on the UI; the CSV sheet alone gets
         0 18px 48px rgba(0,0,0,0.10).
OBJECTS: The real login screen, Monitoring, Experiment history, Compare (stills and
         frame-exact clips from capture/capture.ts). The cursor: a classic black arrow,
         54 px tall (5% of the frame) with a white outline, constant on-screen size
         at every zoom. The CSV sheet: a white page with the real file name and its first
         12 lines in Chivo Mono 28 px, Chamber 3 rows tinted #eef3fd. The RuSim mark: the
         app's own BrandMark SVG, the wordmark set in Chivo 600. The QR code: version 3-M,
         29 x 29 modules at 10 px, black on a white 372 px card with a 40 px quiet zone
         and a 1px #e3e3de edge (capture/build-qr.py).

MOTION:  Camera: one rig, page point (cx, cy) held at stage centre (960, 460), scale k.
         Every camera move power2.inOut, 2.4–3.2 s; the long chart pan 6.4 s. A hold over
         3 s creeps in by 2.5% (sine.inOut) about the stage centre; the last one about
         the RuSim mark. The zoom through the mark power3.in, 1.8 s.
         Cursor: x on power2.inOut, y on power3.inOut (a slight arc), 1.2–2.0 s a move;
         shown and hidden in 0.3–0.5 s; press = scale 0.86 in 0.07 s, back in 0.14 s.
         No click rings.
         UI motion: only the app's own (recorded clips, real speed). Small state changes
         the app does instantly cross over in 0.10–0.14 s.
         Captions: land in <= 1.4 s, then dead still; exit 0.3 s (blur 6 px + fade).
         Entrances rotate: per-word blur-in (6 px, 0.26 s, 0.134 s apart); per-word grey
         to ink (0.24 s, 0.16 s apart); whole-line blur-in (8 px, 0.36 s).
         End card: mark slides to the lockup on expo.out 0.7 s; wordmark wipes in
         (clip-path, power3.out 0.6 s); lines per word grey to ink; URL types in.
         Then the lockup steps 260 px left (power3.inOut 1.2 s), the QR card opens from
         its middle line (clip-path, power3.out 0.7 s) and a scan line uncovers the code
         (power1.inOut 1.2 s).

SHOTS (seconds; camera states as (cx, cy, k)):
  0.0–4.4   Login wide (720,405,1.12), clip from 0.6 s: curve draws, bubbles rise.
            Cap 1 "Every chamber, from any browser" at 0.5.
  4.4–6.8   Camera to the form (1097,440,1.85). Cursor enters from the right 5.8–7.8.
  7.9       Click the token field (tap). 8.3–10.92 17 dots type in, 164 ms apart (typing
            ticks); the button turns blue on the first dot. Cap 2 "Sign in with the
            device token" at 9.1.
  13.0      Click "Sign in" (tap) -> "Verifying token" (still) until 14.6.
  14.6–16.4 The app's own hand-off clip; camera pulls back to (720,405,1.12) 14.6–17.2,
            then creeps 17.2–21.2. Cap 3 "The live experiment, at a glance" at 16.0.
  21.2–23.8 Camera to strip + KPIs (836,470,1.45). Cap 4 "All 24 chambers. Silent ones
            turn amber" at 22.0. Cursor points at amber 07 (23.0–24.6), then moves to 03.
  27.7      Click 03 (tap): clip 1.4 s, cell turns black, five values tick in.
            Cap 5 "Pick one to read its five sensors" at 28.2. Creep 29.2–34.0.
  34.0–36.4 Camera to the nav (380,300,1.6). 36.9 click "Experiment history" (tap),
            clip 0.6 s. Cap 6 "Look back at past experiments" at 37.4.
  38.1–40.6 Camera to the list (836,380,1.55). 42.9 click Experiment 4 (tap).
  43.2–46.4 Camera down to the detail (836,905,1.35). Cap 7 "Open a run, then pick the
            chamber" at 43.8. 47.1 pick Chamber 3 (tap): clip 1.1 s, pH chart draws.
  50.6–57.0 Pan down the five charts to CH4 (836,2131,1.55). Cap 8 "Every sensor,
            every reading of the run" at 51.2.
  60.0      Hover the lowest CH4 point: tooltip. Push to (620,2185,2.0) 60.4–63.2,
            creep 63.2–66.8. Cap 9 "Hover any point for its value and time" at 60.4.
  66.8–70.0 Camera back to the tabs (836,760,1.55). 72.1 click "Results" (tap): clip.
            Cap 10 "Mean, min and max for every chamber" at 72.8. Camera to the table
            (836,1037,1.55) 73.2–75.8.
  80.0–82.6 Camera to the CSV button (836,676,1.55). 83.1 click (tap): the button grows
            into the CSV sheet (1.2 s), lines arrive 140 ms apart from 84.3 (ticks);
            holds to 92.8, shrinks back 0.9 s. Cap 11 "Export exactly the rows you see
            as CSV" at 83.8.
  93.8–96.2 Camera to the nav. 96.5 click "Compare" (tap): clip.
            Cap 12 "Overlay chambers from any experiment" at 97.2.
  97.8–100.2 Camera to the selection (836,440,1.45). 102.3 pick Chamber 3; 104.3 "Add
            chamber": clip, blue series draws. 109.1 pick Experiment 2; 111.1 "Add
            chamber": clip, green series draws (all clicks tap).
  113.8–116.4 Camera to the chart (836,610,1.5), creep 116.4–121.2. Cap 13 "Chamber 3
            in two runs, on one chart" at 114.8.
  121.2–124.0 Pull back wide (720,425,1.12). Cap 14 (claim) "Watch it live. Look back.
            Compare." at 122.0. Creep about the mark to (703.27,415.46,1.148) 124.0–128.8:
            the mark stays at (191.7, 22.1) on screen, fully in frame.
  128.8–130.6 Zoom through the sidebar mark in two parts: centre it (34,34,1.5)
            128.8–129.7, then zoom about it to (34,34,6.0) 129.7–130.5 (power3.in) and
            hold. A single move swung the mark off-screen. The vector mark takes over at
            129.8; the screen fades out 130.1–130.5. Whoosh peaking at 130.5.
  130.6–134.6 End card: mark carried into the lockup (swell peaking at 130.85), "RuSim /
            In Vitro Rumen Simulator", then "In Vitro Rumen Fermentation Dashboard",
            "IAIS Lab · Teknik Elektro · Universitas Padjadjaran" with the Unpad emblem,
            then the URL unpadrumensim.netlify.app types in 132.7–134.1.
  134.6–141.6 The lockup steps left; 135.1 the QR card opens (soft whoosh peaking at
            135.3); 135.5–136.7 the scan line uncovers the code; 136.7 "Scan to open the
            dashboard". Holds 5 s.

BANNED:  dark void; glow, particles, gradients, glass; centred layouts (end card excepted);
         every element fading and sliding up; text that moves while read; corner labels,
         frame borders, HUD readouts; three cards in a row; title over a dimmed
         screenshot; the logo docked in a corner; click rings; the same caption entrance
         three times running; redrawn UI (only captured pixels, the cursor, the CSV sheet,
         the mark and the QR card are drawn); CSS blur or filters on the captured screens.

QA:      0.0 (thumbnail: full login, curve mid-draw, no black); 9.6 (dots mid-typing,
         button blue); 14.8 (hand-off frame); 25.0 (amber cells and caption 4 readable);
         31.0 (03 black, five values); 48.6 (pH chart drawn); 63.8 (tooltip legible);
         77.0 (results table legible); 87.0 (CSV sheet, Chamber 3 rows tinted); 117.6
         (two series and legend); 130.567 / 130.6 (seam into the end card, mark identical
         both sides); 135.9 (scan line mid-code); 141.5 (final frame: the QR decodes to
         https://unpadrumensim.netlify.app).
