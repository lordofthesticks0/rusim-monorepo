# 5. Frontend UI — `templates/index.html`

Single-page kiosk dashboard rendered by Flask. The live copy uses a **CarPlay-style** layout: a left dock with clock + nav (Home/Controls/Bottle/Logs/Settings) + REC button, and a main viewport.

## Tabs

| Tab | Content |
|---|---|
| **Home (Water)** | Bath water temp card; avg bottle temp card (expandable per-bottle breakdown); system status card (water level with tap-to-expand ADC drawer, heater, pump, stirrer, exhaust fan). |
| **Controls** | Per-actuator mode overrides (`AUTO` / `FORCE ON` / `OFF`) for heater and pump; stirrer mode and timing controls; bottle hysteresis target controls. |
| **Bottle** | 4-bottle grid: temp, pH, methane ppm + mL, CO2 ppm + mL, pressure. |
| **Logs (Data)** | List of CSV log files; live log inspector; LAN QR download and USB drive export. |
| **Settings** | Pi perf strip (CPU load, RAM, SoC temp, SD storage, 5 V rail); packet statistics strip; device info (SSID/IP); refresh + shutdown; timezone + manual date/time. |

## Live data flow

- On load, `setInterval` polls `GET /status` every 1 s and calls `updateUI(data)`.
- `updateUI` updates bath temp, water level status and expandable ADC details (`water_adc`, computed voltage, `water_thr`), heater/pump/stirrer states (`STOPPED` when `water_low` is true), bottle grid, avg bottle temp, and the REC button.

## Expandable Water Level ADC Drawer

On the Home (`water`) tab, the right-hand system status card (`#waterStatusCard`) is clickable (`onclick="toggleWaterDetails()"`):
- By default, the `WATER LEVEL` row remains compact, displaying only `NORMAL` (green `.on` pill) or `CRITICAL LOW` (red `.warning` pill) alongside a chevron indicator.
- Tapping the card toggles `.expanded` on `#waterStatusCard`, revealing `.water-adc-drawer` directly beneath `WATER LEVEL` with three pills:
  - **RAW ADC** (`#waterAdcVal`): `<water_adc> / 1023` (or `--` before the first master frame).
  - **VOLTAGE** (`#waterVoltVal`): `<(water_adc * 5.0 / 1023).toFixed(2)> V`.
  - **MIN THR** (`#waterThrVal`): `≥ <water_thr>` (defaults to `≥ 200`).
- When `data.water_low` is true, `HEATER`, `PUMP`, and `STIRRER` badges on the Home card all display `STOPPED`.

## REC button

- `POST /toggle_logging` flips `logging_active`; button shows `REC ●` (recording) or `STOPPED`.

## Mode button

- Not present in the current live copy. Mode changes require `POST /set_mode` or the physical `D6` switch on the master. See [13-rest-api-reference.md](13-rest-api-reference.md).

## Export flow

1. User selects a log file and a USB drive.
2. `POST /export` with `{filename, dest_path}` → backend `shutil.copy2` to `/media/usb` + `sync`.
3. Success message + drive rescan.

## Shutdown

- "SHUTDOWN PI" → confirm dialog → `POST /command {command:"SHUTDOWN"}` → Pi powers off.

## Styling

iOS-style with solid surfaces: CSS variables (`--ios-blue`, `--ios-green`,
etc.), white cards on a pale blue background, 1 px dividers, rounded corners,
and the left CarPlay dock.

### Text scale

`--font-scale: 1.2` in `:root` scales text elements that reference it by 20 %. See
[pi-kiosk-font-scale.md](../pi-kiosk-font-scale.md) for the deploy and reload
procedure.

### Sidebar navigation buttons

The left CarPlay sidebar tabs (`.nav-btn`) have dimensions of 92×62 px, 25×25 px SVG icons, and `.nav-label` scaled via `calc(11.5px * var(--font-scale))` with font-weight 700. The clock time is scaled at `calc(21px * var(--font-scale))` (bold), while the date is rendered smaller at `calc(9px * var(--font-scale))` with normal weight (400).

## Physical Buttons Info Bar

An on-screen bottom legend bar (`#hwInfoBar`, class `.bottom-info-bar`) is displayed at the bottom of the main viewport exclusively on the Home (`water`) and Settings (`settings`) tabs with text `STIR · PUMP · HEAT`. It is hidden on Controls, Bottle, and Logs tabs to provide maximum vertical space for data grids and controls.

To align with physical buttons mounted on the chassis bezel outside the screen, the inner labels wrapper (`.hw-btn-labels`) is shifted 1.5 cm to the left:
- **Physical screen width**: 15.5 cm (7-inch panel).
- **Horizontal resolution**: 1024 px (as documented in [`pi-kiosk-font-scale.md`](../pi-kiosk-font-scale.md)).
- **Pixel density**: $1024\text{ px} / 15.5\text{ cm} \approx 66.06\text{ px/cm}$.
- **Offset calculation**: $1.5\text{ cm} \times 66.0645\text{ px/cm} = \frac{1.5}{15.5} \times 1024\text{ px} \approx 99.1\text{ px} \approx 99\text{ px}$.
- **Implementation**: Defined in `:root` as `--hw-bar-offset-x: -99px` and applied via `transform: translateX(var(--hw-bar-offset-x, -99px))` on `.hw-btn-labels`.

## Bottle Cards Styling

Bottle cards in `#pageBottles` use a 1 px solid `--divider` border and `--text-primary` header titles (`BOTTLE 1`–`BOTTLE 4`).

## pH Metric Display

In the Bottle grid, pH is formatted via `fmtMetric(b.ph, "", -1)` as a dimensionless numerical value (e.g. `6.85`).

## Settings Tab Centering

On the Settings tab (`#pageSettings`), `.settings-grid` (Device Info and Date & Time cards) centers vertically in the available viewport height:
- `#pageSettings.active` inherits `.page.active` (`flex: 1; display: flex; flex-direction: column; justify-content: center;`).
- With the telemetry dock anchored at the top and `#hwInfoBar` anchored at the bottom, `.settings-grid` is vertically centered in the remaining space (equal margin above and below).

---
Content generated by `google/gemini-4-argon` in `antigravity`.
