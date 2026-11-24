#!/usr/bin/env bun
/**
 * Runs the SQL check suites in verification/sql/ and saves the results.
 *
 *   export DATABASE_URL='postgresql://postgres:<password>@db.<ref>.supabase.co:5432/postgres?sslmode=require'
 *   bun verification/db-check.ts --suite integrity --device 0 --exp 4 --interval-s 30
 *   bun verification/db-check.ts --suite security
 *   bun verification/db-check.ts --suite performance --device 0 --exp 4
 *   bun verification/db-check.ts --suite all --device 0 --exp 4 --interval-s 30
 *
 * Everything runs inside one READ ONLY transaction that is rolled back, so the
 * tool cannot change the database. DATABASE_URL is read from the environment
 * only; this tool never opens a .env file.
 *
 * Output: <out>/db-<suite>.md and .json (default verification/runs/<stamp>-db).
 */
import { SQL } from "bun";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { parseArgs } from "./lib/stats";

const REPO = resolve(import.meta.dir, "..");
const { flags } = parseArgs(process.argv.slice(2));
const one = (k: string, d?: string) => flags[k]?.at(-1) ?? d;
const suite = one("suite", "integrity")!;
const suites = suite === "all" ? ["integrity", "security", "performance"] : [suite];
const url = process.env.DATABASE_URL;
if (!url || flags.help) {
  console.log("Usage: DATABASE_URL=... bun verification/db-check.ts --suite integrity|security|performance|all [--device 0] [--exp N] [--interval-s S] [--out dir]");
  process.exit(url ? 0 : 1);
}

// Only integers are substituted, so the text replacement cannot inject SQL.
const vars: Record<string, string> = {};
for (const [flag, name] of [["device", "device"], ["exp", "exp"], ["interval-s", "interval_s"]] as const) {
  const v = one(flag);
  if (v === undefined) continue;
  if (!/^\d+$/.test(v)) throw new Error(`--${flag} must be a non-negative integer`);
  vars[name] = v;
}
vars.device ??= "0";

type Block = { name: string; doc: string; text: string };
function blocks(file: string): Block[] {
  const out: Block[] = [];
  let cur: Block | null = null;
  for (const line of readFileSync(file, "utf8").split("\n")) {
    const n = /^-- name:\s*(\S+)/.exec(line);
    if (n) {
      if (cur) out.push(cur);
      cur = { name: n[1]!, doc: "", text: "" };
      continue;
    }
    if (!cur) continue;
    const d = /^-- doc:\s*(.*)$/.exec(line);
    if (d) cur.doc = d[1]!;
    else if (!line.startsWith("--")) cur.text += line + "\n";
  }
  if (cur) out.push(cur);
  return out;
}

function substitute(text: string): string | null {
  let missing: string | null = null;
  const t = text.replace(/:([a-z_]+)\b/g, (m, name: string, offset: number, all: string) => {
    if (all[offset - 1] === ":") return m; // ::cast
    if (!(name in vars)) {
      missing = name;
      return m;
    }
    return vars[name]!;
  });
  return missing ? null : t.replace(/;\s*$/, "");
}

const stamp = new Date().toISOString().replace(/[:]/g, "").replace(/\..*/, "Z");
const outDir = resolve(one("out", join(REPO, "verification", "runs", `${stamp}-db`))!);
mkdirSync(outDir, { recursive: true });
const sql = new SQL(url);

const cell = (v: unknown) => (v === null || v === undefined ? "" : v instanceof Date ? v.toISOString() : String(v)).replace(/\|/g, "\\|").replace(/\n/g, " ");

for (const s of suites) {
  const md: string[] = [`# DB check: ${s}`, "", `Run ${new Date().toISOString()}; variables ${JSON.stringify(vars)}.`, ""];
  const json: Record<string, unknown> = { suite: s, vars, at: new Date().toISOString() };
  for (const b of blocks(join(import.meta.dir, "sql", `${s}.sql`))) {
    md.push(`## ${b.name}`, "", b.doc, "");
    const text = substitute(b.text);
    if (text === null) {
      md.push("_Skipped: a required variable was not given._", "");
      continue;
    }
    try {
      const rows: Record<string, unknown>[] = await sql.begin(async (tx) => {
        await tx.unsafe("set transaction read only");
        const r = await tx.unsafe(text);
        throw Object.assign(new Error("rollback"), { rows: [...r] });
      }).catch((e: { rows?: Record<string, unknown>[] }) => {
        if (e.rows) return e.rows;
        throw e;
      });
      json[b.name] = rows;
      if (rows.length === 0) {
        md.push("_No rows._", "");
        continue;
      }
      const cols = Object.keys(rows[0]!);
      if (cols.length === 1 && cols[0] === "QUERY PLAN") {
        md.push("```", ...rows.map((r) => String(r["QUERY PLAN"])), "```", "");
        continue;
      }
      md.push(`| ${cols.join(" | ")} |`, `|${cols.map(() => "---").join("|")}|`);
      for (const r of rows) md.push(`| ${cols.map((c) => cell(r[c])).join(" | ")} |`);
      md.push("");
    } catch (e) {
      md.push(`_Error: ${(e as Error).message}_`, "");
      json[b.name] = { error: (e as Error).message };
    }
  }
  writeFileSync(join(outDir, `db-${s}.md`), md.join("\n") + "\n");
  writeFileSync(join(outDir, `db-${s}.json`), JSON.stringify(json, (_k, v) => (typeof v === "bigint" ? v.toString() : v), 2) + "\n");
  console.log(`Wrote ${join(outDir, `db-${s}.md`)}`);
}
await sql.close();
