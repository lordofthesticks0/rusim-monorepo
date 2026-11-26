# Storylines: RuSim dashboard demo

Subject: the `dashboard/` web app as it is on `master` (login screen, Monitoring,
Experiment history, Compare). Facts come from `docs/dashboard.md` and the running app.
Pace for all three: slow but never idle. Each step holds 4–6 s: the camera arrives
(0.9–1.2 s, eased), the cursor acts, the real UI answers with its own animation, and one
short line names what happened. No dead holds, no repeated transition, no slow-motion typing.

Shared assumptions: 1920×1080, 30 fps, no voiceover, short on-screen lines in the
dashboard's own font (Chivo, Chivo Mono). Real screens only, captured from the running
app at 2× so close-ups stay sharp. The app's own motion (chart draw, KPI tick, nav pill
slide, sign-in hand-off) is captured frame by frame from the app, not redrawn.

---

## A. One chamber, start to finish (recommended)

Recommended because a stranger follows one operator doing one real check through every view.

- **Device / form:** the worked example (one chamber's readings travel through every view)
  / one continuous take (the camera never cuts; it glides over the real screens and
  pushes into each control the cursor touches).
- **What the viewer sees and feels:** An operator signs in with the device token, sees all
  24 chambers at a glance, picks one, and follows its methane reading from the live KPI
  to the chart, into a finished experiment's results and CSV, and onto a comparison with
  another run. It feels like sitting next to someone who knows the tool and shows it
  calmly, one click at a time.
- **First 2 s:** frame 0 is the real login screen at full size, the fermentation curve
  already drawing across the identity pane; the large cursor glides in from the right
  toward the token field; the line "Every chamber, from any browser" lands top left.
- **Beats:** login, curve draws → token typed, "Sign in" → dashboard fades in (the app's own
  hand-off) → push into the 24-chamber strip, silent chambers in amber → click a chamber,
  five sensor values tick in → click CH₄, chart line draws, hover a point → Experiment
  history: pick a run, Results table, CSV download → Compare: add the same chamber from
  another run, two lines draw → pull back to the whole sheet → claim "Watch it live. Look
  back. Compare." → end card (RuSim mark carried in, URL).
- **Look:** the app's own off-white ground `#f7f7f5` and white sheet, Chivo + Chivo Mono,
  black ink, accent blue `#2563eb` kept for the click ring and one word per line;
  material: the real UI at 2×, close-ups lifted on the sheet's 1px edge.
- **Sound:** calm, warm ambient bed (~80 BPM) faded in and out; soft clicks on each press,
  a light air whoosh on each camera travel.
- **Length:** 70–85 s.

## B. Four questions at the bench

- **Device / form:** a moment in someone's day (a lab operator's routine check, each step
  opened by the question they would ask) / object-to-object chain (each answer ends on the
  element that opens the next question).
- **What the viewer sees and feels:** Large questions on the plain brand ground, each answered
  by the real screen: the amber cell becomes the disconnection notice, the CH₄ cell
  becomes the chart, the experiment row becomes the Results table, a table row becomes a
  Compare chip. It feels like a calm routine where every question gets a direct answer.
- **First 2 s:** off-white ground, the question "Are all 24 chambers reporting?" types on in
  large Chivo; its last word slides down and becomes the presence strip.
- **Beats:** Q1 "Are all 24 chambers reporting?" → presence strip + notice → Q2 "What is this
  chamber producing?" → KPI strip, CH₄ chart → Q3 "How did the last run end?" → History,
  Results, CSV → Q4 "Is it behaving like last run?" → Compare overlay → claim "Four
  questions. One dashboard." → end card.
- **Look:** same brand ground; questions set very large (about 110 px) in Chivo, answers in
  the real UI; blue only on the element that answers.
- **Sound:** soft piano-led bed; one gentle tick as each question lands; clicks on presses.
- **Length:** 75–90 s.

## C. The silent chamber (the bold one)

- **Device / form:** the object is the explanation (the dashboard's rule: a missing row means
  a chamber went silent, a missing value means one sensor failed, and neither is ever
  drawn as zero) / an endless zoom (the camera dives into a detail that opens the next screen).
- **What the viewer sees and feels:** One silent chamber is followed through the app: the
  striped amber cell, the disconnection notice, the chart line that breaks instead of
  dropping to zero, the dash in the Results table, the empty cell in the exported CSV.
  It feels like a short investigation that ends in trust in the numbers.
- **First 2 s:** an extreme close-up of one striped amber cell "07"; the line "Chamber 07
  went quiet" lands; the camera starts pulling back to reveal 23 neighbours.
- **Beats:** amber cell → pull back to strip + notice → dive into the chart's gap → dive into
  the Results dash → dive into the CSV's empty cell → claim "A gap is never a zero" →
  end card.
- **Look:** same brand ground; the app's amber stripe is the one material and motif; blue
  rationed to the end card.
- **Sound:** sparse low ambient pulse; a soft drop-out tone when the line breaks; clicks.
- **Length:** 45–60 s. Risk: it needs a real gap in a finished experiment for the Results
  and CSV beats; the Preview data has silent chambers 7 and 19 and one empty sensor,
  but the stored experiments may not.
