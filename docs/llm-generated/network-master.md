# network-master (`firmware/network-master`) — production WT32 firmware

Scope: the WT32-ETH01 firmware that ships. It brings up wired Ethernet,
takes the captive portal username/password from the display over the
`rusim-line` serial link, logs in to the portal, then holds the connection up
with ping plus an HTTPS data fetch. Re-authentication reuses the same display
path; there is no serial-console credential entry.

`link-serial-protocol.md` is the wire specification. `display-docs.md` §10
documents the display end of the same link. `wt32-eth01-ping-test.md`
documents the prototype this env was copied from.
`captive-portal-workaround.md` documents the portal relay plan.

## 1. Environment

| Property | Value |
|---|---|
| Environment | `[env:network-master]` (production) |
| Board | `wt32-eth01` |
| Platform / framework | `espressif32` / `arduino` |
| `monitor_speed` | 115200 |
| Libraries | `dvarrel/ESPping@^1.0.5` |
| Source | `firmware/network-master/main.cpp` |

Build and flash:

```bash
pio run -e network-master
pio run -e network-master -t upload
pio device monitor -e network-master
```

## 2. Wiring

| WT32 pin | Display pin | Direction |
|---|---|---|
| Debug-header TXD (IO1), UART0 TX | IO18 (RX) | WT32 -> display |
| Debug-header RXD (IO3), UART0 RX | IO17 (TX) | display -> WT32 |
| GND | GND | common, wired directly |

Settings: 115200 8N1. `Serial` is both the link and the log sink. The WT32
prints every diagnostic as a `LOG <text>` line and the display forwards it to
its own USB console; nothing else may write raw text to `Serial`.

`Serial1` is not opened in this phase. The old AT-header pair (`IO5`/`IO17`)
is free and reserved for the RS485 sensor bus (`link-serial-protocol.md` §8).
`IO2` remains an optional bench LED (external LED + 1k to GND) that blinks on
any line received from the display. The `IO11 -> IO14` poll-request wire and
its ISR are gone.

If no lines arrive, swap the two data wires first, then check the direct GND.
Do not rely on shared USB ground. `IO1`/`IO3` carry no boot strapping, so
power-up order does not matter; a UART transmitter idles HIGH.

## 3. Line protocol

Must match `firmware/display-v9/link_serial.h`. One ASCII line per message,
`\n` terminated, 1024 bytes maximum, 50 ms partial-line discard, no CRC and no
acks. Full rules: `link-serial-protocol.md` §2.

### 3.1 Received from the display

| Word | Payload | Meaning |
|---|---|---|
| `HELLO` | - | display booted; reply with the full snapshot |
| `EXPERIMENT` | `run=<0\|1> num=<n> dur=<1-99>` | experiment truth; log on change |
| `CREDS` | `user=<pct> pass=<pct>` | staged portal pair; decode, trim, log in |
| `DUMP` | - | emit a `LOG [DUMP] ...` state snapshot (read-only, ungated) |

### 3.2 Sent to the display

| Word | Payload | Cadence |
|---|---|---|
| `HELLO` | - | once at WT32 boot |
| `LOGINREQ` | - | every heartbeat while credentials are needed |
| `RESULT` | `OK` / `FAIL` | after every login attempt, plus the HELLO snapshot in RUN |
| `NET` | `ip=<str> up=<0\|1> [ping=<n\|->]` | every heartbeat and ETH event |
| `DB` | `latest=<n\|-> time=<0\|1> bottles=<n\|-> drops=<n>` | every heartbeat and after each DB pass |
| `LOG` | `<text>` | every diagnostic line |

`DB latest=0` means "no experiments yet" (the display maps it to -1), `-`
means unknown. `drops=` is always a number; `0` is meaningful.

### 3.3 Credential state

Fixed 64-byte buffers, no Arduino `String`. A new pair is decoded from
percent-encoding, trimmed, and copied to the working buffers and to the
last-attempted pair. An identical pair answers with the stored verdict instead
of logging in again. The working pair is overwritten with `x` then zeroed
after each attempt; the last-attempted pair stays in RAM for the repeat rule.
Nothing is written to NVS or flash.

The display keeps its own copy for the session, so a WT32 reboot (`HELLO`)
makes the display re-push the same pair without retyping.

## 4. State machine

```
WAIT_IP -> NEED_CREDS -> LOGIN -> RUN
   ^           ^                    |
   |___________|____________________|
   link lost    login fail / walled mid-run
```

- `WAIT_IP`: heartbeat `NET up=0`. Logs `[ETH] Waiting for IP address (DHCP)...`
  every 2 s. `ETH_GOT_IP` moves to `NEED_CREDS`.
- `NEED_CREDS`: every 5 s emits `NET up=1`, `DB` and `LOGINREQ`, and runs the
  30 s already-authenticated probe (portal trigger + data fetch). A `CREDS`
  line moves to `LOGIN` immediately. The display re-pushes its pair on every
  heartbeat until a verdict arrives.
- `LOGIN`: `doLogin()` (§5) with the RAM-only pair, then `RESULT OK`/`FAIL`.
  The working pair is wiped. Success moves to `RUN`; failure returns to
  `NEED_CREDS`, where a corrected pair from the display is acted on at once.
- `RUN`: every 5 s prints the IP, pings `1.1.1.1` 3x, fetches the data URL,
  runs the DB/push pass, and emits `NET` + `DB`. A WALLED fetch returns to
  `NEED_CREDS` for re-auth. A ping failure alone does not trigger re-auth.

`loop()` is non-blocking between stage ticks: it pulls and dispatches link
lines before anything else, then checks `millis()` gates. `doLogin()` and the
HTTPS calls still block for their duration; the UART FIFO absorbs one
heartbeat and the next pass drains it.

There is no `DONE` halt state: every failure path returns to the display.
Reset is never required to retry.

## 5. Portal login (`doLogin()`)

Ported unchanged from `wt32-eth01_ping-test` except that username/password
arrive over the line link instead of the serial console:

1. Trigger: GET `http://connectivitycheck.gstatic.com/generate_204` with
   redirects disabled, parse `window.location="..."`. Empty means no
   interception; a genuine data fetch then means already authenticated.
2. Login page: HTTPS GET of the portal URL with `setInsecure()`,
   rolling-window scan for form `action`, `4Tredir`, `magic`.
   Abort when the form or token is missing.
3. Submit: POST form-urlencoded
   `4Tredir=<enc>&magic=<enc>&username=<enc>&password=<enc>` with
   `Content-Type` and `Referer`. A 3xx or a body mentioning success counts
   as acceptance; the verifier below decides.
4. Verify: `fetchData()`. Genuine lyrics JSON means success.

Body reads are capped (`kMaxBody = 4096`) and the login page is scanned
through a 320-byte rolling window because the WT32-ETH01 has no PSRAM.

## 6. Memory and limits

- Flash used: ~985 KB of 1280 KB (75%). RAM used: ~58 KB of 320 KB (18%).
  The tight flash is why the SoftAP credential form lives on the display
  rather than here: a web server does not fit in the remaining ~300 KB. See
  `display-provisioning-ap.md`. The Supabase CA bundle (`supabase_ca.h`,
  3 roots, ~4 KB flash) is included in those figures. The dormant push
  queue + JSON body (~54 KB `.bss`) is currently folded away by the
  optimizer because nothing enqueues yet; when the sensor pass adds a
  caller it materializes and RAM rises to ~112 KB (34%), still inside the
  100-150 KB ceiling.
- The line link adds two fixed 1 KB log buffers, one 1 KB RX buffer, and
  ~130 B of state. No heap allocation is added.
- DHCP only, no static-IP fallback.
- `setInsecure()` skips TLS verification on the captive-portal path only
  (interception sits above TLS termination, so it cannot be verified).
  Supabase calls (`sbAuthenticate`, `sbFetchLatestExp`, `sbPushReadings`)
  pin the CA bundle via `setCACert()` instead: chain + hostname checking
  on, no insecure fallback. See `esp-db-sync.md` §5.
- Sample push queue: fixed 1200-row ring (~50 rounds), oldest-drop with a
  saturating drop counter, sample-time stamps, clock-gated on NTP, flushed
  2 rounds per 30 s cycle to `push_readings()`. Dormant until the sensor
  chain lands. See `esp-db-sync.md` §7-8.
- Credential lifetime: RAM only. The working pair is overwritten with `x`
  before release. The last-attempted pair is kept for the same-pair rule.
  Never written to flash or NVS.
- This firmware does not know where the credentials came from. The display's
  SoftAP form and its USB `CREDS` command both land in
  `link_serial_set_credentials()`, which sends `CREDS` immediately, and this
  side only ever sees a staged pair.
- Link limits: 1024-byte lines, 50 ms gap discard. A lost or corrupted line is
  replaced by the next heartbeat within 5 s.

## 7. Expected output

Lines prefixed `LOG ` reach the display, which prints them as `[W32] ...` on
its USB console. A bare adapter on the WT32 Debug header shows the prefix.

```
LOG network-master (production WT32 portal + rusim-line peer)
LOG Ping target: 1.1.1.1
LOG Data URL: https://unison.boidu.dev/lyrics?v=dQw4w9WgXcQ
LOG Credentials come from the display over UART0, never stored.
HELLO
LOG [LINK] rusim-line v1 on UART0 IO1/IO3 115200 8N1
LOG [ETH] Waiting for link + DHCP...
LOG [ETH] Started
LOG [ETH] Link up
LOG [ETH] Got IP 192.168.1.10 gw ... mask ... mac ...
LOG [LINK] link up, waiting for credentials from the display
LOG [LINK] captured creds user='...' (password in RAM only)
LOG [LOGIN] Verifying with a real data fetch...
LOG [LOGIN] SUCCESS: port is authenticated.
LOG [PING] 1.1.1.1 OK, avg time = 15 ms
LOG [RUN] exp=1 duration=24h running=1 dbLatest=3 bottles=24 ntp=1 queued=0 drops=0
```

On a rejected pair:

```
LOG [LOGIN] FAILED: still walled (wrong credentials or portal changed).
LOG [LOGIN] will wait for a corrected pair from the display
```

The display shows the same verdict on `/setup` and on the phone's form page.

## 8. Related documents

- `link-serial-protocol.md` — the wire spec, log forwarding, `PASSTH`, RS485
  follow-on.
- `display-docs.md` §10, §12 — display end, login gate, SoftAP form.
- `display-provisioning-ap.md` — the phone login path on the display.
- `wt32-eth01-ping-test.md` — prototype this env was copied from.
- `wt32-eth01-captive-recon.md` — portal recon sketch.
- `captive-portal-workaround.md` — portal relay plan.
- `platformio-environments.md` — build environments.
- `../hardware.md` — board pinouts (read-only reference).

---
Content generated by `opencode/deepseek-v4.1-flash-free` in `opencode`.
