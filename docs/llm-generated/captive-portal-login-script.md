# Captive portal login from Linux (bash)

`legacy/pi/captive-portal-login.sh` logs a Linux host into a walled
network's captive portal. It is a line-for-line port of the login sequence in
`firmware/network-master/main.cpp` (§5 of `network-master.md`), so a portal that
rejects the script can be compared against the firmware's serial log for the
same failure.

The script exists because the firmware cannot be the first thing to log in: it
asks the display for credentials over Modbus, and the display asks the phone or
the USB console. A host that is already attached to the walled network has no
such path, so the same logic runs standalone here.

## 1. Usage

```
legacy/pi/captive-portal-login.sh --user alice
legacy/pi/captive-portal-login.sh --user alice --pass s3cret
legacy/pi/captive-portal-login.sh                    # prompts for both
legacy/pi/captive-portal-login.sh --ping --verbose
legacy/pi/captive-portal-login.sh --max-attempts 2
```

| Option | Meaning |
| --- | --- |
| `--user <name>` | Portal username. Prompted for when missing. |
| `--pass <secret>` | Portal password. Prompted for (hidden) when missing. A password given as a flag stays in shell history and is visible in `ps`; prefer the prompt. |
| `--probe-url <url>` | Connectivity probe. Default `http://connectivitycheck.gstatic.com/generate_204`. |
| `--max-attempts <n>` | Stop after `n` rejected logins instead of asking again. Default `0`, which keeps asking. Only matters when stdin is a terminal. |
| `--ping` | Ping `1.1.1.1` three times after success. Informational only; a failure does not change the exit code. |
| `--verbose` | Print every response header and the POST response head. |
| `-h`, `--help` | Usage. |

Exit codes:

| Code | Meaning |
| --- | --- |
| 0 | Authenticated (the probe answers 204). |
| 1 | Still walled: the login was refused, or the portal served no login URL. |
| 2 | Usage error, missing `curl`, or no credentials on stdin. |
| 3 | The probe URL itself could not be fetched (no route, DNS failure, timeout). |

The script is not a daemon: it does not re-check the session after it exits. It
does keep running while the portal rejects the credentials it was given, and
asks for a new username and password after each rejection (§2, step 5). A
session that dies after the script exits needs a second run.

## 2. The five steps

Each step names the firmware function it came from.

1. **Trigger** (`fetchPortalUrl`). `GET` the probe URL with redirects
   disabled. `204` means the network is open, so the script exits 0 without
   logging in. `3xx` takes the portal URL from the `Location` header. `200`
   means the request was intercepted, so the body is searched for the
   interception page's `window.location="..."` script. Any other status is
   reported as walled with nothing to follow.
2. **Login page** (`fetchLoginPage`). HTTPS `GET` of that URL with TLS
   verification off (`curl -k`), matching the firmware's `setInsecure()`.
   The page is scanned for the form `action`, the hidden `4Tredir`, and the
   hidden `magic`. A missing form action or a missing `magic` exits 1: the
   portal's markup changed, and different credentials would not help.
3. **Submit** (`postCredentials`). `POST`
   `application/x-www-form-urlencoded` with the fields in the firmware's
   order, `4Tredir`, `magic`, `username`, `password`, plus a `Referer`
   header. Values are percent-encoded by `curl --data-urlencode`, which
   matches the firmware's `urlEncode()`. The response status and body are
   logged as a hint only.
4. **Verify** (`fetchData`). `GET` the probe URL again. `204` is the verdict:
   authenticated. Retried three times, 2 s apart, because the firmware checks
   immediately after the POST and some portals need a moment.
5. **Re-ask** (no firmware counterpart). If step 4 still reports a wall, the
   portal rejected the credentials. The script asks for the username and
   password again and repeats steps 2-4. Steps 2 and 3 are repeated in full,
   not just the POST: the `magic` token and any session cookie belong to one
   page load, and a reused token would make a correct password look rejected.
   The cookie jar is emptied at the start of each attempt for the same reason.

Step 5 needs a terminal on stdin. Without one (cron, systemd, `ssh` without
`-t`, a pipeline) the script makes a single attempt and exits 1, so unattended
runs behave as they did before. `--max-attempts` caps the loop; the default is
unlimited because someone is at the keyboard to press Ctrl-C. A step-2 failure
exits 1 without re-asking: a page with no form is not a credentials problem.

Step 3's response never decides the outcome. The Unpad portal answers a
correct login with a `200` HTML status page and an empty `Location`
(`network-master.md` §5), so "no redirect" means nothing on its own.

## 3. Helper functions and their firmware counterparts

| Script | Firmware | Notes |
| --- | --- | --- |
| `probe_once` | `fetchPortalUrl` | Return value classifies the result: 0 open, 1 portal URL found, 2 walled without a URL, 3 transport failure. |
| `extract_window_location` | `extractWindowLocation` | Handles both `"` and `'` quoting. |
| `form_action` | `scanPage` form branch | Reads `action="..."` from the first `<form` onwards. |
| `value_after` | `valueAfterFixed` | First `value="..."` within 512 bytes after a `name="..."` needle. |
| `resolve_action` | `resolveAction` | Absolute, root-relative, and bare relative actions. |
| `url_base` | `urlBase` | `scheme://host[:port]`. |
| `attempt_login` | steps 2-4 of the login sequence | One full pass. Returns 0 for open, 1 for a rejected login, 2 for a login page that cannot be used. |
| `prompt_credentials` | none (the display prompts) | Fills `USER_NAME` and `PASSWORD`. Called with `force=1` after a rejection, which clears both first. |

One inherited limitation, on purpose: both the firmware and the script only
read double-quoted HTML attributes. A portal that serves `action='...'` or
`value='...'` yields nothing and the script exits 1 at step 2. Fixing it in the
script alone would make the two diverge, so both would need the change.

## 4. Differences from the firmware

- **Body size.** The firmware caps body reads at 4096 bytes and scans the
  login page through a 320-byte rolling window, because the WT32-ETH01 has no
  PSRAM. The script scans the whole page.
- **Cookies.** A cookie jar (in a `mktemp -d`, mode 600, removed on exit) is
  shared by the login page `GET`, the `POST`, and the verify. The firmware does
  not need one: the Unpad portal authenticates by MAC. A portal that hands out
  a session cookie works here.
- **Verify retries.** Three attempts, 2 s apart, instead of one immediate check.
- **Re-ask on rejection.** The firmware has no console, so a rejected login
  goes back to `NEED_CREDS` and waits for the display to supply new
  credentials. The script asks again on stdin, with no limit unless
  `--max-attempts` is given.
- **Credentials.** The firmware receives them from the display over Modbus and
  zeroes them in RAM after use. The script prompts for them, or takes
  `--user`/`--pass`, overwrites the variable on the way out, and never writes
  them to disk. A password passed as a flag is in the shell's history and in
  `ps` output for the run; the prompt avoids both.
- **No session re-auth.** See §1.
- **No ping.** `--ping` is informational. The firmware pings `1.1.1.1` in `RUN`
  but does not re-authenticate on a ping failure alone; only a walled data
  fetch does.

## 5. Testing

Checked on this host, which is not behind a portal:

- `bash -n` clean; `--help`, unknown flag, and a flag missing its value exit 2.
- A missing username on closed stdin exits 2 with a message instead of a
  silent failure.
- Against the real probe URL: `204`, so the script reports already
  authenticated and exits 0 without a POST.
- Against `https://example.com/`: `200` with no `window.location`, so the
  walled-without-a-URL path exits 1.
- Against `http://127.0.0.1:9/`: connection refused, so the transport path
  exits 3.
- `form_action`, `value_after`, `resolve_action`, `url_base` and
  `extract_window_location` checked against HTML fixtures, including the
  negative cases (no `<form>`, missing needle, absolute/root-relative/bare
  actions, single-quoted attributes).
- The `EXIT` trap leaves no temp directory behind.

The POST-and-verify path was not exercised end to end: that needs a real walled
network. When testing on one, run with `--verbose` and compare against the
firmware's serial log for the same portal.

## 6. Related documents

- `network-master.md` §5 — the firmware sequence this ports, and §4 for the
  stage machine.
- `network-master.md` §5 — the firmware login flow, plus the
  measured Unpad portal behaviour.
- `wt32-eth01-captive-recon.md` — the recon sketch that found the
  `4Tredir`/`magic` fields.
- `captive-portal-workaround.md` — why the firmware delegates the credential
  prompt to the display.
- `display-provisioning-ap.md` — the phone path that stages credentials into
  the firmware's Modbus registers.

---
Content generated by `opencode/deepseek-v4.1-flash-free` in `opencode`.
