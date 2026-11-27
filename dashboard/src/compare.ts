import { displayChamberNumber } from "./chambers";
import { formatExperimentId } from "./types/domain";

/**
 * Pure helpers for the Compare view.
 *
 * Compare overlays up to four chamber series on one chart. A series is one
 * (experiment, chamber) pair, so chambers may come from different experiments.
 * Each series is an editable row in the UI; these helpers keep the rows
 * distinct as they are added and edited.
 * Keeping the selection rules here (rather than in App.tsx) lets them be
 * tested without rendering.
 */

/** Maximum number of series that can overlay on the Compare chart. */
export const MAX_COMPARE_SERIES = 4;

/**
 * Series colours, in selection order, read from the `--chart-1` to `--chart-4`
 * tokens in styles.css: the blue accent first, then green, amber, and violet,
 * so overlapping series stand off from each other on a white sheet.
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

/**
 * The first chamber in `chamberIds` that no other row uses with `experimentId`,
 * searching from `preferred` onward and then wrapping around. Row `ignoreIndex`
 * (the row being edited) does not count as using its own chamber. Returns null
 * when every chamber is taken.
 */
export function firstFreeChamber(
  selections: CompareSelection[],
  experimentId: number,
  chamberIds: number[],
  preferred?: number,
  ignoreIndex?: number,
): number | null {
  const others = selections.filter((_, index) => index !== ignoreIndex);
  const start = preferred === undefined ? 0 : Math.max(0, chamberIds.indexOf(preferred));
  for (let step = 0; step < chamberIds.length; step++) {
    const bottleId = chamberIds[(start + step) % chamberIds.length]!;
    if (!isDuplicateSelection(others, { experimentId, bottleId })) return bottleId;
  }
  return null;
}

/**
 * The row the `Add chamber` button appends: the last row's experiment (or
 * `fallbackExperimentId` when there are no rows) with its next free chamber.
 * Returns null at the series cap, without an experiment, or when that
 * experiment has no free chamber.
 */
export function nextSelection(
  selections: CompareSelection[],
  fallbackExperimentId: number | null,
  chamberIds: number[],
): CompareSelection | null {
  if (selections.length >= MAX_COMPARE_SERIES) return null;
  const last = selections.at(-1);
  const experimentId = last?.experimentId ?? fallbackExperimentId;
  if (experimentId === null) return null;
  const preferred = last ? chamberIds[chamberIds.indexOf(last.bottleId) + 1] : undefined;
  const bottleId = firstFreeChamber(selections, experimentId, chamberIds, preferred);
  return bottleId === null ? null : { experimentId, bottleId };
}

/**
 * Changes one row in place, keeping every row distinct.
 *
 * - A chamber change that would duplicate another row is refused (the list is
 *   returned unchanged); the UI also disables those options.
 * - An experiment change keeps the row's chamber when it is free in the new
 *   experiment, and otherwise moves to the next free one. With none free the
 *   change is refused.
 */
export function updateSelection(
  selections: CompareSelection[],
  index: number,
  patch: Partial<CompareSelection>,
  chamberIds: number[],
): CompareSelection[] {
  const current = selections[index];
  if (!current) return selections;
  const experimentId = patch.experimentId ?? current.experimentId;
  const requested = patch.bottleId ?? current.bottleId;
  const others = selections.filter((_, i) => i !== index);

  let bottleId: number | null = requested;
  if (isDuplicateSelection(others, { experimentId, bottleId: requested })) {
    bottleId = patch.bottleId !== undefined
      ? null
      : firstFreeChamber(selections, experimentId, chamberIds, requested, index);
  }
  if (bottleId === null) return selections;
  if (experimentId === current.experimentId && bottleId === current.bottleId) return selections;

  const next = [...selections];
  next[index] = { experimentId, bottleId };
  return next;
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
