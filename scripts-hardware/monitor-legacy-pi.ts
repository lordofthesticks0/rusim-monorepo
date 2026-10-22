#!/usr/bin/env bun
/**
 * Monitor the legacy water bath master on the Raspberry Pi over SSH.
 *
 * Two sources, both served by the Flask app on the Pi:
 *
 *   default   `/status`: the parsed and merged state, one row per second.
 *
 *   --raw     `/raw`: the exact serial lines the master sent, as recorded by
 *             the Flask app's serial worker. `water-ui` keeps running and the
 *             board is not reset. The script polls with a sequence cursor, so
 *             a line appears up to one `--interval` after it was received.
 *
 * The script only needs `bun` and `ssh` on the machine that runs it. Nothing
 * platform-specific runs locally: the serial port stays with the Flask app and
 * the client only reads JSON over SSH.
 *
 * Usage:
 *   bun scripts-hardware/monitor-legacy-pi.ts
 *   bun scripts-hardware/monitor-legacy-pi.ts --raw
 *   bun scripts-hardware/monitor-legacy-pi.ts --json --duration 30
 *   bun scripts-hardware/monitor-legacy-pi.ts --host invitro@10.92.128.23
 */

type Bottle = {
  id?: number;
  ph?: number;
  methane?: number;
  temp?: number;
  co2?: number;
  press?: number;
};

type MasterStatus = {
  type?: string;
  water_temp?: number | string;
  bottle_temp_avg?: number | string;
  heater?: number | string | boolean;
  pump?: number | string | boolean;
  fan?: number | string | boolean;
  water_low?: number | string | boolean;
  water_adc?: number | string;
  system_mode?: string;
};

type FlaskStatus = {
  temp?: number;
  bottle_avg_temp?: number;
  bottles?: Bottle[];
  heater?: boolean;
  pump?: boolean;
  fan?: boolean;
  water_low?: boolean;
  system_mode?: string;
  logging?: boolean;
  packets?: { rx_slave?: number; lost?: number; loss_pct?: number };
  error?: string;
};

type RawLine = {
  seq: number;
  ts: number;
  line: string;
};

type RawResponse = {
  seq?: number;
  count?: number;
  buffered?: number;
  connected?: boolean;
  lines?: RawLine[];
};

const DEFAULT_HOST = "invitro@10.92.128.23";
const DEFAULT_INTERVAL = 1;
const PM2_APP = "water-ui";
const STATUS_URL = "http://127.0.0.1:5000/status";
const RAW_URL = "http://127.0.0.1:5000/raw";
const RAW_LIMIT = 500;

const BOTTLE_IDS = [1, 2, 3, 4];

const STATUS_HEADERS = ["time", "bath°C", "avg°C", "mode", "heat", "pump", "stir", "rx", "B1", "B2", "B3", "B4"];
const STATUS_WIDTHS = [8, 8, 7, 5, 4, 4, 4, 4, 8, 8, 8, 8];
const STATUS_ALIGN: Array<"left" | "right"> = ["left", "right", "right", "left", "left", "left", "left", "right", "right", "right", "right", "right"];

const BOTTLE_HEADERS = ["time", "bottle", "pH (V)", "CH4 (ppm)", "temp (°C)", "CO2 (ppm)", "press (Pa)"];
const BOTTLE_WIDTHS = [8, 6, 7, 9, 9, 9, 10];
const BOTTLE_ALIGN: Array<"left" | "right"> = ["left", "right", "right", "right", "right", "right", "right"];

let child: Bun.Subprocess | undefined;
let headersPrinted = 0;
let shuttingDown = false;

// ---------------------------------------------------------------------------
// Arguments
// ---------------------------------------------------------------------------

function getArg(name: string): string | undefined {
  const index = process.argv.indexOf(name);
  if (index === -1 || index + 1 >= process.argv.length) return undefined;
  return process.argv[index + 1];
}

function getNumberArg(name: string, fallback: number, { allowZero = false } = {}): number {
  const raw = getArg(name);
  if (raw === undefined) return fallback;
  const value = Number(raw);
  if (!Number.isFinite(value) || value < 0 || (!allowZero && value === 0)) {
    throw new Error(`Invalid value for ${name}: ${raw}`);
  }
  return value;
}

function hasFlag(...names: string[]): boolean {
  return names.some((name) => process.argv.includes(name));
}

// ---------------------------------------------------------------------------
// Formatting
// ---------------------------------------------------------------------------

function cell(value: string, width: number, align: "left" | "right"): string {
  return align === "left" ? value.padEnd(width) : value.padStart(width);
}

function numberCell(value: unknown, digits: number, width: number): string {
  const numeric = typeof value === "number" ? value : Number(value);
  const text = Number.isFinite(numeric) ? numeric.toFixed(digits) : "-";
  return text.padStart(width);
}

function onOff(value: unknown): string {
  return value === true || value === 1 || value === "1" ? "ON" : "OFF";
}

function clock(atSeconds?: number): string {
  const millis = atSeconds === undefined ? Date.now() : atSeconds * 1000;
  return new Date(millis).toTimeString().slice(0, 8);
}

function printStatusHeader(): void {
  console.log(STATUS_HEADERS.map((header, i) => cell(header, STATUS_WIDTHS[i], STATUS_ALIGN[i])).join("  "));
  console.log(STATUS_WIDTHS.map((width) => "-".repeat(width)).join("  "));
}

function printStatusRow(data: FlaskStatus): void {
  if (data.error) {
    console.error(`[warn] ${clock()} ${STATUS_URL} unreachable; is pm2 app ${PM2_APP} running?`);
    return;
  }

  if (headersPrinted === 0) {
    printStatusHeader();
    headersPrinted = 1;
  }

  const bottles = new Map<number, Bottle>();
  for (const bottle of data.bottles ?? []) {
    if (typeof bottle.id === "number") bottles.set(bottle.id, bottle);
  }

  const row = [
    cell(clock(), STATUS_WIDTHS[0], "left"),
    numberCell(data.temp, 2, STATUS_WIDTHS[1]),
    numberCell(data.bottle_avg_temp, 2, STATUS_WIDTHS[2]),
    cell(String(data.system_mode ?? "-"), STATUS_WIDTHS[3], "left"),
    cell(onOff(data.heater), STATUS_WIDTHS[4], "left"),
    cell(onOff(data.pump), STATUS_WIDTHS[5], "left"),
    cell(onOff(data.fan), STATUS_WIDTHS[6], "left"),
    String(data.packets?.rx_slave ?? "-").padStart(STATUS_WIDTHS[7]),
    ...BOTTLE_IDS.map((id) => numberCell(bottles.get(id)?.temp, 2, STATUS_WIDTHS[8])),
  ];

  console.log(row.join("  "));
}

function printBottleHeader(): void {
  console.log(BOTTLE_HEADERS.map((header, i) => cell(header, BOTTLE_WIDTHS[i], BOTTLE_ALIGN[i])).join("  "));
  console.log(BOTTLE_WIDTHS.map((width) => "-".repeat(width)).join("  "));
}

function printBottleBlock(bottles: Bottle[], atSeconds?: number): void {
  printBottleHeader();

  const time = clock(atSeconds);

  bottles.forEach((bottle, index) => {
    const row = [
      index === 0 ? time : "",
      String(bottle.id ?? index + 1),
      numberCell(bottle.ph, 2, BOTTLE_WIDTHS[2]),
      numberCell(bottle.methane, 1, BOTTLE_WIDTHS[3]),
      numberCell(bottle.temp, 2, BOTTLE_WIDTHS[4]),
      numberCell(bottle.co2, 0, BOTTLE_WIDTHS[5]),
      numberCell(bottle.press, 0, BOTTLE_WIDTHS[6]),
    ];

    console.log(row.map((value, i) => cell(value, BOTTLE_WIDTHS[i], BOTTLE_ALIGN[i])).join("  "));
  });
}

function printMasterRow(status: MasterStatus, atSeconds?: number): void {
  const parts = [
    `${clock(atSeconds)}  master`,
    `bath ${numberCell(status.water_temp, 2, 6)}`,
    `avg ${numberCell(status.bottle_temp_avg, 2, 6)}`,
    String(status.system_mode ?? "-"),
    `heat ${onOff(status.heater)}`,
    `pump ${onOff(status.pump)}`,
    `stir ${onOff(status.fan)}`,
    `water ${onOff(status.water_low) === "ON" ? "LOW" : "OK"}`,
    `adc ${String(status.water_adc ?? "-")}`,
  ];

  console.log(parts.join("  "));
}

function handleRawLine(line: string, atSeconds?: number): void {
  const clean = line.replace(/\x1b\[[0-9;]*m/g, "").trim();
  if (!clean) return;

  let message: MasterStatus & { bottles?: Bottle[]; status?: string };
  try {
    message = JSON.parse(clean);
  } catch {
    console.log(clean);
    return;
  }

  if (message.type === "master_status") {
    printMasterRow(message, atSeconds);
    return;
  }

  if (Array.isArray(message.bottles)) {
    printBottleBlock(message.bottles, atSeconds);
    return;
  }

  if (typeof message.status === "string") {
    console.log(`[status] ${message.status}`);
    return;
  }

  console.log(clean);
}

function handleStatusLine(line: string, json: boolean): void {
  const clean = line.trim();
  if (!clean) return;

  if (json) {
    console.log(clean);
    return;
  }

  let data: FlaskStatus;
  try {
    data = JSON.parse(clean) as FlaskStatus;
  } catch {
    console.log(clean);
    return;
  }

  printStatusRow(data);
}

// ---------------------------------------------------------------------------
// SSH plumbing
// ---------------------------------------------------------------------------

type RemoteResult = { code: number; stdout: string; stderr: string };

async function runRemote(host: string, command: string): Promise<RemoteResult> {
  // The raw mode polls once per interval. Fail fast instead of waiting for the
  // default TCP timeout or a password prompt that has nowhere to go.
  const proc = Bun.spawn(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", host, command], {
    stdin: "inherit",
    stdout: "pipe",
    stderr: "pipe",
  });
  child = proc;

  try {
    const [stdout, stderr, code] = await Promise.all([
      new Response(proc.stdout).text(),
      new Response(proc.stderr).text(),
      proc.exited,
    ]);

    return { code, stdout: stdout.trim(), stderr: stderr.trim() };
  } finally {
    if (child === proc) child = undefined;
  }
}

async function streamRemote(
  host: string,
  command: string,
  onLine: (line: string) => void,
): Promise<void> {
  const proc = Bun.spawn(["ssh", host, command], {
    stdin: "inherit",
    stdout: "pipe",
    stderr: "pipe",
  });
  child = proc;

  // Surface ssh's own errors (auth failure, unknown host key) as they arrive.
  void (async () => {
    const decoder = new TextDecoder();
    for await (const chunk of proc.stderr) {
      const text = decoder.decode(chunk, { stream: true }).trim();
      if (text) console.error(`[ssh] ${text}`);
    }
  })();

  const decoder = new TextDecoder();
  let buffer = "";

  for await (const chunk of proc.stdout) {
    buffer += decoder.decode(chunk, { stream: true });
    const lines = buffer.split(/\r?\n/);
    buffer = lines.pop() ?? "";
    for (const line of lines) onLine(line);
  }

  buffer += decoder.decode();
  if (buffer.length > 0) onLine(buffer);

  const code = await proc.exited;
  child = undefined;

  if (code !== 0 && !shuttingDown) {
    throw new Error(`ssh exited with code ${code}`);
  }
}

// ---------------------------------------------------------------------------
// Modes
// ---------------------------------------------------------------------------

function statusCommand(interval: number): string {
  return [
    "while :; do",
    `  curl -fs --max-time 5 ${STATUS_URL} || echo '{"error":"status-unreachable"}'`,
    "  echo",
    `  sleep ${interval}`,
    "done",
  ].join("\n");
}

async function fetchRaw(host: string, query: string): Promise<RawResponse> {
  const result = await runRemote(host, `curl -fs --max-time 5 '${RAW_URL}${query}'`);

  if (result.code !== 0) {
    throw new Error(result.stderr || `curl exited with code ${result.code}`);
  }

  return JSON.parse(result.stdout) as RawResponse;
}

async function runStatus(host: string, interval: number, json: boolean): Promise<void> {
  console.error(`Polling ${STATUS_URL} on ${host} every ${interval}s. Ctrl+C stops.`);
  await streamRemote(host, statusCommand(interval), (line) => handleStatusLine(line, json));
}

async function runRaw(host: string, interval: number, json: boolean): Promise<void> {
  console.error(`Reading ${RAW_URL} on ${host} every ${interval}s. Ctrl+C stops.`);
  console.error(`This reads the ${PM2_APP} buffer; the service keeps running and the board is not reset.`);

  let after: number | null = null;
  let continuous = false;
  let lastConnected: boolean | null = null;
  let warnedUnreachable = false;

  while (!shuttingDown) {
    const query = after === null ? "?limit=1" : `?after=${after}&limit=${RAW_LIMIT}`;
    let data: RawResponse;

    try {
      data = await fetchRaw(host, query);
    } catch (error) {
      if (shuttingDown) break;

      const message = error instanceof Error ? error.message : String(error);
      if (!warnedUnreachable) {
        console.error(`[warn] ${clock()} cannot read ${RAW_URL}: ${message}`);
        warnedUnreachable = true;
      }

      await Bun.sleep(interval * 1000);
      continue;
    }

    if (shuttingDown) break;
    warnedUnreachable = false;

    if (typeof data.seq !== "number" || !Array.isArray(data.lines)) {
      console.error(`[warn] ${clock()} unexpected response from ${RAW_URL}`);
      await Bun.sleep(interval * 1000);
      continue;
    }

    if (typeof data.connected === "boolean" && data.connected !== lastConnected) {
      if (lastConnected !== null) {
        console.error(`[warn] ${clock()} serial link ${data.connected ? "up" : "down"}`);
      }
      lastConnected = data.connected;
    }

    if (after === null) {
      // Start from now: the first request only learns the newest sequence, so
      // a long-running dashboard does not dump its buffer at startup.
      after = data.seq;
      continuous = true;
    } else if (data.seq < after) {
      console.error(`[warn] ${clock()} ${PM2_APP} restarted (seq ${data.seq}); resuming from the new buffer`);
      after = 0;
      continuous = false;
    } else {
      if (continuous && data.lines.length > 0 && data.lines[0].seq > after + 1) {
        const missed = data.lines[0].seq - after - 1;
        console.error(`[warn] ${clock()} ${missed} line(s) dropped by the ${RAW_LIMIT}-line buffer`);
      }
      continuous = true;

      for (const entry of data.lines) {
        if (json) {
          console.log(entry.line);
        } else {
          handleRawLine(entry.line, entry.ts);
        }
        after = entry.seq;
      }

      if (data.lines.length === 0) {
        after = data.seq;
      }
    }

    await Bun.sleep(interval * 1000);
  }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

function shutdown(code: number): void {
  if (shuttingDown) return;
  shuttingDown = true;

  if (child) {
    try {
      child.kill();
    } catch {
      // The child already exited.
    }
  }

  process.exit(code);
}

function printHelp(): void {
  console.log(`Monitor the legacy water bath master on the Raspberry Pi over SSH

Usage:
  bun scripts-hardware/monitor-legacy-pi.ts [options]

Options:
  --raw                 Read the ${PM2_APP} /raw buffer over SSH and print every
                        received serial line. ${PM2_APP} keeps running and the
                        board is not reset. Lines arrive up to one interval
                        late.
  --json                Print each message as received, without the table.
  --interval <seconds>  Poll and read interval (default: ${DEFAULT_INTERVAL})
  --duration <seconds>  Stop after this long (default: run until Ctrl+C)
  --host <target>       SSH target (default: ${DEFAULT_HOST})
  -h, --help            Show this help

Sources:
  default   /status on the Pi, parsed state, dashboard keeps running
  --raw     /raw on the Pi, every received serial line

Examples:
  bun scripts-hardware/monitor-legacy-pi.ts
  bun scripts-hardware/monitor-legacy-pi.ts --raw --duration 20
  bun scripts-hardware/monitor-legacy-pi.ts --raw --json --interval 0.5`);
}

async function main(): Promise<void> {
  if (hasFlag("-h", "--help")) {
    printHelp();
    return;
  }

  const host = getArg("--host") ?? DEFAULT_HOST;
  const interval = getNumberArg("--interval", DEFAULT_INTERVAL);
  const duration = getNumberArg("--duration", 0, { allowZero: true });
  const json = hasFlag("--json");
  const raw = hasFlag("--raw");

  process.on("SIGINT", () => shutdown(130));
  process.on("SIGTERM", () => shutdown(143));

  if (duration > 0) {
    setTimeout(() => shutdown(0), Math.round(duration * 1000));
  }

  if (raw) {
    await runRaw(host, interval, json);
  } else {
    await runStatus(host, interval, json);
  }
}

try {
  await main();
  process.exit(0);
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.error(message);

  if (/unknown host|permission denied|connection refused|could not resolve/i.test(message)) {
    console.error(`Check that you can reach the Pi: ssh ${DEFAULT_HOST}`);
  }

  process.exit(1);
}
