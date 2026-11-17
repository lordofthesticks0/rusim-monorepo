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

**Re-verified 2026-11-17:** link check over all 61 Markdown files after the
documents moved from `docs/llm-generated/` to `docs/`, the footer rule changed
from provider attribution to the `Updated at` stamp, and the two test-sketch
documents (`water-level-test.md`, `pwm-pins-readout.md`) were removed because a
test is not a subsystem. Paths inside every document were repointed for the new
depth, `../platformio-environments.md` was re-counted (15 `[env:*]` sections), and
`../network-master.md` had its garbled em dashes repaired.

## 1. Fixes applied

| File | What was wrong | Fix |
|---|---|---|
| `docs/index.md` | `docs/images/` was listed but the directory does not exist | Row removed |
| `docs/index.md` | `../learned-mistakes.md` and `../pi-kiosk-ui-css-redesign.md` were not listed anywhere | Both indexed |
| `docs/index.md` | `firmware/motor-driver/` had an environment in `platformio.ini` but no index row | Row added |
| `docs/index.md` | `co2-calibrate` listed as ATmega328 Nano, `pressure-calibrate` as ATmega2560 | Corrected to Mega Mini and Uno, matching `platformio.ini` |
| `docs/index.md` | Row for `raspi-docs/04` said "Flask + SocketIO" | Now "Flask" |
| `docs/index.md` | Row for `raspi-docs/12` said "their uncommitted changes" | Now "their divergence" |
| `docs/sensor-calibration-firmware.md` §1, §4 | `co2-calibrate` documented as a Nano reading D2 | `[env:co2-calibrate]` became `megaatmega2560` on 2026-10-05 (`9cd3753`) and the source reads `kPwmPin = 7` |
| `docs/sensor-calibration-firmware.md` §5 | `pressure-calibrate` documented as a Mega Mini on D2/D3 with `SENSORID 3` | `[env:pressure-calibrate]` is `uno`; the source uses DOUT 4, SCK 7, `SENSORID 0` |
| `docs/sensor-calibration-firmware.md` §8 | Linked `subsystem-index.md`, which does not exist | Now links `../index.md` |
| `docs/raspi-docs/13-rest-api-reference.md` | `POST /control` was missing entirely — the summary table numbered 19 entries but slot 8 was the port-7514 server, so the main actuator endpoint had no entry | `POST /control` added as entry 4, later entries renumbered to 20, with the four `action` values documented from `web_control.py` |
| `docs/raspi-docs/13-rest-api-reference.md` | `/set_mode` described as "the only endpoint that changes actuator state"; `/status` documented a `fan` field and said `water_low` was "always false (sensing bypassed)"; heater-off section cited a 39.8 °C bath ceiling | Corrected: `/control` handles per-actuator overrides, `/status` returns `stirrer` plus `heater_mode` / `pump_mode` / `stir_mode` / `water_adc` / `water_thr`, water sensing is implemented in `master.cpp`, ceiling is 41.0 °C |
| `docs/raspi-docs/03-project-overview.md` | Said "Flask + SocketIO", "two daemon threads", "exhaust fan at 45 °C", "39.8 °C bath ceiling", and that the live copy had bypassed water-level sensing | Plain Flask, three workers (`serial_worker`, `polling_worker`, `telemetry_worker`), stirrer instead of a fan, 41.0 °C ceiling, and the copy table now points at `legacy/master/master.cpp` as the source of truth |
| `docs/raspi-docs/README.md` | Architecture diagram said telemetry is "re-broadcast over SocketIO"; TOC said "Flask + SocketIO"; diagram said `pump/fan/heater`; SSH listed as password auth | The kiosk polls `GET /status` over HTTP; diagram says `pump/stirrer/heater`; SSH noted as key-only since the inspection |
| `docs/raspi-docs/01-hardware-and-os.md` | "SSH password auth is enabled with the trivial password `password`" | Marked as the 2026-10-19/20 state, corrected against `AGENTS.md` (password login disabled, keys added) |
| `docs/raspi-docs/09-network-and-services.md` | Same SSH claim; `eth0` written as `/24` while `01` and `README` say `/21` | SSH corrected; the prefix-length conflict is now flagged instead of silently contradicting the other two files |
| `docs/raspi-docs/10-security-and-caveats.md` | Operational item 2 claimed water-level sensing was bypassed; item 7 claimed `/sys_stats` was never implemented; recommendations still asked for the password fix, the water-level fix, the commit, and `/sys_stats` | `/sys_stats` exists (`web_control.py:722`), water sensing is in `master.cpp`, the work is committed and deployed by `sync_pi.sh`, and the recommendations now carry done/open status |
| `docs/raspi-docs/11-key-paths.md` | `.ino` annotated "water level bypassed" | Annotated as written by `sync_pi.sh` from `legacy/master/master.cpp` |
| `docs/raspi-docs/12-copies-and-git-state.md` | Live-UI row claimed a "system mode button" | `templates/index.html` never calls `set_mode`; heater, pump and stirrer go through `POST /control`. Also recorded that `logs/` is git-ignored |
| `docs/raspi-docs/07-data-and-logging.md` | The 2026-10-19 log was called "current schema" | Its header still has `Mode` and `Fan`; `CSV_COLUMNS` now writes `Targ_C` and `Stir` (`web_control.py:471`) |
| `docs/dashboard.md` | File map omitted `src/history.ts` (132 lines) and `src/monitoring.ts` (28 lines); `dev-db.ts` counted 694 instead of 697; constant line numbers in §13.2 were 4–7 lines low; `getMetricValue` listed as dead code although `createChartPoints()` calls it; `.session-list` listed as a dead rule although it shares its rule with the used `.result-list` | File map, counts, line numbers and the dead-code table corrected |
| `docs/pi-water-control-system-mirror.md` | Paragraph run together (`...web-control.md).On 2026-10-24...`); layout comments said "Flask + SocketIO" and "current schema"; the uncommitted-changes paragraph predated the commits | Split into paragraphs, comments corrected, git state corrected. The MD5 table was re-run against the files: all six hashes match |
| `docs/SUNTON-luna.md` | Linked `CODE-INDEX.md` and `FILE-MANIFEST.txt`, which belong to the external archive and were never committed here | Named as plain text with an explanation |
| `docs/platformio-environments.md` | Verification count needed a date refresh | Re-counted: 16 `[env:*]` sections, `motor-driver` included |

## 2. Checked and left alone

These matched their source on this pass:

- `../raspi-docs/04-backend-web-control.md` — the `current_data` block, the three
  workers, the 19-row route table and the CSV columns match `web_control.py`.
- `../raspi-docs/05-frontend-ui.md`, `../raspi-docs/06-master-firmware.md` — pins,
  thresholds (200 / 16 samples / 500 ms / 5 checks), boot defaults and the
  command list match `master.cpp` and `index.html`. The one static element, the
  `EXHAUST FAN — ALWAYS ON` badge, is a fixed label in the HTML, not telemetry.
- `../raspi-docs/02-boot-and-kiosk.md`, `../raspi-docs/08-auxiliary-tools.md`,
  `../raspi-docs/14-leaf-reader.md` — no contradiction found.
- `../legacy-master.md`, `../legacy-slave.md`, `migration-plan.md`,
  `../network-master.md`, `../link-serial-protocol.md`, `comms-rework-plan.md`,
  `../memory-safety-hardening.md`, `esp-db-sync.md`, `../db-sync-experiment-ntp.md`,
  `../display-docs.md`, `../display-provisioning-ap.md`, `../display-sim.md`,
  `../display-debug-flag.md`, `../display-lvgl-9-6-port.md`, `../compile-commands.md` —
  all revised in the 2026-10-24 verification commit and still consistent with
  `firmware/` and `platformio.ini`.
- `../pi-sync-script.md` — compared against `sync_pi.sh` line by line: modes,
  flags, environment defaults, the sparse checkout, the `.ino` exclusion, the
  `fuser` wait, the hash state file and the failure table all match.
- `../database.md` — the 17 migrations, the five `device_data` tables and the
  grant/RLS description match `dashboard/supabase/migrations/`.
- `../sensor-calibration-firmware.md` — the remaining calibration sketches match
  their sources; the standalone test sketches and their sections were removed.
- `../raspi-docs/07-data-and-logging.md` — the current header matches
  `CSV_COLUMNS`; the two log files were read directly and the older-schema
  description matches `2025-12-12.csv`.

## 3. Open items

1. **`docs/hardware.md` has four broken links.** The documents it points at
   moved out of `docs/llm-generated/` and now sit directly in `docs/`, but the
   human-owned file was not edited: line 52 `llm-generated/wt32-eth01.md` (link
   text also says `wt32-eth01-LLM.md`), line 65 `llm-generated/SUNTON-luna.md`
   and `llm-generated/SUNTON-qwen.md`, and line 331
   `llm-generated/atmega2560-mega-mini.md`. Each target is now
   `./<name>.md`. The earlier reference to `firmware/max485-test/main.cpp` was
   in this section, not in `hardware.md`, and that directory no longer exists.
2. **Five documents had no generation footer** and were not edited in this
   pass: `../raspi-docs/02-boot-and-kiosk.md`, `../raspi-docs/08-auxiliary-tools.md`,
   `migration-notes.md`, `../wt32-eth01.md`, `../learned-mistakes.md`. Their original
   generator is unknown, so no attribution was invented for them. Resolved
   2026-11-17: provider attribution was retired from the footer rule, and every
   document under `docs/` now closes with the `Updated at` stamp instead.
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
`../platformio-environments.md`.

## 5. Content audit, 2026-11-18

After the test-sketch documents were removed, every document was re-read against
the source it describes. Facts and identifiers that had drifted were corrected:

- `../display-sim.md`, `../platformio-environments.md`: the stub is
  `sim/link_stub.cpp`, and `display-sim` no longer excludes (or stubs)
  `provision_ap.cpp`; the USB command list now matches `serial_cmd.cpp`.
- `../display-docs.md`: idle backlight timeout is 60 s (`UI_SLEEP_MS`), not 30 s.
- `../link-serial-protocol.md`: the `RUN` stage ticks every 30 s
  (`kIntervalMs`), not 5 s; 5 s is only the heartbeat.
- `../network-master.md`: renamed functions (`sbFetchDevices`,
  `sbFetchDeviceConfig`, `sbFetchExperiments`, `sbFetchLatestReadingExp`,
  `sbPushOneRound`, `dbPassTick`) and repaired three `Â§` sequences.
- `../sensor-calibration-firmware.md`: `co2-calibrate` now reads four PWM
  sensors plus one UART sensor and prints JSON (the section described a
  single-sensor version); the pressure calibration factor, the methane
  `RS_AIR`/`RS_SPAN`/`PPM_SPAN`/`PPM_AIR` values and fitted exponents were
  updated to the constants in the source, and the trailing section renumbered.
- `../memory-safety-hardening.md`, `../db-sync-experiment-ntp.md`,
  `migration-notes.md`: added a status note where the record describes code
  that has since been replaced (`provision_ap.cpp`, the `CREDS` line,
  Supabase build flags, `firmware/display/`), so the present-tense text cannot
  be mistaken for current behaviour.
- `../display-wt32-link.md`: `IO5`/`IO17` now carries the Mega slave, not a
  future RS485 bus.
- `../flash-factory-tool.md`: removed the deleted `[env:display]`.
- `../raspi-docs/06-master-firmware.md`, `../raspi-docs/13-rest-api-reference.md`:
  two pumps with separate overrides, water thresholds `400`/`460`/`550` with no
  recovery deadband, `A9` sampling, the full command list, and the `THRESH:`
  argument order.
- `../pi-water-control-system-mirror.md`, `../pi-kiosk-font-scale.md`: checksums
  re-computed from the repository (the old ones predated later page edits); the
  Pi-side hash is labelled as a 2026-10-20 observation.
- `../index.md`: the captive-portal script is `legacy/pi/`, and the
  `memory-safety-hardening.md` description no longer claims a live atomic ISR.
- `../database.md`: title no longer says "LLM Documentation".
- `../dashboard.md`: file-map line counts re-counted, and the test-suite row was
  removed under the rule that a test is not a subsystem.
- `../captive-portal-login-script.md`: the verify step names `probeOnline`, the
  function that exists, instead of `fetchData`.

Not verified: board-level datasheet claims in `../atmega2560-mega-mini.md`,
`../SUNTON-luna.md`, `../SUNTON-qwen.md` and `../wt32-eth01.md`; the live
Raspberry Pi and deployed database state.

---
Updated at 2026-11-18-00:12.
