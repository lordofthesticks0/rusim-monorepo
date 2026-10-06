# Display <-> WT32 link (`display-wt32-link`)

Scope: the single Modbus RTU line between the display MCU and the WT32.
This is the only wire between the two halves of the device: credential
collection, portal verdicts, and network status all travel on it.
The display-side slave lives in `firmware/display-v9/link_modbus.*`;
the WT32-side master lives in `firmware/network-master/main.cpp`.
`display-docs.md` §10 and `network-master.md` describe each end;
this document describes the wire itself.

## 1. Physical layer

One direct TTL UART pair, no MAX485, no direction pin.

| WT32 pin | Display pin | Direction |
|---|---|---|
| AT-header TXD (IO17), Serial1 TX | IO18 (RX) | WT32 -> display |
| AT-header RXD (IO5), Serial1 RX | IO17 (TX) | display -> WT32 |
| GND | GND | common, wired directly |

Settings: 9600 8N1 on `Serial1` both ends. USB `Serial` (TX0/RX0 debug
header) is debug logging only and never carries Modbus frames.

Bring-up order: if no frames arrive, swap the two data wires first, then
check the direct GND wire; do not rely on shared USB ground. IO5 is a
boot-strapping pin on the WT32: the board must boot with RX idle high, so
power the WT32 first when testing. Both ends drain the line for ~20 ms at
init so boot noise can never parse as a frame.

## 2. Data link

Modbus RTU, slave ID 1. The WT32 is the only master; the display never
initiates a transaction. Functions:

| Code | Use |
|---|---|
| `0x03` | Read holding registers (master reads creds / display state) |
| `0x06` | Write single register (flags, ping, link state) |
| `0x10` | Write multiple registers (IP address block, flag batches) |

Anything else gets exception `0x01`; unknown addresses get `0x02`.
CRC is standard Modbus CRC16, low byte first
(`01 03 00 00 00 01` -> `84 0A`).

Framing: at 9600 baud one byte is ~1 ms, so both ends treat 8 ms of line
silence as end-of-frame (above the 3.5-char Modbus gap of ~4 ms). The
display parses fixed-length `0x03`/`0x06` requests eagerly at 8 bytes and
variable-length `0x10` after silence; a partial frame older than 150 ms is
dropped so it never wedges the parser. The master waits up to 1 s per
response. A failed transaction is logged and retried on the next poll;
there is no reset-to-retry path anywhere on this link.

## 3. Register map

Single source of truth. All strings are ASCII, big-endian (high byte
first), NUL-padded. Defined twice, kept identical:
`firmware/display-v9/link_modbus.h` and `firmware/network-master/main.cpp`.

| Address | Name | Write owner | Meaning |
|---|---|---|---|
| `0x0000` | LOGIN_REQ | master | `1` = show the login gate |
| `0x0001` | CREDS_READY | slave sets, master clears | `1` = staged creds ready to read |
| `0x0002` | RESULT | master | `0` none, `1` success, `2` fail |
| `0x0003` | EXP_RUNNING | display only | `1` = experiment under way |
| `0x0004` | EXP_NUM | display only | current experiment number |
| `0x0010-0x002F` | USERNAME | display only | 32 regs = 64 bytes |
| `0x0030-0x004F` | PASSWORD | display only | 32 regs = 64 bytes |
| `0x0060` | DURATION_H | display only | 1-99, quick-polled by the master |
| `0x0070-0x0077` | IP_ADDR | master | 8 regs = 16 bytes (`"192.168.1.10"`) |
| `0x0078` | PING_MS | master | avg ping to 1.1.1.1 in ms, `0xFFFF` = no data / failed |
| `0x0079` | NET_UP | master | `1` = WT32 holds a DHCP IP, `0` = link down |

Ownership rule: each register has exactly one writer. The master writes
only `LOGIN_REQ`, `CREDS_READY=0` (ack), `RESULT`, and the `0x007x` status
block. The display writes only the credential/duration block via its
Confirm handler; the master only reads those. A `0x10` write touching any
read-only register is rejected whole (exception `0x02`, nothing committed).

## 4. Transaction patterns

### 4.1 Credential collection (portal login)

```
WT32 NEED_CREDS, every 5 s          Display
  -- FC 0x06 LOGIN_REQ=1 -----------> raises /setup (idempotent while typing)
  -- FC 0x03 CREDS_READY -----------> 0 = not yet, 1 = user tapped Confirm
  -- FC 0x03 USERNAME x32 --------->
  -- FC 0x03 PASSWORD x32 --------->
  -- FC 0x03 DURATION / EXP_NUM /
     EXP_RUNNING ------------------> quick poll, every cycle, both stages
  -- FC 0x06 RESULT=1/2 -----------> 1 routes home, 2 stays with retry text
  -- FC 0x06 CREDS_READY=0 --------->
  -- FC 0x06 LOGIN_REQ=0 -----------> only on success
```

Duration, experiment number and the running flag are display-owned and
quick-polled by the master (every NEED_CREDS poll and every RUN cycle), so
post-login edits on `/control` take effect without re-login. RESULT=1 routes
home only; it does not start the run. The run starts from `/control`
(hold-to-confirm), which sets EXP_RUNNING=1 and auto-increments EXP_NUM.

Empty username or password is ignored until Confirm. Credentials live in
WT32 RAM only between capture and login and are overwritten before release.

### 4.2 Network status push (/info)

The WT32 pushes after every network event; the display stores and shows
the values when /info opens (rebuilt fresh on open, no polling needed):

- On each NEED_CREDS poll: IP + `NET_UP=1`, ping none (IP is known from
  DHCP before login, latency is not).
- On each RUN cycle: IP + fresh `PING_MS` from `Ping.ping(1.1.1.1, 3)`
  (`0xFFFF` on no reply) + `NET_UP=1`.
- On any link loss: `NET_UP=0` (clears the display's ping verdict), IP blank.

The IP block goes out as one FC `0x10` write of 8 registers; ping and link
state follow as FC `0x06` singles.

## 5. Timing budget

| Event | Period |
|---|---|
| Credential poll (NEED_CREDS) | 5 s |
| Status push while waiting | piggybacked on each poll |
| RUN cycle (ping + fetch + push) | 5 s |
| Per-transaction timeout (master) | 1 s |
| Inter-frame silence (both ends) | 8 ms |

At 9600 baud a full credential read is ~7 transactions of ~20-50 ms each,
well inside the 5 s poll. The network loop pauses in NEED_CREDS: nothing
but the display poll runs until credentials arrive.

## 6. Debug

- Display USB serial: `LINK` prints
  `req/ready/result/user/dur ip/ping/up`, e.g.
  `[LINK] req=0 ready=0 result=1 user='...' dur=24 ip='192.168.1.10' ping=15 up=1`.
- WT32 USB serial: `[MB]` lines for every poll, write failure, CRC error,
  and exception; `[PING]` for each 1.1.1.1 result.
- Loopback: short display RX to TX to echo Modbus frames during
  development without the WT32 attached.
- First suspects on a dead link: swapped data wires, missing direct GND,
  WT32 held in reset by IO5 strapping.

## 7. Extending the map

Free ranges: `0x0003-0x000F`, `0x0050-0x005F`, `0x0061-0x006F`,
`0x007A` up. To add a signal (e.g. the 5-minute sensor push):

1. Pick an address and add the same `#define` on both ends.
2. Extend the display's `reg_read` / `reg_write` / `reg_addr_valid` and
   `regCount()` together; extend the master's read/write call.
3. Keep the one-writer rule and document the new pattern in §4.
4. Update this file, `display-docs.md` §10, and `network-master.md` §3.

---
Content generated by `opencode/muse-spark-1.3-contributor-free` in `opencode`.
