# 10. Security & caveats

## Security

1. **Trivial SSH password** (`password`) with password auth enabled — full shell access for anyone on the network.
2. **Passwordless sudo** for `invitro` (`.sudo_as_admin_successful`).
3. **Autologin** into openbox — physical access = desktop session.
4. **Unauthenticated shutdown endpoint**: `POST /command {"command":"SHUTDOWN"}` powers off the Pi with no auth.
5. **No auth anywhere** in the web UI (settings, export, time change, shutdown).
6. `/get_drives` runs `sudo mount` and `/set_timezone`/`/set_time` run `sudo` — safe only because sudo is passwordless.
7. `captive_portal.sh` is sourced from `~/.bashrc` on every interactive login — runs the portal flow (prompts for credentials) on each shell; footgun.
8. Captive-portal credentials are prompted, not stored — good.

## Operational caveats

1. **No Arduino connected** → bioreactor telemetry offline; UI shows zeros/empty bottles; actuator control cannot run. Serial worker is in its reconnect loop. `/status` reflects defaults (temp 0.0, no bottles, mode AUTO, logging false).
2. **Water-level sensing bypassed in live firmware** (`waterRaw = 1023`) → dry-run protection disabled even when the master is connected.
3. **Multiple divergent UI copies** (live vs Desktop vs `%USERPROFILE%`) — the running one is the redesigned CarPlay UI; the simpler original lives on the Desktop. Risk of editing the wrong copy.
4. **Logging off**; last CSV is `2026-10-19.csv` (2 rows, all sensor fields `null`); the 2025-12-12 log uses an older schema.
5. **No USB drive mounted** at `/media/usb` → CSV export unavailable.
6. **Kiosk depends on localhost:5000**; if PM2 dies, Chromium shows an error page until resurrected.
7. **`/sys_stats` route referenced but not implemented** → settings perf strip never populates.
8. **`shutdown_button.py` is not auto-started** — the GPIO 17 button does nothing unless launched manually.
9. **Chromium shows SSL handshake errors** in `.xsession-errors` (network likely needs captive-portal auth); Google GCM deprecated-endpoint messages.
10. **openbox** complains about missing `PyXDG` and a missing Debian menu file (cosmetic).
11. **Cloudflare tunnel** (`cloudflared-app1`) is enabled/active — exposes the Pi remotely; combined with the above security issues this is significant.
12. **OpenCode** was only downloaded as a tarball (`/tmp/opencode_install_1557`); not installed. `~/.opencode/bin` is empty.
13. **`eth0` is up as well as Wi-Fi** — the Pi is reachable on both `10.92.128.23` (wired, default route) and `10.92.53.157` (Wi-Fi).

## Recommendations (not applied)

- Set a strong SSH password / disable password auth in favour of keys.
- Add authentication to the Flask app (at least for `/command`, `/set_*`, `/export`).
- Re-enable water-level sensing in the live firmware (use the Desktop copy's logic).
- Consolidate the three project copies into one git-tracked working tree; commit the uncommitted changes.
- Implement `/sys_stats` or remove the perf strip.
- Auto-start `shutdown_button.py` if the hardware button is intended to work.
- Mount USB drives on demand only; validate device paths.
- Remove the `captive_portal.sh` invocation from `~/.bashrc` (or guard it to non-interactive shells).
- Consider whether the Cloudflare tunnel should remain enabled.
