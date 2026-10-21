# 1. Hardware & OS

## Board

- **Model:** Raspberry Pi 3 Model B Rev 1.2
- **Revision code:** `a02082`
- **Serial:** `000000005801f869`
- **CPU:** 4× ARM Cortex-A53 @ 1.2 GHz (default clock; no `arm_freq` override), 64-bit mode (`arm_64bit=1`)
- **RAM:** 856 MiB (+ 855 MiB zram swap)
- **GPU:** VideoCore IV (`gpu_mem=128`, `config_hdmi_boost=7`)
- **Storage:** 58 GB SD card `/dev/mmcblk0` — p1 510 MB FAT boot (`/boot/firmware`), p2 ext4 root (`/`)
- **Network:** `eth0` up on `10.92.128.23/21` (default route via `10.92.128.1`); Wi-Fi `wlan0` (brcmfmac) also up on `10.92.53.157/21`
- **Peripherals in use:** touchscreen (autotouch), USB serial to Arduino (when connected), HDMI display

`vcgencmd measure_temp` reported ~47.2 °C at the 2026-10-20 re-inspection.

## OS

- **Distro:** Debian GNU/Linux 13 (trixie), full version 13.7
- **Kernel:** `6.18.50+rpt-rpi-v8 #1 SMP PREEMPT Debian 1:6.18.50-1+rpt1 (2026-09-11)` (aarch64, Pi v8 flavour)
- **Locale/timezone:** `en_GB.UTF-8`, `Asia/Jakarta` (WIB +07:00), NTP synced via `systemd-timesyncd`
- **Hostname:** `invitrocontroller`
- **Boot partition:** `/boot/firmware` (FAT, 510 MB, 16 % used)

## Toolchain & versions

| Tool | Version |
|---|---|
| Python | 3.13.5 |
| Flask | 3.1.1 |
| Flask-SocketIO | 5.5.1 |
| pyserial | 3.5 |
| gpiozero | 2.0.1 |
| Node.js | v20.19.2 |
| npm | 9.2.0 |
| PM2 | 6.0.13 (global) |
| Arduino CLI | 1.3.1 (AVR core `arduino:avr@1.8.6`) |
| git | 2.47.3 (user `DextroColdwood <dextrocoldwood@gmail.com>`) |
| Chromium | 155.0.8059.79 |
| Xorg | X.Org X Server 1.21.1.16 (Pi GPU) |

Python dependencies are installed system-wide (no venv). Arduino libraries present: `DallasTemperature`, `OneWire` (under `~/Arduino/libraries`).

## Misc installed utilities

- `btop`, `yazi` (installed)
- `cloudflared` — referenced; a `cloudflared-app1.service` is enabled and active (tunnel)
- `rpi-clone` tool cloned under `~/rpi-clone`
- `unclutter`, `autotouch` (touchscreen helpers)
- `zenity` (used by the shutdown button dialog)

## User account

`invitro` (uid 1000), groups include `sudo`, `dialout`, `gpio`, `i2c`, `spi`, `video`. Sudo is passwordless (`.sudo_as_admin_successful` marker present). SSH password auth is enabled with the trivial password `password`.
