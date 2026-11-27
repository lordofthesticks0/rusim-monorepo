import { getSupabase } from "./supabase";
import type { Device, DeviceSession, Reading } from "./types/domain";

export const PARAMETER_NAMES = ["pH", "Temp", "CO2", "CH4", "Pressure"] as const;

export type ParameterName = (typeof PARAMETER_NAMES)[number];

export const PARAMETER_UNITS = {
  pH: "",
  Temp: "°C",
  CO2: "ppm",
  CH4: "ppm",
  // The sensor slave emits Pa (`cachedPress = sensorValue * PA_PER_UNIT` in
  // legacy/slave/new-slave.cpp). Firmware values are stored unchanged, so the
  // label must match that source unit rather than the old kPa assumption.
  Pressure: "Pa",
} as const satisfies Record<ParameterName, string>;

// Chamber numbering lives in its own module so pure helpers stay importable
// without constructing the Supabase client.
export { displayChamberNumber, internalChamberIds, FIRST_INTERNAL_CHAMBER_ID, splitBottlesByPresence } from "./chambers";

type ReadingRow = {
  device_id: number;
  bottle_id: number;
  experiment_id: number;
  timestamp: string;
  ph: number | null;
  pressure: number | null;
  temp: number | null;
  co2: number | null;
  ch4: number | null;
};

const READING_COLUMNS = "device_id, bottle_id, experiment_id, timestamp, ph, pressure, temp, co2, ch4";

/**
 * A reading annotated with how concluded its experiment looks.
 *
 * The readings table holds one row per bottle per experiment, so "concluded" is
 * inferred from the sample time rather than stored.
 */
export type ReadingWithStatus = Reading & { concluded: boolean };

/**
 * One experiment that has readings on the session's device.
 *
 * The timestamps are the true first and last sample in the database, not just
 * the rows the dashboard has loaded. There is no row count: counting would need
 * an aggregate, which PostgREST does not offer to this client.
 */
export type ExperimentSummary = {
  experiment_id: number;
  first_timestamp: string | null;
  last_timestamp: string | null;
  concluded: boolean;
};

/** Cross-bottle statistics for one parameter within one experiment. */
export type ParameterStats = {
  min: number;
  average: number;
  max: number;
};

/**
 * Readings stop counting as live once this stale, in milliseconds.
 * A sample older than this means the device has gone quiet for that experiment.
 */
export const CONCLUDED_AFTER_MS = 10 * 60 * 1000;

/** How often to re-check an experiment that still looks live. */
export const POLL_INTERVAL_MS = 5 * 60 * 1000 + 30 * 1000;

/**
 * How much of an experiment the dashboard loads: the last 24 hours before the
 * experiment's newest sample.
 *
 * A time window rather than a row count, because the device's sample interval
 * is per experiment (`experiments.sample_interval_s`, 30 s when unset) and the
 * number of reporting chambers varies. The window means "one day" for any of
 * them. Reads are newest first, so anything older than the window is the
 * START of the experiment; a loaded experiment that has older rows is marked
 * truncated, and the UI says so.
 */
export const EXPERIMENT_WINDOW_MS = 24 * 60 * 60 * 1000;

/** Largest number of chambers a device can report. */
export const MAX_CHAMBERS = 24;

/** Database rounds in one window at the firmware's default 30 s interval. */
const DEFAULT_ROUNDS_PER_WINDOW = EXPERIMENT_WINDOW_MS / 30_000;

/**
 * Safety ceiling on rows one experiment read returns: a full window for every
 * chamber at the default interval (24 x 2,880 = 69,120 rows, about 10 MB in
 * memory, at most 70 requests).
 *
 * The window normally ends the read long before this. It only binds for a run
 * with a shorter `sample_interval_s` (the firmware allows down to 5 s), which
 * then loads less than a day and is marked truncated.
 */
export const MAX_EXPERIMENT_ROWS = MAX_CHAMBERS * DEFAULT_ROUNDS_PER_WINDOW;

/** One experiment read: rows oldest first, and whether older rows were left out. */
export type ExperimentRead = {
  readings: Reading[];
  truncated: boolean;
};

/**
 * Rows requested per PostgREST request while paging.
 *
 * Must stay at or below the server's max_rows setting, otherwise every page
 * returns a truncated response and the paging loop misreads the last one as
 * the end of the data.
 */
const PAGE_SIZE = 1000;

export function isConcluded(timestamp: string, now: number = Date.now()): boolean {
  const sampled = Date.parse(timestamp);
  if (Number.isNaN(sampled)) return true;
  return now - sampled > CONCLUDED_AFTER_MS;
}

function toReading(row: ReadingRow): Reading {
  return {
    device_id: row.device_id,
    bottle_id: row.bottle_id,
    experiment_id: row.experiment_id,
    timestamp: row.timestamp,
    ph: row.ph,
    pressure: row.pressure,
    temp: row.temp,
    co2: row.co2,
    ch4: row.ch4,
  };
}

/**
 * Exchanges a plaintext device token for a session.
 *
 * The database does the hashing and comparison, so the plaintext never reaches
 * storage. The returned session token is what scopes every later read.
 */
export async function authenticate(deviceToken: string): Promise<DeviceSession> {
  const { data, error } = await getSupabase(null).rpc("authenticate", { p_token: deviceToken });
  if (error) {
    // Every failure maps to one generic message to avoid oracling device
    // existence.
    throw new Error("Invalid device token.");
  }
  const row = data?.[0];
  if (!row) throw new Error("Invalid device token.");

  return {
    sessionToken: row.session_token,
    device: {
      device_id: row.device_id,
      bottle_count: row.bottle_count,
      owner_name: row.owner_name,
      owner_email: row.owner_email,
    },
    expiresAt: row.expires_at,
  };
}

/**
 * Ends the caller's session server side.
 *
 * A failure here must not trap the operator in the dashboard, since the
 * session expires on its own within a day.
 */
export async function signOut(sessionToken: string): Promise<void> {
  try {
    await getSupabase(sessionToken).rpc("revoke_session");
  } catch {
    // Ignored deliberately; see above.
  }
}

export async function fetchDevice(sessionToken: string): Promise<Device | undefined> {
  const { data, error } = await getSupabase(sessionToken)
    .from("devices")
    .select("device_id, bottle_count, owner_name, owner_email")
    .maybeSingle();
  if (error) throw error;
  if (!data) return undefined;

  return {
    device_id: data.device_id,
    bottle_count: data.bottle_count,
    owner_name: data.owner_name,
    owner_email: data.owner_email,
  };
}

/**
 * Reads the single newest reading on the session's device.
 *
 * This is the first query the dashboard makes: its experiment_id decides which
 * experiment Monitoring loads, and its timestamp decides whether that
 * experiment is still live. Returns null when the device has no readings.
 */
export async function fetchLatestReading(sessionToken: string): Promise<Reading | null> {
  const { data, error } = await getSupabase(sessionToken)
    .from("readings")
    .select(READING_COLUMNS)
    .order("timestamp", { ascending: false })
    .order("bottle_id", { ascending: false })
    .limit(1);
  if (error) throw error;
  const row = data?.[0];
  return row ? toReading(row) : null;
}

/**
 * Reads the last EXPERIMENT_WINDOW_MS (24 h) of one experiment, measured back
 * from its newest sample, and at most MAX_EXPERIMENT_ROWS rows.
 *
 * RLS already restricts this to the session's own device, so no device filter
 * is applied here.
 *
 * Rows are read newest first in (timestamp, bottle_id) order, and each page
 * starts after the last row of the previous one (keyset paging) instead of at
 * a numeric offset. Two reasons:
 *  - Every bottle is sampled at the same instant, so a cut (window or row
 *    ceiling) falls at a point in time for all bottles together, never
 *    dropping whole bottles.
 *  - Rows keep arriving while a run is active. An offset would shift under the
 *    reader as new rows land at the front and could skip or repeat rows;
 *    "older than this row" cannot.
 *
 * bottle_id is the tiebreak because several bottles share one timestamp, and
 * a timestamp-only cursor could skip the rest of a round split across pages.
 *
 * The window start is only known once the newest row arrives, so it is
 * applied in the client: paging stops at the first page that reaches past it.
 * Seeing a row older than the window is also how `truncated` is known
 * exactly; at most one page of older rows is read and discarded.
 *
 * `since` (an ISO timestamp, inclusive) instead limits the read to recent
 * rows, which lets a poll fetch only what arrived since the last load. Such a
 * read does not apply the window; the cache trims when it merges.
 *
 * `onProgress` is called after each page with the rows kept so far, so the UI
 * can show a running count during a long read (about 12 requests for a day of
 * 4 chambers at 30 s).
 *
 * The result is returned oldest first, the order every chart and export uses.
 */
export async function fetchExperimentReadings(
  sessionToken: string,
  experimentId: number,
  options: { since?: string; onProgress?: (rowsSoFar: number) => void } = {},
): Promise<ExperimentRead> {
  const rows: ReadingRow[] = [];
  let cursor: { timestamp: string; bottle_id: number } | null = null;
  let windowStart: number | null = null;
  let truncated = false;

  while (rows.length < MAX_EXPERIMENT_ROWS) {
    let query = getSupabase(sessionToken)
      .from("readings")
      .select(READING_COLUMNS)
      .eq("experiment_id", experimentId);
    if (options.since) query = query.gte("timestamp", options.since);
    if (cursor) {
      // Quotes keep the timestamp's "+" and ":" literal inside the filter.
      query = query.or(
        `timestamp.lt."${cursor.timestamp}",` +
          `and(timestamp.eq."${cursor.timestamp}",bottle_id.lt.${cursor.bottle_id})`,
      );
    }
    const pageSize = Math.min(PAGE_SIZE, MAX_EXPERIMENT_ROWS - rows.length);
    const { data, error } = await query
      .order("timestamp", { ascending: false })
      .order("bottle_id", { ascending: false })
      .limit(pageSize);
    if (error) throw error;

    const page = data ?? [];
    if (!options.since && windowStart === null && page[0]) {
      windowStart = Date.parse(page[0].timestamp) - EXPERIMENT_WINDOW_MS;
    }
    const inWindow = windowStart === null
      ? page
      : page.filter((row) => Date.parse(row.timestamp) >= windowStart!);
    rows.push(...inWindow);
    options.onProgress?.(rows.length);
    if (inWindow.length < page.length) {
      // Rows older than the window exist: the start of the experiment is cut.
      truncated = true;
      break;
    }
    // A short page means the experiment has no more rows.
    if (page.length < pageSize) break;
    const last = page[page.length - 1]!;
    cursor = { timestamp: last.timestamp, bottle_id: last.bottle_id };
  }
  // Stopping at the ceiling may leave older rows unread.
  if (rows.length >= MAX_EXPERIMENT_ROWS) truncated = true;

  return { readings: rows.reverse().map(toReading), truncated: !options.since && truncated };
}

/**
 * Lists every experiment id with readings on the session's device, newest first.
 *
 * PostgREST cannot run SELECT DISTINCT for this client, so this does a
 * "skip scan": ask for the highest experiment_id, then the highest one below
 * that, and so on until nothing is left. Each step is one LIMIT 1 query served
 * by the (experiment_id, timestamp) index, so the cost grows with the number
 * of experiments, not the number of readings.
 */
export async function fetchExperimentIds(
  sessionToken: string,
  onId: (experimentId: number) => void = () => {},
): Promise<number[]> {
  const ids: number[] = [];
  for (;;) {
    let query = getSupabase(sessionToken).from("readings").select("experiment_id");
    const below = ids.at(-1);
    if (below !== undefined) query = query.lt("experiment_id", below);
    const { data, error } = await query.order("experiment_id", { ascending: false }).limit(1);
    if (error) throw error;
    const row = data?.[0];
    if (!row) return ids;
    ids.push(row.experiment_id);
    onId(row.experiment_id);
  }
}

/** Reads the first and last sample time of one experiment (two LIMIT 1 queries). */
async function fetchExperimentBounds(
  sessionToken: string,
  experimentId: number,
): Promise<{ first: string | null; last: string | null }> {
  const edge = (ascending: boolean) => getSupabase(sessionToken)
    .from("readings")
    .select("timestamp")
    .eq("experiment_id", experimentId)
    .order("timestamp", { ascending })
    .limit(1);
  const [first, last] = await Promise.all([edge(true), edge(false)]);
  if (first.error) throw first.error;
  if (last.error) throw last.error;
  return { first: first.data?.[0]?.timestamp ?? null, last: last.data?.[0]?.timestamp ?? null };
}

/**
 * Builds the Experiment history list: every experiment on the device with its
 * first and last sample time, newest experiment first.
 *
 * The bounds queries for each experiment start as soon as its id is found,
 * so they run while the skip scan is still looking for the next id.
 */
export async function fetchExperimentList(sessionToken: string, now: number = Date.now()): Promise<ExperimentSummary[]> {
  const pending: Array<ReturnType<typeof fetchExperimentBounds>> = [];
  const ids = await fetchExperimentIds(sessionToken, (id) => {
    const request = fetchExperimentBounds(sessionToken, id);
    // Marks the rejection as handled in case the skip scan fails first and
    // Promise.all below is never reached; Promise.all still sees the error.
    request.catch(() => {});
    pending.push(request);
  });
  const bounds = await Promise.all(pending);
  return ids.map((experiment_id, index) => {
    const { first, last } = bounds[index]!;
    return {
      experiment_id,
      first_timestamp: first,
      last_timestamp: last,
      concluded: last === null ? true : isConcluded(last, now),
    };
  });
}

/**
 * Reads one parameter from a sample.
 *
 * Returns null when the sensor produced no value, so callers must handle a
 * missing measurement rather than assuming every reported sample is complete.
 */
export function getParameterValue(reading: Reading, parameter: ParameterName): number | null {
  switch (parameter) {
    case "pH": return reading.ph;
    case "Temp": return reading.temp;
    case "CO2": return reading.co2;
    case "CH4": return reading.ch4;
    case "Pressure": return reading.pressure;
  }
}

/**
 * Cross-bottle statistics for one parameter.
 *
 * Nulls are excluded rather than treated as zero: a bottle that reported no
 * value should lower the sample count, not drag the average toward nothing.
 * Returns null when no bottle produced a value at all.
 */
export function summarizeParameter(values: Array<number | null>): ParameterStats | null {
  const reported = values.filter((value): value is number => value !== null);
  if (reported.length === 0) return null;
  return {
    min: Math.min(...reported),
    average: reported.reduce((sum, value) => sum + value, 0) / reported.length,
    max: Math.max(...reported),
  };
}
