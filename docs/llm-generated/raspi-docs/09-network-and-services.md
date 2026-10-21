# 9. Network & services

## Network

- Interface `eth0` UP on `10.92.128.23/21` (gateway `10.92.128.1`, default route), and `wlan0` also UP on `10.92.53.157/21` (gateway `10.92.48.1`), via NetworkManager + wpa_supplicant.
- `eth0` DOWN.
- Default route via `10.92.48.1` dev wlan0.
- DNS resolution works (probe `connectivitycheck.gstatic.com` used by captive portal).
- A `cloudflared-app1.service` is enabled and active (Cloudflare tunnel).

## Enabled services (selected)

| Service | Purpose |
|---|---|
| `ssh` / `sshd` | Remote shell (port 22) |
| `lightdm` | Graphical autologin/kiosk |
| `pm2-invitro` | Restores PM2 process list |
| `NetworkManager` (+ wait-online, dispatcher) | Wi-Fi |
| `wpa_supplicant` | Wi-Fi auth |
| `bluetooth` | Bluetooth |
| `cron` | Cron daemon (no user crontab) |
| `systemd-timesyncd` | NTP |
| `avahi-daemon` | mDNS |
| `cups` / `cups-browsed` | Printing |
| `ModemManager` | Modem management |
| `udisks2` | Disk/USB management |
| `cloudflared-app1` | Cloudflare tunnel |
| `cloud-init*` | Cloud-init (first-boot) |
| `rpi-eeprom-update` | EEPROM updates |
| `getty@` | Serial console |
| `apparmor` | MAC |

## Listening ports

- `22` — SSH
- `5000` — Flask `water-ui` (bound to 0.0.0.0)
- LightDM/X on VT7

## Security note (see §10)

SSH password auth with a trivial password, passwordless sudo, autologin, and an unauthenticated `/command` SHUTDOWN endpoint.
