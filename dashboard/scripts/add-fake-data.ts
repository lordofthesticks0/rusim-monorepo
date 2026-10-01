#!/usr/bin/env bun
/**
 * Interactive wrapper around supabase/seed_readings.sql.
 *
 * Prompts for each generation setting, then executes the SQL against the linked
 * Supabase project. Run it with:
 *
 *   bun run add-fake-data
 *
 * Non-interactive use passes the flags through instead of prompting:
 *
 *   bun run add-fake-data --experiment-id 3 --sample-count 48 --null-rate 0
 */
import { spawn } from "node:child_process";
import { createInterface } from "node:readline/promises";
import { existsSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join, resolve } from "node:path";

type Knobs = {
  deviceId: number;
  experimentId: number;
  sampleCount: number;
  sampleGapMinutes: number;
  bottleCount: number;
  nullRate: number;
  bottleTotal: number;
};

const SCRIPT_PATH = resolve(join(dirname(fileURLToPath(import.meta.url)), "..", "supabase", "seed_readings.sql"));
const PROJECT_ROOT = resolve(join(dirname(fileURLToPath(import.meta.url)), ".."));

const DEFAULTS: Knobs = {
  deviceId: 0,
  experimentId: 0,
  sampleCount: 24,
  sampleGapMinutes: 5,
  bottleCount: 8,
  nullRate: 0.0005,
  bottleTotal: 24,
};

/** Bounds kept deliberately loose; the database constrains the real values. */
type FieldRule = {
  key: keyof Knobs;
  label: string;
  hint: string;
  min: number;
  max: number;
  integer: boolean;
};

const FIELDS: FieldRule[] = [
  { key: "deviceId", label: "Device ID", hint: "0 is the development device", min: 0, max: 9999, integer: true },
  {
    key: "experimentId",
    label: "Experiment ID",
    hint: "0 is the testing experiment, 1+ are regular experiments",
    min: 0,
    max: 9999,
    integer: true,
  },
  { key: "sampleCount", label: "Number of samples", hint: "how many timestamps to generate", min: 1, max: 100_000, integer: true },
  { key: "sampleGapMinutes", label: "Minutes between samples", hint: "spacing of the time series", min: 1, max: 1440, integer: true },
  {
    key: "bottleCount",
    label: "Bottles reporting",
    hint: "bottles 0 to N-1 report; the rest are treated as disconnected",
    min: 0,
    max: 10_000,
    integer: true,
  },
  {
    key: "bottleTotal",
    label: "Total bottles on device",
    hint: "bottle_count stored on the device; the dashboard offers this many",
    min: 0,
    max: 10_000,
    integer: true,
  },
  {
    key: "nullRate",
    label: "Null rate",
    hint: "chance any single measurement is null; 0.0005 is rare, 0.02 is obvious",
    min: 0,
    max: 1,
    integer: false,
  },
];

type FlagMap = Record<string, string>;

/** Converts sampleGapMinutes to sample-gap-minutes for CLI flag matching. */
function kebabCase(value: string): string {
  return value.replace(/[A-Z]/g, (letter) => `-${letter.toLowerCase()}`);
}

function parseFlags(argv: string[]): FlagMap {
  const flags: FlagMap = {};
  for (let index = 0; index < argv.length; index += 1) {
    const token = argv[index]!;
    if (!token.startsWith("--")) continue;
    const name = token.slice(2);
    const next = argv[index + 1];
    if (next !== undefined && !next.startsWith("--")) {
      flags[name] = next;
      index += 1;
    } else {
      flags[name] = "true";
    }
  }
  return flags;
}

/**
 * Parses one answer, falling back to the default when the user just presses
 * enter. Rejects anything outside the allowed range rather than silently
 * clamping, so a typo cannot quietly produce surprising data.
 */
function parseValue(raw: string, rule: FieldRule): number {
  const value = Number(raw);
  if (!Number.isFinite(value)) {
    throw new Error(`${rule.label} must be a number, got "${raw}".`);
  }
  if (rule.integer && !Number.isInteger(value)) {
    throw new Error(`${rule.label} must be a whole number, got "${raw}".`);
  }
  if (value < rule.min || value > rule.max) {
    throw new Error(`${rule.label} must be between ${rule.min} and ${rule.max}, got ${value}.`);
  }
  return value;
}

async function promptForKnobs(): Promise<Knobs> {
  const rl = createInterface({ input: process.stdin, output: process.stdout });

  // Reading through the 'line' event rather than readline.question() keeps the
  // prompt logic identical whether stdin is a terminal or a pipe. question()
  // never settles once the stream has ended, which made piped input hang.
  const pending: string[] = [];
  let ended = false;
  let resolveLine: ((line: string) => void) | null = null;

  rl.on("line", (line) => {
    if (resolveLine) {
      const resolve = resolveLine;
      resolveLine = null;
      resolve(line);
    } else {
      pending.push(line);
    }
  });
  rl.on("close", () => {
    ended = true;
    if (resolveLine) {
      const resolve = resolveLine;
      resolveLine = null;
      resolve("");
    }
  });

  /** Next line of input, or null once the stream has ended. */
  const nextLine = (): Promise<string | null> => {
    const buffered = pending.shift();
    if (buffered !== undefined) return Promise.resolve(buffered);
    if (ended) return Promise.resolve(null);
    return new Promise((resolve) => {
      resolveLine = (line) => resolve(line === "" && ended ? null : line);
    });
  };

  const knobs = { ...DEFAULTS };

  try {
    console.log("Generating dummy readings. Press enter to accept the default in brackets.\n");
    for (const rule of FIELDS) {
      const defaultValue = DEFAULTS[rule.key];
      process.stdout.write(`${rule.label} [${defaultValue}]: ${rule.hint}\n  > `);

      const answer = await nextLine();
      if (answer === null) {
        // Input ran out, for example when piping a short list. Fall back to the
        // default so a truncated run still produces usable data.
        process.stdout.write("\n");
        knobs[rule.key] = defaultValue;
        continue;
      }

      const trimmed = answer.trim();
      knobs[rule.key] = trimmed === "" ? defaultValue : parseValue(trimmed, rule);
    }
    process.stdout.write("\n");
  } finally {
    rl.close();
  }

  return knobs;
}

function summarise(knobs: Knobs): string {
  const window = (knobs.sampleCount - 1) * knobs.sampleGapMinutes;
  return [
    `  device          ${knobs.deviceId}`,
    `  experiment      ${knobs.experimentId}${knobs.experimentId === 0 ? " (testing)" : ""}`,
    `  samples         ${knobs.sampleCount} every ${knobs.sampleGapMinutes} min (~${window} min span)`,
    `  bottles         0-${knobs.bottleCount - 1} report, ${knobs.bottleTotal} configured`,
    `  null rate       ${knobs.nullRate}`,
    `  rows to insert  ${knobs.sampleCount * Math.max(knobs.bottleCount, 0)}`,
  ].join("\n");
}

/**
 * Runs the seed script through psql, which owns the database credentials.
 *
 * A migration file is not an option here: `supabase db push` applies pending
 * migrations once and records them, so a second run would be a no-op. The
 * script needs to be re-runnable on demand, which means connecting directly.
 *
 * The connection string comes from DATABASE_URL. When it is unset this falls
 * back to the Supabase CLI, which prompts for the database password if needed.
 */
function runSeed(knobs: Knobs): Promise<number> {
  const variables = [
    `device_id=${knobs.deviceId}`,
    `experiment_id=${knobs.experimentId}`,
    `sample_count=${knobs.sampleCount}`,
    `sample_gap_minutes=${knobs.sampleGapMinutes}`,
    `bottle_count=${knobs.bottleCount}`,
    `bottle_total=${knobs.bottleTotal}`,
    `null_rate=${knobs.nullRate}`,
  ];

  const databaseUrl = process.env.DATABASE_URL;
  if (databaseUrl) {
    // -X aborts on the first error, so a failed seed cannot half-apply.
    const args = ["psql", databaseUrl, "-X", "-v", "ON_ERROR_STOP=1", "-f", SCRIPT_PATH];
    for (const variable of variables) args.push("-v", variable);
    return execute("psql", args);
  }

  console.error(
    "DATABASE_URL is not set, so the seeded rows cannot be sent to the database.\n" +
      "The SQL was prepared but nothing was written. Export a connection string and retry:\n" +
      "  export DATABASE_URL='postgresql://postgres:<password>@db.<ref>.supabase.co:5432/postgres'\n",
  );
  return Promise.resolve(1);
}

/**
 * Locates an executable by scanning PATH.
 *
 * Bun's spawn does not resolve a bare command name against PATH, so `psql` has
 * to be turned into an absolute path before it is spawned.
 */
function resolveExecutable(name: string): string | null {
  if (name.includes("/")) return existsSync(name) ? name : null;

  for (const directory of (process.env.PATH ?? "").split(":")) {
    if (directory === "") continue;
    const candidate = join(directory, name);
    if (existsSync(candidate)) return candidate;
  }
  return null;
}

function execute(command: string, args: string[]): Promise<number> {
  const binary = resolveExecutable(command);
  if (binary === null) {
    console.error(
      `\nCould not find "${command}" on your PATH.\n` +
        "Install the PostgreSQL client, for example:\n" +
        "  Debian/Ubuntu  sudo apt-get install postgresql-client\n" +
        "  macOS          brew install libpq\n",
    );
    return Promise.resolve(1);
  }

  return new Promise((resolvePromise, rejectPromise) => {
    const child = spawn(binary, args, { cwd: PROJECT_ROOT, stdio: "inherit" });
    child.on("error", rejectPromise);
    child.on("close", (code) => resolvePromise(code ?? 1));
  });
}

async function main(): Promise<void> {
  if (!existsSync(SCRIPT_PATH)) {
    throw new Error(`Seed script not found at ${SCRIPT_PATH}`);
  }

  const flags = parseFlags(process.argv.slice(2));

  // Flags are named after the CLI convention (kebab-case) and are also matched
  // against the field keys, so both --sample-count and --sampleCount work.
  const resolveFlag = (rule: FieldRule): number | undefined => {
    const raw = flags[rule.key] ?? flags[kebabCase(rule.key)];
    return raw === undefined ? undefined : parseValue(raw, rule);
  };

  const knobs = { ...DEFAULTS };
  for (const rule of FIELDS) {
    const value = resolveFlag(rule);
    if (value !== undefined) knobs[rule.key] = value;
  }

  if (Object.keys(flags).length === 0) {
    Object.assign(knobs, await promptForKnobs());
  } else {
    const unknown = Object.keys(flags).filter(
      (name) => !FIELDS.some((rule) => rule.key === name || kebabCase(rule.key) === name),
    );
    if (unknown.length > 0) {
      throw new Error(`Unknown option${unknown.length > 1 ? "s" : ""}: ${unknown.map((n) => `--${n}`).join(", ")}`);
    }
  }

  console.log(`\nAbout to generate:\n${summarise(knobs)}`);
  console.log("\nNote: this replaces all existing readings for that device, because the");
  console.log("primary key is (device_id, bottle_id, timestamp) and excludes experiment_id.\n");

  const code = await runSeed(knobs);
  process.exit(code);
}

main().catch((error: unknown) => {
  console.error(`\n${error instanceof Error ? error.message : String(error)}`);
  process.exit(1);
});