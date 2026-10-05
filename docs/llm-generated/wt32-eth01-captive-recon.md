# WT32-ETH01 Captive Portal Recon (`firmware/wt32-eth01_captive-recon`)

Scope: firmware that brings up the WT32-ETH01's Ethernet and then probes the
network for a captive portal. It is the first step of the plan in
`captive-portal-workaround.md`. It answers four questions over serial:

1. Is the port walled, and what portal URL does the interception point to?
2. Does the login page load, or does Cloudflare challenge the ESP32?
3. What are the login form's action and hidden field values this session?
4. Is the port currently authenticated?

Credentials are typed on the serial console, not compiled in.

`wt32-eth01-ping-test.md` documents the sibling ping-test firmware.

## 1. Environment

| Property | Value |
|---|---|
| Environment | `[env:wt32-eth01_captive-recon]` |
| Board | `wt32-eth01` |
| Platform / framework | `espressif32` / `arduino` |
| `monitor_speed` | 115200 |
| Libraries | none beyond the ESP32 Arduino core |
| Source | `firmware/wt32-eth01_captive-recon/main.cpp` |

## 2. What it does

1. Opens serial at 115200, waits one second for a USB monitor to attach,
   prints five blank lines to push the ROM bootloader's boot message (sent at
   74880 baud, so it renders as garbage at 115200) off screen, and flushes.
2. Prints the banner, then blocks in `waitForEnter()` until a newline arrives.
   The wait is indefinite and anything before the newline is ignored, so
   stray boot bytes can never be mistaken for input. The RX buffer is drained
   both before the prompt and after it. These sketches are interactive-only:
   without a terminal nothing proceeds, which is consistent with the
   credential prompts that follow.
2. Registers `onEthEvent()` and calls `ETH.begin()` with the variant's RMII
   pin constants (PHY addr 1, power 16, MDC 23, MDIO 18, LAN8720).
3. When DHCP delivers an IP, the sketch prompts on serial for the portal
   username and password (see §5).
4. `runRecon()` then runs once:
   - **Step 1:** GET `http://example.com/`. The campus network intercepts this
     and answers `200` with a body containing a JavaScript redirect. The sketch
     parses `window.location="..."` out of the body to obtain the portal URL,
     and also prints the `Location` header for comparison (normally empty, since
     the interception is not an HTTP redirect).
   - **Step 2:** HTTPS GET of the portal URL with `setInsecure()`. Prints status,
     `Content-Type`, `Server`, `Set-Cookie`, then scans the streamed body and
     prints the form `action`, the `4Tredir` and `magic` hidden values, whether
     a `username` field is present, whether a Cloudflare challenge marker was
     seen, and the total body size.
   - **Verify:** HTTPS GET of `https://unison.boidu.dev/lyrics?v=dQw4w9WgXcQ`
     (the same real-data fetch the ping-test sketch performs). The body is
     checked for the interception marker instead of trusting the status code,
     because the campus network answers intercepted requests with `200`.
5. Prints "Recon complete" and does not repeat unless the board resets.

Reading the output:

| Step 1 result | Meaning |
|---|---|
| `window.location` prints a `pintas.unpad.ac.id:1003/fgtauth?` URL | Portal intercepted this request; the port is walled |
| `window.location: (none)` and status 200 | No interception on that URL; run the verify probe to decide whether the port is authenticated |

| Step 2 result | Meaning |
|---|---|
| status 200 with a `form action` and a `magic` value | The login page loads for the ESP32; headless login is feasible |
| `cloudflare challenge: YES` | Cloudflare is challenging the ESP32, which cannot solve the JavaScript challenge |
| `no form found` | The scan finished without seeing a `<form`; either the page is not the login page, or the pattern needs updating |
| negative code | TCP or TLS failure reaching the portal |

| Verify result | Meaning |
|---|---|
| real content, no `window.location` marker | Authenticated. The body is printed (truncated at 4 KB). |
| `200` plus an interception page | Walled. Re-authentication needed. |

The verify probe fetches a real resource and inspects the body because the
interceptor answers with `200`, so the status code alone proves nothing. The
same check will serve as the login success detector once the POST is
implemented: real lyrics back means the credentials worked.

## 3. Memory behaviour

The ESP32 has no PSRAM on this board, so the login page is never held in memory.

- The Unpad login page is roughly 8 KB, almost all of it inline CSS before the
  form. Buffering it truncates before the `<form>` element is reached: an
  earlier version capped reads at 4096 bytes and reported
  `form action: (no form found)` and `body size: 4352`.
- `scanPage()` therefore streams the body in 256-byte chunks and keeps only a
  320-character rolling window (`kWindow`). Each chunk is appended, the window
  is searched for `<form`, `name="magic"`, `name="4Tredir"`,
  `name="username"`, and three Cloudflare challenge markers, then the window is
  trimmed back to its last `kWindow` characters. The window is wider than the
  longest needle, so a token split across a chunk boundary is still found.
  Results land in a small `PageScan` struct; peak heap use is one window plus
  one chunk. Both appends use `concat(ptr, n)` with the exact byte count
  rather than `operator+=`, because the 256-byte chunk buffer is not
  NUL-terminated and `+=` would over-read into stale stack memory. That bug
  once corrupted a `magic` token mid-string
  (`056714bc...8eef9205` printed with garbage bytes inserted).
- `readBody()` is kept only for the interception page, which is about 120
  characters, and for the verify probe. It is capped at `kMaxBody` (4096).
- `Location`, `Set-Cookie`, and `Content-Type` are stored as short `String`s
  from `collectHeaders()`; a portal that sets an enormous header could still
  exhaust heap, which is a known limit for v1 of this sketch.

## 4. Capturing the output

```bash
pio run -e wt32-eth01_captive-recon -t upload
pio device monitor -e wt32-eth01_captive-recon | tee recon.log
```

`tee` keeps the serial output on disk. The sketch already prints the parsed
form fields, so no extraction step is needed. Paste `recon.log` for analysis.

## 5. Serial credential entry

The portal password is asked for on the serial console rather than compiled into
the firmware, so it never reaches the flash image and never sits in a file in
this repository. The reasoning is that the device lives in a shared lab and
anyone can attach a USB-serial adapter.

`loop()` advances through `STAGE_WAIT_IP` → `STAGE_PROMPT_USER` →
`STAGE_PROMPT_PASS` → recon → `STAGE_DONE`, so a link drop before the prompt
returns the sketch to waiting for an IP instead of hanging.

`readLine(echo, timeoutMs)` handles the line editing itself:

| Behaviour | Detail |
|---|---|
| Echo | Username is echoed as it is typed. Password is not echoed at all, so it stays out of terminal scrollback and out of `tee` output. |
| Backspace | `0x08` and `0x7F` delete the previous character; a visible backspace is printed only when echoing. |
| Timeout | 30 seconds. An empty line at either prompt is treated as a timeout and ends the run; reset the board to try again. |
| Carriage returns | Ignored, so a terminal sending `\r\n` does not submit early. |

Credentials are held in two `String`s in RAM for the duration of the run and
cleared (`username = ""; password = "";`) once recon finishes. Clearing a
`String` does not wipe the underlying heap allocation, so a determined reader
of the board's RAM could still recover the password; that is acceptable for a
lab credential and not for a long-lived production secret.

The Arduino ESP32 core used here exposes no `setEcho()` API, which is why the
suppression is done by not printing rather than by asking the driver to
suppress it.

## 6. Limits

- **The credentials are read but never sent.** This sketch does not log in; the
  POST that authenticates the port is not implemented yet.
- **Credentials are re-entered on every boot.** Nothing is persisted, which is
  the intended trade-off: no credential survives a power cycle.
- **TLS verification is disabled** on the portal fetch. The interception sits
  above TLS termination, so the certificate is not checked.
- **The scan finds only the attributes it names.** `scanPage()` looks for the
  form `action`, `magic`, `4Tredir`, a `username` field, and Cloudflare
  challenge markers. A portal that renamed or reordered those attributes would
  need the pattern list updated.
- **One-shot.** A new run needs a reset (`EN` button or re-flash).

## 7. Related documents

- `captive-portal-workaround.md` — the full headless authentication plan;
  this sketch implements its Phase 3 reconnaissance.
- `wt32-eth01-ping-test.md` — the sibling Ethernet smoke test.
- `wt32-eth01.md` — board hardware, pinout, gotchas.
- `platformio-environments.md` — the build environment.

---
Content generated by `opencode/fledge-alpha-free` in `opencode`.
