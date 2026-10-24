// IMPORTANT: For AI agents, ALWAYS update the documentation in docs/llm-generated/dashboard.md whenever any changes are made.

import { useCallback, useEffect, useMemo, useState } from "react";
import { useAuth } from "./auth";
import { csvFileName, downloadCsv, toCsv } from "./csv";
import { displayChamberNumber, internalChamberIds, FIRST_INTERNAL_CHAMBER_ID } from "./chambers";
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
  splitBottlesByPresence,
  type ExperimentSummary,
  type ParameterName,
  type ParameterStats,
} from "./data";
import {
  isMonitoringPollingEligible,
  selectActiveExperiment,
  selectMonitoringReadings,
} from "./monitoring";
import {
  getHistoryPointDetails,
  getExperimentChambers,
  groupReadingsByChamber,
  selectChamberReadings,
  toggleExperimentSelection,
  type ChamberSummary,
} from "./history";
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

  return <main className="login-shell">
    <header className="login-header">
      <div className="login-brand">
        <span className="login-brand-mark" aria-hidden="true">R</span>
        <span>RuSim <small>/ telemetry</small></span>
      </div>
    </header>

    <div className="login-main">
      <section className="login-card" aria-labelledby="login-title">
        <div className="login-heading">
          <p className="eyebrow">Device access</p>
          <h1 className="login-title" id="login-title">Sign in</h1>
          <p className="login-subtitle">Enter your device token to view telemetry.</p>
        </div>

        <form className="login-form" onSubmit={(event) => { event.preventDefault(); void onSignIn(token); }}>
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
          {error && <div className="login-error" role="alert">{error}</div>}
          <button className="login-submit" type="submit" disabled={signingIn || token === ""}>{signingIn ? "Checking token..." : "Sign in"}</button>
        </form>

        <p className="login-hint">Your device token is exchanged for a 12-hour session scoped to its own device. The device token itself is never stored; only the session token is kept until you sign out or it expires.</p>
      </section>
    </div>
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
  const [selectedBottle, setSelectedBottle] = useState(FIRST_INTERNAL_CHAMBER_ID);
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

  // Freshness depends on the current clock, so recalculate on renders caused by
  // refresh and error state changes instead of memoizing only by the rows array.
  const experiments = summarizeExperiments(readings);
  const liveExperiment = selectActiveExperiment(experiments);
  const monitoringReadings = useMemo(
    () => selectMonitoringReadings(readings, liveExperiment?.experiment_id ?? null),
    [readings, liveExperiment?.experiment_id],
  );

  // Which bottles reported at the newest sample, and which are missing from it.
  const presence = useMemo(
    () => splitBottlesByPresence(monitoringReadings, bottleCount),
    [monitoringReadings, bottleCount],
  );
  const selectedBottleIsDisconnected =
    presence.latestSample !== null && presence.disconnected.includes(selectedBottle);

  // Poll while Monitoring is open, including when no experiment is active, so
  // a newly started experiment can be discovered without a page reload. The
  // interval is far longer than the staleness threshold, so each tick decides
  // again from fresh data.
  useEffect(() => {
    if (!isMonitoringPollingEligible(view, pollSuspended)) return;
    const timer = window.setInterval(() => void loadData(), POLL_INTERVAL_MS);
    return () => window.clearInterval(timer);
  }, [view, pollSuspended, liveExperiment?.experiment_id, loadData]);

  const bottleReadings = useMemo(
    () => monitoringReadings
      .filter((reading) => reading.bottle_id === selectedBottle)
      .sort((left, right) => Date.parse(left.timestamp) - Date.parse(right.timestamp)),
    [monitoringReadings, selectedBottle],
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
  const pulseTone = loading ? "pending" : error ? "error" : readings.length > 0 ? "ok" : "idle";
  const bottleState = selectedBottleIsDisconnected
    ? "Disconnected"
    : latestReading
      ? (isConcluded(latestReading.timestamp) ? "Concluded" : "Live")
      : "No data";
  const bottleStateTone: "live" | "warning" | "neutral" = selectedBottleIsDisconnected
    ? "warning"
    : latestReading && !isConcluded(latestReading.timestamp)
      ? "live"
      : "neutral";

  return <main className="shell">
    <header className="topbar">
      <div className="brand"><span className="brand-mark">R</span><span>RuSim <small>/ telemetry</small></span></div>
      <div className="status">
        <span className={`pulse${view === "monitoring" ? ` ${pulseTone}` : ""}`} />
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
        <p className="subtitle">{view === "monitoring" ? "Latest readings for the selected chamber." : "Review readings from testing and regular experiments."}</p>
      </div>
      <button className="refresh" onClick={() => void loadData()}>Refresh data</button>
    </section>

    <section className="controls section-navigation">
      <button className={`nav-tab${view === "monitoring" ? " active" : ""}`} onClick={() => setView("monitoring")} aria-pressed={view === "monitoring"}>Monitoring</button>
      <button className={`nav-tab${view === "history" ? " active" : ""}`} onClick={() => setView("history")} aria-pressed={view === "history"}>Experiment History</button>
    </section>

    {view === "monitoring" ? <div className="monitoring-view">
      {error && <div className="notice notice-error">Unable to refresh readings; showing the last loaded data. <span>{error}</span></div>}
      {loading && <div className="notice notice-info">Loading readings...</div>}
      {!loading && !liveExperiment ? <section className="panel monitoring-empty-state">
        <p className="eyebrow">Monitoring</p>
        <h2>Tidak ada eksperimen aktif</h2>
        <p className="subtitle">Historical readings remain available in Experiment History.</p>
      </section> : <>

      <section className="controls monitoring-controls">
        <div className="control-copy"><span className="label">Showing</span><strong>Chamber {displayChamberNumber(selectedBottle)}</strong><span className="count">{bottleCount} chambers</span></div>
        <label>Chamber
          <select value={selectedBottle} onChange={(event) => setSelectedBottle(Number(event.target.value))}>
            {internalChamberIds(bottleCount).map((internalId) => <option key={internalId} value={internalId}>Chamber {displayChamberNumber(internalId)}</option>)}
          </select>
        </label>
        <div className="monitoring-device">
          <span className="device-id">Device {deviceId}</span>
          <span className="device-owner">{ownerName ?? "Unnamed owner"}{ownerEmail ? ` (${ownerEmail})` : ""}</span>
        </div>
      </section>

      <section className="status-summary" aria-label="Chamber status">
        <StatusCard label="Chamber" value={`Chamber ${displayChamberNumber(selectedBottle)}`} />
        <StatusCard label="Samples" value={String(bottleReadings.length)} />
        <StatusCard label="Latest sample" value={latestReadingAt} />
        <StatusCard label="Connection" value={bottleState} tone={bottleStateTone} />
      </section>

      {presence.latestSample && presence.disconnected.length > 0 && <div className="notice connection-warning">
        <span className="notice-label">Missing chamber data</span>
        <p>Disconnected at {formatDate(presence.latestSample)}: chambers {presence.disconnected.map((internalId) => `Chamber ${displayChamberNumber(internalId)}`).join(", ")} sent no data.</p>
      </div>}

      <section className="readings-grid" aria-label="Latest parameter readings">
        {PARAMETER_NAMES.map((name, index) => <MetricCard key={name} name={name} value={latestReading ? getParameterValue(latestReading, name) : null} index={index} compact />)}
      </section>

      {!loading && !error && bottleReadings.length === 0 && <div className="notice notice-info">
        No readings are available for Chamber {displayChamberNumber(selectedBottle)}{presence.latestSample ? "; it did not report at the latest sample" : ""}.
      </div>}

      <section className="panel monitoring-chart">
        <div className="panel-heading">
          <div><p className="eyebrow">Across experiments</p><h2>{parameterLabel(chartParameter)} readings</h2></div>
          <div className="chart-controls">
            <select aria-label="Chart parameter" value={chartParameter} onChange={(event) => { if (isChartParameter(event.target.value)) setChartParameter(event.target.value); }}>
              {PARAMETER_NAMES.map((name) => <option key={name} value={name}>{name}</option>)}
            </select>
            <span className="tag">{bottleReadings.length} records</span>
          </div>
        </div>
        <div className="chart">
          {bottleReadings.length < 2
            ? <div className="chart-empty">Not enough data for chart.</div>
            : <Chart rows={bottleReadings} parameter={chartParameter} />}
        </div>
      </section>

      <footer><span>Data source: Supabase</span><span>-</span><span>Current experiment chamber readings</span></footer>
      </>}
    </div> : <ExperimentHistoryView experiments={experiments} loading={loading} error={error} sessionToken={sessionToken} />}
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

  const activeExperimentId = selectedExperimentId;

  useEffect(() => {
    if (activeExperimentId === null) {
      setReadings([]);
      return;
    }

    let cancelled = false;
    setReadings([]);
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
  const experimentReadings = useMemo(
    () => activeExperimentId === null
      ? []
      : readings.filter((reading) => reading.experiment_id === activeExperimentId),
    [readings, activeExperimentId],
  );
  const availableChambers = useMemo(
    () => getExperimentChambers(experimentReadings, activeExperimentId),
    [experimentReadings, activeExperimentId],
  );
  const [selectedChamberId, setSelectedChamberId] = useState<number | null>(null);
  useEffect(() => {
    setSelectedChamberId((current) => (
      current !== null && availableChambers.includes(current)
        ? current
        : availableChambers[0] ?? null
    ));
  }, [availableChambers]);
  const selectedChamberReadings = useMemo(
    () => selectChamberReadings(experimentReadings, activeExperimentId, selectedChamberId),
    [experimentReadings, activeExperimentId, selectedChamberId],
  );
  const chamberSummaries = useMemo(
    () => groupReadingsByChamber(experimentReadings, activeExperimentId),
    [experimentReadings, activeExperimentId],
  );

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
          onClick={() => setSelectedExperimentId((current) => toggleExperimentSelection(current, experiment.experiment_id))}
        >
          <strong>{formatExperimentId(experiment.experiment_id)}</strong>
          <span>{experiment.reading_count} chamber readings{isTestingExperiment(experiment.experiment_id) ? " | Testing experiment" : ""}</span>
          <small>{experiment.concluded ? "Concluded" : "Live"} | Last sample {formatDate(experiment.last_timestamp)}</small>
        </button>)}
      </div>
    </section>

    {selectedExperiment === undefined && <div className="notice">Select an experiment to view its chamber readings and results.</div>}

    {selectedExperiment && <>
      <section className="controls history-navigation">
        <div className="control-copy"><span className="label">Selected experiment</span><strong>{formatExperimentId(selectedExperiment.experiment_id)}</strong><span className="count">{selectedExperiment.concluded ? "Concluded" : "Live"}</span></div>
        <div className="history-actions">
          <button
            className="history-tab"
            onClick={() => downloadExperimentCsv(selectedExperiment.experiment_id, experimentReadings)}
            disabled={detailLoading || detailError !== "" || experimentReadings.length === 0}
            title={csvDisabledReason(experimentReadings.length, detailLoading, detailError)}
          >
            Download CSV
          </button>
          <button className="history-tab" onClick={() => setHistoryPanel("readings")} aria-pressed={historyPanel === "readings"}>Chamber Readings</button>
          <button className="history-tab" onClick={() => setHistoryPanel("results")} aria-pressed={historyPanel === "results"}>Results</button>
        </div>
      </section>

      <section className="panel history-summary">
        <p className="eyebrow">{historyPanel === "readings" ? "Chamber readings" : "Experiment results"}</p>
        <h2>{formatExperimentId(selectedExperiment.experiment_id)}</h2>
        <p>{selectedExperiment.reading_count} chamber readings, sampled between {formatDate(selectedExperiment.first_timestamp)} and {formatDate(selectedExperiment.last_timestamp)}.</p>
        <span className="history-dates">{selectedExperiment.concluded ? "No recent samples, so this experiment is treated as concluded." : "Recent samples are present, so this experiment is treated as live."}</span>
      </section>

      {detailError && <div className="notice">Unable to load experiment readings. <span>{detailError}</span></div>}
      {detailLoading && !detailError && <div className="notice">Loading experiment readings...</div>}
      {!detailLoading && !detailError && experimentReadings.length === 0 && <div className="notice">No readings are available for this experiment.</div>}
      {isTruncated(experimentReadings.length) && <div className="notice">
        Only the most recent {MAX_READING_ROWS.toLocaleString()} readings were loaded, so this experiment is incomplete on screen and any CSV export is truncated too.
      </div>}

      {experimentReadings.length > 0 && historyPanel === "readings" && <>
        <section className="controls history-navigation">
          <label className="control-copy" htmlFor="history-chamber-select">
            <span className="label">Chamber</span>
            <strong>Select a chamber to inspect its readings</strong>
          </label>
          <select
            id="history-chamber-select"
            value={selectedChamberId ?? ""}
            onChange={(event) => setSelectedChamberId(Number(event.target.value))}
          >
            {availableChambers.map((internalId) => <option value={internalId} key={internalId}>Chamber {displayChamberNumber(internalId)}</option>)}
          </select>
        </section>
        <section className="metrics">
          {PARAMETER_NAMES.map((name, index) => <MetricCard key={name} name={name} value={selectedChamberReadings.at(-1) ? getParameterValue(selectedChamberReadings.at(-1)!, name) : null} index={index} />)}
        </section>
        <div className="history-chart-grid">
          {PARAMETER_NAMES.map((parameter) => <BottleChart key={parameter} readings={selectedChamberReadings} parameter={parameter} bottleId={selectedChamberId} />)}
        </div>
      </>}

      {experimentReadings.length > 0 && historyPanel === "results" && <ExperimentResultsView chamberSummaries={chamberSummaries} experiment={selectedExperiment} />}
    </>}
  </>;
}

function ExperimentResultsView({
  chamberSummaries,
  experiment,
}: {
  chamberSummaries: ChamberSummary[];
  experiment: ExperimentSummary;
}) {
  return <>
    <section className="metrics results-overview">
      <article className="metric"><p>Experiment</p><strong>{formatExperimentId(experiment.experiment_id)}</strong><span className="unit">{isTestingExperiment(experiment.experiment_id) ? "Testing experiment" : "Regular experiment"}</span></article>
      <article className="metric"><p>Status</p><strong>{experiment.concluded ? "Concluded" : "Live"}</strong></article>
      <article className="metric"><p>Chambers</p><strong>{chamberSummaries.length}</strong></article>
      <article className="metric"><p>Readings</p><strong>{experiment.reading_count}</strong></article>
    </section>

    <section className="panel">
      <div className="panel-heading"><div><p className="eyebrow">Per chamber</p><h2>Chamber results</h2></div></div>
      <div className="result-list">
        {chamberSummaries.map(({ bottle_id, readings, lastReading, stats }) => <article className="result-card" key={bottle_id}>
          <div className="result-heading">
            <div><strong>Chamber {displayChamberNumber(bottle_id)}</strong><span>Latest sample: {formatDate(lastReading.timestamp)}</span></div>
            <span className="tag">{readings.length} readings</span>
          </div>
          <div className="result-metrics">
            {PARAMETER_NAMES.map((name) => {
              const parameterStats = stats[name];
              return <div className="result-metric" key={name}>
                <strong>{parameterLabel(name)}</strong>
                {parameterStats
                  ? <div className="result-stat-values">
                    <div><span>Min</span><strong>{formatValue(parameterStats.min, name)}</strong></div>
                    <div><span>Mean</span><strong>{formatValue(parameterStats.average, name)}</strong></div>
                    <div><span>Max</span><strong>{formatValue(parameterStats.max, name)}</strong></div>
                  </div>
                  : <span className="result-empty">No readings</span>}
              </div>;
            })}
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

type ChartInteraction = { index: number; pinned: boolean };

function Chart({ rows, parameter, bottleId, interactive = false }: { rows: Reading[]; parameter: ParameterName; bottleId?: number | null; interactive?: boolean }) {
  const points = useMemo(() => createChartPoints(rows, parameter), [rows, parameter]);
  const labels = useMemo(() => createTimeLabels(rows), [rows]);
  const [activePoint, setActivePoint] = useState<ChartInteraction | null>(null);

  useEffect(() => {
    setActivePoint(null);
  }, [rows, parameter, bottleId]);

  const activeReading = activePoint === null ? undefined : rows[activePoint.index];
  const activeDetails = activeReading === undefined
    ? undefined
    : getHistoryPointDetails(activeReading, parameter);

  return <>
    <div className="gridlines"><i /><i /><i /><i /></div>
    <svg viewBox="0 0 800 220" preserveAspectRatio="none" aria-label={`${parameterLabel(parameter)} reading chart`}>
      <path d={chartPath(points)} />
      {interactive && points.map((point, index) => {
        const reading = rows[index];
        if (reading === undefined) return null;
        const selected = activePoint?.index === index;
        return <circle
          key={`${reading.timestamp}-${index}`}
          cx={point.x}
          cy={point.y ?? 110}
          r={selected ? 6 : 4}
          fill={point.y === null ? "#a0a9b3" : "#2462ee"}
          stroke="white"
          strokeWidth="2"
          tabIndex={0}
          role="button"
          aria-label={`${parameterLabel(parameter)} at ${formatDateTime(reading.timestamp)}`}
          onMouseEnter={() => setActivePoint({ index, pinned: false })}
          onMouseLeave={() => setActivePoint((current) => current?.pinned ? current : null)}
          onFocus={() => setActivePoint({ index, pinned: false })}
          onBlur={() => setActivePoint((current) => current?.pinned ? current : null)}
          onClick={() => setActivePoint({ index, pinned: true })}
        />;
      })}
    </svg>
    <div className="axis" aria-label="Sample time">
      {labels.map((label, index) => <span key={`${label}-${index}`}>{label}</span>)}
    </div>
    <div className="chart-context">
      <span className="chart-axis-label">Sample time</span>
      <span className="chart-legend"><i aria-hidden="true" />{parameterLabel(parameter)}</span>
    </div>
    {interactive && activeDetails && <div className="chart-tooltip" role="status">
      <span>Timestamp: {formatDateTime(activeDetails.timestamp)}</span>
      <span>Chamber: {bottleId == null ? "Unavailable" : `Chamber ${displayChamberNumber(bottleId)}`}</span>
      <span>Experiment: {formatExperimentId(activeReading!.experiment_id)}</span>
      <span>Parameter: {parameterLabel(activeDetails.parameter)}</span>
      <span>Value: {formatTooltipValue(activeDetails.value, activeDetails.parameter)}</span>
    </div>}
  </>;
}

function BottleChart({ readings, parameter, bottleId }: { readings: Reading[]; parameter: ParameterName; bottleId: number | null }) {
  return <section className="panel telemetry-chart-panel">
    <div className="panel-heading"><div><p className="eyebrow">{bottleId === null ? "Selected chamber" : `Chamber ${displayChamberNumber(bottleId)}`}</p><h2>{parameterLabel(parameter)}</h2></div><span className="tag">{readings.length} readings</span></div>
    <div className="chart"><Chart rows={readings} parameter={parameter} bottleId={bottleId} interactive /></div>
  </section>;
}

/** Renders a measurement, or a dash when the sensor reported nothing. */
function formatValue(value: number | null, parameter: ParameterName): string {
  return value === null ? "-" : value.toFixed(PARAMETER_DIGITS[parameter]);
}

function formatDate(value: string | null): string {
  return value ? new Date(value).toLocaleString([], { dateStyle: "medium", timeStyle: "short" }) : "Not available";
}

function formatDateTime(value: string): string {
  return new Date(value).toLocaleString([], { dateStyle: "medium", timeStyle: "medium" });
}

function formatTooltipValue(value: number | null, parameter: ParameterName): string {
  if (value === null) return "Unavailable";
  const unit = PARAMETER_UNITS[parameter];
  return unit ? `${String(value)} ${unit}` : String(value);
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
  if (rows.length === 0) return [];
  const values = rows.map((row) => getMetricValue(row, parameter));
  const reported = values.filter((value): value is number => value !== null);
  if (rows.length === 1) return [{ x: 400, y: values[0] === null ? null : 110 }];
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

function MetricCard({ name, value, index, compact = false }: { name: ParameterName; value: number | null; index: number; compact?: boolean }) {
  const colors = ["blue", "orange", "green", "purple", "pink"];
  if (compact) {
    return <article className="metric metric-compact">
      <p>{name}</p>
      <strong>{value === null ? "-" : formatValue(value, name)}</strong>
      <span className="unit">{value === null ? "No reading" : PARAMETER_UNITS[name] || "Unitless"}</span>
    </article>;
  }
  return <article className="metric">
    <div className={`metric-icon ${colors[index]}`}><span /></div>
    <div><p>{name}</p><strong>{value === null ? "-" : formatValue(value, name)}</strong><span className="unit">{value === null ? "No reading" : PARAMETER_UNITS[name] || "Unitless"}</span></div>
    <span className="trend">{value === null ? "-" : "Latest"}</span>
  </article>;
}

function StatusCard({ label, value, tone }: { label: string; value: string; tone?: "live" | "warning" | "neutral" }) {
  return <article className="metric status-card">
    <p>{label}</p>
    <strong>{tone ? <span className={`status-dot ${tone}`} aria-hidden="true" /> : null}{value}</strong>
  </article>;
}

export default App;
