import { displayChamberNumber } from "./chambers";
import { formatExperimentId } from "./types/domain";

/**
 * Pure helpers for the Compare view.
 *
 * Compare overlays up to four chamber series on one chart. A series is one
 * (experiment, chamber) pair, so chambers may come from different experiments.
 * Keeping the selection rules here (rather than in App.tsx) lets them be
 * tested without rendering.
 */

/** Maximum number of series that can overlay on the Compare chart. */
export const MAX_COMPARE_SERIES = 4;

/**
 * Series colours, in selection order. The first is the chart teal; the other
 * three are saturated blue, orange, and pink so overlapping series stand off
 * from each other and from the teal on a white sheet.
 */
export const COMPARE_SERIES_COLORS = [
  "var(--chart-1)",
  "var(--chart-2)",
  "var(--chart-3)",
  "var(--chart-4)",
] as const;

export type CompareSelection = {
  experimentId: number;
  bottleId: number;
};

/** Whether the candidate is already in the selection (same experiment and chamber). */
export function isDuplicateSelection(
  selections: CompareSelection[],
  candidate: CompareSelection,
): boolean {
  return selections.some(
    (selection) =>
      selection.experimentId === candidate.experimentId &&
      selection.bottleId === candidate.bottleId,
  );
}

/** Whether the candidate can be added: not a duplicate and under the cap. */
export function canAddSelection(
  selections: CompareSelection[],
  candidate: CompareSelection,
): boolean {
  return selections.length < MAX_COMPARE_SERIES && !isDuplicateSelection(selections, candidate);
}

/**
 * Returns a new selection list with the candidate appended, or the original
 * list unchanged when the candidate cannot be added.
 */
export function addSelection(
  selections: CompareSelection[],
  candidate: CompareSelection,
): CompareSelection[] {
  if (!canAddSelection(selections, candidate)) return selections;
  return [...selections, candidate];
}

/** Returns a new selection list without the entry at the given index. */
export function removeSelection(
  selections: CompareSelection[],
  index: number,
): CompareSelection[] {
  return selections.filter((_, i) => i !== index);
}

/** Distinct experiment ids in selection order, for fetching. */
export function distinctExperimentIds(selections: CompareSelection[]): number[] {
  return [...new Set(selections.map((selection) => selection.experimentId))];
}

/** Human label for one series, e.g. "Experiment 3 · Chamber 5". */
export function compareSeriesLabel(selection: CompareSelection): string {
  return `${formatExperimentId(selection.experimentId)} · Chamber ${displayChamberNumber(selection.bottleId)}`;
}

/** Colour for the series at the given position in the selection list. */
export function compareSeriesColor(index: number): string {
  return COMPARE_SERIES_COLORS[index % COMPARE_SERIES_COLORS.length]!;
}
