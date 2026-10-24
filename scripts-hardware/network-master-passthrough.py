"""Arm the display's serial tunnel around a network-master upload.

The tunnel has to survive the upload tool's own serial open. A normal open
raises DTR/RTS, the display's USB-UART bridge pulses its EN pin, the board
reboots and the tunnel is gone before the first sync byte arrives; esptool's
``--before no_reset`` only skips esptool's own reset sequence, not the reset
that pyserial performs while opening the port.

So this hook keeps one raw handle on the display for the whole upload, with
the modem lines untouched, and hands esptool a pseudoterminal instead. Bytes
are relayed between the two in both directions for the duration.

See docs/llm-generated/link-serial-protocol.md section 7.
"""

import atexit
import fcntl
import os
import pty
import select
import sys
import termios
import threading
import time

from serial.tools import list_ports

BAUD = termios.B115200
# Espressif native USB-Serial/JTAG, CH34x and CP210x bridges. The display on
# the bench is a CH340, so restricting this to 0x303A found nothing.
USB_VIDS = frozenset((0x303A, 0x1A86, 0x10C4))
ON_MARKER = b"passthrough ON"
OFF_MARKER = b"passthrough OFF"
ON_TIMEOUT_S = 1.5
OFF_TIMEOUT_S = 1.5

# (display fd, relay) while a tunnel is open, else None.
_active = None


def _display_ports():
    return [
        item.device for item in list_ports.comports() if item.vid in USB_VIDS
    ]


def _configured_port(env):
    configured = str(env.get("UPLOAD_PORT") or env.subst("$UPLOAD_PORT") or "").strip()
    if configured.startswith("$"):
        return ""
    if configured.lower() in ("none", "auto"):
        return ""
    return configured


def _find_port(env):
    configured = _configured_port(env)
    if configured:
        return configured

    candidates = _display_ports()
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        raise RuntimeError(
            "no upload port is configured and no USB serial port was found; "
            "connect the display or pass --upload-port"
        )
    raise RuntimeError(
        "multiple USB serial ports were found ("
        + ", ".join(candidates)
        + "); set upload_port or pass --upload-port to select the display"
    )


def _configure_tty(fd):
    """Raw 8N1 at 115200, no modem-line control.

    Deliberately avoids TIOCMBIS/TIOCMBIC: driving DTR or RTS is what resets
    the display. pyserial's open does exactly that, which is why this module
    opens the device with os.open instead.
    """
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0  # iflag: no XON/XOFF, no CR/LF translation
    attrs[1] = 0  # oflag: no post-processing
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0  # lflag: raw, no echo, no signals
    attrs[4] = attrs[5] = BAUD
    attrs[6][termios.VMIN] = 1
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    # Drop whatever the CH340 buffered while no process held the port, so the
    # confirmation search starts from a known point.
    termios.tcflush(fd, termios.TCIFLUSH)


def _open_display(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        _configure_tty(fd)
        flags = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)
    except Exception:
        os.close(fd)
        raise
    return fd


def _write_all(fd, data):
    view = memoryview(data)
    while view:
        written = os.write(fd, view)
        if written <= 0:
            # os.write returning 0 would leave `view` unchanged and spin here
            # forever; treat it as a failed write instead.
            raise OSError("short write to fd {}".format(fd))
        view = view[written:]


def _saw(data, marker):
    """Case-insensitive marker test.

    Both sides must be lowered: comparing a capitalised marker against
    lowered text never matches, which silently turns every confirmation
    into a timeout.
    """
    return marker.lower() in bytes(data).lower()


def _read_until(fd, marker, timeout_s):
    """Read until `marker` appears; return everything read either way."""
    received = bytearray()
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        ready, _, _ = select.select([fd], [], [], 0.1)
        if not ready:
            continue

        chunk = os.read(fd, 256)
        if not chunk:
            # Zero length means hangup, not "wait for more". Continuing would
            # spin at full CPU until the deadline.
            break
        received.extend(chunk)
        if _saw(received, marker):
            break
    return bytes(received)


def _arm_tunnel(fd, attempts=3):
    """Send PASSTH ON until the display confirms it.

    The first command after the port opens is occasionally lost -- the CH340
    settles late, or the display is still printing its boot banner -- so a
    single miss must not fail the upload. Re-sending while the tunnel is
    already open is harmless: the display relays the bytes to the WT32, which
    ignores them until esptool starts its sync.

    Returns (confirmed, everything_read).
    """
    seen = b""
    time.sleep(0.2)  # let the bridge settle after tcsetattr
    for _ in range(attempts):
        _write_all(fd, b"PASSTH ON\r\n")
        seen += _read_until(fd, ON_MARKER, ON_TIMEOUT_S)
        if _saw(seen, ON_MARKER):
            return True, seen
    return False, seen


def _close_tunnel(fd):
    """Ask the display to leave passthrough and wait for its confirmation.

    The exit word is only recognised at the start of a line. An upload leaves
    the byte stream mid-line, so send a newline first; the WT32 ignores the
    empty line.
    """
    _write_all(fd, b"\nPASSTH OFF\r\n")
    return _saw(_read_until(fd, OFF_MARKER, OFF_TIMEOUT_S), OFF_MARKER)


class _Relay:
    """Two blocking pumps between the display and a pty esptool can open."""

    def __init__(self, display_fd):
        self.display_fd = display_fd
        self.bytes_out = 0  # esptool -> display -> WT32
        self.bytes_in = 0  # WT32 -> display -> esptool
        self._stop = threading.Event()
        self._threads = []
        self.master_fd = None
        self.slave_fd = None
        try:
            self.master_fd, self.slave_fd = pty.openpty()
            _configure_tty(self.slave_fd)
            self.path = os.ttyname(self.slave_fd)
            self._threads = [
                threading.Thread(
                    target=self._pump,
                    args=(self.master_fd, self.display_fd, "out"),
                    daemon=True,
                ),
                threading.Thread(
                    target=self._pump,
                    args=(self.display_fd, self.master_fd, "in"),
                    daemon=True,
                ),
            ]
            for thread in self._threads:
                thread.start()
        except BaseException:
            # Configuring the pty or starting a pump can fail after both
            # descriptors exist. Release them here rather than leaking a pty
            # pair, and stop any pump that did start.
            self._stop.set()
            for fd in (self.slave_fd, self.master_fd):
                if fd is not None:
                    try:
                        os.close(fd)
                    except OSError:
                        pass
            raise

    def _pump(self, src, dst, direction):
        while not self._stop.is_set():
            try:
                ready, _, _ = select.select([src], [], [], 0.1)
                if not ready:
                    continue
                chunk = os.read(src, 4096)
            except OSError:
                return
            if not chunk:
                return
            try:
                _write_all(dst, chunk)
            except OSError:
                return
            if direction == "out":
                self.bytes_out += len(chunk)
            else:
                self.bytes_in += len(chunk)

    def close(self):
        # Stop and join before the caller reads the display again: a pump
        # blocked on the display fd would otherwise steal the exit
        # confirmation and drop it into a closed pty.
        #
        # The join timeout bounds a pump stuck writing into a full pty buffer
        # (only possible while esptool is not reading). Closing the fd under a
        # still-blocked thread is safe here because nothing in this process
        # opens another file afterwards, so the number cannot be reused.
        self._stop.set()
        for thread in self._threads:
            thread.join(timeout=0.5)
        for fd in (self.slave_fd, self.master_fd):
            try:
                os.close(fd)
            except OSError:
                pass


def _shutdown():
    """Close any open tunnel. Returns (confirmed, relay) or None."""
    global _active
    if _active is None:
        return None

    display_fd, relay = _active
    _active = None
    relay.close()
    try:
        confirmed = _close_tunnel(display_fd)
    except OSError:
        confirmed = False
    try:
        os.close(display_fd)
    except OSError:
        pass
    return confirmed, relay


def _before_upload(source, target, env):
    global _active

    # A second upload in the same process must not leave the first tunnel's
    # descriptors and pump threads behind.
    _shutdown()

    try:
        port_name = _find_port(env)
        display_fd = _open_display(port_name)
    except Exception as exc:
        print("[PASSTH] upload cancelled: {}".format(exc), file=sys.stderr)
        print(
            "[PASSTH] esptool was not started; check the display USB port and firmware",
            file=sys.stderr,
        )
        return 1

    try:
        confirmed, seen = _arm_tunnel(display_fd)
        if not confirmed:
            # The command may have taken effect even if its confirmation was
            # lost, so attempt to leave the display in its normal mode.
            _close_tunnel(display_fd)
            raise RuntimeError(
                "display did not confirm PASSTH ON after 3 attempts; "
                "last output: {!r}".format(seen[-400:])
            )
        relay = _Relay(display_fd)
    except Exception as exc:
        try:
            os.close(display_fd)
        except OSError:
            pass
        print("[PASSTH] upload cancelled: {}".format(exc), file=sys.stderr)
        print(
            "[PASSTH] esptool was not started; check the display USB port and firmware",
            file=sys.stderr,
        )
        return 1

    # esptool talks to the pty; the real port stays in this process so no
    # open can pulse the display's EN pin.
    env.Replace(UPLOAD_PORT=relay.path)
    _active = (display_fd, relay)
    print(
        "[PASSTH] tunnel confirmed on {}; uploading through {}".format(
            port_name, relay.path
        )
    )
    return 0


def _after_upload(source, target, env):
    result = _shutdown()
    if result is None:
        return 0

    confirmed, relay = result
    print(
        "[PASSTH] relayed {} bytes out, {} bytes back".format(
            relay.bytes_out, relay.bytes_in
        )
    )
    if confirmed:
        print("[PASSTH] tunnel closed; display UI resumed")
    else:
        print(
            "[PASSTH] warning: upload completed but tunnel exit was not "
            "confirmed; send PASSTH OFF from the display serial console",
            file=sys.stderr,
        )
    return 0


def _cleanup_after_failed_upload():
    result = _shutdown()
    if result is None:
        return
    confirmed, relay = result
    print(
        "[PASSTH] upload did not complete; relayed {} bytes out, {} bytes back".format(
            relay.bytes_out, relay.bytes_in
        )
    )
    if confirmed:
        print("[PASSTH] tunnel closed after the failed upload; display UI resumed")
    else:
        print(
            "[PASSTH] warning: tunnel cleanup could not be confirmed; "
            "send PASSTH OFF from the display serial console if needed",
            file=sys.stderr,
        )


Import("env")

# Resolve the display port before the platform's upload action runs. Its
# built-in BeforeUpload step autodetects a port again; a concrete value here
# makes that step keep the port we arm with PASSTH.
if not _configured_port(env):
    candidates = _display_ports()
    if len(candidates) == 1:
        env.Replace(UPLOAD_PORT=candidates[0])

env.AddPreAction("upload", _before_upload)
env.AddPostAction("upload", _after_upload)
atexit.register(_cleanup_after_failed_upload)
