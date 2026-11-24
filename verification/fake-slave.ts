#!/usr/bin/env bun
/**
 * Fake Mega slave: answers the WT32's "#1:PING*" with scripted JSON so the
 * Display / WT32 / database / dashboard chain can be tested without the
 * partner's sensor hardware (docs/verification-toolkit.md §8).
 *
 * Wiring: a 3.3 V USB-UART adapter in place of the Mega.
 *   adapter TX -> WT32 IO5 (UART1 RX), adapter RX <- WT32 IO17, GND -> GND.
 *   9600 8N1. A 3.3 V adapter needs no divider (the real Mega does).
 *
 *   bun verification/fake-slave.ts --port /dev/ttyUSB1 --scenario mixed --seed 7
 *
 * Scenarios:
 *   normal     4 bottles, known values that change every round
 *   nulls      like normal, ~20 % of fields replaced by the slave's invalid
 *              markers (-1, temp -999)
 *   missing    only bottles 1 and 3 reported
 *   sentinel   legitimate values close to the markers (-0.999, -998.9, 0)
 *   malformed  reply that is not JSON, or JSON cut short
 *   oversize   600-byte reply (WT32 cap is 512)
 *   slow       reply after 1700 ms (WT32 waits 1500 ms)
 *   silent     no reply at all
 *   mixed      cycles normal x6, nulls, missing, sentinel, malformed,
 *              oversize, slow, silent
 *
 * Every reply is written to <out>/expected.csv (one row per bottle sent).
 * verification/fidelity.ts compares it with what reached the display and the
 * database.
 */
import { appendFileSync, createReadStream, createWriteStream, mkdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { configurePort, resolvePort } from "./lib/serial";
import { parseArgs, toCsv } from "./lib/stats";

const REPO = resolve(import.meta.dir, "..");
const { flags } = parseArgs(process.argv.slice(2));
const one = (k: string, d: string) => flags[k]?.at(-1) ?? d;
const scenario = one("scenario", "normal");
const SCENARIOS = ["normal", "nulls", "missing", "sentinel", "malformed", "oversize", "slow", "silent", "mixed"];
if (!SCENARIOS.includes(scenario) || flags.help) {
  console.log(`Usage: bun verification/fake-slave.ts --port <adapter> [--scenario ${SCENARIOS.join("|")}] [--seed n] [--out dir]`);
  process.exit(flags.help ? 0 : 1);
}
const port = resolvePort(flags.port?.at(-1));
const stamp = new Date().toISOString().replace(/[:]/g, "").replace(/\..*/, "Z");
const outDir = resolve(one("out", join(REPO, "verification", "runs", `${stamp}-fake-slave-${scenario}`)));
mkdirSync(outDir, { recursive: true });
const expectedPath = join(outDir, "expected.csv");
const HEADER = ["host_ms", "iso", "round", "scenario", "kind", "bottle", "ph", "methane", "temp", "co2", "press"];
writeFileSync(expectedPath, toCsv(HEADER, []));

// Deterministic PRNG so a run can be repeated exactly.
let seed = Number(one("seed", "1")) >>> 0 || 1;
const rand = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 0x100000000);

type Bottle = { id: number; ph: number; methane: number; temp: number; co2: number; press: number };
const MIXED = ["normal", "normal", "normal", "normal", "normal", "normal", "nulls", "missing", "sentinel", "malformed", "oversize", "slow", "silent"];

function values(round: number, id: number): Bottle {
  // Distinct per round and bottle, within each sensor's plausible range.
  // %.4g on the display link keeps 4 significant digits, so pressure (Pa,
  // 6 digits) shows the link's rounding; the database keeps full float32.
  return {
    id,
    ph: Number((6.0 + id * 0.1 + (round % 50) * 0.01).toFixed(2)),
    methane: Number((500 + id * 25 + (round % 200) * 0.5).toFixed(1)),
    temp: Number((38.0 + id * 0.1 + (round % 20) * 0.05).toFixed(2)),
    co2: 1000 + id * 100 + (round % 500),
    press: Number((101000 + id * 50 + (round % 100) * 1.5).toFixed(1)),
  };
}

function build(round: number, kind: string): { text: string | null; bottles: Bottle[]; delayMs: number } {
  let ids = [1, 2, 3, 4];
  if (kind === "missing") ids = [1, 3];
  const bottles = ids.map((id) => values(round, id));
  if (kind === "nulls") {
    for (const b of bottles) {
      if (rand() < 0.2) b.ph = -1;
      if (rand() < 0.2) b.methane = -1;
      if (rand() < 0.2) b.temp = -999;
      if (rand() < 0.2) b.co2 = -1;
      if (rand() < 0.2) b.press = -1;
    }
  }
  if (kind === "sentinel") {
    bottles[0] = { ...bottles[0]!, ph: -0.999, temp: -998.9, co2: 0, methane: 0, press: -1.001 };
  }
  const json = JSON.stringify({ bottles: bottles.map((b) => ({ id: b.id, ph: b.ph, ph_raw: 512, methane: b.methane, temp: b.temp, co2: b.co2, press: b.press })) });
  if (kind === "silent") return { text: null, bottles: [], delayMs: 0 };
  if (kind === "malformed") return { text: rand() < 0.5 ? "ERR sensor bus" : json.slice(0, 40), bottles: [], delayMs: 0 };
  if (kind === "oversize") return { text: json + " ".repeat(Math.max(0, 600 - json.length)), bottles: [], delayMs: 0 };
  return { text: json, bottles, delayMs: kind === "slow" ? 1700 : 60 };
}

configurePort(port, 9600);
const rd = createReadStream(port, { highWaterMark: 64 });
const wr = createWriteStream(port);
let buf = "";
let round = 0;
console.error(`[fake-slave] ${port} @ 9600, scenario=${scenario}, expected -> ${expectedPath}`);
rd.on("data", (chunk: Buffer | string) => {
  buf += typeof chunk === "string" ? chunk : chunk.toString("latin1");
  if (buf.length > 256) buf = buf.slice(-64);
  let at = buf.indexOf("#1:PING*");
  while (at >= 0) {
    buf = buf.slice(at + 8);
    round++;
    const kind = scenario === "mixed" ? MIXED[(round - 1) % MIXED.length]! : scenario;
    const { text, bottles, delayMs } = build(round, kind);
    const host = Date.now();
    if (bottles.length === 0) {
      appendFileSync(expectedPath, toCsv(HEADER, [[host, new Date(host).toISOString(), round, scenario, kind, "", "", "", "", "", ""]]).split("\n")[1] + "\n");
    } else {
      const rows = bottles.map((b) => [host, new Date(host).toISOString(), round, scenario, kind, b.id, b.ph, b.methane, b.temp, b.co2, b.press]);
      appendFileSync(expectedPath, toCsv(HEADER, rows).split("\n").slice(1).join("\n"));
    }
    if (text !== null) setTimeout(() => wr.write(text + "\r\n"), delayMs);
    console.error(`[fake-slave] round ${round} ${kind}${text === null ? " (no reply)" : ` ${text.length} B`}`);
    at = buf.indexOf("#1:PING*");
  }
});
rd.on("error", (e) => {
  console.error(`[fake-slave] ${e.message}`);
  process.exit(1);
});
process.on("SIGINT", () => process.exit(0));
