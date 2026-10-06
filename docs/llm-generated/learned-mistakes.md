### 2026-10-06
- Shipped network-master DB/NTP code without compiling: a debug `q1` variable
  was declared inside the `if (at >= 0)` block but referenced after it, so the
  build failed (`'q1' was not declared in this scope`). Fix was to delete the
  variable. Lesson: run `pio run -e network-master` (and `-e display-v9` when
  the display changes) before reporting done. The project builds from the repo
  root `platformio.ini`, not from inside `firmware/`.
- Proposed WT32 IO2 as an interrupt input before stating it is a strapping pin
  (must be low/floating at boot). User wired it, then had to move the signal
  to IO14. Fix: lead with the boot constraint, propose a non-strapping pin
  (IO14/IO32/IO33) first, offer IO2/IO4/IO5/IO12/IO15 only with the warning
  attached. Lesson: check strapping + peripheral collisions (ETH RMII, UART,
  touch I2C, USB) before naming any GPIO, and put the safest option first.
  (network-master INT test, display IO11 -> WT32 IO14, LED IO2)

### 2026-10-05
- LVGL: `lv_obj_set_pos()` is an offset relative to the object's current
  alignment, not absolute screen coordinates. `lv_keyboard_create()` aligns
  itself `LV_ALIGN_BOTTOM_MID` in its constructor, so calling
  `lv_obj_set_pos(kb, 0, 310)` put the keyboard 310 px below the bottom edge,
  fully off-screen, while every other widget (default `TOP_LEFT`) positioned
  fine. Fix: `lv_obj_set_align(kb, LV_ALIGN_TOP_LEFT)` before `set_pos`.
  Lesson: after creating any LVGL widget, check whether its constructor sets
  a non-default alignment before positioning it. (display-v9 login gate)
- `strtok()` hides the information needed to recover a command's arguments.
  It replaces the first delimiter after the command word with a NUL and keeps
  the rest of the line only in internal state, so after
  `char *cmd = strtok(line, " \t")` the arguments are reachable only via a
  further `strtok(nullptr, ...)`. Reaching them as `cmd + strlen(cmd) + 1`
  looks correct and is right whenever a delimiter followed the command word,
  but reads past the end of the line into stale buffer content when it did not
  (`CREDS` with no arguments, against a `s_line` that still held a longer
  previous command). Fix: capture `strlen(line)` before tokenizing and compare
  `cmd + strlen(cmd)` against `line + lineLen` first. Lesson: whenever pointer
  arithmetic depends on a delimiter existing, check that it exists rather
  than assuming. (display-v9 `CREDS` command)
- A handler registered through `WebServer::on()` takes no arguments
  (`std::function<void()>`), so handlers cannot receive the server they are
  registered on. Writing `void handle(WebServer&)` fails to compile with a
  candidate list that looks unrelated to the mistake. Fix: reach the singleton
  through a file-scope accessor. Note that `WebServer::arg()` returns a
  `String`, so it needs `.c_str()` for `strcmp`. (display-v9 `provision_ap`)
- `lv_qrcode` is a subclass of `lv_canvas`, so `LV_USE_CANVAS` must be 1 or the
  widget's base class does not exist. Enabling `LV_USE_QRCODE` alone fails to
  link in a way that does not name the canvas. The canvas buffer is
  `ceil(size/8) * size` bytes because the format is `LV_COLOR_FORMAT_I1`, so a
  300 px code costs 11.4 KB of `LV_MEM_SIZE` — check that against the pool
  before sizing the widget. (display-v9 `/setup` QR)
- `lv_qrcode_update()` rejects an empty payload and needs valid data to draw,
  so "clear the code" means encoding a placeholder (a single space), not
  skipping the call. A stale QR left on screen still scans. (display-v9
  `/setup` QR)
- A WIFI: QR payload must not end in a newline. `echo "WIFI:T:nopass;S:x;;" |
  qrencode` fails on both iOS and Android; `echo -n` works. Relatedly, a
  third-party scanner that displays the raw `WIFI:` text instead of offering to
  join the network is expected behaviour, not a bug in the payload. (display-v9
  `/setup` QR)
- The WIFI: `T:` token is `WPA` for **every** passphrase network, not `WPA2`.
  The phone negotiates WPA2 vs WPA3 during the join; the QR never names a
  version. `T:WPA2` is not a legal token and fails unpredictably across
  handsets. `T:nopass` is the open-network form and requires `P` to be absent.
  Guess the token from the network's security type and you will ship a code
  that reads on one phone and silently does nothing on another. (display-v9
  `/setup` QR)
- A cloaked SSID requires `H:true` in the WIFI: payload. Worse, cloaked
  networks are the weak point of camera-app support: iOS in particular is
  documented as not supporting hidden SSIDs from a WIFI: QR. Make one build-time
  constant drive both the radio and the payload field so they cannot disagree,
  and expose a flag to broadcast. (display-v9 `provision_ap`)
- The argument that removing a Wi-Fi passphrase removes typing does not hold
  when a QR code carries the passphrase. Reasoning "a passphrase would have to
  be read off the screen and typed into the phone" is wrong if the passphrase
  is in the code being scanned; check what the QR actually contains before
  trading away encryption. (display-v9 `provision_ap`)
- Tracking "which of two QR payloads is on screen" with a state enum needs the
  enum poked to the *wrong* value to force a repaint, and any path that changes
  the payload without going through that one gets missed — e.g. an idle timeout
  leaving a stale join code on screen. Track the painted payload string and
  compare it instead. Same class of bug as caching with an invalidation that
  has to be remembered at every call site. (display-v9 `/setup` QR)
- Size a payload buffer for the worst case of its configurable inputs, not the
  default. An 80-byte buffer held the MAC-derived QR payload (57 bytes) but
  truncated a 63-char `PROVISION_AP_PASSWORD` override, and a truncated WIFI:
  payload is rejected by the phone with no diagnostic. Put the size in a shared
  constant so the header, the screen, and the serial command cannot disagree.
  (display-v9 `provision_ap`)


---
Model credit is unnecessary here.
