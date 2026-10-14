#!/usr/bin/env bash
#
# captive-portal-login.sh: log in to a walled network's captive portal from a
# Linux host, using the same steps as firmware/network-master/main.cpp.
#
# The firmware sequence, and where each step lives here:
#   1. fetchPortalUrl()  GET http://connectivitycheck.gstatic.com/generate_204
#                        with redirects off.
#                        204 -> no portal, network is open.
#                        3xx -> portal URL from the Location header.
#                        200 -> portal URL from the interception page's
#                        `window.location="..."` script.
#   2. fetchLoginPage()  HTTPS GET of that URL (TLS verification off, same as
#                        the firmware's setInsecure()), scan for the form
#                        `action`, the hidden `4Tredir`, and the hidden `magic`.
#   3. postCredentials() POST application/x-www-form-urlencoded
#                        4Tredir=...&magic=...&username=...&password=... to the
#                        resolved form action, with Content-Type and Referer.
#   4. fetchData()       GET the probe URL again. 204 means the port is open.
#                        That verdict is final; the POST response is only logged.
#
# Differences from the firmware, all deliberate:
#   - The firmware caps body reads at 4096 bytes and scans the login page
#     through a 320-byte rolling window because the WT32-ETH01 has no PSRAM.
#     Here the whole page is scanned.
#   - A cookie jar is kept across the login page GET, the POST and the verify,
#     so a portal that hands out a session cookie works. The firmware does not
#     need one (the Unpad portal authenticates by MAC).
#   - Verification retries three times with a 2 s gap, because the firmware
#     checks immediately after the POST.
#   - Credentials are asked for on a hidden prompt unless --user/--pass are
#     given. They live in shell variables for the length of the run and are
#     unset on exit. Nothing is written to disk.
#
# Exit codes: 0 authenticated, 1 still walled, 2 usage or missing dependency,
# 3 the probe could not be fetched at all.
#
# Usage:
#   scripts-hardware/captive-portal-login.sh --user alice
#   scripts-hardware/captive-portal-login.sh --user alice --pass s3cret
#   scripts-hardware/captive-portal-login.sh            # prompts for both
#   scripts-hardware/captive-portal-login.sh --ping --verbose
#
set -euo pipefail

PROBE_URL_DEFAULT="http://connectivitycheck.gstatic.com/generate_204"
USER_NAME=""
PASSWORD=""
PROBE_URL="$PROBE_URL_DEFAULT"
DO_PING=0
VERBOSE=0
VERIFY_ATTEMPTS=3
VERIFY_DELAY=2
BODY_HEAD=800

usage() {
  cat <<EOF
Log in to a captive portal and verify the port is open (Linux/cURL port of the
network-master firmware login sequence).

Usage:
  captive-portal-login.sh [options]

Options:
  --user <name>     Portal username. Prompted for when missing.
  --pass <secret>   Portal password. Prompted for (hidden) when missing.
                    Passing it on the command line leaves it in shell history
                    and in \`ps\`; prefer the prompt.
  --probe-url <url> Connectivity probe (default: $PROBE_URL_DEFAULT).
  --ping            Ping 1.1.1.1 three times after success. Informational;
                    a failure does not change the exit code.
  --verbose         Print the POST response head and every curl status.
  -h, --help        Show this help.

Exit codes: 0 authenticated, 1 still walled, 2 usage/dependency, 3 probe failed.
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --user|--pass|--probe-url)
      if [ $# -lt 2 ]; then
        printf '%s needs a value\n' "$1" >&2
        exit 2
      fi
      case "$1" in
        --user) USER_NAME="$2" ;;
        --pass) PASSWORD="$2" ;;
        --probe-url) PROBE_URL="$2" ;;
      esac
      shift 2
      ;;
    --ping) DO_PING=1; shift ;;
    --verbose|-v) VERBOSE=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) printf 'unknown option: %s\n\n' "$1" >&2; usage >&2; exit 2 ;;
  esac
done

command -v curl >/dev/null 2>&1 || { echo "curl is required" >&2; exit 2; }

log() { printf '[LOGIN] %s\n' "$*"; }

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
  : >"$TMP_HDR"
  : >"$TMP_BODY"
  local code
  log "GET trigger $PROBE_URL"
  if ! code="$(curl -sS --connect-timeout 5 --max-time 10 \
      -c "$TMP_JAR" -b "$TMP_JAR" -o "$TMP_BODY" -D "$TMP_HDR" \
      -w '%{http_code}' "$PROBE_URL" 2>"$TMP/err")"; then
    log "trigger error: $(tr -d '\n' <"$TMP/err")"
    return 3
  fi
  PROBE_STATUS="$code"
  log "trigger status: $code ($label)"
  if [ "$VERBOSE" = 1 ]; then
    sed 's/\r$//' "$TMP_HDR" | sed 's/^/[LOGIN] hdr: /' || true
  fi

  case "$code" in
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
      log "unexpected trigger status (treating as walled)"
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

# --- main ----------------------------------------------------------------

# Probe before prompting: an already-open network exits here without asking
# for credentials. probe_once returns non-zero for every classification but
# "open", so the call is guarded: under `set -e` a bare call would abort the
# script instead of reaching the case below.
probe_rc=0
probe_once "initial" || probe_rc=$?
case "$probe_rc" in
  0)
    log "Already authenticated, skipping POST."
    log "SUCCESS: port is authenticated."
    exit 0
    ;;
  2)
    log "FAILED: walled, but the portal served no login URL to follow."
    exit 1
    ;;
  3)
    log "FAILED: the probe URL could not be fetched."
    exit 3
    ;;
esac

if [ -z "$USER_NAME" ]; then
  printf 'Portal username: '
  if ! read -r USER_NAME; then
    printf '\nno username on stdin; pass --user instead\n' >&2
    exit 2
  fi
fi
if [ -z "$PASSWORD" ]; then
  if ! read -r -s -p "Portal password: " PASSWORD; then
    printf '\nno password on stdin; pass --pass instead\n' >&2
    exit 2
  fi
  printf '\n'
fi
if [ -z "$USER_NAME" ] || [ -z "$PASSWORD" ]; then
  echo "username and password are required" >&2
  exit 2
fi

log "user='$USER_NAME' (password held in memory only)"

# --- step 2: login page --------------------------------------------------
log "GET login page $PORTAL_URL"
: >"$TMP_HDR"
: >"$TMP_BODY"
page_code="$(curl -sS -k --connect-timeout 5 --max-time 15 \
  -c "$TMP_JAR" -b "$TMP_JAR" -o "$TMP_BODY" -D "$TMP_HDR" \
  -w '%{http_code}' "$PORTAL_URL" 2>"$TMP/err" || true)"
page_code="$(printf '%s' "$page_code" | tr -cd '0-9')"
[ -n "$page_code" ] || page_code=0
log "login page status: $page_code"
if [ "$page_code" != "200" ]; then
  log "Could not read the login page ($(tr -d '\n' <"$TMP/err"))."
  exit 1
fi

ACTION="$(form_action "$TMP_BODY")"
REDIR="$(value_after 'name="4Tredir"' "$TMP_BODY")"
MAGIC="$(value_after 'name="magic"' "$TMP_BODY")"
HAS_USER_FIELD=no
if grep -q 'name="username"' "$TMP_BODY"; then HAS_USER_FIELD=yes; fi

log "form action: ${ACTION:-(no form found)}"
log "4Tredir: ${REDIR:-(not found)}"
log "magic: ${MAGIC:-(not found)}"
log "username field: $HAS_USER_FIELD"

if [ -z "$ACTION" ] || [ -z "$MAGIC" ]; then
  log "FAILED: login page has no form action or no magic token (portal changed?)."
  exit 1
fi

# --- step 3: POST --------------------------------------------------------
POST_URL="$(resolve_action "$PORTAL_URL" "$ACTION")"
log "POST $POST_URL"
: >"$TMP_HDR"
: >"$TMP_BODY"
post_code="$(curl -sS -k --connect-timeout 5 --max-time 20 \
  -c "$TMP_JAR" -b "$TMP_JAR" \
  -D "$TMP_HDR" -o "$TMP_BODY" -w '%{http_code}' \
  -H 'Content-Type: application/x-www-form-urlencoded' \
  -H "Referer: $PORTAL_URL" \
  --data-urlencode "4Tredir=$REDIR" \
  --data-urlencode "magic=$MAGIC" \
  --data-urlencode "username=$USER_NAME" \
  --data-urlencode "password=$PASSWORD" \
  "$POST_URL" 2>"$TMP/err" || true)"
post_code="$(printf '%s' "$post_code" | tr -cd '0-9')"
[ -n "$post_code" ] || post_code=0
log "POST status: $post_code"
PASSWORD=''
unset PASSWORD || true

post_failed=0
if [ "$post_code" -lt 200 ] 2>/dev/null; then post_failed=1; fi
if [ "$VERBOSE" = 1 ] || [ "$post_failed" = 1 ]; then
  sed 's/\r$//' "$TMP_HDR" | sed 's/^/[LOGIN] hdr: /' || true
  head -c "$BODY_HEAD" "$TMP_BODY" | sed 's/^/[LOGIN] body: /' || true
fi
if [ "$post_failed" = 1 ]; then
  log "POST error: $(tr -d '\n' <"$TMP/err")"
fi

# The firmware logs acceptance here but lets the verifier decide. Keep the log
# line, not the verdict: the Unpad portal answers a good login with a 200 HTML
# status page and no Location header.
accepted=no
case "$post_code" in
  301|302|303|307|308) accepted=yes ;;
esac
if grep -qi 'success' "$TMP_BODY"; then accepted=yes; fi
if [ -n "$REDIR" ] && grep -qF "$REDIR" "$TMP_BODY"; then accepted=yes; fi
log "POST looked accepted: $accepted (verdict still comes from the probe)"

# --- step 4: verify ------------------------------------------------------
log "Verifying with a real fetch..."
rc=1
for attempt in $(seq 1 "$VERIFY_ATTEMPTS"); do
  if [ "$attempt" -gt 1 ]; then
    log "probe attempt $attempt of $VERIFY_ATTEMPTS"
    sleep "$VERIFY_DELAY"
  fi
  probe_rc=0
  probe_once "verify" || probe_rc=$?
  case "$probe_rc" in
    0)
      log "SUCCESS: port is authenticated."
      rc=0
      break
      ;;
    *)
      log "still walled after $attempt attempt(s) (probe classification $probe_rc)"
      ;;
  esac
done

if [ "$rc" = 0 ] && [ "$DO_PING" = 1 ]; then
  if command -v ping >/dev/null 2>&1; then
    ping -c 3 -W 2 1.1.1.1 || log "ping failed (informational only)"
  else
    log "ping not installed, skipping"
  fi
fi

if [ "$rc" != 0 ]; then
  log "FAILED: still walled (wrong credentials or portal changed)."
fi
exit "$rc"
