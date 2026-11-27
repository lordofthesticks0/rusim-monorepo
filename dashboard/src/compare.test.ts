import { describe, expect, test } from "bun:test";
import {
  addSelection,
  canAddSelection,
  compareSeriesColor,
  compareSeriesLabel,
  distinctExperimentIds,
  firstFreeChamber,
  isDuplicateSelection,
  MAX_COMPARE_SERIES,
  nextSelection,
  removeSelection,
  updateSelection,
  type CompareSelection,
} from "./compare";

const CHAMBERS = [0, 1, 2, 3];

function selection(experimentId: number, bottleId: number): CompareSelection {
  return { experimentId, bottleId };
}

describe("Compare selection", () => {
  test("allows up to four series and blocks the fifth", () => {
    const four = [
      selection(1, 0),
      selection(1, 1),
      selection(2, 0),
      selection(2, 1),
    ];

    expect(MAX_COMPARE_SERIES).toBe(4);
    expect(canAddSelection(four, selection(3, 0))).toBe(false);
    expect(addSelection(four, selection(3, 0))).toEqual(four);
  });

  test("allows the same chamber across different experiments", () => {
    const selections = [selection(1, 0)];

    expect(canAddSelection(selections, selection(2, 0))).toBe(true);
    expect(addSelection(selections, selection(2, 0))).toEqual([
      selection(1, 0),
      selection(2, 0),
    ]);
  });

  test("blocks the same experiment and chamber twice", () => {
    const selections = [selection(1, 0)];

    expect(isDuplicateSelection(selections, selection(1, 0))).toBe(true);
    expect(canAddSelection(selections, selection(1, 0))).toBe(false);
    expect(addSelection(selections, selection(1, 0))).toEqual(selections);
  });

  test("removes the entry at the given index", () => {
    const selections = [selection(1, 0), selection(2, 1), selection(3, 2)];

    expect(removeSelection(selections, 1)).toEqual([selection(1, 0), selection(3, 2)]);
  });

  test("lists distinct experiment ids in selection order", () => {
    const selections = [selection(2, 0), selection(1, 1), selection(2, 2)];

    expect(distinctExperimentIds(selections)).toEqual([2, 1]);
  });

  test("labels series with the formatted experiment and displayed chamber number", () => {
    expect(compareSeriesLabel(selection(3, 0))).toBe("Experiment 3 · Chamber 1");
    expect(compareSeriesLabel(selection(0, 23))).toBe("Experiment 0 (testing) · Chamber 24");
  });

  test("gives each position a distinct colour", () => {
    const colors = [0, 1, 2, 3].map(compareSeriesColor);

    expect(new Set(colors).size).toBe(4);
  });
});

describe("Compare rows", () => {
  test("a new row copies the last row's experiment and takes its next free chamber", () => {
    expect(nextSelection([selection(3, 1)], 9, CHAMBERS)).toEqual(selection(3, 2));
  });

  test("the first row uses the fallback experiment and the first chamber", () => {
    expect(nextSelection([], 5, CHAMBERS)).toEqual(selection(5, 0));
    expect(nextSelection([], null, CHAMBERS)).toBeNull();
  });

  test("the next free chamber wraps around past the end", () => {
    expect(nextSelection([selection(1, 1), selection(1, 3)], null, CHAMBERS)).toEqual(selection(1, 0));
  });

  test("no new row at the cap or when the experiment has no free chamber", () => {
    const four = [selection(1, 0), selection(1, 1), selection(2, 0), selection(2, 1)];
    expect(nextSelection(four, 1, CHAMBERS)).toBeNull();
    expect(nextSelection([selection(1, 0), selection(1, 1)], 1, [0, 1])).toBeNull();
  });

  test("firstFreeChamber ignores the row being edited", () => {
    const rows = [selection(1, 0), selection(1, 1)];
    expect(firstFreeChamber(rows, 1, CHAMBERS, 0, 0)).toBe(0);
    expect(firstFreeChamber(rows, 1, CHAMBERS, 0)).toBe(2);
  });

  test("changing a row's chamber edits it in place", () => {
    const rows = [selection(1, 0), selection(2, 0)];
    expect(updateSelection(rows, 1, { bottleId: 3 }, CHAMBERS)).toEqual([selection(1, 0), selection(2, 3)]);
  });

  test("a chamber change that would duplicate another row is refused", () => {
    const rows = [selection(1, 0), selection(1, 1)];
    expect(updateSelection(rows, 1, { bottleId: 0 }, CHAMBERS)).toBe(rows);
  });

  test("an experiment change keeps the chamber when free, otherwise moves to the next free one", () => {
    const rows = [selection(1, 2), selection(2, 2)];
    expect(updateSelection(rows, 1, { experimentId: 3 }, CHAMBERS)).toEqual([selection(1, 2), selection(3, 2)]);
    expect(updateSelection(rows, 1, { experimentId: 1 }, CHAMBERS)).toEqual([selection(1, 2), selection(1, 3)]);
  });

  test("an experiment change with no free chamber is refused", () => {
    const rows = [selection(1, 0), selection(1, 1), selection(2, 0)];
    expect(updateSelection(rows, 2, { experimentId: 1 }, [0, 1])).toBe(rows);
  });
});
