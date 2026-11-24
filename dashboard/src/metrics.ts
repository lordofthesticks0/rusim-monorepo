/**
 * Opt-in timing records for the verification report (docs/verification-toolkit.md §6).
 *
 * Off unless the page is opened with `?metrics` in the URL, so normal visitors
 * pay nothing. When on, each record is printed to the console as
 * `[metrics] {...}` and kept in `window.__rusimMetrics`; run
 * `copy(window.__rusimMetricsCsv())` in the browser console to copy them as CSV.
 * Each record is also POSTed to the local receiver `verification/dashboard-log.ts`
 * (127.0.0.1:8787, or the port in `?metrics=<port>`); when it is not running
 * the request fails silently and nothing else changes.
 *
 * Recorded: page navigation timing, every readings fetch (duration, rows,
 * age of the newest sample), the time from data arrival to the next painted
 * frame (chart render), and the JS heap where the browser exposes it.
 */

type MetricRecord = { kind: string; at: string } & Record<string, string | number | null>;

declare global {
  interface Window {
    __rusimMetrics?: MetricRecord[];
    __rusimMetricsCsv?: () => string;
  }
}

export const metricsEnabled =
  typeof window !== "undefined" && new URLSearchParams(window.location.search).has("metrics");

/** Port of the local receiver: `?metrics=<port>` or 8787. */
function sinkUrl(): string {
  const value = new URLSearchParams(window.location.search).get("metrics");
  const port = value && /^\d{2,5}$/.test(value) ? value : "8787";
  return `http://127.0.0.1:${port}/`;
}

/** Best-effort copy of a record to verification/dashboard-log.ts. Never throws. */
function sendToSink(record: MetricRecord): void {
  try {
    // text/plain + no-cors is a "simple" request: no preflight, and a refused
    // connection (receiver not running) only rejects the promise.
    void fetch(sinkUrl(), {
      method: "POST",
      mode: "no-cors",
      keepalive: true,
      headers: { "Content-Type": "text/plain" },
      body: JSON.stringify(record),
    }).catch(() => {});
  } catch {
    // logging must never affect the page
  }
}

function heapBytes(): number | null {
  const memory = (performance as Performance & { memory?: { usedJSHeapSize: number } }).memory;
  return memory ? memory.usedJSHeapSize : null;
}

export function recordMetric(kind: string, fields: Record<string, string | number | null> = {}): void {
  if (!metricsEnabled) return;
  const record: MetricRecord = { kind, at: new Date().toISOString(), heap_bytes: heapBytes(), ...fields };
  window.__rusimMetrics ??= [];
  window.__rusimMetrics.push(record);
  console.info("[metrics]", JSON.stringify(record));
  sendToSink(record);
}

/** Seconds between now and the newest timestamp in `rows`, or null when empty. */
export function newestAgeSeconds(rows: ReadonlyArray<{ timestamp: string }>): number | null {
  let newest = Number.NEGATIVE_INFINITY;
  for (const row of rows) newest = Math.max(newest, Date.parse(row.timestamp));
  return Number.isFinite(newest) ? Math.round((Date.now() - newest) / 100) / 10 : null;
}

/**
 * Starts a timer for one fetch. Call the returned function when the data has
 * been handed to React: it records the fetch time, then waits two animation
 * frames (React commit + browser paint) and records the render time.
 */
export function startFetchTimer(name: string): (fields: Record<string, string | number | null>) => void {
  if (!metricsEnabled) return () => {};
  const t0 = performance.now();
  return (fields) => {
    const fetched = performance.now();
    recordMetric("fetch", { name, fetch_ms: Math.round(fetched - t0), ...fields });
    requestAnimationFrame(() =>
      requestAnimationFrame(() => recordMetric("render", { name, render_ms: Math.round(performance.now() - fetched), ...fields })),
    );
  };
}

if (metricsEnabled) {
  window.__rusimMetricsCsv = () => {
    const rows = window.__rusimMetrics ?? [];
    const keys = [...new Set(rows.flatMap((r) => Object.keys(r)))];
    const cell = (v: unknown) => (v === null || v === undefined ? "" : /[",\n]/.test(String(v)) ? `"${String(v).replace(/"/g, '""')}"` : String(v));
    return [keys.join(","), ...rows.map((r) => keys.map((k) => cell(r[k])).join(","))].join("\n");
  };
  window.addEventListener("load", () => {
    setTimeout(() => {
      const nav = performance.getEntriesByType("navigation")[0] as PerformanceNavigationTiming | undefined;
      recordMetric("page_load", {
        dom_content_loaded_ms: nav ? Math.round(nav.domContentLoadedEventEnd) : null,
        load_ms: nav ? Math.round(nav.loadEventEnd) : null,
        transfer_bytes: nav ? nav.transferSize : null,
        user_agent: navigator.userAgent,
        viewport: `${window.innerWidth}x${window.innerHeight}`,
      });
    }, 0);
  });
}
