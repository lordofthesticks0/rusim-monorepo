# Display <-> WT32 link (`display-wt32-link`) — replaced

This document described the retired Modbus RTU master/slave link on the WT32
AT-header (`IO5`/`IO17`) with the `IO11 -> IO14` poll-request wire.

The production link is now `rusim-line` v1 on the WT32 Debug header
(`IO1`/`IO3`), documented in [`link-serial-protocol.md`](link-serial-protocol.md).
The display end is `firmware/display-v9/link_serial.{h,cpp}`; the WT32 end is
`firmware/network-master/main.cpp`. The old `link_modbus.*` files no longer
exist.

The freed `IO5`/`IO17` pair now carries the legacy Mega slave directly over
TTL, with no RS485 transceiver; `link-serial-protocol.md` §8 and
`network-master.md` §2.2 record the wiring and electrical caveat.

---
Updated at 2026-11-18-00:12.
