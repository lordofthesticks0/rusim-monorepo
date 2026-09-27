import { supabase } from "./supabase";
import type { Json } from "./types/database";
import type { Metadata, TelemetryReading } from "./types/domain";

export const PARAMETER_NAMES = ["CH4", "CO2", "pH", "Temperature", "Pressure"] as const;

export type DashboardTelemetryRow = TelemetryReading & {
  device_id: string;
  device_code: string;
  device_name: string | null;
  node_id: string;
  node_code: string;
  chamber_id: string;
  chamber_code: string;
};

function normalizeMetadata(value: Json): Metadata {
  if (value === null || typeof value !== "object" || Array.isArray(value)) return {};

  const metadata: Metadata = {};
  for (const [key, entry] of Object.entries(value)) metadata[key] = entry;
  return metadata;
}

function normalizeQuality(value: string): TelemetryReading["quality"] {
  if (value === "valid" || value === "suspect" || value === "invalid") return value;
  throw new Error(`Unsupported telemetry quality: ${value}`);
}

export async function fetchTelemetry(): Promise<DashboardTelemetryRow[]> {
  const { data, error } = await supabase
    .from("telemetry_readings")
    .select(
      `
        id,
        experiment_id,
        experiment_chamber_id,
        session_id,
        recorded_at,
        ch4,
        co2,
        ph,
        temperature,
        pressure,
        quality,
        source_sequence,
        received_at,
        metadata,
        experiment_chamber:experiment_chambers!telemetry_readings_experiment_chamber_fk(
          chamber:chambers!experiment_chambers_chamber_id_fkey(
            id,
            chamber_code,
            node:nodes!bottles_node_id_fkey(
              id,
              node_code,
              device:devices!nodes_device_id_fkey(
                id,
                device_code,
                name
              )
            )
          )
        )
      `,
    )
    .order("recorded_at", { ascending: false })
    .limit(5000);
  if (error) throw error;

  return (data ?? []).map((row): DashboardTelemetryRow => ({
    id: row.id,
    experiment_id: row.experiment_id,
    experiment_chamber_id: row.experiment_chamber_id,
    session_id: row.session_id,
    recorded_at: row.recorded_at,
    ch4: row.ch4,
    co2: row.co2,
    ph: row.ph,
    temperature: row.temperature,
    pressure: row.pressure,
    quality: normalizeQuality(row.quality),
    source_sequence: row.source_sequence,
    received_at: row.received_at,
    metadata: normalizeMetadata(row.metadata),
    device_id: row.experiment_chamber.chamber.node.device.id,
    device_code: row.experiment_chamber.chamber.node.device.device_code,
    device_name: row.experiment_chamber.chamber.node.device.name,
    node_id: row.experiment_chamber.chamber.node.id,
    node_code: row.experiment_chamber.chamber.node.node_code,
    chamber_id: row.experiment_chamber.chamber.id,
    chamber_code: row.experiment_chamber.chamber.chamber_code,
  }));
}
