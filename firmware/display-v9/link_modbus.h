#pragma once

// Modbus RTU slave for the Display <-> WT32 link (production).
//
// Wiring: Display IO17 = TX, IO18 = RX, 9600 8N1, common GND.
// WT32 side: AT-header TXD (IO17) -> Display IO18 (RX),
//            AT-header RXD (IO5)  <- Display IO17 (TX).
// If no frames arrive, swap the two data wires first.
//
// Protocol: Modbus RTU, slave ID 1. No MAX485 here (direct TTL UART),
// so there is no DE/RE direction pin. Frames carry CRC16-Modbus.
//
// Register map (holding registers, big-endian, ASCII NUL-padded):
//   0x0000 LOGIN_REQ   master WR, slave RO. 1 = show the login gate.
//   0x0001 CREDS_READY master ack WR, slave RW. Slave sets 1 on Confirm,
//                        master writes 0 after it has read the credentials.
//   0x0002 RESULT      master WR, slave RO. 0=none 1=success 2=fail.
//   0x0010-0x002F USERNAME 32 regs = 64 bytes ASCII.
//   0x0030-0x004F PASSWORD 32 regs = 64 bytes ASCII.
//   0x0060 DURATION_H 1 reg, 1-99.
//   0x0070-0x0077 IP_ADDR 8 regs = 16 bytes ASCII NUL-padded, master WR.
//   0x0078 PING_MS 1 reg, master WR. Avg ping to 1.1.1.1 in ms,
//                        0xFFFF = no data / ping failed.
//   0x0079 NET_UP 1 reg, master WR. 1 = WT32 has DHCP IP, 0 = link down.
//
// Flow: WT32 writes LOGIN_REQ=1 every 5 s while it needs credentials.
// The slave raises the combined login+duration gate. The user fills
// username/password/duration and taps Confirm. The slave stores the
// strings, sets CREDS_READY=1. The master reads them with FC 0x03,
// attempts the portal login, writes RESULT, then writes CREDS_READY=0
// and LOGIN_REQ=0 to consume them.

#include <Arduino.h>

#define LINK_UART_RX_PIN 18
#define LINK_UART_TX_PIN 17
#define LINK_UART_BAUD 9600
#define LINK_MODBUS_SLAVE_ID 1

#define LINK_REG_LOGIN_REQ 0x0000
#define LINK_REG_CREDS_READY 0x0001
#define LINK_REG_RESULT 0x0002
#define LINK_REG_USER_BASE 0x0010
#define LINK_REG_USER_REGS 32
#define LINK_REG_PASS_BASE 0x0030
#define LINK_REG_PASS_REGS 32
#define LINK_REG_DURATION 0x0060
#define LINK_REG_IP_BASE 0x0070
#define LINK_REG_IP_REGS 8
#define LINK_REG_PING_MS 0x0078
#define LINK_REG_NET_UP 0x0079

#define LINK_IP_MAX 16
#define LINK_PING_NONE 0xFFFF

#define LINK_USER_MAX 64
#define LINK_PASS_MAX 64

#define LINK_RESULT_NONE 0
#define LINK_RESULT_SUCCESS 1
#define LINK_RESULT_FAIL 2

void link_modbus_init();
void link_modbus_poll();

// Called from loop(). Returns true once per master LOGIN_REQ frame.
bool link_modbus_take_login_request();

// Slave-side credential image set by the boot gate on Confirm.
void link_modbus_set_credentials(const char *user, const char *pass, int durationH);
bool link_modbus_creds_ready();
int link_modbus_result();

// Consumed by the boot gate when RESULT arrives.
bool link_modbus_take_result(int &out);

// WT32-pushed network status (master WR regs). Display is slave RO.
void link_modbus_get_ip(char *out, size_t n);
int link_modbus_get_ping_ms();  // -1 = no data / ping failed
bool link_modbus_net_up();
uint32_t link_modbus_net_age_ms();  // ms since last NET_UP write, ~0 = never

// Debug over USB Serial.
void link_modbus_debug_print();

uint16_t link_modbus_crc16(const uint8_t *data, size_t len);
