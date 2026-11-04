#!/usr/bin/env bun
/**
 * Interactive REPL for the legacy water-bath master over USB serial.
 *
 * The master (`legacy/master/master.cpp`) speaks newline-terminated ASCII
 * commands on `Serial` (UART0, D0/D1) at 115200 baud. It replies to `STATUS`
 * with a `{"type":"master_status", ...}` JSON line, and it forwards every
 * slave packet it receives on `Serial1` (9600 baud) straight to USB as a
 * `{"bottles":[...]}` JSON line. This script opens that USB link directly,
 * so any computer with Bun + PlatformIO can control the master without the
 * Raspberry Pi Flask backend (`legacy/pi/water-control-system/web_control.py`).
 *
 * Opening the serial port usually resets the Mega (DTR pulse). The wrapper
 * discards the startup window (default 5 s after the first bytes arrive) and
 * resyncs to the next newline, so the first printed line is complete.
 *
 * Usage:
 *   bun scripts-hardware/legacy-master-usb.ts                  # autodetect port, REPL
 *   bun scripts-hardware/legacy-master-usb.ts --port /dev/ttyUSB0
 *   bun scripts-hardware/legacy-master-usb.ts --json           # raw JSON, no tables
 *   bun scripts-hardware/legacy-master-usb.ts --cmd STATUS     # one-shot, then exit
 *   bun scripts-hardware/legacy-master-usb.ts --list-ports
 *
 * REPL commands (sent to the master, case-sensitive):
 *   STATUS
 *   MODE_AUTO | MODE_OFF
 *   HEATER_AUTO | HEATER_ON | HEATER_OFF
 *   PUMP_AUTO | PUMP_ON | PUMP_OFF
 *   PUMP1_AUTO | PUMP1_ON | PUMP1_OFF
 *   PUMP2_AUTO | PUMP2_ON | PUMP2_OFF
 *   PWM:<D3|D7|D4>:<0-255>     e.g. PWM:D3:200
 *   STIR:<mode 0-3>:<mins>:<secs>  e.g. STIR:2:10:30
 *   TEMP:<target 20-50>:<hysteresis 0.1-5>  e.g. TEMP:39.0:0.3
 *
 * Local REPL commands (not sent to the master):
 *   help, exit, quit, clear
 *
 * Values are printed exactly as the firmware sent them. `-999` (temperature)
 * and `-1` (leak ADC when disabled) are the firmware's invalid markers.
 */

import { createReadStream, createWriteStream } from "node:fs";
import { createInterface } from "node:readline";

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
  target_temp?: number | string;
  hysteresis?: number | string;
  system_mode?: string;
  heater?: number | string | boolean;
  pump?: number | string | boolean;
  pump1?: number | string | boolean;
  pump2?: number | string | boolean;
  stirrer?: number | string | boolean;
  heater_mode?: string;
  pump_mode?: string;
  pump1_mode?: string;
  pump2_mode?: string;
  stir_mode?: string;
  stir_int_min?: number | string;
  stir_pulse_sec?: number | string;
  stir_timed_min?: number | string;
  stir_rem_sec?: number | string;
  pwm_d3?: number | string;
  pwm_d7?: number | string;
  pwm_d4?: number | string;
  out_d3?: number | string;
  out_d7?: number | string;
  out_d4?: number | string;
  water_low?: number | string | boolean;
  water_adc?: number | string;
  water_thr?: number | string;
  leak_enabled?: number | string | boolean;
  pump_leak?: number | string | boolean;
  leak_adc?: number | string;
  leak_thr?: number | string;
};

type Telemetry = MasterStatus & {
  bottles?: Bottle[];
  status?: string;
};

type Device = { port: string; description?: string; hwid?: string };

const DEFAULT_BAUD = 115200;
const DEFAULT_SETTLE_SECONDS = 5;
// Wait this long after the last complete line before parsing. Serial data
// arrives in chunks; this lets the monitor finish delivering a frame.
const FRAME_QUIET_MS = 100;

const BOTTLE_HEADERS = ["time", "bottle", "pH", "CH4 (ppm)", "temp (°C)", "CO2 (ppm)", "press (Pa)"];
const BOTTLE_WIDTHS = [8, 6, 7, 9, 9, 9, 10];
const BOTTLE_ALIGN: Array<"left" | "right"> = ["left", "right", "right", "right", "right", "right", "right"];

let headerPrinted = false;
let jsonMode = false;

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
  const numeric = typeof value === "number" ? value : Number(value);
  if (!Number.isFinite(numeric)) return String(value ?? "-");
  return numeric.toFixed(digits);
}

function formatInt(value: unknown): string {
  const numeric = typeof value === "number" ? value : Number(value);
  if (!Number.isFinite(numeric)) return String(value ?? "-");
  return String(Math.round(numeric));
}

function onOff(value: unknown): string {
  return value === true || value === 1 || value === "1" ? "ON" : "OFF";
}

function clock(): string {
  return new Date().toTimeString().slice(0, 8);
}

function printBottleHeader(): void {
  const line = BOTTLE_HEADERS.map((header, i) => formatCell(header, BOTTLE_WIDTHS[i], BOTTLE_ALIGN[i])).join("  ");
  console.log(line);
  console.log(BOTTLE_WIDTHS.map((width) => "-".repeat(width)).join("  "));
}

function printBottles(bottles: Bottle[]): void {
  if (!headerPrinted) {
    printBottleHeader();
    headerPrinted = true;
  }

  const time = clock();

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

    console.log(row.map((cell, i) => formatCell(cell, BOTTLE_WIDTHS[i], BOTTLE_ALIGN[i])).join("  "));
  });
}

function printMasterStatus(status: MasterStatus): void {
  if (jsonMode) {
    console.log(JSON.stringify(status));
    return;
  }

  const parts = [
    `${clock()}  master`,
    `bath ${formatNumber(status.water_temp, 2)}°C`,
    `avg ${formatNumber(status.bottle_temp_avg, 2)}°C`,
    `target ${formatNumber(status.target_temp, 1)}°C`,
    String(status.system_mode ?? "-"),
    `heat ${onOff(status.heater)} (${status.heater_mode ?? "-"})`,
    `pump1 ${onOff(status.pump1)} (${status.pump1_mode ?? "-"})`,
    `pump2 ${onOff(status.pump2)} (${status.pump2_mode ?? "-"})`,
    `stir ${onOff(status.stirrer)} (${status.stir_mode ?? "-"})`,
    `water ${onOff(status.water_low) === "ON" ? "LOW" : "OK"} adc=${formatInt(status.water_adc)}/${formatInt(status.water_thr)}`,
    `pwm D3=${formatInt(status.pwm_d3)}→${formatInt(status.out_d3)} D7=${formatInt(status.pwm_d7)}→${formatInt(status.out_d7)} D4=${formatInt(status.pwm_d4)}→${formatInt(status.out_d4)}`,
  ];

  console.log(parts.join("  "));

  if (status.leak_enabled === 1 || status.leak_enabled === true || status.leak_enabled === "1") {
    console.log(
      `  leak ${onOff(status.pump_leak)} adc=${formatInt(status.leak_adc)}/${formatInt(status.leak_thr)}`,
    );
  }
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

  if (message.type === "master_status") {
    printMasterStatus(message);
    return;
  }

  if (Array.isArray(message.bottles)) {
    if (jsonMode) {
      console.log(clean);
    } else {
      printBottles(message.bottles);
    }
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
  // Windows has no stty; the COM port is opened with defaults and Bun/Node
  // handles the rest. Linux and macOS need raw mode so the terminal driver
  // does not buffer, echo, or wait for modem lines.
  if (process.platform === "win32") {
    console.error(`Windows: skipping stty config for ${port}; using defaults.`);
    return;
  }

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

function printHelp(): void {
  console.log(`Interactive REPL for the legacy water-bath master over USB serial

Usage:
  bun scripts-hardware/legacy-master-usb.ts [options]
  bun scripts-hardware/legacy-master-usb.ts --cmd STATUS

Options:
  --port <port>       Serial port (default: autodetect via 'pio device list')
  --baud <baud>       Baud rate (default: ${DEFAULT_BAUD})
  --settle <seconds>  Discard telemetry for this long after the first data
                      arrives (default: ${DEFAULT_SETTLE_SECONDS}; 0 disables)
  --json              Print raw JSON lines instead of tables
  --cmd <command>     Send one command (e.g. STATUS), print the reply, exit
  --list-ports        Print autodetected candidate ports and exit
  -h, --help          Show this help

Master commands (type in the REPL, case-sensitive):
  STATUS
  MODE_AUTO | MODE_OFF
  HEATER_AUTO | HEATER_ON | HEATER_OFF
  PUMP_AUTO | PUMP_ON | PUMP_OFF
  PUMP1_AUTO | PUMP1_ON | PUMP1_OFF
  PUMP2_AUTO | PUMP2_ON | PUMP2_OFF
  PWM:<D3|D7|D4>:<0-255>     e.g. PWM:D3:200
  STIR:<mode 0-3>:<mins>:<secs>  e.g. STIR:2:10:30
  TEMP:<target 20-50>:<hysteresis 0.1-5>  e.g. TEMP:39.0:0.3

Local REPL commands:
  help, exit, quit, clear

Examples:
  bun scripts-hardware/legacy-master-usb.ts
  bun scripts-hardware/legacy-master-usb.ts --port /dev/ttyUSB0
  bun scripts-hardware/legacy-master-usb.ts --cmd STATUS
  bun scripts-hardware/legacy-master-usb.ts --list-ports`);
}

function printReplHelp(): void {
  console.log(`Master commands (sent to the board):
  STATUS
  MODE_AUTO | MODE_OFF
  HEATER_AUTO | HEATER_ON | HEATER_OFF
  PUMP_AUTO | PUMP_ON | PUMP_OFF
  PUMP1_AUTO | PUMP1_ON | PUMP1_OFF
  PUMP2_AUTO | PUMP2_ON | PUMP2_OFF
  PWM:<D3|D7|D4>:<0-255>     e.g. PWM:D3:200
  STIR:<mode 0-3>:<mins>:<secs>  e.g. STIR:2:10:30
  TEMP:<target 20-50>:<hysteresis 0.1-5>  e.g. TEMP:39.0:0.3

Local commands:
  help, exit, quit, clear`);
}

async function runRepl(port: string, baud: number, settleSeconds: number, oneShot?: string): Promise<number> {
  if (!Number.isInteger(baud) || baud <= 0) {
    throw new Error(`Invalid baud rate: ${baud}`);
  }

  configurePort(port, baud);

  const settleMs = Math.round(settleSeconds * 1000);
  console.error(`Opened ${port} at ${baud} baud.`);

  const readStream = createReadStream(port, { highWaterMark: 256 });
  const writeStream = createWriteStream(port);
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
      if (oneShot) {
        writeStream.write(`${oneShot}\n`);
        console.error(`Sent: ${oneShot}`);
        // Give the master time to reply, then exit.
        setTimeout(() => {
          shutdown(0);
        }, 1500);
      } else {
        console.error("Type 'help' for commands, 'exit' to quit.");
      }
    }, settleMs);
  };

  let rl: ReturnType<typeof createInterface> | undefined;
  let shuttingDown = false;

  function shutdown(code: number): void {
    if (shuttingDown) return;
    shuttingDown = true;
    if (warmupTimer) clearTimeout(warmupTimer);
    gate.clear();
    if (rl) rl.close();
    readStream.destroy();
    writeStream.destroy();
    process.exit(code);
  }

  process.on("SIGINT", () => shutdown(130));
  process.on("SIGTERM", () => shutdown(143));

  readStream.on("error", (error) => {
    console.error(`Serial read failed: ${error.message}`);
    console.error("Close any other monitor using the port and try again.");
    shutdown(1);
  });

  writeStream.on("error", (error) => {
    console.error(`Serial write failed: ${error.message}`);
    shutdown(1);
  });

  // If --cmd is given with --settle 0, send immediately.
  if (oneShot && settleMs <= 0) {
    writeStream.write(`${oneShot}\n`);
    console.error(`Sent: ${oneShot}`);
    setTimeout(() => shutdown(0), 1500);
  }

  if (!oneShot) {
    rl = createInterface({
      input: process.stdin,
      output: process.stdout,
      prompt: "master> ",
    });

    rl.prompt();

    rl.on("line", (input: string) => {
      const cmd = input.trim();
      if (!cmd) {
        rl?.prompt();
        return;
      }

      const lower = cmd.toLowerCase();
      if (lower === "help") {
        printReplHelp();
        rl?.prompt();
        return;
      }
      if (lower === "exit" || lower === "quit") {
        shutdown(0);
        return;
      }
      if (lower === "clear") {
        process.stdout.write("\x1b[2J\x1b[H");
        rl?.prompt();
        return;
      }

      writeStream.write(`${cmd}\n`);
      console.error(`Sent: ${cmd}`);
      rl?.prompt();
    });

    rl.on("close", () => shutdown(0));
  }

  try {
    for await (const chunk of readStream) {
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
    if (shuttingDown) return 0;
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

if (hasFlag("-h", "--help")) {
  printHelp();
  process.exit(0);
}

if (hasFlag("--list-ports")) {
  const devices = listDevices();
  const candidates = devices.filter((device) => !device.port.startsWith("/dev/ttyS"));
  console.log(JSON.stringify({ devices, candidates }, null, 2));
  process.exit(0);
}

jsonMode = hasFlag("--json");

try {
  const port = resolvePort(getArg("--port"));
  const baud = getNumberArg("--baud", DEFAULT_BAUD);
  const settleSeconds = getNumberArg("--settle", DEFAULT_SETTLE_SECONDS);
  const oneShot = getArg("--cmd");
  process.exit(await runRepl(port, baud, settleSeconds, oneShot));
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
}
