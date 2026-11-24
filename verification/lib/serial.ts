/**
 * Serial helpers shared by the verification tools.
 *
 * The display's USB is a CH340 with an auto-reset circuit. A pyserial open
 * resets the board; a plain open() + termios (what `stty` does) does not, so
 * every tool here configures the port with stty and then opens it as a file.
 * See docs/link-serial-protocol.md §7.
 */
import { createReadStream, createWriteStream, existsSync, type WriteStream } from "node:fs";

type Device = { port: string; description?: string; hwid?: string };

export function listPorts(): Device[] {
  const result = Bun.spawnSync(["pio", "device", "list", "--json-output"], { stdout: "pipe", stderr: "pipe" });
  if (result.exitCode !== 0) return [];
  try {
    const parsed: unknown = JSON.parse(new TextDecoder().decode(result.stdout));
    return Array.isArray(parsed) ? (parsed as Device[]) : [];
  } catch {
    return [];
  }
}

/** Explicit port, else the single USB serial port; refuses to guess between several. */
export function resolvePort(explicit?: string): string {
  if (explicit) return explicit;
  const usb = listPorts().filter((d) => !d.port.startsWith("/dev/ttyS") && d.hwid && d.hwid !== "n/a");
  if (usb.length === 1) return usb[0]!.port;
  if (usb.length === 0) throw new Error("No USB serial port found. Pass --port <path>.");
  throw new Error(`Several serial ports found (${usb.map((d) => d.port).join(", ")}). Pass --port <path>.`);
}

export function configurePort(port: string, baud: number): void {
  const flag = process.platform === "darwin" ? "-f" : "-F";
  const result = Bun.spawnSync(
    ["stty", flag, port, String(baud), "cs8", "cread", "clocal", "-hupcl", "raw", "-echo", "-echoe", "-echok", "-echoctl", "-echoke"],
    { stdout: "pipe", stderr: "pipe" },
  );
  if (result.exitCode !== 0) {
    throw new Error(`stty failed on ${port}: ${new TextDecoder().decode(result.stderr).trim()}`);
  }
}

export type LinePort = {
  write(line: string): void;
  close(): void;
};

/**
 * Opens `port` and calls `onLine` with every complete line (CR stripped) and
 * the host's receive time. `onClose` fires when the device disappears (for
 * example a power cycle); the caller decides whether to reopen.
 */
export function openLinePort(
  port: string,
  baud: number,
  onLine: (line: string, hostMs: number) => void,
  onClose?: (why: string) => void,
): LinePort {
  configurePort(port, baud);
  const rd = createReadStream(port, { highWaterMark: 512 });
  let wr: WriteStream | null = createWriteStream(port);
  const decoder = new TextDecoder();
  let pending = "";
  let closed = false;
  const finish = (why: string) => {
    if (closed) return;
    closed = true;
    try {
      rd.destroy();
    } catch {}
    try {
      wr?.destroy();
    } catch {}
    wr = null;
    onClose?.(why);
  };
  rd.on("data", (chunk: Buffer | string) => {
    const now = Date.now();
    pending += typeof chunk === "string" ? chunk : decoder.decode(chunk, { stream: true });
    let nl = pending.indexOf("\n");
    while (nl >= 0) {
      const line = pending.slice(0, nl).replace(/\r$/, "");
      pending = pending.slice(nl + 1);
      onLine(line, now);
      nl = pending.indexOf("\n");
    }
    if (pending.length > 8192) pending = ""; // binary noise, never a line
  });
  rd.on("error", (e) => finish(`read error: ${e.message}`));
  rd.on("close", () => finish("closed"));
  wr.on("error", (e) => finish(`write error: ${e.message}`));
  return {
    write(line: string) {
      wr?.write(`${line}\n`);
    },
    close() {
      finish("closed by tool");
    },
  };
}

export function portExists(port: string): boolean {
  return existsSync(port);
}
