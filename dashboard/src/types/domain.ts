export type Metadata = Record<string, unknown>;

export type DeviceStatus =
  | "provisioning"
  | "online"
  | "offline"
  | "degraded"
  | "maintenance"
  | "retired";

export type NodeStatus =
  | "provisioning"
  | "online"
  | "offline"
  | "degraded"
  | "fault"
  | "maintenance"
  | "retired";

export type ChamberStatus =
  | "configured"
  | "ready"
  | "running"
  | "complete"
  | "empty"
  | "missing"
  | "fault"
  | "disabled";

export type ExperimentStatus =
  | "draft"
  | "scheduled"
  | "active"
  | "paused"
  | "completed"
  | "cancelled"
  | "failed";

export type ExperimentSessionStatus =
  | "active"
  | "paused"
  | "interrupted"
  | "completed"
  | "abandoned";

export type TelemetryQuality = "valid" | "suspect" | "invalid";

export type ExperimentDeviceRole = "primary" | "secondary" | "gateway" | "reference";

export interface Device {
  id: string;
  device_code: string;
  name: string;
  serial_number: string | null;
  status: DeviceStatus;
  last_seen_at: string | null;
  metadata: Metadata;
  created_at: string;
  updated_at: string;
}

export interface Node {
  id: string;
  device_id: string;
  node_code: string;
  name: string;
  status: NodeStatus;
  last_seen_at: string | null;
  metadata: Metadata;
  created_at: string;
  updated_at: string;
}

export interface Chamber {
  id: string;
  node_id: string;
  chamber_code: string;
  name: string | null;
  status: ChamberStatus;
  enabled: boolean;
  metadata: Metadata;
  created_at: string;
  updated_at: string;
}

export interface Experiment {
  id: string;
  experiment_code: string;
  name: string;
  description: string | null;
  status: ExperimentStatus;
  planned_start_at: string | null;
  started_at: string | null;
  ended_at: string | null;
  created_by: string | null;
  metadata: Metadata;
  created_at: string;
  updated_at: string;
}

export interface ExperimentDevice {
  experiment_id: string;
  device_id: string;
  role: ExperimentDeviceRole;
  assigned_at: string;
  removed_at: string | null;
  metadata: Metadata;
}

export interface ExperimentNode {
  id: string;
  experiment_id: string;
  node_id: string;
  assigned_at: string;
  removed_at: string | null;
  metadata: Metadata;
}

export interface ExperimentChamber {
  id: string;
  experiment_id: string;
  chamber_id: string;
  assigned_at: string;
  removed_at: string | null;
  metadata: Metadata;
}

export interface ExperimentSession {
  id: string;
  experiment_id: string;
  session_number: number;
  status: ExperimentSessionStatus;
  started_at: string;
  ended_at: string | null;
  resume_of_session_id: string | null;
  interruption_reason: string | null;
  metadata: Metadata;
  created_at: string;
}

export interface TelemetryReading {
  id: number;
  experiment_id: string;
  experiment_chamber_id: string;
  session_id: string;
  recorded_at: string;
  ch4: number;
  co2: number;
  ph: number;
  temperature: number;
  pressure: number;
  quality: TelemetryQuality;
  source_sequence: number | null;
  received_at: string;
  metadata: Metadata;
}

export interface DeviceStatusEvent {
  id: number;
  device_id: string;
  experiment_id: string | null;
  recorded_at: string;
  status: DeviceStatus;
  message: string | null;
  metadata: Metadata;
}

export interface NodeStatusEvent {
  id: number;
  node_id: string;
  experiment_id: string | null;
  recorded_at: string;
  status: NodeStatus;
  message: string | null;
  metadata: Metadata;
}

export interface ChamberStatusEvent {
  id: number;
  chamber_id: string;
  experiment_id: string | null;
  recorded_at: string;
  status: ChamberStatus;
  message: string | null;
  metadata: Metadata;
}
