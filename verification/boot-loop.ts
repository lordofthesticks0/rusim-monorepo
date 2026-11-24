#!/usr/bin/env bun
/**
 * Repeated boot-time measurement (proposal Table 2.1 #4).
 *
 *   bun verification/boot-loop.ts --board display --reps 20
 *   bun verification/boot-loop.ts --board display --method manual --reps 20
 *   bun verification/boot-loop.ts --board wt32 --reps 20 --until ready
 *
 * Methods:
 *   command (default)  software restart: display console REBOOT, or
 *                      "W32CMD DBG REBOOT" for the WT32 (metrics builds)
 *   manual             the operator power-cycles the board when prompted;
 *                      this is the cold boot the proposal describes
 *
 * After each boot the tool asks the board to replay its milestones
 * (BOOTLOG / DBG BOOTLOG), so lines lost while USB re-enumerates after a
 * power cycle are still recorded. Milestone times are the board's own
 * millis() since the application started; ROM and bootloader time before
 * that is not included (check it once with video).
 *
 * Output: <out>/raw.log and <out>/boot-loop.csv (one row per repetition).
 */
import { appendFileSync, existsSync, mkdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { createInterface } from "node:readline/promises";
import { openLinePort, resolvePort, type LinePort } from "./lib/serial";
import { parseArgs, summarize, summaryRow, SUMMARY_HEADER, toCsv } from "./lib/stats";

const REPO = resolve(import.meta.dir, "..");
const { flags } = parseArgs(process.argv.slice(2));
const one = (k: string, d: string) => flags[k]?.at(-1) ?? d;

if (flags.help) {
  console.log(`Usage: bun verification/boot-loop.ts [options]
  --board display|wt32  board under test (default: display)
  --method command|manual
                        command: software restart (console REBOOT / DBG REBOOT)
                        manual: the operator power-cycles when prompted (default: command)
  --reps <n>            repetitions (default: 20)
  --until <stage>       milestone that ends a boot
                        (default: touch_ready for display, ready for wt32)
  --timeout-s <n>       per-boot timeout (default: 30 display, 240 wt32)
  --gap-s <n>           pause between boots (default: 5)
  --port <path>         display USB port (default: the single USB serial port)
  --out <dir>           default verification/runs/<UTC stamp>-boot-<board>-<method>`);
  process.exit(0);
}

const board = one("board", "display");
const method = one("method", "command");
const reps = Number(one("reps", "20"));
const until = one("until", board === "display" ? "touch_ready" : "ready");
const timeoutMs = Number(one("timeout-s", board === "display" ? "30" : "240")) * 1000;
const gapMs = Number(one("gap-s", "5")) * 1000;
if (!["display", "wt32"].includes(board) || !["command", "manual"].includes(method)) {
  console.error("--board display|wt32, --method command|manual");
  process.exit(1);
}
const stamp = new Date().toISOString().replace(/[:]/g, "").replace(/\..*/, "Z");
const outDir = resolve(one("out", join(REPO, "verification", "runs", `${stamp}-boot-${board}-${method}`)));
mkdirSync(outDir, { recursive: true });
const port = resolvePort(flags.port?.at(-1));
const prefix = board === "display" ? "MET " : "[W32] MET ";

type Waiter = { pred: (l: string) => boolean; resolve: (l: string | null) => void };
const waiters: Waiter[] = [];
const recent: string[] = [];
function onLine(line: string, hostMs: number): void {
  appendFileSync(join(outDir, "raw.log"), `${new Date(hostMs).toISOString()}\t${hostMs}\t${line}\n`);
  recent.push(line);
  if (recent.length > 400) recent.shift();
  for (const w of [...waiters]) {
    if (w.pred(line)) {
      waiters.splice(waiters.indexOf(w), 1);
      w.resolve(line);
    }
  }
}
function waitFor(pred: (l: string) => boolean, ms: number): Promise<string | null> {
  return new Promise((res) => {
    const w: Waiter = { pred, resolve: res };
    waiters.push(w);
    setTimeout(() => {
      const i = waiters.indexOf(w);
      if (i >= 0) {
        waiters.splice(i, 1);
        res(null);
      }
    }, ms);
  });
}

let link: LinePort | null = null;
function open(): void {
  link = openLinePort(port, 115200, onLine, () => {
    link = null;
    const retry = () => (existsSync(port) ? open() : setTimeout(retry, 300));
    setTimeout(retry, 300);
  });
}
open();
const send = (s: string) => link?.write(s);
const rl = createInterface({ input: process.stdin, output: process.stdout });

const stagesSeen = new Set<string>();
const rows: Record<string, string | number>[] = [];
for (let rep = 1; rep <= reps; rep++) {
  recent.length = 0;
  const resetP = waitFor((l) => l.startsWith(`${prefix}RESET `) && !l.includes("replay=1"), method === "manual" ? 600000 : timeoutMs);
  if (method === "manual") {
    await rl.question(`[boot ${rep}/${reps}] Power-cycle the ${board} now, then press Enter (or just wait)... `);
  } else {
    send(board === "display" ? "REBOOT" : "W32CMD DBG REBOOT");
  }
  const target = await waitFor((l) => l.startsWith(`${prefix}BOOT stage=${until} `), timeoutMs);
  await resetP;
  await Bun.sleep(1500);
  // Replay the milestone table: complete even if early lines were lost.
  const replay: Record<string, number> = {};
  let reason = "";
  const collect = (l: string) => {
    const m = new RegExp(`^${prefix.replace(/[[\]]/g, "\\$&")}BOOT stage=(\\S+) ms=(\\d+)`).exec(l);
    if (m) replay[m[1]!] = Number(m[2]);
    const r = /RESET reason=(\S+)/.exec(l);
    if (r && l.startsWith(prefix)) reason = r[1]!;
  };
  for (const l of recent) collect(l);
  send(board === "display" ? "BOOTLOG" : "W32CMD DBG BOOTLOG");
  await Bun.sleep(1500);
  for (const l of recent) collect(l);
  for (const k of Object.keys(replay)) stagesSeen.add(k);
  const row: Record<string, string | number> = { rep, board, method, reset_reason: reason, reached: target ? 1 : 0, ...replay };
  rows.push(row);
  console.log(`[boot ${rep}/${reps}] ${target ? "ok" : "TIMEOUT"} reason=${reason} ${until}=${replay[until] ?? "-"} ms`);
  if (rep < reps) await Bun.sleep(gapMs);
}

const stages = [...stagesSeen].sort((a, b) => (rows[0]?.[a] as number ?? 0) - (rows[0]?.[b] as number ?? 0));
const header = ["rep", "board", "method", "reset_reason", "reached", ...stages];
writeFileSync(join(outDir, "boot-loop.csv"), toCsv(header, rows.map((r) => header.map((h) => r[h] ?? ""))));
const md = [`# Boot loop: ${board}, ${method}, ${reps} reps`, "", SUMMARY_HEADER];
for (const s of stages) md.push(summaryRow(`${s}, ms`, summarize(rows.map((r) => Number(r[s] ?? NaN)), s === "touch_ready" ? 10000 : null), 0));
writeFileSync(join(outDir, "boot-loop.md"), md.join("\n") + "\n");
console.log(md.join("\n"));
console.log(`\nWrote ${outDir}`);
rl.close();
(link as LinePort | null)?.close();
process.exit(0);
