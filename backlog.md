# The Backlog 
> For LLM agents: Add as little text as possible while still being descriptive. When things are done, **REMOVE** it. 

- [ALL] Remove LLM design artifacts (unnecessary descriptions like this).
- [ALL] Indonesian localization.
- [FIRMWARE] Mega Mini sensor firmware: sample the five sensors and serve them over RS485 Modbus. Does not exist yet; blocks all reading uploads. See docs/llm-generated/esp-db-sync.md §3. WT32 `IO5`/`IO17` now carry the direct Mega TTL link (migration-plan.md), so a MAX485 master needs other pins; `IO14` is still free..
- [ALL] Update `hardware.md`.
