# The Backlog 
> For LLM agents: Add as little text as possible while still being descriptive. When things are done, **REMOVE** it. 

- [ALL] Remove LLM design artifacts (unnecessary descriptions like this).
- [ALL] Indonesian localization.
- [FIRMWARE] Mega Mini sensor firmware: sample the five sensors and serve them over RS485 Modbus. Does not exist yet; blocks all reading uploads. See docs/plans-and-audits/esp-db-sync.md §3. WT32 `IO5`/`IO17` now carry the direct Mega TTL link (docs/plans-and-audits/migration-plan.md), so a MAX485 master needs other pins; `IO14` is still free.
- [ALL] Update `hardware.md`.
- [LEGACY] pH recalibration (slave, `ph_raw` counts, AVCC 5.1 V). Logs: `.logs/ph-calibration-2026-11-20/`. Have: 4.01 for 1-4; "7" for 2; 9.18 for 1, 2, 4. Todo: retake "7" for 1 and 4 (not settled, slopes inconsistent); confirm "7" is 6.86 or 7.00; bottle 3 (new sensor flat at ~471 in 9.18 and "7", test shorted BNC / swap probe, then retake 4.01); then write per-bottle table in `legacy/slave/new-slave.cpp` (counts or volts), update `docs/calibration.md` §4. Pots floor at ~524 counts (2.6 V), accepted.
- [FIRMWARE] Display stirrers + water pump buttons (`/control`, `g.stirrerOn`/`g.pumpOn`) have no transport: define the `rusim-line` words, WT32 handling and actuator wiring, then remove the "Not connected yet" notes. Same for the temperature setpoint.
- [FIRMWARE] Display trend: 12 min window (24 points × 30 s) and wiped after a 90 s outage. Longer window needs memory (move `trend` to PSRAM?) and a test-contract change. docs/plans-and-audits/lvgl-ui-ux-audit.md §7.
- [FIRMWARE] Host test "trend / one point per new frame" fails (already on ec70151): expects a trend point the 30 s trend step does not write.
- [FIRMWARE] Display: show real elapsed time for a run adopted after a display reboot (WT32 would need to send the run start time).
