import { describe, expect, test } from "bun:test";
import {
  getHistoryPointDetails,
  getExperimentChambers,
  groupReadingsByChamber,
  selectChamberReadings,
  toggleExperimentSelection,
} from "./history";
import type { Reading } from "./types/domain";

function reading(overrides: Partial<Reading> = {}): Reading {
  return {
    device_id: 1,
    bottle_id: 1,
    experiment_id: 1,
    timestamp: "2026-01-01T00:00:00.000Z",
    ph: 7,
    pressure: 10,
    temp: 20,
    co2: 30,
    ch4: 40,
    ...overrides,
  };
}

describe("historical chamber data", () => {
  test("starts without a selection and toggles selection on repeated clicks", () => {
    expect(toggleExperimentSelection(null, 1)).toBe(1);
    expect(toggleExperimentSelection(1, 1)).toBeNull();
    expect(toggleExperimentSelection(1, 2)).toBe(2);
  });

  test("selects only the requested chamber for the requested experiment", () => {
    const rows = [
      reading({ bottle_id: 1, timestamp: "2026-01-01T00:01:00.000Z" }),
      reading({ bottle_id: 2, timestamp: "2026-01-01T00:02:00.000Z" }),
      reading({ experiment_id: 2, bottle_id: 1, timestamp: "2026-01-01T00:03:00.000Z" }),
    ];

    expect(selectChamberReadings(rows, 1, 1)).toHaveLength(1);
    expect(getExperimentChambers(rows, 1)).toEqual([1, 2]);
  });

  test("switching chambers replaces the chart dataset", () => {
    const rows = [
      reading({ bottle_id: 1, timestamp: "2026-01-01T00:01:00.000Z" }),
      reading({ bottle_id: 2, timestamp: "2026-01-01T00:02:00.000Z" }),
    ];

    expect(selectChamberReadings(rows, 1, 1).every((row) => row.bottle_id === 1)).toBe(true);
    expect(selectChamberReadings(rows, 1, 2).every((row) => row.bottle_id === 2)).toBe(true);
  });

  test("creates one result summary per chamber", () => {
    const summaries = groupReadingsByChamber([
      reading({ bottle_id: 1, timestamp: "2026-01-01T00:01:00.000Z" }),
      reading({ bottle_id: 1, timestamp: "2026-01-01T00:02:00.000Z" }),
      reading({ bottle_id: 2, timestamp: "2026-01-01T00:01:00.000Z" }),
      reading({ bottle_id: 2, timestamp: "2026-01-01T00:02:00.000Z" }),
    ], 1);

    expect(summaries).toHaveLength(2);
    expect(summaries.map((summary) => summary.readings.length)).toEqual([2, 2]);
  });

  test("calculates statistics independently for each chamber", () => {
    const summaries = groupReadingsByChamber([
      reading({ bottle_id: 1, ph: 6, timestamp: "2026-01-01T00:01:00.000Z" }),
      reading({ bottle_id: 1, ph: 8, timestamp: "2026-01-01T00:02:00.000Z" }),
      reading({ bottle_id: 2, ph: 100, timestamp: "2026-01-01T00:01:00.000Z" }),
    ], 1);

    expect(summaries[0]!.stats.pH).toEqual({ min: 6, average: 7, max: 8 });
    expect(summaries[1]!.stats.pH).toEqual({ min: 100, average: 100, max: 100 });
  });

  test("orders chamber readings chronologically", () => {
    const summary = groupReadingsByChamber([
      reading({ timestamp: "2026-01-01T00:03:00.000Z", ph: 9 }),
      reading({ timestamp: "2026-01-01T00:01:00.000Z", ph: 6 }),
      reading({ timestamp: "2026-01-01T00:02:00.000Z", ph: 7 }),
    ], 1)[0]!;

    expect(summary.firstReading.ph).toBe(6);
    expect(summary.lastReading.ph).toBe(9);
  });

  test("excludes null measurements from statistics", () => {
    const summary = groupReadingsByChamber([
      reading({ ph: null }),
      reading({ ph: 8, timestamp: "2026-01-01T00:01:00.000Z" }),
    ], 1)[0]!;

    expect(summary.stats.pH).toEqual({ min: 8, average: 8, max: 8 });
  });

  test("returns unavailable statistics when a chamber has no valid values", () => {
    const summary = groupReadingsByChamber([reading({ ph: null })], 1)[0]!;

    expect(summary.stats.pH).toBeNull();
  });

  test("isolates charts and results from other experiments", () => {
    const rows = [
      reading({ experiment_id: 1, bottle_id: 1, ph: 7 }),
      reading({ experiment_id: 2, bottle_id: 1, ph: 99 }),
    ];

    expect(selectChamberReadings(rows, 1, 1)[0]!.ph).toBe(7);
    expect(groupReadingsByChamber(rows, 1)[0]!.stats.pH?.max).toBe(7);
  });

  test("resolves tooltip details for the active parameter and reading", () => {
    const row = reading({ timestamp: "2026-01-01T12:34:56.000Z", temp: 19.42 });

    expect(getHistoryPointDetails(row, "Temp")).toEqual({
      timestamp: row.timestamp,
      parameter: "Temp",
      value: 19.42,
    });
    expect(getHistoryPointDetails(reading({ ph: null }), "pH").value).toBeNull();
  });
});
