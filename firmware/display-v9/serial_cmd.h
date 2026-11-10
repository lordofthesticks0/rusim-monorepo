#pragma once

// Human-typeable USB controls and diagnostics; measured values are read-only.
// Examples: SETPOINT 37.5 | DURATION 24 | STIRRER ON | POST OFF |
//           RUN ON | EXP 3 | PUSH (WT32 snapshot) | STATUS | LINK | HELP
//
// NOTE: this line protocol is display-local debug on USB Serial only.
// The WT32 link is rusim-line v1 on Serial1 (IO18 RX / IO17 TX, 115200 8N1);
// see link_serial.h and docs/llm-generated/link-serial-protocol.md. USB debug
// output never goes out Serial1, so monitor logs cannot corrupt link lines.
void serial_cmd_poll();
