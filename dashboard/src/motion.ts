// IMPORTANT: For AI agents, ALWAYS update the documentation in docs/dashboard.md whenever any changes are made.

/**
 * Small motion helpers for the dashboard.
 *
 * The CSS in styles.css does almost all of the motion work. These hooks only
 * cover what CSS cannot do by itself: measuring where the selected tab sits so
 * a single indicator can travel to it, and remembering whether the first-load
 * entrance has already played.
 */

import { useEffect, useLayoutEffect, useRef, useState, type CSSProperties } from "react";

/** Selected items are found by their ARIA state, so markup stays the source of truth. */
const SELECTED = '[aria-selected="true"], [aria-current="page"]';

/**
 * Positions one indicator element on the selected child of a container.
 *
 * - `"box"` sets the indicator's width and height to the selected item and moves
 *   it with `translate`, for a background pill (the sidebar navigation).
 * - `"bar"` draws a 2px bar under the selected item. Its width comes from
 *   `scaleX` on a 100px element, so the whole travel is a transform and never
 *   triggers layout.
 *
 * The first placement and any placement caused by a resize "jump" without a
 * transition (`is-jump`), so the indicator only travels when the selection
 * itself changes. `activeKey` is whatever identifies the current selection.
 */
export function useIndicator<T extends HTMLElement>(activeKey: unknown, mode: "box" | "bar") {
  // A callback ref, so a container that unmounts and mounts again (a view
  // switch, a loading state) is measured again as soon as it is back.
  const [container, containerRef] = useState<T | null>(null);
  const placedOn = useRef<T | null>(null);
  const [style, setStyle] = useState<CSSProperties>({ opacity: 0 });
  const [jump, setJump] = useState(true);

  useLayoutEffect(() => {
    if (!container) return;

    const measure = (animate: boolean) => {
      const active = container.querySelector<HTMLElement>(SELECTED);
      if (!active) {
        setStyle({ opacity: 0 });
        return;
      }
      setJump(!animate);
      const next = mode === "box"
        ? { "--x": `${active.offsetLeft}px`, "--y": `${active.offsetTop}px`, "--w": `${active.offsetWidth}px`, "--h": `${active.offsetHeight}px` }
        : { "--x": `${active.offsetLeft}px`, "--y": `${active.offsetTop + active.offsetHeight - 2}px`, "--sx": String(active.offsetWidth / 100) };
      setStyle({ ...next, opacity: 1 } as CSSProperties);
    };

    // Travel only when the selection changed on a container already placed.
    measure(placedOn.current === container);
    placedOn.current = container;

    // A ResizeObserver reports once as soon as it starts observing. That first
    // report is skipped, or it would cut short the travel that just began.
    let first = true;
    const observer = new ResizeObserver(() => {
      if (first) {
        first = false;
        return;
      }
      measure(false);
    });
    observer.observe(container);
    return () => observer.disconnect();
  }, [container, activeKey, mode]);

  return { containerRef, indicatorStyle: style, indicatorClass: jump ? " is-jump" : "" };
}

let entranceDone = false;

/** The entrance takes about 1.1 s; the class is removed a little after that. */
const ENTRANCE_MS = 1400;

/**
 * True during the first moments of the first screen shown in this page load,
 * so the title entrance plays once per visit and never again on a view switch
 * or a re-render. After `ENTRANCE_MS` it turns false, so content that arrives
 * later (for example after a slow fetch) appears without a delayed animation.
 *
 * `replay` plays it again on a screen that mounts with it set: the login
 * screen coming back after a sign-out (brandTransition.ts, phase "return").
 */
export function useEntranceOnce(replay = false): boolean {
  const [entering, setEntering] = useState(() => !entranceDone || replay);
  entranceDone = true;
  useEffect(() => {
    if (!entering) return;
    const timer = window.setTimeout(() => setEntering(false), ENTRANCE_MS);
    return () => window.clearTimeout(timer);
  }, [entering]);
  return entering;
}
