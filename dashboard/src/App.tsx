// IMPORTANT: For AI agents, ALWAYS update the documentation in docs/dashboard.md whenever any changes are made.

import { useCallback, useEffect, useId, useLayoutEffect, useMemo, useState, type CSSProperties, type KeyboardEvent } from "react";
import { useAuth } from "./auth";
import { useEntranceOnce, useIndicator } from "./motion";
import { csvFileName, downloadCsv, toCsv } from "./csv";
import { newestAgeSeconds, startFetchTimer } from "./metrics";
import { displayChamberNumber, internalChamberIds, FIRST_INTERNAL_CHAMBER_ID } from "./chambers";
import {
  fetchReadings,
  fetchExperimentReadings,
  getParameterValue,
  isConcluded,
  MAX_EXPERIMENT_ROWS,
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

/** Names shown on screen. Stored and exported names stay as PARAMETER_NAMES. */
const PARAMETER_DISPLAY: Record<ParameterName, string> = {
  pH: "pH",
  Temp: "Temperature",
  CO2: "CO\u2082",
  CH4: "CH\u2084",
  Pressure: "Pressure",
};

function App() {
  const { session, signingIn, error: authError, signIn, signOut } = useAuth();
  // True after a sign-in in this page load, so the dashboard fades in over the
  // login screen instead of cutting to it. A stored session opens without it.
  const [handoff, setHandoff] = useState(false);
  // It is set before the attempt, so the dashboard's very first frame already
  // carries the class and never flashes at full opacity.
  const signInWithHandoff = useCallback(async (token: string) => {
    setHandoff(true);
    const ok = await signIn(token);
    if (!ok) setHandoff(false);
    return ok;
  }, [signIn]);

  if (!session) {
    return <LoginScreen signingIn={signingIn} error={authError} onSignIn={signInWithHandoff} />;
  }

  // The wrapper always renders, so dropping the class never remounts the dashboard.
  return <div
    className={handoff ? "login-handoff" : undefined}
    onAnimationEnd={(event) => { if (event.target === event.currentTarget) setHandoff(false); }}
  >
    <Dashboard session={session} onSignOut={signOut} />
  </div>;
}

function BrandMark() {
  return <svg className="brand-mark" viewBox="0 0 28 28" aria-hidden="true">
    <rect x="2" y="2" width="24" height="24" rx="5" fill="none" stroke="currentColor" strokeWidth="1.5" />
    <path d="M9 8v12M14 8v12M19 8v12" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" />
    <path d="M6 20h16" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" />
    <circle className="brand-dot" cx="19" cy="8" r="2.25" />
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

function RefreshIcon() {
  return <svg className="btn-icon" viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
    <path d="M13.5 8a5.5 5.5 0 1 1-1.6-3.9" />
    <path d="M13.5 2.5v3h-3" />
  </svg>;
}

function MenuIcon() {
  return <svg className="btn-icon" viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round">
    <path d="M2.5 4.5h11M2.5 8h11M2.5 11.5h11" />
  </svg>;
}

function ChevronIcon() {
  return <svg className="row-chevron" viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
    <path d="M6 3.5 10.5 8 6 12.5" />
  </svg>;
}

/** A title that rises line by line on the first load (styles.css `.is-entering .line`). */
function TitleLines({ lines }: { lines: string[] }) {
  return <>{lines.map((line, index) => <span className="line" key={index}><span>{line}</span></span>)}</>;
}

/** Ambient bubbles behind the login identity panel: size (px), column (%), duration (s), delay (s). Decorative only. */
const LOGIN_BUBBLES: Array<[size: number, left: number, duration: number, delay: number]> = [
  [10, 8, 14, 0], [6, 18, 11, 3.5], [14, 31, 17, 1.2], [8, 44, 12, 6],
  [5, 55, 10, 2.4], [12, 67, 16, 4.8], [7, 78, 13, 0.6], [16, 88, 19, 7.2],
];

/** One quiet fermentation curve (gas rising, then levelling) drawn once on entrance. */
const LOGIN_TRACE = "M0 92 C 60 91, 110 88, 160 78 S 250 46, 320 34 S 430 20, 520 17 S 610 15, 640 15";

function ArrowIcon() {
  return <svg className="btn-icon login-arrow" viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
    <path d="M3 8h10M9 4l4 4-4 4" />
  </svg>;
}

/** The reveal button's icon shows its action: a plain eye to show the token, a slashed eye (the slash draws in) to hide it. */
function EyeIcon({ slashed }: { slashed: boolean }) {
  return <svg viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
    <path d="M1.5 8s2.4-4.5 6.5-4.5S14.5 8 14.5 8 12.1 12.5 8 12.5 1.5 8 1.5 8Z" />
    <circle cx="8" cy="8" r="2" />
    <path className={`eye-slash${slashed ? " is-on" : ""}`} pathLength={1} d="M2.5 2.5l11 11" />
  </svg>;
}

function LockIcon() {
  return <svg viewBox="0 0 16 16" aria-hidden="true" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
    <rect x="3" y="7" width="10" height="7" rx="1.5" />
    <path d="M5.5 7V5a2.5 2.5 0 0 1 5 0v2" />
  </svg>;
}

/** The RuSim logo: the BrandMark glyph with the product wordmark beside it. */
function RusimLogo() {
  return <div className="rusim-logo">
    <BrandMark />
    <span className="rusim-logo-name">RuSim<small>In Vitro Rumen Simulator</small></span>
  </div>;
}

/**
 * Plays a short horizontal shake on an element when a sign-in attempt fails.
 * The Web Animations API is used instead of a CSS class so the shake replays
 * on every failure without remounting the input, which would drop its focus.
 */
function shake(element: HTMLElement | null) {
  if (!element || window.matchMedia("(prefers-reduced-motion: reduce)").matches) return;
  element.animate(
    [
      { transform: "none" },
      { transform: "translateX(-6px)" },
      { transform: "translateX(5px)" },
      { transform: "translateX(-3px)" },
      { transform: "translateX(2px)" },
      { transform: "none" },
    ],
    { duration: 360, easing: "cubic-bezier(0.36, 0.07, 0.19, 0.97)" },
  );
}

/** Props that put an element into the entrance sequence at position `i` (styles.css `[data-enter-step]`). */
function enterStep(i: number) {
  return { "data-enter-step": true, style: { "--i": i } as CSSProperties };
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
  const [reveal, setReveal] = useState(false);
  const [capsLock, setCapsLock] = useState(false);
  const [field, setField] = useState<HTMLDivElement | null>(null);
  const entering = useEntranceOnce();
  const errorId = useId();
  const year = new Date().getFullYear();

  // auth.ts clears the error to "" before every attempt, so each failure is a new value and shakes once.
  useEffect(() => {
    if (error) shake(field);
  }, [error, field]);

  const trackCaps = (event: KeyboardEvent<HTMLInputElement>) => setCapsLock(event.getModifierState("CapsLock"));

  return <main className={`login-shell${entering ? " is-entering" : ""}`}>
    <aside className="login-aside" aria-labelledby="login-product">
      <div className="login-bubbles" aria-hidden="true">
        {LOGIN_BUBBLES.map(([size, left, duration, delay], index) => <span
          key={index}
          style={{ "--s": `${size}px`, "--l": `${left}%`, "--t": `${duration}s`, "--d": `${delay}s` } as CSSProperties}
        />)}
      </div>

      <div className="lab-id" {...enterStep(0)}>
        {/* One-colour emblem for the dark panel; public/unpad-mono.svg, see docs/dashboard.md §5.11. */}
        <img className="unpad-logo" src="/unpad-mono.svg" width="44" height="44" alt="Universitas Padjadjaran" />
        <span className="lab-divider" aria-hidden="true" />
        <span className="lab-name">IAIS Lab<small>Teknik Elektro · Universitas Padjadjaran</small></span>
      </div>

      <div className="login-aside-body">
        <p className="login-eyebrow" {...enterStep(0)}><span className="live-dot" aria-hidden="true" />RuSim · In-vitro rumen simulator</p>
        <h1 id="login-product"><TitleLines lines={["In Vitro Rumen", "Fermentation Dashboard"]} /></h1>
        <p className="login-lead" {...enterStep(1)}>
          Real-time monitoring of every fermentation chamber, with experiment history, chamber comparison, and CSV export.
        </p>
        <ul className="param-chips" aria-label="Measured parameters" {...enterStep(2)}>
          {PARAMETER_NAMES.map((name) => <li key={name}>{PARAMETER_DISPLAY[name]}</li>)}
        </ul>
      </div>

      <svg className="login-trace" viewBox="0 0 640 110" preserveAspectRatio="none" aria-hidden="true">
        <path className="login-trace-grid" d="M0 30H640M0 60H640M0 90H640" />
        <path className={`login-trace-line${entering ? " draw" : ""}`} pathLength={1} d={LOGIN_TRACE} />
      </svg>

      <p className="login-aside-foot" {...enterStep(3)}>© {year} IAIS Lab, Departemen Teknik Elektro, Universitas Padjadjaran</p>
    </aside>

    <section className="login-panel" aria-labelledby="login-title">
      <div className="login-form-wrap">
        <div className="login-heading" {...enterStep(1)}>
          <RusimLogo />
          <h2 className="login-title" id="login-title">Sign in</h2>
          <p className="login-subtitle">Enter the device token issued for your RuSim unit.</p>
        </div>

        <form className="login-form" onSubmit={(event) => { event.preventDefault(); if (!signingIn) void onSignIn(token); }} {...enterStep(2)}>
          <div className="login-field" ref={setField}>
            <label htmlFor="device-token">Device token</label>
            <div className={`login-input${error ? " is-invalid" : ""}`}>
              <input
                id="device-token"
                type={reveal ? "text" : "password"}
                value={token}
                onChange={(event) => setToken(event.target.value)}
                onKeyDown={trackCaps}
                onKeyUp={trackCaps}
                onBlur={() => setCapsLock(false)}
                placeholder="Paste your device token"
                autoComplete="current-password"
                spellCheck={false}
                aria-invalid={error ? true : undefined}
                aria-describedby={error ? errorId : undefined}
                autoFocus
                required
              />
              <button
                type="button"
                className="login-reveal"
                onClick={() => setReveal((value) => !value)}
                aria-label={reveal ? "Hide token" : "Show token"}
                aria-pressed={reveal}
              >
                <EyeIcon slashed={reveal} />
              </button>
            </div>
            <p className={`login-caps${capsLock ? " is-on" : ""}`} aria-live="polite">{capsLock ? "Caps Lock is on" : ""}</p>
          </div>

          {error && <div className="login-error" role="alert" id={errorId}>{error}</div>}

          <button className={`btn primary login-submit${signingIn ? " is-busy" : ""}`} type="submit" disabled={signingIn || token === ""} aria-busy={signingIn}>
            <span className="login-submit-label" key={signingIn ? "busy" : "idle"}>{signingIn ? "Verifying token" : "Sign in"}</span>
            {signingIn ? <RefreshIcon /> : <ArrowIcon />}
          </button>
        </form>

        <p className="login-hint" {...enterStep(3)}>
          <LockIcon />
          <span>The token opens a 12-hour session scoped to its own device. The token itself is never stored in the browser.</span>
        </p>
      </div>
    </section>
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
  // Time of the last successful load. It keys the status dot's pulse, so the
  // ring plays once per fresh batch of data instead of looping forever.
  const [loadedAt, setLoadedAt] = useState(0);
  const entering = useEntranceOnce();
  // Set by the first view switch; from then on each new view settles in
  // (styles.css .view-swap.is-swapping). The first view uses the entrance.
  const [swapped, setSwapped] = useState(false);
  const nav = useIndicator<HTMLElement>(view, "box");
  const kpiBar = useIndicator<HTMLDivElement>(chartParameter, "bar");

  const loadData = useCallback(async () => {
    setLoading(true);
    setError("");
    try {
      const timer = startFetchTimer("fetchReadings");
      const rows = await fetchReadings(sessionToken);
      setReadings(rows);
      timer({ rows: rows.length, newest_age_s: newestAgeSeconds(rows) });
      setLoadedAt(Date.now());
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
  const navLabel = view === "monitoring" ? "Monitoring" : view === "history" ? "Experiment history" : "Compare";
  const pageSub = view === "monitoring"
    ? liveExperiment
      ? `${presence.connected.length} of ${bottleCount} chambers reporting at the latest sample.`
      : "No live experiment. Historical readings remain available in Experiment history."
    : view === "history"
      ? `${experiments.length} experiments with stored readings. Select one to chart its chambers or export it.`
      : "Overlay up to four chambers, from any experiment, on one chart.";
  const dotTone = pulseTone === "ok" ? "live" : pulseTone === "error" ? "warning" : "";

  const go = (next: DashboardView) => {
    if (next !== view) setSwapped(true);
    setView(next);
    setMenuOpen(false);
  };

  return <div className="app">
    <aside className={`sidebar${menuOpen ? " open" : ""}`} aria-label="Primary">
      <div className="brand">
        <BrandMark />
        <span className="brand-name">RuSim <small>Telemetry</small></span>
      </div>
      <div className="scope">
        <div className="scope-label">Signed in to</div>
        <div className="scope-value">Device <span className="scope-id">{deviceId}</span></div>
        <div className="scope-owner">{ownerName ?? "Unnamed owner"}{ownerEmail ? ` · ${ownerEmail}` : ""}</div>
      </div>
      <div className="group-label">Views</div>
      <nav className="nav" ref={nav.containerRef} aria-label="Views">
        <span className={`nav-indicator${nav.indicatorClass}`} style={nav.indicatorStyle} aria-hidden="true" />
        <button className="nav-item" aria-current={view === "monitoring" ? "page" : undefined} onClick={() => go("monitoring")}>
          <MonitoringIcon /> Monitoring
        </button>
        <button className="nav-item" aria-current={view === "history" ? "page" : undefined} onClick={() => go("history")}>
          <HistoryIcon /> Experiment history
        </button>
        <button className="nav-item" aria-current={view === "compare" ? "page" : undefined} onClick={() => go("compare")}>
          <CompareIcon /> Compare
        </button>
      </nav>
      <div className="bottom">
        <p className="session-note">Session expires {new Date(session.expiresAt).toLocaleString([], { dateStyle: "medium", timeStyle: "short" })}.</p>
        <button className="btn" onClick={() => void onSignOut()}>Sign out</button>
      </div>
    </aside>
    <button className={`backdrop${menuOpen ? " open" : ""}`} aria-hidden="true" tabIndex={-1} onClick={() => setMenuOpen(false)} />

    <header className="topbar">
      <div className="topbar-left">
        <button className="btn menu-toggle" onClick={() => setMenuOpen((open) => !open)} aria-expanded={menuOpen} aria-label={menuOpen ? "Close menu" : "Open menu"}><MenuIcon /></button>
        <span className="crumbs">
          <span className="crumb-device">Device <span className="mono">{deviceId}</span></span>
          <span className="sep" aria-hidden="true">/</span>
          <b>{navLabel}</b>
        </span>
      </div>
      <div className="topbar-right">
        <span className="stamp">
          {view === "monitoring"
            ? <span key={loadedAt} className={`status-dot ${dotTone}${dotTone === "live" && loadedAt > 0 ? " pulse" : ""}`} aria-hidden="true" />
            : <span className="status-dot" aria-hidden="true" />}
          {view === "monitoring" ? `${connectionStatus} · Updated ${latestReadingAt}` : "Historical data"}
        </span>
        <button className={`btn primary refresh${loading ? " is-busy" : ""}`} onClick={() => void loadData()} disabled={loading} aria-label="Refresh data">
          <RefreshIcon /><span className="btn-label">Refresh data</span>
        </button>
      </div>
    </header>

    <main className={`sheet${entering ? " is-entering" : ""}`}>
      <div className={`view-swap${swapped ? " is-swapping" : ""}`} key={view}>
        <div className="title-row">
          <div>
            {view === "monitoring" && liveExperiment && <p className="eyebrow">
              <span className={`status-dot ${isSyntheticPreview || !liveExperiment.concluded ? "live" : ""}`} aria-hidden="true" />
              Live experiment <span className="mono">{formatExperimentId(liveExperiment.experiment_id)}</span>
            </p>}
            <h1><TitleLines lines={[pageTitle]} /></h1>
            <p className="sub" data-enter-step style={{ "--i": 0 } as CSSProperties}>{pageSub}</p>
          </div>
        </div>

        <div data-enter-step style={{ "--i": 1 } as CSSProperties}>
        {view === "monitoring" ? <div className="stack">
          {isSyntheticPreview && <div className="notice" role="status">
            <span className="notice-label">Preview</span>
            <p>Synthetic live data (DEV_FORCE_LIVE_EXPERIMENT=1). Set to 0 before deploying.</p>
          </div>}
          {error && <div className="error-block" role="alert">
            <span>Unable to refresh readings; showing the last loaded data. {error}</span>
            <button className="btn retry" onClick={() => void loadData()}>Retry</button>
          </div>}
          {loading && <div className="loading-line" role="status" aria-label="Loading readings" />}
          {!loading && !liveExperiment ? <section className="panel">
            <h2>No live experiment</h2>
            <p className="panel-note">Historical readings remain available in Experiment history.</p>
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
                  aria-label={`Chamber ${displayChamberNumber(internalId)}, ${disconnected ? "disconnected" : "connected"}`}
                  title={`Chamber ${displayChamberNumber(internalId)} — ${disconnected ? "Disconnected" : "Connected"}`}
                  onClick={() => setSelectedBottle(internalId)}
                >
                  {String(displayChamberNumber(internalId)).padStart(2, "0")}
                </button>;
              })}
            </div>
            <div className="presence-legend" aria-hidden="true">
              <span><i /> Connected</span>
              <span><i className="off" /> Disconnected</span>
              <span><i className="sel" /> Selected</span>
            </div>
          </section>

          {presence.latestSample && presence.disconnected.length > 0 && <div className="notice warn" role="status">
            <span className="notice-label">Missing chamber data</span>
            <p>Disconnected at {formatDate(presence.latestSample)}: chambers {presence.disconnected.map((internalId) => `Chamber ${displayChamberNumber(internalId)}`).join(", ")} sent no data.</p>
          </div>}

          <section aria-label="Latest parameter readings">
            <div className="kpis" role="tablist" aria-label="Latest readings and chart parameter" ref={kpiBar.containerRef}>
              <span className={`tab-bar${kpiBar.indicatorClass}`} style={kpiBar.indicatorStyle} aria-hidden="true" />
              {PARAMETER_NAMES.map((name, index) => {
                const value = latestReading ? getParameterValue(latestReading, name) : null;
                const selected = name === chartParameter;
                return <button
                  key={name}
                  className="kpi"
                  role="tab"
                  aria-selected={selected}
                  onClick={() => setChartParameter(name)}
                >
                  <span className="kpi-label"><span>{PARAMETER_DISPLAY[name]}</span>{PARAMETER_UNITS[name] && <span className="unit">{PARAMETER_UNITS[name]}</span>}</span>
                  <span className="kpi-value num">
                    {value === null
                      ? <span className="dash">-</span>
                      : <span className="tick" key={`${selectedBottle}-${formatValue(value, name)}`}>{formatValue(value, name)}</span>}
                  </span>
                  <Sparkline rows={bottleReadings} parameter={name} drawKey={`${selectedBottle}`} delay={index * 60} />
                </button>;
              })}
            </div>
            <p className="status-line">
              <span className={`status-dot ${bottleStateTone === "live" ? "live" : bottleStateTone === "warning" ? "warning" : ""}`} aria-hidden="true" />
              <span>Chamber {displayChamberNumber(selectedBottle)}</span>
              <span className="sep" aria-hidden="true">·</span><span>{bottleState}</span>
              <span className="sep" aria-hidden="true">·</span><span>{bottleReadings.length} samples</span>
              <span className="sep" aria-hidden="true">·</span><span>Latest sample {latestReadingAt}</span>
            </p>
          </section>

          {!loading && !error && bottleReadings.length === 0 && <div className="notice">
            No readings are available for Chamber {displayChamberNumber(selectedBottle)}{presence.latestSample ? "; it did not report at the latest sample" : ""}.
          </div>}

          <section className="panel" aria-label={`${parameterLabel(chartParameter)} chart`}>
            <div className="chart-head">
              <h3>Chamber {displayChamberNumber(selectedBottle)} · {parameterLabel(chartParameter)}</h3>
              <span className="meta">{bottleReadings.length} records</span>
            </div>
            <div className="chart">
              {bottleReadings.length < 2
                ? <div className="empty compact">Not enough data for chart.</div>
                : <Chart rows={bottleReadings} parameter={chartParameter} bottleId={selectedBottle} interactive />}
            </div>
          </section>

          <footer className="page-foot">RuSim · Device telemetry · Data source: Supabase</footer>
          </>}
        </div> : view === "history" ? <ExperimentHistoryView experiments={experiments} loading={loading} error={error} sessionToken={sessionToken} /> : <CompareView experiments={experiments} loading={loading} error={error} sessionToken={sessionToken} bottleCount={bottleCount} />}
        </div>
      </div>
    </main>
  </div>;
}

/**
 * A small trace of one parameter for the selected chamber, drawn under its
 * value in the KPI strip. It uses the same rows as the main chart, so it shows
 * real readings; a missing sample breaks the line just as it does there.
 */
function Sparkline({ rows, parameter, drawKey, delay }: { rows: Reading[]; parameter: ParameterName; drawKey: string; delay: number }) {
  const recent = rows.slice(-40);
  const values = recent.map((row) => getMetricValue(row, parameter));
  const reported = values.filter((value): value is number => value !== null);
  if (reported.length < 2) return <span className="kpi-spark-empty" aria-hidden="true" />;
  const min = Math.min(...reported);
  const max = Math.max(...reported);
  const range = max - min || 1;
  const d = chartPath(values.map((value, index) => ({
    x: (index / (values.length - 1)) * 100,
    y: value === null ? null : 20 - ((value - min) / range) * 18,
  })));
  return <svg className="kpi-spark" viewBox="0 0 100 22" preserveAspectRatio="none" aria-hidden="true">
    <path key={`${drawKey}-${parameter}`} className="draw" pathLength={1} d={d} style={{ "--d": `${delay}ms` } as CSSProperties} />
  </svg>;
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
  const tabBar = useIndicator<HTMLDivElement>(parameter, "bar");

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
      {loading && <div className="loading-line" role="status" aria-label="Loading experiments" />}
      {!loading && experiments.length === 0 && <div className="empty compact">No experiments have readings yet.</div>}
      {!loading && experiments.length > 0 && <>
        <div className="filters">
          <label htmlFor="compare-experiment-select" className="field-label">Experiment</label>
          <select
            id="compare-experiment-select"
            className="input"
            value={draftExperimentId ?? ""}
            onChange={(event) => setDraftExperimentId(Number(event.target.value))}
          >
            {experiments.map((experiment) => <option value={experiment.experiment_id} key={experiment.experiment_id}>{formatExperimentId(experiment.experiment_id)}</option>)}
          </select>
          <label htmlFor="compare-chamber-select" className="field-label">Chamber</label>
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
          ? <p className="panel-text">Chambers can come from different experiments. Up to four series overlay on one chart, aligned by sample order.</p>
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

    <div className="tabs" role="tablist" aria-label="Compare parameter" ref={tabBar.containerRef}>
      <span className={`tab-bar${tabBar.indicatorClass}`} style={tabBar.indicatorStyle} aria-hidden="true" />
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
            ? <div className="loading-line" role="status" aria-label="Loading experiment readings" />
            : !hasChartable
              ? <div className="empty compact">Not enough data for chart.</div>
              : <CompareChart series={series} parameter={parameter} />}
      </div>
      {selections.length > 0 && <p className="panel-note">Series are aligned by sample order (first sample to first sample) with a shared y-axis. Each point shows its timestamp and value.</p>}
    </section>
  </div>;
}

function CompareChart({ series, parameter }: { series: CompareSeries[]; parameter: ParameterName }) {
  const [svgRef, chartWidth] = useMeasuredWidth<SVGSVGElement>();
  const points = useMemo(() => series.map((item) => createChartPoints(item.rows, parameter)), [series, parameter]);
  const yTicks = useMemo(() => createYTicks(series.flatMap((item) => item.rows), parameter), [series, parameter]);
  const labels = useMemo(() => {
    const longest = series.reduce((best, item) => (item.rows.length > best.rows.length ? item : best), series[0] ?? { rows: [] as Reading[] });
    return createTimeLabels(longest.rows, chartWidth < 480 ? 3 : 5);
  }, [series, chartWidth]);
  const [activePoint, setActivePoint] = useState<CompareActivePoint | null>(null);

  useEffect(() => {
    setActivePoint(null);
  }, [series, parameter]);

  const activeReading = activePoint === null ? undefined : series[activePoint.seriesIndex]?.rows[activePoint.index];
  const activeDetails = activeReading === undefined ? undefined : getHistoryPointDetails(activeReading, parameter);
  const activeLabel = activePoint === null ? undefined : series[activePoint.seriesIndex]?.label;

  const geometry = chartGeometry(chartWidth);
  const { plotLeft, plotRight, plotTop, plotBottom, height: chartHeight } = geometry;
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
    <svg ref={svgRef} viewBox={`0 0 ${chartWidth} ${chartHeight}`} role="img" aria-label={`${parameterLabel(parameter)} comparison chart`}>
      {yTicks.map((tick) => <g key={tick}>
        <line x1={plotLeft} x2={plotRight} y1={toY(tick)} y2={toY(tick)} stroke="var(--grid)" strokeWidth="1" />
        <text x={plotLeft - 8} y={toY(tick) + 4} textAnchor="end" fontSize="11" fill="var(--axis-ink)">{formatTick(tick, parameter)}</text>
      </g>)}
      <line x1={plotLeft} x2={plotRight} y1={plotBottom} y2={plotBottom} stroke="var(--border)" strokeWidth="1" />
      {activePointCoords && <line className="chart-guide" x1={activePointCoords.x} x2={activePointCoords.x} y1={plotTop} y2={plotBottom} />}
      {series.map((item, seriesIndex) => <path
        key={`${item.label}-${parameter}-${item.rows.length > 0}`}
        className="chart-line draw"
        pathLength={1}
        style={{ "--d": `${seriesIndex * 90}ms` } as CSSProperties}
        d={chartPath((points[seriesIndex] ?? []).map((point, index) => ({ x: toX(index, item.rows.length), y: point.y === null ? null : toY(point.value) })))}
        stroke={item.color}
      />)}
      <g className="chart-points draw" key={`points-${parameter}-${series.length}`}>
      {series.map((item, seriesIndex) => (points[seriesIndex] ?? []).map((point, index) => {
        const reading = item.rows[index];
        if (reading === undefined) return null;
        const selected = activePoint?.seriesIndex === seriesIndex && activePoint?.index === index;
        return <circle
          key={`${item.label}-${reading.timestamp}-${index}`}
          className={`chart-point${selected ? " is-active" : ""}`}
          cx={toX(index, item.rows.length)}
          cy={point.y === null ? (plotTop + plotBottom) / 2 : toY(point.value)}
          r={3}
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
      </g>
      {labels.map((label, index) => {
        const x = labels.length <= 1 ? (plotLeft + plotRight) / 2 : plotLeft + (index / (labels.length - 1 || 1)) * plotWidth;
        const anchor = index === 0 ? "start" : index === labels.length - 1 ? "end" : "middle";
        return <text key={`${label}-${index}`} x={x} y={geometry.labelY} textAnchor={anchor} fontSize="11" fill="var(--axis-ink)">{label}</text>;
      })}
    </svg>
    {activeDetails && activeLabel && activePointCoords && <div
      className="chart-tooltip"
      role="status"
      style={{
        left: `clamp(90px, ${(activePointCoords.x / chartWidth) * 100}%, calc(100% - 90px))`,
        top: `${(activePointCoords.y / chartHeight) * 100}%`,
        transform: activePointCoords.y < 80 ? "translate(-50%, 8px)" : "translate(-50%, calc(-100% - 8px))",
      }}
    >
      <div className="chart-tooltip-inner">
        <span className="tip-label"><span className="compare-dot" style={{ background: series[activePoint?.seriesIndex ?? 0]?.color }} aria-hidden="true" />{activeLabel}</span>
        <span className="tip-value">{formatTooltipValue(activeDetails.value, activeDetails.parameter)}</span>
        <span className="tip-time">{formatDateTime(activeDetails.timestamp)}</span>
      </div>
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
  const tabBar = useIndicator<HTMLDivElement>(historyPanel, "bar");

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
    const timer = startFetchTimer("fetchExperimentReadings");
    void fetchExperimentReadings(sessionToken, activeExperimentId)
      .then((result) => {
        if (cancelled) return;
        setReadings(result);
        timer({ rows: result.length, experiment_id: activeExperimentId, newest_age_s: newestAgeSeconds(result) });
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
      {loading && <div className="loading-line" role="status" aria-label="Loading experiments" />}
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
            <span className="row-date">Last sample {formatDate(experiment.last_timestamp)}</span>
          </span>
          <ChevronIcon />
        </button>)}
      </div>}
      {experiments.length > 6 && <div className="more-row">
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
        <p className="panel-text">{selectedExperiment.reading_count} chamber readings, sampled between {formatDate(selectedExperiment.first_timestamp)} and {formatDate(selectedExperiment.last_timestamp)}.</p>
        <p className="panel-text">{selectedExperiment.concluded ? "No recent samples, so this experiment is treated as concluded." : "Recent samples are present, so this experiment is treated as live."}</p>
      </section>

      <div className="tabs" role="tablist" aria-label="Experiment detail" ref={tabBar.containerRef}>
        <span className={`tab-bar${tabBar.indicatorClass}`} style={tabBar.indicatorStyle} aria-hidden="true" />
        <button role="tab" aria-selected={historyPanel === "readings"} onClick={() => setHistoryPanel("readings")}>Chamber readings</button>
        <button role="tab" aria-selected={historyPanel === "results"} onClick={() => setHistoryPanel("results")}>Results</button>
      </div>

      {detailError && <div className="error-block" role="alert"><span>Unable to load experiment readings. {detailError}</span></div>}
      {detailLoading && !detailError && <div className="loading-line" role="status" aria-label="Loading experiment readings" />}
      {!detailLoading && !detailError && experimentReadings.length === 0 && <div className="empty compact">No readings are available for this experiment.</div>}
      {isTruncated(experimentReadings.length) && <div className="notice warn">
        Only the first {MAX_EXPERIMENT_ROWS.toLocaleString()} readings were loaded, so the end of this experiment is missing on screen and any CSV export is truncated too.
      </div>}

      {experimentReadings.length > 0 && historyPanel === "readings" && <>
        <div className="filters">
          <label htmlFor="history-chamber-select" className="field-label">Chamber</label>
          <select
            id="history-chamber-select"
            className="input"
            value={selectedChamberId ?? ""}
            onChange={(event) => setSelectedChamberId(Number(event.target.value))}
          >
            {availableChambers.map((internalId) => <option value={internalId} key={internalId}>Chamber {displayChamberNumber(internalId)}</option>)}
          </select>
          <span className="spacer" />
          <span className="meta">{selectedChamberReadings.length} readings</span>
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
            <td className="chamber-cell">Chamber {displayChamberNumber(bottle_id)}</td>
            <td className="n num">{readings.length}</td>
            {PARAMETER_NAMES.map((name) => {
              const parameterStats = stats[name];
              return <td key={name} className="n num">
                {parameterStats
                  ? <>{formatValue(parameterStats.average, name)}<span className="sub">{formatValue(parameterStats.min, name)} – {formatValue(parameterStats.max, name)}</span></>
                  : <span className="meta">-</span>}
              </td>;
            })}
          </tr>)}
        </tbody>
      </table>
    </div>
    <p className="panel-note">Mean with min – max below. A dash means no bottle reported that sensor.</p>
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
  return rowCount >= MAX_EXPERIMENT_ROWS;
}

type ChartInteraction = { index: number; pinned: boolean };

function Chart({ rows, parameter, bottleId, interactive = false }: { rows: Reading[]; parameter: ParameterName; bottleId?: number | null; interactive?: boolean }) {
  // The line draws again only when the chamber or parameter changes, not on
  // every poll, so a refresh extends the line quietly.
  const drawKey = `${parameter}-${bottleId ?? "none"}`;
  const gradientId = `area-${useId().replace(/:/g, "")}`;
  const points = useMemo(() => createChartPoints(rows, parameter), [rows, parameter]);
  const [svgRef, chartWidth] = useMeasuredWidth<SVGSVGElement>();
  const labels = useMemo(() => createTimeLabels(rows, chartWidth < 480 ? 3 : 5), [rows, chartWidth]);
  const yTicks = useMemo(() => createYTicks(rows, parameter), [rows, parameter]);
  const [activePoint, setActivePoint] = useState<ChartInteraction | null>(null);

  useEffect(() => {
    setActivePoint(null);
  }, [rows, parameter, bottleId]);

  const activeReading = activePoint === null ? undefined : rows[activePoint.index];
  const activeDetails = activeReading === undefined
    ? undefined
    : getHistoryPointDetails(activeReading, parameter);

  const geometry = chartGeometry(chartWidth);
  const { plotLeft, plotRight, plotTop, plotBottom, height: chartHeight } = geometry;
  const plotWidth = plotRight - plotLeft;
  const plotHeight = plotBottom - plotTop;
  const yMin = yTicks[0] ?? 0;
  const yMax = yTicks[yTicks.length - 1] ?? 1;
  const yRange = yMax - yMin || 1;
  const toY = (value: number) => plotBottom - ((value - yMin) / yRange) * plotHeight;
  const toX = (index: number) => rows.length <= 1 ? (plotLeft + plotRight) / 2 : plotLeft + (index / (rows.length - 1)) * plotWidth;
  const plotted = points.map((point, index) => ({ x: toX(index), y: point.y === null ? null : toY(point.value) }));
  const activePointCoords = activePoint === null ? null : (() => {
    const point = points[activePoint.index];
    return {
      x: toX(activePoint.index),
      y: point?.y === null || point === undefined ? (plotTop + plotBottom) / 2 : toY(point.value),
    };
  })();

  return <>
    <svg ref={svgRef} viewBox={`0 0 ${chartWidth} ${chartHeight}`} role="img" aria-label={`${parameterLabel(parameter)} reading chart`}>
      {yTicks.map((tick) => <g key={tick}>
        <line x1={plotLeft} x2={plotRight} y1={toY(tick)} y2={toY(tick)} stroke="var(--grid)" strokeWidth="1" />
        <text x={plotLeft - 8} y={toY(tick) + 4} textAnchor="end" fontSize="11" fill="var(--axis-ink)">{formatTick(tick, parameter)}</text>
      </g>)}
      <line x1={plotLeft} x2={plotRight} y1={plotBottom} y2={plotBottom} stroke="var(--border)" strokeWidth="1" />
      <defs>
        <linearGradient id={gradientId} x1="0" x2="0" y1="0" y2="1">
          <stop offset="0%" stopColor="var(--chart-1)" stopOpacity="0.14" />
          <stop offset="100%" stopColor="var(--chart-1)" stopOpacity="0" />
        </linearGradient>
      </defs>
      <path key={`area-${drawKey}`} className="chart-area draw" d={chartAreaPath(plotted, plotBottom)} fill={`url(#${gradientId})`} />
      {activePointCoords && <line className="chart-guide" x1={activePointCoords.x} x2={activePointCoords.x} y1={plotTop} y2={plotBottom} />}
      <path key={`line-${drawKey}`} className="chart-line draw" pathLength={1} d={chartPath(plotted)} stroke="var(--chart-1)" />
      {interactive && <g className="chart-points draw" key={`points-${drawKey}`}>{points.map((point, index) => {
        const reading = rows[index];
        if (reading === undefined) return null;
        const selected = activePoint?.index === index;
        return <circle
          key={`${reading.timestamp}-${index}`}
          className={`chart-point${selected ? " is-active" : ""}`}
          cx={toX(index)}
          cy={point.y === null ? (plotTop + plotBottom) / 2 : toY(point.value)}
          r={3}
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
      })}</g>}
      {labels.map((label, index) => {
        const x = rows.length <= 1 ? (plotLeft + plotRight) / 2 : plotLeft + (index / (labels.length - 1 || 1)) * plotWidth;
        const anchor = index === 0 ? "start" : index === labels.length - 1 ? "end" : "middle";
        return <text key={`${label}-${index}`} x={x} y={geometry.labelY} textAnchor={anchor} fontSize="11" fill="var(--axis-ink)">{label}</text>;
      })}
    </svg>
    {interactive && activeDetails && activePointCoords && <div
      className="chart-tooltip"
      role="status"
      style={{
        left: `clamp(90px, ${(activePointCoords.x / chartWidth) * 100}%, calc(100% - 90px))`,
        top: `${(activePointCoords.y / chartHeight) * 100}%`,
        transform: activePointCoords.y < 80 ? "translate(-50%, 8px)" : "translate(-50%, calc(-100% - 8px))",
      }}
    >
      <div className="chart-tooltip-inner">
        <span className="tip-value">{formatTooltipValue(activeDetails.value, activeDetails.parameter)}</span>
        <span className="tip-time">{formatDateTime(activeDetails.timestamp)}</span>
      </div>
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
  return unit ? `${PARAMETER_DISPLAY[name]} (${unit})` : PARAMETER_DISPLAY[name];
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

/**
 * Tracks an element's rendered width. The charts use it as their viewBox
 * width, so one SVG unit is one screen pixel and axis text stays 11px on a
 * phone instead of shrinking with the drawing.
 */
function useMeasuredWidth<T extends Element>(fallback = 800) {
  const [element, ref] = useState<T | null>(null);
  const [width, setWidth] = useState(fallback);
  useLayoutEffect(() => {
    if (!element) return;
    const update = () => {
      const next = Math.round(element.getBoundingClientRect().width);
      if (next > 0) setWidth(next);
    };
    update();
    const observer = new ResizeObserver(update);
    observer.observe(element);
    return () => observer.disconnect();
  }, [element]);
  return [ref, width] as const;
}

/** Plot margins for a chart of the given pixel width; phones get a shorter plot. */
function chartGeometry(width: number) {
  const height = width < 560 ? 200 : 260;
  return { height, plotLeft: 56, plotRight: width - 8, plotTop: 10, plotBottom: height - 30, labelY: height - 10 };
}

/**
 * Builds the shaded area under a line: one closed shape per unbroken run of
 * samples, so a gap in the data leaves a gap in the shading too.
 */
function chartAreaPath(points: Array<{ x: number; y: number | null }>, baseline: number): string {
  const shapes: string[] = [];
  let run: Array<{ x: number; y: number }> = [];
  const flush = () => {
    if (run.length >= 2) {
      const first = run[0]!;
      const last = run[run.length - 1]!;
      shapes.push(`M${first.x} ${baseline} ${run.map((point) => `L${point.x} ${point.y}`).join(" ")} L${last.x} ${baseline} Z`);
    }
    run = [];
  };
  for (const point of points) {
    if (point.y === null) flush();
    else run.push({ x: point.x, y: point.y });
  }
  flush();
  return shapes.join(" ");
}

function createTimeLabels(rows: Reading[], maxLabels = 5): string[] {
  if (rows.length === 0) return [];
  const count = Math.min(maxLabels, rows.length);
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
