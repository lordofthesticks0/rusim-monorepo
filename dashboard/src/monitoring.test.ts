import { describe, expect, test } from "bun:test";
import {
  isMonitoringPollingEligible,
  selectActiveExperiment,
  selectMonitoringReadings,
} from "./monitoring";
import type { ExperimentSummary } from "./data";
import type { Reading } from "./types/domain";

function reading(experiment_id: number, bottle_id: number, timestamp: string): Reading {
  return {
    device_id: 1,
    bottle_id,
    experiment_id,
    timestamp,
    ph: 7,
    pressure: 100,
    temp: 20,
    co2: 300,
    ch4: 40,
  };
}

function summary(experiment_id: number, concluded: boolean): ExperimentSummary {
  return {
    experiment_id,
    reading_count: 1,
    first_timestamp: "2026-01-01T00:00:00Z",
    last_timestamp: "2026-01-01T00:00:00Z",
    concluded,
  };
}

describe("Monitoring experiment isolation", () => {
  test("keeps polling eligible without an active experiment", () => {
    expect(isMonitoringPollingEligible("monitoring", false)).toBe(true);
  });

  test("does not poll in History or after a polling error", () => {
    expect(isMonitoringPollingEligible("history", false)).toBe(false);
    expect(isMonitoringPollingEligible("monitoring", true)).toBe(false);
  });

  test("selects a newly available experiment after a later fetch", () => {
    const initialReadings: Reading[] = [];
    const refreshedReadings = [
      reading(1, 0, "2026-01-01T00:00:00Z"),
      reading(2, 0, "2026-01-01T00:01:00Z"),
      reading(1, 1, "2026-01-01T00:00:00Z"),
    ];

    expect(selectMonitoringReadings(initialReadings, null)).toEqual([]);

    const active = selectActiveExperiment([summary(2, false), summary(1, true)]);
    expect(active?.experiment_id).toBe(2);
    expect(selectMonitoringReadings(refreshedReadings, active?.experiment_id ?? null)).toEqual([
      refreshedReadings[1]!,
    ]);
  });

  test("selects the existing first live experiment and excludes other experiments", () => {
    const readings = [
      reading(1, 0, "2026-01-01T00:00:00Z"),
      reading(2, 0, "2026-01-01T00:01:00Z"),
    ];
    const active = selectActiveExperiment([summary(2, false), summary(1, true)]);

    expect(active?.experiment_id).toBe(2);
    expect(selectMonitoringReadings(readings, active?.experiment_id ?? null)).toEqual([readings[1]!]);
  });

  test("returns no Monitoring readings when there is no active experiment", () => {
    const readings = [reading(1, 0, "2026-01-01T00:00:00Z")];
    const active = selectActiveExperiment([summary(1, true)]);

    expect(active).toBeNull();
    expect(selectMonitoringReadings(readings, active?.experiment_id ?? null)).toEqual([]);
  });

  test("isolates the same bottle across experiments", () => {
    const readings = [
      reading(1, 0, "2026-01-01T00:00:00Z"),
      reading(2, 0, "2026-01-01T00:01:00Z"),
      reading(2, 1, "2026-01-01T00:01:00Z"),
    ];
    const activeReadings = selectMonitoringReadings(readings, 2);

    expect(activeReadings).toHaveLength(2);
    expect(activeReadings.every((item) => item.experiment_id === 2)).toBe(true);
    expect(activeReadings.filter((item) => item.bottle_id === 0)).toEqual([readings[1]!]);
  });

  test("prevents historical-only bottles from affecting current presence input", () => {
    const readings = [
      reading(1, 1, "2026-01-01T00:02:00Z"),
      reading(2, 0, "2026-01-01T00:03:00Z"),
    ];

    const activeReadings = selectMonitoringReadings(readings, 2);

    expect(activeReadings.map((item) => item.bottle_id)).toEqual([0]);
  });

  test("switches datasets on refresh without retaining the previous experiment", () => {
    const firstRefresh = [reading(1, 0, "2026-01-01T00:00:00Z")];
    const secondRefresh = [reading(2, 0, "2026-01-01T00:10:00Z")];

    expect(selectMonitoringReadings(firstRefresh, 1)).toEqual(firstRefresh);
    expect(selectMonitoringReadings(secondRefresh, 2)).toEqual(secondRefresh);
    expect(selectMonitoringReadings(secondRefresh, 2)).not.toContain(firstRefresh[0]!);
  });

  test("does not fall back to a completed experiment", () => {
    const readings = [
      reading(3, 0, "2026-01-01T00:00:00Z"),
      reading(2, 0, "2025-12-31T00:00:00Z"),
    ];

    expect(selectMonitoringReadings(readings, null)).toEqual([]);
  });
});
