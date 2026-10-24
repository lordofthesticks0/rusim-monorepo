#pragma once

// Human-typeable USB-serial protocol for driving the fake-data pass.
// Examples:  TEMP 38.3 | SETPOINT 37.5 | DURATION 24 | STIRRER ON |
//            POST OFF | NODE 2 ON | NULL 0 1 4 | UNNULL 0 1 4 | PUSH |
//            STATUS | LINK | HELP
//
// NOTE: this line protocol is display-local debug on USB Serial only.
// The WT32 link is rusim-line v1 on Serial1 (IO18 RX / IO17 TX, 115200 8N1);
// see link_serial.h and docs/llm-generated/link-serial-protocol.md. USB debug
// output never goes out Serial1, so monitor logs cannot corrupt link lines.
void serial_cmd_poll();
