# The Backlog 
> For LLM agents: Add as little text as possible while still being descriptive. When things are done, **REMOVE** it. 

- [ALL] Remove LLM design artifacts (unnecessary descriptions like this).
- [ALL] Indonesian localization.
- [FIRMWARE] Delegate GPIOs in the master ESP for control.
- [ALL] Pin the Supabase CA in `network-master` `sbAuthenticate`/`sbFetchLatestExp`. Bundle GTS Root R4 + GlobalSign R3 + ISRG X1; `setCACert()` replaces the trust store, `setCACertBundle()` is too big. See docs/llm-generated/esp-db-sync.md §5.
- [ALL] Decide the `readings` write grant for the device: SECURITY DEFINER RPC, INSERT plus WITH CHECK policy, or a dedicated ingest role. See docs/llm-generated/esp-db-sync.md §6.
- [FIRMWARE] Mega Mini sensor firmware: sample the five sensors and serve them over RS485 Modbus. Does not exist yet; blocks all reading uploads. See docs/llm-generated/esp-db-sync.md §3. The WT32 `IO5`/`IO17` pair (and `IO14`) is now free for the MAX485 master.
- [ALL] `experiments` table so the dashboard can command a run. Nothing stores run configuration server-side; duration comes from the display UI. See docs/llm-generated/esp-db-sync.md §6.
- [SITE] Fix monitoring readings: Change it so only the latest experiment is shown on the monitoring tab.
- [FIRMWARE] Find the heap corruption behind the /info reboot (getHeapSize walk hangs; corrupt by end of setup). See docs/llm-generated/display-info-reboot.md.
