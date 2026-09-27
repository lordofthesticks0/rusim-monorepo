import { useEffect, useMemo, useState } from "react";
import { fetchTelemetry, PARAMETER_NAMES, type DashboardTelemetryRow } from "./data";

function App() {
  const [rows, setRows] = useState<DashboardTelemetryRow[]>([]);
  const [selectedDevice, setSelectedDevice] = useState("all");
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");

  async function loadData() {
    setLoading(true);
    setError("");
    try {
      const result = await fetchTelemetry();
      setRows(result);
    } catch (err) {
      setRows([]);
      setError(err instanceof Error ? err.message : "Unable to load telemetry data");
    } finally {
      setLoading(false);
    }
  }

  useEffect(() => {
    void loadData();
  }, []);

  const devices = useMemo(() => [...new Set(rows.map((row) => row.device_code))], [rows]);
  const deviceNames = useMemo(
    () => new Map(rows.map((row) => [row.device_code, row.device_name ?? row.device_code])),
    [rows],
  );
  const visibleRows = selectedDevice === "all" ? rows : rows.filter((row) => row.device_code === selectedDevice);
  const latest = visibleRows.length > 0 ? visibleRows[0] : undefined;
  const lastUpdated = latest
    ? new Date(latest.recorded_at).toLocaleString([], { dateStyle: "medium", timeStyle: "short" })
    : "—";
  const connectionStatus = loading
    ? "Loading"
    : error
      ? "Unable to load"
      : rows.length > 0
        ? "Data loaded"
        : "No telemetry data";

  return <main className="shell">
    <header className="topbar"><div className="brand"><span className="brand-mark">R</span><span>RuSim <small>/ telemetry</small></span></div><div className="status"><span className="pulse" /> {connectionStatus}<span className="divider" /> Updated {lastUpdated}</div></header>
    <section className="intro"><div><p className="eyebrow">Operations overview</p><h1>Device telemetry</h1><p className="subtitle">A clear view of your connected environment.</p></div><button className="refresh" onClick={() => void loadData()}>↻ <span>Refresh data</span></button></section>
    {error && <div className="notice">Unable to load telemetry data. <span>{error}</span></div>}
    {!loading && !error && rows.length === 0 && <div className="notice">No telemetry data is available.</div>}
    <section className="controls"><div className="control-copy"><span className="label">Showing</span><strong>{selectedDevice === "all" ? "All devices" : deviceNames.get(selectedDevice) ?? selectedDevice}</strong><span className="count">{devices.length} connected</span></div><label>Device <select value={selectedDevice} onChange={(event) => setSelectedDevice(event.target.value)}><option value="all">All devices</option>{devices.map((device) => <option key={device} value={device}>{deviceNames.get(device) ?? device}</option>)}</select></label></section>
    <section className="metrics">{PARAMETER_NAMES.map((name, index) => <MetricCard key={name} name={name} value={latest ? getMetricValue(latest, name) : null} index={index} />)}</section>
    <section className="panel"><div className="panel-heading"><div><p className="eyebrow">Visual placeholder</p><h2>Chart preview (placeholder)</h2></div><span className="tag">{visibleRows.length} records</span></div><div className="chart"><div className="gridlines"><i /><i /><i /><i /></div><svg viewBox="0 0 800 220" preserveAspectRatio="none" aria-label="Placeholder chart not plotted from Supabase data"><path d="M0 171 C55 145 75 185 125 142 S195 110 245 132 S315 75 365 101 S425 159 480 110 S550 64 605 95 S680 142 735 72 S770 80 800 48" /><path className="line-secondary" d="M0 112 C70 90 94 132 150 103 S224 139 278 92 S350 136 410 75 S492 105 546 76 S628 116 685 62 S750 90 800 70" /></svg><div className="axis"><span>Placeholder</span><span>—</span><span>—</span><span>—</span><span>—</span></div></div></section>
    <footer><span>Data source: Supabase</span><span>•</span><span>Fetched telemetry</span></footer>
  </main>;
}

function getMetricValue(row: DashboardTelemetryRow, name: (typeof PARAMETER_NAMES)[number]): number {
  switch (name) {
    case "CH4": return row.ch4;
    case "CO2": return row.co2;
    case "pH": return row.ph;
    case "Temperature": return row.temperature;
    case "Pressure": return row.pressure;
  }
}

function MetricCard({ name, value, index }: { name: string; value: number | null; index: number }) {
  const colors = ["blue", "orange", "green", "purple", "pink"];
  return <article className="metric"><div className={`metric-icon ${colors[index]}`}><span /></div><div><p>{name}</p><strong>{value === null ? "—" : value.toFixed(1)}</strong><span className="unit">{value === null ? "" : " units"}</span></div><span className="trend">{value === null ? "—" : "↗ 4.2%"}</span></article>;
}

export default App;
