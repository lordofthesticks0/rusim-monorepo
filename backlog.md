# The Backlog 
> For LLM agents: Add as little text as possible while still being descriptive. When things are done, **REMOVE** it.

- Fix racing condition: When WT32 is connected and logged in but not display, the UI is not aware.
- Decide what to do with the experiment duration now that SoftAP is possible.
- Attempt to fetch real data now that connection is possible.
- Explore remote control via SoftAP possibility.
- Add network QR to `/info`.
- LVGL UI redesign.
- Remove LLM design artifacts (unnecessary descriptions like this).
- Indonesian localization.
- Keep language consistent: Bottle -> Chamber; Cluster -> Node
- Delegate GPIOs in the master ESP for control.
- Allow duration to be set after login: WT32 reads `0x0060` once in `captureDisplayCreds()`, so post-login edits are cosmetic until the master re-reads it. Decide route (`/run`, `/control`, `/setup` phase 2) and whether `experimentRunning` still flips at login.
- Fix monitoring readings: Change it so only the latest experiment is shown on the monitoring tab.
