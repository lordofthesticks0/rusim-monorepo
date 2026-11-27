// IMPORTANT: For AI agents, ALWAYS update the documentation in docs/dashboard.md whenever any changes are made.

/**
 * The logout and sign-in transitions, modelled on the outro of the product
 * video (video-demo/hyperframes): the camera zooms through the sidebar's RuSim
 * mark until the mark stands alone in the centre, then the wordmark opens
 * under it.
 *
 * Everything plays on one full-screen layer, the stage (`BrandStage` in
 * App.tsx). The stage's own CSS lays out the finished lockup: the mark, "RuSim"
 * and "In Vitro Rumen Simulator", centred on the page ground. Each phase
 * measures where the pieces must come from or go to, then moves them with the
 * Web Animations API (`element.animate()`). A phase returns a `Run`. App starts
 * the next phase when its `finished` promise resolves.
 *
 *   Logout   "outro"   zoom through the sidebar mark, then the wordmark opens.
 *            "return"  the lockup glides into the login form's logo while the
 *                      stage fades and the login screen plays its entrance.
 *   Sign-in  "lift"    the login logo glides out to the centred lockup while
 *                      the stage covers the login screen.
 *            "arrive"  the wordmark clears and the dashboard zooms out of the
 *                      sidebar mark, which the centred mark shrinks into.
 *
 * Elements are found by data attributes, so the markup in App.tsx stays the
 * source of truth: `data-brand-anchor="sidebar"` (the sidebar BrandMark),
 * `data-brand-anchor="login"` (the login RusimLogo), and inside those and the
 * stage `data-brand-part="mark" | "word" | "sub"` (plus "ground" on the stage).
 */

export type BrandPhase = "outro" | "return" | "lift" | "arrive";

/** The animations of one phase. `finished` resolves when all of them end and rejects if they are cancelled. */
export type Run = { finished: Promise<void>; cancel: () => void };

export type Point = { x: number; y: number };
export type Box = { left: number; top: number; width: number; height: number };

/**
 * A translate-then-scale transform. The element's transform-origin must be the
 * reference point the shift was computed for (named in each helper below).
 */
export type Shift = { x: number; y: number; scale: number };

/** Origin at the square's top-left: moves a square `box` so its centre is `centre` and its side is `size`. */
export function squareAt(box: Box, centre: Point, size: number): Shift {
  return { x: centre.x - size / 2 - box.left, y: centre.y - size / 2 - box.top, scale: size / box.width };
}

/** Origin at `box`'s top-left: lays `box` over `target`, scaled by their height ratio (text scales with font size). */
export function boxOnto(box: Box, target: Box): Shift {
  return { x: target.left - box.left, y: target.top - box.top, scale: target.height / box.height };
}

/** Origin at `from`: a camera move that carries the screen point `from` to `to` and magnifies by `scale` about it. */
export function cameraShift(from: Point, to: Point, scale: number): Shift {
  return { x: to.x - from.x, y: to.y - from.y, scale };
}

export function centreOf(box: Box): Point {
  return { x: box.left + box.width / 2, y: box.top + box.height / 2 };
}

const css = ({ x, y, scale }: Shift) => `translate(${x}px, ${y}px) scale(${scale})`;
const IDENTITY = css({ x: 0, y: 0, scale: 1 });

/** The video's GSAP eases as cubic-bezier curves. */
const EASE = {
  inOut: "cubic-bezier(0.65, 0, 0.35, 1)", // power2.inOut
  in3: "cubic-bezier(0.32, 0, 0.67, 0)", // power3.in
  out3: "cubic-bezier(0.33, 1, 0.68, 1)", // power3.out
  out2: "cubic-bezier(0.5, 1, 0.89, 1)", // power2.out
  expoOut: "cubic-bezier(0.16, 1, 0.3, 1)", // expo.out
  in: "cubic-bezier(0.5, 0, 0.75, 0)",
  out: "cubic-bezier(0.2, 0.8, 0.2, 1)", // --ease in styles.css
} as const;

/** Durations in milliseconds. */
const MS = {
  centre: 380, // outro, part 1: the mark travels to the screen centre at 1.5x
  zoom: 420, // outro, part 2: zoom about the centre until the mark is the stage mark's size
  settle: 650, // the mark slides up into the lockup
  word: 550, // "RuSim" opens from its centre line
  wordDelay: 220,
  sub: 260, // "In Vitro Rumen Simulator" blurs in
  subDelay: 420,
  hold: 320, // the finished lockup holds before the login screen comes back
  land: 700, // lockup to login logo
  lift: 520, // login logo to lockup
  stagger: 45, // between the pieces of the lockup when it rearranges
  arrive: 700, // zoom out of the sidebar mark
} as const;

/** WAAPI animations are not CSS animations, so the global reduced-motion rule in styles.css does not stop them. App checks this first. */
export function prefersReducedMotion(): boolean {
  return window.matchMedia("(prefers-reduced-motion: reduce)").matches;
}

export function playPhase(phase: BrandPhase, stage: HTMLElement): Run {
  const play = { outro: playOutro, return: playReturn, lift: playLift, arrive: playArrive }[phase];
  return play(stage);
}

const PIECES = ["mark", "word", "sub"] as const;
type Pieces = Record<(typeof PIECES)[number], Element>;

function findPieces(root: ParentNode | null): Pieces | null {
  if (!root) return null;
  const [mark, word, sub] = PIECES.map((name) => root.querySelector(`[data-brand-part="${name}"]`));
  return mark && word && sub ? { mark, word, sub } : null;
}

function group(animations: Animation[]): Run {
  return {
    finished: Promise.all(animations.map((animation) => animation.finished)).then(() => undefined),
    cancel: () => animations.forEach((animation) => animation.cancel()),
  };
}

/**
 * The visible box of a piece: the element's own box for the mark, the glyph box
 * of the text for a word. A Range measures the text itself and ignores
 * line-height, so a 17px word and an 80px word compare by font size alone.
 */
function glyphBox(element: Element): DOMRect {
  if (element instanceof SVGElement) return element.getBoundingClientRect();
  const range = document.createRange();
  range.selectNodeContents(element);
  return range.getBoundingClientRect();
}

/** What it takes to lay a stage piece over `target`: the transform-origin (its glyph box's top-left) and the shift. */
function flight(piece: Element, target: Box): { transformOrigin: string; shift: Shift } {
  const own = piece.getBoundingClientRect();
  const box = glyphBox(piece);
  return { transformOrigin: `${box.left - own.left}px ${box.top - own.top}px`, shift: boxOnto(box, target) };
}

/** The sidebar mark, if it is on screen. The phone drawer may be closed and off to the left. */
function sidebarMark(view: DOMRect): Element | null {
  const mark = document.querySelector('[data-brand-anchor="sidebar"]');
  const box = mark?.getBoundingClientRect();
  if (!mark || !box || box.width === 0) return null;
  const visible = box.right > view.left && box.left < view.right && box.bottom > view.top && box.top < view.bottom;
  return visible ? mark : null;
}

type CameraFrame = { shift: Shift; offset?: number; easing?: string };

/**
 * Moves the dashboard like a camera: every top-level box of `.app` gets the same
 * transform about the same screen point `from`. Each box moves on its own,
 * because a transform on their shared parent would make that parent the
 * containing block of the phone drawer and backdrop (position: fixed), and they
 * would jump when the page is scrolled.
 */
function camera(app: Element | null, from: Point, frames: CameraFrame[], timing: KeyframeAnimationOptions): Animation[] {
  if (!app) return [];
  return Array.from(app.children, (layer) => {
    const box = layer.getBoundingClientRect();
    const transformOrigin = `${from.x - box.left}px ${from.y - box.top}px`;
    return layer.animate(frames.map(({ shift, ...rest }) => ({ transformOrigin, transform: css(shift), ...rest })), timing);
  });
}

function stageGround(stage: HTMLElement): Element | null {
  return stage.querySelector('[data-brand-part="ground"]');
}

/** Logout, part 1: the video's zoom through the mark, then its end card. */
function playOutro(stage: HTMLElement): Run {
  const pieces = findPieces(stage);
  const ground = stageGround(stage);
  if (!pieces || !ground) return group([]);

  const view = stage.getBoundingClientRect();
  const centre = centreOf(view);
  const home = pieces.mark.getBoundingClientRect(); // the mark's place in the finished lockup
  const zoomEnd = MS.centre + MS.zoom;
  const markEnd = zoomEnd + MS.settle;
  const animations: Animation[] = [];
  const anchor = sidebarMark(view);

  if (anchor) {
    // Two parts, as in the video, so the mark travels straight to the centre:
    // first centre it with a slight zoom, then zoom about that fixed point.
    // The stage mark rides on top of the sidebar mark the whole way, so the
    // mark stays sharp while the dashboard under it is magnified.
    const start = anchor.getBoundingClientRect();
    const from = centreOf(start);
    const at = (shift: Shift) => ({ transformOrigin: "0 0", transform: css(shift) });
    animations.push(pieces.mark.animate([
      { ...at(squareAt(home, from, start.width)), easing: EASE.inOut },
      { ...at(squareAt(home, centre, start.width * 1.5)), offset: MS.centre / markEnd, easing: EASE.in3 },
      // Landed alone in the centre; then it slides up into the lockup.
      { ...at(squareAt(home, centre, home.width)), offset: zoomEnd / markEnd, easing: EASE.expoOut },
      { transformOrigin: "0 0", transform: IDENTITY },
    ], { duration: markEnd, fill: "both" }));
    animations.push(...camera(anchor.closest(".app"), from, [
      { shift: cameraShift(from, from, 1), easing: EASE.inOut },
      { shift: cameraShift(from, centre, 1.5), offset: MS.centre / zoomEnd, easing: EASE.in3 },
      { shift: cameraShift(from, centre, home.width / start.width) },
    ], { duration: zoomEnd, fill: "forwards" }));
    // The dashboard fades out during the second half of the zoom.
    animations.push(ground.animate([
      { opacity: 0 },
      { opacity: 0, offset: 0.55, easing: EASE.in },
      { opacity: 1, offset: 0.92 },
      { opacity: 1 },
    ], { duration: zoomEnd, fill: "both" }));
  } else {
    // No mark to zoom through: the ground covers the page and the mark fades in at the centre.
    const centred = { transformOrigin: "0 0", transform: css(squareAt(home, centre, home.width)) };
    animations.push(pieces.mark.animate([
      { ...centred, opacity: 0 },
      { ...centred, opacity: 1, offset: zoomEnd / markEnd, easing: EASE.expoOut },
      { transformOrigin: "0 0", transform: IDENTITY, opacity: 1 },
    ], { duration: markEnd, fill: "both" }));
    animations.push(ground.animate([{ opacity: 0 }, { opacity: 1 }], { duration: zoomEnd / 2, easing: EASE.out, fill: "both" }));
  }

  // The wordmark opens from its centre line, then the subtitle blurs in.
  animations.push(pieces.word.animate(
    [{ clipPath: "inset(0% 50% 0% 50%)" }, { clipPath: "inset(0% 0% 0% 0%)" }],
    { duration: MS.word, delay: zoomEnd + MS.wordDelay, easing: EASE.out3, fill: "both" },
  ));
  animations.push(pieces.sub.animate(
    [{ opacity: 0, filter: "blur(8px)" }, { opacity: 1, filter: "blur(0px)" }],
    { duration: MS.sub, delay: zoomEnd + MS.subDelay, easing: EASE.out2, fill: "both" },
  ));
  // An empty animation holds the finished lockup for a moment.
  const shown = Math.max(markEnd, zoomEnd + MS.wordDelay + MS.word, zoomEnd + MS.subDelay + MS.sub);
  animations.push(stage.animate(null, { duration: shown + MS.hold }));
  return group(animations);
}

/** Logout, part 2: the lockup glides into the login logo while the login screen comes back. */
function playReturn(stage: HTMLElement): Run {
  window.scrollTo(0, 0);
  const pieces = findPieces(stage);
  const ground = stageGround(stage);
  if (!pieces || !ground) return group([]);

  const logo = findPieces(document.querySelector('[data-brand-anchor="login"]'));
  const animations: Animation[] = [];
  PIECES.forEach((name, index) => {
    const piece = pieces[name];
    if (logo) {
      // The vertical lockup becomes the horizontal logo, so the paths cross.
      // The mark leaves first (mark, word, sub), which keeps it clear of the
      // word while it shrinks.
      const { transformOrigin, shift } = flight(piece, glyphBox(logo[name]));
      animations.push(piece.animate(
        [{ transformOrigin, transform: IDENTITY }, { transformOrigin, transform: css(shift) }],
        { duration: MS.land, delay: index * MS.stagger, easing: EASE.inOut, fill: "both" },
      ));
    } else {
      animations.push(piece.animate([{ opacity: 1 }, { opacity: 0 }], { duration: 300, easing: EASE.out, fill: "both" }));
    }
  });
  animations.push(ground.animate([{ opacity: 1 }, { opacity: 0 }], { duration: 560, delay: 120, easing: EASE.out, fill: "both" }));
  return group(animations);
}

/** Sign-in, part 1: the login logo glides out into the centred lockup and the stage covers the login screen. */
function playLift(stage: HTMLElement): Run {
  const pieces = findPieces(stage);
  const ground = stageGround(stage);
  if (!pieces || !ground) return group([]);

  const logo = findPieces(document.querySelector('[data-brand-anchor="login"]'));
  const animations: Animation[] = [];
  PIECES.forEach((name, index) => {
    const piece = pieces[name];
    if (logo) {
      // The return in reverse: the text leaves first (sub, word, mark) and
      // moves down out of the way before the mark grows over its old place.
      const { transformOrigin, shift } = flight(piece, glyphBox(logo[name]));
      animations.push(piece.animate(
        [{ transformOrigin, transform: css(shift) }, { transformOrigin, transform: IDENTITY }],
        { duration: MS.lift, delay: (PIECES.length - 1 - index) * MS.stagger, easing: EASE.inOut, fill: "both" },
      ));
    } else {
      animations.push(piece.animate([{ opacity: 0 }, { opacity: 1 }], { duration: MS.lift, easing: EASE.out, fill: "both" }));
    }
  });
  animations.push(ground.animate([{ opacity: 0 }, { opacity: 1 }], { duration: 420, easing: EASE.out, fill: "both" }));
  return group(animations);
}

/** Sign-in, part 2: the logout zoom in reverse, out of the sidebar mark. */
function playArrive(stage: HTMLElement): Run {
  window.scrollTo(0, 0);
  const pieces = findPieces(stage);
  const ground = stageGround(stage);
  if (!pieces || !ground) return group([]);

  const view = stage.getBoundingClientRect();
  const home = pieces.mark.getBoundingClientRect();
  const homeCentre = centreOf(home);
  const animations: Animation[] = [];

  // The wordmark clears first, so only the mark goes back into the dashboard.
  for (const piece of [pieces.word, pieces.sub]) {
    animations.push(piece.animate(
      [{ opacity: 1, filter: "blur(0px)" }, { opacity: 0, filter: "blur(6px)" }],
      { duration: 200, easing: EASE.in, fill: "both" },
    ));
  }

  const anchor = sidebarMark(view);
  if (anchor) {
    // Zoom out about the centre to 1.5x first, then travel to the sidebar:
    // the outro's two parts in reverse order.
    const end = anchor.getBoundingClientRect();
    const to = centreOf(end);
    const split = 0.45;
    const at = (shift: Shift) => ({ transformOrigin: "0 0", transform: css(shift) });
    animations.push(pieces.mark.animate([
      { ...at(squareAt(home, homeCentre, home.width)), easing: EASE.out3 },
      { ...at(squareAt(home, homeCentre, end.width * 1.5)), offset: split, easing: EASE.inOut },
      at(squareAt(home, to, end.width)),
    ], { duration: MS.arrive, fill: "both" }));
    animations.push(...camera(anchor.closest(".app"), to, [
      { shift: cameraShift(to, homeCentre, home.width / end.width), easing: EASE.out3 },
      { shift: cameraShift(to, homeCentre, 1.5), offset: split, easing: EASE.inOut },
      { shift: cameraShift(to, to, 1) },
    ], { duration: MS.arrive, fill: "both" }));
    animations.push(ground.animate([
      { opacity: 1, easing: EASE.out },
      { opacity: 0, offset: 0.4 },
      { opacity: 0 },
    ], { duration: MS.arrive, fill: "both" }));
  } else {
    // Phone with the drawer closed: the mark fades and the dashboard settles in about the screen centre.
    const centre = centreOf(view);
    animations.push(pieces.mark.animate(
      [{ transformOrigin: "50% 50%", transform: IDENTITY, opacity: 1 }, { transformOrigin: "50% 50%", transform: css({ x: 0, y: 0, scale: 0.8 }), opacity: 0 }],
      { duration: 300, easing: EASE.in, fill: "both" },
    ));
    animations.push(...camera(document.querySelector(".app"), centre, [
      { shift: cameraShift(centre, centre, 1.04), easing: EASE.out3 },
      { shift: cameraShift(centre, centre, 1) },
    ], { duration: 500, fill: "both" }));
    animations.push(ground.animate([{ opacity: 1 }, { opacity: 0 }], { duration: 400, delay: 100, easing: EASE.out, fill: "both" }));
  }
  return group(animations);
}
