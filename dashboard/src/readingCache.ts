import type { ExperimentRead, ExperimentSummary } from "./data";
import type { Reading } from "./types/domain";

/**
 * Pure helpers for the in-memory reading cache (see useReadingCache.ts).
 *
 * Kept free of runtime imports from data.ts so they can be tested without
 * constructing the Supabase client.
 */

/** Everything the dashboard has loaded for one experiment. */
export type CachedExperiment = {
  /** Oldest first, unique per (bottle_id, timestamp), within the limits below. */
  readings: Reading[];
  /** True when older rows of the experiment were left out (window or row cap). */
  truncated: boolean;
};

/** How much of an experiment the cache keeps. */
export type CacheLimits = {
  /** Keep rows at most this long before the newest row. */
  windowMs: number;
  /** Keep at most this many rows (the newest). */
  maxRows: number;
};

/**
 * How far before the newest cached row an incremental poll starts reading.
 *
 * A device can push a buffered round late, with a timestamp older than rows
 * already stored. Re-reading a short overlap catches those; duplicates are
 * removed by mergeReadings.
 */
export const INCREMENTAL_OVERLAP_MS = 10 * 60 * 1000;

function readingKey(reading: Reading): string {
  return `${reading.bottle_id}|${Date.parse(reading.timestamp)}`;
}

function compareReadings(left: Reading, right: Reading): number {
  const difference = Date.parse(left.timestamp) - Date.parse(right.timestamp);
  return difference !== 0 ? difference : left.bottle_id - right.bottle_id;
}

/**
 * Combines cached rows with newly fetched ones.
 *
 * A row is identified by (bottle_id, timestamp), the table's key within one
 * device; an incoming row replaces a cached row with the same key. The result
 * is oldest first. Rows older than `windowMs` before the newest row are
 * dropped, then only the newest `maxRows` are kept. `trimmed` says whether
 * either limit dropped anything.
 */
export function mergeReadings(
  existing: Reading[],
  incoming: Reading[],
  limits: CacheLimits,
): { readings: Reading[]; trimmed: boolean } {
  let merged = existing;
  if (incoming.length > 0) {
    const byKey = new Map<string, Reading>();
    for (const reading of existing) byKey.set(readingKey(reading), reading);
    for (const reading of incoming) byKey.set(readingKey(reading), reading);
    merged = [...byKey.values()].sort(compareReadings);
  }
  const newest = merged.at(-1);
  if (!newest) return { readings: merged, trimmed: false };

  // Sorted oldest first, so the rows outside the window form a prefix.
  const windowStart = Date.parse(newest.timestamp) - limits.windowMs;
  let first = merged.findIndex((reading) => Date.parse(reading.timestamp) >= windowStart);
  first = Math.max(first, merged.length - limits.maxRows);
  return first > 0
    ? { readings: merged.slice(first), trimmed: true }
    : { readings: merged, trimmed: false };
}

/**
 * Builds the cache entry after a fetch.
 *
 * `previous` is the entry before an incremental fetch, or undefined for a full
 * one. The entry is truncated when the read left older rows out or the merge
 * trimmed any; once truncated it stays truncated.
 */
export function nextCacheEntry(
  previous: CachedExperiment | undefined,
  fetched: ExperimentRead,
  limits: CacheLimits,
): CachedExperiment {
  const { readings, trimmed } = mergeReadings(previous?.readings ?? [], fetched.readings, limits);
  return {
    readings,
    truncated: (previous?.truncated ?? false) || fetched.truncated || trimmed,
  };
}

/**
 * Where an incremental poll should start reading, or undefined when nothing
 * is cached and the experiment must be read in full.
 */
export function incrementalSince(entry: CachedExperiment | undefined): string | undefined {
  const newest = entry?.readings.at(-1);
  if (!newest) return undefined;
  const from = Date.parse(newest.timestamp) - INCREMENTAL_OVERLAP_MS;
  return Number.isNaN(from) ? undefined : new Date(from).toISOString();
}

/**
 * Folds the newest reading into the experiment list.
 *
 * The list is fetched once, but polls keep finding newer rows. This moves the
 * latest experiment's last sample forward, or adds the experiment when it
 * started after the list was fetched. The list stays newest experiment first.
 */
export function withLatestReading(
  experiments: ExperimentSummary[],
  latest: Reading | null,
): ExperimentSummary[] {
  if (!latest) return experiments;
  const index = experiments.findIndex((experiment) => experiment.experiment_id === latest.experiment_id);
  if (index === -1) {
    return [
      { experiment_id: latest.experiment_id, first_timestamp: null, last_timestamp: latest.timestamp, concluded: false },
      ...experiments,
    ].sort((left, right) => right.experiment_id - left.experiment_id);
  }
  const current = experiments[index]!;
  if (current.last_timestamp !== null && Date.parse(current.last_timestamp) >= Date.parse(latest.timestamp)) {
    return experiments;
  }
  const next = [...experiments];
  next[index] = { ...current, last_timestamp: latest.timestamp };
  return next;
}

/** Recomputes each experiment's live/concluded state against the current clock. */
export function withConcludedState(
  experiments: ExperimentSummary[],
  isConcluded: (timestamp: string) => boolean,
): ExperimentSummary[] {
  return experiments.map((experiment) => ({
    ...experiment,
    concluded: experiment.last_timestamp === null ? true : isConcluded(experiment.last_timestamp),
  }));
}
