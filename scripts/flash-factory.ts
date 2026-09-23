#!/usr/bin/env bun
/**
 * Flash the full-factory display image with PlatformIO's esptool.
 *
 * Default image: .backup/display_factory.bin (16MB full-flash dump for
 * esp32-s3-devkitc-1, same for [env:display] / [env:display-v9])
 * Flash offset: 0x0
 *
 * Usage:
 *   bun scripts/flash-factory.ts [--port /dev/ttyUSB0] [--binary .backup/display_factory.bin] [--baud 921600] [--chip esp32-s3]
 *   bun scripts/flash-factory.ts --list-ports
 *
 * Port autodetect uses `pio device list --json-output` and ignores
 * internal /dev/ttyS* entries with hwid "n/a". If several candidates
 * remain it probes each with `esptool.py chip_id` and picks the one
 * that answers as the expected chip.
 */

type Device = { port: string; description?: string; hwid?: string };

const REPO_ROOT = import.meta.dir.endsWith("/scripts")
  ? `${import.meta.dir}/..`
  : process.cwd();

const DEFAULT_BINARY = `${REPO_ROOT}/.backup/display_factory.bin`;
const ESPTOOL_PACKAGE = "platformio/tool-esptoolpy@^2.41100.260830";
const DEFAULT_CHIP = "esp32-s3";
const DEFAULT_BAUD = 921600;
const FALLBACK_BAUD = 460800;

function printHelp() {
  console.log(`Flash display factory image via PlatformIO esptool

Usage:
  bun scripts/flash-factory.ts [options]

Options:
  --binary <path>   Factory .bin to flash (default: .backup/display_factory.bin)
  --port <port>     Serial port (default: autodetect via 'pio device list')
  --baud <baud>     Initial baud (default: ${DEFAULT_BAUD}, retries at ${FALLBACK_BAUD})
  --chip <chip>     esptool chip (default: ${DEFAULT_CHIP})
  --list-ports      Print autodetected candidate ports and exit
  -h, --help        Show this help

Examples:
  bun scripts/flash-factory.ts
  bun scripts/flash-factory.ts --port /dev/ttyUSB0
  bun scripts/flash-factory.ts --list-ports`);
}

function getArg(name: string): string | undefined {
  const idx = process.argv.indexOf(name);
  if (idx === -1 || idx + 1 >= process.argv.length) return undefined;
  return process.argv[idx + 1];
}

function hasFlag(...names: string[]): boolean {
  return names.some((n) => process.argv.includes(n));
}

function runCapture(cmd: string[]): { ok: boolean; stdout: string; stderr: string; code: number } {
  const res = Bun.spawnSync(cmd, { stdout: "pipe", stderr: "pipe" });
  const dec = new TextDecoder();
  return {
    ok: res.exitCode === 0,
    stdout: dec.decode(res.stdout).trim(),
    stderr: dec.decode(res.stderr).trim(),
    code: res.exitCode ?? 1,
  };
}

function listDevices(): Device[] {
  const res = runCapture(["pio", "device", "list", "--json-output"]);
  if (!res.ok) {
    throw new Error(`'pio device list --json-output' failed:\n${res.stderr || res.stdout}`);
  }
  try {
    const parsed = JSON.parse(res.stdout);
    return Array.isArray(parsed) ? parsed : [];
  } catch (e) {
    throw new Error(`Could not parse 'pio device list' output: ${(e as Error).message}\n${res.stdout}`);
  }
}

/** Drop internal UARTs (/dev/ttyS*, hwid n/a) — keep real USB serial adapters. */
function candidatePorts(devices: Device[]): Device[] {
  return devices.filter((d) => {
    if (!d.port) return false;
    if (d.port.startsWith("/dev/ttyS")) return false;
    if (!d.hwid || d.hwid === "n/a") return false;
    return true;
  });
}

function scoreDevice(d: Device, chip: string): number {
  const hay = `${d.description ?? ""} ${d.hwid ?? ""}`.toLowerCase();
  let score = 0;
  if (hay.includes("1a86:7523")) score += 3; // CH340, commonly used here
  if (/ch34|ch341|cp210|ftdi|silicon labs|espressif|esp32|usb serial/.test(hay)) score += 2;
  if (d.port.includes("ttyUSB") || d.port.includes("ttyACM")) score += 1;
  void chip;
  return score;
}

function probeChip(port: string, chip: string): boolean {
  // Quick probe: does an ESP answer on this port?
  const res = runCapture([
    "pio",
    "pkg",
    "exec",
    "-p",
    ESPTOOL_PACKAGE,
    "--",
    "esptool.py",
    "--chip",
    chip,
    "--port",
    port,
    "--baud",
    "115200",
    "chip_id",
  ]);
  return res.ok;
}

async function autodetectPort(chip: string): Promise<string> {
  const devices = listDevices();
  const candidates = candidatePorts(devices);

  if (candidates.length === 0) {
    const all = devices.map((d) => d.port).join(", ") || "(none)";
    throw new Error(
      `No usable serial port found. Seen: ${all}\nPlug in the display board, then retry with --port <port>. Check 'pio device list'.`
    );
  }

  if (candidates.length === 1) return candidates[0]!.port;

  // Several candidates: prefer likely USB-UARTs, then probe for the expected chip.
  const ranked = [...candidates].sort((a, b) => scoreDevice(b, chip) - scoreDevice(a, chip));
  for (const d of ranked) {
    if (probeChip(d.port, chip)) {
      console.log(`Probed ${d.port}: ${chip} responded, using it.`);
      return d.port;
    }
  }

  throw new Error(
    `Multiple serial ports found, none clearly the ${chip}: ${candidates.map((d) => d.port).join(", ")}\nRe-run with --port <port>.`
  );
}

async function flash(binary: string, port: string, chip: string, baud: number): Promise<void> {
  const file = Bun.file(binary);
  if (!(await file.exists())) throw new Error(`Binary not found: ${binary}`);
  if (file.size !== 16 * 1024 * 1024) {
    console.warn(`warn: expected 16MB full-flash image, got ${file.size} bytes — continuing anyway.`);
  }

  const baseArgs = (b: number) => [
    "pio",
    "pkg",
    "exec",
    "-p",
    ESPTOOL_PACKAGE,
    "--",
    "esptool.py",
    "--chip",
    chip,
    "--port",
    port,
    "--baud",
    String(b),
    "write_flash",
    "-z",
    "--flash_mode",
    "dio",
    "--flash_freq",
    "80m",
    "--flash_size",
    "16MB",
    "0x0",
    binary,
  ];

  for (const b of baud === FALLBACK_BAUD ? [baud] : [baud, FALLBACK_BAUD]) {
    console.log(`\n$ ${baseArgs(b).join(" ")}`);
    const proc = Bun.spawn(baseArgs(b), { stdio: ["inherit", "inherit", "inherit"] });
    const code = await proc.exited;
    if (code === 0) {
      console.log(`\nDone: flashed ${binary} to ${port} (${chip}).`);
      return;
    }
    console.warn(`esptool exited with code ${code} at baud ${b}.`);
    if (b !== FALLBACK_BAUD) console.log(`Retrying at fallback baud ${FALLBACK_BAUD} (hold BOOT if needed)...`);
    else throw new Error(`Flash failed on ${port} (tried baud ${baud} and ${FALLBACK_BAUD}).`);
  }
}

async function main() {
  if (hasFlag("-h", "--help")) {
    printHelp();
    return;
  }
  const chip = getArg("--chip") ?? DEFAULT_CHIP;
  const baud = Number(getArg("--baud") ?? DEFAULT_BAUD);
  const binary = getArg("--binary") ?? DEFAULT_BINARY;

  if (hasFlag("--list-ports")) {
    const devices = listDevices();
    const candidates = candidatePorts(devices);
    console.log(JSON.stringify({ devices, candidates }, null, 2));
    return;
  }

  const port = getArg("--port") ?? (await autodetectPort(chip));
  console.log(`Using port: ${port}`);
  await flash(binary, port, chip, baud);
}

main().catch((e) => {
  console.error(`error: ${(e as Error).message}`);
  process.exit(1);
});
