# network-master (`firmware/network-master`) — production WT32 firmware

Scope: the WT32-ETH01 firmware that ships. It brings up wired Ethernet,
takes the captive portal username/password from the display over Modbus RTU,
logs in to the portal, then holds the connection up with ping plus an HTTPS
data fetch. Re-authentication reuses the same display path; there is no
serial-console credential entry.

`display-docs.md` §10 documents the display side of the same link
(Modbus slave, combined login gate). `wt32-eth01-ping-test.md` documents the
prototype this env was copied from. `captive-portal-workaround.md` documents
the portal relay plan.

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
| AT-header TXD (IO17), Serial1 TX | IO18 (RX) | WT32 -> display |
| AT-header RXD (IO5), Serial1 RX | IO17 (TX) | display -> WT32 |
| GND | GND | common, wired directly |

Settings: 9600 8N1, Modbus RTU slave ID 1. `Serial` (TX0/RX0 debug header)
is USB debug only and never carries Modbus frames.

If no frames arrive, swap the two data wires first, then check the direct
GND wire. Do not rely on shared USB ground. IO5 is a boot-strapping pin:
the board must boot with RX idle high; a display that drives TX low during
WT32 reset can block boot, so power the WT32 first when testing.

## 3. Register map

Must match `firmware/display-v9/link_modbus.h`.

| Address | Name | Access | Meaning |
|---|---|---|---|
| `0x0000` | LOGIN_REQ | master WR | `1` = show the login gate |
| `0x0001` | CREDS_READY | slave sets, master writes `0` to ack | `1` = staged creds ready to read |
| `0x0002` | RESULT | master WR | `0` none, `1` success, `2` fail |
| `0x0010-0x002F` | USERNAME | master RD (32 regs) | 64 bytes ASCII, big-endian, NUL-padded |
| `0x0030-0x004F` | PASSWORD | master RD (32 regs) | same encoding as username |
| `0x0060` | DURATION_H | master RD (1 reg) | 1-99, clamped |
| `0x0070-0x0077` | IP_ADDR | master WR (8 regs) | 16 bytes ASCII NUL-padded, pushed every cycle for /info |
| `0x0078` | PING_MS | master WR | avg ping to 1.1.1.1 in ms, `0xFFFF` = no data / failed |
| `0x0079` | NET_UP | master WR | `1` = holds DHCP IP, `0` = link down |

Full wire spec (framing, flows, timing, how to extend the map):
`display-wt32-link.md`.

Only FC `0x03` (read holding) and FC `0x06` (write single) are used by the
master. Each string register holds two ASCII bytes, high byte first. The CRC
is standard Modbus CRC16 (`01 03 00 00 00 01` -> `84 0A` low byte first).

## 4. State machine

```
WAIT_IP -> NEED_CREDS -> LOGIN -> RUN
   ^           ^                    |
   |___________|____________________|
   link lost    login fail / walled mid-run
```

- `WAIT_IP`: waits for `ARDUINO_EVENT_ETH_GOT_IP`. Prints
  `[ETH] Waiting for IP address (DHCP)...` every 2 s.
- `NEED_CREDS`: the network loop pauses. Every 5 s it writes
  `LOGIN_REQ=1`, reads `CREDS_READY`, and when `1` reads USERNAME (32),
  PASSWORD (32), DURATION (1). Empty username or password is ignored until
  the user taps Confirm on the display. A captured pair moves to LOGIN.
- `LOGIN`: runs `doLogin()` (§5) with the RAM-only credentials, writes
  RESULT (`1`/`2`), writes `CREDS_READY=0`, and on success also
  `LOGIN_REQ=0`. Credentials are overwritten and released before leaving.
  Success moves to RUN; failure returns to NEED_CREDS for re-poll in 5 s.
- `RUN`: prints the IP, pings `1.1.1.1` 3x, fetches the data URL. The fetch
  verdict decides: genuine content stays in RUN, a WALLED body (portal
  interception page) returns to NEED_CREDS for re-auth. A ping failure alone
  does not trigger re-auth.

There is no `DONE` halt state: every failure path re-polls the display.
Reset is never required to retry.

## 5. Portal login (`doLogin()`)

Ported unchanged from `wt32-eth01_ping-test` except that username/password
arrive from Modbus instead of the serial console:

1. Trigger: GET `http://example.com/` with redirects disabled, parse
   `window.location="..."`. Empty means no interception; a genuine data
   fetch then means already authenticated.
2. Login page: HTTPS GET of the `fgtauth` URL with `setInsecure()`,
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

- Flash used: ~964 KB of 1280 KB (73%). RAM used: ~47 KB of 320 KB (14%).
  The tight flash is why the SoftAP credential form lives on the display
  rather than here: a web server does not fit in the remaining ~316 KB. See
  `display-provisioning-ap.md`.
- DHCP only, no static-IP fallback.
- `setInsecure()` skips TLS verification (same tradeoff as the ping test).
- Credential lifetime: RAM only between capture and `clearCreds()`. Overwrite
  with `x` before release. Never written to flash or NVS.
- This firmware does not know where the credentials came from. Both the
  `/setup` touchscreen and the display's SoftAP form stage them into the same
  registers (§3) via `link_modbus_set_credentials()`, and this side only ever
  sees a staged pair.
- Link timeouts: 1 s per Modbus transaction, 8 ms inter-frame silence at
  9600 baud. A failed transaction waits for the next 5 s poll.

## 7. Expected output

```
network-master (production WT32 portal + Modbus master)
[MB] master on Serial1 RX=5 TX=17 9600 8N1 slave=1
[ETH] Waiting for link + DHCP...
[ETH] Started
[ETH] Link up
[ETH] Got IP
[MB] link up, polling display for credentials
[MB] display has no staged credentials yet
[MB] captured creds user='...' dur=24h
[LOGIN] Verifying with a real data fetch...
[LOGIN] SUCCESS: port is authenticated.
[PING] Pinging 1.1.1.1 ... OK, avg time = 15.55 ms
```

## 8. Related documents

- `display-docs.md` §10, §12 — display slave, login gate, and the SoftAP
  credential form.
- `display-provisioning-ap.md` — the phone login path on the display.
- `wt32-eth01-ping-test.md` — prototype this env was copied from.
- `wt32-eth01-captive-recon.md` — portal recon sketch.
- `captive-portal-workaround.md` — portal relay plan.
- `platformio-environments.md` — build environments.
- `../hardware.md` — board pinouts (read-only reference).

---
Content generated by `opencode/muse-spark-1.3-contributor-free` in `opencode`.
