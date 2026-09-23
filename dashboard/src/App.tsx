import { useEffect, useMemo, useState } from "react";
import { fetchTelemetry, PARAMETER_NAMES, type TelemetryRow } from "./data";

const fallbackRows: TelemetryRow[] = Array.from({ length: 8 }, (_, index) => ({
  device_id: `Device ${String(index + 1).padStart(2, "0")}`,
  recorded_at: new Date(Date.now() - index * 3600000).toISOString(),
  parameter_1: 62 + index, parameter_2: 38 + index * 2, parameter_3: 74 - index,
  parameter_4: 51 + index, parameter_5: 27 + index * 3,
}));

function App() {
  const [rows, setRows] = useState<TelemetryRow[]>(fallbackRows);
  const [selectedDevice, setSelectedDevice] = useState("all");
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  async function loadData() {
    setLoading(true); setError("");
    try { const result = await fetchTelemetry(); if (result.length) setRows(result); }
    catch (err) { setError(err instanceof Error ? err.message : "Could not load Supabase data"); }
    finally { setLoading(false); }
  }
  useEffect(() => { void loadData(); }, []);
  const devices = useMemo(() => [...new Set(rows.map((row) => row.device_id))], [rows]);
  const visibleRows = selectedDevice === "all" ? rows : rows.filter((row) => row.device_id === selectedDevice);
  const latest = visibleRows[0];
  const lastUpdated = latest ? new Date(latest.recorded_at).toLocaleString([], { dateStyle: "medium", timeStyle: "short" }) : "—";
  return <main className="shell">
    <header className="topbar"><div className="brand"><span className="brand-mark">R</span><span>RuSim <small>/ telemetry</small></span></div><div className="status"><span className="pulse" /> {loading ? "Syncing" : "Live connection"}<span className="divider" /> Updated {lastUpdated}</div></header>
    <section className="intro"><div><p className="eyebrow">Operations overview</p><h1>Device telemetry</h1><p className="subtitle">A clear view of your connected environment.</p></div><button className="refresh" onClick={() => void loadData()}>↻ <span>Refresh data</span></button></section>
    {error && <div className="notice">Using temporary preview data. Connect Supabase to load live readings. <span>{error}</span></div>}
    {!error && !import.meta.env.VITE_SUPABASE_URL && <div className="notice">Preview mode · Add your Supabase keys to <code>.env.local</code> when ready.</div>}
    <section className="controls"><div className="control-copy"><span className="label">Showing</span><strong>{selectedDevice === "all" ? "All devices" : selectedDevice}</strong><span className="count">{devices.length} connected</span></div><label>Device <select value={selectedDevice} onChange={(event) => setSelectedDevice(event.target.value)}><option value="all">All devices</option>{devices.map((device) => <option key={device} value={device}>{device}</option>)}</select></label></section>
    <section className="metrics">{PARAMETER_NAMES.map((name, index) => <MetricCard key={name} name={name} value={latest ? Number(Object.values(latest)[index + 2] ?? 0) : 0} index={index} />)}</section>
    <section className="panel"><div className="panel-heading"><div><p className="eyebrow">Across time</p><h2>Recent readings</h2></div><span className="tag">{visibleRows.length} records</span></div><div className="chart"><div className="gridlines"><i /><i /><i /><i /></div><svg viewBox="0 0 800 220" preserveAspectRatio="none" aria-label="Telemetry preview chart"><path d="M0 171 C55 145 75 185 125 142 S195 110 245 132 S315 75 365 101 S425 159 480 110 S550 64 605 95 S680 142 735 72 S770 80 800 48" /><path className="line-secondary" d="M0 112 C70 90 94 132 150 103 S224 139 278 92 S350 136 410 75 S492 105 546 76 S628 116 685 62 S750 90 800 70" /></svg><div className="axis"><span>Now</span><span>− 6 hours</span><span>− 12 hours</span><span>− 18 hours</span><span>− 24 hours</span></div></div></section>
    <footer><span>Data source: Supabase</span><span>•</span><span>Last 24 hours</span></footer>
  </main>;
}

function MetricCard({ name, value, index }: { name: string; value: number; index: number }) {
  const colors = ["blue", "orange", "green", "purple", "pink"];
  return <article className="metric"><div className={`metric-icon ${colors[index]}`}><span /></div><div><p>{name}</p><strong>{value.toFixed(1)}</strong><span className="unit"> units</span></div><span className="trend">↗ 4.2%</span></article>;
}
export default App;
