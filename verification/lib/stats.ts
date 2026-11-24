/** Descriptive statistics used by every report table. */

export type Summary = {
  n: number;
  min: number | null;
  mean: number | null;
  sd: number | null;
  p50: number | null;
  p95: number | null;
  p99: number | null;
  max: number | null;
  /** Count of values strictly above `limit` (null when no limit was given). */
  over: number | null;
  limit: number | null;
};

/** Nearest-rank percentile on a sorted copy (no interpolation, so p100 = max). */
export function percentile(sorted: number[], p: number): number | null {
  if (sorted.length === 0) return null;
  const rank = Math.ceil((p / 100) * sorted.length);
  return sorted[Math.min(sorted.length, Math.max(1, rank)) - 1]!;
}

export function summarize(values: number[], limit: number | null = null): Summary {
  const v = values.filter((x) => Number.isFinite(x)).sort((a, b) => a - b);
  const n = v.length;
  if (n === 0) return { n, min: null, mean: null, sd: null, p50: null, p95: null, p99: null, max: null, over: limit === null ? null : 0, limit };
  const mean = v.reduce((a, b) => a + b, 0) / n;
  const sd = n > 1 ? Math.sqrt(v.reduce((a, b) => a + (b - mean) ** 2, 0) / (n - 1)) : 0;
  return {
    n,
    min: v[0]!,
    mean,
    sd,
    p50: percentile(v, 50),
    p95: percentile(v, 95),
    p99: percentile(v, 99),
    max: v[n - 1]!,
    over: limit === null ? null : v.filter((x) => x > limit).length,
    limit,
  };
}

/** Least-squares slope of y over x (units of y per unit of x); null below 2 points. */
export function slope(xs: number[], ys: number[]): number | null {
  const n = Math.min(xs.length, ys.length);
  if (n < 2) return null;
  const mx = xs.slice(0, n).reduce((a, b) => a + b, 0) / n;
  const my = ys.slice(0, n).reduce((a, b) => a + b, 0) / n;
  let num = 0;
  let den = 0;
  for (let i = 0; i < n; i++) {
    num += (xs[i]! - mx) * (ys[i]! - my);
    den += (xs[i]! - mx) ** 2;
  }
  return den === 0 ? null : num / den;
}

const fmt = (x: number | null, digits: number) => (x === null ? "-" : x.toFixed(digits));

export const SUMMARY_HEADER = "| Metric | n | min | mean | p50 | p95 | p99 | max | sd | over limit |\n|---|---|---|---|---|---|---|---|---|---|";

export function summaryRow(label: string, s: Summary, digits = 1): string {
  const over = s.over === null ? "-" : `${s.over} (> ${s.limit})`;
  return `| ${label} | ${s.n} | ${fmt(s.min, digits)} | ${fmt(s.mean, digits)} | ${fmt(s.p50, digits)} | ${fmt(s.p95, digits)} | ${fmt(s.p99, digits)} | ${fmt(s.max, digits)} | ${fmt(s.sd, digits)} | ${over} |`;
}

/** Minimal CSV writer: quotes only when needed. */
export function toCsv(header: string[], rows: (string | number | null | undefined)[][]): string {
  const cell = (v: string | number | null | undefined) => {
    if (v === null || v === undefined) return "";
    const s = String(v);
    return /[",\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
  };
  return [header.join(","), ...rows.map((r) => r.map(cell).join(","))].join("\n") + "\n";
}

export function parseArgs(argv: string[]): { flags: Record<string, string[]>; positional: string[] } {
  const flags: Record<string, string[]> = {};
  const positional: string[] = [];
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i]!;
    if (!a.startsWith("--")) {
      positional.push(a);
      continue;
    }
    const eq = a.indexOf("=");
    let key: string;
    let val: string;
    if (eq > 0) {
      key = a.slice(2, eq);
      val = a.slice(eq + 1);
    } else {
      key = a.slice(2);
      const next = argv[i + 1];
      if (next !== undefined && !next.startsWith("--")) {
        val = next;
        i++;
      } else {
        val = "true";
      }
    }
    (flags[key] ??= []).push(val);
  }
  return { flags, positional };
}

/** "90", "90s", "15m", "24h" -> milliseconds. */
export function parseDuration(s: string): number {
  const m = /^(\d+(?:\.\d+)?)(ms|s|m|h)?$/.exec(s.trim());
  if (!m) throw new Error(`Bad duration: ${s}`);
  const n = Number(m[1]);
  const unit = m[2] ?? "s";
  return n * (unit === "ms" ? 1 : unit === "s" ? 1000 : unit === "m" ? 60000 : 3600000);
}
