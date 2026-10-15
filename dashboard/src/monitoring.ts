import type { ExperimentSummary } from "./data";
import type { Reading } from "./types/domain";

export type DashboardView = "monitoring" | "history";

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
