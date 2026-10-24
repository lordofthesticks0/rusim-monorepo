# network-master (`firmware/network-master`) â€” production WT32 firmware

Scope: the WT32-ETH01 firmware that ships. It brings up wired Ethernet,
takes the captive portal username/password from the display over the
`rusim-line` serial link, logs in to the portal, then holds the connection up
with ping plus an HTTPS data fetch. Re-authentication reuses the same display
path; there is no serial-console credential entry.

`link-serial-protocol.md` is the wire specification. `display-docs.md` Â§10
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

### 2.1 Display link (UART0)

| WT32 pin | Display pin | Direction |
|---|---|---|
| Debug-header TXD (IO1), UART0 TX | IO18 (RX) | WT32 -> display |
| Debug-header RXD (IO3), UART0 RX | IO17 (TX) | display -> WT32 |
| GND | GND | common, wired directly |

Settings: 115200 8N1. `Serial` is both the link and the log sink. The WT32
prints every diagnostic as a `LOG <text>` line and the display forwards it to
its own USB console; nothing else may write raw text to `Serial`.

`IO2` remains an optional bench LED (external LED + 1k to GND) that blinks on
any line received from the display. The `IO11 -> IO14` poll-request wire and
its ISR are gone.

If no lines arrive, swap the two data wires first, then check the direct GND.
Do not rely on shared USB ground. `IO1`/`IO3` carry no boot strapping, so
power-up order does not matter; a UART transmitter idles HIGH.

### 2.2 Mega slave link (UART1)

Direct TTL, no MAX485: the legacy Mega answers on its own `Serial1`, and the
link needs no direction control.

| Signal | WT32 pin | Mega pin | Direction |
|---|---|---|---|
| Data | IO5 (UART1 RX) | D18 (TX1) | Mega -> WT32 |
| Data | IO17 (UART1 TX) | D19 (RX1) | WT32 -> Mega |
| Ground | GND | GND | common |

Settings: 9600 8N1, matching `Serial1.begin(9600)` in
`legacy/slave/new-slave.cpp`. `Serial1.begin(9600, SERIAL_8N1, 5, 17)` remaps
the pins, because the ESP32 UART1 defaults (`IO9`/`IO10`) are flash pins on
this module. `IO14` stays spare.

Electrical warning: the Mega drives D18 at 5 V and the WT32 pins are 3.3 V
only, so D18 -> IO5 needs a divider (2k2/3k3) or a level shifter. IO17 -> D19
at 3.3 V sits close to the ATmega's worst-case `VIH` (about 3.0 V at a 5 V
supply); it works in practice but is not guaranteed. The common ground is
mandatory; two USB grounds are not a substitute.

### 2.3 Mega slave poll behaviour

- `sens_poll()` is called from `loop()` before the stage switch, so it runs in
every state, including while the portal gate is open. One round per 30 s
(`kSensPollMs`).
- Each round drains stale RX bytes, writes `#1:PING*` (8 bytes), and reads
until `\n`, a full buffer, or 1500 ms (`kSensReplyMs`: 50 ms capture window
plus about 310 ms of line time for a ~500-byte reply). A frame that does not
begin with `{` is rejected before parsing.
- The frame is one fixed `char sensRx[513]`. No `String`, no JSON library: the
shape is fixed and flash is tight. The parser locates `"id":`, then `"ph":`,
`"methane":`, `"temp":`, `"co2":` and `"press":` with `strstr` + `strtof`,
bounded to one bottle object and to the buffer's NUL. Ids outside 1-4 are
ignored.
- The slave's invalid markers become `nullMask` bits instead of numbers:
`ph`/`methane`/`co2`/`press` `-1` and `temp` `-999`, mapped to bits 0/4/3/1/2
the same way `PushRow` does. `ph` is pH units: the slave applies its per-bottle
3-point calibration, so no table exists in this firmware.
- One `LOG [SENS]` line per round, for example
`LOG [SENS] 4 bottles ok=20 fail=0 b1 ph=2.51 ch4=812.4 t=38.5 co2=1204 p=1013 ...`.
`ok=` counts accepted values, `fail=` counts bottle slots with no object in
the frame. Failed rounds (timeout, overlong frame, non-JSON) are logged on the
first failure and then every 20th, so a disconnected Mega cannot flood the
display's log.
- Nothing is enqueued. The push queue above stays dormant until the deferred
step in `migration-plan.md` Â§6.

## 3. Line protocol

Must match `firmware/display-v9/link_serial.h`. One ASCII line per message,
`\n` terminated, 1024 bytes maximum, 50 ms partial-line discard, no CRC and no
acks. Full rules: `link-serial-protocol.md` Â§2.

### 3.1 Received from the display

| Word | Payload | Meaning |
|---|---|---|
| `HELLO` | - | display booted; reply with the full snapshot |
| `EXPERIMENT` | `run=<0\|1> num=<n> dur=<1-99>` | experiment truth; log on change |
| `CREDS` | `user=<pct> pass=<pct>` | staged portal pair; decode, trim, log in |
| `DUMP` | - | emit a `LOG [DUMP] ...` state snapshot, including the sensor counters `sensOk`, `sensFail` and `sensAge` (read-only, ungated) |

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
- `LOGIN`: `doLogin()` (Â§5) with the RAM-only pair, then `RESULT OK`/`FAIL`.
  The working pair is wiped. Success moves to `RUN`; failure returns to
  `NEED_CREDS`, where a corrected pair from the display is acted on at once.
- `RUN`: every 5 s prints the IP, pings `1.1.1.1` 3x, fetches the data URL,
  runs the DB/push pass, and emits `NET` + `DB`. A WALLED fetch returns to
  `NEED_CREDS` for re-auth. A ping failure alone does not trigger re-auth.

`loop()` is non-blocking between stage ticks: it pulls and dispatches link
lines before anything else, then checks `millis()` gates. `doLogin()` and the
HTTPS calls still block for their duration; the UART FIFO absorbs one
heartbeat and the next pass drains it. The sensor poll blocks for at most
1.5 s once per 30 s, and it pulls the display link from inside that wait so a
`CREDS` line arriving during a reply is not left in the FIFO.

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

- Flash used: 990,117 B of 1,310,720 B (75.5%). RAM used: 62,312 B of
  327,680 B (19.0%). The tight flash is why the SoftAP credential form lives
  on the display rather than here: a web server does not fit in the
  remaining ~300 KB. See `display-provisioning-ap.md`. The Supabase CA bundle
  (`supabase_ca.h`, 3 roots, ~4 KB flash) is included in those figures.
  The sensor poll adds one fixed 513-byte RX frame and ~20 B of counters.
- Push queue: the 1200-row ring (~48 KB `.bss`) is still folded away because
  nothing enqueues yet, but the 6,144-byte JSON body is present in the image
  (`nm` shows `s_pushBody` at 0x1800 bytes, `s_pushQueue` absent). When the
  sensor step adds a caller the ring materializes and RAM rises by roughly
  48 KB, still inside the 100-150 KB ceiling.
- The line link adds two fixed 1 KB log buffers, one 1 KB RX buffer, and
  ~130 B of state. No heap allocation is added.
- DHCP only, no static-IP fallback.
- `setInsecure()` skips TLS verification on the captive-portal path only
  (interception sits above TLS termination, so it cannot be verified).
  Supabase calls (`sbAuthenticate`, `sbFetchLatestExp`, `sbPushReadings`)
  pin the CA bundle via `setCACert()` instead: chain + hostname checking
  on, no insecure fallback. See `esp-db-sync.md` Â§5.
- Sample push queue: fixed 1200-row ring (~50 rounds), oldest-drop with a
  saturating drop counter, sample-time stamps, clock-gated on NTP, flushed
  2 rounds per 30 s cycle to `push_readings()`. Still dormant: the sensor
  poll logs but does not enqueue (`migration-plan.md` Â§6). See
  `esp-db-sync.md` Â§7-8.
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
LOG [SENS] UART1 on IO5 RX / IO17 TX, 9600 8N1
LOG [ETH] Waiting for link + DHCP...
LOG [ETH] Started
LOG [ETH] Link up
LOG [ETH] Got IP 192.168.1.10 gw ... mask ... mac ...
LOG [LINK] link up, waiting for credentials from the display
LOG [LINK] captured creds user='...' (password in RAM only)
LOG [LOGIN] Verifying with a real data fetch...
LOG [LOGIN] SUCCESS: port is authenticated.
LOG [PING] 1.1.1.1 OK, avg time = 15 ms
LOG [SENS] 4 bottles ok=20 fail=0 b1 ph=2.51 ch4=812.4 t=38.5 co2=1204 p=1013 b2 ph=2.49 ch4=790.1 t=38.2 co2=1198 p=1013 b3 ph=2.53 ch4=824.9 t=38.7 co2=1210 p=1014 b4 ph=2.47 ch4=801.3 t=38.4 co2=1202 p=1013
LOG [RUN] exp=1 duration=24h running=1 dbLatest=3 bottles=24 ntp=1 queued=0 drops=0
```

With the Mega absent or its TX wire open, a failure line is logged on the
first failure and then every 20th:

```
LOG [SENS] poll failed: reply timeout (fail=1)
```

A field the slave reported invalid prints as `null`:

```
LOG [SENS] 2 bottles ok=5 fail=2 b1 ph=null ch4=null t=null co2=null p=null b2 ph=2.5 ch4=700 t=37 co2=1000 p=1000 ...
```

On a rejected pair:

```
LOG [LOGIN] FAILED: still walled (wrong credentials or portal changed).
LOG [LOGIN] will wait for a corrected pair from the display
```

The display shows the same verdict on `/setup` and on the phone's form page.

## 8. Related documents

- `link-serial-protocol.md` â€” the wire spec, log forwarding, `PASSTH`, and
  the Mega link that replaced the RS485 pin reservation.
- `display-docs.md` Â§10, Â§12 â€” display end, login gate, SoftAP form.
- `display-provisioning-ap.md` â€” the phone login path on the display.
- `wt32-eth01-ping-test.md` â€” prototype this env was copied from.
- `wt32-eth01-captive-recon.md` â€” portal recon sketch.
- `captive-portal-workaround.md` â€” portal relay plan.
- `migration-plan.md` â€” the sensor pull this implements, plus the deferred
  enqueue and in-slave pH conversion.
- `legacy-slave.md` â€” the Mega program and its JSON reply.
- `platformio-environments.md` â€” build environments.
- `../hardware.md` â€” board pinouts (read-only reference).

---
Content generated by `codebuff/deepseek-v4.1-flash` in `freebuff`.
