# 12. Copies & git state

## Initial commit

All three copies share initial commit `f96fb1b Initial water control system with Arduino and web interface` on branch `main`, tracking `origin/main`. Git user: `DextroColdwood <dextrocoldwood@gmail.com>`.

## Uncommitted changes (identical git status in all three copies)

```
Changes not staged for commit:
  deleted:    arduino/water_control_arduino.ino
  modified:   templates/index.html
  modified:   web_control.py
Untracked files:
  water_control_arduino/
```

i.e. the sketch was moved from `arduino/` to `water_control_arduino/` (directory rename) and both `index.html` and `web_control.py` were heavily reworked, but nothing has been committed.

## Divergence between copies

### UI
- **Live** (`~/water-control-system`): redesigned CarPlay/glassmorphism UI — left dock with clock + nav, perf strip, bottle grid, system mode button.
- **Desktop copies**: simpler flat blue theme, top header bar, no perf strip, no system-mode button, `MODE: AUTO/OFF` toggle in header.

### Firmware (water-level handling)
- **Live**: water-level sensing **bypassed** (`waterRaw = 1023`, `waterFaultCounter = 0`, `waterDepleted = false`) → dry-run protection disabled.
- **Desktop copies**: reads `analogRead(WATER_SENSOR_PIN)`, 5-cycle debounce against threshold 500 → dry-run protection enabled.

### `web_control.py`
Substantially expanded in both (multi-file diff vs the initial minimal version): resilient serial auto-reconnect, two worker threads, CSV logging engine, USB export, network/time routes, SocketIO with CORS. Differences between copies are cosmetic (formatting) plus the UI-facing field names.

## Implications

- There is **no single source of truth**; the "running" copy is the live one, but it contains a safety regression (bypassed water level) and an unimplemented `/sys_stats` route.
- None of the work is committed, so a fresh clone from `origin` would revert to the minimal initial version.
- Recommend: consolidate into one copy, re-enable water-level sensing, commit, and tag.

## Local documentation origin

This `raspi-docs/` folder was created on the operator's machine and transferred to the Pi at `~/Documentation`.
