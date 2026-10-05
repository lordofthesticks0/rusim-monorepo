#pragma once

// Human-typeable USB-serial protocol for driving the fake-data pass.
// Examples:  TEMP 38.3 | SETPOINT 37.5 | DURATION 24 | STIRRER ON |
//            POST OFF | NODE 2 ON | NULL 0 1 4 | UNNULL 0 1 4 | PUSH |
//            STATUS | LINK | HELP
//
// NOTE: this line protocol is display-local debug on USB Serial only.
// The WT32 link is Modbus RTU slave ID 1 on Serial1 (IO18 RX / IO17 TX,
// 9600 8N1); see link_modbus.h for the register map. USB debug output
// never goes out Serial1, so monitor logs cannot corrupt Modbus frames.
void serial_cmd_poll();
