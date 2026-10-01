import { getSupabase } from "./supabase";
import type { Device, DeviceSession, Reading } from "./types/domain";

export const PARAMETER_NAMES = ["pH", "Temp", "CO2", "CH4", "Pressure"] as const;

export type ParameterName = (typeof PARAMETER_NAMES)[number];

export const PARAMETER_UNITS = {
  pH: "",
  Temp: "°C",
  CO2: "ppm",
  CH4: "ppm",
  Pressure: "kPa",
} as const satisfies Record<ParameterName, string>;

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
    // Preserve the throttle signal (SQLSTATE 42900) so the UI can tell
    // "slow down" apart from "wrong token". Everything else stays generic
    // to avoid oracling device existence.
    if (/too many attempts/i.test(error.message)) {
      throw new Error("Too many attempts. Try again later.");
    }
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
 * Reads every bottle's reading for one experiment.
 *
 * RLS already restricts this to the session's own device, so no device filter
 * is applied here. Ordering by bottle then time keeps a multi-page read
 * contiguous, so `computeBottleStats` sees each bottle's samples together.
 */
export async function fetchExperimentReadings(sessionToken: string, experimentId: number): Promise<Reading[]> {
  const rows: ReadingRow[] = [];
  for (let offset = 0; offset < MAX_READING_ROWS; offset += PAGE_SIZE) {
    const { data, error } = await getSupabase(sessionToken)
      .from("readings")
      .select(READING_COLUMNS)
      .eq("experiment_id", experimentId)
      .order("bottle_id", { ascending: true })
      .order("timestamp", { ascending: true })
      .range(offset, Math.min(offset + PAGE_SIZE - 1, MAX_READING_ROWS - 1));
    if (error) throw error;
    rows.push(...(data ?? []));
    if (!data || data.length < PAGE_SIZE) break;
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
 * Determines which bottles reported at a given sample time.
 *
 * A bottle that is configured on the device but has no row at the latest sample
 * is treated as disconnected: it sent nothing at all for that timestamp. This is
 * distinct from a row that exists with null measurements, which means the bottle
 * reported but that one sensor failed.
 *
 * Returns the connected and disconnected bottle ids for the most recent sample.
 */
export function splitBottlesByPresence(
  readings: Reading[],
  bottleCount: number,
): { latestSample: string | null; connected: number[]; disconnected: number[] } {
  const all = Array.from({ length: bottleCount }, (_, bottleId) => bottleId);
  if (readings.length === 0) {
    return { latestSample: null, connected: [], disconnected: all };
  }

  const latestSample = readings.reduce(
    (latest, reading) => (reading.timestamp > latest ? reading.timestamp : latest),
    readings[0]!.timestamp,
  );
  const reporting = new Set(
    readings.filter((reading) => reading.timestamp === latestSample).map((reading) => reading.bottle_id),
  );

  return {
    latestSample,
    connected: all.filter((bottleId) => reporting.has(bottleId)),
    disconnected: all.filter((bottleId) => !reporting.has(bottleId)),
  };
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
