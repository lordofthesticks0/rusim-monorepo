#pragma once

// raw line link between the display and the WT32 (production).
//
// Wire: Display IO17 = TX, IO18 = RX, 115200 8N1, direct GND.
// WT32 side: Debug-header RXD (IO3) <- Display IO17,
//            Debug-header TXD (IO1) -> Display IO18.
// If no lines arrive, swap the two data wires first.
//
// Protocol: rusim-line v1, peer to peer. One ASCII line per message,
// '\n' terminated, 1024 bytes maximum. See
// docs/llm-generated/link-serial-protocol.md for the wire specification.
//
// Inbound words handled here: HELLO, LOGINREQ, RESULT, NET, DB, LOG.
// Outbound words: HELLO, EXPERIMENT, CREDS (DUMP via W32CMD in debug builds).
// Unknown or wrong-direction lines are ignored.

#include <Arduino.h>

#define LINK_UART_RX_PIN 18
#define LINK_UART_TX_PIN 17
#define LINK_UART_BAUD 115200

// A partially received line is discarded after this much silence.
#define LINK_GAP_MS 50
// Maximum line length in either direction, excluding the terminator.
#define LINK_LINE_MAX 1024
// Both ends declare their full state on this cadence.
#define LINK_BEAT_MS 5000

#define LINK_IP_MAX 16
#define LINK_USER_MAX 64
#define LINK_PASS_MAX 64

#define LINK_RESULT_NONE 0
#define LINK_RESULT_SUCCESS 1
#define LINK_RESULT_FAIL 2

void link_serial_init();
void link_serial_poll();

// Called from loop(). Returns true once per LOGINREQ line received.
bool link_serial_take_login_request();

// Stores a credential pair and emits CREDS immediately. The pair is re-emitted
// on every heartbeat until a RESULT for it arrives. Called by every staging
// path (phone form, USB serial).
void link_serial_set_credentials(const char *user, const char *pass);

// True while a pair has been staged and no verdict for it has arrived.
bool link_serial_creds_ready();
int link_serial_result();

// Experiment state mirror; emits EXPERIMENT immediately when it changes.
void link_serial_set_exp_state(bool running, int expNum, int durationH);
// Forces one EXPERIMENT push without changing the state (bench tool).
void link_serial_emit_experiment();

// Consumed by the boot gate when RESULT arrives.
bool link_serial_take_result(int &out);

// WT32-pushed network status.
void link_serial_get_ip(char *out, size_t n);
int link_serial_get_ping_ms();  // -1 = no data / ping failed
bool link_serial_net_up();
uint32_t link_serial_net_age_ms();  // ms since last NET line, 0 = never

// WT32-pushed DB state (backlog #9).
// Returns -2 when never synced, -1 when DB has no experiments yet, else latest id.
int link_serial_get_db_latest();
bool link_serial_time_ok();
// bottle_count from authenticate(), -1 when unknown.
int link_serial_get_db_bottles();
// Buffered push rows dropped on WT32 queue overflow.
int link_serial_get_push_drops();

// Debug over USB Serial.
void link_serial_debug_print();

// Sends one arbitrary line to the WT32 (W32CMD bench tool). Returns false when
// the line is too long or the link is in passthrough.
bool link_serial_send_raw(const char *line);

// Raw USB <-> IO17/IO18 tunnel for flashing the WT32 through the display and
// for watching unprefixed WT32 boot output. Runtime only; never persists.
void link_serial_passthrough_set(bool on);
bool link_serial_passthrough_active();
// Raw write used by the USB-side tunnel. Ignored outside passthrough.
void link_serial_passthrough_write(const uint8_t *data, size_t n);
