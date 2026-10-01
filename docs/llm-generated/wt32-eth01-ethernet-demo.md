# WT32-ETH01 Ethernet demo (`firmware/wt32-eth01`)

Scope: the standalone firmware that proves the WT32-ETH01's wired Ethernet
works. It brings up the LAN8720 PHY over RMII, prints the DHCP address, and
pings a public address.

`wt32-eth01.md` documents the board's hardware. `captive-portal-workaround.md`
documents the planned headless captive portal authentication that will run on
this board. This document covers the demo sketch that exists today.

## 1. Environment

| Property | Value |
|---|---|
| Environment | `[env:wt32-eth01]` |
| Board | `wt32-eth01` |
| Platform / framework | `espressif32` / `arduino` |
| `monitor_speed` | 115200 |
| Libraries | `dvarrel/ESPping@^1.0.5` |
| Source | `firmware/wt32-eth01/main.cpp`, 92 lines |

The board variant supplies the RMII pin definitions, so this environment does not
set any of them in `platformio.ini`.

## 2. What it does

1. Opens serial at 115200 and waits 500 ms so a USB monitor has time to attach.
   The wait is fixed rather than conditional on `Serial`, because the sketch is
   expected to run headless on a board that is not connected to a terminal.
2. Registers `onEthEvent()` on `WiFi.onEvent()`.
3. Calls `ETH.begin()` with the variant's pin constants.
4. Sets `eth_connected = true` when an IP arrives.
5. From then on, prints the assigned IP and pings `1.1.1.1` three times, every
   five seconds.

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
connectivity to the internet, which is more than the rest of the project needs;
the captive portal plan needs DNS and an HTTP exchange, not ICMP.

## 6. Expected output

```
WT32-ETH01 Ethernet ping test
Target: 1.1.1.1
[ETH] Started
[ETH] Link up
[ETH] Got IP
[ETH] IP address: 192.168.1.42
[ETH] Gateway: 192.168.1.1
[ETH] Netmask: 255.255.255.0
[ETH] MAC: 84:F3:EB:xx:xx:xx
[ETH] Assigned IP: 192.168.1.42
[PING] Pinging 1.1.1.1 ... OK, avg time = 12 ms
```

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

## 8. Limits

- **DHCP only.** There is no static-IP fallback, so the sketch depends on a DHCP
  server being present.
- **No reconnect logic.** A link drop clears `eth_connected` but never re-runs
  `ETH.begin()`; recovery depends on the core restarting the interface on a new
  `GOT_IP`.
- **One ping target, fixed.** No DNS resolution and no HTTP request, so the
  sketch cannot be used to test name resolution or TLS.
- **No RS-485 or sensor code.** The bus side of this board is covered by
  `rs485-loopback-test.md`.

## 9. Running it

```bash
pio run -e wt32-eth01 -t upload
pio device monitor -e wt32-eth01
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
Content generated by `opencode/space-bunny-free` in `opencode`.