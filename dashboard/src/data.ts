import { supabase } from "./supabase";
import type { Database, Json } from "./types/database";
import type {
  ChamberStatus,
  DeviceStatus,
  Experiment,
  ExperimentStatus,
  Metadata,
  NodeStatus,
  TelemetryReading,
} from "./types/domain";

export const PARAMETER_NAMES = ["CH4", "CO2", "pH", "Temperature", "Pressure"] as const;

export type DashboardTelemetryRow = TelemetryReading & {
  device_id: string;
  device_code: string;
  device_name: string | null;
  node_id: string;
  node_code: string;
  chamber_id: string;
  chamber_code: string;
  chamber_name: string | null;
};

export type DashboardChamber = {
  id: string;
  chamber_code: string;
  chamber_name: string | null;
  node_id: string;
  node_code: string;
  node_name: string;
  device_id: string;
  device_code: string;
  device_name: string | null;
  device_status: DeviceStatus;
  node_status: NodeStatus;
  chamber_status: ChamberStatus;
};

export type MonitoringStatus = {
  system_active: boolean;
  experiment: {
    id: string;
    experiment_code: string;
    name: string;
    status: string;
    started_at: string | null;
  } | null;
  device: {
    id: string;
    device_code: string;
    name: string;
    status: DeviceStatus;
  } | null;
  node: {
    id: string;
    node_code: string;
    name: string;
    status: NodeStatus;
  } | null;
  chamber: {
    id: string;
    chamber_code: string;
    name: string | null;
    status: ChamberStatus;
  } | null;
};

export type TelemetryDatabaseRow = Database["public"]["Tables"]["telemetry_readings"]["Row"];
export type DeviceDatabaseRow = Database["public"]["Tables"]["devices"]["Row"];
export type NodeDatabaseRow = Database["public"]["Tables"]["nodes"]["Row"];
export type ChamberDatabaseRow = Database["public"]["Tables"]["chambers"]["Row"];
export type ExperimentDatabaseRow = Database["public"]["Tables"]["experiments"]["Row"];

export type ExperimentHistoryItem = Pick<Experiment, "id" | "experiment_code" | "name" | "description" | "status" | "started_at" | "ended_at">;

export type ExperimentChamberSummary = {
  id: string;
  experiment_id: string;
  chamber_id: string;
  chamber_code: string;
  chamber_name: string | null;
  node_code: string;
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

export function normalizeDeviceStatus(value: string): DeviceStatus {
  switch (value) {
    case "provisioning":
    case "online":
    case "offline":
    case "degraded":
    case "maintenance":
    case "retired":
      return value;
  }
  throw new Error(`Unsupported device status: ${value}`);
}

export function normalizeNodeStatus(value: string): NodeStatus {
  switch (value) {
    case "provisioning":
    case "online":
    case "offline":
    case "degraded":
    case "fault":
    case "maintenance":
    case "retired":
      return value;
  }
  throw new Error(`Unsupported node status: ${value}`);
}

export function normalizeChamberStatus(value: string): ChamberStatus {
  switch (value) {
    case "configured":
    case "ready":
    case "running":
    case "complete":
    case "empty":
    case "missing":
    case "fault":
    case "disabled":
      return value;
  }
  throw new Error(`Unsupported chamber status: ${value}`);
}

export function normalizeExperimentStatus(value: string): ExperimentStatus {
  switch (value) {
    case "draft":
    case "scheduled":
    case "active":
    case "paused":
    case "completed":
    case "cancelled":
    case "failed":
      return value;
  }
  throw new Error(`Unsupported experiment status: ${value}`);
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
            name,
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
    chamber_name: row.experiment_chamber.chamber.name,
  }));
}

export async function fetchChambers(): Promise<DashboardChamber[]> {
  const { data, error } = await supabase
    .from("chambers")
    .select(
      `
        id,
        chamber_code,
        name,
        status,
        node:nodes!bottles_node_id_fkey(
          id,
          node_code,
          name,
          status,
          device:devices!nodes_device_id_fkey(
            id,
            device_code,
            name,
            status
          )
        )
      `,
    )
    .order("chamber_code", { ascending: true });
  if (error) throw error;

  return (data ?? []).map((row): DashboardChamber => ({
    id: row.id,
    chamber_code: row.chamber_code,
    chamber_name: row.name,
    node_id: row.node.id,
    node_code: row.node.node_code,
    node_name: row.node.name,
    device_id: row.node.device.id,
    device_code: row.node.device.device_code,
    device_name: row.node.device.name,
    device_status: normalizeDeviceStatus(row.node.device.status),
    node_status: normalizeNodeStatus(row.node.status),
    chamber_status: normalizeChamberStatus(row.status),
  }));
}

export async function fetchMonitoringStatus(
  selectedChamber: DashboardChamber | undefined,
): Promise<MonitoringStatus> {
  const { data, error } = await supabase
    .from("experiments")
    .select("id, experiment_code, name, status, started_at")
    .eq("status", "active")
    .order("started_at", { ascending: false, nullsFirst: false })
    .limit(1)
    .maybeSingle();
  if (error) throw error;

  const experiment = data
    ? {
      id: data.id,
      experiment_code: data.experiment_code,
      name: data.name,
      status: data.status,
      started_at: data.started_at,
    }
    : null;
  const device = selectedChamber
    ? {
      id: selectedChamber.device_id,
      device_code: selectedChamber.device_code,
      name: selectedChamber.device_name ?? selectedChamber.device_code,
      status: selectedChamber.device_status,
    }
    : null;
  const node = selectedChamber
    ? {
      id: selectedChamber.node_id,
      node_code: selectedChamber.node_code,
      name: selectedChamber.node_name,
      status: selectedChamber.node_status,
    }
    : null;
  const chamber = selectedChamber
    ? {
      id: selectedChamber.id,
      chamber_code: selectedChamber.chamber_code,
      name: selectedChamber.chamber_name,
      status: selectedChamber.chamber_status,
    }
    : null;

  return {
    system_active: experiment !== null
      && device?.status === "online"
      && node?.status === "online"
      && chamber?.status === "running",
    experiment,
    device,
    node,
    chamber,
  };
}

export function mapRealtimeTelemetryRow(
  row: TelemetryDatabaseRow,
  context: DashboardTelemetryRow | undefined,
): DashboardTelemetryRow | undefined {
  if (!context || context.experiment_chamber_id !== row.experiment_chamber_id) return undefined;

  return {
    ...context,
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
  };
}

export async function fetchExperiments(): Promise<ExperimentHistoryItem[]> {
  const { data, error } = await supabase
    .from("experiments")
    .select("id, experiment_code, name, description, status, started_at, ended_at")
    .order("started_at", { ascending: false, nullsFirst: false });
  if (error) throw error;

  return (data ?? []).map((row): ExperimentHistoryItem => ({
    id: row.id,
    experiment_code: row.experiment_code,
    name: row.name,
    description: row.description,
    status: normalizeExperimentStatus(row.status),
    started_at: row.started_at,
    ended_at: row.ended_at,
  }));
}

export async function fetchExperimentChambers(experimentId: string): Promise<ExperimentChamberSummary[]> {
  const { data, error } = await supabase
    .from("experiment_chambers")
    .select(
      `
        id,
        experiment_id,
        chamber_id,
        chamber:chambers!experiment_chambers_chamber_id_fkey(
          chamber_code,
          name,
          node:nodes!bottles_node_id_fkey(
            node_code
          )
        )
      `,
    )
    .eq("experiment_id", experimentId)
    .order("assigned_at", { ascending: true });
  if (error) throw error;

  return (data ?? []).map((row): ExperimentChamberSummary => ({
    id: row.id,
    experiment_id: row.experiment_id,
    chamber_id: row.chamber_id,
    chamber_code: row.chamber.chamber_code,
    chamber_name: row.chamber.name,
    node_code: row.chamber.node.node_code,
  }));
}

export async function fetchExperimentTelemetry(
  experimentId: string,
  experimentChamberId: string,
): Promise<DashboardTelemetryRow[]> {
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
            name,
            node:nodes!bottles_node_id_fkey(
              id,
              node_code,
              name,
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
    .eq("experiment_id", experimentId)
    .eq("experiment_chamber_id", experimentChamberId)
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
    chamber_name: row.experiment_chamber.chamber.name,
  }));
}
