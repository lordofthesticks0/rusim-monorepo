import { useCallback, useRef, useState } from "react";
import { EXPERIMENT_WINDOW_MS, fetchExperimentReadings, MAX_EXPERIMENT_ROWS } from "./data";
import { incrementalSince, nextCacheEntry, type CacheLimits, type CachedExperiment } from "./readingCache";

const LIMITS: CacheLimits = { windowMs: EXPERIMENT_WINDOW_MS, maxRows: MAX_EXPERIMENT_ROWS };

export type ReadingCache = {
  /** Every experiment loaded this session, keyed by experiment_id. */
  entries: Readonly<Record<number, CachedExperiment>>;
  /**
   * Experiments being read in full right now, keyed by experiment_id, with
   * the rows received so far. Incremental poll reads are not listed: they are
   * one short request and the experiment is already on screen.
   */
  loading: Readonly<Record<number, number>>;
  /**
   * Makes sure an experiment is in the cache and resolves with its entry.
   *
   * - Not cached: reads the experiment's last 24 h (EXPERIMENT_WINDOW_MS),
   *   at most MAX_EXPERIMENT_ROWS rows.
   * - Cached, `refresh` false: resolves at once with no request.
   * - Cached, `refresh` true: reads only rows since the newest cached one
   *   (minus a short overlap) and merges them in.
   *
   * Concurrent calls for the same experiment share one request.
   */
  load: (experimentId: number, options?: { refresh?: boolean }) => Promise<CachedExperiment>;
};

/**
 * One in-memory store of readings shared by Monitoring, History and Compare.
 *
 * Entries live for the whole session: jumping back to an experiment never
 * re-reads it. Each entry holds one day of that experiment. At the default
 * 30 s interval a 4-chamber run is about 11,500 rows (about 2 MB at roughly
 * 150 bytes a row); the ceiling of MAX_EXPERIMENT_ROWS (69,120) is about 10 MB.
 *
 * The cache belongs to one session. The Dashboard is keyed by session token,
 * so a new sign-in remounts it and starts with an empty cache.
 */
export function useReadingCache(sessionToken: string): ReadingCache {
  const [entries, setEntries] = useState<Record<number, CachedExperiment>>({});
  const [loading, setLoading] = useState<Record<number, number>>({});
  // Mirrors `entries` so `load` can read the latest cache without being
  // recreated on every update (which would re-run effects that depend on it).
  const entriesRef = useRef(entries);
  const inFlight = useRef(new Map<number, Promise<CachedExperiment>>());

  const setProgress = useCallback((experimentId: number, rows: number | null) => {
    setLoading((previous) => {
      if (rows === null) {
        if (!(experimentId in previous)) return previous;
        const { [experimentId]: _finished, ...rest } = previous;
        return rest;
      }
      return { ...previous, [experimentId]: rows };
    });
  }, []);

  const load = useCallback((experimentId: number, options: { refresh?: boolean } = {}) => {
    const running = inFlight.current.get(experimentId);
    if (running) return running;

    const previous = entriesRef.current[experimentId];
    if (previous && !options.refresh) return Promise.resolve(previous);

    const since = incrementalSince(previous);
    const full = since === undefined;
    if (full) setProgress(experimentId, 0);
    const request = fetchExperimentReadings(sessionToken, experimentId, {
      since,
      onProgress: full ? (rows) => setProgress(experimentId, rows) : undefined,
    })
      .then((read) => {
        // Merge into whatever is cached now, not what was cached at the start.
        const entry = nextCacheEntry(since ? entriesRef.current[experimentId] : undefined, read, LIMITS);
        entriesRef.current = { ...entriesRef.current, [experimentId]: entry };
        setEntries(entriesRef.current);
        return entry;
      })
      .finally(() => {
        inFlight.current.delete(experimentId);
        if (full) setProgress(experimentId, null);
      });
    inFlight.current.set(experimentId, request);
    return request;
  }, [sessionToken, setProgress]);

  return { entries, loading, load };
}
