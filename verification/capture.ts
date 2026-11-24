#!/usr/bin/env bun
/**
 * Records the display's USB console to a run directory for later analysis.
 *
 * The display forwards every WT32 log line as "[W32] ...", and its metrics
 * build echoes every inbound link line as "[LINK<] ...", so this one port
 * carries both boards. Every line is stored with the host receive time.
 *
 *   bun verification/capture.ts --label soak-1 --duration 24h \
 *       --operator "<name>" --interval-s 30 --experiment 4 --bottles 4
 *
 * While it runs, type a line and press Enter:
 *   #<label>   writes a marker (e.g. "#tab-swipe") used to group the events
 *              that follow, e.g. one marker per interaction type
 *   <text>     is sent to the display console (MEM, BOOTLOG, W32CMD DUMP, ...)
 *
 * Output: <out>/raw.log and <out>/meta.json (the test-run log of
 * docs/verification-toolkit.md §2). Analyse with verification/analyze.ts.
 */
import { appendFileSync, existsSync, mkdirSync, readdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { createInterface } from "node:readline";
import { openLinePort, resolvePort, type LinePort } from "./lib/serial";
import { parseArgs, parseDuration } from "./lib/stats";

const REPO = resolve(import.meta.dir, "..");
const { flags } = parseArgs(process.argv.slice(2));
const one = (k: string, d?: string) => flags[k]?.at(-1) ?? d;

if (flags.help) {
  console.log(`Usage: bun verification/capture.ts [options]
  --label <name>        run label (directory name suffix)
  --duration <d>        stop after e.g. 90s, 30m, 24h (default: until Ctrl+C)
  --port <path>         display USB port (default: the single USB serial port)
  --baud <n>            default 115200
  --out <dir>           default verification/runs/<UTC stamp>-<label>
  --every <s>:<cmd>     send <cmd> every <s> seconds (repeatable), e.g. --every "300:W32CMD DUMP"
  --cmd <cmd>           send once at start (repeatable), e.g. --cmd BOOTLOG
  --operator, --interval-s, --experiment, --bottles, --network,
  --fw-display, --fw-wt32, --notes   recorded in meta.json
  --verbose             print every line, not only MET lines and markers
  --quiet               print nothing but errors`);
  process.exit(0);
}

const label = (one("label", "run") ?? "run").replace(/[^A-Za-z0-9._-]/g, "-");
const stamp = new Date().toISOString().replace(/[:]/g, "").replace(/\..*/, "Z");
const outDir = resolve(one("out", join(REPO, "verification", "runs", `${stamp}-${label}`))!);
mkdirSync(outDir, { recursive: true });
const rawPath = join(outDir, "raw.log");
const port = resolvePort(one("port"));
const baud = Number(one("baud", "115200"));
const durationMs = one("duration") ? parseDuration(one("duration")!) : 0;
const verbose = !!flags.verbose;
const quiet = !!flags.quiet;

function git(args: string[]): string {
  const r = Bun.spawnSync(["git", ...args], { cwd: REPO, stdout: "pipe", stderr: "pipe" });
  return new TextDecoder().decode(r.stdout).trim();
}

function latestMigration(): string {
  const dir = join(REPO, "dashboard", "supabase", "migrations");
  if (!existsSync(dir)) return "";
  return readdirSync(dir).filter((f) => f.endsWith(".sql")).sort().at(-1) ?? "";
}

const meta = {
  label,
  operator: one("operator", ""),
  notes: flags.notes?.join(" | ") ?? "",
  started: new Date().toISOString(),
  ended: "",
  port,
  baud,
  git: {
    commit: git(["rev-parse", "HEAD"]),
    branch: git(["rev-parse", "--abbrev-ref", "HEAD"]),
    dirty: git(["status", "--porcelain"]).length > 0,
  },
  firmware: { display: one("fw-display", "display-v9-metrics"), wt32: one("fw-wt32", "network-master-metrics") },
  dbLatestLocalMigration: latestMigration(),
  config: {
    sampleIntervalS: one("interval-s") ? Number(one("interval-s")) : null,
    experimentId: one("experiment") ? Number(one("experiment")) : null,
    bottles: Number(one("bottles", "4")),
    network: one("network", ""),
  },
  plannedDurationMs: durationMs,
  lines: 0,
  marks: [] as string[],
  reopens: 0,
};
const writeMeta = () => writeFileSync(join(outDir, "meta.json"), JSON.stringify(meta, null, 2) + "\n");
writeMeta();

const t0 = Date.now();
function record(text: string, hostMs = Date.now()): void {
  appendFileSync(rawPath, `${new Date(hostMs).toISOString()}\t${hostMs}\t${text}\n`);
}

let link: LinePort | null = null;
let stopping = false;

function onLine(line: string, hostMs: number): void {
  meta.lines++;
  record(line, hostMs);
  if (quiet) return;
  if (verbose || line.startsWith("MET ") || line.startsWith("[W32] MET ") || line.startsWith("[MET]")) {
    const t = ((hostMs - t0) / 1000).toFixed(1).padStart(8);
    console.log(`${t}s ${line}`);
  }
}

function open(): void {
  try {
    link = openLinePort(port, baud, onLine, (why) => {
      link = null;
      if (stopping) return;
      record(`#EVENT port-closed ${why}`);
      if (!quiet) console.error(`[capture] port closed (${why}); waiting for it to return...`);
      setTimeout(reopen, 500);
    });
  } catch (e) {
    record(`#EVENT open-failed ${(e as Error).message}`);
    setTimeout(reopen, 1000);
  }
}

function reopen(): void {
  if (stopping) return;
  if (!existsSync(port)) {
    setTimeout(reopen, 500);
    return;
  }
  meta.reopens++;
  record("#EVENT port-reopen");
  open();
}

record(`#EVENT capture-start label=${label}`);
open();
if (!quiet) console.error(`[capture] ${port} @ ${baud} -> ${rawPath}`);

for (const cmd of flags.cmd ?? []) {
  setTimeout(() => {
    record(`#EVENT sent ${cmd}`);
    link?.write(cmd);
  }, 500);
}
for (const spec of flags.every ?? []) {
  const c = spec.indexOf(":");
  const sec = Number(spec.slice(0, c));
  const cmd = spec.slice(c + 1);
  if (!(sec > 0) || !cmd) throw new Error(`Bad --every ${spec}; use <seconds>:<command>`);
  setInterval(() => {
    record(`#EVENT sent ${cmd}`);
    link?.write(cmd);
  }, sec * 1000);
}

if (!flags["no-stdin"]) {
  const rl = createInterface({ input: process.stdin });
  rl.on("line", (input) => {
    const s = input.trim();
    if (!s) return;
    if (s.startsWith("#")) {
      const m = s.slice(1).trim();
      meta.marks.push(m);
      record(`#MARK ${m}`);
      if (!quiet) console.error(`[capture] mark: ${m}`);
    } else {
      record(`#EVENT sent ${s}`);
      link?.write(s);
    }
  });
}

function stop(code = 0): void {
  if (stopping) return;
  stopping = true;
  record("#EVENT capture-stop");
  meta.ended = new Date().toISOString();
  writeMeta();
  link?.close();
  if (!quiet) console.error(`[capture] stopped. ${meta.lines} lines in ${rawPath}`);
  process.exit(code);
}
process.on("SIGINT", () => stop(0));
process.on("SIGTERM", () => stop(0));
setInterval(writeMeta, 60000);
if (durationMs > 0) setTimeout(() => stop(0), durationMs);
