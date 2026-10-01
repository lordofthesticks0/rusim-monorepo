// IMPORTANT: For AI agents, ALWAYS update the documentation in docs/llm-generated/dashboard.md whenever any changes are made.

import { useCallback, useEffect, useMemo, useState } from "react";
import { useAuth } from "./auth";
import { csvFileName, downloadCsv, toCsv } from "./csv";
import {
  fetchReadings,
  fetchExperimentReadings,
  getParameterValue,
  isConcluded,
  MAX_READING_ROWS,
  PARAMETER_NAMES,
  PARAMETER_UNITS,
  POLL_INTERVAL_MS,
  summarizeExperiments,
  summarizeParameter,
  splitBottlesByPresence,
  type ExperimentSummary,
  type ParameterName,
  type ParameterStats,
} from "./data";
import type { DeviceSession, Reading } from "./types/domain";
import { formatExperimentId, isTestingExperiment } from "./types/domain";

type DashboardView = "monitoring" | "history";
type HistoryPanel = "readings" | "results";

/** Decimal places per parameter, chosen to suit each sensor's useful precision. */
const PARAMETER_DIGITS: Record<ParameterName, number> = {
  pH: 2,
  Temp: 2,
  CO2: 1,
  CH4: 2,
  Pressure: 2,
};

function App() {
  const { session, signingIn, error: authError, signIn, signOut } = useAuth();

  if (!session) {
    return <LoginScreen signingIn={signingIn} error={authError} onSignIn={signIn} />;
  }

  return <Dashboard session={session} onSignOut={signOut} />;
}

function LoginScreen({
  signingIn,
  error,
  onSignIn,
}: {
  signingIn: boolean;
  error: string;
  onSignIn: (token: string) => Promise<boolean>;
}) {
  const [token, setToken] = useState("");

  return <main className="shell">
    <header className="topbar"><div className="brand"><span className="brand-mark">R</span><span>RuSim <small>/ telemetry</small></span></div></header>
    <section className="intro"><div><p className="eyebrow">Device access</p><h1>Sign in</h1><p className="subtitle">Enter your device token to view telemetry.</p></div></section>
    <form className="panel login-panel" onSubmit={(event) => { event.preventDefault(); void onSignIn(token); }}>
      <label className="login-field">Device token
        <input
          type="password"
          value={token}
          onChange={(event) => setToken(event.target.value)}
          placeholder="Enter your device token"
          autoComplete="current-password"
          autoFocus
          required
        />
      </label>
      {error && <div className="notice login-error">{error}</div>}
      <button className="refresh" type="submit" disabled={signingIn || token === ""}>{signingIn ? "Checking token..." : "Sign in"}</button>
      <p className="login-hint">Your device token is exchanged for a 12-hour session scoped to its own device. The device token itself is never stored; only the session token is kept until you sign out or it expires.</p>
    </form>
  </main>;
}

function Dashboard({
  session,
  onSignOut,
}: {
  session: DeviceSession;
  onSignOut: () => Promise<void>;
}) {
  const { device_id: deviceId, bottle_count: bottleCount, owner_name: ownerName, owner_email: ownerEmail } = session.device;
  const sessionToken = session.sessionToken;
  const [view, setView] = useState<DashboardView>("monitoring");
  const [readings, setReadings] = useState<Reading[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState("");
  const [selectedBottle, setSelectedBottle] = useState(0);
  const [chartParameter, setChartParameter] = useState<ParameterName>("CH4");
  // A failed fetch is treated as the end of the live feed, so polling stops
  // until the operator refreshes by hand.
  const [pollSuspended, setPollSuspended] = useState(false);

  const loadData = useCallback(async () => {
    setLoading(true);
    setError("");
    try {
      setReadings(await fetchReadings(sessionToken));
      setPollSuspended(false);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to load readings");
      setPollSuspended(true);
    } finally {
      setLoading(false);
    }
  }, [sessionToken]);

  useEffect(() => {
    void loadData();
  }, [loadData]);

  const experiments = useMemo(() => summarizeExperiments(readings), [readings]);
  const liveExperiment = experiments.find((experiment) => !experiment.concluded);

  // Which bottles reported at the newest sample, and which are missing from it.
  const presence = useMemo(
    () => splitBottlesByPresence(readings, bottleCount),
    [readings, bottleCount],
  );
  const selectedBottleIsDisconnected =
    presence.latestSample !== null && presence.disconnected.includes(selectedBottle);

  // Poll only while some experiment still looks live. The interval is far longer
  // than the staleness threshold, so each tick decides again from fresh data.
  useEffect(() => {
    if (view !== "monitoring" || pollSuspended || !liveExperiment) return;
    const timer = window.setInterval(() => void loadData(), POLL_INTERVAL_MS);
    return () => window.clearInterval(timer);
  }, [view, pollSuspended, liveExperiment?.experiment_id, loadData]);

  const bottleReadings = useMemo(
    () => readings
      .filter((reading) => reading.bottle_id === selectedBottle)
      .sort((left, right) => Date.parse(left.timestamp) - Date.parse(right.timestamp)),
    [readings, selectedBottle],
  );
  const latestReading = bottleReadings.at(-1);
  const latestReadingAt = latestReading
    ? new Date(latestReading.timestamp).toLocaleString([], { dateStyle: "medium", timeStyle: "short" })
    : "-";

  const connectionStatus = loading
    ? "Loading"
    : error
      ? "Unable to load"
      : readings.length > 0
        ? "Data loaded"
        : "No readings";

  return <main className="shell">
    <header className="topbar">
      <div className="brand"><span className="brand-mark">R</span><span>RuSim <small>/ telemetry</small></span></div>
      <div className="status">
        <span className="pulse" />
        {view === "monitoring" ? connectionStatus : "Experiment history"}
        <span className="divider" />
        {view === "monitoring"
          ? <>Updated {latestReadingAt}<span className="divider" />{liveExperiment ? "Auto-refreshing" : "No live experiment"}</>
          : "Historical data"}
        <span className="divider" />
        <button className="link-button" onClick={() => void onSignOut()}>Sign out</button>
      </div>
    </header>

    <section className="intro">
      <div>
        <p className="eyebrow">{view === "monitoring" ? "Operations overview" : "Experiment history"}</p>
        <h1>{view === "monitoring" ? "Device telemetry" : "Experiment history"}</h1>
        <p className="subtitle">{view === "monitoring" ? "Latest readings for the selected bottle." : "Review readings from testing and regular experiments."}</p>
      </div>
      <button className="refresh" onClick={() => void loadData()}>Refresh data</button>
    </section>

    <section className="controls section-navigation">
      <button className={`nav-tab${view === "monitoring" ? " active" : ""}`} onClick={() => setView("monitoring")} aria-pressed={view === "monitoring"}>Monitoring</button>
      <button className={`nav-tab${view === "history" ? " active" : ""}`} onClick={() => setView("history")} aria-pressed={view === "history"}>Experiment History</button>
    </section>

    {view === "monitoring" ? <>
      {error && <div className="notice">Unable to load readings. <span>{error}</span></div>}
      {!loading && !error && readings.length === 0 && <div className="notice">No readings are available for this device.</div>}

      <section className="controls">
        <div className="control-copy"><span className="label">Showing</span><strong>Bottle {selectedBottle}</strong><span className="count">{bottleCount} bottles</span></div>
        <label>Bottle
          <select value={selectedBottle} onChange={(event) => setSelectedBottle(Number(event.target.value))}>
            {Array.from({ length: bottleCount }, (_, bottleId) => <option key={bottleId} value={bottleId}>Bottle {bottleId}</option>)}
          </select>
        </label>
      </section>

      <div className="notice">Device {deviceId} <span>| {ownerName ?? "Unnamed owner"}{ownerEmail ? ` (${ownerEmail})` : ""}</span></div>

      <section className="metrics">
        <StatusCard label="Bottle" value={`Bottle ${selectedBottle}`} />
        <StatusCard label="Samples" value={String(bottleReadings.length)} />
        <StatusCard label="Latest sample" value={latestReadingAt} />
        <StatusCard
          label="Connection"
          value={selectedBottleIsDisconnected ? "Disconnected" : latestReading ? (isConcluded(latestReading.timestamp) ? "Concluded" : "Live") : "No data"}
        />
      </section>

      {presence.latestSample && presence.disconnected.length > 0 && <div className="notice">
        Disconnected at {formatDate(presence.latestSample)}: bottles {presence.disconnected.map((bottleId) => `Bottle ${bottleId}`).join(", ")} sent no data.
      </div>}

      <section className="metrics">
        {PARAMETER_NAMES.map((name, index) => <MetricCard key={name} name={name} value={latestReading ? getParameterValue(latestReading, name) : null} index={index} />)}
      </section>

      {!loading && !error && bottleReadings.length === 0 && <div className="notice">
        No readings are available for this bottle{presence.latestSample ? "; it did not report at the latest sample" : ""}.
      </div>}

      <section className="panel">
        <div className="panel-heading">
          <div><p className="eyebrow">Across experiments</p><h2>{parameterLabel(chartParameter)} readings</h2></div>
          <div>
            <select aria-label="Chart parameter" value={chartParameter} onChange={(event) => { if (isChartParameter(event.target.value)) setChartParameter(event.target.value); }}>
              {PARAMETER_NAMES.map((name) => <option key={name} value={name}>{name}</option>)}
            </select>
            <span className="tag">{bottleReadings.length} records</span>
          </div>
        </div>
        <div className="chart">
          {bottleReadings.length < 2
            ? <div className="notice">Not enough data for chart.</div>
            : <Chart rows={bottleReadings} parameter={chartParameter} />}
        </div>
      </section>

      <footer><span>Data source: Supabase</span><span>-</span><span>Bottle readings across experiments</span></footer>
    </> : <ExperimentHistoryView experiments={experiments} loading={loading} error={error} sessionToken={sessionToken} />}
  </main>;
}

function ExperimentHistoryView({
  experiments,
  loading,
  error,
  sessionToken,
}: {
  experiments: ExperimentSummary[];
  loading: boolean;
  error: string;
  sessionToken: string;
}) {
  const [selectedExperimentId, setSelectedExperimentId] = useState<number | null>(null);
  const [historyPanel, setHistoryPanel] = useState<HistoryPanel>("readings");
  const [readings, setReadings] = useState<Reading[]>([]);
  const [detailLoading, setDetailLoading] = useState(false);
  const [detailError, setDetailError] = useState("");

  // Default to the newest experiment whenever the list changes underneath us.
  const activeExperimentId = selectedExperimentId ?? experiments[0]?.experiment_id ?? null;

  useEffect(() => {
    if (activeExperimentId === null) {
      setReadings([]);
      return;
    }

    let cancelled = false;
    setDetailLoading(true);
    setDetailError("");
    void fetchExperimentReadings(sessionToken, activeExperimentId)
      .then((result) => {
        if (!cancelled) setReadings(result);
      })
      .catch((err: unknown) => {
        if (cancelled) return;
        setReadings([]);
        setDetailError(err instanceof Error ? err.message : "Unable to load experiment readings");
      })
      .finally(() => {
        if (!cancelled) setDetailLoading(false);
      });

    return () => {
      cancelled = true;
    };
  }, [activeExperimentId, sessionToken]);

  const selectedExperiment = experiments.find((experiment) => experiment.experiment_id === activeExperimentId);
  const bottleStats = useMemo(() => computeBottleStats(readings), [readings]);

  return <>
    {error && <div className="notice">Unable to load experiment history. <span>{error}</span></div>}
    <section className="panel">
      <div className="panel-heading"><div><p className="eyebrow">Experiment list</p><h2>Experiments</h2></div><span className="tag">{experiments.length} records</span></div>
      {loading && <div className="notice">Loading experiments...</div>}
      {!loading && experiments.length === 0 && <div className="notice">No experiments have readings yet.</div>}
      <div className="history-list">
        {experiments.map((experiment) => <button
          className={`history-item${experiment.experiment_id === activeExperimentId ? " selected" : ""}`}
          key={experiment.experiment_id}
          onClick={() => setSelectedExperimentId(experiment.experiment_id)}
        >
          <strong>{formatExperimentId(experiment.experiment_id)}</strong>
          <span>{experiment.reading_count} bottle readings{isTestingExperiment(experiment.experiment_id) ? " | Testing experiment" : ""}</span>
          <small>{experiment.concluded ? "Concluded" : "Live"} | Last sample {formatDate(experiment.last_timestamp)}</small>
        </button>)}
      </div>
    </section>

    {selectedExperiment && <>
      <section className="controls history-navigation">
        <div className="control-copy"><span className="label">Selected experiment</span><strong>{formatExperimentId(selectedExperiment.experiment_id)}</strong><span className="count">{selectedExperiment.concluded ? "Concluded" : "Live"}</span></div>
        <div className="history-actions">
          <button
            className="history-tab"
            onClick={() => downloadExperimentCsv(selectedExperiment.experiment_id, readings)}
            disabled={detailLoading || detailError !== "" || readings.length === 0}
            title={csvDisabledReason(readings.length, detailLoading, detailError)}
          >
            Download CSV
          </button>
          <button className="history-tab" onClick={() => setHistoryPanel("readings")} aria-pressed={historyPanel === "readings"}>Bottle Readings</button>
          <button className="history-tab" onClick={() => setHistoryPanel("results")} aria-pressed={historyPanel === "results"}>Results</button>
        </div>
      </section>

      <section className="panel history-summary">
        <p className="eyebrow">{historyPanel === "readings" ? "Bottle readings" : "Experiment results"}</p>
        <h2>{formatExperimentId(selectedExperiment.experiment_id)}</h2>
        <p>{selectedExperiment.reading_count} bottle readings, sampled between {formatDate(selectedExperiment.first_timestamp)} and {formatDate(selectedExperiment.last_timestamp)}.</p>
        <span className="history-dates">{selectedExperiment.concluded ? "No recent samples, so this experiment is treated as concluded." : "Recent samples are present, so this experiment is treated as live."}</span>
      </section>

      {detailError && <div className="notice">Unable to load experiment readings. <span>{detailError}</span></div>}
      {detailLoading && !detailError && <div className="notice">Loading experiment readings...</div>}
      {!detailLoading && !detailError && readings.length === 0 && <div className="notice">No readings are available for this experiment.</div>}
      {isTruncated(readings.length) && <div className="notice">
        Only the most recent {MAX_READING_ROWS.toLocaleString()} readings were loaded, so this experiment is incomplete on screen and any CSV export is truncated too.
      </div>}

      {readings.length > 0 && historyPanel === "readings" && <>
        <section className="metrics">
          {PARAMETER_NAMES.map((name, index) => <MetricCard key={name} name={name} value={getParameterValue(readings[0]!, name)} index={index} />)}
        </section>
        <div className="history-chart-grid">
          {PARAMETER_NAMES.map((parameter) => <BottleChart key={parameter} readings={readings} parameter={parameter} />)}
        </div>
      </>}

      {readings.length > 0 && historyPanel === "results" && <ExperimentResultsView bottleStats={bottleStats} experiment={selectedExperiment} />}
    </>}
  </>;
}

function ExperimentResultsView({
  bottleStats,
  experiment,
}: {
  bottleStats: Array<{ bottle_id: number; reading: Reading }>;
  experiment: ExperimentSummary;
}) {
  const parameters = PARAMETER_NAMES.map((name) => ({
    name,
    stats: summarizeParameter(bottleStats.map((entry) => getParameterValue(entry.reading, name))),
  }));

  return <>
    <section className="metrics results-overview">
      <article className="metric"><p>Experiment</p><strong>{formatExperimentId(experiment.experiment_id)}</strong><span className="unit">{isTestingExperiment(experiment.experiment_id) ? "Testing experiment" : "Regular experiment"}</span></article>
      <article className="metric"><p>Status</p><strong>{experiment.concluded ? "Concluded" : "Live"}</strong></article>
      <article className="metric"><p>Bottles</p><strong>{bottleStats.length}</strong></article>
      <article className="metric"><p>Readings</p><strong>{experiment.reading_count}</strong></article>
    </section>

    <section className="panel">
      <div className="panel-heading"><div><p className="eyebrow">Across bottles</p><h2>Parameter summary</h2></div><span className="tag">{bottleStats.length} bottles</span></div>
      <div className="result-metrics">
        {parameters.map(({ name, stats }) => <div className="result-metric" key={name}><strong>{parameterLabel(name)}</strong><span>{stats ? <>Min {formatValue(stats.min, name)} | Avg {formatValue(stats.average, name)} | Max {formatValue(stats.max, name)}</> : "No readings"}</span></div>)}
      </div>
    </section>

    <section className="panel">
      <div className="panel-heading"><div><p className="eyebrow">Per bottle</p><h2>Bottle results</h2></div></div>
      <div className="result-list">
        {bottleStats.map(({ bottle_id, reading }) => <article className="result-card" key={bottle_id}>
          <div className="result-heading">
            <div><strong>Bottle {bottle_id}</strong><span>{formatDate(reading.timestamp)}</span></div>
            <span className="tag">Sample</span>
          </div>
          <div className="result-metrics">
            {PARAMETER_NAMES.map((name) => <div className="result-metric" key={name}><strong>{parameterLabel(name)}</strong><span>{formatValue(getParameterValue(reading, name), name)}</span></div>)}
          </div>
        </article>)}
      </div>
    </section>
  </>;
}

/**
 * Saves the selected experiment's loaded readings as a CSV file.
 *
 * This exports what is already on screen rather than re-querying, so the file and
 * the charts always describe the same rows. A truncated read therefore produces a
 * truncated file, which is why the limit is called out on screen next to the
 * button. Nothing happens for an experiment with no loaded rows.
 */
function downloadExperimentCsv(experimentId: number, readings: Reading[]): void {
  const csv = toCsv(readings);
  if (csv === "") return;
  downloadCsv(csv, csvFileName(experimentId));
}

/** Explains a disabled download button, so the reason is not left to guesswork. */
function csvDisabledReason(rowCount: number, loading: boolean, error: string): string {
  if (loading) return "Readings are still loading";
  if (error !== "") return "Readings failed to load, so there is nothing to export";
  if (rowCount === 0) return "This experiment has no readings to export";
  return "Download this experiment's readings as CSV";
}

/** Whether a read hit the row limit and therefore holds only part of the data. */
function isTruncated(rowCount: number): boolean {
  return rowCount >= MAX_READING_ROWS;
}

/** Pairs each bottle with its reading, so charts and tables can align by bottle. */
function computeBottleStats(readings: Reading[]): Array<{ bottle_id: number; reading: Reading }> {
  return [...readings]
    .sort((left, right) => left.bottle_id - right.bottle_id)
    .map((reading) => ({ bottle_id: reading.bottle_id, reading }));
}

function Chart({ rows, parameter }: { rows: Reading[]; parameter: ParameterName }) {
  const points = useMemo(() => createChartPoints(rows, parameter), [rows, parameter]);
  const labels = useMemo(() => createTimeLabels(rows), [rows]);

  return <>
    <div className="gridlines"><i /><i /><i /><i /></div>
    <svg viewBox="0 0 800 220" preserveAspectRatio="none" aria-label={`${parameterLabel(parameter)} reading chart`}><path d={chartPath(points)} /></svg>
    <div className="axis">{labels.map((label, index) => <span key={`${label}-${index}`}>{label}</span>)}</div>
  </>;
}

function BottleChart({ readings, parameter }: { readings: Reading[]; parameter: ParameterName }) {
  return <section className="panel telemetry-chart-panel">
    <div className="panel-heading"><div><p className="eyebrow">Across bottles</p><h2>{parameterLabel(parameter)}</h2></div><span className="tag">{readings.length} bottles</span></div>
    <div className="chart"><Chart rows={readings} parameter={parameter} /></div>
  </section>;
}

/** Renders a measurement, or a dash when the sensor reported nothing. */
function formatValue(value: number | null, parameter: ParameterName): string {
  return value === null ? "-" : value.toFixed(PARAMETER_DIGITS[parameter]);
}

function formatDate(value: string | null): string {
  return value ? new Date(value).toLocaleString([], { dateStyle: "medium", timeStyle: "short" }) : "Not available";
}

function getMetricValue(reading: Reading, name: ParameterName): number | null {
  return getParameterValue(reading, name);
}

function parameterLabel(name: ParameterName): string {
  const unit = PARAMETER_UNITS[name];
  return unit ? `${name} (${unit})` : name;
}

function isChartParameter(value: string): value is ParameterName {
  return PARAMETER_NAMES.some((name) => name === value);
}

/**
 * Scales samples into chart coordinates.
 *
 * A null measurement becomes a null y, which chartPath renders as a break in
 * the line rather than a drop to zero. The scale is computed only from samples
 * that actually reported, so one missing value cannot distort the axis.
 */
function createChartPoints(rows: Reading[], parameter: ParameterName): Array<{ x: number; y: number | null }> {
  if (rows.length < 2) return [];
  const values = rows.map((row) => getMetricValue(row, parameter));
  const reported = values.filter((value): value is number => value !== null);
  if (reported.length === 0) return values.map((_, index) => ({ x: (index / (values.length - 1)) * 800, y: null }));

  const min = Math.min(...reported);
  const max = Math.max(...reported);
  const range = max - min;
  return values.map((value, index) => ({
    x: (index / (values.length - 1)) * 800,
    y: value === null ? null : range === 0 ? 110 : 190 - ((value - min) / range) * 160,
  }));
}

/**
 * Builds an SVG path, lifting the pen between null samples so the line is drawn
 * as separate segments instead of joining across a gap.
 */
function chartPath(points: Array<{ x: number; y: number | null }>): string {
  let penDown = false;
  return points
    .map((point) => {
      if (point.y === null) {
        penDown = false;
        return "";
      }
      const command = penDown ? "L" : "M";
      penDown = true;
      return `${command}${point.x} ${point.y}`;
    })
    .join(" ");
}

function createTimeLabels(rows: Reading[]): string[] {
  if (rows.length === 0) return [];
  const count = Math.min(5, rows.length);
  const indexes = Array.from({ length: count }, (_, index) => Math.round((index * (rows.length - 1)) / (count - 1 || 1)));
  return indexes.flatMap((index) => {
    const row = rows[index];
    return row === undefined ? [] : [new Date(row.timestamp).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })];
  });
}

function MetricCard({ name, value, index }: { name: ParameterName; value: number | null; index: number }) {
  const colors = ["blue", "orange", "green", "purple", "pink"];
  return <article className="metric">
    <div className={`metric-icon ${colors[index]}`}><span /></div>
    <div><p>{name}</p><strong>{value === null ? "-" : formatValue(value, name)}</strong><span className="unit">{value === null ? "" : PARAMETER_UNITS[name]}</span></div>
    <span className="trend">{value === null ? "-" : "Latest"}</span>
  </article>;
}

function StatusCard({ label, value }: { label: string; value: string }) {
  return <article className="metric"><p>{label}</p><strong>{value}</strong></article>;
}

export default App;
