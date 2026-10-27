# Raspberry Pi `water-control-system` mirror

`legacy/pi/water-control-system/` mirrors the live project at
`/home/invitro/water-control-system` on the `invitrocontroller` Raspberry Pi 3B.
It was first pulled over SSH on 2026-10-20 and is the place to edit before
deploying a change to the Pi. Future deploys should use the manual
[`sync_pi.sh`](../../sync_pi.sh); see [pi-sync-script.md](pi-sync-script.md).

The initial pull did not modify the Pi. The copy is of the **working tree**, not
of the initial commit, so it captured the then-uncommitted work; that work has
since been committed, and the mirror now tracks the repository. See
[`raspi-docs/12-copies-and-git-state.md`](raspi-docs/12-copies-and-git-state.md).

On 2026-10-20 the kiosk text was scaled by 20 % (`--font-scale: 1.2`), applied
to the mirror and deployed to the Pi. The mirror and the live file are identical
again; see [pi-kiosk-font-scale.md](pi-kiosk-font-scale.md).

On 2026-10-22 the `/raw` endpoint was added to `web_control.py` (a 500-line ring
buffer of received serial lines, with an `after` cursor). The change was made
here first, then copied to `/home/invitro/water-control-system/web_control.py`
and activated with `pm2 restart water-ui`. Newer changes should go through
`sync_pi.sh`, which performs the same copy and restart. The previous live file is backed up
on the Pi at `/home/invitro/web_control.py.bak-raw-20261022`. Route details:
[raspi-docs/04-backend-web-control.md](raspi-docs/04-backend-web-control.md).

On 2026-10-24 the actuators were changed to start OFF. `web_control.py` now sends `HEATER_OFF`, `PUMP_OFF` and `STIR:0:10:30` once on its first serial connection, and its cached `heater_mode` / `pump_mode` start at `OFF`. The kiosk page highlights `OFF` for heater and pump instead of `AUTO`. The firmware side of the same change is in [`legacy/master/master.cpp`](../../legacy/master/master.cpp);
see [legacy-master.md](legacy-master.md#3-boot-defaults). These edits are committed to the repository and will be deployed on the next `sync_pi.sh` run from `origin/master`.

## Layout

```
legacy/pi/water-control-system/
├── .gitignore
├── web_control.py                  # Flask backend (no SocketIO)
├── templates/
│   └── index.html                  # kiosk dashboard, live version
├── water_control_arduino/
│   └── water_control_arduino.ino   # Arduino master firmware
└── logs/
    ├── 2025-12-12.csv              # older experiment log (104 KB)
    └── 2026-10-19.csv              # latest log (2 rows; header predates the
                                   # Targ_C/Stir column rename; git-ignored)
```

The relative layout matches the Pi exactly. `legacy/pi/` also holds
`captive-portal-login.sh`, which lives at `/home/invitro/captive_portal.sh` on
the Pi, outside the project directory.

`water_control_arduino/water_control_arduino.ino` in this mirror is the
historical sketch. Since 2026-10-22 [`sync_pi.sh`](../../sync_pi.sh) overwrites
the Pi's copy with `legacy/master/master.cpp`; the checksum below describes the
repository snapshot, not the live file. See
[pi-sync-script.md](pi-sync-script.md#master-firmware-flashing).

## Checksums

| File | MD5 |
|---|---|
| `web_control.py` | `b424a3af1128975a45933b971d078768` |
| `templates/index.html` | `cf16f6f3d2f8bb1973513b28b0777cb4` |
| `water_control_arduino/water_control_arduino.ino` | `bc89f1143cf87f1d2b64acdad167a28b` |
| `logs/2025-12-12.csv` | `f0015017bf14092087b80083d519977f` |
| `logs/2026-10-19.csv` | `2e0ec0ad23c58eb2bd844ca07385a5d3` |
| `.gitignore` | `2b4f59772005f5c43e50664c3d1f7b82` |

The `logs/` files are git-ignored, so they are not in the repository: a fresh
clone of this monorepo does not contain them, and the two rows above describe
the copies on this machine. Re-run the refresh command below to pick up whatever
is on the Pi.

## What is not included

- **`.git`** (188 KB). The Pi project is a repository with origin
  `https://github.com/DextroColdwood/water-control-system.git`, but it is not
  mirrored: copying it would create a nested repository inside this monorepo.
  The remote `.git` is left in place and untouched.
- `__pycache__/`, virtual environments and other `.gitignore` entries. None were
  present on the Pi at pull time.

## Refreshing the mirror

The pull is read-only on the Pi. Re-running it replaces the local files with the
current remote working tree, discarding local edits; the remote `.git` stays
excluded:

```sh
ssh invitro@10.92.128.23 \
  'cd /home/invitro/water-control-system && tar --exclude=.git -cf - .' \
  | tar -xf - -C legacy/pi/water-control-system
```

## Relationship to the deleted `main_ui.html`

The repository previously held `legacy/pi/main_ui.html`, a snapshot of the kiosk
page with a rewritten stylesheet (see
[pi-kiosk-ui-css-redesign.md](pi-kiosk-ui-css-redesign.md)). It was removed on
2026-10-20 when the mirror was created. It remains in git history at commit
`0e6f68f`. It was not the live page: the live page is the mirrored
`templates/index.html`, which has the telemetry dock, packet counters, log
inspector and hotspot download, but still uses the original iOS-style CSS.

## Related documents

- [raspi-docs/03-project-overview.md](raspi-docs/03-project-overview.md) — what
  the application does.
- [raspi-docs/04-backend-web-control.md](raspi-docs/04-backend-web-control.md) —
  `web_control.py` routes and workers.
- [raspi-docs/05-frontend-ui.md](raspi-docs/05-frontend-ui.md) — kiosk UI.
- [raspi-docs/06-master-firmware.md](raspi-docs/06-master-firmware.md) — the
  Arduino sketch.
- [raspi-docs/07-data-and-logging.md](raspi-docs/07-data-and-logging.md) — CSV
  schema for the files under `logs/`.
- [pi-kiosk-font-scale.md](pi-kiosk-font-scale.md) — 20 % text scale and the
  deploy/reload procedure.

---
Content generated by `codebuff/deepseek-v4.1-flash` in `freebuff`.

Update by `xiaomi/mimo-v2.6-flash-slow` in `freebuff` (2026-10-27): verified the MD5 table against the files and corrected the log-schema and SocketIO notes.
