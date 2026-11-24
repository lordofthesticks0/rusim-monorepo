#!/usr/bin/env bun
/**
 * Live contract test of push_readings() through PostgREST, the same path the
 * WT32 uses (report checklist §5 "Data integrity" and "Validation rejects").
 *
 *   export RUSIM_DEVICE_TOKEN=<device token>      # never committed, never logged
 *   bun verification/db-replay.ts --exp 0 --yes [--latency-n 20]
 *
 * WRITES test rows into experiment --exp of the token's device, timestamped
 * about one hour in the past. The default is experiment 0, the reserved
 * testing experiment. Do not use a large unused id: the WT32 and the display
 * take the highest experiment id with readings as their "next experiment"
 * hint, so a test id of 9000 would make the display suggest 9001. The API
 * cannot delete rows; remove them afterwards with SQL if wanted:
 *   delete from device_data.readings where device_id = <id> and experiment_id = 0
 *     and "timestamp" between '<start>' and '<end>';
 *
 * Cases: first insert, exact replay (0 inserted), same key with different
 * values (0 inserted, stored values unchanged = first write wins), mixed
 * old/new keys, one bad row rolls back the batch, timestamp before 2025,
 * timestamp > 10 min ahead, non-array body, 25 rows, forged device_id,
 * wrong device token, garbage session. Then --latency-n timed inserts.
 *
 * SUPABASE_URL / SUPABASE_ANON_KEY default to the public values compiled into
 * the firmware (firmware/network-master/main.cpp).
 */
import { mkdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { parseArgs, summarize, summaryRow, SUMMARY_HEADER } from "./lib/stats";

const REPO = resolve(import.meta.dir, "..");
const { flags } = parseArgs(process.argv.slice(2));
const one = (k: string, d: string) => flags[k]?.at(-1) ?? d;
const URL_ = process.env.SUPABASE_URL ?? "https://mdjpqjkduyexrlimepmb.supabase.co";
const KEY = process.env.SUPABASE_ANON_KEY ?? "sb_publishable_oBhy37ygaFxOOhO69c-lww_5R8FcEwB";
const TOKEN = process.env.RUSIM_DEVICE_TOKEN ?? "";
const exp = Number(one("exp", "0"));
const latencyN = Number(one("latency-n", "20"));
if (!TOKEN || !flags.yes) {
  console.log("Usage: RUSIM_DEVICE_TOKEN=... bun verification/db-replay.ts --exp 0 --yes [--latency-n 20]\nThis writes test rows; --yes confirms.");
  process.exit(1);
}

async function call(path: string, body: unknown, session?: string, method = "POST"): Promise<{ status: number; json: any; ms: number }> {
  const headers: Record<string, string> = { apikey: KEY, Accept: "application/json" };
  if (method === "POST") {
    headers["Content-Type"] = "application/json";
    headers["Content-Profile"] = "device_data";
  } else headers["Accept-Profile"] = "device_data";
  if (session) headers["x-device-session"] = session;
  const t = performance.now();
  const r = await fetch(`${URL_}${path}`, { method, headers, body: method === "POST" ? JSON.stringify(body) : undefined });
  const ms = performance.now() - t;
  const text = await r.text();
  let json: any = text;
  try {
    json = JSON.parse(text);
  } catch {}
  return { status: r.status, json, ms };
}

const results: { name: string; expect: string; got: string; pass: boolean }[] = [];
const check = (name: string, expect: string, got: string, pass: boolean) => {
  results.push({ name, expect, got, pass });
  console.log(`${pass ? "PASS" : "FAIL"}  ${name}: expected ${expect}, got ${got}`);
};
const code = (r: { status: number; json: any }) => `${r.status}${r.json?.code ? " " + r.json.code : ""}`;

const auth = await call("/rest/v1/rpc/authenticate", { p_token: TOKEN });
if (auth.status !== 200 || !Array.isArray(auth.json) || !auth.json[0]) {
  console.error(`authenticate failed: HTTP ${auth.status}`);
  process.exit(1);
}
const session: string = auth.json[0].session_token;
const device: number = auth.json[0].device_id;
const bottles = Math.min(4, auth.json[0].bottle_count as number);
console.log(`authenticated device=${device} bottle_count=${auth.json[0].bottle_count} in ${auth.ms.toFixed(0)} ms (session not printed)`);

const base = Math.floor(Date.now() / 1000) - 3600; // an hour ago: inside the accepted window, clear of live data
const iso = (s: number) => new Date(s * 1000).toISOString();
const round = (ts: number, v: number, n = bottles) =>
  Array.from({ length: n }, (_, b) => ({ bottle_id: b, experiment_id: exp, timestamp: iso(ts), ph: 6 + v, temp: 38 + v, co2: 1000 + v, ch4: 500 + v, pressure: 101000 + v }));
const push = (rows: unknown, s = session) => call("/rest/v1/rpc/push_readings", { p_readings: rows }, s);
const stored = async (ts: number) =>
  (await call(`/rest/v1/readings?select=bottle_id,device_id,ph&experiment_id=eq.${exp}&timestamp=eq.${encodeURIComponent(iso(ts))}&order=bottle_id`, null, session, "GET")).json as any[];

let r = await push(round(base, 0.1));
check("first insert", `200 ${bottles}`, `${code(r)} ${r.json}`, r.status === 200 && r.json === bottles);
r = await push(round(base, 0.1));
check("exact replay", "200 0", `${code(r)} ${r.json}`, r.status === 200 && r.json === 0);
r = await push(round(base, 0.9));
const after = await stored(base);
check("same key, different values (first write wins)", "200 0, ph stays 6.1", `${code(r)} ${r.json}, ph=${after.map((x) => x.ph).join("/")}`, r.status === 200 && r.json === 0 && after.every((x) => Math.abs(x.ph - 6.1) < 1e-4));
r = await push([...round(base, 0.1), ...round(base + 1, 0.2)]);
check("mixed old + new keys", `200 ${bottles}`, `${code(r)} ${r.json}`, r.status === 200 && r.json === bottles);
r = await push([...round(base + 2, 0.3, 1), { bottle_id: auth.json[0].bottle_count, experiment_id: exp, timestamp: iso(base + 2), ph: 7 }]);
const rolled = await stored(base + 2);
check("one bad row rolls back the batch", "4xx 22023, 0 rows stored", `${code(r)}, ${rolled.length} rows stored`, r.status >= 400 && r.json?.code === "22023" && rolled.length === 0);
r = await push([{ ...round(base, 0, 1)[0], timestamp: "2024-12-31T23:59:59Z" }]);
check("timestamp before 2025-01-01", "4xx 22023", code(r), r.status >= 400 && r.json?.code === "22023");
r = await push([{ ...round(base, 0, 1)[0], timestamp: iso(Math.floor(Date.now() / 1000) + 1200) }]);
check("timestamp 20 min ahead", "4xx 22023", code(r), r.status >= 400 && r.json?.code === "22023");
r = await call("/rest/v1/rpc/push_readings", { p_readings: { not: "an array" } }, session);
check("non-array body", "4xx 22023", code(r), r.status >= 400 && r.json?.code === "22023");
r = await push(Array.from({ length: 25 }, (_, i) => ({ ...round(base + 10 + i, 0, 1)[0] })));
check("25 rows in one call", "4xx 22023", code(r), r.status >= 400 && r.json?.code === "22023");
r = await push(round(base + 3, 0.4, 1).map((x) => ({ ...x, device_id: device + 1 })));
const forged = await stored(base + 3);
check("forged device_id ignored", `row stored under device ${device}`, `${code(r)}, device=${forged.map((x) => x.device_id).join("/")}`, r.status === 200 && forged.length === 1 && forged[0].device_id === device);
r = await call("/rest/v1/rpc/authenticate", { p_token: "not-a-real-token" });
check("wrong device token", "4xx, generic error", `${code(r)} ${JSON.stringify(r.json?.message ?? "")}`, r.status >= 400);
r = await push(round(base + 4, 0.5, 1), "0".repeat(64));
check("garbage session", "4xx 28000 (no rows)", code(r), r.status >= 400);

const lat: number[] = [];
for (let i = 0; i < latencyN; i++) {
  const p = await push(round(base + 100 + i, 0.01 * i));
  if (p.status === 200) lat.push(p.ms);
}
const md = [
  `# push_readings live contract test`,
  "",
  `Run ${new Date().toISOString()} against ${URL_}, device ${device}, experiment ${exp}. Test rows remain in experiment ${exp}.`,
  "",
  "| Case | Expected | Got | Result |",
  "|---|---|---|---|",
  ...results.map((x) => `| ${x.name} | ${x.expect} | ${x.got} | ${x.pass ? "PASS" : "FAIL"} |`),
  "",
  "Insert round trip from this host (includes TLS reuse by fetch; the WT32 opens a new TLS session per request, see MET HTTP):",
  "",
  SUMMARY_HEADER,
  summaryRow(`push_readings ${bottles} rows, ms`, summarize(lat), 0),
  "",
];
const stamp = new Date().toISOString().replace(/[:]/g, "").replace(/\..*/, "Z");
const outDir = resolve(one("out", join(REPO, "verification", "runs", `${stamp}-db-replay`)));
mkdirSync(outDir, { recursive: true });
writeFileSync(join(outDir, "db-replay.md"), md.join("\n") + "\n");
await call("/rest/v1/rpc/revoke_session", {}, session);
console.log(`\n${results.filter((x) => x.pass).length}/${results.length} passed. Wrote ${join(outDir, "db-replay.md")}`);
process.exit(results.every((x) => x.pass) ? 0 : 2);
