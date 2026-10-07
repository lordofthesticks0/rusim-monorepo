# WT32-ETH01 Ethernet ping test (`firmware/wt32-eth01_ping-test`)

Scope: the standalone firmware that proves the WT32-ETH01's wired Ethernet
works, including through the campus captive portal. It brings up the LAN8720
PHY over RMII, logs in to the portal over HTTPS with credentials typed on the
serial console, then pings a public address and fetches real data over HTTPS
as a connectivity check.

`wt32-eth01.md` documents the board's hardware. `captive-portal-workaround.md`
documents the headless captive portal authentication plan; this sketch
implements its Phase 5 relay. `wt32-eth01-captive-recon.md` documents the
sibling probe-only sketch.

## 1. Environment

| Property | Value |
|---|---|
| Environment | `[env:wt32-eth01_ping-test]` |
| Board | `wt32-eth01` |
| Platform / framework | `espressif32` / `arduino` |
| `monitor_speed` | 115200 |
| Libraries | `dvarrel/ESPping@^1.0.5` |
| Source | `firmware/wt32-eth01_ping-test/main.cpp` |

The board variant supplies the RMII pin definitions, so this environment does not
set any of them in `platformio.ini`.

## 2. What it does

1. Opens serial at 115200, waits one second for a USB monitor to attach,
   prints five blank lines to push the ROM bootloader's boot message (sent at
   74880 baud, so it renders as garbage at 115200) off screen, and flushes.
2. Prints the banner, then blocks in `waitForEnter()` until a newline arrives.
   The wait is indefinite and anything before the newline is ignored, so
   stray boot bytes can never be mistaken for input. The RX buffer is drained
   both before the prompt and after it. These sketches are interactive-only:
   without a terminal nothing proceeds, which is consistent with the
   credential prompts that follow.
2. Registers `onEthEvent()` on `WiFi.onEvent()`.
3. Calls `ETH.begin()` with the variant's pin constants.
4. Sets `eth_connected = true` when an IP arrives.
5. Prompts on serial for the portal username (echoed) and password (not
   echoed), with the same `readLine()` behaviour as the recon sketch
   (backspace handling, 30 s timeout per prompt, credentials cleared from RAM
   after login). See `wt32-eth01-captive-recon.md` §5.
6. Runs `doLogin()` once (§5a), then enters the steady-state loop: each pass
   prints the assigned IP, pings `1.1.1.1` three times, then fetches
   `https://unison.boidu.dev/lyrics?v=dQw4w9WgXcQ` over HTTPS and prints the
   body. This is the same check `curl` would do from a full OS:

   ```bash
   curl 'https://unison.boidu.dev/lyrics?v=dQw4w9WgXcQ'
   ```

   It repeats every five seconds. A failed login halts the sketch instead of
   looping; reset the board to retry.

## 3. RMII pin configuration

```cpp
ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO,
          ETH_PHY_TYPE, ETH_CLK_MODE);
```

The arguments come from the board variant's `pins_arduino.h`. The source restates
them in a comment, because they are the settings most often wrong when moving to
a different Ethernet board:

| Signal | GPIO |
|---|---|
| PHY address | 1 |
| PHY power / reset | 16 |
| MDC | 23 |
| MDIO | 18 |
| PHY type | LAN8720 |
| Clock | 50 MHz input on GPIO0 |

If `ETH.begin()` returns false, the sketch prints `[ETH] ERROR: ETH.begin()
failed` and keeps looping. It does not retry the initialisation. A board with no
link partner still brings the PHY up successfully, so a failure here is a wiring
or configuration problem, not a cabling one.

## 4. Event handling

`onEthEvent()` is registered with `WiFi.onEvent()` even though these are Ethernet
events; this is how the Arduino ESP32 core delivers them.

| Event | Output | `eth_connected` |
|---|---|---|
| `ARDUINO_EVENT_ETH_START` | `[ETH] Started`, then `ETH.setHostname("wt32-eth01")` | unchanged |
| `ARDUINO_EVENT_ETH_CONNECTED` | `[ETH] Link up` | unchanged |
| `ARDUINO_EVENT_ETH_GOT_IP` | `[ETH] Got IP`, IP, gateway, netmask, MAC | `true` |
| `ARDUINO_EVENT_ETH_DISCONNECTED` | `[ETH] Link down` | `false` |
| `ARDUINO_EVENT_ETH_STOP` | `[ETH] Stopped` | `false` |

Setting the hostname after `ETH_START` means it is in place before DHCP runs, so
the lease request carries it.

`eth_connected` is declared `volatile` because it is written from the event
context and read from `loop()`.

## 5. Ping test

Once an IP is held, each pass of `loop()` prints the address and calls:

```cpp
bool ok = Ping.ping(kPingTarget, kPingCount);   // 1.1.1.1, 3 packets
```

`Ping.averageTime()` is reported on success. On failure the sketch prints
`[PING] FAIL (no reply)` and waits `kPingIntervalMs` (5000 ms) before trying
again.

`ESPping` does ICMP echo over the raw socket API. It depends on the network
allowing ICMP and on a default route existing. A success here proves IP
connectivity to the internet, which is more than the rest of the project needs.

## 5a. Portal login (`doLogin()`)

Implements the Phase 5 relay from `captive-portal-workaround.md` against the
measured Unpad portal behaviour (§2.4 of that document):

1. **Trigger** (`fetchPortalUrl()`): GET `http://example.com/` with redirects
   disabled, parse `window.location="..."` out of the interception page. An
   empty result means no interception, so the sketch checks the data fetch:
   genuine content means the port was already authenticated and the POST is
   skipped; otherwise it halts.
2. **Login page** (`fetchLoginPage()`): HTTPS GET of the `fgtauth` URL with
   `setInsecure()`, scanned through the same rolling-window `scanPage()` as
   the recon sketch for the form `action`, `4Tredir`, and `magic`. Login
   aborts when the form or the token is missing.
3. **Submit** (`postCredentials()`): POST to the resolved action URL
   (`resolveAction()` handles absolute, root-relative, and bare relative
   actions; the measured action is `/`, i.e. `https://pintas.unpad.ac.id:1003/`)
   with `Content-Type: application/x-www-form-urlencoded` and a `Referer` of
   the login page. The body is
   `4Tredir=<enc>&magic=<enc>&username=<enc>&password=<enc>`, in the form's
   document order, each value passed through `urlEncode()` (which leaves the
   RFC 3986 unreserved set alone and percent-encodes the rest, so passwords
   with `&`, `=`, or non-ASCII bytes survive). The response is judged loosely
   here — a 3xx `Location` or a body mentioning success or the `4Tredir`
   target counts as acceptance — because the verifier below has the final
   word. The first 800 response bytes are printed for diagnosis either way.
4. **Verify**: `fetchData()` again. Genuine lyrics back means `SUCCESS` and
   the sketch enters `STAGE_RUN`; the interception page means `FAILED` and it
   halts with the credentials already cleared.

## 5b. HTTPS data fetch

`fetchData()` verifies that real HTTPS application data flows, not just ICMP:

- `WiFiClientSecure` is created with `setInsecure()`, so the server
  certificate is not verified. This trades authenticity for simplicity and
  is appropriate for a connectivity test, not for production traffic.
- The response body is streamed from `http.getStreamPtr()` in 256-byte
  chunks, so the body is never buffered in full. The WT32-ETH01 has no
  PSRAM, and even lyrics-sized payloads can fragment heap if copied into a
  single `String`.
- On a negative status (TLS or TCP failure), `http.errorToString(code)` is
  printed. A failure here with a successful ping points at TLS/proxy
  interference — e.g. the captive portal blocking a non-portal TLS
  handshake.
- `fetchData()` returns whether the body is genuine. It checks for the
  `window.location` interception marker rather than trusting the status code,
  because the campus network answers intercepted requests with `200`. A
  walled response prints `WALLED` instead of the body. Both `doLogin()` and
  the steady-state loop use this verdict.

## 6. Expected output

```
WT32-ETH01 Ethernet test with captive portal login
Ping target: 1.1.1.1
Data URL: https://unison.boidu.dev/lyrics?v=dQw4w9WgXcQ
Credentials are read from this console and never stored.
Press Enter to start.
[ETH] Started
[ETH] Link up
[ETH] Got IP
[ETH] IP address: 10.92.128.22
[ETH] Gateway: 10.92.128.1
[ETH] Netmask: 255.255.255.0
[ETH] MAC: 68:09:47:xx:xx:xx
Portal username: [username]
Portal password:
Credentials held in RAM for username='[username]', password=<hidden>
[LOGIN] GET trigger http://example.com/
[LOGIN] trigger status: 200
[LOGIN] portal URL: https://pintas.unpad.ac.id:1003/fgtauth?006418c1affb337e
[LOGIN] GET login page https://pintas.unpad.ac.id:1003/fgtauth?006418c1affb337e
[LOGIN] login page status: 200
[LOGIN] form action: /
[LOGIN] 4Tredir: http://example.com/
[LOGIN] magic: 006418c1affb337e
[LOGIN] POST https://pintas.unpad.ac.id:1003/
[LOGIN] body length: 105
[LOGIN] POST status: 200
[LOGIN] Location:
[LOGIN] response bytes: 2723
[LOGIN] response head:
<!DOCTYPE html><html lang="en"> ... (a styled status page, not a redirect)
[LOGIN] Verifying with a real data fetch...
[HTTP] GET https://unison.boidu.dev/lyrics?v=dQw4w9WgXcQ
[HTTP] status: 200
[HTTP] body is genuine content (AUTHENTICATED):
493
{"success":true,"data":{"videoId":"dQw4w9WgXcQ", ... lyrics JSON ...}}
0
[LOGIN] SUCCESS: port is authenticated.
[ETH] Assigned IP: 10.92.128.22
[PING] Pinging 1.1.1.1 ... OK, avg time = 15.55 ms
...
```

Reference values from the first successful run (2026-10-02, `login.log`):

| Item | Known-good value |
|---|---|
| `POST status` | `200` with an empty `Location`. The portal answers a successful login with an HTML status page, not a redirect. Do not treat "no 3xx" as failure; the data-fetch verifier is the authority. |
| `response bytes` | `2723` (a styled page; the printed head is CSS, cut at 800 chars) |
| `body length` (login request) | `105` for this username; varies with credential length |
| Verify body | The lyrics server uses chunked transfer encoding, so the printed body carries the chunk framing (`493` before the JSON, `0` after). That is transport framing, not corruption. |
| Ping after auth | `OK, avg time = 15.55 ms`. Pre-auth the same ping fails, so ping alone distinguishes the two states once the portal is known. |

Before DHCP completes, each pass prints
`[ETH] Waiting for IP address (DHCP)...` every five seconds.

## 7. Diagnosing the output

| Symptom | Cause |
|---|---|
| `[ETH] ERROR: ETH.begin() failed` | RMII pin constants, PHY type, or 3V3 versus 5V supply. `wt32-eth01.md` §8 lists the power gotcha. |
| `Started` then nothing, no `Link up` | No cable, wrong PHY address, or the transceiver's LED is off at the switch. |
| `Link up` then `Link down` repeatedly | Flapping link. Cable, switch port, or a board being powered from two rails. |
| Repeated `Waiting for IP address` | Link is up but DHCP is not answering. Check the gateway, the VLAN, and that DHCP is not being filtered. |
| IP acquired, `FAIL (no reply)` | Routing or ICMP filtering. The board is on the network; the network is not reaching 1.1.1.1. |
| `avg time` very high or varying wildly | Link quality. The ESP32 Ethernet PHY has no flow control to fall back on. |
| `[HTTP] status: -1` or other negative code | TLS/TCP failure on the HTTPS fetch; likely portal or proxy interference |
| `[LOGIN] FAILED: still walled` | The POST did not authenticate the port. Usual causes: wrong credentials, or the portal renamed its fields (compare the printed `magic`/`4Tredir` against a fresh browser capture). Reset to retry. |

## 8. Limits

- **DHCP only.** There is no static-IP fallback, so the sketch depends on a DHCP
  server being present.
- **No reconnect logic.** A link drop clears `eth_connected` but never re-runs
  `ETH.begin()`; recovery depends on the core restarting the interface on a new
  `GOT_IP`.
- **One ping target, one URL, both fixed.** The HTTPS check does not verify
  the server certificate (`setInsecure()`), so it proves reachability, not
  authenticity. The same applies to the login POST.
- **No re-login.** If the portal session expires mid-run, the loop keeps
  reporting `WALLED` without re-prompting; reset the board to log in again.
- **No RS-485 or sensor code.** The bus side of this board is covered by
  `rs485-loopback-test.md`.

## 9. Running it

```bash
pio run -e wt32-eth01_ping-test -t upload
pio device monitor -e wt32-eth01_ping-test
```

If upload fails, pass `--upload-port` explicitly; the WT32-ETH01 often enumerates
as a second port once the native USB stack is active.

## 10. Related documents

- `wt32-eth01.md` — board hardware, pinout, power, and gotchas.
- `captive-portal-workaround.md` — the planned headless authentication workflow
  on this board.
- `rs485-loopback-test.md` — the RS-485 side of the same board.
- `platformio-environments.md` — the build environment.
- `subsystem-index.md` — the rest of the repo.

---
Content generated by `stealth/fledge-alpha-free` in `opencode`.
