#!/usr/bin/env bun
/**
 * Read the JSON telemetry from legacy/slave/new-slave.cpp and print it as a
 * per-bottle table.
 *
 * The slave emits one line per sample on USB serial at 9600 baud:
 *   {"bottles":[{"id":1,"ph":2.51,"methane":812.4,"temp":38.5,"co2":1204,"press":1013.2}, ...]}
 *
 * Opening the serial port usually resets the Mega, so the first messages are
 * full of the firmware's invalid markers (-1 / -999) while the cache fills.
 * The wrapper discards that startup window before printing.
 *
 * Usage:
 *   bun scripts-hardware/read-legacy-slave.ts                  # autodetect port
 *   bun scripts-hardware/read-legacy-slave.ts --port /dev/ttyUSB0
 *   bun scripts-hardware/read-legacy-slave.ts --settle 8
 *   pio device monitor -e legacy-slave --quiet --raw | \
 *     bun scripts-hardware/read-legacy-slave.ts --stdin
 *
 * Values are printed exactly as the firmware sent them. `-1` (pH, methane, CO2,
 * pressure) and `-999` (temperature) are the firmware's invalid markers and
 * are not hidden or translated.
 */

import { createReadStream } from "node:fs";

type Bottle = {
  id?: number;
  ph?: number;
  methane?: number;
  temp?: number;
  co2?: number;
  press?: number;
};

type Telemetry = {
  bottles?: Bottle[];
  status?: string;
};

type Device = { port: string; description?: string; hwid?: string };

const DEFAULT_BAUD = 9600;
const DEFAULT_SETTLE_SECONDS = 5;
// Wait this long after the last complete line before parsing. Serial data
// arrives in chunks; this lets the monitor finish delivering a frame.
const FRAME_QUIET_MS = 100;

const HEADERS = ["time", "bottle", "pH (V)", "CH4 (ppm)", "temp (°C)", "CO2 (ppm)", "press (Pa)"];
const WIDTHS = [8, 6, 7, 9, 9, 9, 10];
const ALIGN: Array<"left" | "right"> = ["left", "right", "right", "right", "right", "right", "right"];

let headerPrinted = false;

function getArg(name: string): string | undefined {
  const idx = process.argv.indexOf(name);
  if (idx === -1 || idx + 1 >= process.argv.length) return undefined;
  return process.argv[idx + 1];
}

function getNumberArg(name: string, fallback: number): number {
  const raw = getArg(name);
  if (raw === undefined) return fallback;
  const value = Number(raw);
  if (!Number.isFinite(value) || value < 0) {
    throw new Error(`Invalid value for ${name}: ${raw}`);
  }
  return value;
}

function hasFlag(...names: string[]): boolean {
  return names.some((name) => process.argv.includes(name));
}

function formatCell(value: string, width: number, align: "left" | "right"): string {
  return align === "left" ? value.padEnd(width) : value.padStart(width);
}

function formatNumber(value: unknown, digits: number): string {
  if (typeof value !== "number" || !Number.isFinite(value)) return String(value);
  return value.toFixed(digits);
}

function formatInt(value: unknown): string {
  if (typeof value !== "number" || !Number.isFinite(value)) return String(value);
  return String(Math.round(value));
}

function printHeader(): void {
  const line = HEADERS.map((header, i) => formatCell(header, WIDTHS[i], ALIGN[i])).join("  ");
  console.log(line);
  console.log(WIDTHS.map((width) => "-".repeat(width)).join("  "));
  headerPrinted = true;
}

function printBottles(bottles: Bottle[]): void {
  if (!headerPrinted) printHeader();

  const time = new Date().toTimeString().slice(0, 8);

  bottles.forEach((bottle, index) => {
    const row = [
      index === 0 ? time : "",
      String(bottle.id ?? index + 1),
      formatNumber(bottle.ph, 2),
      formatNumber(bottle.methane, 1),
      formatNumber(bottle.temp, 2),
      formatInt(bottle.co2),
      formatNumber(bottle.press, 0),
    ];

    console.log(row.map((cell, i) => formatCell(cell, WIDTHS[i], ALIGN[i])).join("  "));
  });
}

function printLine(line: string): void {
  // pio monitor with --raw should be clean, but strip any ANSI escapes that
  // survive a reconnect or a terminal filter.
  const clean = line.replace(/\x1b\[[0-9;]*m/g, "").trim();
  if (!clean) return;

  let message: Telemetry;
  try {
    message = JSON.parse(clean) as Telemetry;
  } catch {
    // Bootloader noise, partial frames, or a PING reply split across chunks.
    console.log(clean);
    return;
  }

  if (Array.isArray(message.bottles)) {
    printBottles(message.bottles);
    return;
  }

  if (typeof message.status === "string") {
    // The board prints this once after setup(). A reset or reconnect prints it
    // again, so show the table header again on the next sample.
    headerPrinted = false;
    console.log(`[status] ${message.status}`);
    return;
  }

  console.log(JSON.stringify(message, null, 2));
}

function processChunk(
  chunk: Uint8Array,
  decoder: TextDecoder,
  buffer: { text: string },
  onLine: (line: string) => void = printLine,
): void {
  buffer.text += decoder.decode(chunk, { stream: true });
  const lines = buffer.text.split(/\r?\n/);
  buffer.text = lines.pop() ?? "";
  for (const line of lines) onLine(line);
}

function createLineGate(onLines: (lines: string[]) => void) {
  let pending: string[] = [];
  let timer: ReturnType<typeof setTimeout> | undefined;

  return {
    push(line: string): void {
      pending.push(line);
      if (timer) clearTimeout(timer);
      timer = setTimeout(() => {
        timer = undefined;
        const lines = pending;
        pending = [];
        onLines(lines);
      }, FRAME_QUIET_MS);
    },
    clear(): void {
      if (timer) clearTimeout(timer);
      timer = undefined;
      pending = [];
    },
  };
}

async function readStdin(): Promise<void> {
  const decoder = new TextDecoder();
  const buffer = { text: "" };

  for await (const chunk of Bun.stdin.stream()) {
    processChunk(chunk, decoder, buffer);
  }

  buffer.text += decoder.decode();
  if (buffer.text) printLine(buffer.text);
}

function listDevices(): Device[] {
  const result = Bun.spawnSync(["pio", "device", "list", "--json-output"], {
    stdout: "pipe",
    stderr: "pipe",
  });

  if (result.exitCode !== 0) {
    const stderr = new TextDecoder().decode(result.stderr).trim();
    throw new Error(`'pio device list --json-output' failed: ${stderr || `exit ${result.exitCode}`}`);
  }

  const parsed: unknown = JSON.parse(new TextDecoder().decode(result.stdout));
  return Array.isArray(parsed) ? (parsed as Device[]) : [];
}

function resolvePort(explicitPort?: string): string {
  if (explicitPort) return explicitPort;

  // 'pio device list' also reports every /dev/ttyS* legacy port as "n/a".
  // Keep USB adapters first; fall back to any non-ttyS port.
  const devices = listDevices().filter((device) => !device.port.startsWith("/dev/ttyS"));
  const usbDevices = devices.filter((device) => device.hwid && device.hwid !== "n/a");
  const candidates = usbDevices.length > 0 ? usbDevices : devices;

  if (candidates.length === 0) {
    throw new Error("No serial port found. Connect the board or pass --port <port>.");
  }

  if (candidates.length > 1) {
    const list = candidates
      .map((device) => `  ${device.port} (${device.description ?? "no description"})`)
      .join("\n");
    throw new Error(`Multiple serial ports found:\n${list}\nPick one with --port <port>.`);
  }

  return candidates[0].port;
}

function configurePort(port: string, baud: number): void {
  // macOS stty uses -f; Linux uses -F. 'raw' and 'clocal' are what stop the
  // terminal driver from buffering, echoing, or waiting for modem lines.
  const flag = process.platform === "darwin" ? "-f" : "-F";
  const result = Bun.spawnSync(
    [
      "stty",
      flag,
      port,
      String(baud),
      "cs8",
      "cread",
      "clocal",
      "-hupcl",
      "raw",
      "-echo",
      "-echoe",
      "-echok",
      "-echoctl",
      "-echoke",
    ],
    { stdout: "pipe", stderr: "pipe" },
  );

  if (result.exitCode !== 0) {
    const stderr = new TextDecoder().decode(result.stderr).trim();
    throw new Error(`Could not configure ${port}: ${stderr || `stty exited ${result.exitCode}`}`);
  }
}

async function readSerial(): Promise<number> {
  const port = resolvePort(getArg("--port"));
  const baud = getNumberArg("--baud", DEFAULT_BAUD);
  const settleSeconds = getNumberArg("--settle", DEFAULT_SETTLE_SECONDS);

  if (!Number.isInteger(baud) || baud <= 0) {
    throw new Error(`Invalid baud rate: ${baud}`);
  }

  configurePort(port, baud);

  const settleMs = Math.round(settleSeconds * 1000);
  console.error(`Opened ${port} at ${baud} baud.`);

  const stream = createReadStream(port, { highWaterMark: 256 });
  const decoder = new TextDecoder();
  const buffer = { text: "" };

  // Only parse after a completed line has been followed by 100 ms of silence.
  // This is the frame boundary; it is independent of the startup warm-up.
  const gate = createLineGate((lines) => {
    for (const line of lines) printLine(line);
  });

  // Opening the port usually resets the Mega. setup() can take several
  // seconds (HX711 probes, DS18B20 start), so the warm-up timer starts when
  // the first telemetry bytes arrive, not when the port is opened.
  let warmupStarted = settleMs <= 0;
  let settled = settleMs <= 0;
  // Opening the port can happen mid-message. Drop bytes until the first
  // newline after settling so the first printed line is complete.
  let resync = true;
  let warmupTimer: ReturnType<typeof setTimeout> | undefined;

  const startWarmup = (): void => {
    if (warmupStarted) return;
    warmupStarted = true;
    console.error(
      `Telemetry started. Waiting ${settleSeconds}s for the sensor cache to fill...`,
    );
    warmupTimer = setTimeout(() => {
      warmupTimer = undefined;
      settled = true;
      resync = true;
      gate.clear();
      console.error("Telemetry settled; showing live data.");
    }, settleMs);
  };

  const stop = () => {
    if (warmupTimer) clearTimeout(warmupTimer);
    gate.clear();
    stream.destroy();
    process.exit(130);
  };
  process.on("SIGINT", stop);
  process.on("SIGTERM", stop);

  try {
    for await (const chunk of stream) {
      if (!warmupStarted) startWarmup();

      // Discard the startup window. The board has just been reset, so these
      // messages still hold the firmware's invalid cache values.
      if (!settled) continue;

      let data = chunk instanceof Uint8Array ? chunk : new Uint8Array(chunk);

      if (resync) {
        const newline = data.indexOf(0x0a);
        if (newline === -1) continue;
        data = data.subarray(newline + 1);
        resync = false;
        if (data.length === 0) continue;
      }

      processChunk(data, decoder, buffer, (line) => gate.push(line));
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    console.error(`Serial read failed: ${message}`);
    console.error("Close any other monitor using the port and try again.");
    return 1;
  } finally {
    if (warmupTimer) clearTimeout(warmupTimer);
    gate.clear();
  }

  buffer.text += decoder.decode();
  if (buffer.text) printLine(buffer.text);

  return 0;
}

function printHelp(): void {
  console.log(`Pretty-print the legacy slave JSON telemetry as a bottle table

Usage:
  bun scripts-hardware/read-legacy-slave.ts [options]
  bun scripts-hardware/read-legacy-slave.ts --stdin < captured.log

Options:
  --port <port>       Serial port (default: autodetect via 'pio device list')
  --baud <baud>       Baud rate (default: ${DEFAULT_BAUD})
  --settle <seconds>  Discard telemetry for this long after the first data
                      arrives (default: ${DEFAULT_SETTLE_SECONDS}; 0 disables)
  --stdin             Read JSON lines from stdin instead of opening the serial port
  -h, --help          Show this help

Examples:
  bun scripts-hardware/read-legacy-slave.ts
  bun scripts-hardware/read-legacy-slave.ts --port /dev/ttyUSB0
  bun scripts-hardware/read-legacy-slave.ts --settle 8
  pio device monitor -e legacy-slave --quiet --raw | bun scripts-hardware/read-legacy-slave.ts --stdin`);
}

if (hasFlag("-h", "--help")) {
  printHelp();
  process.exit(0);
}

if (hasFlag("--stdin")) {
  await readStdin();
} else {
  try {
    process.exit(await readSerial());
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    process.exit(1);
  }
}
