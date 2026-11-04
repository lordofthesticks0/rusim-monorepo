import type { ExperimentSummary } from "./data";
import type { Reading } from "./types/domain";

export type DashboardView = "monitoring" | "history" | "compare";

/**
 * Temporary preview flag: when 1, Monitoring shows synthetic live data if no
 * real experiment is live, so the live view can be reviewed without a device.
 * Set to 0 before deploying. History always shows real data.
 */
export const DEV_FORCE_LIVE_EXPERIMENT = 1;

/** Experiment id used for synthetic preview rows; never a real experiment. */
export const SYNTHETIC_EXPERIMENT_ID = 999;

/** Whether an experiment id is the synthetic preview experiment. */
export function isSyntheticExperiment(experimentId: number): boolean {
  return experimentId === SYNTHETIC_EXPERIMENT_ID;
}

/**
 * Generates fresh synthetic readings for previewing Monitoring.
 *
 * Twelve samples per chamber at 5-minute intervals, with the latest sample
 * 2 minutes ago so the experiment reads as live. Two chambers are missing at
 * the latest sample (disconnected) and one sensor is null, so the presence
 * strip, dash, and line break are all visible. Values are deterministic.
 */
export function generateSyntheticLiveReadings(
  deviceId: number,
  bottleCount: number,
  now: number = Date.now(),
): Reading[] {
  if (bottleCount <= 0) return [];
  const readings: Reading[] = [];
  const disconnected = new Set([6, 18]);
  for (let bottle = 0; bottle < bottleCount; bottle++) {
    for (let t = 0; t < 12; t++) {
      if (t === 0 && disconnected.has(bottle)) continue;
      const timestamp = new Date(now - (2 + t * 5) * 60 * 1000).toISOString();
      readings.push({
        device_id: deviceId,
        bottle_id: bottle,
        experiment_id: SYNTHETIC_EXPERIMENT_ID,
        timestamp,
        ph: 6.2 + (bottle % 5) * 0.05 + t * 0.005,
        pressure: 101 + t * 0.1,
        temp: 38.5 + (bottle % 3) * 0.1 + t * 0.02,
        co2: 1200 + t * 10 + bottle,
        ch4: bottle === 0 && t === 0 ? null : 450 + t * 3 + bottle,
      });
    }
  }
  return readings;
}

/** Monitoring may poll even before an active experiment has been discovered. */
export function isMonitoringPollingEligible(
  view: DashboardView,
  pollSuspended: boolean,
): boolean {
  return view === "monitoring" && !pollSuspended;
}

/** Selects the first live experiment using the existing sorted-summary policy. */
export function selectActiveExperiment(
  experiments: ExperimentSummary[],
): ExperimentSummary | null {
  return experiments.find((experiment) => !experiment.concluded) ?? null;
}

/** Returns only rows belonging to the experiment selected for Monitoring. */
export function selectMonitoringReadings(
  readings: Reading[],
  experimentId: number | null,
): Reading[] {
  if (experimentId === null) return [];
  return readings.filter((reading) => reading.experiment_id === experimentId);
}
