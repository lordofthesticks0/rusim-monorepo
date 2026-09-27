import { useEffect, useMemo, useState } from "react";
import {
  fetchChambers,
  fetchMonitoringStatus,
  fetchTelemetry,
  PARAMETER_NAMES,
  type DashboardChamber,
  type MonitoringStatus,
  type DashboardTelemetryRow,
} from "./data";

type ChartParameter = (typeof PARAMETER_NAMES)[number];

function App() {
  const [rows, setRows] = useState<DashboardTelemetryRow[]>([]);
  const [chambers, setChambers] = useState<DashboardChamber[]>([]);
  const [selectedDevice, setSelectedDevice] = useState("all");
  const [selectedChamber, setSelectedChamber] = useState("");
  const [chartParameter, setChartParameter] = useState<ChartParameter>("CH4");
  const [monitoringStatus, setMonitoringStatus] = useState<MonitoringStatus | null>(null);
  const [statusLoading, setStatusLoading] = useState(false);
  const [statusError, setStatusError] = useState("");
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");

  async function loadData() {
    setLoading(true);
    setError("");
    try {
      const [telemetry, chamberCatalog] = await Promise.all([fetchTelemetry(), fetchChambers()]);
      setRows(telemetry);
      setChambers(chamberCatalog);
    } catch (err) {
      setRows([]);
      setChambers([]);
      setError(err instanceof Error ? err.message : "Unable to load telemetry data");
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => {
    void loadData();
  }, []);

  const devices = useMemo(
    () => uniqueBy(chambers, (chamber) => chamber.device_code),
    [chambers],
  );
  const deviceChambers = useMemo(
    () => selectedDevice === "all"
      ? chambers
      : chambers.filter((chamber) => chamber.device_code === selectedDevice),
    [chambers, selectedDevice],
  );
  const defaultChamber = useMemo(
    () => deviceChambers.find((chamber) => rows.some(
      (row) => row.device_code === chamber.device_code && row.chamber_code === chamber.chamber_code,
    )) ?? deviceChambers[0],
    [deviceChambers, rows],
  );
  const activeChamber = deviceChambers.find((chamber) => chamber.chamber_code === selectedChamber)
    ?? defaultChamber;
  const selectedRows = useMemo(
    () => rows
      .filter((row) => activeChamber !== undefined && row.chamber_code === activeChamber.chamber_code)
      .sort((left, right) => Date.parse(left.recorded_at) - Date.parse(right.recorded_at)),
    [activeChamber, rows],
  );
  const latest = selectedRows.length > 0 ? selectedRows[selectedRows.length - 1] : undefined;
  const lastUpdated = latest
    ? new Date(latest.recorded_at).toLocaleString([], { dateStyle: "medium", timeStyle: "short" })
    : "-";
  const chartPoints = useMemo(
    () => createChartPoints(selectedRows, chartParameter),
    [chartParameter, selectedRows],
  );
  const timeLabels = useMemo(
    () => createTimeLabels(selectedRows),
    [selectedRows],
  );
  const connectionStatus = loading
    ? "Loading"
    : error
      ? "Unable to load"
      : rows.length > 0
        ? "Data loaded"
        : "No telemetry data";

  useEffect(() => {
    let cancelled = false;
    if (!activeChamber) {
      setMonitoringStatus(null);
      setStatusError("");
      setStatusLoading(false);
      return () => {
        cancelled = true;
      };
    }

    setStatusLoading(true);
    setStatusError("");
    void fetchMonitoringStatus(activeChamber)
      .then((status) => {
        if (!cancelled) setMonitoringStatus(status);
      })
      .catch((err: unknown) => {
        if (!cancelled) {
          setMonitoringStatus(null);
          setStatusError(err instanceof Error ? err.message : "Unable to load monitoring status");
        }
      })
      .finally(() => {
        if (!cancelled) setStatusLoading(false);
      });

    return () => {
      cancelled = true;
    };
  }, [activeChamber]);

  function handleDeviceChange(deviceCode: string) {
    setSelectedDevice(deviceCode);
    setSelectedChamber("");
  }

  return <main className="shell">
    <header className="topbar"><div className="brand"><span className="brand-mark">R</span><span>RuSim <small>/ telemetry</small></span></div><div className="status"><span className="pulse" /> {connectionStatus}<span className="divider" /> Updated {lastUpdated}</div></header>
    <section className="intro"><div><p className="eyebrow">Operations overview</p><h1>Device telemetry</h1><p className="subtitle">A clear view of your connected environment.</p></div><button className="refresh" onClick={() => void loadData()}>Refresh data</button></section>
    {error && <div className="notice">Unable to load telemetry data. <span>{error}</span></div>}
    {!loading && !error && rows.length === 0 && <div className="notice">No telemetry data is available.</div>}
    <section className="controls">
      <div className="control-copy"><span className="label">Showing</span><strong>{activeChamber?.chamber_name ?? activeChamber?.chamber_code ?? "No chamber selected"}</strong><span className="count">{devices.length} device{devices.length === 1 ? "" : "s"}</span></div>
      <label>Device <select value={selectedDevice} onChange={(event) => handleDeviceChange(event.target.value)}><option value="all">All devices</option>{devices.map((device) => <option key={device.device_code} value={device.device_code}>{device.device_name ?? device.device_code}</option>)}</select></label>
      <label>Chamber <select value={activeChamber?.chamber_code ?? ""} onChange={(event) => setSelectedChamber(event.target.value)} disabled={deviceChambers.length === 0}><option value="" disabled>Select chamber</option>{deviceChambers.map((chamber) => <option key={chamber.chamber_code} value={chamber.chamber_code}>{chamber.chamber_name ?? chamber.chamber_code}</option>)}</select></label>
    </section>
    {activeChamber && <div className="notice">Device: {activeChamber.device_name ?? activeChamber.device_code} <span>Node: {activeChamber.node_code} | Chamber: {activeChamber.chamber_code}</span></div>}
    <section className="metrics">
      <StatusCard label="System" value={statusLoading ? "Loading" : !activeChamber || statusError ? "Unavailable" : monitoringStatus?.system_active ? "Active" : "Inactive"} />
      <StatusCard label="Device" value={statusLoading ? "Loading" : humanizeStatus(monitoringStatus?.device?.status)} />
      <StatusCard label="Node" value={statusLoading ? "Loading" : humanizeStatus(monitoringStatus?.node?.status)} />
      <StatusCard label="Chamber" value={statusLoading ? "Loading" : humanizeStatus(monitoringStatus?.chamber?.status)} />
      <StatusCard label="Experiment" value={statusLoading ? "Loading" : monitoringStatus?.experiment?.experiment_code ?? "No active experiment"} detail={monitoringStatus?.experiment?.name} />
    </section>
    <section className="metrics">{PARAMETER_NAMES.map((name, index) => <MetricCard key={name} name={name} value={latest ? getMetricValue(latest, name) : null} index={index} />)}</section>
    {!loading && !error && activeChamber && selectedRows.length === 0 && <div className="notice">No telemetry data is available for this chamber.</div>}
    <section className="panel"><div className="panel-heading"><div><p className="eyebrow">Across time</p><h2>Recent readings</h2></div><div><select aria-label="Chart parameter" value={chartParameter} onChange={(event) => { if (isChartParameter(event.target.value)) setChartParameter(event.target.value); }}>{PARAMETER_NAMES.map((name) => <option key={name} value={name}>{name}</option>)}</select><span className="tag">{selectedRows.length} records</span></div></div><div className="chart">{chartPoints.length < 2 ? <div className="notice">Not enough data for chart.</div> : <><div className="gridlines"><i /><i /><i /><i /></div><svg viewBox="0 0 800 220" preserveAspectRatio="none" aria-label={`${chartParameter} telemetry chart`}><path d={chartPath(chartPoints)} /></svg><div className="axis">{timeLabels.map((label) => <span key={label}>{label}</span>)}</div></>}</div></section>
    <footer><span>Data source: Supabase</span><span>-</span><span>Selected chamber telemetry</span></footer>
  </main>;
}

function uniqueBy<T>(items: T[], key: (item: T) => string): T[] {
  const seen = new Set<string>();
  return items.filter((item) => {
    const value = key(item);
    if (seen.has(value)) return false;
    seen.add(value);
    return true;
  });
}

function getMetricValue(row: DashboardTelemetryRow, name: ChartParameter): number {
  switch (name) {
    case "CH4": return row.ch4;
    case "CO2": return row.co2;
    case "pH": return row.ph;
    case "Temperature": return row.temperature;
    case "Pressure": return row.pressure;
  }
}

function isChartParameter(value: string): value is ChartParameter {
  return PARAMETER_NAMES.some((name) => name === value);
}

function getChartValue(row: DashboardTelemetryRow, name: ChartParameter): number {
  return getMetricValue(row, name);
}

function createChartPoints(rows: DashboardTelemetryRow[], parameter: ChartParameter): Array<{ x: number; y: number }> {
  if (rows.length < 2) return [];
  const values = rows.map((row) => getChartValue(row, parameter));
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min;
  return values.map((value, index) => ({
    x: (index / (values.length - 1)) * 800,
    y: range === 0 ? 110 : 190 - ((value - min) / range) * 160,
  }));
}

function chartPath(points: Array<{ x: number; y: number }>): string {
  return points.map((point, index) => `${index === 0 ? "M" : "L"}${point.x} ${point.y}`).join(" ");
}

function createTimeLabels(rows: DashboardTelemetryRow[]): string[] {
  if (rows.length === 0) return [];
  const count = Math.min(5, rows.length);
  const indexes = Array.from({ length: count }, (_, index) => Math.round((index * (rows.length - 1)) / (count - 1 || 1)));
  return indexes.flatMap((index) => {
    const row = rows[index];
    return row === undefined
      ? []
      : [new Date(row.recorded_at).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })];
  });
}

function MetricCard({ name, value, index }: { name: string; value: number | null; index: number }) {
  const colors = ["blue", "orange", "green", "purple", "pink"];
  return <article className="metric"><div className={`metric-icon ${colors[index]}`}><span /></div><div><p>{name}</p><strong>{value === null ? "-" : value.toFixed(1)}</strong><span className="unit">{value === null ? "" : " units"}</span></div><span className="trend">{value === null ? "-" : "Data"}</span></article>;
}

function StatusCard({ label, value, detail }: { label: string; value: string; detail?: string }) {
  return <article className="metric"><p>{label}</p><strong>{value}</strong>{detail && <span className="unit">{detail}</span>}</article>;
}

function humanizeStatus(status: string | undefined): string {
  if (!status) return "Unavailable";
  return status.charAt(0).toUpperCase() + status.slice(1);
}

export default App;
