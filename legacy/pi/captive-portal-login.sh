#!/usr/bin/env bash
#
# captive-portal-login.sh: log in to a walled network's captive portal from a
# Linux host, using the same steps as firmware/network-master/main.cpp.
#
# The firmware sequence, and where each step lives here:
#   1. check             GET http://connectivitycheck.gstatic.com/generate_204
#                        with redirects off (probe_once). 204 -> no portal,
#                        network is open, done. Anything else -> walled.
#   2. prompt            Username and password are asked for (or taken from
#                        --user/--pass). The firmware has no equivalent: it gets
#                        them from the display.
#   3. fetch and push    Starts at once, with no pause after the prompt:
#        a. fetchPortalUrl()  GET the probe URL again. The portal link found in
#                        step 1 can change while the user types, so the link
#                        is taken fresh. 204 -> authenticated, nothing to post.
#                        3xx -> portal URL from the Location header.
#                        200 -> portal URL from the interception page's
#                        `window.location="..."` script.
#        b. fetchLoginPage()  HTTPS GET of that URL (TLS verification off, same
#                        as the firmware's setInsecure()), scan for the form
#                        `action`, the hidden `4Tredir`, and the hidden `magic`.
#        c. postCredentials() POST application/x-www-form-urlencoded
#                        4Tredir=...&magic=...&username=...&password=... to the
#                        resolved form action, with Content-Type and Referer.
#        d. fetchData()  GET the probe URL again. 204 means the port is open.
#                        That verdict is final; the POST response is only logged.
#   4. re-ask            When step 3d still reports a wall, the portal rejected
#                        the credentials. Steps 1-3 run again: the check, new
#                        credentials, then a fresh link, login page (new magic
#                        token, new session cookie) and POST.
#
# Diagnosis mode (default):
#   - Every line is timestamped and also appended to a log file
#     (default ~/captive-portal-login.log, mode 600). Every HTTP exchange logs
#     its status, curl exit code, peer address, timings, response headers and
#     the head of the response body. Each pass logs the host's addresses,
#     default route, resolvers and the DNS answers for the probe and portal.
#   - Any failure (probe unreachable, no login URL, unusable login page,
#     network dropped during verification) is logged and the whole sequence
#     restarts after --retry-delay seconds. Ctrl-C stops the loop.
#   - --once restores single-pass behaviour; --max-loops caps the passes.
#
# The request body of the POST (which holds the password) is never logged.
#
# Re-asking after a rejection needs a terminal on stdin. Without one (cron,
# systemd, ssh without -t, a pipeline) a rejected login exits 1 instead of
# re-posting the same credentials, which could lock the portal account.
#
# Differences from the firmware, all deliberate:
#   - The firmware caps body reads at 4096 bytes and scans the login page
#     through a 320-byte rolling window because the WT32-ETH01 has no PSRAM.
#     Here the whole page is scanned.
#   - A cookie jar is kept across the fresh link GET, the login page GET, the
#     POST and the verify, so a portal that hands out a session cookie works.
#     The firmware does not need one (the Unpad portal authenticates by MAC).
#   - The portal link is fetched again after the credentials are typed, and the
#     login page, POST and verify follow with no pause in between. The firmware
#     never waits for input, so it has no gap in which the link could change.
#   - Verification retries three times with a 2 s gap, because the firmware
#     checks immediately after the POST.
#   - Credentials are asked for on a hidden prompt unless --user/--pass are
#     given. They live in shell variables for the length of the run and are
#     overwritten on exit. Nothing is written to disk.
#   - A rejected login re-prompts instead of exiting. The firmware has no
#     console to prompt on and gives up after one POST.
#
# Exit codes: 0 authenticated, 1 still walled, 2 usage or missing dependency,
# 3 the probe could not be fetched at all, 130 stopped with Ctrl-C.
#
# Usage:
#   legacy/pi/captive-portal-login.sh --user alice
#   legacy/pi/captive-portal-login.sh            # prompts for both
#   legacy/pi/captive-portal-login.sh --once --quiet
#   legacy/pi/captive-portal-login.sh --retry-delay 30 --max-loops 20
#
set -euo pipefail

PROBE_URL_DEFAULT="http://connectivitycheck.gstatic.com/generate_204"
LOG_FILE_DEFAULT="${HOME:-/tmp}/captive-portal-login.log"
USER_NAME=""
PASSWORD=""
PROBE_URL="$PROBE_URL_DEFAULT"
LOG_FILE="$LOG_FILE_DEFAULT"
LOG_MAX_BYTES=1048576
DO_PING=0
VERBOSE=1
VERIFY_ATTEMPTS=3
VERIFY_DELAY=2
BODY_HEAD=800
MAX_ATTEMPTS=0
RETRY_DELAY=1
MAX_LOOPS=0
ONCE=0

usage() {
  cat <<EOF
Log in to a captive portal and verify the port is open (Linux/cURL port of the
network-master firmware login sequence). Retries on failure until it succeeds
or is stopped with Ctrl-C.

Usage:
  captive-portal-login.sh [options]

Options:
  --user <name>       Portal username. Prompted for when missing.
  --pass <secret>     Portal password. Prompted for (hidden) when missing.
                      Passing it on the command line leaves it in shell
                      history and in \`ps\`; prefer the prompt.
  --probe-url <url>   Connectivity probe (default: $PROBE_URL_DEFAULT).
  --max-attempts <n>  Stop after n rejected logins instead of asking again.
                      Default 0, which keeps asking. Only matters when stdin
                      is a terminal.
  --retry-delay <s>   Seconds to wait before retrying after a failure that is
                      not a rejected login. Default $RETRY_DELAY.
  --max-loops <n>     Stop after n passes. Default 0, which loops until
                      success or Ctrl-C.
  --once              One pass only; exit on the first failure.
  --log-file <path>   Append every log line here. Default
                      $LOG_FILE_DEFAULT. An empty value turns it off.
  --quiet             Drop response headers and bodies from the log.
  --verbose, -v       Accepted for compatibility; verbose is the default.
  --ping              Ping 1.1.1.1 three times after success. Informational;
                      a failure does not change the exit code.
  -h, --help          Show this help.

Exit codes: 0 authenticated, 1 still walled, 2 usage/dependency, 3 probe
failed, 130 stopped with Ctrl-C.
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --user|--pass|--probe-url|--max-attempts|--retry-delay|--max-loops|--log-file)
      if [ $# -lt 2 ]; then
        printf '%s needs a value\n' "$1" >&2
        exit 2
      fi
      case "$1" in
        --user) USER_NAME="$2" ;;
        --pass) PASSWORD="$2" ;;
        --probe-url) PROBE_URL="$2" ;;
        --max-attempts) MAX_ATTEMPTS="$2" ;;
        --retry-delay) RETRY_DELAY="$2" ;;
        --max-loops) MAX_LOOPS="$2" ;;
        --log-file) LOG_FILE="$2" ;;
      esac
      shift 2
      ;;
    --once) ONCE=1; shift ;;
    --quiet|-q) VERBOSE=0; shift ;;
    --ping) DO_PING=1; shift ;;
    --verbose|-v) VERBOSE=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) printf 'unknown option: %s\n\n' "$1" >&2; usage >&2; exit 2 ;;
  esac
done

command -v curl >/dev/null 2>&1 || { echo "curl is required" >&2; exit 2; }

for pair in "max-attempts:$MAX_ATTEMPTS" "retry-delay:$RETRY_DELAY" "max-loops:$MAX_LOOPS"; do
  case "${pair#*:}" in
    ''|*[!0-9]*) printf '%s must be a non-negative integer\n' "${pair%%:*}" >&2; exit 2 ;;
  esac
done

# --- logging -------------------------------------------------------------
#
# The log file keeps a record after the terminal is closed. It is rotated to
# <file>.1 once it passes LOG_MAX_BYTES, and created mode 600 because response
# headers can carry session cookies.
if [ -n "$LOG_FILE" ]; then
  if [ -f "$LOG_FILE" ] && [ "$(stat -c %s "$LOG_FILE" 2>/dev/null || echo 0)" -gt "$LOG_MAX_BYTES" ]; then
    mv -f "$LOG_FILE" "$LOG_FILE.1" 2>/dev/null || true
  fi
  if ! ( umask 077; : >>"$LOG_FILE" ) 2>/dev/null; then
    printf 'cannot write log file %s; logging to the terminal only\n' "$LOG_FILE" >&2
    LOG_FILE=""
  fi
fi

log() {
  local ts line
  printf -v ts '%(%F %T)T' -1
  line="$ts [LOGIN] $*"
  printf '%s\n' "$line"
  if [ -n "$LOG_FILE" ]; then
    printf '%s\n' "$line" >>"$LOG_FILE" 2>/dev/null || true
  fi
}

# log_lines <prefix> <file>: every line of a file through log(), CR stripped.
log_lines() {
  local prefix="$1" file="$2" line
  [ -s "$file" ] || return 0
  while IFS= read -r line || [ -n "$line" ]; do
    log "  $prefix: ${line%$'\r'}"
  done <"$file"
}

TMP="$(mktemp -d)" || { echo "mktemp failed" >&2; exit 2; }
TMP_HDR="$TMP/headers"
TMP_BODY="$TMP/body"
TMP_JAR="$TMP/cookies"

cleanup() {
  # Overwrite the password in this shell's memory before the script leaves.
  # Other processes that read the environment are out of reach of this.
  PASSWORD='xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'
  PASSWORD=''
  unset PASSWORD || true
  if [ -e "$TMP_JAR" ]; then
    chmod 600 "$TMP_JAR" 2>/dev/null || true
    rm -f "$TMP_JAR" 2>/dev/null || true
  fi
  if [ -n "${TMP:-}" ] && [ "$TMP" != "/" ] && [ "$TMP" != "." ] && [ -d "$TMP" ]; then
    rm -rf -- "$TMP"
  fi
}
trap cleanup EXIT

# pause <seconds>: sleep that a signal can cut short. Bash runs a trap only
# after a foreground child exits, so a plain `sleep 30` would hold off a
# `kill` for up to 30 s. `wait` on a background sleep returns at once.
SLEEP_PID=""
pause() {
  sleep "$1" &
  SLEEP_PID=$!
  wait "$SLEEP_PID" || true
  SLEEP_PID=""
}

on_interrupt() {
  if [ -n "$SLEEP_PID" ]; then kill "$SLEEP_PID" 2>/dev/null || true; fi
  # A Ctrl-C during the hidden password prompt can leave echo off.
  if [ -t 0 ]; then stty echo 2>/dev/null || true; fi
  printf '\n'
  log "Stopped by a signal (Ctrl-C or kill)."
  exit 130
}
trap on_interrupt INT TERM

# --- network context -----------------------------------------------------

# Host part of a URL ("scheme://host[:port]/..." -> "host").
url_host() {
  local rest="${1#*://}"
  rest="${rest%%/*}"
  printf '%s' "${rest%%:*}"
}

# The addresses a name resolves to, space separated, or "(no answer)".
resolve() {
  local answer
  answer="$(getent ahosts "$1" 2>/dev/null | awk '{ print $1 }' | sort -u | tr '\n' ' ' || true)"
  printf '%s' "${answer:-(no answer)}"
}

log_network() {
  local line host
  log "network context:"
  if command -v ip >/dev/null 2>&1; then
    while IFS= read -r line; do
      log "  addr: $line"
    done < <(ip -br addr 2>/dev/null | grep -v '^lo ' || true)
    while IFS= read -r line; do
      log "  route: $line"
    done < <(ip route show default 2>/dev/null || true)
    [ -n "$(ip route show default 2>/dev/null || true)" ] || log "  route: (no default route)"
  fi
  while IFS= read -r line; do
    log "  resolver: $line"
  done < <(grep '^nameserver' /etc/resolv.conf 2>/dev/null || true)
  host="$(url_host "$PROBE_URL")"
  log "  dns $host: $(resolve "$host")"
}

# --- HTTP ----------------------------------------------------------------

curl_rc_meaning() {
  case "$1" in
    0) echo "ok" ;;
    3) echo "malformed URL" ;;
    6) echo "could not resolve host" ;;
    7) echo "could not connect" ;;
    28) echo "timed out" ;;
    35) echo "TLS handshake failed" ;;
    47) echo "too many redirects" ;;
    52) echo "server sent nothing" ;;
    56) echo "connection reset while receiving" ;;
    60) echo "certificate problem" ;;
    *) echo "see 'man curl', EXIT CODES" ;;
  esac
}

# do_curl <label> <extra curl args...>
#
# One request with the shared header, body and cookie files. Sets HTTP_CODE
# (0 when no response arrived) and CURL_RC, and logs the status, peer, timings,
# any transport error, and (unless --quiet) the headers and body head.
HTTP_CODE=0
CURL_RC=0
do_curl() {
  local label="$1"
  shift
  : >"$TMP_HDR"
  : >"$TMP_BODY"
  : >"$TMP/err"
  CURL_RC=0
  curl -sS -c "$TMP_JAR" -b "$TMP_JAR" -o "$TMP_BODY" -D "$TMP_HDR" \
    -w '%{http_code}|%{remote_ip}|%{remote_port}|%{time_namelookup}|%{time_connect}|%{time_appconnect}|%{time_total}|%{size_download}' \
    "$@" >"$TMP/wout" 2>"$TMP/err" || CURL_RC=$?

  local code="" ip="" port="" t_dns="" t_conn="" t_tls="" t_total="" size=""
  IFS='|' read -r code ip port t_dns t_conn t_tls t_total size <"$TMP/wout" || true
  HTTP_CODE="$(printf '%s' "$code" | tr -cd '0-9')"
  HTTP_CODE="${HTTP_CODE:-0}"
  HTTP_CODE=$((10#$HTTP_CODE))

  log "$label: http=$HTTP_CODE curl_rc=$CURL_RC peer=${ip:-none}:${port:-0} dns=${t_dns:-?}s connect=${t_conn:-?}s tls=${t_tls:-?}s total=${t_total:-?}s bytes=${size:-0}"
  if [ "$CURL_RC" != 0 ]; then
    log "$label: curl error $CURL_RC ($(curl_rc_meaning "$CURL_RC")): $(tr -d '\n' <"$TMP/err")"
  fi
  if [ "$VERBOSE" = 1 ]; then
    log_lines "$label hdr" "$TMP_HDR"
    head -c "$BODY_HEAD" "$TMP_BODY" >"$TMP/bodyhead" || true
    log_lines "$label body" "$TMP/bodyhead"
    if [ "$(stat -c %s "$TMP_BODY" 2>/dev/null || echo 0)" -gt "$BODY_HEAD" ]; then
      log "  $label body: ... (truncated at $BODY_HEAD bytes)"
    fi
  fi
}

# --- probe ---------------------------------------------------------------
#
# One GET of the probe URL with redirects disabled. Sets PROBE_STATUS and,
# on a redirect, PORTAL_URL. Return value is the classification the firmware
# gets from fetchPortalUrl():
#   0  204, network is open (no interception)
#   1  a portal URL was recovered (3xx Location or a JS redirect)
#   2  walled but no portal URL to follow (unexpected status, bare 3xx)
#   3  transport failure
#
PORTAL_URL=""
PROBE_STATUS=""
probe_once() {
  local label="$1"
  log "GET trigger $PROBE_URL ($label)"
  do_curl "trigger" --connect-timeout 5 --max-time 10 "$PROBE_URL"
  if [ "$CURL_RC" != 0 ]; then
    return 3
  fi
  PROBE_STATUS="$HTTP_CODE"

  case "$HTTP_CODE" in
    204)
      log "portal URL: (none, 204 online)"
      return 0
      ;;
    301|302|303|307|308)
      # The portal URL is the last Location header; -D can emit more than one
      # block when the probe URL was itself redirected by something else.
      PORTAL_URL="$(sed 's/\r$//' "$TMP_HDR" \
        | awk 'tolower($1) == "location:" { sub(/^[^:]*:[ \t]*/, ""); v = $0 } END { print v }')"
      log "portal URL: ${PORTAL_URL:-(redirect without Location)}"
      if [ -n "$PORTAL_URL" ]; then return 1; fi
      return 2
      ;;
    200)
      PORTAL_URL="$(extract_window_location "$TMP_BODY")"
      log "portal URL: ${PORTAL_URL:-(200 without redirect URL)}"
      if [ -n "$PORTAL_URL" ]; then return 1; fi
      return 2
      ;;
    *)
      log "unexpected trigger status $HTTP_CODE (treating as walled)"
      return 2
      ;;
  esac
}

# The interception page is a JS redirect, not an HTTP one:
#   <script language="JavaScript">window.location="URL";</script>
extract_window_location() {
  local file="$1"
  local url
  url="$(sed -n 's/.*window\.location[[:space:]]*=[[:space:]]*"\([^"]*\)".*/\1/p' "$file" | head -n 1)"
  if [ -z "$url" ]; then
    url="$(sed -n "s/.*window\\.location[[:space:]]*=[[:space:]]*'\\([^']*\\)'.*/\1/p" "$file" | head -n 1)"
  fi
  printf '%s' "$url"
}

# --- login page scan -----------------------------------------------------

# First value="..." at or after <needle>, mirroring valueAfterFixed(). The
# firmware gives up when the value sits more than 512 bytes past the needle.
value_after() {
  local needle="$1" file="$2"
  awk -v needle="$needle" '
    { doc = doc $0 "\n" }
    END {
      at = index(doc, needle)
      if (at == 0) exit
      rest = substr(doc, at + length(needle))
      v = index(rest, "value=\"")
      if (v == 0 || v > 512) exit
      tail = substr(doc, at + length(needle) + v + 6)
      e = index(tail, "\"")
      if (e <= 1) exit
      print substr(tail, 1, e - 1)
    }
  ' "$file"
}

# The form action, searched from the first <form onwards, mirroring scanPage().
form_action() {
  local file="$1"
  awk '
    { doc = doc $0 "\n" }
    END {
      f = index(doc, "<form")
      if (f == 0) exit
      k = index(substr(doc, f), "action=\"")
      if (k == 0) exit
      tail = substr(doc, f + k + 7)
      e = index(tail, "\"")
      if (e <= 1) exit
      print substr(tail, 1, e - 1)
    }
  ' "$file"
}

# --- form action resolution ---------------------------------------------

# "scheme://host[:port]" of a URL, mirroring urlBase().
url_base() {
  local url="$1"
  case "$url" in
    *://*) ;;
    *) return 1 ;;
  esac
  local rest="${url#*://}"
  printf '%s://%s' "${url%%://*}" "${rest%%/*}"
}

# Resolves the form action against the page URL: absolute URLs, root-relative
# paths, and bare relative paths, mirroring resolveAction().
resolve_action() {
  local page_url="$1" action="$2"
  case "$action" in
    http://*|https://*)
      printf '%s' "$action"
      return 0
      ;;
  esac
  local base
  base="$(url_base "$page_url")" || { printf '%s' "$action"; return 0; }
  case "$action" in
    /*)
      printf '%s%s' "$base" "$action"
      return 0
      ;;
  esac
  local no_query="$page_url"
  case "$no_query" in
    *\?*) no_query="${no_query%%\?*}" ;;
  esac
  printf '%s%s' "${no_query%/*}/" "$action"
}

# --- credentials ---------------------------------------------------------

# Re-asking needs somewhere to read the answer from. A terminal on stdin is
# the only case where that is true.
CAN_REASK=0
if [ -t 0 ]; then CAN_REASK=1; fi

# prompt_credentials <force>
#
# Fills USER_NAME and PASSWORD. Values already held (from --user/--pass or an
# earlier pass) are kept and only missing ones are asked for. force=1 clears
# both first: a rejected login has to be answered with different values,
# because the same ones would just be rejected again.
prompt_credentials() {
  local force="${1:-0}"

  if [ "$force" = 1 ]; then
    USER_NAME=""
    PASSWORD=""
  fi

  # An empty answer on the first attempt is fatal, as it always was. After a
  # rejection it is a slip, so ask again instead.
  if [ -z "${USER_NAME:-}" ]; then
    while :; do
      printf 'Portal username: '
      if ! read -r USER_NAME; then
        printf '\n'
        log "FAILED: no username on stdin; pass --user instead."
        exit 2
      fi
      if [ -n "$USER_NAME" ]; then
        break
      fi
      if [ "$force" != 1 ]; then
        log "FAILED: username and password are required."
        exit 2
      fi
      printf 'username cannot be empty\n' >&2
    done
  fi

  if [ -z "${PASSWORD:-}" ]; then
    while :; do
      if ! read -r -s -p "Portal password: " PASSWORD; then
        printf '\n'
        log "FAILED: no password on stdin; pass --pass instead."
        exit 2
      fi
      printf '\n'
      if [ -n "$PASSWORD" ]; then
        break
      fi
      if [ "$force" != 1 ]; then
        log "FAILED: username and password are required."
        exit 2
      fi
      printf 'password cannot be empty\n' >&2
    done
  fi
}

# --- login ---------------------------------------------------------------

# attempt_login: steps 3a-3d, run back to back once the credentials are in.
#
#   0  the port is open: either the fresh link check already returned 204, or
#      verification did
#   1  the POST went through and the port is still walled, so the portal
#      rejected the credentials
#   2  the login page could not be used (not 200, no form action, no magic
#      token), or the fresh link check gave no login URL, so other credentials
#      would not change the outcome
#   3  the probe stopped answering (on the fresh link check or during
#      verification): the network dropped, and a new password would not be
#      tried against anything
#
# The link, the login page and the magic token are fetched on every call. They
# belong to one page load: reusing them would make a good password look
# rejected, and a link fetched before the prompt may already be stale.
attempt_login() {
  # The cookie jar is emptied first, so the link GET, login page GET, POST and
  # verify share one session and nothing is carried over from an earlier pass.
  : >"$TMP_JAR"

  # --- step 3a: fresh portal link ----------------------------------------
  local fresh_rc=0
  probe_once "fresh link" || fresh_rc=$?
  case "$fresh_rc" in
    0)
      log "Port opened before the POST (fresh link check returned 204)."
      return 0
      ;;
    1) ;;
    2)
      log "Fresh link check: walled, but no login URL to follow."
      return 2
      ;;
    3)
      log "Fresh link check: the probe URL could not be fetched."
      return 3
      ;;
  esac

  # --- step 3b: login page -----------------------------------------------
  local portal_host
  portal_host="$(url_host "$PORTAL_URL")"
  log "dns $portal_host: $(resolve "$portal_host")"
  log "GET login page $PORTAL_URL"
  do_curl "login page" -k --connect-timeout 5 --max-time 15 "$PORTAL_URL"
  if [ "$HTTP_CODE" != "200" ]; then
    log "Could not read the login page (status $HTTP_CODE)."
    return 2
  fi

  local action redir magic has_user_field has_pass_field
  action="$(form_action "$TMP_BODY")"
  redir="$(value_after 'name="4Tredir"' "$TMP_BODY")"
  magic="$(value_after 'name="magic"' "$TMP_BODY")"
  has_user_field=no
  has_pass_field=no
  if grep -q 'name="username"' "$TMP_BODY"; then has_user_field=yes; fi
  if grep -q 'name="password"' "$TMP_BODY"; then has_pass_field=yes; fi

  log "form action: ${action:-(no form found)}"
  log "4Tredir: ${redir:-(not found)}"
  log "magic: ${magic:-(not found)}"
  log "username field: $has_user_field, password field: $has_pass_field"
  if [ "$VERBOSE" = 1 ]; then
    grep -o '<input[^>]*>' "$TMP_BODY" >"$TMP/inputs" 2>/dev/null || true
    log_lines "form input" "$TMP/inputs"
  fi

  if [ -z "$action" ] || [ -z "$magic" ]; then
    log "Login page has no form action or no magic token (portal changed?)."
    return 2
  fi

  # --- step 3c: POST ----------------------------------------------------
  local post_url accepted
  post_url="$(resolve_action "$PORTAL_URL" "$action")"
  log "POST $post_url (fields: 4Tredir, magic, username='$USER_NAME', password=<${#PASSWORD} chars>)"
  do_curl "POST" -k --connect-timeout 5 --max-time 20 \
    -H 'Content-Type: application/x-www-form-urlencoded' \
    -H "Referer: $PORTAL_URL" \
    --data-urlencode "4Tredir=$redir" \
    --data-urlencode "magic=$magic" \
    --data-urlencode "username=$USER_NAME" \
    --data-urlencode "password=$PASSWORD" \
    "$post_url"

  # The firmware logs acceptance here but lets the verifier decide. Keep the
  # log line, not the verdict: the Unpad portal answers a good login with a
  # 200 HTML status page and no Location header.
  accepted=no
  case "$HTTP_CODE" in
    301|302|303|307|308) accepted=yes ;;
  esac
  if grep -qi 'success' "$TMP_BODY"; then accepted=yes; fi
  if [ -n "$redir" ] && grep -qF "$redir" "$TMP_BODY"; then accepted=yes; fi
  log "POST looked accepted: $accepted (verdict still comes from the probe)"
  if grep -qiE 'fail|invalid|incorrect|denied|error' "$TMP_BODY"; then
    log "POST body mentions: $(grep -oiE '[^<>]{0,60}(fail|invalid|incorrect|denied|error)[^<>]{0,60}' "$TMP_BODY" | head -n 3 | tr '\n' ' ')"
  fi

  # --- step 3d: verify --------------------------------------------------
  log "Verifying with a real fetch..."
  local try probe_rc saw_answer
  saw_answer=0
  for try in $(seq 1 "$VERIFY_ATTEMPTS"); do
    if [ "$try" -gt 1 ]; then
      log "probe attempt $try of $VERIFY_ATTEMPTS in ${VERIFY_DELAY}s"
      pause "$VERIFY_DELAY"
    fi
    probe_rc=0
    probe_once "verify $try/$VERIFY_ATTEMPTS" || probe_rc=$?
    if [ "$probe_rc" = 0 ]; then
      return 0
    fi
    if [ "$probe_rc" = 3 ]; then
      log "probe attempt $try of $VERIFY_ATTEMPTS: the probe could not be fetched"
    else
      saw_answer=1
      log "still walled after $try attempt(s) (probe classification $probe_rc)"
    fi
  done
  if [ "$saw_answer" = 0 ]; then
    return 3
  fi
  return 1
}

# --- main loop -----------------------------------------------------------
#
# Each pass is: check (204 or walled), then prompt for credentials, then fetch
# the link, login page and POST in one go (attempt_login). A failure is logged
# with its reason and the pass repeats after RETRY_DELAY seconds, unless --once
# or --max-loops says to stop. A rejected login with a terminal attached
# re-prompts at once instead of waiting.
log "===== captive-portal-login start (pid $$, log ${LOG_FILE:-off}) ====="
log "probe=$PROBE_URL retry_delay=${RETRY_DELAY}s max_loops=$MAX_LOOPS once=$ONCE tty=$CAN_REASK verbose=$VERBOSE"

pass=0
rejections=0
force_prompt=0
rc=1
while :; do
  pass=$((pass + 1))
  log "----- pass $pass$( [ "$ONCE" = 1 ] || printf ' (Ctrl-C to stop)' ) -----"
  log_network

  # Step 1: check. Only the verdict matters here. The portal link this returns
  # is not used, because it is fetched again after the credentials are in.
  probe_rc=0
  probe_once "check" || probe_rc=$?
  attempt_rc=0
  reason=""
  fail_rc=1
  case "$probe_rc" in
    0)
      log "Already authenticated, skipping login."
      rc=0
      ;;
    2)
      reason="walled, but the portal served no login URL to follow"
      ;;
    3)
      reason="the probe URL could not be fetched"
      fail_rc=3
      ;;
    1)
      # Step 2: credentials. Nothing is fetched while this waits for input.
      prompt_credentials "$force_prompt"
      force_prompt=0
      log "user='$USER_NAME' (password held in memory only)"

      # Step 3: fresh link, login page and POST, back to back.
      attempt_login || attempt_rc=$?
      case "$attempt_rc" in
        0) rc=0 ;;
        1)
          rejections=$((rejections + 1))
          reason="still walled after the POST: the portal rejected the credentials (rejection $rejections)"
          ;;
        2) reason="the portal login page could not be used" ;;
        3) reason="still walled, and the probe URL stopped answering (network dropped?)" ;;
      esac
      ;;
  esac

  if [ "$rc" = 0 ]; then
    log "SUCCESS: port is authenticated (pass $pass)."
    break
  fi

  log "FAILED (pass $pass): $reason."

  if [ "$attempt_rc" = 1 ]; then
    # A rejection. Re-posting the same credentials unattended could lock the
    # account, so without a terminal this ends the run.
    if [ "$CAN_REASK" != 1 ]; then
      log "No terminal to ask for new credentials on; stopping."
      exit 1
    fi
    if [ "$MAX_ATTEMPTS" -gt 0 ] && [ "$rejections" -ge "$MAX_ATTEMPTS" ]; then
      log "Stopping after $rejections rejected login(s) (--max-attempts $MAX_ATTEMPTS)."
      exit 1
    fi
  fi

  if [ "$ONCE" = 1 ]; then
    exit "$fail_rc"
  fi
  if [ "$MAX_LOOPS" -gt 0 ] && [ "$pass" -ge "$MAX_LOOPS" ]; then
    log "Stopping after $pass pass(es) (--max-loops $MAX_LOOPS)."
    exit "$fail_rc"
  fi

  if [ "$attempt_rc" = 1 ]; then
    log "Asking for new credentials."
    force_prompt=1
  else
    log "Retrying in ${RETRY_DELAY}s (Ctrl-C to stop)."
    pause "$RETRY_DELAY"
  fi
done

if [ "$DO_PING" = 1 ]; then
  if command -v ping >/dev/null 2>&1; then
    ping -c 3 -W 2 1.1.1.1 || log "ping failed (informational only)"
  else
    log "ping not installed, skipping"
  fi
fi

exit "$rc"
