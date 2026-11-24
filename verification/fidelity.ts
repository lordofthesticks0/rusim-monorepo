#!/usr/bin/env bun
/**
 * Value fidelity: compares what the fake slave sent (expected.csv) with what
 * reached the display link (SENS lines in a capture) and the database (a
 * dashboard CSV export, or a CSV dump of device_data.readings).
 *
 *   bun verification/fidelity.ts --expected <fake-slave run>/expected.csv \
 *       [--capture <capture run>/raw.log] [--db export.csv --db-bottle-base 1] \
 *       [--tolerance-s 3] [--out <dir>]
 *
 * Matching is by time: the fake slave and the capture run on the same host,
 * so a SENS line belongs to the latest reply sent before it. A database row
 * carries the WT32's NTP sample time, matched to the nearest reply for that
 * bottle within --tolerance-s (host and WT32 must both be NTP-synced).
 *
 * Expected results: invalid markers (-1, temp -999) arrive as NULL / nan;
 * values near a marker arrive unchanged; the display link rounds to 4
 * significant digits (%.4g); the database stores float32 ("real").
 * --db-bottle-base: 1 for a dashboard export (chamber numbers), 0 for a raw
 * dump of bottle_id.
 */
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { parseSens, readRaw } from "./lib/log";
import { parseArgs, toCsv } from "./lib/stats";

const { flags } = parseArgs(process.argv.slice(2));
const one = (k: string) => flags[k]?.at(-1);
if (!one("expected")) {
  console.log("Usage: bun verification/fidelity.ts --expected expected.csv [--capture raw.log] [--db file.csv] [--db-bottle-base 1] [--tolerance-s 3]");
  process.exit(1);
}
const tolMs = Number(one("tolerance-s") ?? "3") * 1000;
const outDir = resolve(one("out") ?? join(dirname(resolve(one("expected")!)), "fidelity"));
mkdirSync(outDir, { recursive: true });

function readCsv(path: string): Record<string, string>[] {
  const text = readFileSync(path, "utf8").replace(/^﻿/, "");
  const rows: string[][] = [];
  let row: string[] = [];
  let cell = "";
  let q = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i]!;
    if (q) {
      if (c === '"' && text[i + 1] === '"') {
        cell += '"';
        i++;
      } else if (c === '"') q = false;
      else cell += c;
    } else if (c === '"') q = true;
    else if (c === ",") {
      row.push(cell);
      cell = "";
    } else if (c === "\n" || c === "\r") {
      if (c === "\r" && text[i + 1] === "\n") i++;
      row.push(cell);
      if (row.some((x) => x !== "")) rows.push(row);
      row = [];
      cell = "";
    } else cell += c;
  }
  if (cell !== "" || row.length > 0) {
    row.push(cell);
    rows.push(row);
  }
  const [head, ...body] = rows;
  return body.map((r) => Object.fromEntries(head!.map((h, i) => [h.trim(), r[i] ?? ""])));
}

type Exp = { hostMs: number; round: number; kind: string; bottle: number; v: Record<string, number> };
const FIELDS = ["ph", "methane", "temp", "co2", "press"] as const;
const SENTINEL: Record<string, number> = { ph: -1, methane: -1, temp: -999, co2: -1, press: -1 };
const expected: Exp[] = readCsv(one("expected")!)
  .filter((r) => r.bottle !== "")
  .map((r) => ({
    hostMs: Number(r.host_ms),
    round: Number(r.round),
    kind: r.kind!,
    bottle: Number(r.bottle),
    v: Object.fromEntries(FIELDS.map((f) => [f, Number(r[f])])),
  }));
const rounds = [...new Set(expected.map((e) => e.round))].map((r) => expected.filter((e) => e.round === r));

// Distance between a reply's bottle and received values: summed relative
// error of the fields both sides have. Used to choose among replies inside
// the time window, so closely spaced replies are not confused.
function distance(e: Exp, got: Record<string, number | null>): number {
  let d = 0;
  for (const f of FIELDS) {
    const want = e.v[f]!;
    const g = got[f];
    const wantNull = want === SENTINEL[f];
    const gotNull = g === null || g === undefined || !Number.isFinite(g);
    if (wantNull !== gotNull) d += 1;
    else if (!wantNull) d += Math.abs(g! - want) / Math.max(1e-9, Math.abs(want));
  }
  return d;
}

type Cmp = { n: number; nullOk: number; nullBad: number; maxAbs: number; maxRel: number; missing: number; extra: number };
const blank = (): Cmp => ({ n: 0, nullOk: 0, nullBad: 0, maxAbs: 0, maxRel: 0, missing: 0, extra: 0 });
const detail: (string | number)[][] = [];
function compare(c: Cmp, where: string, t: number, e: Exp, field: string, got: number | null): void {
  const want = e.v[field]!;
  const wantNull = want === SENTINEL[field];
  const gotNull = got === null || !Number.isFinite(got);
  c.n++;
  if (wantNull || gotNull) {
    if (wantNull && gotNull) c.nullOk++;
    else {
      c.nullBad++;
      detail.push([where, t, e.round, e.bottle, field, want, got ?? "null", "null mismatch"]);
    }
    return;
  }
  const abs = Math.abs(got! - want);
  const relErr = want !== 0 ? abs / Math.abs(want) : abs;
  c.maxAbs = Math.max(c.maxAbs, abs);
  c.maxRel = Math.max(c.maxRel, relErr);
  detail.push([where, t, e.round, e.bottle, field, want, got!, relErr.toExponential(2)]);
}

const md: string[] = ["# Value fidelity", "", `Expected: \`${one("expected")}\` (${rounds.length} replies with bottles).`, ""];
const report = (name: string, c: Cmp, note: string) => {
  md.push(`## ${name}`, "", note, "", "| values | NULL correct | NULL wrong | max abs diff | max rel diff | bottles missing | bottles extra |", "|---|---|---|---|---|---|---|");
  md.push(`| ${c.n} | ${c.nullOk} | ${c.nullBad} | ${c.maxAbs.toPrecision(4)} | ${c.maxRel.toExponential(2)} | ${c.missing} | ${c.extra} |`, "");
};

// Display link: SENS lines.
if (one("capture")) {
  const SENS_KEY: Record<string, string> = { ph: "ph", methane: "ch4", temp: "t", co2: "co2", press: "p" };
  const c = blank();
  let matched = 0;
  for (const r of readRaw(one("capture")!)) {
    if (!r.text.startsWith("[LINK<] SENS ")) continue;
    const { bottles } = parseSens(r.text.slice(8));
    const asGot = (b: Record<string, number> | undefined) =>
      Object.fromEntries(FIELDS.map((f) => [f, b?.[SENS_KEY[f]!] ?? null])) as Record<string, number | null>;
    const window = rounds.filter((g) => g[0]!.hostMs <= r.hostMs && r.hostMs - g[0]!.hostMs < tolMs);
    const score = (g: Exp[]) => g.reduce((a, e) => a + (bottles[e.bottle] ? distance(e, asGot(bottles[e.bottle])) : 5), 0) + Object.keys(bottles).filter((b) => !g.some((e) => e.bottle === Number(b))).length * 5;
    const reply = window.length === 0 ? undefined : window.reduce((a, b) => (score(b) < score(a) ? b : a));
    if (!reply) continue;
    matched++;
    for (const e of reply) {
      const got = bottles[e.bottle];
      if (!got) {
        c.missing++;
        continue;
      }
      for (const f of FIELDS) compare(c, "sens", r.hostMs, e, f, got[SENS_KEY[f]!] ?? null);
    }
    for (const b of Object.keys(bottles)) if (!reply.some((e) => e.bottle === Number(b))) c.extra++;
  }
  report("Display link (SENS)", c, `SENS lines matched to a reply: ${matched}. Expected relative difference up to ~5e-4 from %.4g.`);
}

// Database: dashboard CSV export or raw dump.
if (one("db")) {
  const base = Number(one("db-bottle-base") ?? "1");
  const rows = readCsv(one("db")!);
  const col = (r: Record<string, string>, re: RegExp) => {
    const k = Object.keys(r).find((h) => re.test(h));
    return k === undefined || r[k] === "" ? null : Number(r[k]);
  };
  const c = blank();
  let unmatched = 0;
  for (const r of rows) {
    const t = Date.parse(r.timestamp!);
    const bottle = Number(r.bottle_id) - base + 1; // -> fake-slave id 1..4
    const cands = expected.filter((e) => e.bottle === bottle && Math.abs(e.hostMs - t) <= tolMs);
    if (cands.length === 0) {
      unmatched++;
      continue;
    }
    const got: Record<string, number | null> = {
      ph: col(r, /^ph\b/i),
      methane: col(r, /^(ch4|methane)/i),
      temp: col(r, /^temp/i),
      co2: col(r, /^co2/i),
      press: col(r, /^(pressure|press)/i),
    };
    const e = cands.reduce((a, b) => (distance(b, got) < distance(a, got) ? b : a));
    for (const f of FIELDS) compare(c, "db", t, e, f, got[f]!);
  }
  report("Database rows", c, `Rows ${rows.length}, matched ${rows.length - unmatched}, unmatched ${unmatched} (outside ±${tolMs / 1000} s of any reply for that bottle). Only enqueued rounds (one per sample_interval_s) reach the database. Expected relative difference ~1e-7 (float32).`);
}

writeFileSync(join(outDir, "fidelity.md"), md.join("\n") + "\n");
writeFileSync(join(outDir, "fidelity.csv"), toCsv(["where", "host_or_sample_ms", "round", "bottle", "field", "sent", "received", "rel_err"], detail));
console.log(md.join("\n"));
console.log(`Wrote ${outDir}`);
