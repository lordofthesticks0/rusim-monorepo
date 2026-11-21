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
legacy/pi/captive-portal-login.sh                    # prompts for both
legacy/pi/captive-portal-login.sh --once --quiet     # old single-pass behaviour
legacy/pi/captive-portal-login.sh --retry-delay 30 --max-loops 20
legacy/pi/captive-portal-login.sh --max-attempts 2
```

| Option | Meaning |
| --- | --- |
| `--user <name>` | Portal username. Prompted for when missing. |
| `--pass <secret>` | Portal password. Prompted for (hidden) when missing. A password given as a flag stays in shell history and is visible in `ps`; prefer the prompt. |
| `--probe-url <url>` | Connectivity probe. Default `http://connectivitycheck.gstatic.com/generate_204`. |
| `--max-attempts <n>` | Stop after `n` rejected logins instead of asking again. Default `0`, which keeps asking. Only matters when stdin is a terminal. |
| `--retry-delay <s>` | Seconds to wait before a new pass after a failure that is not a rejected login. Default `10`. |
| `--max-loops <n>` | Stop after `n` passes. Default `0`, which loops until success or Ctrl-C. |
| `--once` | One pass only; exit on the first failure. |
| `--log-file <path>` | Append every log line to this file. Default `~/captive-portal-login.log`. An empty value turns it off. |
| `--quiet`, `-q` | Leave response headers, response bodies and form inputs out of the log. |
| `--verbose`, `-v` | Accepted for compatibility. Verbose output is the default. |
| `--ping` | Ping `1.1.1.1` three times after success. Informational only; a failure does not change the exit code. |
| `-h`, `--help` | Usage. |

Exit codes:

| Code | Meaning |
| --- | --- |
| 0 | Authenticated (the probe answers 204). |
| 1 | Still walled: the login was refused, or the portal served no login URL. Only returned when the loop stops (`--once`, `--max-loops`, `--max-attempts`, or a rejection with no terminal). |
| 2 | Usage error, missing `curl`, or no credentials on stdin. |
| 3 | The probe URL itself could not be fetched (no route, DNS failure, timeout), and the loop stopped. |
| 130 | Stopped by Ctrl-C or `kill` (SIGINT/SIGTERM). |

The script is not a daemon: it does not re-check the session after it exits.
Until it succeeds, it keeps retrying (§2, step 5). A session that dies after the
script exits needs a second run.

## 1a. Diagnostic output

The script is verbose by default so a failure can be diagnosed from its output
alone.

- Every line starts with a timestamp (`YYYY-MM-DD HH:MM:SS [LOGIN] ...`).
- Every line is also appended to the log file (default
  `~/captive-portal-login.log`), so a run can be reviewed after the terminal
  closes. The file is created with mode 600 because response headers can
  contain session cookies. Once it passes 1 MiB it is moved to
  `captive-portal-login.log.1` at the next start.
- Each pass starts with the network context: interface addresses
  (`ip -br addr`), the default route, the resolvers in `/etc/resolv.conf`, and
  the addresses the probe host resolves to. The portal host's DNS answer is
  logged before the login page is fetched.
- Every HTTP request logs one summary line:
  `http=<status> curl_rc=<curl exit code> peer=<ip>:<port> dns= connect= tls= total= bytes=`.
  A non-zero `curl_rc` is followed by its meaning (for example `6 could not
  resolve host`, `7 could not connect`, `28 timed out`, `35 TLS handshake
  failed`) and curl's own error text.
- Unless `--quiet` is given, each response's headers and its first 800 bytes
  of body are logged, followed by every `<input>` tag of the login form.
- The POST line names the fields and shows the password only as its length
  (`password=<6 chars>`). The request body is never logged.
- After the POST, phrases such as `failed`, `invalid` or `denied` from the
  response body are logged, as a hint only.

How to read a failure:

| Last lines show | Likely cause |
| --- | --- |
| `trigger: http=0 curl_rc=6` | DNS is not working. Check the `resolver:` and `dns` lines. |
| `trigger: http=0 curl_rc=7` or `28` | No route or the link is down. Check the `addr:` and `route:` lines. |
| `login page: http=0 curl_rc=35` | TLS to the portal failed. |
| `form action: (no form found)` or `magic: (not found)` | The portal page changed. Compare the logged body and `form input` lines with §2 step 3b. |
| `POST looked accepted: yes`, then `still walled` three times | The portal took the POST but did not open the port. Compare the POST body with the firmware log. |
| `POST body mentions: ... invalid ...` | Wrong username or password. |

## 2. The steps

Each step names the firmware function it came from.

1. **Check** (`fetchPortalUrl`, first call). `GET` the probe URL with redirects
   disabled. `204` means the network is open, so the script exits 0 without
   asking for anything. Any other answer means the port is walled, so the
   script asks for credentials. The link in this answer is not used: it can
   change while the user types, so step 3a fetches it again.
2. **Credentials** (no firmware counterpart; the display supplies them there).
   The username and password are asked for, or taken from `--user`/`--pass`.
   Nothing is fetched while the prompt waits.
3. **Fetch and submit** (`fetchPortalUrl`, `fetchLoginPage`, `postCredentials`,
   `fetchData`). Steps 3a-3d run back to back, with no pause between them:
   - **a. Link.** `GET` the probe URL again with redirects disabled. `204`
     means the port opened, so the script exits 0 without posting. `3xx`
     takes the portal URL from the `Location` header. `200` means the request
     was intercepted, so the body is searched for the interception page's
     `window.location="..."` script. Any other status is reported as walled
     with nothing to follow, and the pass fails.
   - **b. Login page** (`fetchLoginPage`). HTTPS `GET` of that URL with TLS
     verification off (`curl -k`), matching the firmware's `setInsecure()`.
     The page is scanned for the form `action`, the hidden `4Tredir`, and the
     hidden `magic`. A missing form action or a missing `magic` fails the
     pass: the portal's markup changed, and different credentials would not
     help.
   - **c. Submit** (`postCredentials`). `POST`
     `application/x-www-form-urlencoded` with the fields in the firmware's
     order, `4Tredir`, `magic`, `username`, `password`, plus a `Referer`
     header. Values are percent-encoded by `curl --data-urlencode`, which
     matches the firmware's `urlEncode()`. The response status and body are
     logged as a hint only.
   - **d. Verify** (`probeOnline`). `GET` the probe URL again. `204` is the
     verdict: authenticated. Retried three times, 2 s apart, because the
     firmware checks immediately after the POST and some portals need a
     moment.
4. **Re-ask** (no firmware counterpart). If step 3d still reports a wall, the
   portal rejected the credentials. The script repeats steps 1-3 in full: the
   check, a new username and password, then a fresh link, login page, POST and
   verify. Nothing from the earlier attempt is reused. The `magic` token and any
   session cookie belong to one page load, and a reused token would make a
   correct password look rejected. The cookie jar is emptied at the start of
   each attempt for the same reason.
5. **Retry** (no firmware counterpart). Any other failure (the check or the
   link cannot be fetched, the link is walled but gives no login URL, the login
   page cannot be used, or the network drops during steps 3a-3d) is logged as
   `FAILED (pass N): <reason>` and the whole sequence restarts from step 1
   after `--retry-delay` seconds. The username and password are kept in memory
   between passes, so a retry does not ask again. Ctrl-C stops the loop at
   any point; the wait between passes is interruptible, so `kill` also stops
   it at once.

Step 4 needs a terminal on stdin. Without one (cron, systemd, `ssh` without
`-t`, a pipeline) a rejected login exits 1 instead of posting the same
credentials again, because repeated wrong logins could lock the portal
account. `--max-attempts` caps the number of rejections; the default is
unlimited because someone is at the keyboard to press Ctrl-C. A rejection
re-prompts at once without waiting `--retry-delay`.

`--once` turns step 5 off and gives the old single-pass behaviour.
`--max-loops` caps the number of passes.

Step 3c's response never decides the outcome. The Unpad portal answers a
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
| `attempt_login` | steps 3a-3d of the login sequence | One fetch-and-submit run, started after the credentials are in. Returns 0 for open, 1 for a rejected login, 2 for a link or login page that cannot be used, 3 when the probe stops answering. |
| `prompt_credentials` | none (the display prompts) | Fills `USER_NAME` and `PASSWORD`, asking only for missing ones. Called with `force=1` after a rejection, which clears both first. |
| `do_curl` | none | Runs one request with the shared header, body and cookie files. Sets `HTTP_CODE` and `CURL_RC` and writes the summary, error, headers and body head to the log. |
| `log_network` | none | Logs addresses, default route, resolvers and the probe host's DNS answer. |
| `log`, `log_lines` | `Serial.printf("[LOGIN] ...")` | Timestamped line to the terminal and the log file. |
| `pause` | none | A sleep that SIGINT/SIGTERM can cut short (background `sleep` plus `wait`). |

One inherited limitation, on purpose: both the firmware and the script only
read double-quoted HTML attributes. A portal that serves `action='...'` or
`value='...'` yields nothing and the script fails at step 3b. Fixing it in the
script alone would make the two diverge, so both would need the change.

## 4. Differences from the firmware

- **Body size.** The firmware caps body reads at 4096 bytes and scans the
  login page through a 320-byte rolling window, because the WT32-ETH01 has no
  PSRAM. The script scans the whole page.
- **Cookies.** A cookie jar (in a `mktemp -d`, mode 600, removed on exit) is
  shared by the fresh link `GET`, the login page `GET`, the `POST`, and the
  verify. It is emptied at the start of each attempt. The firmware does not
  need one: the Unpad portal authenticates by MAC. A portal that hands out a
  session cookie works here.
- **Link freshness.** The firmware fetches the link straight before the login
  page, with no user input in between. The script asks for credentials first,
  so it fetches the link again after the prompt: the link found by the check
  can change while the user types. Step 3a takes the link that the POST uses.
- **Verify retries.** Three attempts, 2 s apart, instead of one immediate check.
- **Re-ask on rejection.** The firmware has no console, so a rejected login
  goes back to `NEED_CREDS` and waits for the display to supply new
  credentials. The script asks again on stdin, with no limit unless
  `--max-attempts` is given.
- **Retry loop.** The firmware's stage machine retries on its own schedule.
  The script retries the whole sequence every `--retry-delay` seconds until it
  succeeds or is stopped (§2, step 5).
- **Credentials.** The firmware receives them from the display over Modbus and
  zeroes them in RAM after use. The script prompts for them, or takes
  `--user`/`--pass`, keeps them in memory across retry passes, overwrites the
  variable on the way out, and never writes them to disk or to the log. A password passed as a flag is in the shell's history and in
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

Retry loop and logging, checked 2026-11-21 on the development host:

- Bad `--retry-delay` and missing option values exit 2.
- Probe on a closed port: two passes with `--max-loops 2`, `curl_rc=7`
  logged with its meaning; `--once` exits 3.
- `https://example.com/` with `--once`: no login URL, exits 1.
- A local mock portal serving the Unpad interception page, a form with
  `4Tredir`/`magic`, and a 204 probe only after a correct POST:
  - correct credentials: login page, POST, verify 204, exit 0;
  - wrong credentials with no terminal: one POST, then exit 1 with no re-post;
  - wrong credentials on a pseudo-terminal: re-prompt, then success on pass 2;
    with `--max-attempts 1`, exit 1 after the first rejection.
- SIGTERM during a 30 s retry wait stops the script at once with exit 130.
- The log file is mode 600 and does not contain the password.

Credentials-first flow, checked 2026-11-21 against a local mock portal. The
mock is not part of the repository. It hands out a new session for each link,
expires a session after a set number of seconds, and requires the session
cookie on the POST:

- correct credentials, no delay: check, link, login page, POST, verify, exit 0;
- password typed 5 s after the check, sessions expiring after 3 s: the fresh
  link after the prompt is used, so the login succeeds and exits 0. The first
  link, taken before the prompt, is never used;
- wrong credentials with no terminal: one POST, then exit 1 with no re-post;
- wrong password on a pseudo-terminal, then the right one: the check runs again
  before the re-prompt, and the second pass succeeds;
- already open: exit 0 at the check, with no prompt;
- probe port closed: exit 3 at the check, with no prompt.

On the walled Pi (2026-11-21, with the previous version of the script), the
probe returned `200` with
`window.location="https://pintas.unpad.ac.id:1003/fgtauth?..."`, so step 1
works there. The steps after that were not run because no credentials were
given.

The POST-and-verify path has not been run end to end against the real portal:
that needs a real walled network and valid credentials. When testing on one, run with `--verbose` and compare against the
firmware's serial log for the same portal.

## 6. Deployment on the legacy Pi

The legacy Pi (`invitro@10.92.128.23`) keeps two identical copies in its home
directory: `~/captive-portal-login.sh` and `~/captive_portal.sh`. `~/.bashrc`
runs `~/captive_portal.sh` near its end. The top of `~/.bashrc` returns early
for non-interactive shells, so the script runs only when an interactive shell
starts: an SSH login or a terminal window. The kiosk (`~/.config/openbox/autostart`)
does not run it.

Because the script now loops until it succeeds, an interactive shell on a
walled or offline Pi waits in the script until the login works or Ctrl-C is
pressed. Ctrl-C returns to the normal shell prompt. The log of every run is in
`~/captive-portal-login.log` on the Pi.

## 7. Related documents

- `network-master.md` §5 — the firmware sequence this ports, and §4 for the
  stage machine.
- `network-master.md` §5 — the firmware login flow, plus the
  measured Unpad portal behaviour.
- `wt32-eth01-captive-recon.md` — the recon sketch that found the
  `4Tredir`/`magic` fields.
- `plans-and-audits/captive-portal-workaround.md` — why the firmware delegates the credential
  prompt to the display.
- `display-provisioning-ap.md` — the phone path that stages credentials into
  the firmware's Modbus registers.

---
Updated at 2026-11-21-12:22.
