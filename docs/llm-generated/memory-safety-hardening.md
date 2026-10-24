# Memory-safety hardening (`display-v9`, `network-master`)

Scope: `firmware/display-v9/main.cpp`, `firmware/display-v9/provision_ap.cpp`,
`firmware/network-master/main.cpp`. Build: `pio run -e display-v9`,
`pio run -e network-master`, `pio run -e display-sim` (all pass).

## display-v9: RAII ownership

- `bus` / `lcd` (`main.cpp`): were global raw `new`, never freed. Now
  `static std::unique_ptr<...>` allocated in `setup()` after the heap is ready,
  single ownership explicit, lives until reboot. Also fixed the
  `DISPLAY_DEV_KIT` path so `gfx` vs `lcd` code compiles on either config
  (`my_disp_flush`, `setup` init, width/height, fill).
- `disp_draw_buf` (`main.cpp`): was raw `malloc` / `heap_caps_malloc`, never
  freed. Now `std::unique_ptr<uint8_t[], DispBufDeleter>` where the deleter
  calls `heap_caps_free` on ESP32 and `free` elsewhere. Passed to LVGL via
  `.get()`. Allocation failure still guards before `lv_display_create`.
- `s_server` (`provision_ap.cpp`): was raw `new WebServer`, `stop()` only.
  Now `std::unique_ptr<WebServer>`; `provision_ap_end()` calls `stop()` then
  `reset()`, freeing heap while the AP is down. `begin()` recreates on demand.
  `provision_ap_poll()` null-guards. C++11 note: no `std::make_unique`
  (toolchain is C++11), so `std::unique_ptr<T>(new T(...))` is used.

## network-master: bounded buffers

- Credentials: `username` / `password` were Arduino `String` globals. Now
  `char[65]` (`LINK_USER_MAX` / `LINK_PASS_MAX` = 64, plus NUL). A `CREDS` line
  is split with `arg_lookup()`, percent-decoded by `pct_decode()` into stack
  buffers, trimmed in place with `trimInPlace()`, validated non-empty, then
  copied with `strncpy` and the stack copies `memset`. `clearCreds()`
  overwrites then zeroes. `doLogin()` logs `username` directly.
- Auth metadata: `dbOwnerName` / `dbOwnerEmail` are `char[64]` globals filled
  by `sbParseIntField()` / `sbParseStrField()`, which scan the one-shot
  authenticate reply with `strstr` + bounded copy (truncate at `cap - 1`,
  NUL always written). Missing keys keep defaults; explicit JSON null
  yields empty. No `String` temps per field.
- HTTP bodies: added `readBodyFixed(http, out, cap)` capped at `kMaxBody`,
  `memcpy` chunks, NUL-terminates, returns length. `readBody()` kept as a
  one-shot wrapper (single `String` alloc from the fixed buffer) so existing
  callers (`fetchPortalUrl`, `postCredentials`, `fetchData`) keep working
  without per-chunk `concat` realloc churn.
- Push queue: `s_pushQueue[1200]` static ring of 40-byte `PushRow` structs
  (~48 KB) plus a static 6 KB JSON body (`kPushBodyCap`, separate from the
  portal's `kMaxBody`). Oldest-drop on overflow with a saturating
  `uint32_t` drop counter; no `malloc`/`String` in enqueue, format, or
  flush. `pushFormatHeadRound()` builds the POST body with cursor-checked
  `snprintf`/`memcpy` and bails returning 0 on any overflow instead of
  truncating silently. Dequeue happens only on HTTP 2xx.
- Page scan: `PageScan.formAction[256]` / `redir[256]` / `magic[128]` replace
  `String` fields. `valueAfterFixed()` uses `strstr`/`strchr` + bounded
  `memcpy`. `scanPage()` keeps a `char win[321]` rolling window with
  `memmove` eviction; no `String` in the hot loop. `fetchLoginPage()` checks
  `scan.redir[0]` / `scan.magic[0]`. URL helpers (`resolveAction`,
  `urlEncode`) still take one-shot `String` temps at the call site.
- Mega slave poll: one fixed `char sensRx[513]` frame plus `size_t sensRxLen`,
  a `bool` overflow flag and `uint32_t` counters. No `String`, no JSON
  library. `sens_float_field()` searches only within `[begin, end)`, where
  `end` is the next bottle object or the buffer's NUL, and rejects a `strtof`
  that stops past `end`. A frame over 512 bytes is rejected, never
  truncated, and the 1500 ms read budget bounds the blocking section.
- The old `IO11 -> IO14` poll-request ISR (`volatile bool s_pollRequest`,
  later `std::atomic<bool>`) is gone with the Modbus link. `rusim-line` is
  peer-to-peer and polled from `loop()`, so there is no ISR-shared flag to
  keep atomic. See `link-serial-protocol.md` §3.3.

## display-v9: DB fields from the `DB` line

- `link_serial.cpp` stores `s_dbBottles` (`int`, `-1` unknown) and
  `s_pushDrops` (`uint16_t`, saturating) from the `DB bottles=` and `drops=`
  fields, exposed through `link_serial_get_db_bottles()` and
  `link_serial_get_push_drops()`. The `DB latest=` field uses `-` for unknown
  and `0` for "no rows", which `link_serial` maps to `-2`/`-1`. `/info`
  formats the DB tile into a `char[32]` with `snprintf`, ASCII only (no glyph
  risk from the compressed font).

## Verification

- `pio run -e display-v9` — SUCCESS (RAM 36.0%, Flash 34.4%).
- `pio run -e network-master` — SUCCESS (RAM 19.0%, Flash 75.5% after the
  Mega slave poll was added; the figure was 17.7% / 75.2% at the time of this
  hardening pass).
- `pio run -e display-sim` — SUCCESS (unchanged shared screens + new register stubs).
- Queue/auth-meta/format logic: host test compiling the real functions
  (`sbParseIntField`, `sbParseStrField`, `sbParseAuthMeta`,
  `pushEnqueueRound`, `pushFormatHeadRound`) against Arduino stubs —
  auth reply shapes, clock-gate refusals, exact JSON output, oldest-drop
  counting to cap — all pass. The 1200-row ring is still folded away by the
  optimizer (absent via `nm`); the 6,144-byte JSON body is present. Both
  materialize fully when an enqueue caller lands (see `migration-plan.md`
  §7).
- Sensor poll: the same host-test method, compiling the real `sens_poll`,
  parse helpers and `sens_report_round` against Arduino stubs, covers the
  `legacy-slave.md` reply, invalid markers, a missing field, ids outside 1-4,
  a timeout, a non-JSON frame, the 512/513-byte boundary and the failure-log
  throttle. All pass. See `migration-plan.md` §7.
- Hardware refs: ESP32-S3 display + WT32-ETH01 per `docs/hardware.md` §2-3;
  the Mega slave link uses `IO5`/`IO17` (see `network-master.md` §2.2).
- The display link itself is the `rusim-line` peer on UART0 `IO1`/`IO3`;
  the register map named in earlier revisions of this document no longer
  exists (`link-serial-protocol.md`).

---
Content generated by `codebuff/deepseek-v4.1-flash` in `freebuff`.
