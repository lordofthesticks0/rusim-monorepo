/**
 * Raw capture format and MET line parsing.
 *
 * raw.log holds one record per line: "<iso>\t<host_ms>\t<text>".
 *   <text> is the device line as received, or a tool record starting with
 *   "#MARK " (operator label) or "#EVENT " (tool event, e.g. port reopen).
 *
 * Device lines of interest (docs/verification-toolkit.md):
 *   "MET <KIND> k=v ..."            display metric
 *   "[W32] MET <KIND> k=v ..."      WT32 metric forwarded by the display
 *   "[LINK<] <word> ..."            inbound rusim-line line (display metrics build)
 *   "[W32] <text>"                  any other WT32 log line
 */
import { readFileSync } from "node:fs";

export type RawRecord = { iso: string; hostMs: number; text: string };

export type MetRecord = {
  hostMs: number;
  src: "display" | "wt32";
  kind: string;
  f: Record<string, string>;
  mark: string; // most recent #MARK label before this record ("" = none)
  epoch: number; // increments at every RESET line of the same source
};

export function readRaw(path: string): RawRecord[] {
  const out: RawRecord[] = [];
  for (const line of readFileSync(path, "utf8").split("\n")) {
    if (!line) continue;
    const a = line.indexOf("\t");
    const b = a >= 0 ? line.indexOf("\t", a + 1) : -1;
    if (a < 0 || b < 0) continue;
    out.push({ iso: line.slice(0, a), hostMs: Number(line.slice(a + 1, b)), text: line.slice(b + 1) });
  }
  return out;
}

/** "k=v k2=v2" -> record. Values never contain spaces in MET lines. */
export function parseKv(s: string): Record<string, string> {
  const f: Record<string, string> = {};
  for (const tok of s.trim().split(/\s+/)) {
    const eq = tok.indexOf("=");
    if (eq > 0) f[tok.slice(0, eq)] = tok.slice(eq + 1);
  }
  return f;
}

export function parseMet(records: RawRecord[]): MetRecord[] {
  const out: MetRecord[] = [];
  let mark = "";
  const epoch = { display: 0, wt32: 0 };
  for (const r of records) {
    if (r.text.startsWith("#MARK ")) {
      mark = r.text.slice(6).trim();
      continue;
    }
    let src: "display" | "wt32" | null = null;
    let body = "";
    if (r.text.startsWith("MET ")) {
      src = "display";
      body = r.text.slice(4);
    } else if (r.text.startsWith("[W32] MET ")) {
      src = "wt32";
      body = r.text.slice(10);
    }
    if (!src) continue;
    const sp = body.indexOf(" ");
    const kind = sp < 0 ? body : body.slice(0, sp);
    const f = parseKv(sp < 0 ? "" : body.slice(sp + 1));
    if (kind === "RESET" && f.replay !== "1") epoch[src]++;
    out.push({ hostMs: r.hostMs, src, kind, f, mark, epoch: epoch[src] });
  }
  return out;
}

/** Inbound link lines echoed by the display metrics build. */
export function parseLinkIn(records: RawRecord[]): { hostMs: number; word: string; f: Record<string, string>; raw: string }[] {
  const out = [];
  for (const r of records) {
    if (!r.text.startsWith("[LINK<] ")) continue;
    const line = r.text.slice(8);
    const sp = line.indexOf(" ");
    const word = sp < 0 ? line : line.slice(0, sp);
    out.push({ hostMs: r.hostMs, word, f: parseKv(sp < 0 ? "" : line.slice(sp + 1)), raw: line });
  }
  return out;
}

/** Parses a SENS argument string into per-bottle values (b1..b4). */
export function parseSens(raw: string): { seq?: number; bottles: Record<number, Record<string, number>> } {
  const bottles: Record<number, Record<string, number>> = {};
  let cur = -1;
  let seq: number | undefined;
  for (const tok of raw.split(/\s+/).slice(1)) {
    const m = /^b([1-9]\d*)$/.exec(tok);
    if (m) {
      cur = Number(m[1]);
      bottles[cur] = {};
      continue;
    }
    const eq = tok.indexOf("=");
    if (eq <= 0) continue;
    const k = tok.slice(0, eq);
    const v = Number(tok.slice(eq + 1)); // "nan" -> NaN
    if (cur < 0) {
      if (k === "seq") seq = v;
      continue;
    }
    bottles[cur]![k] = v;
  }
  return { seq, bottles };
}
