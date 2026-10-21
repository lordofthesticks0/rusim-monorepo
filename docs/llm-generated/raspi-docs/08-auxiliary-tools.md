# 8. Auxiliary tools & scripts

## In home (`/home/invitro`)

| Path | Purpose |
|---|---|
| `analog_test/analog_test.ino` | Scans analog pins A0–A15, prints any reading > 300. |
| `test_relays/test_relays.ino` | Sets pump (D3) and heater (D5) to `INPUT` (high-Z) — disables relays (safe state). |
| `test_serial.py` | Opens `/dev/ttyUSB0` at 115200, sends `STATUS` in a loop, prints replies. Manual serial debug. |
| `shutdown_button.py` | Watches GPIO 17 (hold 2 s) and pops a zenity confirmation to shut down the Pi. Not auto-started. |
| `captive_portal.sh` | Captive-portal login script (curl-based) mirroring a firmware login flow. Invoked from `~/.bashrc` on every interactive login. Not in cron. |
| `rpi-clone/` | `rpi-clone` tool (git clone) for cloning the SD card to another device. |
| `Arduino/libraries/` | `DallasTemperature`, `OneWire`. |
| `Arduino/` (no sketches) | Arduino IDE/CLI workspace; AVR core 1.8.6 installed; no boards connected at the 2026-10-20 re-inspection. |

## Misc

- Empty marker files `Auto`, `On`, `Off` (dated 2025-11-25) and a named pipe `.lgd-nfy0`.
- `autostart.log` (261 B) — openbox/autostart log.
- `.terminfo` adds `xterm-kitty`.
- `.opencode/` exists (empty `bin/`) — OpenCode was downloaded as a tarball in `/tmp/opencode_install_1557` but not installed.

## `bin/`

`~/bin` is empty (but on `PATH` via `.profile`).
