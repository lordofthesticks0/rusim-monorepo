import { supabase } from "./supabase";

export const PARAMETER_NAMES = ["Parameter 1", "Parameter 2", "Parameter 3", "Parameter 4", "Parameter 5"];

export type TelemetryRow = {
  device_id: string;
  recorded_at: string;
  parameter_1: number;
  parameter_2: number;
  parameter_3: number;
  parameter_4: number;
  parameter_5: number;
};

export async function fetchTelemetry(): Promise<TelemetryRow[]> {
  if (!supabase) return [];
  const { data, error } = await supabase
    .from("time_series_readings")
    .select("device_id, recorded_at, parameter_1, parameter_2, parameter_3, parameter_4, parameter_5")
    .order("recorded_at", { ascending: false })
    .limit(5000);
  if (error) throw error;
  return (data ?? []) as TelemetryRow[];
}
