# Documentation review — all of `docs/`

**Date:** 2026-10-27
**Scope:** every file under `docs/`, cross-checked against the repository as it
stands on `master` (`65983a3`). The first pass of this review covered `legacy/`
only; the second pass widened it to the whole tree.
**Method:** a link check over all 60 Markdown files, then a line-by-line
comparison of each document against the code, build configuration and file
inventory it describes. The Raspberry Pi itself was unreachable, so statements
about the live Pi come from the 2026-10-19/20 SSH inspection and are marked as
such where they matter.

## 1. Fixes applied

| File | What was wrong | Fix |
|---|---|---|
| `docs/index.md` | `docs/images/` was listed but the directory does not exist | Row removed |
| `docs/index.md` | `learned-mistakes.md` and `pi-kiosk-ui-css-redesign.md` were not listed anywhere | Both indexed |
| `docs/index.md` | `firmware/motor-driver/` had an environment in `platformio.ini` but no index row | Row added |
| `docs/index.md` | `co2-calibrate` listed as ATmega328 Nano, `pressure-calibrate` as ATmega2560 | Corrected to Mega Mini and Uno, matching `platformio.ini` |
| `docs/index.md` | Row for `raspi-docs/04` said "Flask + SocketIO" | Now "Flask" |
| `docs/index.md` | Row for `raspi-docs/12` said "their uncommitted changes" | Now "their divergence" |
| `docs/llm-generated/sensor-calibration-firmware.md` §1, §4 | `co2-calibrate` documented as a Nano reading D2 | `[env:co2-calibrate]` became `megaatmega2560` on 2026-10-05 (`9cd3753`) and the source reads `kPwmPin = 7` |
| `docs/llm-generated/sensor-calibration-firmware.md` §5 | `pressure-calibrate` documented as a Mega Mini on D2/D3 with `SENSORID 3` | `[env:pressure-calibrate]` is `uno`; the source uses DOUT 4, SCK 7, `SENSORID 0` |
| `docs/llm-generated/sensor-calibration-firmware.md` §8 | Linked `subsystem-index.md`, which does not exist | Now links `../index.md` |
| `docs/llm-generated/raspi-docs/13-rest-api-reference.md` | `POST /control` was missing entirely — the summary table numbered 19 entries but slot 8 was the port-7514 server, so the main actuator endpoint had no entry | `POST /control` added as entry 4, later entries renumbered to 20, with the four `action` values documented from `web_control.py` |
| `docs/llm-generated/raspi-docs/13-rest-api-reference.md` | `/set_mode` described as "the only endpoint that changes actuator state"; `/status` documented a `fan` field and said `water_low` was "always false (sensing bypassed)"; heater-off section cited a 39.8 °C bath ceiling | Corrected: `/control` handles per-actuator overrides, `/status` returns `stirrer` plus `heater_mode` / `pump_mode` / `stir_mode` / `water_adc` / `water_thr`, water sensing is implemented in `master.cpp`, ceiling is 41.0 °C |
| `docs/llm-generated/raspi-docs/03-project-overview.md` | Said "Flask + SocketIO", "two daemon threads", "exhaust fan at 45 °C", "39.8 °C bath ceiling", and that the live copy had bypassed water-level sensing | Plain Flask, three workers (`serial_worker`, `polling_worker`, `telemetry_worker`), stirrer instead of a fan, 41.0 °C ceiling, and the copy table now points at `legacy/master/master.cpp` as the source of truth |
| `docs/llm-generated/raspi-docs/README.md` | Architecture diagram said telemetry is "re-broadcast over SocketIO"; TOC said "Flask + SocketIO"; diagram said `pump/fan/heater`; SSH listed as password auth | The kiosk polls `GET /status` over HTTP; diagram says `pump/stirrer/heater`; SSH noted as key-only since the inspection |
| `docs/llm-generated/raspi-docs/01-hardware-and-os.md` | "SSH password auth is enabled with the trivial password `password`" | Marked as the 2026-10-19/20 state, corrected against `AGENTS.md` (password login disabled, keys added) |
| `docs/llm-generated/raspi-docs/09-network-and-services.md` | Same SSH claim; `eth0` written as `/24` while `01` and `README` say `/21` | SSH corrected; the prefix-length conflict is now flagged instead of silently contradicting the other two files |
| `docs/llm-generated/raspi-docs/10-security-and-caveats.md` | Operational item 2 claimed water-level sensing was bypassed; item 7 claimed `/sys_stats` was never implemented; recommendations still asked for the password fix, the water-level fix, the commit, and `/sys_stats` | `/sys_stats` exists (`web_control.py:722`), water sensing is in `master.cpp`, the work is committed and deployed by `sync_pi.sh`, and the recommendations now carry done/open status |
| `docs/llm-generated/raspi-docs/11-key-paths.md` | `.ino` annotated "water level bypassed" | Annotated as written by `sync_pi.sh` from `legacy/master/master.cpp` |
| `docs/llm-generated/raspi-docs/12-copies-and-git-state.md` | Live-UI row claimed a "system mode button" | `templates/index.html` never calls `set_mode`; heater, pump and stirrer go through `POST /control`. Also recorded that `logs/` is git-ignored |
| `docs/llm-generated/raspi-docs/07-data-and-logging.md` | The 2026-10-19 log was called "current schema" | Its header still has `Mode` and `Fan`; `CSV_COLUMNS` now writes `Targ_C` and `Stir` (`web_control.py:471`) |
| `docs/llm-generated/dashboard.md` | File map omitted `src/history.ts` (132 lines) and `src/monitoring.ts` (28 lines); `dev-db.ts` counted 694 instead of 697; constant line numbers in §13.2 were 4–7 lines low; `getMetricValue` listed as dead code although `createChartPoints()` calls it; `.session-list` listed as a dead rule although it shares its rule with the used `.result-list` | File map, counts, line numbers and the dead-code table corrected |
| `docs/llm-generated/pi-water-control-system-mirror.md` | Paragraph run together (`...web-control.md).On 2026-10-24...`); layout comments said "Flask + SocketIO" and "current schema"; the uncommitted-changes paragraph predated the commits | Split into paragraphs, comments corrected, git state corrected. The MD5 table was re-run against the files: all six hashes match |
| `docs/llm-generated/SUNTON-luna.md` | Linked `CODE-INDEX.md` and `FILE-MANIFEST.txt`, which belong to the external archive and were never committed here | Named as plain text with an explanation |
| `docs/llm-generated/platformio-environments.md` | Verification count needed a date refresh | Re-counted: 16 `[env:*]` sections, `motor-driver` included |

## 2. Checked and left alone

These matched their source on this pass:

- `raspi-docs/04-backend-web-control.md` — the `current_data` block, the three
  workers, the 19-row route table and the CSV columns match `web_control.py`.
- `raspi-docs/05-frontend-ui.md`, `raspi-docs/06-master-firmware.md` — pins,
  thresholds (200 / 16 samples / 500 ms / 5 checks), boot defaults and the
  command list match `master.cpp` and `index.html`. The one static element, the
  `EXHAUST FAN — ALWAYS ON` badge, is a fixed label in the HTML, not telemetry.
- `raspi-docs/02-boot-and-kiosk.md`, `raspi-docs/08-auxiliary-tools.md`,
  `raspi-docs/14-leaf-reader.md` — no contradiction found.
- `legacy-master.md`, `legacy-slave.md`, `migration-plan.md`,
  `network-master.md`, `link-serial-protocol.md`, `comms-rework-plan.md`,
  `memory-safety-hardening.md`, `esp-db-sync.md`, `db-sync-experiment-ntp.md`,
  `display-docs.md`, `display-provisioning-ap.md`, `display-sim.md`,
  `display-debug-flag.md`, `display-lvgl-9-6-port.md`, `compile-commands.md` —
  all revised in the 2026-10-24 verification commit and still consistent with
  `firmware/` and `platformio.ini`.
- `pi-sync-script.md` — compared against `sync_pi.sh` line by line: modes,
  flags, environment defaults, the sparse checkout, the `.ino` exclusion, the
  `fuser` wait, the hash state file and the failure table all match.
- `database.md` — the 17 migrations, the five `device_data` tables and the
  grant/RLS description match `dashboard/supabase/migrations/`.
- `sensor-calibration-firmware.md` — the remaining calibration sketches match
  their sources; the standalone test sketches and their sections were removed.
- `raspi-docs/07-data-and-logging.md` — the current header matches
  `CSV_COLUMNS`; the two log files were read directly and the older-schema
  description matches `2025-12-12.csv`.

## 3. Open items

1. **`docs/hardware.md` links to removed MAX485 test code.** The link points to
   `../../firmware/max485-test/main.cpp`. This human-owned file must be updated
   by a human; it was not edited as part of this cleanup.
2. **Five documents have no generation footer** and were not edited in this
   pass: `raspi-docs/02-boot-and-kiosk.md`, `raspi-docs/08-auxiliary-tools.md`,
   `migration-notes.md`, `wt32-eth01.md`, `learned-mistakes.md`. AGENTS.md
   requires the footer on everything under `docs/llm-generated/`. Their original
   generator is unknown, so no attribution was invented for them.
3. **The Raspberry Pi was not reachable.** Everything about its live state —
   the three copies, the enabled services, what firmware is actually flashed,
   the `/21` versus `/24` prefix on `eth0` — still needs one SSH pass to
   confirm. The repository side of each claim is now correct.
4. **`lvgl-redesign.md` (1379 lines) has not been touched since 2026-10-14**,
   while `firmware/display-v9/` changed on 2026-10-24. It says on its first
   line that it "goes stale faster" and that its redesign tables are empty on
   purpose; treat the as-built blocks as a snapshot from 2026-10-14.
5. **`security-report.md` dates from 2026-10-01.** The findings it lists are
   about grants, RLS and headers rather than UI code, and the migration it
   describes (`20261001000000`) is still the one in the tree, so it was left as
   is. Re-run its verification queries if the deployed project has changed.

## 4. What a reader should take from this

Most stale text came from one cause: the water-level bypass was fixed in
`master.cpp` and the actuator boot defaults were changed on 2026-10-24, but
documents written before those commits kept repeating the old state. The other
cause is `platformio.ini` being edited without its documents — two board
assignments and one pin moved. When a fix lands in code, the four documents
that name it are `docs/index.md`, the subsystem document, `raspi-docs/`, and
`platformio-environments.md`.

---

Content generated by `xiaomi/mimo-v2.6-flash-slow` in `freebuff`.

---
Content generated by `openai/gpt-6-codex` in `harness`.
