# Phone credential provisioning on the display (`provision_ap`)

Scope: the SoftAP, the two-step QR flow, and the web form on the display board
that replace on-screen typing for the campus captive portal login. Source is
`firmware/display-v9/provision_ap.{h,cpp}`, with the screen side in
`screen_setup.cpp` and the serial side in `serial_cmd.cpp`.

The portal login itself is unchanged and still lives on the WT32:
`network-master.md` §5. `display-wt32-link.md` describes the wire. This
document covers only where the credentials come from.

## 1. Why

The `/setup` route used to collect a username and a password through an
`lv_keyboard` on an 800×480 touch panel. Typing a real credential pair on that
keyboard is slow, and a mistyped password is indistinguishable from a wrong
one until the WT32 reports a failure. The keyboard has been removed. The
device now shows a QR code, and the phone does the typing.

The design constraint that shaped the choice: the display cannot reach the
portal. It has no wired network of its own, and `network-master` is the only
board that talks to the campus network. So the display does not authenticate
anything. It hosts a form, and whatever arrives is staged onto the existing
Modbus registers. `network-master` required no changes at all.

## 2. The two-step flow

The user taps **Start phone login**, which raises the AP and paints step 1.
The screen then switches codes on its own as the phone progresses:

| Step | QR encodes | Phone action |
|---|---|---|
| 1 | `WIFI:T:WPA;S:RUSIM-SETUP-A1B2C3;P:setup-C3D4E5F6;H:true;;` | camera scans, phone joins the AP |
| 2 | `http://192.168.4.1/` | camera scans, phone opens the form |

A phone cannot open the URL until it has joined the network, which is why the
join code has to come first. The transition is driven by
`WiFi.softAPgetStationNum()`: once any station associates, the QR re-encodes as
the URL and the step label changes. If the phone disconnects, the code reverts
to step 1, so someone who walks away and comes back is told how to rejoin.

Both codes are re-encoded in place in a single `lv_qrcode` object. See §6 for
why there is not one widget per step.

## 3. QR payload format

Step 1 uses the WIFI: URI defined by ZXing and adopted by the Wi-Fi Alliance in
the WPA3 specification section 7. For this AP, a WPA2 network with a cloaked
SSID, the payload has all four fields:

```
WIFI:T:WPA;S:<ssid>;P:<passphrase>;H:true;;
```

Three of those choices are load-bearing, and each was wrong in an earlier
revision of this code:

**`T:WPA`, never `T:WPA2`.** The format has one token for every
passphrase-protected network. `WPA` covers WPA2 and WPA3, and the phone
negotiates the actual protocol during the join; the QR never names a version.
`T:WPA2` is not a legal token and behaves unpredictably across handsets — some
treat it as WPA, others fail the join silently. `T:nopass` is the open-network
form and requires `P` to be absent.

**All four fields are always emitted.** A three-field payload is valid for most
scanners, but the failure mode when a parser guesses wrong is a phone that
scans the code and then does nothing, which reads as "the QR is broken". The two
fields that describe the radio, `T:` and `H:`, are generated from the same
constants that configure the radio, so the code and the access point cannot
disagree.

**No trailing newline.** iOS and Android both reject the code. `snprintf` does
not add one, which is the main reason to build the payload in C rather than
concatenate it in a shell.

Both iOS (11+) and Android (10+) read this with the stock camera app, so no
third-party scanner is needed. A third-party scanner that only displays the raw
`WIFI:` text instead of offering to join is the usual cause of "it did not
work".

Neither string can contain a character that needs escaping (`;`, `,`, `:`, `"`,
`\`), because both are built from MAC hex digits and two fixed literal prefixes.
That is why there is no escaper in `build_identity()`'s callers. It is a
property of how the strings are built, not a general guarantee: a
`PROVISION_AP_PASSWORD` override containing any of those characters needs
escaping added, and will otherwise produce a code that some phones read and
others reject.

Step 2 is a plain URL, rendered black-on-white. Phone cameras key on a light
background, and the panel background is dark, so the QR colours are set
explicitly rather than left to the widget's defaults.

## 4. Lifecycle

| Event | Effect |
|---|---|
| Boot | AP down. Nothing is listening on any radio. |
| Tap "Start phone login" on `/setup`, or send `AP` over USB serial | AP up, HTTP on port 80, QR shows step 1 |
| A station associates | QR re-encodes as the step 2 URL |
| The last station disconnects | QR reverts to the step 1 join code |
| 10 minutes with no HTTP request | AP torn down |
| Tap "Stop", or send `AP` again | AP torn down |
| Navigate off `/setup` | AP torn down |
| `RESULT=success`, run starts | AP torn down |

The AP is never started automatically. A board sitting on a bench with a
reachable credential endpoint is a worse default than a board that has to be
asked first, and the idle timeout covers the case where nobody comes back.

`screen_setup_poll()` checks `!ui_is_setup() && provision_ap_active()` every
loop, so leaving the route tears the AP down even if the tap did not go
through the button. `setup_close_success()` tears it down explicitly before
`ui_show_home()`, because the route check alone would race the rebuild.

## 5. Cloaked and encrypted, and why

The AP is WPA2 with a passphrase, and the SSID is cloaked (`PROVISION_AP_HIDDEN=1`
by default). An earlier revision shipped it open on the reasoning that a
passphrase would have to be read off the panel and typed into the phone. That
reasoning was wrong: the passphrase is in the QR code, so nobody types it. Both
properties came back without reintroducing any typing, and they cost 44 bytes of
heap.

Cloaking means the network does not appear in a phone's network list at all.
Nothing has to discover it; the QR carries the SSID.

### One constant drives both the radio and the payload

`kHidden` in `provision_ap.cpp` is the single source of truth. It goes to
`WiFi.softAP()` as `ssid_hidden` and to the payload's `H:` field. A payload
that disagrees with its access point is rejected quietly, so making disagreement
impossible is worth more here than the convenience of two independent settings.

To broadcast instead:

```sh
pio run -e display-v9 --project-option "build_flags=-D PROVISION_AP_HIDDEN=0"
```

## 6. Identity and memory

| Item | Value |
|---|---|
| SSID | `RUSIM-SETUP-<last 3 MAC bytes, uppercase hex>` (19 chars) |
| Passphrase | `setup-<first 4 MAC bytes, uppercase hex>` (14 chars) |
| Security | WPA2-PSK, CCMP, SSID cloaked |
| Channel | 1, fixed |
| IP | `192.168.4.1`, set explicitly via `WiFi.softAPConfig()` |
| Port | 80 |

Both strings come off the base MAC via `esp_efuse_mac_get_default()`, so they
are stable across reboots and unique per board. A phone that joined once can
reconnect without rereading anything, which is why the channel is fixed at 1
rather than left to the default scan.

The passphrase is predictable to anyone who holds the device, which is why the
right column also prints it (see §9). Override before deploying anywhere
unattended:

```sh
pio run -e display-v9 --project-option 'build_flags=-D PROVISION_AP_PASSWORD="some-long-secret"'
```

An override outside 8–63 characters is rejected at boot with a console warning
and the MAC-derived default is used instead. `WiFi.softAP()` would also refuse a
short passphrase, but it fails by returning `false` with nothing on the console,
so the check is explicit.

One QR object, 300 px square. `lv_qrcode_set_size()` allocates through
`lv_draw_buf_create()` in `LV_COLOR_FORMAT_I1`, so the buffer is
`ceil(300/8) * 300 = 11400` bytes of LVGL heap, not internal RAM. `LV_MEM_SIZE`
is 64 KB, shared with the spinbox, labels, and chart. Two codes of this size
would be 22.8 KB and would technically fit, but a single object re-encoded in
place is simpler and keeps the headroom.

Re-encoding is not free: `lv_qrcode_update()` clears the canvas and walks the
whole buffer. It is therefore only called when the payload differs from the last
one painted, which `qr_show()` compares against `s_qrShown[PROVISION_QR_MAX]`.

Tracking the payload string rather than a two-value stage enum is deliberate.
The stage version had to be poked to the *wrong* value to force a repaint, which
worked for the Start button and silently missed the idle-timeout path in
`provision_ap_poll()`, leaving a join code on screen for a network that no
existed. Any path that changes the payload is now handled the same way.

In practice that is once when the AP starts and once when the phone associates.
The 5-minute fake data push calls `ui_refresh_current()`, which already skips
route 6, so it cannot cause a repaint.

`PROVISION_QR_MAX` is 128, sized for the worst case: a 31-char SSID and a
63-char `PROVISION_AP_PASSWORD`. The MAC-derived default is 57 bytes and would
fit an 80-byte buffer, but sizing to the default would truncate a long override
into a payload the phone rejects with no diagnostic.

When the AP is down the canvas holds a single space rather than being blanked,
because `lv_qrcode_update()` needs valid data to draw. A stale scan cannot
reach a network that no longer exists, because the AP is what a phone would
have to be connected to in order to try.

The `WebServer` object is allocated once with `new` in `provision_ap_begin()`
and kept across stop/start cycles, so repeated toggles do not fragment the
heap. Only the handlers are re-registered each time.

## 7. Endpoints

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/` | Credential form, with the session token embedded |
| `POST` | `/creds` | Stage credentials, reply with a result page |
| `GET` | `/status` | Plain-text state, polled by the result page |
| any | other | 404 |

`/creds` requires a `t` field matching the per-boot token that `/` embeds.
Without it, any page open in a browser on the AP could blind-POST credentials
into the device. The token is 16 hex characters from `esp_random()`, generated
in `provision_ap_begin()` and cleared in `provision_ap_end()`.

What the token buys is narrower than it looks. A client that has associated can
just `GET /`, read the token, and post with it, so this blocks cross-site POSTs
and not a local user. Association is the actual gate, and WPA2 is what now
provides it.

Field handling on `POST /creds`:

- `user` and `pass` are both required and non-empty after trimming. Phone
  keyboards add trailing spaces, which the portal rejects as a wrong password
  rather than as a typo.
- Length is capped at `LINK_USER_MAX - 1` / `LINK_PASS_MAX - 1`, matching what
  the Modbus registers hold. Over-long input is refused rather than silently
  truncated, because a truncated password fails at the portal with no useful
  diagnostic.
- On success the handler calls `link_modbus_set_credentials()` and replies with
  a page that polls `/status` every 1500 ms. Duration and experiment number
  are no longer part of the login form; they live on `/control` and are
  quick-polled by the master.

`/status` reports one of four states:

Every body leads with a machine-readable state token followed by the message
the page prints: `waiting:`, `success:`, or `fail:`.

| State | Body | Result page |
|---|---|---|
| `LINK_RESULT_SUCCESS` | `success: Login succeeded...` | green box, stops polling |
| `LINK_RESULT_FAIL` | `fail: Wrong username or password...` | red box, new heading, **Enter them again** link, stops polling |
| creds ready, no verdict | `waiting: Credentials received...` | keeps polling |
| nothing staged | `waiting: Waiting for credentials...` | keeps polling |

The page stops or retries on the token, not on a substring of the message, so
re-wording a message cannot strand the page mid-poll. A reject also gets a link
back to `/`, which is served with the matching banner (§7.1). The handler reads
`link_modbus_result()`, not `link_modbus_take_result()`: the latter consumes
the pending flag and `screen_setup_poll()` owns it. If `/status` consumed it,
the display would never route to `/home`.

### 7.1 A wrong password, and where the verdict appears

| Surface | What the user sees |
|---|---|
| the phone's result page | red box, heading "Wrong username or password", and an **Enter them again** link |
| the phone's form page (`GET /`) | a red banner above the form |
| the display panel | `/setup` status line: "Wrong username or password. Correct them on your phone and resend." |

The banner reads `RESULT`, which the master leaves at `fail` until the next pair
is staged. That is what makes it survive the page navigation and clear itself on
the retry; `creds_ready` is checked with it so a retry already in flight cannot
put a stale banner over the new attempt.

Submitting that retry stages the pair and pulses the poll-request line
(`display-wt32-link.md` §4.3), so the WT32 re-reads it on its next loop pass
rather than on its next 5 s cycle. The loop is: wrong password, red verdict,
correct it, immediate re-poll.

No user-supplied text is interpolated into any HTML. The staged username is
never echoed to a page; the form reports state, not values.

## 8. Credential lifetime

Unchanged from the RAM-only rule in `network-master.md` §6 and
`display-wt32-link.md` §4.1:

1. The phone sends the pair over the AP.
2. The display holds it in `link_modbus`'s `s_user` / `s_pass`, sets
   `CREDS_READY`.
3. The WT32 reads it, logs in, overwrites its own copy in `clearCreds()`.
4. The WT32 writes `CREDS_READY=0`.

Nothing is written to NVS or flash on either board. The `arg()` Strings hold
the password in heap for the duration of the handler; the handler assigns
`String()` to them after staging to release the buffers early. That is
best-effort, not a guarantee: the ESP32 heap is not scrubbed, so a password
handled this way can survive in free heap until overwritten. The same is
already true of the Modbus staging path, so this introduces nothing new, but
it is why the rule is stated as "RAM only" rather than "erased".

The USB serial `CREDS` command (`CREDS <user> <pass>`) is a third entry point
and the weakest of the three, since the password lands in terminal scrollback
and in any `pio device monitor` log. It exists for bench testing. Prefer the
QR flow.

## 9. Screen layout

`/setup` is now QR-left, controls-right. The content area is 800×436 (the
status bar takes the top 44 px).

| Widget | Position | Size | Role |
|---|---|---|---|
| Title | (16, 6) | 500×26 | "Setup: network login" |
| Step label | (16, 34) | 760×20 | which scan to do next |
| QR | (24, 64) | 300×300 | join code, then URL |
| "Network:" / SSID | (356, 64) / (356, 86) | 400 wide | SSID, with "(hidden)" appended |
| "Password:" / passphrase | (356, 116) / (436, 116) | 260 wide | manual-join fallback |
| "Duration:" + spinbox, ± | (356, 150) / (356, 172) | 120×42, 48×42 | 1–99 h |
| Start/Stop button | (356, 236) | 300×48 | toggles the AP |
| Status | (24, 380) | 740×20 | last verdict |

300 px leaves a quiet zone and stays scannable at roughly arm's length on a
bench. All of it fits the 800×436 content area with no overlaps.

### 9.1 The association popup

The step-1 to step-2 swap re-encodes the payload into the same `lv_qrcode_t`
canvas, so the panel shows a 300×300 square staying a 300×300 square with its
entire contents replaced. A phone camera pointed at it does not notice, and the
user's attention is on the handset rather than the screen. The only cue was the
step label flipping from "Step 1" to "Step 2" in `UI_COL_DIM` at the top of the
screen. It went unnoticed in testing.

The fix is a blocking `ui_modal_show()` on the rising edge of
`provision_ap_client_count() > 0`, telling the user to press OK and scan again.
It deliberately covers the QR: covering the code is the point, since the whole
problem is that the code gives no sign of having changed. The ack press is what
returns the user's attention to the panel, and the body text carries the
instruction.

Two details worth keeping:

- **The latch is on the client count, not the painted payload.** Both change
  together when a phone associates, but the payload also changes when the AP is
  stopped and restarted with a phone still attached, which is not an arrival and
  should not raise a popup. `s_phoneSeen` clears in the zero-client branch so the
  next association re-arms it.
- **The modal must be hidden on the way out.** It is a sibling of `s_content`,
  not a child, so `ui_show_home()` runs `lv_obj_clean(s_content)` and leaves the
  modal stranded over the home screen. `setup_close_success()` calls
  `ui_modal_hide()` before routing away.

The step label is also compared against `s_stepShown` before being set.
`lv_label_set_text()` reallocates and re-wraps the text buffer on every call, and
`qr_refresh()` runs every `loop()`, so the unconditional version churned a heap
allocation roughly 200 times a second for a string that changes twice per login.
`screen_setup_show()` clears that cache, because the label is rebuilt with the
same string the AP-down branch uses and the first paint would otherwise be
skipped.

The SSID and passphrase are printed on screen. That is the fallback for a phone
that scans the join code and does nothing: Settings → Wi-Fi → Other network,
then type what the panel shows. Only the SSID is a problem in the common case
(a cloaked network does not appear in the network list at all); the passphrase
is a single field. `join_details_refresh()` keeps both current, since they have
to follow the AP up and down rather than being read once at build time.

The duration spinbox has moved to `/control` (together with the experiment
number and the run switch), so the same value applies whether credentials
arrive by QR or by serial. The on-screen Confirm button is gone; there is
nothing on the panel to confirm.

`LV_USE_QRCODE` and `LV_USE_CANVAS` were `0` in `lv_conf.h` and are now `1`;
the widget is a `lv_canvas` subclass and will not build without the canvas.
`LV_USE_KEYBOARD` is now `0` — nothing uses it, and leaving it on would carry
the dead widget into every build.

## 10. Serial commands

```
CREDS <user> <pass>   stage portal credentials for the WT32
AP                    toggle the cloaked SoftAP credential form
```

`AP` is the bench shortcut: it starts the AP and prints both QR payloads and
the URL to the console, which is faster than scanning when the board is on a
bench with the screen facing away. It prints the full payload including the
passphrase, which is why the payload is the useful thing to print and not the
SSID and passphrase separately.

`CREDS` reuses the existing `strtok` tokenizing. The command word is
uppercased in place and `strtok` has replaced the delimiter after it with a
NUL, so the arguments start at `cmd + strlen(cmd) + 1`. That pointer is only
valid if a delimiter followed the command word, so the handler compares
`cmd + strlen(cmd)` against `line + lineLen` first. Without that check a bare
`CREDS` reads past the end of the line into whatever the previous, longer
command left in `s_line`.

The line buffer is 200 bytes. 96 would not hold two 64-byte fields plus the
command word.

## 11. What is not covered

- **No hardware run.** The build is verified; the WIFI: payload format, the
  step-2 transition logic, the canvas allocation size, and the widget layout
  were checked on the host. No ESP32-S3 has executed this. See §12.
- **Credentials are still RAM-only**, so the flow repeats after every reboot.
  Persisting to encrypted NVS would remove the last typing and reverses a
  deliberate security decision. See §8.
- **No HTTPS.** WPA2 protects association, but the form is plain HTTP, so the
  portal credentials cross the air in the clear to anyone who captures the
  handshake. Adding TLS would need a certificate and roughly 30 KB more flash.
  The exposure is bounded by the window in §4.
- **The passphrase is on the screen.** That is the manual-join fallback, and it
  is also what makes a phone that will not auto-join from a QR code usable. It
  costs nothing in security terms, because it is MAC-derived and the AP only
  exists while someone is holding the device. Set `PROVISION_AP_PASSWORD` before
  deploying unattended, and note that the screen will print that value too.
- **The step transition is association-based, not request-based.** A phone
  that joins and then sleeps still flips the screen to step 2. Driving it off
  the first HTTP request instead would be tighter, but it fails the case
  where the camera opens the URL without completing the scan.
- **The popup has not been seen on hardware.** It compiles and the trigger logic
  is reasoned from `softAPgetStationNum()`, but no ESP32-S3 has run it. The
  on-device steps are in §12.

## 12. Verify

Compile only, no board needed:

```sh
pio run -e display-v9
```

On device:

1. Flash, open `/setup`, tap **Start phone login**. The QR appears and the
   step label reads "Step 1".
2. Console:
   `[AP] up: ssid='RUSIM-SETUP-A1B2C3' wpa2 hidden=1 at http://192.168.4.1/`
3. Confirm the SSID does **not** appear in the phone's Wi-Fi network list. That
   is `hidden=1` working.
4. Scan with the phone's **stock camera app**. The join prompt should name the
   SSID from the panel. Nothing should need typing.
5. Within a second or two the popup appears: "Phone connected", covering the QR,
   asking for OK. Console prints `[SETUP] client associated -> prompt for second
   scan`.
6. Press **OK**. The popup clears and the step label reads "Step 2: scan again to
   open the login page."
7. Scan again. The form opens on the phone.
8. Send credentials. Console prints `[AP] staged user='...' dur=24h over the
   air`, then `[LINK] creds staged user='...' dur=24h ready=1`. No password.
9. The WT32 logs in; the phone page stops polling on a terminal state.
10. Navigate off `/setup`; console prints `[AP] down`.

Wrong password, end to end (WT32 attached):

1. Send a deliberately wrong pair. The page turns red: heading "Wrong username
   or password", the message from `/status`, and an **Enter them again** link.
2. Console: `[LOGIN] FAILED: still walled...`, then `[LOGIN] will re-poll
   display in 5 s (or at once on a poll request)`. The panel's `/setup` status
   line carries the same verdict.
3. Tap **Enter them again**. The form comes back with the red banner above it.
4. Send the correct pair. The display pulses IO11, the WT32 prints `[INT]
   display staged new credentials; polling now` at once rather than up to 5 s
   later, and the page turns green.

Without a WT32 attached, short display RX to TX to loop back Modbus frames
(`display-wt32-link.md` §6), then `CREDS alice s3cret` over USB serial and
`LINK` to confirm the registers took the values.

`AP` over USB serial prints both payloads without needing the screen, which is
the fastest way to confirm the payload string on the bench:

```
[AP] ssid='RUSIM-SETUP-A1B2C3' hidden=1
[AP] step 1 QR: WIFI:T:WPA;S:RUSIM-SETUP-A1B2C3;P:setup-C3D4E5F6;H:true;;
[AP] step 2 QR: http://192.168.4.1/
```

Worth checking by hand:

- The scan does nothing, or shows raw text. Decode the code with any generic QR
  reader and check the string is `WIFI:T:WPA;...` with all four fields and no
  trailing newline. If the string is right, the phone's camera is the problem:
  use the stock app rather than a third-party scanner.
- A third-party scanner app shows the payload as plain text instead of offering
  to join. Use the stock camera app; this is expected, not a bug.
- No prompt at all on an iPhone. Cloaked SSIDs are the weak point of WIFI: QR
  support across handsets. Rebuild with `-D PROVISION_AP_HIDDEN=0` and retest
  before concluding the payload is wrong.
- Disconnect the phone's Wi-Fi and confirm the screen reverts to step 1, with
  the popup popping again on the next association rather than staying silent.
- Stop and restart the AP with the phone still attached. The screen returns to
  step 1, but no popup appears: the count never fell to zero, so this is not a
  new arrival.
- Complete the login to RESULT=success while the popup is still up. The home
  screen must come up clean, with no popup stranded over it.
- `POST /creds` without `t` returns 403.
- A phone left idle for 10 minutes finds the AP gone.
- A 70-character username returns the "too long" page rather than staging a
  truncated string.
- `-D PROVISION_AP_PASSWORD="short"` logs the length warning at boot and the
  AP still comes up on the MAC-derived default.

## 13. Related documents

- `display-docs.md` §10, §12 — the Modbus link and the `/setup` route.
- `display-wt32-link.md` — the register map and both flows.
- `network-master.md` §5 — the portal login that consumes these credentials.
- `captive-portal-workaround.md` §2.2, Phase 4 — the original plan for a
  SoftAP, written for the WT32. This is the same idea moved to the display.
- `../hardware.md` §2 — WT32 notes (read-only reference).

---
Content generated by `opencode/space-bunny-free` in `opencode`.
Updated by `freebuff/buffy` in `freebuff`: `/status` state tokens, the wrong-password surfaces on the phone and the panel (§7.1), and the poll-request pulse on a staged pair.
