#!/usr/bin/env bun
/**
 * Turns one or more capture runs into report tables and plot-ready CSVs.
 *
 *   bun verification/analyze.ts verification/runs/<run> [<run> ...] [--out <dir>]
 *
 * Writes <out>/summary.md, <out>/summary.json and one CSV per metric family
 * (default <out> = <first run>/analysis). Definitions of every metric, and
 * which checklist item each one answers: docs/verification-toolkit.md.
 */
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { parseLinkIn, parseMet, parseSens, readRaw, type MetRecord, type RawRecord } from "./lib/log";
import { parseArgs, slope, summarize, summaryRow, SUMMARY_HEADER, toCsv, type Summary } from "./lib/stats";

// Specification limits from the proposal (Table 2.1) and the firmware caps.
const LIMIT = {
  guiMs: 200,
  internalMs: 1000,
  bootMs: 10000,
  reconnectMs: 10000,
  replyMs: 1500,
  frameBytes: 512,
  pushBodyBytes: 6144,
  respBytes: 4096,
};
const LINK_BAUD = 115200;

const { flags, positional } = parseArgs(process.argv.slice(2));
if (positional.length === 0 || flags.help) {
  console.log("Usage: bun verification/analyze.ts <run-dir> [<run-dir> ...] [--out <dir>]");
  process.exit(positional.length === 0 ? 1 : 0);
}
const runs = positional.map((p) => resolve(p));
const outDir = resolve(flags.out?.at(-1) ?? join(runs[0]!, "analysis"));
mkdirSync(outDir, { recursive: true });

let raw: RawRecord[] = [];
const metas: unknown[] = [];
for (const r of runs) {
  raw = raw.concat(readRaw(join(r, "raw.log")));
  const mp = join(r, "meta.json");
  if (existsSync(mp)) metas.push(JSON.parse(readFileSync(mp, "utf8")));
}
raw.sort((a, b) => a.hostMs - b.hostMs);
const met = parseMet(raw);
const linkIn = parseLinkIn(raw);
const num = (v: string | undefined) => (v === undefined || v === "-" ? NaN : Number(v));
const of = (src: "display" | "wt32", kind: string) => met.filter((m) => m.src === src && m.kind === kind);
const t0 = raw[0]?.hostMs ?? 0;
const rel = (ms: number) => ((ms - t0) / 1000).toFixed(3);

const md: string[] = [];
const json: Record<string, unknown> = { runs, metas, limits: LIMIT };
const csv = (name: string, header: string[], rows: (string | number | null | undefined)[][]) =>
  writeFileSync(join(outDir, name), toCsv(header, rows));
const table = (rows: [string, Summary, number?][]) => {
  md.push(SUMMARY_HEADER);
  for (const [label, s, d] of rows) md.push(summaryRow(label, s, d ?? 1));
  md.push("");
};

md.push(`# Verification analysis`, "", `Runs: ${runs.map((r) => "`" + r + "`").join(", ")}`, "");
md.push(`Records: ${raw.length} lines, ${met.length} MET lines, ${linkIn.length} inbound link lines. Span ${((raw.at(-1)?.hostMs ?? 0) - t0) / 3600000 > 0 ? (((raw.at(-1)?.hostMs ?? 0) - t0) / 3600000).toFixed(2) : "0"} h.`, "");

// ---------------------------------------------------------------- resets
{
  const resets = met.filter((m) => m.kind === "RESET" && m.f.replay !== "1");
  md.push("## Resets", "", "| Host time (s) | Board | Reason |", "|---|---|---|");
  for (const r of resets) md.push(`| ${rel(r.hostMs)} | ${r.src} | ${r.f.reason} |`);
  md.push("");
  json.resets = resets.map((r) => ({ t: rel(r.hostMs), src: r.src, reason: r.f.reason }));
}

// ---------------------------------------------------------------- touch latency (Table 2.1 #1)
{
  const touches = of("display", "TOUCH");
  csv(
    "touch.csv",
    ["host_s", "mark", "edge", "lat_ms", "inv_ms", "render_ms", "nf", "stuck", "wake", "from", "to", "x", "y"],
    touches.map((t) => [rel(t.hostMs), t.mark, t.f.edge, num(t.f.lat_us) / 1000, num(t.f.inv_us) / 1000, num(t.f.render_us) / 1000, t.f.nf, t.f.stuck, t.f.wake, t.f.from, t.f.to, t.f.x, t.f.y]),
  );
  const valid = touches.filter((t) => t.f.nf === "0" && t.f.stuck === "0" && t.f.wake === "0");
  md.push("## GUI latency (touch edge -> frame flushed), ms", "");
  md.push(`Edges: ${touches.length}; measured: ${valid.length}; no visible change (nf): ${touches.filter((t) => t.f.nf === "1").length}; stuck: ${touches.filter((t) => t.f.stuck === "1").length}; wake taps: ${touches.filter((t) => t.f.wake === "1").length}. Spec: every sample <= ${LIMIT.guiMs} ms.`, "");
  const groups = new Map<string, number[]>();
  for (const t of valid) {
    for (const key of [`all / ${t.f.edge}`, `${t.mark || "(no mark)"} / ${t.f.edge}`]) {
      const arr = groups.get(key) ?? [];
      arr.push(num(t.f.lat_us) / 1000);
      groups.set(key, arr);
    }
  }
  const rows: [string, Summary][] = [...groups.entries()].map(([k, v]) => [k, summarize(v, LIMIT.guiMs)]);
  table(rows);
  json.touch = Object.fromEntries(rows);
  const holds = of("display", "HOLD");
  if (holds.length > 0) {
    const commits = holds.filter((h) => h.f.event === "commit").map((h) => num(h.f.held_ms));
    const cancels = holds.filter((h) => h.f.event === "cancel").map((h) => num(h.f.held_ms));
    md.push("### Hold-to-confirm (2000 ms)", "");
    md.push(`Starts ${holds.filter((h) => h.f.event === "start").length}, commits ${commits.length}, cancels ${cancels.length}. Commits held < 2000 ms (false triggers): ${commits.filter((x) => x < 2000).length}.`, "");
    table([
      ["commit held_ms", summarize(commits)],
      ["cancel held_ms", summarize(cancels)],
    ]);
    csv("hold.csv", ["host_s", "mark", "event", "held_ms", "route"], holds.map((h) => [rel(h.hostMs), h.mark, h.f.event, h.f.held_ms, h.f.route]));
  }
  const fps = of("display", "FPS");
  if (fps.length > 0) {
    csv("fps.csv", ["host_s", "window_ms", "frames", "avg_render_us", "max_render_us", "px", "idle_pct"], fps.map((f) => [rel(f.hostMs), f.f.window_ms, f.f.frames, f.f.avg_render_us, f.f.max_render_us, f.f.px, f.f.idle_pct]));
    md.push("### Render time per frame", "");
    table([
      ["avg render per window, ms", summarize(fps.map((f) => num(f.f.avg_render_us) / 1000)), 2],
      ["max render per window, ms", summarize(fps.map((f) => num(f.f.max_render_us) / 1000)), 2],
      ["frames per second (active windows)", summarize(fps.map((f) => (num(f.f.frames) * 1000) / num(f.f.window_ms))), 1],
      ["LVGL idle %", summarize(fps.map((f) => num(f.f.idle_pct))), 0],
    ]);
  }
}

// ---------------------------------------------------------------- boot (Table 2.1 #4)
{
  const bootRows: (string | number)[][] = [];
  for (const src of ["display", "wt32"] as const) {
    const boots = of(src, "BOOT").filter((b) => b.f.replay !== "1");
    if (boots.length === 0) continue;
    const byEpoch = new Map<number, Record<string, number>>();
    for (const b of boots) {
      const e = byEpoch.get(b.epoch) ?? {};
      e[b.f.stage!] = num(b.f.ms);
      byEpoch.set(b.epoch, e);
      bootRows.push([src, b.epoch, b.f.stage!, num(b.f.ms)]);
    }
    const stages = [...new Set(boots.map((b) => b.f.stage!))];
    md.push(`## Boot milestones, ${src} (ms since app start; one sample per reset)`, "");
    md.push(
      src === "display"
        ? `Spec: "touch_ready" (first touch-responsive screen) <= ${LIMIT.bootMs} ms. Network milestones depend on the login at /setup and are reported separately.`
        : `Network milestones; "portal_ok" and later include any time spent waiting for the portal login on the WT32 web page.`,
      "",
    );
    table(stages.map((s) => [s, summarize([...byEpoch.values()].map((e) => e[s] ?? NaN), s === "touch_ready" ? LIMIT.bootMs : null), 0]));
    json[`boot_${src}`] = Object.fromEntries(stages.map((s) => [s, summarize([...byEpoch.values()].map((e) => e[s] ?? NaN))]));
  }
  if (bootRows.length > 0) csv("boot.csv", ["board", "boot_index", "stage", "ms"], bootRows);
}

// ---------------------------------------------------------------- internal latency (Table 2.1 #2)
{
  const tx = of("wt32", "SENSTX").filter((t) => t.f.src === "poll");
  const rx = of("display", "SENSRX");
  const rows: (string | number)[][] = [];
  const totals: number[] = [];
  const procs: number[] = [];
  const renders: number[] = [];
  const polls = of("wt32", "POLL").filter((p) => p.f.ok === "1");
  for (const r of rx) {
    const wseq = num(r.f.wseq);
    if (!(wseq > 0) || r.f.noframe === "1") continue;
    // Same seq within 10 s of host time: WT32 seq restarts at every reboot.
    const t = tx.find((x) => num(x.f.seq) === wseq && Math.abs(x.hostMs - r.hostMs) < 10000);
    if (!t) continue;
    const wireMs = (num(t.f.bytes) * 10 * 1000) / LINK_BAUD;
    const procMs = num(t.f.proc_us) / 1000;
    const renderMs = num(r.f.render_us) / 1000;
    const total = procMs + wireMs + renderMs;
    // Request -> reply time of the poll that produced it (Mega side incl.).
    const poll = polls.filter((p) => p.hostMs <= t.hostMs + 50).at(-1);
    rows.push([rel(r.hostMs), wseq, num(poll?.f.reply_ms), procMs, wireMs.toFixed(2), num(r.f.apply_us) / 1000, renderMs, total.toFixed(2), r.f.route!]);
    totals.push(total);
    procs.push(procMs);
    renders.push(renderMs);
  }
  csv("internal_latency.csv", ["host_s", "wseq", "poll_reply_ms", "wt32_proc_ms", "wire_ms", "display_apply_ms", "display_render_ms", "total_ms", "route"], rows);
  md.push("## Internal data latency (reply parsed -> SENS -> rendered), ms", "");
  md.push(`Joined frames: ${rows.length} (needs both metrics builds). Total = WT32 parse+emit + wire time at ${LINK_BAUD} baud + display receive->frame. Each part is measured on one clock, so no clock sync is needed. Spec < ${LIMIT.internalMs} ms. The display applies frames on a ${1000} ms timer (UI_SENS_APPLY_MS), which dominates.`, "");
  table([
    ["total", summarize(totals, LIMIT.internalMs)],
    ["WT32 parse + emit", summarize(procs), 2],
    ["display receive -> frame", summarize(renders)],
  ]);
  json.internal = { total: summarize(totals, LIMIT.internalMs) };
}

// ---------------------------------------------------------------- sensor poll + enqueue spacing
{
  const polls = of("wt32", "POLL");
  if (polls.length > 0) {
    const ok = polls.filter((p) => p.f.ok === "1");
    const whys: Record<string, number> = {};
    for (const p of polls.filter((x) => x.f.ok === "0")) whys[p.f.why!] = (whys[p.f.why!] ?? 0) + 1;
    csv("poll.csv", ["host_s", "ok", "why", "reply_ms", "bytes", "present", "valid"], polls.map((p) => [rel(p.hostMs), p.f.ok, p.f.why, p.f.reply_ms, p.f.bytes, p.f.present, p.f.valid]));
    md.push("## Sensor poll (WT32 <-> Mega)", "");
    md.push(`Rounds ${polls.length}, ok ${ok.length} (${((100 * ok.length) / polls.length).toFixed(2)} %). Failures by cause: ${JSON.stringify(whys)}. Largest frame ${Math.max(...polls.map((p) => num(p.f.bytes)))} B (cap ${LIMIT.frameBytes}).`, "");
    const gaps: number[] = [];
    for (let i = 1; i < polls.length; i++) if (polls[i]!.epoch === polls[i - 1]!.epoch) gaps.push((polls[i]!.hostMs - polls[i - 1]!.hostMs) / 1000);
    table([
      ["reply time ok rounds, ms", summarize(ok.map((p) => num(p.f.reply_ms)), LIMIT.replyMs), 0],
      ["poll spacing (host clock), s", summarize(gaps), 2],
    ]);
    json.poll = { rounds: polls.length, ok: ok.length, whys };
  }
  // Per-field NULL rate from the SENS lines the display received.
  const sens = linkIn.filter((l) => l.word === "SENS");
  if (sens.length > 0) {
    const fields = ["ph", "ch4", "t", "co2", "p"];
    const tally: Record<string, { n: number; nul: number }> = {};
    const bottleSeen: Record<string, number> = {};
    for (const s of sens) {
      const { bottles } = parseSens(s.raw);
      for (let b = 1; b <= 4; b++) {
        if (!bottles[b]) continue;
        bottleSeen[`b${b}`] = (bottleSeen[`b${b}`] ?? 0) + 1;
        for (const k of fields) {
          const key = `b${b}.${k}`;
          tally[key] ??= { n: 0, nul: 0 };
          tally[key].n++;
          if (!Number.isFinite(bottles[b]![k] ?? NaN)) tally[key].nul++;
        }
      }
    }
    md.push("### NULL rate per bottle and field (SENS lines received by the display)", "", `SENS lines: ${sens.length}. Bottle present in: ${JSON.stringify(bottleSeen)}.`, "", "| Field | n | NULL | % |", "|---|---|---|---|");
    for (const [k, v] of Object.entries(tally)) md.push(`| ${k} | ${v.n} | ${v.nul} | ${((100 * v.nul) / v.n).toFixed(2)} |`);
    md.push("");
  }
  const enq = of("wt32", "ENQ");
  if (enq.length > 0) {
    csv("enq.csv", ["host_s", "unix", "exp", "rows", "queued", "drops", "dt_ms", "period_ms"], enq.map((e) => [rel(e.hostMs), e.f.unix, e.f.exp, e.f.rows, e.f.queued, e.f.drops, e.f.dt_ms, e.f.period_ms]));
    const spaced = enq.filter((e) => num(e.f.dt_ms) >= 0);
    const err = spaced.map((e) => (num(e.f.dt_ms) - num(e.f.period_ms)) / 1000);
    const periods = [...new Set(spaced.map((e) => e.f.period_ms))];
    md.push("## Enqueued sample spacing vs sample_interval_s", "");
    md.push(`Enqueued rounds ${enq.length}; rows ${enq.reduce((a, e) => a + num(e.f.rows), 0)}; configured period(s) ms: ${periods.join(", ")}. Error = actual spacing - configured. The 5 s poll quantises single spacings; the mean error shows drift.`, "");
    table([
      ["spacing error, s", summarize(err), 2],
      ["actual spacing, s", summarize(spaced.map((e) => num(e.f.dt_ms) / 1000)), 2],
    ]);
    json.enqueue = { rounds: enq.length, error: summarize(err) };
  }
}

// ---------------------------------------------------------------- HTTPS / Supabase
{
  const http = of("wt32", "HTTP");
  if (http.length > 0) {
    csv("http.csv", ["host_s", "op", "code", "ms", "tls_ms", "req", "resp", "trunc"], http.map((h) => [rel(h.hostMs), h.f.op, h.f.code, h.f.ms, h.f.tls_ms, h.f.req, h.f.resp, h.f.trunc]));
    md.push("## HTTPS requests (WT32 -> Supabase)", "", "| op | n | 2xx | status codes |", "|---|---|---|---|");
    const ops = [...new Set(http.map((h) => h.f.op!))];
    for (const op of ops) {
      const hs = http.filter((h) => h.f.op === op);
      const codes: Record<string, number> = {};
      for (const h of hs) codes[h.f.code!] = (codes[h.f.code!] ?? 0) + 1;
      md.push(`| ${op} | ${hs.length} | ${hs.filter((h) => num(h.f.code) >= 200 && num(h.f.code) < 300).length} | ${JSON.stringify(codes)} |`);
    }
    md.push("");
    const okH = http.filter((h) => num(h.f.code) >= 200 && num(h.f.code) < 300);
    table([
      ...ops.map((op) => [`${op} round trip, ms`, summarize(okH.filter((h) => h.f.op === op).map((h) => num(h.f.ms))), 0] as [string, Summary, number]),
      ["TLS connect (all ok requests), ms", summarize(okH.map((h) => num(h.f.tls_ms))), 0],
      ["push body, bytes", summarize(http.filter((h) => h.f.op === "push").map((h) => num(h.f.req)), LIMIT.pushBodyBytes), 0],
      ["response body, bytes", summarize(http.map((h) => num(h.f.resp)), LIMIT.respBytes), 0],
    ]);
    md.push(`Truncated responses: ${http.filter((h) => h.f.trunc === "1").length}.`, "");
  }
  const auth = of("wt32", "AUTH");
  if (auth.length > 0) {
    md.push("### Authentication", "", "| why | n | ok | ms (mean) |", "|---|---|---|---|");
    for (const why of [...new Set(auth.map((a) => a.f.why!))]) {
      const as = auth.filter((a) => a.f.why === why);
      md.push(`| ${why} | ${as.length} | ${as.filter((a) => a.f.ok === "1").length} | ${(as.reduce((s, a) => s + num(a.f.ms), 0) / as.length).toFixed(0)} |`);
    }
    md.push("");
  }
  const back = of("wt32", "BACKOFF");
  if (back.length > 0) {
    csv("backoff.csv", ["host_s", "fault", "wait_ms", "retry_after_s", "queued"], back.map((b) => [rel(b.hostMs), b.f.fault, b.f.wait_ms, b.f.retry_after_s, b.f.queued]));
    const byFault: Record<string, number> = {};
    for (const b of back) byFault[b.f.fault!] = (byFault[b.f.fault!] ?? 0) + 1;
    md.push(`### Upload backoff`, "", `Events by fault: ${JSON.stringify(byFault)}. Minimum wait ${Math.min(...back.map((b) => num(b.f.wait_ms)))} ms (no hot loop if > 0).`, "");
  }
  const pass = of("wt32", "PASS");
  if (pass.length > 0) {
    csv("pass.csv", ["host_s", "ms", "pushes", "queued", "drops", "acks", "fault", "ready", "run"], pass.map((p) => [rel(p.hostMs), p.f.ms, p.f.pushes, p.f.queued, p.f.drops, p.f.acks, p.f.fault, p.f.ready, p.f.run]));
    md.push("## Queue", "");
    md.push(`Passes ${pass.length}; max queued ${Math.max(...pass.map((p) => num(p.f.queued)))} rows; final drops ${pass.at(-1)?.f.drops}; acknowledged rounds (last) ${pass.at(-1)?.f.acks}.`, "");
    table([["DB pass duration, ms", summarize(pass.map((p) => num(p.f.ms))), 0]]);
  }
  const acks = of("wt32", "ACK");
  if (acks.length > 0) {
    const sent = acks.reduce((a, x) => a + num(x.f.rows), 0);
    const ins = acks.reduce((a, x) => a + num(x.f.inserted), 0);
    md.push(`Acknowledged uploads ${acks.length}: rows sent ${sent}, server inserted ${ins} (difference = replays).`, "");
  }
}

// ---------------------------------------------------------------- DB/NET state from the wire
{
  const db = linkIn.filter((l) => l.word === "DB");
  const net = linkIn.filter((l) => l.word === "NET");
  if (db.length > 0) {
    csv("db_state.csv", ["host_s", "queued", "drops", "ready", "state", "reads_ms", "ack_ms", "aexp", "time"], db.map((d) => [rel(d.hostMs), d.f.queued, d.f.drops, d.f.ready, d.f.state, d.f.reads, d.f.ack, d.f.aexp, d.f.time]));
  }
  if (net.length > 0) csv("net_state.csv", ["host_s", "up", "ping_ms", "ip"], net.map((n) => [rel(n.hostMs), n.f.up, n.f.ping, n.f.ip]));
  // Fault/restore episodes: the operator types "#fault <name>" when the fault
  // is applied and "#restore <name>" when it is removed (capture.ts markers).
  const marks = raw.filter((r) => r.text.startsWith("#MARK fault") || r.text.startsWith("#MARK restore"));
  if (marks.length > 0) {
    const firstAfter = (t: number, pred: (r: RawRecord) => boolean) => raw.find((r) => r.hostMs >= t && pred(r))?.hostMs;
    const isNet = (up: string) => (r: RawRecord) => r.text.startsWith("[LINK<] NET ") && r.text.includes(` up=${up}`);
    const dbState = (none: boolean) => (r: RawRecord) => r.text.startsWith("[LINK<] DB ") && (none ? / state=none\b/.test(r.text) : !/ state=none\b/.test(r.text));
    const metIs = (kind: string, extra = "") => (r: RawRecord) => r.text.startsWith(`[W32] MET ${kind}`) && r.text.includes(extra);
    const rows: (string | number | undefined)[][] = [];
    for (const m of marks) {
      const label = m.text.slice(6);
      const t = m.hostMs;
      const d = (x: number | undefined) => (x === undefined ? undefined : x - t);
      if (label.startsWith("fault")) {
        rows.push([rel(t), label, d(firstAfter(t, isNet("0"))), d(firstAfter(t, dbState(false))), d(firstAfter(t, metIs("POLL", "ok=0"))), "", "", "", ""]);
      } else {
        rows.push([rel(t), label, "", "", "", d(firstAfter(t, isNet("1"))), d(firstAfter(t, metIs("POLL", "ok=1"))), d(firstAfter(t, dbState(true))), d(firstAfter(t, metIs("ACK")))]);
      }
    }
    csv("episodes.csv", ["host_s", "mark", "net_down_ms", "db_fault_ms", "poll_fail_ms", "net_up_ms", "poll_ok_ms", "db_ok_ms", "first_ack_ms"], rows);
    md.push("## Fault / restore episodes (ms after the marker)", "", "Reconnect time = first successful action after `#restore` (spec <= 10 000 ms). Columns per marker type; empty = not applicable or never seen.", "");
    md.push("| t (s) | marker | NET up=0 | DB fault | POLL fail | NET up=1 | POLL ok | DB state=none | first ACK |", "|---|---|---|---|---|---|---|---|---|");
    for (const r of rows) md.push(`| ${r.map((x) => (x === undefined ? "never" : x)).join(" | ")} |`);
    md.push("");
  }
}

// ---------------------------------------------------------------- link loss proxies (Table 2.1 #3)
{
  const dl = of("display", "LINK");
  if (dl.length > 0) {
    const sum = (k: string) => dl.reduce((a, l) => a + num(l.f[k]), 0);
    const windowMs = dl.reduce((a, l) => a + num(l.f.window_ms), 0);
    const expectedNet = Math.floor(windowMs / 5000);
    md.push("## Display link (WT32 -> display)", "");
    md.push(`Window ${(windowMs / 3600000).toFixed(2)} h. Lines ${sum("rx")}: NET ${sum("net")} (heartbeat expects ~${expectedNet}), DB ${sum("db")}, SENS ${sum("sens")}, LOG ${sum("log")}, unknown ${sum("other")}. Partial-line discards ${sum("gap")}, over-length ${sum("ovf")}. Longest NET gap ${Math.max(...dl.map((l) => num(l.f.net_gap_max_ms)))} ms (5000 = no heartbeat missed).`, "");
    csv("link_display.csv", ["host_s", "window_ms", "rx", "net", "db", "sens", "log", "other", "gap", "ovf", "net_gap_max_ms", "db_gap_max_ms"], dl.map((l) => [rel(l.hostMs), l.f.window_ms, l.f.rx, l.f.net, l.f.db, l.f.sens, l.f.log, l.f.other, l.f.gap, l.f.ovf, l.f.net_gap_max_ms, l.f.db_gap_max_ms]));
    json.link_display = { windowMs, net: sum("net"), expectedNet, gap: sum("gap"), ovf: sum("ovf"), other: sum("other") };
  }
  const wl = of("wt32", "LINK");
  if (wl.length > 0) {
    const sum = (k: string) => wl.reduce((a, l) => a + num(l.f[k]), 0);
    const windowMs = wl.reduce((a, l) => a + num(l.f.window_ms), 0);
    md.push("## Display link (display -> WT32)", "");
    md.push(`Window ${(windowMs / 3600000).toFixed(2)} h. Lines ${sum("rx")}: EXPERIMENT ${sum("exp")} (heartbeat expects ~${Math.floor(windowMs / 5000)} once released), HELLO ${sum("hello")}, unknown ${sum("other")}. Partial-line discards ${sum("gap")}, over-length ${sum("ovf")}. Longest EXPERIMENT gap ${Math.max(...wl.map((l) => num(l.f.exp_gap_max_ms)))} ms.`, "");
    csv("link_wt32.csv", ["host_s", "window_ms", "rx", "exp", "hello", "other", "gap", "ovf", "exp_gap_max_ms"], wl.map((l) => [rel(l.hostMs), l.f.window_ms, l.f.rx, l.f.exp, l.f.hello, l.f.other, l.f.gap, l.f.ovf, l.f.exp_gap_max_ms]));
  }
  const loop = of("wt32", "LOOP");
  if (loop.length > 0) {
    csv("loop_wt32.csv", ["host_s", "iters", "max_iter_ms", "max_link_gap_ms"], loop.map((l) => [rel(l.hostMs), l.f.iters, l.f.max_iter_ms, l.f.max_link_gap_ms]));
    md.push("### WT32 responsiveness (per 60 s window)", "");
    table([
      ["longest loop iteration, ms", summarize(loop.map((l) => num(l.f.max_iter_ms))), 0],
      ["longest display-link service gap, ms", summarize(loop.map((l) => num(l.f.max_link_gap_ms))), 0],
    ]);
  }
}

// ---------------------------------------------------------------- memory + clock
{
  for (const src of ["display", "wt32"] as const) {
    const mem = of(src, "MEM");
    if (mem.length === 0) continue;
    const keys = src === "display" ? ["heap_free", "heap_min", "heap_maxblock", "psram_free", "psram_min", "psram_maxblock"] : ["free", "min", "maxblock"];
    csv(`mem_${src}.csv`, ["host_s", "uptime_s", ...keys], mem.map((m) => [rel(m.hostMs), m.f.uptime_s, ...keys.map((k) => m.f[k])]));
    md.push(`## Memory, ${src} (bytes; slope = bytes per hour, negative = shrinking)`, "", "| Counter | first | last | min | slope /h |", "|---|---|---|---|---|");
    const hours = mem.map((m) => m.hostMs / 3600000);
    for (const k of keys) {
      const v = mem.map((m) => num(m.f[k]));
      md.push(`| ${k} | ${v[0]} | ${v.at(-1)} | ${Math.min(...v)} | ${slope(hours, v)?.toFixed(1) ?? "-"} |`);
    }
    md.push("");
  }
  const time = of("wt32", "TIME").filter((t) => t.f.ntp === "1");
  if (time.length > 1) {
    const off = time.map((t) => num(t.f.unix_ms) - t.hostMs);
    csv("clock_wt32.csv", ["host_s", "offset_ms"], time.map((t, i) => [rel(t.hostMs), off[i]]));
    md.push("## WT32 clock vs host clock", "");
    md.push(`Offset = WT32 UTC - host receive time (includes ~tens of ms of link/USB delay; keep the host NTP-synced). Drift ${slope(time.map((t) => t.hostMs / 3600000), off)?.toFixed(2) ?? "-"} ms/h.`, "");
    table([["offset, ms", summarize(off), 0]]);
  }
}

writeFileSync(join(outDir, "summary.md"), md.join("\n") + "\n");
writeFileSync(join(outDir, "summary.json"), JSON.stringify(json, null, 2) + "\n");
console.log(`Wrote ${join(outDir, "summary.md")}`);
