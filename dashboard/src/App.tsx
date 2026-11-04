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
  DEV_FORCE_LIVE_EXPERIMENT,
  generateSyntheticLiveReadings,
  isMonitoringPollingEligible,
  isSyntheticExperiment,
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
import {
  addSelection,
  canAddSelection,
  compareSeriesColor,
  compareSeriesLabel,
  distinctExperimentIds,
  removeSelection,
  MAX_COMPARE_SERIES,
  type CompareSelection,
} from "./compare";
import type { DeviceSession, Reading } from "./types/domain";
import { formatExperimentId, isTestingExperiment } from "./types/domain";

type DashboardView = "monitoring" | "history" | "compare";
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

function BrandMark() {
  return <svg className="brand-mark" viewBox="0 0 28 28" aria-hidden="true">
    <rect x="2" y="2" width="24" height="24" rx="6" fill="none" stroke="currentColor" strokeWidth="1.5" />
    <path d="M9 8v12M14 8v12M19 8v12" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" />
    <path d="M6 20h16" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" />
  </svg>;
}

function MonitoringIcon() {
  return <svg viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeLinecap="round" strokeLinejoin="round">
    <path d="M1.5 8h3l1.5-4 3 8 1.5-4h4" />
  </svg>;
}

function HistoryIcon() {
  return <svg viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeLinecap="round" strokeLinejoin="round">
    <path d="M2.5 8a5.5 5.5 0 1 1 1.6 3.9M2.5 8V5m0 3h3M8 5.5V8l2 1.5" />
  </svg>;
}

function CompareIcon() {
  return <svg viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeLinecap="round" strokeLinejoin="round">
    <path d="M1.5 11.5 5.5 7l2.5 2.5 6.5-6.5" />
    <path d="M1.5 13.5 5.5 9l2.5 2.5 6.5-6.5" opacity="0.45" />
  </svg>;
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
      <div className="brand">
        <BrandMark />
        <span className="brand-name">RuSim <small>Telemetry</small></span>
      </div>
    </header>

    <div className="login-main">
      <section className="login-card" aria-labelledby="login-title">
        <div className="login-heading">
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
  const [menuOpen, setMenuOpen] = useState(false);
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
  const realLiveExperiment = selectActiveExperiment(experiments);
  // Preview flag: when no real experiment is live, show synthetic fresh data so
  // Monitoring can be reviewed without a device. History always uses real data.
  const syntheticReadings = useMemo(
    () => (DEV_FORCE_LIVE_EXPERIMENT && !realLiveExperiment
      ? generateSyntheticLiveReadings(deviceId, bottleCount)
      : []),
    [deviceId, bottleCount, realLiveExperiment, readings],
  );
  const syntheticExperiment: ExperimentSummary | null = syntheticReadings.length > 0
    ? {
        experiment_id: syntheticReadings[0]!.experiment_id,
        reading_count: syntheticReadings.length,
        first_timestamp: syntheticReadings.reduce(
          (earliest, reading) => (reading.timestamp < earliest ? reading.timestamp : earliest),
          syntheticReadings[0]!.timestamp,
        ),
        last_timestamp: syntheticReadings.reduce(
          (latest, reading) => (reading.timestamp > latest ? reading.timestamp : latest),
          syntheticReadings[0]!.timestamp,
        ),
        concluded: false,
      }
    : null;
  const liveExperiment = realLiveExperiment ?? syntheticExperiment;
  const isSyntheticPreview = liveExperiment !== null && isSyntheticExperiment(liveExperiment.experiment_id);
  const monitoringReadings = useMemo(
    () => (isSyntheticPreview
      ? syntheticReadings
      : selectMonitoringReadings(readings, liveExperiment?.experiment_id ?? null)),
    [readings, liveExperiment?.experiment_id, isSyntheticPreview, syntheticReadings],
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
      : isSyntheticPreview
        ? "Preview"
        : readings.length > 0
          ? "Data loaded"
          : "No readings";
  const pulseTone = loading ? "pending" : error ? "error" : isSyntheticPreview || readings.length > 0 ? "ok" : "idle";
  const bottleState = selectedBottleIsDisconnected
    ? "Disconnected"
    : isSyntheticPreview
      ? "Live"
      : latestReading
        ? (isConcluded(latestReading.timestamp) ? "Concluded" : "Live")
        : "No data";
  const bottleStateTone: "live" | "warning" | "neutral" = selectedBottleIsDisconnected
    ? "warning"
    : isSyntheticPreview
      ? "live"
      : latestReading && !isConcluded(latestReading.timestamp)
        ? "live"
        : "neutral";

  const pageTitle = view === "monitoring" ? "Device telemetry" : view === "history" ? "Experiment history" : "Compare chambers";
  const pageSub = view === "monitoring"
    ? liveExperiment
      ? `${formatExperimentId(liveExperiment.experiment_id)} · ${presence.connected.length} of ${bottleCount} chambers reporting`
      : "No live experiment. Historical readings remain available in Experiment history."
    : view === "history"
      ? `${experiments.length} experiments · Historical data`
      : "Overlay up to four chambers across experiments";

  return <div className="app">
    <aside className={`sidebar${menuOpen ? " open" : ""}`} aria-label="Primary">
      <div className="brand">
        <BrandMark />
        <span className="brand-name">RuSim <small>Telemetry</small></span>
      </div>
      <div className="scope">
        <div className="scope-label">Device</div>
        <div className="scope-value">Device <span className="scope-id">{deviceId}</span></div>
        <div className="scope-owner">{ownerName ?? "Unnamed owner"}{ownerEmail ? ` · ${ownerEmail}` : ""}</div>
      </div>
      <div className="group-label">Views</div>
      <button
        className="nav-item"
        aria-current={view === "monitoring" ? "page" : undefined}
        onClick={() => { setView("monitoring"); setMenuOpen(false); }}
      >
        <MonitoringIcon /> Monitoring
      </button>
      <button
        className="nav-item"
        aria-current={view === "history" ? "page" : undefined}
        onClick={() => { setView("history"); setMenuOpen(false); }}
      >
        <HistoryIcon /> Experiment history
      </button>
      <button
        className="nav-item"
        aria-current={view === "compare" ? "page" : undefined}
        onClick={() => { setView("compare"); setMenuOpen(false); }}
      >
        <CompareIcon /> Compare
      </button>
      <div className="bottom">
        <p className="session-note">Session expires {new Date(session.expiresAt).toLocaleString([], { dateStyle: "medium", timeStyle: "short" })}.</p>
        <button className="btn" onClick={() => void onSignOut()}>Sign out</button>
      </div>
    </aside>

    <header className="topbar">
      <div style={{ display: "flex", alignItems: "center", gap: 12 }}>
        <button className="btn menu-toggle" onClick={() => setMenuOpen((open) => !open)} aria-expanded={menuOpen} aria-label="Menu">Menu</button>
        <span className="topbar-title">{pageTitle}</span>
      </div>
      <div className="topbar-right">
        <span className="stamp">
          <span className={`status-dot ${view === "monitoring" ? (pulseTone === "ok" ? "live" : pulseTone === "error" ? "warning" : "") : ""}`} aria-hidden="true" />
          {" "}{view === "monitoring" ? connectionStatus : "Historical data"}
          {view === "monitoring" ? ` · Updated ${latestReadingAt}` : ""}
        </span>
        <button className="btn primary" onClick={() => void loadData()} disabled={loading}>Refresh data</button>
      </div>
    </header>

    <main className="sheet">
      <div className="title-row">
        <div>
          <h1>{pageTitle}</h1>
          <p className="sub">{pageSub}</p>
        </div>
      </div>

      {view === "monitoring" ? <div className="stack">
        {isSyntheticPreview && <div className="notice" role="status">
          <span className="notice-label">Preview</span>
          <p>Synthetic live data (DEV_FORCE_LIVE_EXPERIMENT=1). Set to 0 before deploying.</p>
        </div>}
        {error && <div className="error-block" role="alert">
          <span>Unable to refresh readings; showing the last loaded data. {error}</span>
          <button className="btn retry" onClick={() => void loadData()}>Retry</button>
        </div>}
        {loading && <div className="notice">Loading readings...</div>}
        {!loading && !liveExperiment ? <section className="panel">
          <h2>No live experiment</h2>
          <p className="meta" style={{ marginTop: 8 }}>Historical readings remain available in Experiment history.</p>
        </section> : <>
        <section className="panel" aria-label="Chamber presence">
          <div className="presence-head">
            <h2>Chambers at latest sample</h2>
            <span className="meta">{presence.latestSample ? `Sampled ${formatDate(presence.latestSample)}` : "No sample yet"}</span>
          </div>
          <div className="presence-strip" role="group" aria-label="Select a chamber">
            {internalChamberIds(bottleCount).map((internalId) => {
              const disconnected = presence.disconnected.includes(internalId);
              const selected = internalId === selectedBottle;
              return <button
                key={internalId}
                className={`presence-cell${disconnected ? " disconnected" : ""}`}
                aria-pressed={selected}
                title={`Chamber ${displayChamberNumber(internalId)} — ${disconnected ? "Disconnected" : "Connected"}`}
                onClick={() => setSelectedBottle(internalId)}
              >
                {displayChamberNumber(internalId)}
              </button>;
            })}
          </div>
          <div className="presence-legend" aria-hidden="true">
            <span><i /> Connected</span>
            <span><i className="off" /> Disconnected</span>
          </div>
        </section>

        {presence.latestSample && presence.disconnected.length > 0 && <div className="notice warn" role="status">
          <span className="notice-label">Missing chamber data</span>
          <p>Disconnected at {formatDate(presence.latestSample)}: chambers {presence.disconnected.map((internalId) => `Chamber ${displayChamberNumber(internalId)}`).join(", ")} sent no data.</p>
        </div>}

        <section aria-label="Latest parameter readings">
          <div className="kpis" role="tablist" aria-label="Latest readings and chart parameter">
            {PARAMETER_NAMES.map((name) => {
              const value = latestReading ? getParameterValue(latestReading, name) : null;
              const selected = name === chartParameter;
              return <button
                key={name}
                className="kpi"
                role="tab"
                aria-selected={selected}
                onClick={() => setChartParameter(name)}
              >
                <span className="kpi-label">{name}</span>
                <span className="kpi-value num">
                  {value === null ? <span className="dash">-</span> : <>{formatValue(value, name)}{PARAMETER_UNITS[name] ? <small>{PARAMETER_UNITS[name]}</small> : null}</>}
                </span>
              </button>;
            })}
          </div>
          <p className="status-line" style={{ marginTop: 8 }}>
            <span className={`status-dot ${bottleStateTone === "live" ? "live" : bottleStateTone === "warning" ? "warning" : ""}`} aria-hidden="true" />
            Chamber {displayChamberNumber(selectedBottle)} · {bottleState} · {bottleReadings.length} samples · Latest sample {latestReadingAt}
          </p>
        </section>

        {!loading && !error && bottleReadings.length === 0 && <div className="notice">
          No readings are available for Chamber {displayChamberNumber(selectedBottle)}{presence.latestSample ? "; it did not report at the latest sample" : ""}.
        </div>}

        <section className="panel" aria-label={`${parameterLabel(chartParameter)} chart`}>
          <div className="chart-head">
            <h3>{parameterLabel(chartParameter)} readings</h3>
            <span className="meta">{bottleReadings.length} records</span>
          </div>
          <div className="chart">
            {bottleReadings.length < 2
              ? <div className="empty compact">Not enough data for chart.</div>
              : <Chart rows={bottleReadings} parameter={chartParameter} bottleId={selectedBottle} interactive />}
          </div>
        </section>

        <footer style={{ color: "var(--faint)", fontSize: "var(--fs-small)" }}>RuSim · Device telemetry — Data source: Supabase</footer>
        </>}
      </div> : view === "history" ? <ExperimentHistoryView experiments={experiments} loading={loading} error={error} sessionToken={sessionToken} /> : <CompareView experiments={experiments} loading={loading} error={error} sessionToken={sessionToken} bottleCount={bottleCount} />}
    </main>
  </div>;
}

type CompareSeries = {
  label: string;
  color: string;
  rows: Reading[];
};

type CompareActivePoint = { seriesIndex: number; index: number; pinned: boolean };

function CompareView({
  experiments,
  loading,
  error,
  sessionToken,
  bottleCount,
}: {
  experiments: ExperimentSummary[];
  loading: boolean;
  error: string;
  sessionToken: string;
  bottleCount: number;
}) {
  const [selections, setSelections] = useState<CompareSelection[]>([]);
  const [draftExperimentId, setDraftExperimentId] = useState<number | null>(null);
  const [draftChamberId, setDraftChamberId] = useState(FIRST_INTERNAL_CHAMBER_ID);
  const [parameter, setParameter] = useState<ParameterName>("CH4");
  const [readingsByExperiment, setReadingsByExperiment] = useState<Record<number, Reading[]>>({});
  const [detailLoading, setDetailLoading] = useState(false);
  const [detailError, setDetailError] = useState("");

  useEffect(() => {
    setDraftExperimentId((current) => (
      current !== null && experiments.some((experiment) => experiment.experiment_id === current)
        ? current
        : experiments[0]?.experiment_id ?? null
    ));
  }, [experiments]);

  useEffect(() => {
    const missing = distinctExperimentIds(selections).filter((id) => !(id in readingsByExperiment));
    if (missing.length === 0) return;

    let cancelled = false;
    setDetailLoading(true);
    setDetailError("");
    void Promise.all(missing.map((id) => fetchExperimentReadings(sessionToken, id)))
      .then((results) => {
        if (cancelled) return;
        setReadingsByExperiment((previous) => {
          const next = { ...previous };
          missing.forEach((id, index) => {
            next[id] = results[index] ?? [];
          });
          return next;
        });
      })
      .catch((err: unknown) => {
        if (cancelled) return;
        setDetailError(err instanceof Error ? err.message : "Unable to load experiment readings");
      })
      .finally(() => {
        if (!cancelled) setDetailLoading(false);
      });

    return () => {
      cancelled = true;
    };
  }, [selections, sessionToken, readingsByExperiment]);

  const series = useMemo<CompareSeries[]>(
    () => selections.map((selection, index) => ({
      label: compareSeriesLabel(selection),
      color: compareSeriesColor(index),
      rows: selectChamberReadings(readingsByExperiment[selection.experimentId] ?? [], selection.experimentId, selection.bottleId),
    })),
    [selections, readingsByExperiment],
  );

  const candidate: CompareSelection | null = draftExperimentId === null
    ? null
    : { experimentId: draftExperimentId, bottleId: draftChamberId };
  const canAdd = candidate !== null && canAddSelection(selections, candidate);
  const totalRecords = series.reduce((sum, item) => sum + item.rows.length, 0);
  const hasChartable = series.some((item) => item.rows.length >= 2);

  return <div className="stack">
    {error && <div className="error-block" role="alert"><span>Unable to load experiments. {error}</span></div>}
    <section className="panel" aria-label="Chambers to compare">
      <header>
        <h2>Chambers to compare</h2>
        <span className="meta">{selections.length} of {MAX_COMPARE_SERIES}</span>
      </header>
      {loading && <div className="notice">Loading experiments...</div>}
      {!loading && experiments.length === 0 && <div className="empty compact">No experiments have readings yet.</div>}
      {!loading && experiments.length > 0 && <>
        <div className="filters">
          <label htmlFor="compare-experiment-select" style={{ fontWeight: 500 }}>Experiment</label>
          <select
            id="compare-experiment-select"
            className="input"
            value={draftExperimentId ?? ""}
            onChange={(event) => setDraftExperimentId(Number(event.target.value))}
          >
            {experiments.map((experiment) => <option value={experiment.experiment_id} key={experiment.experiment_id}>{formatExperimentId(experiment.experiment_id)}</option>)}
          </select>
          <label htmlFor="compare-chamber-select" style={{ fontWeight: 500 }}>Chamber</label>
          <select
            id="compare-chamber-select"
            className="input"
            value={draftChamberId}
            onChange={(event) => setDraftChamberId(Number(event.target.value))}
          >
            {internalChamberIds(bottleCount).map((internalId) => <option value={internalId} key={internalId}>Chamber {displayChamberNumber(internalId)}</option>)}
          </select>
          <button className="btn" onClick={() => { if (candidate) setSelections((previous) => addSelection(previous, candidate)); }} disabled={!canAdd}>
            Add chamber
          </button>
          {selections.length > 0 && <button className="btn ghost" onClick={() => setSelections([])}>Clear all</button>}
        </div>
        {selections.length === 0
          ? <p className="meta" style={{ color: "var(--muted)" }}>Chambers can come from different experiments. Up to four series overlay on one chart, aligned by sample order.</p>
          : <div className="compare-chips" aria-label="Selected chambers">
            {selections.map((selection, index) => <span className="compare-chip" key={`${selection.experimentId}-${selection.bottleId}`}>
              <span className="compare-dot" style={{ background: compareSeriesColor(index) }} aria-hidden="true" />
              <span>{compareSeriesLabel(selection)}</span>
              <button
                className="compare-remove"
                aria-label={`Remove ${compareSeriesLabel(selection)}`}
                onClick={() => setSelections((previous) => removeSelection(previous, index))}
              >
                ×
              </button>
            </span>)}
          </div>}
      </>}
    </section>

    <div className="tabs" role="tablist" aria-label="Compare parameter">
      {PARAMETER_NAMES.map((name) => <button key={name} role="tab" aria-selected={parameter === name} onClick={() => setParameter(name)}>{parameterLabel(name)}</button>)}
    </div>

    {detailError && <div className="error-block" role="alert"><span>Unable to load experiment readings. {detailError}</span></div>}
    <section className="panel" aria-label={`${parameterLabel(parameter)} comparison`}>
      <div className="chart-head">
        <h3>{parameterLabel(parameter)} comparison</h3>
        <span className="meta">{totalRecords} records</span>
      </div>
      {selections.length > 0 && <div className="compare-legend" aria-label="Series legend">
        {series.map((item, index) => <span key={index} className="compare-legend-item">
          <span className="compare-dot" style={{ background: item.color }} aria-hidden="true" />
          <span>{item.label}</span>
          <span className="meta">· {item.rows.length} samples</span>
        </span>)}
      </div>}
      <div className="chart">
        {selections.length === 0
          ? <div className="empty compact">Select up to four chambers to overlay their readings.</div>
          : detailLoading && !hasChartable
            ? <div className="notice">Loading experiment readings...</div>
            : !hasChartable
              ? <div className="empty compact">Not enough data for chart.</div>
              : <CompareChart series={series} parameter={parameter} />}
      </div>
      {selections.length > 0 && <p className="meta" style={{ marginTop: 8, color: "var(--faint)", fontSize: "var(--fs-small)" }}>Series are aligned by sample order (first sample to first sample) with a shared y-axis. Each point shows its timestamp and value.</p>}
    </section>
  </div>;
}

function CompareChart({ series, parameter }: { series: CompareSeries[]; parameter: ParameterName }) {
  const points = useMemo(() => series.map((item) => createChartPoints(item.rows, parameter)), [series, parameter]);
  const yTicks = useMemo(() => createYTicks(series.flatMap((item) => item.rows), parameter), [series, parameter]);
  const labels = useMemo(() => {
    const longest = series.reduce((best, item) => (item.rows.length > best.rows.length ? item : best), series[0] ?? { rows: [] as Reading[] });
    return createTimeLabels(longest.rows);
  }, [series]);
  const [activePoint, setActivePoint] = useState<CompareActivePoint | null>(null);

  useEffect(() => {
    setActivePoint(null);
  }, [series, parameter]);

  const activeReading = activePoint === null ? undefined : series[activePoint.seriesIndex]?.rows[activePoint.index];
  const activeDetails = activeReading === undefined ? undefined : getHistoryPointDetails(activeReading, parameter);
  const activeLabel = activePoint === null ? undefined : series[activePoint.seriesIndex]?.label;

  const plotLeft = 48;
  const plotRight = 792;
  const plotTop = 10;
  const plotBottom = 190;
  const plotWidth = plotRight - plotLeft;
  const plotHeight = plotBottom - plotTop;
  const yMin = yTicks[0] ?? 0;
  const yMax = yTicks[yTicks.length - 1] ?? 1;
  const yRange = yMax - yMin || 1;
  const toY = (value: number) => plotBottom - ((value - yMin) / yRange) * plotHeight;
  const toX = (index: number, count: number) => count <= 1 ? (plotLeft + plotRight) / 2 : plotLeft + (index / (count - 1)) * plotWidth;
  const activePointCoords = activePoint === null ? null : (() => {
    const item = series[activePoint.seriesIndex];
    const point = points[activePoint.seriesIndex]?.[activePoint.index];
    return {
      x: toX(activePoint.index, item?.rows.length ?? 0),
      y: point?.y === null || point === undefined ? (plotTop + plotBottom) / 2 : toY(point.value),
    };
  })();

  return <>
    <svg viewBox="0 0 800 220" role="img" aria-label={`${parameterLabel(parameter)} comparison chart`}>
      {yTicks.map((tick) => <g key={tick}>
        <line x1={plotLeft} x2={plotRight} y1={toY(tick)} y2={toY(tick)} stroke="var(--grid)" strokeWidth="1" />
        <text x={plotLeft - 6} y={toY(tick) + 4} textAnchor="end" fontSize="12" fill="var(--axis-ink)">{formatTick(tick, parameter)}</text>
      </g>)}
      <line x1={plotLeft} x2={plotRight} y1={plotBottom} y2={plotBottom} stroke="var(--border)" strokeWidth="1" />
      {series.map((item, seriesIndex) => <path
        key={`${item.label}-${seriesIndex}`}
        d={chartPath((points[seriesIndex] ?? []).map((point, index) => ({ x: toX(index, item.rows.length), y: point.y === null ? null : toY(point.value) })))}
        fill="none"
        stroke={item.color}
        strokeWidth="2"
        strokeLinejoin="round"
        strokeLinecap="round"
      />)}
      {series.map((item, seriesIndex) => (points[seriesIndex] ?? []).map((point, index) => {
        const reading = item.rows[index];
        if (reading === undefined) return null;
        const selected = activePoint?.seriesIndex === seriesIndex && activePoint?.index === index;
        return <circle
          key={`${item.label}-${reading.timestamp}-${index}`}
          cx={toX(index, item.rows.length)}
          cy={point.y === null ? (plotTop + plotBottom) / 2 : toY(point.value)}
          r={selected ? 5 : 3}
          fill={point.y === null ? "var(--chart-muted)" : item.color}
          stroke="var(--surface)"
          strokeWidth="1.5"
          tabIndex={0}
          role="button"
          aria-label={`${item.label} · ${parameterLabel(parameter)} at ${formatDateTime(reading.timestamp)}`}
          onMouseEnter={() => setActivePoint({ seriesIndex, index, pinned: false })}
          onMouseLeave={() => setActivePoint((current) => current?.pinned ? current : null)}
          onFocus={() => setActivePoint({ seriesIndex, index, pinned: false })}
          onBlur={() => setActivePoint((current) => current?.pinned ? current : null)}
          onClick={() => setActivePoint({ seriesIndex, index, pinned: true })}
        />;
      }))}
      {labels.map((label, index) => {
        const x = labels.length <= 1 ? (plotLeft + plotRight) / 2 : plotLeft + (index / (labels.length - 1 || 1)) * plotWidth;
        const anchor = index === 0 ? "start" : index === labels.length - 1 ? "end" : "middle";
        return <text key={`${label}-${index}`} x={x} y={208} textAnchor={anchor} fontSize="12" fill="var(--axis-ink)">{label}</text>;
      })}
    </svg>
    {activeDetails && activeLabel && activePointCoords && <div
      className="chart-tooltip"
      role="status"
      style={{
        left: `clamp(90px, ${(activePointCoords.x / 800) * 100}%, calc(100% - 90px))`,
        top: `${(activePointCoords.y / 220) * 100}%`,
        transform: activePointCoords.y < 80 ? "translate(-50%, 8px)" : "translate(-50%, calc(-100% - 8px))",
      }}
    >
      <span>{activeLabel}</span>
      <span>{formatDateTime(activeDetails.timestamp)}</span>
      <span>{formatTooltipValue(activeDetails.value, activeDetails.parameter)}</span>
    </div>}
  </>;
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
  const [showAll, setShowAll] = useState(false);

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

  const visibleExperiments = showAll ? experiments : experiments.slice(0, 6);

  return <div className="stack">
    {error && <div className="error-block" role="alert"><span>Unable to load experiment history. {error}</span></div>}
    <section className="panel" aria-label="Experiments">
      <header>
        <h2>Experiments</h2>
        <span className="meta">{experiments.length} records</span>
      </header>
      {loading && <div className="notice">Loading experiments...</div>}
      {!loading && experiments.length === 0 && <div className="empty compact">No experiments have readings yet.</div>}
      {!loading && experiments.length > 0 && <div className="rows">
        {visibleExperiments.map((experiment) => <button
          className="row-button"
          key={experiment.experiment_id}
          aria-selected={experiment.experiment_id === activeExperimentId}
          onClick={() => setSelectedExperimentId((current) => toggleExperimentSelection(current, experiment.experiment_id))}
        >
          <span className="row-main">
            <span className="row-title">{formatExperimentId(experiment.experiment_id)}</span>
            <span className="row-sub">{experiment.reading_count} chamber readings{isTestingExperiment(experiment.experiment_id) ? " · Testing experiment" : ""}</span>
          </span>
          <span className="row-meta">
            <span className={`pill ${experiment.concluded ? "" : "pos"}`}>{experiment.concluded ? "Concluded" : "Live"}</span>
            <span style={{ display: "block", marginTop: 4 }}>Last sample {formatDate(experiment.last_timestamp)}</span>
          </span>
        </button>)}
      </div>}
      {experiments.length > 6 && <div style={{ marginTop: 12 }}>
        <button className="btn ghost" onClick={() => setShowAll((value) => !value)}>{showAll ? `Show fewer` : `Show all ${experiments.length}`}</button>
      </div>}
    </section>

    {selectedExperiment === undefined && <div className="notice">Select an experiment to view its chamber readings and results.</div>}

    {selectedExperiment && <>
      <section className="panel" aria-label="Selected experiment">
        <header>
          <h2>{formatExperimentId(selectedExperiment.experiment_id)}</h2>
          <button
            className="btn"
            onClick={() => downloadExperimentCsv(selectedExperiment.experiment_id, experimentReadings)}
            disabled={detailLoading || detailError !== "" || experimentReadings.length === 0}
            title={csvDisabledReason(experimentReadings.length, detailLoading, detailError)}
          >
            Download CSV
          </button>
        </header>
        <p style={{ color: "var(--muted)" }}>{selectedExperiment.reading_count} chamber readings, sampled between {formatDate(selectedExperiment.first_timestamp)} and {formatDate(selectedExperiment.last_timestamp)}.</p>
        <p style={{ color: "var(--muted)", fontSize: "var(--fs-small)", marginTop: 4 }}>{selectedExperiment.concluded ? "No recent samples, so this experiment is treated as concluded." : "Recent samples are present, so this experiment is treated as live."}</p>
      </section>

      <div className="tabs" role="tablist" aria-label="Experiment detail">
        <button role="tab" aria-selected={historyPanel === "readings"} onClick={() => setHistoryPanel("readings")}>Chamber readings</button>
        <button role="tab" aria-selected={historyPanel === "results"} onClick={() => setHistoryPanel("results")}>Results</button>
      </div>

      {detailError && <div className="error-block" role="alert"><span>Unable to load experiment readings. {detailError}</span></div>}
      {detailLoading && !detailError && <div className="notice">Loading experiment readings...</div>}
      {!detailLoading && !detailError && experimentReadings.length === 0 && <div className="empty compact">No readings are available for this experiment.</div>}
      {isTruncated(experimentReadings.length) && <div className="notice warn">
        Only the most recent {MAX_READING_ROWS.toLocaleString()} readings were loaded, so this experiment is incomplete on screen and any CSV export is truncated too.
      </div>}

      {experimentReadings.length > 0 && historyPanel === "readings" && <>
        <div className="filters">
          <label htmlFor="history-chamber-select" style={{ fontWeight: 500 }}>Chamber</label>
          <select
            id="history-chamber-select"
            className="input"
            value={selectedChamberId ?? ""}
            onChange={(event) => setSelectedChamberId(Number(event.target.value))}
          >
            {availableChambers.map((internalId) => <option value={internalId} key={internalId}>Chamber {displayChamberNumber(internalId)}</option>)}
          </select>
          <span className="spacer" />
          <span className="meta" style={{ color: "var(--faint)", fontSize: "var(--fs-small)" }}>{selectedChamberReadings.length} readings</span>
        </div>
        <div className="stack">
          {PARAMETER_NAMES.map((parameter) => <BottleChart key={parameter} readings={selectedChamberReadings} parameter={parameter} bottleId={selectedChamberId} />)}
        </div>
      </>}

      {experimentReadings.length > 0 && historyPanel === "results" && <ExperimentResultsView chamberSummaries={chamberSummaries} experiment={selectedExperiment} />}
    </>}
  </div>;
}

function ExperimentResultsView({
  chamberSummaries,
  experiment,
}: {
  chamberSummaries: ChamberSummary[];
  experiment: ExperimentSummary;
}) {
  return <section className="panel" aria-label="Chamber results">
    <header>
      <h2>Chamber results</h2>
      <span className="meta">{formatExperimentId(experiment.experiment_id)} · {experiment.concluded ? "Concluded" : "Live"}</span>
    </header>
    <div className="table-wrap">
      <table>
        <thead>
          <tr>
            <th>Chamber</th>
            <th className="n">Samples</th>
            {PARAMETER_NAMES.map((name) => <th key={name} className="n">{parameterLabel(name)}</th>)}
          </tr>
        </thead>
        <tbody>
          {chamberSummaries.map(({ bottle_id, readings, stats }) => <tr key={bottle_id}>
            <td>Chamber {displayChamberNumber(bottle_id)}</td>
            <td className="n num">{readings.length}</td>
            {PARAMETER_NAMES.map((name) => {
              const parameterStats = stats[name];
              return <td key={name} className="n num">
                {parameterStats
                  ? <>{formatValue(parameterStats.average, name)}<span className="sub">{formatValue(parameterStats.min, name)} – {formatValue(parameterStats.max, name)}</span></>
                  : <span style={{ color: "var(--faint)" }}>-</span>}
              </td>;
            })}
          </tr>)}
        </tbody>
      </table>
    </div>
    <p className="meta" style={{ marginTop: 8, color: "var(--faint)", fontSize: "var(--fs-small)" }}>Mean with min – max below. A dash means no bottle reported that sensor.</p>
  </section>;
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
  const yTicks = useMemo(() => createYTicks(rows, parameter), [rows, parameter]);
  const [activePoint, setActivePoint] = useState<ChartInteraction | null>(null);

  useEffect(() => {
    setActivePoint(null);
  }, [rows, parameter, bottleId]);

  const activeReading = activePoint === null ? undefined : rows[activePoint.index];
  const activeDetails = activeReading === undefined
    ? undefined
    : getHistoryPointDetails(activeReading, parameter);

  const plotLeft = 48;
  const plotRight = 792;
  const plotTop = 10;
  const plotBottom = 190;
  const plotWidth = plotRight - plotLeft;
  const plotHeight = plotBottom - plotTop;
  const yMin = yTicks[0] ?? 0;
  const yMax = yTicks[yTicks.length - 1] ?? 1;
  const yRange = yMax - yMin || 1;
  const toY = (value: number) => plotBottom - ((value - yMin) / yRange) * plotHeight;
  const toX = (index: number) => rows.length <= 1 ? (plotLeft + plotRight) / 2 : plotLeft + (index / (rows.length - 1)) * plotWidth;
  const activePointCoords = activePoint === null ? null : (() => {
    const point = points[activePoint.index];
    return {
      x: toX(activePoint.index),
      y: point?.y === null || point === undefined ? (plotTop + plotBottom) / 2 : toY(point.value),
    };
  })();

  return <>
    <svg viewBox="0 0 800 220" role="img" aria-label={`${parameterLabel(parameter)} reading chart`}>
      {yTicks.map((tick) => <g key={tick}>
        <line x1={plotLeft} x2={plotRight} y1={toY(tick)} y2={toY(tick)} stroke="var(--grid)" strokeWidth="1" />
        <text x={plotLeft - 6} y={toY(tick) + 4} textAnchor="end" fontSize="12" fill="var(--axis-ink)">{formatTick(tick, parameter)}</text>
      </g>)}
      <line x1={plotLeft} x2={plotRight} y1={plotBottom} y2={plotBottom} stroke="var(--border)" strokeWidth="1" />
      <path d={chartPath(points.map((point, index) => ({ x: toX(index), y: point.y === null ? null : toY(point.value) })))} fill="none" stroke="var(--chart-1)" strokeWidth="2" strokeLinejoin="round" strokeLinecap="round" />
      {interactive && points.map((point, index) => {
        const reading = rows[index];
        if (reading === undefined) return null;
        const selected = activePoint?.index === index;
        return <circle
          key={`${reading.timestamp}-${index}`}
          cx={toX(index)}
          cy={point.y === null ? (plotTop + plotBottom) / 2 : toY(point.value)}
          r={selected ? 5 : 3}
          fill={point.y === null ? "var(--chart-muted)" : "var(--chart-1)"}
          stroke="var(--surface)"
          strokeWidth="1.5"
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
      {labels.map((label, index) => {
        const x = rows.length <= 1 ? (plotLeft + plotRight) / 2 : plotLeft + (index / (labels.length - 1 || 1)) * plotWidth;
        const anchor = index === 0 ? "start" : index === labels.length - 1 ? "end" : "middle";
        return <text key={`${label}-${index}`} x={x} y={208} textAnchor={anchor} fontSize="12" fill="var(--axis-ink)">{label}</text>;
      })}
    </svg>
    {interactive && activeDetails && activePointCoords && <div
      className="chart-tooltip"
      role="status"
      style={{
        left: `clamp(90px, ${(activePointCoords.x / 800) * 100}%, calc(100% - 90px))`,
        top: `${(activePointCoords.y / 220) * 100}%`,
        transform: activePointCoords.y < 80 ? "translate(-50%, 8px)" : "translate(-50%, calc(-100% - 8px))",
      }}
    >
      <span>{formatDateTime(activeDetails.timestamp)}</span>
      <span>{formatTooltipValue(activeDetails.value, activeDetails.parameter)}</span>
    </div>}
  </>;
}

function BottleChart({ readings, parameter, bottleId }: { readings: Reading[]; parameter: ParameterName; bottleId: number | null }) {
  return <section className="panel" aria-label={`${parameterLabel(parameter)} chart`}>
    <div className="chart-head">
      <h3>{bottleId === null ? "Selected chamber" : `Chamber ${displayChamberNumber(bottleId)}`} · {parameterLabel(parameter)}</h3>
      <span className="meta">{readings.length} readings</span>
    </div>
    <div className="chart">
      {readings.length < 2
        ? <div className="empty compact">Not enough data for chart.</div>
        : <Chart rows={readings} parameter={parameter} bottleId={bottleId} interactive />}
    </div>
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
function createChartPoints(rows: Reading[], parameter: ParameterName): Array<{ x: number; y: number | null; value: number }> {
  if (rows.length === 0) return [];
  const values = rows.map((row) => getMetricValue(row, parameter));
  const reported = values.filter((value): value is number => value !== null);
  if (rows.length === 1) return [{ x: 400, y: values[0] === null ? null : 110, value: values[0] ?? 0 }];
  if (reported.length === 0) return values.map((_, index) => ({ x: (index / (values.length - 1)) * 800, y: null, value: 0 }));

  const min = Math.min(...reported);
  const max = Math.max(...reported);
  const range = max - min;
  return values.map((value, index) => ({
    x: (index / (values.length - 1)) * 800,
    y: value === null ? null : range === 0 ? 110 : 190 - ((value - min) / range) * 160,
    value: value ?? 0,
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

/** Round y ticks whose top sits above the largest value, per app-screens §6.3. */
function createYTicks(rows: Reading[], parameter: ParameterName): number[] {
  const values = rows.map((row) => getMetricValue(row, parameter)).filter((value): value is number => value !== null);
  if (values.length === 0) return [0, 1];
  let min = Math.min(...values);
  let max = Math.max(...values);
  if (max === min) max = min + 1;
  const raw = (max - min) / 4;
  const mag = 10 ** Math.floor(Math.log10(raw));
  const step = [1, 2, 2.5, 5, 10].map((m) => m * mag).find((s) => s >= raw) || 10 * mag;
  const lo = Math.floor(min / step) * step;
  let hi = Math.ceil(max / step) * step;
  if (hi <= max) hi += step;
  const out: number[] = [];
  for (let v = lo; v <= hi + step / 2; v += step) out.push(+v.toFixed(10));
  return out;
}

function formatTick(value: number, parameter: ParameterName): string {
  const digits = PARAMETER_DIGITS[parameter];
  return value.toFixed(digits);
}

export default App;
