#!/usr/bin/env bun
/**
 * Single entry point for preparing the development database.
 *
 * It does both jobs that used to be two scripts, in one connection and one
 * transaction:
 *
 *   1. Applies DEV_0_TOKEN_HASH from .env to a device's token_hash, so no
 *      credential is ever committed to the repository.
 *   2. Generates dummy readings so the dashboard has something to display
 *      while no firmware is writing to the database.
 *
 * Usage:
 *   bun run dev-db                 interactive, prompts for every setting
 *   bun run dev-db --sample-count 48 --null-rate 0.02
 *   bun run dev-db --only token    apply the token and stop
 *   bun run dev-db --only readings skip the token
 *
 * Requires DATABASE_URL. Bun's own Postgres client is used, so psql does not
 * need to be installed:
 *   export DATABASE_URL='postgresql://postgres:<password>@db.<ref>.supabase.co:5432/postgres'
 *
 * A migration is not an option for the readings: `supabase db push` applies
 * pending migrations once and records them, so a second run would be a no-op.
 * This script connects directly so it stays re-runnable on demand.
 */
import { SQL } from "bun";
import { createInterface } from "node:readline/promises";
import { existsSync, readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join, resolve } from "node:path";

const PROJECT_ROOT = resolve(join(dirname(fileURLToPath(import.meta.url)), ".."));
const ENV_PATH = join(PROJECT_ROOT, ".env");

/** Same shape as the column's CHECK constraint, so bad input fails here first. */
const TOKEN_HASH_PATTERN = /^[0-9a-f]{128}$/;

type Knobs = {
  deviceId: number;
  experimentId: number;
  sampleCount: number;
  sampleGapMinutes: number;
  bottleCount: number;
  nullRate: number;
  bottleTotal: number;
};

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

/** Converts sampleGapMinutes to sample-gap-minutes for CLI flag matching. */
function kebabCase(value: string): string {
  return value.replace(/[A-Z]/g, (letter) => `-${letter.toLowerCase()}`);
}

type FlagMap = Record<string, string>;

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

/** Reads .env without executing it, so a stray shell line cannot run code. */
function readEnvFile(path: string): Record<string, string> {
  if (!existsSync(path)) return {};

  const values: Record<string, string> = {};
  for (const line of readFileSync(path, "utf8").split("\n")) {
    const trimmed = line.trim();
    if (trimmed === "" || trimmed.startsWith("#")) continue;
    const separator = trimmed.indexOf("=");
    if (separator === -1) continue;
    values[trimmed.slice(0, separator).trim()] = trimmed
      .slice(separator + 1)
      .trim()
      .replace(/^["']|["']$/g, "");
  }
  return values;
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
 * Supabase requires TLS for anything that is not a local socket, while a local
 * `supabase start` database is plain Postgres on loopback. Guessing from the
 * host keeps the common cases working without extra flags; --tls and --no-tls
 * override the guess.
 */
function shouldUseTls(databaseUrl: string, override: boolean | undefined): boolean {
  if (override !== undefined) return override;
  let hostname: string;
  try {
    hostname = new URL(databaseUrl).hostname;
  } catch {
    return true;
  }
  return !(hostname === "localhost" || hostname === "127.0.0.1" || hostname === "::1" || hostname === "[::1]");
}

/**
 * Points the device at its dummy data.
 *
 * Scoped to a single experiment. The timestamps below give each experiment a
 * window that starts one full span further into the past than the previous
 * experiment's, so windows are disjoint and this delete cannot touch another
 * experiment.
 *
 * Regenerating with a different sample_count changes an experiment's span and
 * therefore shifts its window. That is the one case the window can overlap a
 * neighbour's, and it fails loudly on the primary key rather than overwriting,
 * which is the right outcome: a device cannot genuinely report two experiments
 * for one bottle at the same instant. Keep sample_count fixed across runs for a
 * given device, or delete that device's readings before changing it.
 */
const DELETE_EXPERIMENT = `
delete from device_data.readings
where device_id = $1
  and experiment_id = $2;
`.trim();

/**
 * Generates one row per bottle per sample.
 *
 * Every knob is a bound parameter rather than interpolated text, so no value
 * can reach the server as SQL.
 *
 * Timestamp layout, newest first. Experiment 0, the testing experiment, ends at
 * the current minute so it reads as live. Each higher experiment id is shifted
 * one full experiment span further into the past, which guarantees two
 * experiments on the same device never share a timestamp.
 *
 * The stride has to be the whole span, sample_count * sample_gap_minutes, not
 * the 5 minutes between samples. A 5 minute stride looks right for a single
 * sample per experiment but makes neighbouring experiments overlap almost
 * completely: with 24 samples every 5 minutes, experiment 1's newest sample
 * would land on experiment 0's second-newest, and the two would collide on the
 * primary key (device_id, bottle_id, timestamp). Stepping by a full span keeps
 * every experiment's window disjoint.
 */
const INSERT_READINGS = `
insert into device_data.readings (
  device_id,
  bottle_id,
  experiment_id,
  "timestamp",
  ph,
  pressure,
  temp,
  co2,
  ch4
)
with settings as (
  select
    $1::integer as device_id,
    $2::integer as experiment_id,
    $3::integer as sample_count,
    $4::integer as sample_gap_minutes,
    $5::integer as bottle_count,
    $6::double precision as null_rate
),
samples as (
  select
    generate_series(0, settings.sample_count - 1) as sample_offset,
    settings.device_id,
    settings.experiment_id,
    settings.sample_count,
    settings.sample_gap_minutes,
    settings.bottle_count,
    settings.null_rate
  from settings
),
timed as (
  select
    date_trunc('minute', now())
      - make_interval(mins => experiment_id * (sample_count * sample_gap_minutes))
      - make_interval(mins => (sample_count - 1 - sample_offset) * sample_gap_minutes) as sampled_at,
    device_id,
    experiment_id,
    sample_count,
    sample_gap_minutes,
    bottle_count,
    null_rate
  from samples
),
expanded as (
  select
    bottle.bottle_id,
    timed.device_id,
    timed.experiment_id,
    timed.sampled_at,
    timed.null_rate
  from timed
  cross join lateral generate_series(0, timed.bottle_count - 1) as bottle(bottle_id)
)
select
  expanded.device_id,
  expanded.bottle_id,
  expanded.experiment_id,
  expanded.sampled_at as "timestamp",

  -- Each bottle gets a stable offset from the others so the bottles stay
  -- visually distinguishable across samples instead of being pure noise.
  case when random() < expanded.null_rate then null else
    round((4.20 + expanded.bottle_id * 0.11 + random() * 0.35)::numeric, 4)::real
  end as ph,

  case when random() < expanded.null_rate then null else
    round((98.5 + expanded.bottle_id * 0.28 + random() * 1.4)::numeric, 4)::real
  end as pressure,

  case when random() < expanded.null_rate then null else
    round((17.5 + expanded.bottle_id * 0.42 + random() * 2.2)::numeric, 4)::real
  end as temp,

  case when random() < expanded.null_rate then null else
    round((240 + expanded.bottle_id * 55 + random() * 160)::numeric, 2)::real
  end as co2,

  case when random() < expanded.null_rate then null else
    round((12 + expanded.bottle_id * 6 + random() * 40)::numeric, 4)::real
  end as ch4
from expanded;
`.trim();

/** Summary of what landed, printed so a silent no-op cannot go unnoticed. */
const SUMMARISE = `
select
  count(*)                                  as total_rows,
  count(distinct bottle_id)                 as bottles_reporting,
  count(*) filter (where ph is null)        as null_ph,
  count(*) filter (where ch4 is null)       as null_ch4,
  min("timestamp")                          as first_sample,
  max("timestamp")                          as last_sample
from device_data.readings
where device_id = $1
  and experiment_id = $2;
`.trim();

/**
 * Bottles the device claims to have but which never reported. The dashboard
 * reads this absence as "disconnected", which is a different condition from a
 * row whose measurement is null.
 */
const MISSING_BOTTLES = `
select missing.bottle_id
from device_data.devices d
cross join lateral generate_series(0, d.bottle_count - 1) as missing(bottle_id)
where d.device_id = $1
  and not exists (
    select 1
    from device_data.readings r
    where r.device_id = d.device_id
      and r.bottle_id = missing.bottle_id
  )
order by missing.bottle_id;
`.trim();

/**
 * Makes sure the target device exists, so the readings have a parent row. Its
 * bottle_count becomes the number of bottles the dashboard offers, of which only
 * the first bottle_count actually report. The token hash is left alone: it is
 * owned by setDevToken and must survive a reseed.
 */
const UPSERT_DEVICE = `
insert into device_data.devices (device_id, bottle_count, owner_name, owner_email)
values ($1, $2, 'RUSIM team', 'example@example.com')
on conflict (device_id) do update
set bottle_count = excluded.bottle_count;
`.trim();

/**
 * Stores the digest only. The plaintext token exists solely in the operator's
 * hands, and the hash is a bound parameter, so it never lands in a query log.
 */
const STORE_TOKEN = `
update device_data.devices
set token_hash = $2
where device_id = $1;
`.trim();

type SummaryRow = {
  total_rows: string | number;
  bottles_reporting: string | number;
  null_ph: string | number;
  null_ch4: string | number;
  first_sample: Date | string;
  last_sample: Date | string;
};

async function seedReadings(db: SQL, knobs: Knobs): Promise<void> {
  await db.transaction(async (tx) => {
    await tx.unsafe(UPSERT_DEVICE, [knobs.deviceId, knobs.bottleTotal]);
    await tx.unsafe(DELETE_EXPERIMENT, [knobs.deviceId, knobs.experimentId]);
    await tx.unsafe(INSERT_READINGS, [
      knobs.deviceId,
      knobs.experimentId,
      knobs.sampleCount,
      knobs.sampleGapMinutes,
      knobs.bottleCount,
      knobs.nullRate,
    ]);

    const [summary] = (await tx.unsafe(SUMMARISE, [knobs.deviceId, knobs.experimentId])) as SummaryRow[];
    const missing = (await tx.unsafe(MISSING_BOTTLES, [knobs.deviceId])) as { bottle_id: number }[];

    const format = (value: Date | string): string =>
      value instanceof Date ? value.toISOString().replace("T", " ").slice(0, 16) : String(value);

    console.log(`\n  rows written     ${summary?.total_rows ?? 0}`);
    console.log(`  bottles          ${summary?.bottles_reporting ?? 0} reporting`);
    console.log(`  first sample     ${format(summary!.first_sample)} UTC`);
    console.log(`  last sample      ${format(summary!.last_sample)} UTC`);
    console.log(`  null ph / ch4    ${summary?.null_ph ?? 0} / ${summary?.null_ch4 ?? 0}`);
    if (missing.length > 0) {
      console.log(`  disconnected     ${missing.map((row) => row.bottle_id).join(", ")}`);
    }
  });
}

async function setDevToken(db: SQL, deviceId: number, tokenHash: string): Promise<void> {
  // Bun's SQL resolves to a plain array of row objects, empty when nothing matched.
  const exists = await db.unsafe<{ present: number }[]>(
    "select 1 as present from device_data.devices where device_id = $1 limit 1;",
    [deviceId],
  );
  if (exists.length === 0) {
    throw new Error(
      `Device ${deviceId} does not exist, so no token was stored. Run the readings step first, or create the device, then retry.`,
    );
  }

  // Checked before the write so the unique-constraint violation becomes a
  // readable message. Otherwise the update fails after the readings have
  // already committed, which looks like the seed was lost when it was not.
  const owner = await db.unsafe<{ device_id: number }[]>(
    "select device_id from device_data.devices where token_hash = $2 and device_id <> $1;",
    [deviceId, tokenHash],
  );
  if (owner.length > 0) {
    const ownerId = owner[0]!.device_id;
    throw new Error(
      `That token already belongs to device ${ownerId}. Each device needs its own token, because the hash is\n` +
        `unique across the table. To move it to device ${deviceId}, clear it first:\n` +
        `  bun run dev-db --only token --device-id ${ownerId} --token-hash ""`,
    );
  }

  await db.unsafe(STORE_TOKEN, [deviceId, tokenHash]);
  console.log(`\n  token applied to device ${deviceId} (the value is never printed).`);
}

function resolveTokenHash(env: Record<string, string>, flags: FlagMap): string | null {
  const explicit = flags["token-hash"];

  // An explicit empty value means "clear the token". This is the escape hatch
  // for handing a token to a different device, since the hash is unique.
  if (explicit !== undefined && explicit.trim() === "") {
    return "";
  }

  // A real environment variable wins over .env. Bun loads .env into process.env
  // on startup, so without this an exported value would be silently ignored in
  // favour of the file's copy.
  const value = explicit ?? process.env.DEV_0_TOKEN_HASH ?? env.DEV_0_TOKEN_HASH ?? "";

  if (value === "") {
    console.log(
      "\n  DEV_0_TOKEN_HASH is not set in .env, so the device token was left unchanged.\n" +
        "  Generate one with: printf 'your-token' | sha512sum | cut -d' ' -f1",
    );
    return null;
  }
  if (!TOKEN_HASH_PATTERN.test(value)) {
    // Only the shape is reported; the value is never echoed.
    throw new Error(
      `The token hash must be 128 lowercase hexadecimal characters (a sha512 hex digest), but it is ${value.length} characters long.`,
    );
  }
  return value;
}

function printHelp(): void {
  console.log(
    [
      "Prepares the development database: applies the device token and generates dummy readings.",
      "",
      "Usage:",
      "  bun run dev-db                        interactive, prompts for every setting",
      "  bun run dev-db --only token           only apply the token",
      "  bun run dev-db --only readings        only generate readings",
      "",
      "Reading settings:",
      ...FIELDS.map(
        (rule) =>
          `  --${kebabCase(rule.key).padEnd(20)}${rule.label} (default ${DEFAULTS[rule.key]}, ${rule.hint})`,
      ),
      "",
      "Other options:",
      "  --only <token|readings>  skip the other half of the work",
      "  --token-hash <hex>       override DEV_0_TOKEN_HASH from .env",
      '  --token-hash ""          clear the device token, freeing the hash for another device',
      "  --tls / --no-tls         override the TLS guess from the database host",
      "  --allow-remote           required to run against any non-loopback host (prod guard)",
      "  --help                   show this message",
      "",
      "Requires DATABASE_URL, either exported or set in .env.",
    ].join("\n"),
  );
}

async function main(): Promise<void> {
  const argv = process.argv.slice(2);
  const flags = parseFlags(argv);

  if (flags.help !== undefined) {
    printHelp();
    return;
  }

  const knownFlags = new Set<string>([
    ...FIELDS.map((rule) => rule.key),
    ...FIELDS.map((rule) => kebabCase(rule.key)),
    "only",
    "token-hash",
    "tls",
    "no-tls",
    "allow-remote",
    "help",
  ]);
  const unknown = Object.keys(flags).filter((name) => !knownFlags.has(name));
  if (unknown.length > 0) {
    throw new Error(
      `Unknown option${unknown.length > 1 ? "s" : ""}: ${unknown.map((n) => `--${n}`).join(", ")}\nRun with --help to see the available options.`,
    );
  }

  const only = flags["only"];
  if (only !== undefined && only !== "token" && only !== "readings") {
    throw new Error(`--only must be either "token" or "readings", got "${only}".`);
  }
  if (flags.tls !== undefined && flags["no-tls"] !== undefined) {
    throw new Error("--tls and --no-tls cannot both be given.");
  }

  const env = readEnvFile(ENV_PATH);

  // Bun loads .env into process.env already, but reading the file directly means
  // the script behaves the same when launched from another directory.
  const databaseUrl = process.env.DATABASE_URL ?? env.DATABASE_URL;
  if (databaseUrl === undefined || databaseUrl === "") {
    throw new Error(
      "DATABASE_URL is not set, so nothing can be written. Export a connection string and retry:\n" +
        "  export DATABASE_URL='postgresql://postgres:<password>@db.<ref>.supabase.co:5432/postgres'\n" +
        "  or add DATABASE_URL to dashboard/.env",
    );
  }

  // Safety guard: this script DELETEs an experiment's readings and overwrites
  // token hashes, so it must never run against production by accident. Remote
  // hosts require an explicit opt-in; local loopback always works.
  try {
    const hostname = new URL(databaseUrl).hostname;
    const isLocal = hostname === "localhost" || hostname === "127.0.0.1" || hostname === "::1" || hostname === "[::1]";
    if (!isLocal && flags["allow-remote"] === undefined) {
      throw new Error(
        `Refusing to run against remote host "${hostname}": this script deletes and rewrites data.\n` +
          "If you really mean it (e.g. seeding a staging project), re-run with --allow-remote.",
      );
    }
  } catch (error) {
    if (error instanceof Error && error.message.startsWith("Refusing to run against remote host")) throw error;
    // Unparseable URL: let the SQL driver report it below rather than masking it.
  }

  const knobs = { ...DEFAULTS };
  for (const rule of FIELDS) {
    const raw = flags[rule.key] ?? flags[kebabCase(rule.key)];
    if (raw !== undefined) knobs[rule.key] = parseValue(raw, rule);
  }

  // Only prompt when no flags were passed at all, so a piped script with a
  // couple of overrides does not block waiting for input.
  if (Object.keys(flags).length === 0) {
    Object.assign(knobs, await promptForKnobs());
  }

  const tlsOverride = flags["no-tls"] !== undefined ? false : flags.tls !== undefined ? true : undefined;
  const db = new SQL(databaseUrl, { tls: shouldUseTls(databaseUrl, tlsOverride) });

  try {
    const wantReadings = only !== "token";
    const wantToken = only !== "readings";

    if (wantReadings) {
      console.log(`\nAbout to generate:\n${summarise(knobs)}`);
      console.log("\nThis replaces the readings for that experiment. Other experiments are untouched,");
      console.log("because each one occupies its own window of timestamps.");
    }

    if (wantReadings) {
      await seedReadings(db, knobs);
    }

    // Runs after the readings step because that is what creates the device row
    // on a fresh database, and a token needs somewhere to live. Upserting first
    // when only the token was asked for keeps `--only token` usable on its own.
    if (wantToken) {
      const tokenHash = resolveTokenHash(env, flags);
      if (tokenHash !== null) {
        if (!wantReadings) {
          await db.unsafe(UPSERT_DEVICE, [knobs.deviceId, knobs.bottleTotal]);
        }
        if (tokenHash === "") {
          // Clearing only makes sense on a device that exists, since the flag is
          // mainly used to free a hash for another device.
          await db.unsafe("update device_data.devices set token_hash = null where device_id = $1;", [knobs.deviceId]);
          console.log(`\n  token cleared on device ${knobs.deviceId}.`);
        } else {
          await setDevToken(db, knobs.deviceId, tokenHash);
        }
      }
    }
  } finally {
    await db.end();
  }
}

main().catch((error: unknown) => {
  console.error(`\n${error instanceof Error ? error.message : String(error)}`);
  process.exit(1);
});
