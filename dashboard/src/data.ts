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

export type ExperimentSummary = {
  experiment_id: number;
  reading_count: number;
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
 * Upper bound on rows any single read returns.
 *
 * PostgREST caps each response at max_rows, so reads page rather than asking for
 * everything at once. Callers that hit this bound receive a partial result, so
 * anything presenting itself as a complete dataset (notably CSV export) must
 * check the row count against this and say so.
 */
export const MAX_READING_ROWS = 5000;

/**
 * Upper bound on rows one experiment read returns.
 *
 * Sized for the longest run: 99 h at one round per 30 s is 11,880 rows per
 * bottle, about 47,500 for four bottles. Reads are oldest first, so a result
 * at this bound is missing the END of the experiment.
 */
export const MAX_EXPERIMENT_ROWS = 50_000;

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

export async function fetchReadings(sessionToken: string): Promise<Reading[]> {
  const rows: ReadingRow[] = [];
  for (let offset = 0; offset < MAX_READING_ROWS; offset += PAGE_SIZE) {
    const { data, error } = await getSupabase(sessionToken)
      .from("readings")
      .select(READING_COLUMNS)
      .order("timestamp", { ascending: false })
      .range(offset, Math.min(offset + PAGE_SIZE - 1, MAX_READING_ROWS - 1));
    if (error) throw error;
    rows.push(...(data ?? []));
    if (!data || data.length < PAGE_SIZE) break;
  }

  return rows.map(toReading);
}

/**
 * Reads every chamber's readings for one experiment.
 *
 * RLS already restricts this to the session's own device, so no device filter
 * is applied here.
 *
 * Rows are read oldest first in (timestamp, bottle_id) order, and each page
 * starts after the last row of the previous one (keyset paging) instead of at
 * a numeric offset. Two reasons:
 *  - Every bottle is sampled at the same instant, so hitting the row cap cuts
 *    the experiment off at a point in time for all bottles together, never
 *    dropping whole bottles.
 *  - Rows keep arriving while a run is active. An offset would shift under the
 *    reader and could skip or repeat rows; "after this row" cannot.
 *
 * bottle_id is the tiebreak because several bottles share one timestamp, and
 * a timestamp-only cursor could skip the rest of a round split across pages.
 */
export async function fetchExperimentReadings(sessionToken: string, experimentId: number): Promise<Reading[]> {
  const rows: ReadingRow[] = [];
  let cursor: { timestamp: string; bottle_id: number } | null = null;

  while (rows.length < MAX_EXPERIMENT_ROWS) {
    let query = getSupabase(sessionToken)
      .from("readings")
      .select(READING_COLUMNS)
      .eq("experiment_id", experimentId);
    if (cursor) {
      // Quotes keep the timestamp's "+" and ":" literal inside the filter.
      query = query.or(
        `timestamp.gt."${cursor.timestamp}",` +
          `and(timestamp.eq."${cursor.timestamp}",bottle_id.gt.${cursor.bottle_id})`,
      );
    }
    const pageSize = Math.min(PAGE_SIZE, MAX_EXPERIMENT_ROWS - rows.length);
    const { data, error } = await query
      .order("timestamp", { ascending: true })
      .order("bottle_id", { ascending: true })
      .limit(pageSize);
    if (error) throw error;

    const page = data ?? [];
    rows.push(...page);
    // A short page means the experiment has no more rows.
    if (page.length < pageSize) break;
    const last = page[page.length - 1]!;
    cursor = { timestamp: last.timestamp, bottle_id: last.bottle_id };
  }

  return rows.map(toReading);
}

/**
 * Groups readings into one summary per experiment, newest first.
 *
 * Derived from the readings themselves because the schema has no experiments
 * table; experiment 0 is the testing experiment and sorts first.
 */
export function summarizeExperiments(readings: Reading[], now: number = Date.now()): ExperimentSummary[] {
  const summaries = new Map<number, ExperimentSummary>();

  for (const reading of readings) {
    const existing = summaries.get(reading.experiment_id);
    if (!existing) {
      summaries.set(reading.experiment_id, {
        experiment_id: reading.experiment_id,
        reading_count: 1,
        first_timestamp: reading.timestamp,
        last_timestamp: reading.timestamp,
        concluded: isConcluded(reading.timestamp, now),
      });
      continue;
    }

    existing.reading_count += 1;
    if (reading.timestamp < (existing.first_timestamp ?? reading.timestamp)) {
      existing.first_timestamp = reading.timestamp;
    }
    if (reading.timestamp > (existing.last_timestamp ?? reading.timestamp)) {
      existing.last_timestamp = reading.timestamp;
    }
    // An experiment stays live while any of its samples is still fresh.
    if (!isConcluded(reading.timestamp, now)) existing.concluded = false;
  }

  return [...summaries.values()].sort((left, right) => right.experiment_id - left.experiment_id);
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
