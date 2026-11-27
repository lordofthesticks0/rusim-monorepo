import { describe, expect, test } from "bun:test";
import {
  DEV_FORCE_LIVE_EXPERIMENT,
  generateSyntheticLiveReadings,
  isMonitoringPollingEligible,
  isSyntheticExperiment,
  SYNTHETIC_EXPERIMENT_ID,
} from "./monitoring";
import { displayChamberNumber, internalChamberIds, splitBottlesByPresence } from "./chambers";
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

describe("Monitoring polling", () => {
  test("keeps polling eligible without an active experiment", () => {
    expect(isMonitoringPollingEligible("monitoring", false)).toBe(true);
  });

  test("does not poll in History or after a polling error", () => {
    expect(isMonitoringPollingEligible("history", false)).toBe(false);
    expect(isMonitoringPollingEligible("monitoring", true)).toBe(false);
  });
});

describe("Chamber numbering", () => {
  test("keeps 24 configured IDs internal and displays the range as Chambers 1 to 24", () => {
    const ids = internalChamberIds(24);

    expect(ids).toHaveLength(24);
    expect(ids[0]).toBe(0);
    expect(ids[23]).toBe(23);
    expect(displayChamberNumber(ids[0]!)).toBe(1);
    expect(displayChamberNumber(ids[23]!)).toBe(24);
  });

  test("keeps presence IDs internal while the display mapper provides 1-based labels", () => {
    const presence = splitBottlesByPresence([
      reading(1, 0, "2026-01-01T00:00:00Z"),
      reading(1, 2, "2026-01-01T00:00:00Z"),
    ], 4);

    expect(presence.latestSample).toBe("2026-01-01T00:00:00Z");
    expect(presence.connected).toEqual([0, 2]);
    expect(presence.disconnected).toEqual([1, 3]);
  });

  test("treats every configured chamber as disconnected before the first sample", () => {
    const presence = splitBottlesByPresence([], 24);

    expect(presence.latestSample).toBeNull();
    expect(presence.connected).toEqual([]);
    expect(presence.disconnected).toHaveLength(24);
    expect(presence.disconnected[0]).toBe(0);
    expect(displayChamberNumber(presence.disconnected[23]!)).toBe(24);
  });

  test("maps internal id 0 to displayed Chamber 1 without changing the row", () => {
    const row = reading(1, 0, "2026-01-01T00:00:00Z");
    const presence = splitBottlesByPresence([row], 2);

    expect(presence.connected).toEqual([0]);
    expect(presence.disconnected).toEqual([1]);
    expect(displayChamberNumber(presence.connected[0]!)).toBe(1);
    expect(row.bottle_id).toBe(0);
  });
});

describe("Synthetic preview data", () => {
  test("preview flag is enabled for temporary review", () => {
    expect(DEV_FORCE_LIVE_EXPERIMENT).toBe(1);
  });

  test("generates fresh readings for every chamber with a null and disconnected chambers", () => {
    const now = Date.parse("2026-11-04T12:00:00Z");
    const rows = generateSyntheticLiveReadings(1, 24, now);

    expect(rows.length).toBe(24 * 12 - 2);
    expect(rows.every((row) => row.experiment_id === SYNTHETIC_EXPERIMENT_ID)).toBe(true);
    expect(isSyntheticExperiment(SYNTHETIC_EXPERIMENT_ID)).toBe(true);
    expect(isSyntheticExperiment(3)).toBe(false);

    const latest = rows.reduce((max, row) => (row.timestamp > max ? row.timestamp : max), rows[0]!.timestamp);
    expect(Date.parse(latest)).toBe(now - 2 * 60 * 1000);

    const reporting = new Set(rows.filter((row) => row.timestamp === latest).map((row) => row.bottle_id));
    expect(reporting.has(6)).toBe(false);
    expect(reporting.has(18)).toBe(false);
    expect(rows.find((row) => row.bottle_id === 0 && row.timestamp === latest)?.ch4).toBeNull();
  });

  test("returns no rows when no chambers are configured", () => {
    expect(generateSyntheticLiveReadings(1, 0)).toEqual([]);
  });
});
