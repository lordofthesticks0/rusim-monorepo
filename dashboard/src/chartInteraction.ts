/**
 * Pure helpers for chart hover and keyboard interaction.
 *
 * A chart has one invisible hit area instead of one focusable element per
 * sample. A day at 30 s is 2,880 samples per chamber, and History draws five
 * charts, so per-sample elements meant about 14,400 interactive nodes, each
 * with a locale-formatted label; building them froze the page for about a
 * second. The hit area maps a pointer position to the nearest sample instead,
 * and the chart takes one Tab stop with arrow keys to step between samples.
 */

/**
 * Minimum horizontal gap, in px, between samples for a dot to be drawn on
 * each one. Closer than this the dots merge into a band, so only the line and
 * the active point are drawn.
 */
export const MIN_MARKER_SPACING = 6;

/** Whether `count` samples across `plotWidth` px are far enough apart to dot each one. */
export function showMarkers(count: number, plotWidth: number): boolean {
  return count <= 1 || plotWidth / (count - 1) >= MIN_MARKER_SPACING;
}

/** Converts a pointer's clientX into viewBox x for an SVG drawn at `rect`. */
export function viewBoxX(clientX: number, rect: { left: number; width: number }, viewBoxWidth: number): number {
  return rect.width > 0 ? ((clientX - rect.left) / rect.width) * viewBoxWidth : 0;
}

/** Converts a pointer's clientY into viewBox y for an SVG drawn at `rect`. */
export function viewBoxY(clientY: number, rect: { top: number; height: number }, viewBoxHeight: number): number {
  return rect.height > 0 ? ((clientY - rect.top) / rect.height) * viewBoxHeight : 0;
}

/**
 * The sample nearest to horizontal position `x`, for `count` samples spread
 * evenly from `plotLeft` to `plotRight` (how every chart here places them).
 * Positions outside the plot clamp to the first or last sample. Null when
 * there are no samples.
 */
export function nearestIndex(x: number, count: number, plotLeft: number, plotRight: number): number | null {
  if (count <= 0) return null;
  if (count === 1 || plotRight <= plotLeft) return 0;
  const fraction = (x - plotLeft) / (plotRight - plotLeft);
  return Math.min(count - 1, Math.max(0, Math.round(fraction * (count - 1))));
}

/**
 * The sample a navigation key moves to, or null when the key is not a chart
 * key. ← and → step one sample, Page Up and Page Down a tenth of the series,
 * Home and End jump to the ends. Starting with nothing active, ← or End
 * starts at the newest sample and → or Home at the oldest.
 */
export function stepIndex(key: string, index: number | null, count: number): number | null {
  if (count <= 0) return null;
  const last = count - 1;
  const page = Math.max(1, Math.round(count / 10));
  const clamp = (value: number) => Math.min(last, Math.max(0, value));
  switch (key) {
    case "ArrowRight": return index === null ? 0 : clamp(index + 1);
    case "ArrowLeft": return index === null ? last : clamp(index - 1);
    case "PageDown": return index === null ? 0 : clamp(index + page);
    case "PageUp": return index === null ? last : clamp(index - page);
    case "Home": return 0;
    case "End": return last;
    default: return null;
  }
}

/**
 * Of several series' candidate points at the pointer's x, the one whose y is
 * closest to the pointer's y. Null entries (no sample, or no value) are
 * skipped; returns null when every entry is null.
 */
export function nearestSeries(pointerY: number, ys: Array<number | null>): number | null {
  let best: number | null = null;
  let bestDistance = Number.POSITIVE_INFINITY;
  ys.forEach((y, index) => {
    if (y === null) return;
    const distance = Math.abs(y - pointerY);
    if (distance < bestDistance) {
      best = index;
      bestDistance = distance;
    }
  });
  return best;
}
