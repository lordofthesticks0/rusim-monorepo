#pragma once

// Human-typeable USB-serial protocol for driving the fake-data pass.
// Examples:  TEMP 38.3 | SETPOINT 37.5 | DURATION 24 | STIRRER ON |
//            POST OFF | NODE 2 ON | NULL 0 1 4 | UNNULL 0 1 4 | PUSH |
//            STATUS | HELP
//
// NOTE: this line protocol is display-local debug only. When the WT32 link is
// real, Central->Display traffic will be framed JSON over UART (pushed every
// 5 min and parsed into the same AppState via fake_wt32_push-style updates);
// the on-screen code must not depend on this text format.
void serial_cmd_poll();
