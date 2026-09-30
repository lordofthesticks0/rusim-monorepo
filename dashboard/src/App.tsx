import { useEffect, useMemo, useRef, useState } from "react";
import { supabase } from "./supabase";
import {
  fetchChambers,
  fetchExperimentChambers,
  fetchExperimentResults,
  fetchExperimentTelemetry,
  fetchExperiments,
  fetchMonitoringStatus,
  fetchTelemetry,
  mapRealtimeTelemetryRow,
  normalizeChamberStatus,
  normalizeDeviceStatus,
  normalizeNodeStatus,
  PARAMETER_NAMES,
  PARAMETER_UNITS,
  type ChamberDatabaseRow,
  type DeviceDatabaseRow,
  type DashboardChamber,
  type ExperimentDatabaseRow,
  type MonitoringStatus,
  type NodeDatabaseRow,
  type DashboardTelemetryRow,
  type ExperimentChamberSummary,
  type ExperimentChamberResult,
  type ExperimentHistoryItem,
  type ExperimentResults,
  type TelemetryDatabaseRow,
} from "./data";

type ChartParameter = (typeof PARAMETER_NAMES)[number];
type RealtimeStatus = "connecting" | "connected" | "disconnected" | "error";
type DashboardView = "monitoring" | "history";
type HistoryPanel = "data" | "results";

function App() {
  const [view, setView] = useState<DashboardView>("monitoring");
  const [rows, setRows] = useState<DashboardTelemetryRow[]>([]);
  const [chambers, setChambers] = useState<DashboardChamber[]>([]);
  const [selectedDevice, setSelectedDevice] = useState("all");
  const [selectedChamber, setSelectedChamber] = useState("");
  const [chartParameter, setChartParameter] = useState<ChartParameter>("CH4");
  const [monitoringStatus, setMonitoringStatus] = useState<MonitoringStatus | null>(null);
  const [statusLoading, setStatusLoading] = useState(false);
  const [statusError, setStatusError] = useState("");
  const [realtimeStatus, setRealtimeStatus] = useState<RealtimeStatus>("disconnected");
  const [statusRefreshToken, setStatusRefreshToken] = useState(0);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const [experiments, setExperiments] = useState<ExperimentHistoryItem[]>([]);
  const [historyExperimentId, setHistoryExperimentId] = useState("");
  const [historyPanel, setHistoryPanel] = useState<HistoryPanel>("data");
  const [historyChambers, setHistoryChambers] = useState<ExperimentChamberSummary[]>([]);
  const [historyChamberId, setHistoryChamberId] = useState("");
  const [historyRows, setHistoryRows] = useState<DashboardTelemetryRow[]>([]);
  const [historyLoading, setHistoryLoading] = useState(false);
  const [historyTelemetryLoading, setHistoryTelemetryLoading] = useState(false);
  const [historyError, setHistoryError] = useState("");
  const [historyRefreshToken, setHistoryRefreshToken] = useState(0);
  const [experimentResults, setExperimentResults] = useState<ExperimentResults | null>(null);
  const [resultsLoading, setResultsLoading] = useState(false);
  const [resultsError, setResultsError] = useState("");
  const rowsRef = useRef(rows);
  rowsRef.current = rows;

  async function loadData() {
    setLoading(true);
    setError("");
    setStatusRefreshToken((value) => value + 1);
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

  useEffect(() => {
    if (view !== "history") return;

    setHistoryLoading(true);
    setHistoryError("");
    void fetchExperiments()
      .then((result) => {
        setExperiments(result);
        if (historyExperimentId && !result.some((experiment) => experiment.id === historyExperimentId)) {
          setHistoryExperimentId("");
          setHistoryChambers([]);
          setHistoryChamberId("");
          setHistoryRows([]);
        }
      })
      .catch((err: unknown) => setHistoryError(err instanceof Error ? err.message : "Unable to load experiments"))
      .finally(() => setHistoryLoading(false));
  }, [view, historyRefreshToken]);

  useEffect(() => {
    if (view !== "history" || !historyExperimentId) return;

    setHistoryLoading(true);
    setHistoryError("");
    setHistoryChambers([]);
    setHistoryChamberId("");
    setHistoryRows([]);
    void fetchExperimentChambers(historyExperimentId)
      .then((result) => {
        setHistoryChambers(result);
        setHistoryChamberId(result[0]?.id ?? "");
      })
      .catch((err: unknown) => setHistoryError(err instanceof Error ? err.message : "Unable to load experiment chambers"))
      .finally(() => setHistoryLoading(false));
  }, [view, historyExperimentId]);

  useEffect(() => {
    if (view !== "history" || historyPanel !== "data" || !historyExperimentId || !historyChamberId) {
      setHistoryTelemetryLoading(false);
      return;
    }

    setHistoryTelemetryLoading(true);
    setHistoryError("");
    void fetchExperimentTelemetry(historyExperimentId, historyChamberId)
      .then(setHistoryRows)
      .catch((err: unknown) => {
        setHistoryRows([]);
        setHistoryError(err instanceof Error ? err.message : "Unable to load historical telemetry");
      })
      .finally(() => setHistoryTelemetryLoading(false));
  }, [view, historyPanel, historyExperimentId, historyChamberId]);

  useEffect(() => {
    if (view !== "history" || historyPanel !== "results" || !historyExperimentId) {
      setResultsLoading(false);
      return;
    }

    setResultsLoading(true);
    setResultsError("");
    void fetchExperimentResults(historyExperimentId)
      .then(setExperimentResults)
      .catch((err: unknown) => {
        setExperimentResults(null);
        setResultsError(err instanceof Error ? err.message : "Unable to load experiment results");
      })
      .finally(() => setResultsLoading(false));
  }, [view, historyPanel, historyExperimentId]);

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
  const selectedTelemetryContext = rows.find(
    (row) => activeChamber !== undefined && row.chamber_id === activeChamber.id,
  );
  const experimentChamberId = selectedTelemetryContext?.experiment_chamber_id;
  const activeExperimentId = monitoringStatus?.experiment?.id;
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
  const selectedHistoryExperiment = experiments.find((experiment) => experiment.id === historyExperimentId);

  useEffect(() => {
    let cancelled = false;
    if (view !== "monitoring" || !activeChamber) {
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
  }, [view, activeChamber?.id, selectedChamber, selectedDevice, statusRefreshToken]);

  useEffect(() => {
    if (view !== "monitoring" || !activeChamber) {
      setRealtimeStatus("disconnected");
      return;
    }

    setRealtimeStatus("connecting");
    const channel = supabase.channel(`dashboard-monitoring-${activeChamber.device_id}-${activeChamber.node_id}-${activeChamber.id}`);

    const applyTelemetry = (row: TelemetryDatabaseRow) => {
      const context = rowsRef.current.find((item) => item.experiment_chamber_id === row.experiment_chamber_id);
      const nextRow = mapRealtimeTelemetryRow(row, context);
      if (!nextRow) return;

      setRows((current) => {
        const next = current.filter((item) => item.id !== nextRow.id);
        next.push(nextRow);
        return next.sort((left, right) => Date.parse(right.recorded_at) - Date.parse(left.recorded_at));
      });
    };

    if (experimentChamberId) {
      channel
        .on<TelemetryDatabaseRow>("postgres_changes", { event: "INSERT", schema: "public", table: "telemetry_readings", filter: `experiment_chamber_id=eq.${experimentChamberId}` }, (payload) => applyTelemetry(payload.new))
        .on<TelemetryDatabaseRow>("postgres_changes", { event: "UPDATE", schema: "public", table: "telemetry_readings", filter: `experiment_chamber_id=eq.${experimentChamberId}` }, (payload) => applyTelemetry(payload.new));
    }

    channel.on<DeviceDatabaseRow>("postgres_changes", { event: "UPDATE", schema: "public", table: "devices", filter: `id=eq.${activeChamber.device_id}` }, (payload) => {
      const status = normalizeDeviceStatus(payload.new.status);
      setChambers((current) => current.map((chamber) => chamber.device_id === payload.new.id ? { ...chamber, device_status: status } : chamber));
      setMonitoringStatus((current) => current ? recomputeSystemActive({ ...current, device: current.device && current.device.id === payload.new.id ? { ...current.device, status } : current.device }) : current);
    });
    channel.on<NodeDatabaseRow>("postgres_changes", { event: "UPDATE", schema: "public", table: "nodes", filter: `id=eq.${activeChamber.node_id}` }, (payload) => {
      const status = normalizeNodeStatus(payload.new.status);
      setChambers((current) => current.map((chamber) => chamber.node_id === payload.new.id ? { ...chamber, node_status: status } : chamber));
      setMonitoringStatus((current) => current ? recomputeSystemActive({ ...current, node: current.node && current.node.id === payload.new.id ? { ...current.node, status } : current.node }) : current);
    });
    channel.on<ChamberDatabaseRow>("postgres_changes", { event: "UPDATE", schema: "public", table: "chambers", filter: `id=eq.${activeChamber.id}` }, (payload) => {
      const status = normalizeChamberStatus(payload.new.status);
      setChambers((current) => current.map((chamber) => chamber.id === payload.new.id ? { ...chamber, chamber_status: status } : chamber));
      setMonitoringStatus((current) => current ? recomputeSystemActive({ ...current, chamber: current.chamber && current.chamber.id === payload.new.id ? { ...current.chamber, status } : current.chamber }) : current);
    });
    if (activeExperimentId) {
      channel.on<ExperimentDatabaseRow>("postgres_changes", { event: "UPDATE", schema: "public", table: "experiments", filter: `id=eq.${activeExperimentId}` }, (payload) => {
        setMonitoringStatus((current) => current ? recomputeSystemActive({
          ...current,
          experiment: current.experiment && current.experiment.id === payload.new.id
            ? { ...current.experiment, status: payload.new.status }
            : current.experiment,
        }) : current);
      });
    }

    channel.subscribe((status) => {
      if (status === "SUBSCRIBED") setRealtimeStatus("connected");
      if (status === "CHANNEL_ERROR" || status === "TIMED_OUT") setRealtimeStatus("error");
      if (status === "CLOSED") setRealtimeStatus("disconnected");
    });

    return () => {
      void supabase.removeChannel(channel);
    };
  }, [view, activeChamber?.device_id, activeChamber?.node_id, activeChamber?.id, activeExperimentId, experimentChamberId]);

  function handleDeviceChange(deviceCode: string) {
    setSelectedDevice(deviceCode);
    setSelectedChamber("");
  }

  function handleHistoryExperimentSelect(experimentId: string) {
    setHistoryExperimentId(experimentId);
    setHistoryPanel("data");
    setExperimentResults(null);
    setResultsError("");
  }

  return <main className="shell">
    <header className="topbar"><div className="brand"><span className="brand-mark">R</span><span>RuSim <small>/ telemetry</small></span></div><div className="status"><span className="pulse" /> {view === "monitoring" ? connectionStatus : "Experiment history"}<span className="divider" /> {view === "monitoring" ? <>Updated {lastUpdated}<span className="divider" /> Realtime {humanizeRealtimeStatus(realtimeStatus)}</> : "Historical data"}</div></header>
    <section className="intro"><div><p className="eyebrow">{view === "monitoring" ? "Operations overview" : "Experiment history"}</p><h1>{view === "monitoring" ? "Device telemetry" : "Experiment history"}</h1><p className="subtitle">{view === "monitoring" ? "A clear view of your connected environment." : "Review telemetry from completed and active experiments."}</p></div><button className="refresh" onClick={() => view === "monitoring" ? void loadData() : setHistoryRefreshToken((value) => value + 1)}>Refresh data</button></section>
    <section className="controls section-navigation"><button className={`nav-tab${view === "monitoring" ? " active" : ""}`} onClick={() => setView("monitoring")} aria-pressed={view === "monitoring"}>Realtime Monitoring</button><button className={`nav-tab${view === "history" ? " active" : ""}`} onClick={() => setView("history")} aria-pressed={view === "history"}>Experiment History</button></section>
    {view === "monitoring" ? <>
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
    <section className="panel"><div className="panel-heading"><div><p className="eyebrow">Across time</p><h2>{parameterLabel(chartParameter)} readings</h2></div><div><select aria-label="Chart parameter" value={chartParameter} onChange={(event) => { if (isChartParameter(event.target.value)) setChartParameter(event.target.value); }}>{PARAMETER_NAMES.map((name) => <option key={name} value={name}>{name}</option>)}</select><span className="tag">{selectedRows.length} records</span></div></div><div className="chart">{chartPoints.length < 2 ? <div className="notice">Not enough data for chart.</div> : <><div className="gridlines"><i /><i /><i /><i /></div><svg viewBox="0 0 800 220" preserveAspectRatio="none" aria-label={`${parameterLabel(chartParameter)} telemetry chart`}><path d={chartPath(chartPoints)} /></svg><div className="axis">{timeLabels.map((label) => <span key={label}>{label}</span>)}</div></>}</div></section>
    <footer><span>Data source: Supabase</span><span>-</span><span>Selected chamber telemetry</span></footer>
    </> : <ExperimentHistoryView
      experiments={experiments}
      experimentsLoading={historyLoading}
      selectedExperiment={selectedHistoryExperiment}
      selectedExperimentId={historyExperimentId}
      onSelectExperiment={handleHistoryExperimentSelect}
      historyPanel={historyPanel}
      onHistoryPanelChange={setHistoryPanel}
      chambers={historyChambers}
      chambersLoading={historyLoading && Boolean(historyExperimentId)}
      selectedChamberId={historyChamberId}
      onSelectChamber={setHistoryChamberId}
      telemetryRows={historyRows}
      telemetryLoading={historyTelemetryLoading}
      error={historyError}
      results={experimentResults}
      resultsLoading={resultsLoading}
      resultsError={resultsError}
    />}
  </main>;
}

type ExperimentHistoryViewProps = {
  experiments: ExperimentHistoryItem[];
  experimentsLoading: boolean;
  selectedExperiment: ExperimentHistoryItem | undefined;
  selectedExperimentId: string;
  onSelectExperiment: (id: string) => void;
  historyPanel: HistoryPanel;
  onHistoryPanelChange: (panel: HistoryPanel) => void;
  chambers: ExperimentChamberSummary[];
  chambersLoading: boolean;
  selectedChamberId: string;
  onSelectChamber: (id: string) => void;
  telemetryRows: DashboardTelemetryRow[];
  telemetryLoading: boolean;
  error: string;
  results: ExperimentResults | null;
  resultsLoading: boolean;
  resultsError: string;
};

function ExperimentHistoryView({
  experiments,
  experimentsLoading,
  selectedExperiment,
  selectedExperimentId,
  onSelectExperiment,
  historyPanel,
  onHistoryPanelChange,
  chambers,
  chambersLoading,
  selectedChamberId,
  onSelectChamber,
  telemetryRows,
  telemetryLoading,
  error,
  results,
  resultsLoading,
  resultsError,
}: ExperimentHistoryViewProps) {
  const orderedRows = useMemo(
    () => [...telemetryRows].sort((left, right) => Date.parse(left.recorded_at) - Date.parse(right.recorded_at)),
    [telemetryRows],
  );
  const latest = orderedRows.length > 0 ? orderedRows[orderedRows.length - 1] : undefined;

  return <>
    {error && <div className="notice">Unable to load experiment history. <span>{error}</span></div>}
    <section className="panel">
      <div className="panel-heading"><div><p className="eyebrow">Experiment list</p><h2>Experiments</h2></div><span className="tag">{experiments.length} records</span></div>
      {experimentsLoading && <div className="notice">Loading experiments...</div>}
      {!experimentsLoading && experiments.length === 0 && <div className="notice">No experiments are available.</div>}
      <div className="history-list">{experiments.map((experiment) => <button className={`history-item${experiment.id === selectedExperimentId ? " selected" : ""}`} key={experiment.id} onClick={() => onSelectExperiment(experiment.id)}><strong>{experiment.experiment_code}</strong><span>{experiment.name}</span><small>{humanizeStatus(experiment.status)} | Started {formatDate(experiment.started_at)}{experiment.ended_at ? ` | Ended ${formatDate(experiment.ended_at)}` : ""}</small></button>)}</div>
    </section>
    {selectedExperimentId && !selectedExperiment && !experimentsLoading && <div className="notice">The selected experiment is unavailable.</div>}
    {selectedExperiment && <>
      <section className="controls history-navigation"><div className="control-copy"><span className="label">Selected experiment</span><strong>{selectedExperiment.experiment_code}</strong><span className="count">{humanizeStatus(selectedExperiment.status)}</span></div><div><button className="history-tab" onClick={() => onHistoryPanelChange("data")} aria-pressed={historyPanel === "data"}>Historical Data</button><button className="history-tab" onClick={() => onHistoryPanelChange("results")} aria-pressed={historyPanel === "results"}>Results</button></div></section>
      <section className="panel history-summary"><p className="eyebrow">{historyPanel === "data" ? "Historical data" : "Experiment results"}</p><h2>{selectedExperiment.name}</h2><p>{selectedExperiment.description ?? "No description available."}</p><span className="history-dates">Started {formatDate(selectedExperiment.started_at)}{selectedExperiment.ended_at ? ` | Ended ${formatDate(selectedExperiment.ended_at)}` : ""}</span></section>
      {historyPanel === "data" ? <>
        <section className="controls"><div className="control-copy"><span className="label">Chambers</span><strong>{chambers.length} assigned</strong></div><label>Chamber <select value={selectedChamberId} onChange={(event) => onSelectChamber(event.target.value)} disabled={chambers.length === 0}><option value="" disabled>Select chamber</option>{chambers.map((chamber) => <option key={chamber.id} value={chamber.id}>{chamber.chamber_name ?? chamber.chamber_code} ({chamber.node_code})</option>)}</select></label></section>
        {chambersLoading && <div className="notice">Loading experiment chambers...</div>}
        {!chambersLoading && chambers.length === 0 && <div className="notice">This experiment has no assigned chambers.</div>}
        {chambers.length > 0 && <>
          <section className="metrics">{PARAMETER_NAMES.map((name, index) => <MetricCard key={name} name={name} value={latest ? getMetricValue(latest, name) : null} index={index} />)}</section>
          {telemetryLoading && <div className="notice">Loading historical telemetry...</div>}
          {!telemetryLoading && telemetryRows.length === 0 && <div className="notice">No telemetry data is available for this experiment chamber.</div>}
          <div className="history-chart-grid">{PARAMETER_NAMES.map((parameter) => <TelemetryChart key={parameter} rows={orderedRows} parameter={parameter} recordCount={telemetryRows.length} />)}</div>
        </>}
      </> : <ExperimentResultsView experiment={selectedExperiment} results={results} loading={resultsLoading} error={resultsError} />}
    </>}
  </>;
}

function ExperimentResultsView({
  experiment,
  results,
  loading,
  error,
}: {
  experiment: ExperimentHistoryItem;
  results: ExperimentResults | null;
  loading: boolean;
  error: string;
}) {
  if (loading) return <div className="notice">Loading experiment results...</div>;
  if (error) return <div className="notice">Unable to load experiment results. <span>{error}</span></div>;
  if (!results) return <div className="notice">Experiment results are unavailable.</div>;

  return <>
    <section className="metrics results-overview">
      <article className="metric"><p>Experiment</p><strong>{experiment.experiment_code}</strong><span className="unit">{experiment.name}</span></article>
      <article className="metric"><p>Status</p><strong>{humanizeStatus(experiment.status)}</strong></article>
      <article className="metric"><p>Chambers</p><strong>{results.chambers.length}</strong></article>
      <article className="metric"><p>Telemetry records</p><strong>{results.total_telemetry_count}</strong></article>
      <article className="metric"><p>Sessions</p><strong>{results.sessions.length}</strong></article>
    </section>
    <section className="panel"><div className="panel-heading"><div><p className="eyebrow">Session summary</p><h2>Experiment sessions</h2></div><span className="tag">{results.sessions.length} sessions</span></div>{results.sessions.length === 0 ? <div className="notice">No sessions are recorded for this experiment.</div> : <div className="session-list">{results.sessions.map((session) => <div className="session-item" key={session.id}><strong>Session {session.session_number}</strong><span>{humanizeStatus(session.status)}</span><small>Started {formatDate(session.started_at)}{session.ended_at ? ` | Ended ${formatDate(session.ended_at)}` : ""}</small></div>)}</div>}</section>
    <section className="panel"><div className="panel-heading"><div><p className="eyebrow">Per-chamber results</p><h2>Telemetry summary</h2></div><span className="tag">{results.total_telemetry_count} total records</span></div>{results.chambers.length === 0 ? <div className="notice">No experiment chambers are assigned.</div> : <div className="result-list">{results.chambers.map((chamber) => <ChamberResultCard key={chamber.id} chamber={chamber} />)}</div>}</section>
  </>;
}

function ChamberResultCard({ chamber }: { chamber: ExperimentChamberResult }) {
  const metrics = [
    ["CH4", chamber.ch4],
    ["CO2", chamber.co2],
    ["pH", chamber.ph],
    ["Temperature", chamber.temperature],
    ["Pressure", chamber.pressure],
  ] as const;

  return <article className="result-card"><div className="result-heading"><div><strong>{chamber.chamber_name ?? chamber.chamber_code}</strong><span>{chamber.chamber_code} | {chamber.node_name} ({chamber.node_code})</span></div><span className="tag">{chamber.record_count} records</span></div><div className="result-dates"><span>First: {formatDate(chamber.first_recorded_at)}</span><span>Last: {formatDate(chamber.last_recorded_at)}</span></div><div className="result-metrics">{metrics.map(([name, summary]) => <div className="result-metric" key={name}><strong>{parameterLabel(name)}</strong>{summary ? <span>Min {summary.min.toFixed(2)} | Avg {summary.average.toFixed(2)} | Max {summary.max.toFixed(2)}</span> : <span>No telemetry</span>}</div>)}</div></article>;
}

function TelemetryChart({
  rows,
  parameter,
  recordCount,
}: {
  rows: DashboardTelemetryRow[];
  parameter: ChartParameter;
  recordCount: number;
}) {
  const points = useMemo(() => createChartPoints(rows, parameter), [rows, parameter]);
  const labels = useMemo(() => createTimeLabels(rows), [rows]);

  return <section className="panel telemetry-chart-panel"><div className="panel-heading"><div><p className="eyebrow">Historical telemetry</p><h2>{parameterLabel(parameter)} fluctuation</h2></div><span className="tag">{recordCount} records</span></div><div className="chart">{points.length < 2 ? <div className="notice">Not enough data for chart.</div> : <><div className="gridlines"><i /><i /><i /><i /></div><svg viewBox="0 0 800 220" preserveAspectRatio="none" aria-label={`${parameterLabel(parameter)} historical telemetry chart`}><path d={chartPath(points)} /></svg><div className="axis">{labels.map((label, index) => <span key={`${label}-${index}`}>{label}</span>)}</div></>}</div></section>;
}

function formatDate(value: string | null): string {
  return value ? new Date(value).toLocaleString([], { dateStyle: "medium", timeStyle: "short" }) : "Not available";
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

function parameterLabel(name: ChartParameter): string {
  const unit = PARAMETER_UNITS[name];
  return unit ? `${name} (${unit})` : name;
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

function MetricCard({ name, value, index }: { name: ChartParameter; value: number | null; index: number }) {
  const colors = ["blue", "orange", "green", "purple", "pink"];
  return <article className="metric"><div className={`metric-icon ${colors[index]}`}><span /></div><div><p>{name}</p><strong>{value === null ? "-" : value.toFixed(1)}</strong><span className="unit">{value === null ? "" : PARAMETER_UNITS[name]}</span></div><span className="trend">{value === null ? "-" : "Latest"}</span></article>;
}

function StatusCard({ label, value, detail }: { label: string; value: string; detail?: string }) {
  if (label === "Experiment") {
    return <article className="metric status-card experiment-status-card"><p className="status-label">{label}</p><strong className="experiment-code">{value}</strong>{detail && <span className="experiment-name">{detail}</span>}</article>;
  }
  return <article className="metric"><p>{label}</p><strong>{value}</strong>{detail && <span className="unit">{detail}</span>}</article>;
}

function humanizeStatus(status: string | undefined): string {
  if (!status) return "Unavailable";
  return status.charAt(0).toUpperCase() + status.slice(1);
}

function humanizeRealtimeStatus(status: RealtimeStatus): string {
  return status.charAt(0).toUpperCase() + status.slice(1);
}

function recomputeSystemActive(status: MonitoringStatus): MonitoringStatus {
  return {
    ...status,
    system_active: status.experiment?.status === "active"
      && status.device?.status === "online"
      && status.node?.status === "online"
      && status.chamber?.status === "running",
  };
}

export default App;
