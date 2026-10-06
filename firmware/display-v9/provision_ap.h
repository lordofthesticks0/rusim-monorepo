#pragma once

// Portal credential provisioning over a SoftAP, on the display board.
//
// Why this exists: typing a username and password on the touchscreen is a
// poor experience. This module raises a short-lived access point, serves a
// small web form, and stages whatever arrives into the same Modbus credential
// registers. `network-master` is unaware of this path and needs no change: both
// callers converge on link_modbus_set_credentials().
//
// The network is WPA2 and, by default, cloaked: the SSID is not broadcast and
// the access point does not appear in a network list. The join QR carries
// everything the phone needs, so nothing has to be discovered or typed. Adding
// the WPA2 passphrase back does not reintroduce the friction the on-screen
// keyboard caused, because the passphrase is in the QR too.
//
// The passphrase is derived from the base MAC: readable on screen for the manual
// join path, and predictable to anyone who knows the MAC. Override it before
// deploying anywhere unattended.
//
// Build flags:
//   -D PROVISION_AP_PASSWORD="secret"  8-63 chars, replaces the MAC-derived one
//   -D PROVISION_AP_HIDDEN=0            broadcast the SSID instead of cloaking it
//
// Credential lifetime is unchanged: the strings live in display RAM until the
// WT32 reads them and acks, and the WT32 overwrites its own copy after login.
// Nothing here writes to NVS or flash.
//
// The AP is deliberately NOT started automatically. It stays down until the
// user asks for it on /setup or over USB serial, and it is torn down as soon
// as the route is left, so the device does not advertise a
// credential-accepting endpoint continuously.
//
// Endpoints:
//   GET  /        credential form
//   POST /creds   stage credentials, reply with a result page
//   GET  /status  plain-text state for the result page to poll; leads with a
//                 "waiting" | "success" | "fail" token, then the message
//
// A staged pair also pulses the poll-request line (link_modbus, Display IO11 ->
// WT32 IO14) so the master reads it immediately. A rejected pair leaves RESULT
// at fail, which is what puts the wrong-password banner on the form.

#include <Arduino.h>

// Idempotent. Starts the AP and the HTTP server if they are not already up.
void provision_ap_begin();

// Stops the HTTP server and the AP. Safe to call when already stopped.
void provision_ap_end();

// Call from loop(). Cheap when no client is connected.
void provision_ap_poll();

bool provision_ap_active();

// Valid after provision_ap_begin(). Returned strings are owned by this module
// and stay valid until provision_ap_end().
const char *provision_ap_ssid();
const char *provision_ap_passphrase();

// Compile-time PROVISION_AP_HIDDEN. True means the SSID is not broadcast.
bool provision_ap_hidden();

// "WIFI:" + "T:WPA;" + "S:" + 31-char SSID + ";P:" + 63-char passphrase +
// ";H:false;;" + NUL. 128 covers the worst case with room to spare; sizing to
// the MAC-derived default (57 bytes) would truncate a PROVISION_AP_PASSWORD
// override, and a truncated QR payload is rejected silently by the phone.
#define PROVISION_QR_MAX 128

// Writes the WIFI: URI payload that joins the AP, e.g.
// "WIFI:T:WPA;S:RUSIM-SETUP-A1B2C3;P:setup-C3D4E5F6;H:true;;". Buffer must be at
// least PROVISION_QR_MAX bytes.
void provision_ap_wifi_qr_payload(char *out, size_t n);

// The URL a phone should open once it has joined the AP, e.g.
// "http://192.168.4.1/". Owned by this module, valid while the AP is up.
const char *provision_ap_url();

// Number of stations currently associated with the SoftAP. Used by /setup to
// decide whether to show the Wi-Fi join code or the URL code: a phone can only
// open the URL after it has joined. Returns 0 when the AP is down.
int provision_ap_client_count();

// True once credentials have been staged and the WT32 has not yet reported a
// verdict. Drives the result page's polling text.
bool provision_ap_awaiting_verdict();
