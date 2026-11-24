#!/usr/bin/env bun
/**
 * Concurrent-viewer load test (report checklist §6 "Concurrent viewers").
 *
 * Each virtual viewer signs in once, like the dashboard, then repeats the
 * dashboard's reads: the device row, the newest readings in 1000-row pages up
 * to 5000 rows (fetchReadings), and one experiment by keyset pages
 * (fetchExperimentReadings), with a think time between repeats.
 *
 *   export RUSIM_DEVICE_TOKEN=<device token>
 *   bun verification/dashboard-load.ts --viewers 20 --duration 5m --exp 4
 *
 * Read-only apart from the sessions it creates; every session is revoked at
 * the end. SUPABASE_URL / SUPABASE_ANON_KEY default to the public firmware
 * values. Output: <out>/dashboard-load.md and requests.csv.
 */
import { mkdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { parseArgs, parseDuration, summarize, summaryRow, SUMMARY_HEADER, toCsv } from "./lib/stats";

const REPO = resolve(import.meta.dir, "..");
const { flags } = parseArgs(process.argv.slice(2));
const one = (k: string, d: string) => flags[k]?.at(-1) ?? d;
const URL_ = process.env.SUPABASE_URL ?? "https://mdjpqjkduyexrlimepmb.supabase.co";
const KEY = process.env.SUPABASE_ANON_KEY ?? "sb_publishable_oBhy37ygaFxOOhO69c-lww_5R8FcEwB";
const TOKEN = process.env.RUSIM_DEVICE_TOKEN ?? "";
const viewers = Number(one("viewers", "20"));
const durationMs = parseDuration(one("duration", "5m"));
const thinkMs = parseDuration(one("think", "10s"));
const exp = one("exp", "");
if (!TOKEN) {
  console.log("Usage: RUSIM_DEVICE_TOKEN=... bun verification/dashboard-load.ts [--viewers 20] [--duration 5m] [--think 10s] [--exp N]");
  process.exit(1);
}
const COLS = "device_id,bottle_id,experiment_id,timestamp,ph,pressure,temp,co2,ch4";

type Req = { viewer: number; kind: string; status: number; ms: number; rows: number; at: number };
const reqs: Req[] = [];
async function get(viewer: number, kind: string, path: string, session: string): Promise<any[]> {
  const t = performance.now();
  const r = await fetch(`${URL_}${path}`, { headers: { apikey: KEY, Accept: "application/json", "Accept-Profile": "device_data", "x-device-session": session } });
  const body = await r.text();
  let rows: any[] = [];
  try {
    const j = JSON.parse(body);
    if (Array.isArray(j)) rows = j;
  } catch {}
  reqs.push({ viewer, kind, status: r.status, ms: performance.now() - t, rows: rows.length, at: Date.now() });
  return rows;
}

async function viewer(id: number, end: number): Promise<void> {
  const t = performance.now();
  const a = await fetch(`${URL_}/rest/v1/rpc/authenticate`, {
    method: "POST",
    headers: { apikey: KEY, "Content-Type": "application/json", "Content-Profile": "device_data", Accept: "application/json" },
    body: JSON.stringify({ p_token: TOKEN }),
  });
  const aj: any = await a.json().catch(() => null);
  reqs.push({ viewer: id, kind: "authenticate", status: a.status, ms: performance.now() - t, rows: 0, at: Date.now() });
  const session: string | undefined = aj?.[0]?.session_token;
  if (!session) return;
  while (Date.now() < end) {
    await get(id, "device", "/rest/v1/devices?select=device_id,bottle_count,owner_name,owner_email", session);
    for (let off = 0; off < 5000; off += 1000) {
      const page = await get(id, "readings_page", `/rest/v1/readings?select=${COLS}&order=timestamp.desc&offset=${off}&limit=1000`, session);
      if (page.length < 1000) break;
    }
    if (exp !== "") {
      let cursor: { timestamp: string; bottle_id: number } | null = null;
      for (let i = 0; i < 50; i++) {
        const filter = cursor ? `&or=(timestamp.gt."${encodeURIComponent(cursor.timestamp)}",and(timestamp.eq."${encodeURIComponent(cursor.timestamp)}",bottle_id.gt.${cursor.bottle_id}))` : "";
        const page = await get(id, "experiment_page", `/rest/v1/readings?select=${COLS}&experiment_id=eq.${exp}${filter}&order=timestamp.asc,bottle_id.asc&limit=1000`, session);
        if (page.length < 1000) break;
        const last = page[page.length - 1];
        cursor = { timestamp: last.timestamp, bottle_id: last.bottle_id };
      }
    }
    await Bun.sleep(thinkMs * (0.5 + Math.random()));
  }
  await fetch(`${URL_}/rest/v1/rpc/revoke_session`, {
    method: "POST",
    headers: { apikey: KEY, "Content-Type": "application/json", "Content-Profile": "device_data", "x-device-session": session },
    body: "{}",
  });
}

const start = Date.now();
console.log(`${viewers} viewers for ${durationMs / 1000} s against ${URL_} ...`);
await Promise.all(Array.from({ length: viewers }, (_, i) => Bun.sleep(i * 200).then(() => viewer(i, start + durationMs))));
const kinds = [...new Set(reqs.map((r) => r.kind))];
const md = [
  `# Dashboard read load: ${viewers} viewers, ${durationMs / 1000} s, think ${thinkMs / 1000} s`,
  "",
  `Requests ${reqs.length}; errors (status >= 400 or network) ${reqs.filter((r) => r.status >= 400 || r.status === 0).length}.`,
  "",
  SUMMARY_HEADER,
  ...kinds.map((k) => summaryRow(`${k}, ms`, summarize(reqs.filter((r) => r.kind === k && r.status < 400).map((r) => r.ms)), 0)),
  "",
  `Status codes: ${JSON.stringify(reqs.reduce<Record<number, number>>((a, r) => ((a[r.status] = (a[r.status] ?? 0) + 1), a), {}))}`,
  "",
];
const stamp = new Date().toISOString().replace(/[:]/g, "").replace(/\..*/, "Z");
const outDir = resolve(one("out", join(REPO, "verification", "runs", `${stamp}-dashboard-load`)));
mkdirSync(outDir, { recursive: true });
writeFileSync(join(outDir, "dashboard-load.md"), md.join("\n") + "\n");
writeFileSync(join(outDir, "requests.csv"), toCsv(["viewer", "kind", "status", "ms", "rows", "at"], reqs.map((r) => [r.viewer, r.kind, r.status, r.ms.toFixed(1), r.rows, r.at])));
console.log(md.join("\n"));
