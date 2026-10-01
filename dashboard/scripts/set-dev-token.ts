#!/usr/bin/env bun
/**
 * Applies DEV_0_TOKEN_HASH from dashboard/.env to a device's token_hash.
 *
 * The hash lives only in the untracked .env file, so no device credential is
 * ever committed. Run it after changing the development token:
 *
 *   bun run set-dev-token
 *   bun run set-dev-token --device-id 0
 *
 * Requires DATABASE_URL, since the value is written straight to the database:
 *
 *   export DATABASE_URL='postgresql://postgres:<password>@db.<ref>.supabase.co:5432/postgres'
 */
import { spawn } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const PROJECT_ROOT = resolve(join(dirname(fileURLToPath(import.meta.url)), ".."));

/** Same validation as the column's CHECK constraint, so bad input fails here first. */
const TOKEN_HASH_PATTERN = /^[0-9a-f]{128}$/;

type Device = { deviceId: number; tokenHash: string };

/** Reads .env without executing it, so a stray shell line cannot run code. */
function readEnvFile(path: string): Record<string, string> {
  if (!existsSync(path)) {
    throw new Error(
      `No .env found at ${path}. Add a DEV_0_TOKEN_HASH line containing the sha512 hex digest of the device token.`,
    );
  }

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

function parseDeviceId(argv: string[]): number {
  const index = argv.indexOf("--device-id");
  if (index === -1) return 0;

  const value = Number(argv[index + 1]);
  if (!Number.isInteger(value) || value < 0) {
    throw new Error("--device-id must be a non-negative whole number.");
  }
  return value;
}

/**
 * Locates an executable by scanning PATH. Bun's spawn does not resolve a bare
 * command name against PATH, so psql has to be resolved to an absolute path
 * before it is spawned.
 */
function resolveExecutable(name: string): string {
  for (const directory of (process.env.PATH ?? "").split(":")) {
    if (directory === "") continue;
    const candidate = join(directory, name);
    if (existsSync(candidate)) return candidate;
  }
  throw new Error(
    `Could not find "${name}" on your PATH.\n` +
      "  Debian/Ubuntu  sudo apt-get install postgresql-client\n" +
      "  macOS          brew install libpq",
  );
}

/**
 * Applies the hash with a single parameterised statement.
 *
 * The statement is passed via -f rather than -c because psql only expands
 * :'variable' references in a script file; with -c the placeholder reaches the
 * server literally and is a syntax error. Writing it to a temp file keeps the
 * hash out of the process arguments too, where it would be visible to anyone
 * listing processes.
 */
function applyHash(databaseUrl: string, device: Device): Promise<number> {
  const sqlFile = join(mkdtempSync(join(tmpdir(), "rusim-token-")), "apply.sql");
  writeFileSync(
    sqlFile,
    [
      "update device_data.devices",
      "set token_hash = nullif(:'token_hash', '')",
      "where device_id = :device_id;",
      "",
      "select device_id, token_hash is not null as hash_applied",
      "from device_data.devices",
      "where device_id = :device_id;",
      "",
    ].join("\n"),
    { mode: 0o600 },
  );

  return new Promise((resolvePromise, rejectPromise) => {
    const child = spawn(
      resolveExecutable("psql"),
      [databaseUrl, "-X", "-v", "ON_ERROR_STOP=1", "-v", `token_hash=${device.tokenHash}`, "-v", `device_id=${device.deviceId}`, "-f", sqlFile],
      { cwd: PROJECT_ROOT, stdio: ["ignore", "inherit", "inherit"] },
    );
    child.on("error", rejectPromise);
    child.on("close", (code) => {
      rmSync(sqlFile, { force: true });
      resolvePromise(code ?? 1);
    });
  });
}

function main(): void {
  const deviceId = parseDeviceId(process.argv.slice(2));
  const env = readEnvFile(join(PROJECT_ROOT, ".env"));
  const tokenHash = env.DEV_0_TOKEN_HASH ?? "";

  if (tokenHash === "") {
    throw new Error("DEV_0_TOKEN_HASH is not set in .env, so there is nothing to apply.");
  }
  if (!TOKEN_HASH_PATTERN.test(tokenHash)) {
    // Only the shape is reported; the value is never echoed.
    throw new Error(
      `DEV_0_TOKEN_HASH must be 128 lowercase hexadecimal characters (a sha512 hex digest), but it is ${tokenHash.length} characters long.`,
    );
  }

  const databaseUrl = process.env.DATABASE_URL;
  if (databaseUrl === undefined || databaseUrl === "") {
    throw new Error(
      "DATABASE_URL is not set, so the hash cannot be written.\n" +
        "  export DATABASE_URL='postgresql://postgres:<password>@db.<ref>.supabase.co:5432/postgres'",
    );
  }

  console.log(`Applying DEV_0_TOKEN_HASH to device ${deviceId} (the value is not printed).`);
  const code = applyHash(databaseUrl, { deviceId, tokenHash });
  code.then((exitCode) => {
    if (exitCode === 0) {
      console.log("Done. The device token can now authenticate.");
    }
    process.exit(exitCode);
  });
}

try {
  main();
} catch (error: unknown) {
  console.error(`\n${error instanceof Error ? error.message : String(error)}`);
  process.exit(1);
}