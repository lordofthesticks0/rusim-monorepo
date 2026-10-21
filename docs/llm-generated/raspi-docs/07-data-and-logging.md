# 7. Data & logging

## CSV format (current schema)

File: `logs/YYYY-MM-DD.csv`, one row per second while logging is on.

Header (aligned columns, ` , ` separated):
```
Timestamp, Bath_C, BAvg_C, Mode, Heater, Pump, Fan, Water, Pkt_Tot, Pkt_TX,
Pkt_RX, Pkt_Lost, Loss_%, B1_Temp, B1_pH, B1_CH4, B1_CH4mL, B1_CO2, B1_CO2mL,
B1_Pa, ... B4_Pa
```

- One row per second while logging is on (`log_data_to_csv()` in the 1 s worker loop).
- Timestamp is `HH:MM:SS`; `Mode`, `Heater`, `Pump`, `Fan` and `Water` are text (`AUTO`, `ON`/`OFF`, `LOW`/`OK`).
- Missing or stale master/slave data, and sentinel values (`-999`, `-1`, or anything `<= -50`), are written as `null`.
- `Pkt_*` and `Loss_%` are serial packet counters/statistics, not sensor readings.

## Existing logs

- `~/water-control-system/logs/2026-10-19.csv` — 1.3 KB, 2 rows, current schema with packet stats; all sensor fields `null` (no Arduino).
- `~/water-control-system/logs/2025-12-12.csv` — 104 KB, 838 rows, older schema (see below).
- `~/Desktop/water-control-system/logs/2025-12-12.csv` — 104 KB copy (Oct 13); the Desktop copy has no 2026 log.
- Logging is **off** at the 2026-10-20 re-inspection (`logging_active = False`).

## Older schema (2025-12-12.csv)

The `2025-12-12.csv` log uses an earlier schema:
`Water_Temp_C, Heater_On, Pump_On, Water_Low, B1..B4 Temp/pH/CH4/CO2/Press` (no CH4/CO2 volumes, no bath avg, no mode/fan). The backend's current writer produces the new schema; old files are left as-is.

## USB export

- Endpoint `POST /export` copies a selected log to `/media/usb` and runs `sync`.
- `GET /get_drives` looks for an `sd*` block device and mounts it at `/media/usb` with `uid=1000,gid=1000`.
- No USB drive mounted at the 2026-10-20 re-inspection (`/media/usb` empty).
- Export is unavailable until a drive is plugged in.

## Storage

SD card: 58 GB, ~7.1 GB used, ~49 GB free (13 %). Logs are gitignored (`logs/` in `.gitignore`).
