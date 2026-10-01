export type Metadata = Record<string, unknown>;

/**
 * A device as registered in `device_data.devices`.
 *
 * Identifiers are integers assigned by the factory, not UUIDs. Device 0 is the
 * development device; production numbering starts at 1.
 */
export interface Device {
  device_id: number;
  bottle_count: number;
  owner_name: string | null;
  owner_email: string | null;
}

/** The authenticated device plus the session that scopes every read. */
export interface DeviceSession {
  sessionToken: string;
  device: Device;
  expiresAt: string;
}

/**
 * One sensor sample from `device_data.readings`.
 *
 * The primary key is (device_id, bottle_id, timestamp), so a bottle reports many
 * samples over time within a single experiment. `experiment_id` groups those
 * samples but is no longer part of the key.
 *
 * Every measurement is nullable. A null means the bottle reported for that
 * timestamp but that individual sensor produced no value, which is different
 * from the bottle being absent from the sample entirely (offline). `temp` is the
 * schema's column name for temperature.
 */
export interface Reading {
  device_id: number;
  bottle_id: number;
  experiment_id: number;
  timestamp: string;
  ph: number | null;
  pressure: number | null;
  temp: number | null;
  co2: number | null;
  ch4: number | null;
}

/** Experiment 0 is the testing experiment; 1 and above are regular experiments. */
export const TESTING_EXPERIMENT_ID = 0;

export function isTestingExperiment(experimentId: number): boolean {
  return experimentId === TESTING_EXPERIMENT_ID;
}

export function formatExperimentId(experimentId: number): string {
  return isTestingExperiment(experimentId) ? "Experiment 0 (testing)" : `Experiment ${experimentId}`;
}
