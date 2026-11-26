# SPEC: RuSim dashboard demo, mobile UI (storyline A, "One chamber, start to finish")

The same film as `../hyperframes/` (desktop UI, second cut), shot on the dashboard's phone
layout. Storyline: `../hyperframes/STORYLINES.md`, A.

GOAL:    The viewer has watched one operator sign in on a phone, find a silent chamber,
         follow Chamber 3 from its live values into a finished run, export it, and compare
         it across runs, knows the three views by name, and can open the site from the QR
         code on the last frame.
OUTPUT:  HyperFrames, 1920 x 1080 (landscape, the person's choice), 30 fps. No voiceover, no
         music: sound effects only.
TIME:    141.6 s x 30 = 4248 frames. Scene `walk` 0–130.6 s (one continuous take), scene
         `endcard` 130.6–141.6 s (the desktop film's end card, unchanged).
PACE:    The desktop second cut's times: every hold, camera move, caption and the CSV sheet
         start at the same second, and every click becomes a tap at the same second. The
         phone adds taps: the menu button before each change of view (35.7, 95.4, 124.4),
         the experiment select in Compare (101.3; the select opens on the newest run) and
         the chart point (60.0; desktop hovered it). The zoom through the mark and the end
         card keep their speed.

LOOK:    The real app at phone size: 390 x 844 CSS px with touch input, captured at device
         pixel ratio 4 (1560 x 3376 per screen). Below 860 px the app folds its sidebar into
         a drawer behind the menu button.
         The phone: that screen with 26 px corners in a plain 12 px black (#000) bezel
         (38 px corners). No status bar, notch, buttons or brand; no shadow. Centred on the
         stage centre (960, 460); the wide shot shows all of it at scale 0.98 (851 px tall).
         Ground #f7f7f5, flat. Ink #000, muted #666662, edge #e3e3de. Accent #2563eb only
         on the app's own pixels, the brand dot, the end-card URL and the QR scan line.
         Amber #b86200 only on the word "amber" in caption 4. Chivo / Chivo Mono.
         Captions as the desktop film: band y 920–1080 (ground, 1px #e3e3de top edge),
         Chivo 500 40 px, left edge x = 160; claim line 48 px.
OBJECTS: The real login, Monitoring, the drawer, Experiment history and Compare (stills and
         frame-exact clips from capture/capture.ts). The touch: a 64 px dot (the same size
         at every zoom), fill rgba(0,0,0,0.2), 3 px white ring, shadow 0 3px 8px
         rgba(0,0,0,0.2). The CSV sheet, the RuSim mark and the QR card as the desktop film.

MOTION:  Camera: rig (cx, cy, k) in screen CSS px, (cx, cy) held at the stage centre. Moves
         power2.inOut, 1.5–3.2 s. cx stays 195 (the phone's middle) except the tooltip push
         (170) and the zoom through the mark (34). Holds over 3 s creep in by 2.5 %.
         Scrolling: the page moves inside the screen (--sy), power2.inOut 1.0–6.4 s, at the
         same times as the desktop film's camera pans. No finger is shown for a scroll.
         Touch: the dot lands 0.4 s before the press (opacity 0 -> 1 and scale 1.4 -> 1 in
         0.25 s, power2.out), presses (0.86 in 0.07 s, back in 0.14 s) and lifts 0.3 s after
         (0.25 s, scale 1.15). It appears only for taps, never travels.
         UI motion: only the app's own, at real speed. A tap clip starts one frame after its
         tap: its first recorded frame carries Chrome's tap highlight for 1/30 s only, which
         reads as a flicker; the still before it holds that frame.
         Captions and end card: as the desktop film.

SHOTS (seconds; camera (cx, cy, k), scroll sy in CSS px):
  0.0–4.4    Wide (195,422,0.98), login clip from 0.6 s. Cap 1 "Every chamber, from your
             phone" at 0.5.
  4.4–6.8    To the form (195,595,2.3). 7.9 tap the field. 8.3–11.1 17 dots, 164 ms apart.
             Cap 2 at 9.1. 13.0 tap "Sign in" -> "Verifying token".
  14.6–16.4  Hand-off clip; back to wide 14.6–17.2, creep 17.2–21.2. Cap 3 at 16.0.
  21.2–23.8  To the chamber grid (195,341,2.35). Cap 4 at 22.0.
  25.6–27.4  Back to wide while the page scrolls to 219: grid and all five sensors on one
             screen. 27.7 tap 03: clip, the cell turns black, the values tick in. Cap 5 at
             28.2. 29.2–31.8 to the sensors (195,650,2.2).
  33.6–35.6  Scroll to 0, camera to the menu (195,200,2.2). 35.7 tap the menu: the drawer
             slides in. 36.9 tap "Experiment history": it slides out, the view swaps. Cap 6
             at 37.4.
  38.1–40.6  To the list (195,355,2.25). 42.9 tap Experiment 4 (the second row; Experiment 5
             is live).
  43.2–46.4  Scroll to 538, camera (195,640,2.25). Cap 7 at 43.8. 47.1 pick Chamber 3: clip,
             the pH chart draws.
  50.6–57.0  Scroll to 1693 down the five charts, camera (195,422,2.0). Cap 8 at 51.2.
  60.0       Tap the lowest CH4 point: tooltip. Push to (170,470,3.2) 60.4–63.2, creep to
             66.8. Cap 9 "Tap any point for its value and time" at 60.4.
  66.8–70.0  Scroll to 538, camera to the tabs (195,470,2.2). 72.1 tap "Results": clip.
             Cap 10 at 72.8. 73.2–75.8 scroll to 720, camera (195,534,1.44); creep to 80.0.
  80.0–82.6  Scroll to 538, camera to the CSV button (195,245,2.3). 83.1 tap: the button
             grows into the CSV sheet (start 0.207 at 1178.5, 342.7), lines from 84.3,
             holds to 92.8, shrinks back 0.9 s. Cap 11 at 83.8.
  93.8–95.3  Scroll to 0, camera to the menu. 95.4 tap the menu; 96.5 tap "Compare". Cap 12
             at 97.2.
  97.8–100.2 To the selection (195,380,2.3). 101.3 pick Experiment 4; 102.3 pick Chamber 3;
             104.3 "Add chamber": clip, the chip appears (the chart is below the fold).
  105.6–107.6 Scroll to 135, camera (195,472,1.22): the controls and the blue series.
             109.1 pick Experiment 2; 111.1 "Add chamber": clip, the green series draws.
  113.8–116.4 Scroll to 205, camera to the chart (195,630,2.2), creep to 121.2. Cap 13 at
             114.8.
  121.2–124.0 Scroll to 0, back to wide. Cap 14 (claim) at 122.0. 124.4 tap the menu: the
             drawer shows the three views of the claim. Creep about the drawer's RuSim mark
             to (191.09,412.5,1.0045) 124.0–128.8: the mark stays at (802.2, 79.8).
  128.8–130.6 Zoom through the mark (34,34,1.5) then (34,34,6.0) power3.in; the vector mark
             takes over at 129.8; the phone fades 130.1–130.5. Whoosh peaking at 130.5.
  130.6–141.6 End card, unchanged: lockup, URL, then the QR card.

BANNED:  As the desktop film, plus: a drawn status bar, notch, home indicator or device
         brand; a hand or finger image; swipe arrows; the touch dot travelling between taps.

QA:      0.0 (full phone, login); 9.6 (dots mid-typing, button blue); 13.5 ("Verifying
         token"); 23.0 (amber cells, caption 4); 31.0 (03 black, five values); 35.9 (drawer
         open); 48.6 (pH chart drawn); 63.8 (tooltip legible); 78.0 (results table); 87.0
         (CSV sheet); 107.0 (blue series); 117.6 (two series and legend); 127.0 (drawer
         with the three views); 130.567 / 130.6 (seam, identical); 141.5 (the QR decodes to
         https://unpadrumensim.netlify.app).
