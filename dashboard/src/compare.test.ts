import { describe, expect, test } from "bun:test";
import {
  addSelection,
  canAddSelection,
  compareSeriesColor,
  compareSeriesLabel,
  distinctExperimentIds,
  isDuplicateSelection,
  MAX_COMPARE_SERIES,
  removeSelection,
  type CompareSelection,
} from "./compare";

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
