import { describe, expect, test } from "bun:test";
import {
  INCREMENTAL_OVERLAP_MS,
  incrementalSince,
  mergeReadings,
  nextCacheEntry,
  withConcludedState,
  withLatestReading,
  type CacheLimits,
} from "./readingCache";
import type { ExperimentSummary } from "./data";
import type { Reading } from "./types/domain";

function reading(bottle_id: number, minute: number, overrides: Partial<Reading> = {}): Reading {
  return {
    device_id: 1,
    bottle_id,
    experiment_id: 1,
    timestamp: new Date(Date.UTC(2026, 0, 1, 0, minute)).toISOString(),
    ph: 7,
    pressure: 1,
    temp: 38,
    co2: 1,
    ch4: 1,
    ...overrides,
  };
}

function summary(experiment_id: number, last: string | null): ExperimentSummary {
  return { experiment_id, first_timestamp: null, last_timestamp: last, concluded: true };
}

const MINUTE = 60 * 1000;
const WIDE: CacheLimits = { windowMs: 24 * 60 * MINUTE, maxRows: 100 };

describe("mergeReadings", () => {
  test("removes duplicates by (bottle_id, timestamp) and sorts oldest first", () => {
    const existing = [reading(0, 0), reading(1, 0), reading(0, 5)];
    const incoming = [reading(1, 5), reading(0, 5, { ph: 6 }), reading(0, 10)];

    const { readings, trimmed } = mergeReadings(existing, incoming, WIDE);

    expect(readings.map((r) => `${r.timestamp.slice(14, 16)}#${r.bottle_id}`)).toEqual(["00#0", "00#1", "05#0", "05#1", "10#0"]);
    // The incoming copy wins.
    expect(readings[2]!.ph).toBe(6);
    expect(trimmed).toBe(false);
  });

  test("treats the same instant in different string formats as one row", () => {
    const iso = reading(0, 5);
    const offset = { ...iso, timestamp: iso.timestamp.replace(".000Z", "+00:00") };
    expect(mergeReadings([iso], [offset], WIDE).readings).toHaveLength(1);
  });

  test("drops rows older than the window before the newest row", () => {
    const { readings, trimmed } = mergeReadings(
      [reading(0, 0), reading(0, 5), reading(0, 10)],
      [reading(0, 15)],
      { windowMs: 10 * MINUTE, maxRows: 100 },
    );
    // The window is inclusive: minute 5 is exactly 10 minutes before minute 15.
    expect(readings.map((r) => r.timestamp)).toEqual([reading(0, 5).timestamp, reading(0, 10).timestamp, reading(0, 15).timestamp]);
    expect(trimmed).toBe(true);
  });

  test("keeps only the newest `maxRows` rows", () => {
    const { readings, trimmed } = mergeReadings([reading(0, 0), reading(0, 5)], [reading(0, 10)], { ...WIDE, maxRows: 2 });
    expect(readings.map((r) => r.timestamp)).toEqual([reading(0, 5).timestamp, reading(0, 10).timestamp]);
    expect(trimmed).toBe(true);
  });
});

describe("nextCacheEntry", () => {
  test("a full read that kept everything is not truncated", () => {
    expect(nextCacheEntry(undefined, { readings: [reading(0, 0)], truncated: false }, WIDE).truncated).toBe(false);
  });

  test("a read that left older rows out is truncated", () => {
    expect(nextCacheEntry(undefined, { readings: [reading(0, 0)], truncated: true }, WIDE).truncated).toBe(true);
  });

  test("a poll that pushes rows out of the window truncates the entry, and it stays truncated", () => {
    const limits = { windowMs: 10 * MINUTE, maxRows: 100 };
    const first = nextCacheEntry(undefined, { readings: [reading(0, 0), reading(0, 5)], truncated: false }, limits);
    expect(first.truncated).toBe(false);

    const polled = nextCacheEntry(first, { readings: [reading(0, 15)], truncated: false }, limits);
    expect(polled.readings).toHaveLength(2);
    expect(polled.truncated).toBe(true);
    expect(nextCacheEntry(polled, { readings: [], truncated: false }, limits).truncated).toBe(true);
  });
});

describe("incrementalSince", () => {
  test("is undefined when nothing is cached, so the read is a full one", () => {
    expect(incrementalSince(undefined)).toBeUndefined();
    expect(incrementalSince({ readings: [], truncated: false })).toBeUndefined();
  });

  test("starts the overlap window before the newest cached row", () => {
    const newest = reading(0, 30);
    const since = incrementalSince({ readings: [reading(0, 0), newest], truncated: false });
    expect(Date.parse(since!)).toBe(Date.parse(newest.timestamp) - INCREMENTAL_OVERLAP_MS);
  });
});

describe("withLatestReading", () => {
  test("adds an experiment that started after the list was fetched, keeping newest first", () => {
    const list = withLatestReading([summary(3, "2026-01-01T00:00:00Z")], reading(0, 0, { experiment_id: 4 }));
    expect(list.map((experiment) => experiment.experiment_id)).toEqual([4, 3]);
  });

  test("moves the latest experiment's last sample forward", () => {
    const latest = reading(0, 30, { experiment_id: 3 });
    const list = withLatestReading([summary(3, "2026-01-01T00:00:00Z")], latest);
    expect(list[0]!.last_timestamp).toBe(latest.timestamp);
  });

  test("returns the same list when nothing changed", () => {
    const list = [summary(3, "2026-01-02T00:00:00Z")];
    expect(withLatestReading(list, reading(0, 0, { experiment_id: 3 }))).toBe(list);
    expect(withLatestReading(list, null)).toBe(list);
  });
});

describe("withConcludedState", () => {
  test("recomputes live state from the last sample, and treats no sample as concluded", () => {
    const list = withConcludedState(
      [summary(2, "fresh"), summary(1, "stale"), summary(0, null)],
      (timestamp) => timestamp === "stale",
    );
    expect(list.map((experiment) => experiment.concluded)).toEqual([false, true, true]);
  });
});
