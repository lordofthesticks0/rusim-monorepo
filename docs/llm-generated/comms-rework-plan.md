# Comms rework plan: display <-> WT32 (Modbus RTU -> raw line commands)

Plan for reworking the link between the LVGL display and the network master.
This document is the blueprint; it was implemented on 2026-10-24 and the
resulting wire specification now lives in `link-serial-protocol.md`. Read that
file as the source of truth, not this one. The Modbus link described in
`display-wt32-link.md` is retired.

Requirements from the user (2026-10-24):

1. Replace the master-slave Modbus RTU method with raw commands over serial.
2. Move the link onto the reserved WT32 `IO1`/`IO3` (the Debug/Programming
   header pins, "meant for the USB" - the USB-to-UART adapter pins).
3. Logging that is meant to go to the WT32's USB must be received by the
   display and forwarded to the display's own USB port for debugging.
4. Later: RS485 goes on the ports the WT32 currently uses to receive UART
   from the display (the AT-header pair, `IO5`/`IO17`).

The user has ruled: implement without asking further questions; the decisions
below are final unless the user revisits them.

## 1. Scope

| In scope | Out of scope |
|---|---|
| The display <-> WT32 wire, protocol, firmware on both ends | The Supabase/portal/Ethernet logic inside the WT32 (unchanged) |
| Forwarding WT32 serial logs through the display's USB | The display UI screens (unchanged apart from the API rename) |
| Freeing WT32 `IO5`/`IO17` and pinning them for the later RS485 bus | The Mega Mini sensor firmware (its own backlog item, `esp-db-sync.md` §3) |
| `env:display-sim` stub so the PC build keeps linking | LAN8720 wiring, PCB, Supabase migrations |

Both ends must ship together. A new-protocol display against an old
`[MB]`-style Modbus master just sees no valid frames, so there is no
degradation path - flash both in one session (§9).

## 2. Current state (what is being replaced)

- One TTL UART + one out-of-band wire. Display `IO17` (TX) -> WT32 `IO5` (RX,
  AT-header), WT32 `IO17` (AT-header TXD) -> display `IO18` (RX), plus
  display `IO11` -> WT32 `IO14` "poll now" pulse line. 9600 8N1.
- Modbus RTU, WT32 = master (poller), display = slave ID 1. FC 0x03/0x06/0x10
  over a duplicated register map, plus CRC16 on every frame. Master polls a
  credentials register block, pushes IP/ping/link state, quick-polls
  duration/exp number/running flag every cycle.
- Files: `firmware/display-v9/link_modbus.{h,cpp}` (slave), the master lives
  inside `firmware/network-master/main.cpp`.
- Otherwise the `network-master` needs the display only for: credentials,
  portal verdict feedback, live network status on /info, DB state on /info,
  experiment state (duration, number, running).

## 3. Target physical layer

| WT32 pin | Display pin | Direction | Note |
|---|---|---|---|
| Debug header `TXD` = **IO1** (UART0 TX) | `IO18` (RX) | WT32 -> display | commands + log forwarding |
| Debug header `RXD` = **IO3** (UART0 RX) | `IO17` (TX) | display -> WT32 | commands |
| GND | GND | - | direct wire, shared USB ground is not acceptable |

Display pins stay where they are (`IO17`/`IO18`, `Serial1`); only the WT32 end
of the harness moves from the AT header to the Debug header. The
`IO11 -> IO14` poll-request line is **deleted outright**: with a peer protocol
the display can send whenever it wants, so "poll now" has no function.
Display `IO11` goes unconnected and WT32 `IO14` goes free. That leaves the
WT32 `IO5`/`IO17` pair entirely free for the RS485 bus (§11).

Settings: **115200 8N1** on both ends. Rationale: matches both boards'
existing monitor speeds and the WT32 `Serial.begin(115200)`; at 115200 a
worst-case line (1 KB) takes ~90 ms, and heartbeats cost ~40 B each, so
traffic is negligible.

Boot behaviour improvements this move buys:

- `IO1`/`IO3` carry no boot strapping on the ESP32 (the removed `IO5` was a
  strapping pin and forced the "power the WT32 first" rule). A UART transmitter
  idles HIGH, so the display drives `IO3` correctly at WT32 reset. Powering up
  in any order becomes safe.
- Raw `Serial` output of the WT32 (UART0) now physically reaches the display
  instead of an unattached adapter: the log forwarding requirement is almost
  free - the display just has to parse and re-route those lines.

Two cautions to document:

- Do **not** confuse `IO1`/`IO3` with the display board: on the ESP32-S3
  Sunton, GPIO1 and GPIO3 are RGB-panel blue-channel pins (`main.cpp` bus
  definition, B1/B4). The reserved USB-looking pair exists only on the WT32
  Debug header.
- With the link on UART0, the WT32's flash path changes (§5).

## 4. Protocol: `rusim-line` v1

Peer-to-peer line protocol, no master, no registers, no CRC, no acks.

### 4.1 Line rules (normative)

- Every message is one ASCII line, `\n` terminated. `0x20`-`0x7E` only.
  Receivers ignore stray `\r`.
- Words are exact-match UPPERCASE; argument keys/values are lowercase as
  sent. Unknown or wrong-direction lines are silently ignored (this is also
  how power-on trash and WT32 ROM boot text are swallowed).
- Max line length 1024 bytes, both directions. A partially received line is
  discarded when no byte arrives for 50 ms (`LINK_GAP_MS`), so a truncated
  line never wedges the parser.
- Arguments are `key=value` tokens separated by one space.
- Percent-encoding (`%XX`, hex, uppercase) applies **only** to `user=` and
  `pass=` values: any byte outside `[0-9A-Za-z-._~]`, plus `%` and `=` itself.
  No encoding anywhere else: `LOG` payloads are verbatim text.
- `LOG` payloads must not contain raw CR/LF: the WT32 log helper escapes
  `\n` -> `\\n`, `\r` -> `\\r`, so one log entry is always one line.
- No checksums. Any corrupted line is an unknown line, or a wrong value that
  the next heartbeat overwrites <=5 s later. Idempotent state re-transmission
  is the whole error strategy (§4.4). Both boards keep their Modbus-era
  `CRC16` code, but only RS485 uses it later (§11).

### 4.2 Command set

Direction display <- WT32:

| Command | Payload | Sender cadence | Display action |
|---|---|---|---|
| `HELLO` | - | once at WT32 boot | log link-up; clear local verdict/creds-pending bookkeeping (the staged pair itself is kept and re-pushed, which is what a fresh master wants) |
| `LOGINREQ` | - | every heartbeat while the master needs credentials | raise /setup (idempotent; same as today's register) |
| `RESULT` | `OK` or `FAIL` | after every login attempt; re-sent once as `OK` on `HELLO` if the master is already logged in | `OK`: stop credential re-push, route home; `FAIL`: /setup retry text + phone-page banner (existing result path); today's "RESULT wiped by LOGIN_REQ=0" race disappears because verdicts are plain messages |
| `NET` | `ip=<str> up=<0|1> [ping=<n|->]` | every heartbeat + on every ETH event | /info status store (IP, latency verdict, age stamp). `up=0` carries empty `ip=` and no `ping=` |
| `DB` | `latest=<n|-> time=<0|1> bottles=<n|-> drops=<n>` | every heartbeat + after each DB pass | /info DB store. `-` = unknown (old `0xFFFF`); `latest=0` keeps today's "no rows yet -> suggest 1" mapping |
| `LOG` | `<text>` | continuously | print `"[W32] " + text` on the display's USB console, verbatim, never fed through a printf format string |

Direction WT32 <- display:

| Command | Payload | Sender cadence | WT32 action |
|---|---|---|---|
| `HELLO` | - | once at display boot | log link-up; immediately re-send the current truth (`NET`, `DB`, `LOGINREQ` if the gate is needed, `RESULT OK` if running). This closes today's hole where a display reboot mid-run left a stuck /setup gate forever |
| `EXPERIMENT` | `run=<0|1> num=<n> dur=<1-99>` | on every change (emitted by the same setter the screens already call) + every heartbeat | store as today's quick-poll did; log only on change. Required after a WT32 reboot mid-run (it must not queue rows against a guessed experiment) |
| `CREDS` | `user=<pct> pass=<pct>` | when the user confirms a pair, then every heartbeat while no verdict for that pair has arrived | decode, trim, empty -> ignore. Same pair as last attempted -> re-emit the stored verdict. Changed pair -> run the portal login (`STAGE_LOGIN`). Buffers overwritten/zeroed after use, RAM-only as today |
| `DUMP` | - | bench only, `IS_DEBUG` builds | emit a `LOG [DUMP] ...` snapshot of local state |

The display ignores display-direction words arriving on its RX, the WT32
likewise. Loopback benching (RX shorted to TX) therefore produces no loops.

### 4.3 What is intentionally gone

- Master/slave roles, FC codes, slave ID, the duplicated register map and all
  register defines on both ends.
- The `CREDS_READY` handshake and the `IO11 -> IO14` "poll now" line: staging
  a pair simply transmits `CREDS` immediately, so a corrected password takes
  effect on the spot, not up to 5 s later.
- Polling entirely: both ends declare truth on a cadence; there is no
  request/response anywhere on the link.

### 4.4 Timing budget

| Event | Period |
|---|---|
| Heartbeat both ends (`LINK_BEAT_MS`), carrying each side's full current truth | 5 s |
| Immediate pushes | on staging credentials, on every experiment change, on every ETH event, after each DB pass, after each login attempt |
| Line gap discard | 50 ms |
| Door slams (`HELLO` snapshot) | once per boot, both ways |
| Wire load worst case | a few hundred bytes/s, vs ~11 KB/s line capacity at 115200 |

## 5. Flashing the WT32 (the price of moving onto IO1/IO3)

The Debug header previously belonged to the USB-to-UART adapter. Two paths
remain; both involve `PASSTH` (§6.3):

1. **Primary (unchanged drill):** adapter on the Debug header, keep the
   display link wires connected only if the display is silent - run
   `PASSTH ON` first (§6.3) or unplug one link wire, because the display's
   command pushes would fight the adapter on `IO3` (both push-pull outputs).
   Power-up behaviour is otherwise safe (UART idle HIGH).
2. **Sparing (new):** adapter on the display's own USB, then `PASSTH ON`
   turns the display into a raw bit tunnel: USB -> `IO3`, `IO1` -> USB.
   Flash with the automatic reset path disabled (no RTS/DTR pipe through the
   tunnel): jumper `IO0` to GND, pulse `EN` low, then
   `pio run -e network-master -t upload --upload-port <display USB port>`
   (expect to fall back to `--before no_reset`; verify at the bench). Marked
   experimental until proven; it must not be documented as the default path.

**Reminder for the user (standing rule from `hardware.md`): do not lose the
jumper that pulls IO0 low.**

In both paths the WT32's own debug output now carries the `LOG ` prefix; a
bare adapter simply shows the prefix as part of the text. Boot ROM text comes
out unprefixed and is dropped by the display parser, or is visible raw via
`PASSTH`/the bare adapter.

## 6. Display end (`firmware/display-v9`)

### 6.1 Module swap

Replace `link_modbus.{h,cpp}` with `link_serial.{h,cpp}` (mechanical rename of
the public surface; the register map, CRC, exception handling and notify-pin
code are deleted):

| Old symbol | New symbol | Change |
|---|---|---|
| `link_modbus_init` / `link_modbus_poll` | `link_serial_init` / `link_serial_poll` | line parser + heartbeat tick, `Serial1` unchanged (RX 18 / TX 17, now 115200) |
| `link_modbus_take_login_request` | `link_serial_take_login_request` | same semantics (one-shot event) |
| `link_modbus_set_credentials` | `link_serial_set_credentials` | stores pair, emits `CREDS` immediately; re-emits on heartbeats until a verdict; no pulse |
| `link_modbus_set_exp_state` | `link_serial_set_exp_state` | emits `EXPERIMENT` immediately on change + on heartbeats |
| `link_modbus_creds_ready` / `link_modbus_result` / `link_modbus_take_result` | same `link_serial_*` names | unchanged semantics |
| `link_modbus_get_ip` / `get_ping_ms` / `net_up` / `net_age_ms` | same names | age stamps now track heartbeat cadence |
| `link_modbus_get_db_latest` / `time_ok` / `get_db_bottles` / `get_push_drops` | same names | value domains unchanged (`-2` unknown, `-1` no rows, `>=0` an id) |
| `link_modbus_debug_print` | `link_serial_debug_print` | same fields, no register framing words |
| `link_modbus_crc16`, `link_notify_pulse` | deleted | CRC moves to the future RS485 module; the pulse line is gone |

Every UI call site (`screen_*`, `provision_ap`, `ui_shell`, `serial_cmd`,
`main.cpp`) keeps its logic and only renames the symbol - this is the reason
the surface is held 1:1.

### 6.2 Log forwarding and echo

`link_serial` gains two extra jobs beyond the old module:

- Lines starting `LOG ` are printed to the display's USB `Serial` as
  `[W32] <payload>`. Everything else inbound is either a known command or
  noise. Do not run the payload through a `printf` format string; render with
  `Serial.print("[W32] ")` + `Serial.println(payload)`. No rate-limiting at
  115200; the current log volume is tiny.
- Every emitted command is echoed locally as one `[LINK>] <word> [args]`
  line on the USB console, with the `CREDS` payload logged as the word only
  (mirroring today's user-only `[LINK] creds staged` line). This console
  echo is the primary bench signal during bring-up.

### 6.3 Passthrough mode (`PASSTH`)

New display-local mode enabling the flash/log tunnel of §5:

- Entered/exited by the USB serial commands `PASSTH ON` / `PASSTH OFF`
  (exit line must match exactly at a line boundary).
- While ON: the line parser is suspended both ways, the display sends no link
  commands, `serial_cmd_poll` stops normal parsing, USB bytes stream raw to
  `Serial1` and `Serial1` bytes stream raw to USB.
- State is a plain runtime flag; it never survives a reboot.
- Known edge: flash payload bytes that contain the exact 11-byte exit line at
  a line boundary would end the tunnel; accepted as negligible, and a failed
  flash just means re-issuing `PASSTH ON`.

### 6.4 USB serial (`serial_cmd.cpp`)

- `CREDS`, `AP`, `STATUS`, `MEM`, `HELP` and the fake-data set stay untouched
  (they speak to the display state, not the link).
- `LINK` retained, printing the new link state through
  `link_serial_debug_print`.
- New: `PASSTH ON|OFF` (§6.3). New: `W32CMD <line>` - forwards one arbitrary
  command to the link (bench tool for exercising the WT32 end), gated behind
  `IS_DEBUG=1` per `display-debug-flag.md`, so release builds compile it out.
  It is a bench-only convenience and can leak the password to scrollback just
  like `CREDS`, hence the gating.
- `/control`'s `PULSE INT` button (`link_notify_pulse()`, now orphaned) is
  repurposed to send one immediate `EXPERIMENT` push - same bench function,
  exercising the push path instead of the removed pulse path.

### 6.5 Simulator

`sim/link_provision_stub.cpp` keeps stubbing the whole link surface plus
`provision_ap`; only the function names change. `platformio.ini`
`[env:display-sim]` has a literal `-<display-v9/link_modbus.cpp>` source
filter that must follow the rename.

## 7. WT32 end (`firmware/network-master/main.cpp`)

Single-file firmware, so this lands as one rewrite of the link portion plus a
mechanical logging sweep. The Ethernet/portal/Supabase logic
(`doLogin()`, `fetchData()`, `sbAuthenticate()`, push queue, NTP) is recalled
as-is.

### 7.1 `Serial` becomes the link

- `Serial` (UART0, `IO1`/`IO3`, 115200) is now both the log sink and the link.
  `Serial1` is **not opened at all** in this phase, keeping `IO5`/`IO17` free
  for RS485 (§11).
- New `logf(fmt, ...)` helper: `vsnprintf` into a fixed static buffer (≤1024
  bytes), escapes embedded `\n`/`\r`, prepends `LOG `, writes one line to
  `Serial`. Every existing `Serial.println`/`print` call moves onto it; the
  many `Serial.print("...: "); Serial.println(...)` pairs must be merged into
  single lines for the framing to hold (they are whole-message rewrites, not
  found-and-replaces).
- Tag sweep: `[MB]` (master-poll logs) die with the Modbus master; link events
  log as `[LINK]`; `[ETH]`, `[LOGIN]`, `[HTTP]`, `[PING]`, `[DB]`, `[PUSH]`,
  `[NTP]` unchanged. The 5-blank-lines-at-boot trick stays (harmless to the
  display, still useful for a bare adapter).

### 7.2 Link plumbing

- `link_line_pull()`: non-blocking line parser (1024 B static RX buffer,
  50 ms gap discard), dispatcher for `HELLO`/`EXPERIMENT`/`CREDS`.
- `link_line_emit_*()` senders for `NET`/`DB`/`LOGINREQ`/`RESULT`/`HELLO`,
  with the snapshot reply on `HELLO`.
- Credential state identical to today: fixed char buffers, trimmed, empty
  ignored, stack copies wiped with `memset` before they leave scope
  (`clearCreds()` unchanged), a `lastAttempted` pair for the
  same-pair-answers verdict rule; nothing enters NVS/flash.
- `DUMP` accepted ungated (read-only; the gate lives on the display sender).
- The `IO14` ISR (`onPollRequest`, `s_pollRequest`) is deleted. The bench LED
  on `IO2` stays but is driven from the poll path: brief blink on any line
  received from the display. No interrupts on this link at all.
- Buffer/memory: one extra 1 KB static RX buffer plus ~130 B state; no heap
  Strings added. Within the current budget (~58 KB of 320 KB RAM used).

### 7.3 State machine (semantics kept, shape changed)

```
WAIT_IP  ->  NEED_CREDS  ->  LOGIN  ->  RUN
   ^             ^                      |
   `-------------`----------------------`
        link lost     fail / walled mid-run
```

- `setup()`: `Serial.begin(115200)` as today; emit `HELLO`; `TEST_LED`
  output config.
- `loop()` must become non-blocking (today `STAGE_RUN` sleeps `delay(kIntervalMs)`):
  every pass runs `link_line_pull()` first, then a `millis()`-gated stage
  tick. This is required so the display's `EXPERIMENT`/`CREDS` pushes are
  consumed at all times; even if the tick blocks, the UART FIFO absorbs a
  heartbeat, but freshness of /info and LOG draining would degrade.
- `WAIT_IP`: heartbeat `NET up=0` while `!eth_connected`.
- `NEED_CREDS`: heartbeat = `NET up=1` + `DB` + `LOGINREQ`. `CREDS` arrives in
  -> `LOGIN`. The 30 s already-authenticated probe (`kAuthProbeMs`) stays as
  is, releasing the gate when no login is actually needed.
- `LOGIN`: `doLogin()` -> `RESULT OK`/`RESULT FAIL` emit; store
  `lastAttempted`; wipe credentials. Fail -> `NEED_CREDS` (retry stays
  5 s-cadenced, now driven by the display's re-push).
- `RUN`: every 5 s - ping, data-fetch (walled -> `NEED_CREDS`), `DB` pass,
  emit `NET`+`DB`; consume `EXPERIMENT`/`CREDS`. Same time budget as today.

### 7.4 Porting pitfalls to check at review

- Every previous Modbus transaction had a 1 s timeout; the new model has
  none - verify no `doLogin()`-internal code path wrote a register directly.
- `pollDisplayCreds()`/`pollExpState()`/`consumeDisplayCreds()`/`pushNetStatus()`
  (register version) and `pushDbState()` are deleted, not ported; their
  semantics reappear in the emit functions and the `CREDS`/`EXPERIMENT`
  handlers. Anything that read those values should now read the stored ones.
- `syncDbLatest` still runs on its own 30 s timer and now also emits `DB` on
  change. `drops=0` is meaningful (no overflow) and is emitted as a
  number; `-` is reserved for genuinely unknown fields, which the drop
  counter never is.

## 8. Wiring change checklist (physical)

1. Remove WT32 `IO17` -> display `IO18` and display `IO17` -> WT32 `IO5`
   (the two AT-header data wires).
2. Remove display `IO11` -> WT32 `IO14` (the poll-request wire).
3. Connect WT32 Debug header `TXD` (`IO1`) -> display `IO18` and display
   `IO17` -> WT32 Debug header `RXD` (`IO3`); keep/confirm the direct GND
   wire.
4. Optionally free display `IO11` (leave unconnected; the RGB bus is elsewhere).

Sanity anchors that stay true: swap the two data wires first if no frames
arrive, then check the direct GND; never trust a shared USB ground.

## 9. Bring-up and verification plan

```sh
pio run -e display-v9
pio run -e display-sim
pio run -e network-master
```

All three must compile before anything is claimed done (project
`learned-mistakes.md` rule). Then, bench order:

1. **Loopback, display only:** short `IO18` to `IO17`. `LINK` prints state;
   `PASSTH ON` echoes every typed character; each emit logs one `[LINK>]`
   echo line, and its looped-back copy must be silently ignored (direction
   mismatch - a visible parse reply would mean the direction check is
   missing).
2. **One-board bring-up, display only + WT32 absent:** expect 5 s
   `EXPERIMENT` heartbeats on the console and no errors.
3. **Both boards, bare adapter on the display side:** see `[LINK]`, then
   `[W32]`-forwarded WT32 logs arriving; `NET`/`DB`/`LOGINREQ` heartbeats
   land; /info shows live IP after `HELLO` exchange.
4. **Portal round trip:** stage credentials from the phone form; verdict
   observable as `RESULT OK`/`FAIL` routing on /setup; wrong password shows
   the retry path; the corrected pair takes effect immediately (no 5 s wait).
5. **Reboot mixes (the new edge cases):** WT32 reboot mid-run releases a fresh
   display's stuck gate only via snapshot `RESULT OK`; display reboot mid-run
   re-raises /setup and replies to `HELLO` with experiment state.
6. **Passthrough flash:** §5 path 2, `--before no_reset` drill.
7. Full portal+ping+fetch cycle on the real walled network as recorded in
   `network-master.md` §7.

## 10. Failure modes summary

| Symptom | Likely cause |
|---|---|
| Nothing received on the display | data wires swapped (swap first), missing direct GND |
| Commands arrive, logs never | WT32 end still emits via `Serial.print` fragments (unmerged multi-part lines) |
| WT32 never reacts to `CREDS` | percent-decoding bug, or the display staging path bypassed `link_serial_set_credentials` (all paths must land there) |
| /info stale but hearts beat on the wire | heartbeat tick skipped by a blocking loop on either board |
| Flash over the tunnel fails | RTS/DTR auto-reset attempted; use the manual `IO0`+`EN` drill (§5) |

## 11. Follow-on: RS485 (later phase, separate firmware work)

Once this rework ships, the WT32 `IO5`/`IO17` AT-header pair are free. The
original follow-on plan here was a MAX485 Modbus RTU bus between the WT32 and
the Mega Mini. That was superseded on 2026-10-24: the pair now carries a direct
TTL poll of the legacy Mega slave (`migration-plan.md`, `link-serial-protocol.md`
§8), and no MAX485 hardware is in that path. The RS485 option below stays on
record for a future bus. Physical choices for it:

| Signal | WT32 pin | Note |
|---|---|---|
| `RO` (receiver out) | `IO5` | `Serial1` RX, the pair the link used to occupy |
| `DI` (driver in) | `IO17` | `Serial1` TX |
| `DE`+`RE` tied | **`IO33`** | silkscreened `485_EN` on the WT32 header; non-strapping. Fallback if ever needed: `IO14`, which this rework has also just freed |

Standing constraints, copied forward: 9600 8N1 (matches the two-request,
~0.35 s full-poll math in `esp-db-sync.md` §3); **manual single-pin direction
control only** - these modules have `DE`/`RE` tied, so the `CSE_ArduinoRS485`
library must never be used against them and `firmware/max485-test/`'s
`rs485Dir()` pattern is the reference; 120 Ω termination at bus ends; common
ground. The Modbus helpers (CRC16, frame silence detection) already present
in this repo move from the old display link to this bus; the two transports
(lines to the display, Modbus RS485 to the Mega) share no state.

The Mega Mini slave firmware itself is a separate task and is not planned
here.

## 12. Documentation updates to make when implementing

- New `link-serial-protocol.md` (or extend this file) as the wire spec, and
  retire `display-wt32-link.md` to a "replaced by" header.
- `display-docs.md` §5/§10 and file map; `network-master.md` §2/§3/§4/§6/§7
  (state machine, output samples, wiring).
- `platformio-environments.md` for the `display-sim` filter rename.
- `hardware.md` is human-written and not edited by agents: report the
  `IO1`/`IO3` move and the freed `IO11`/`IO14` there as a discrepancy/heads-up
  instead.

---
Content generated by `codebuff/deepseek-v4.1-flash` in `freebuff`.
