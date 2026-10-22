#!/usr/bin/env bash
#
# sync_pi.sh - mirror legacy/pi/water-control-system from the monorepo's
# origin branch onto the live Raspberry Pi, on demand.
#
# The Pi clones the monorepo once into ~/rusim-src (sparse checkout), then this
# script fetches origin, resets the checkout, and rsyncs the tracked files into
# ~/water-control-system. logs/ and the deploy directory's stale .git are left
# untouched. web_control.py is syntax-checked before anything is written and
# PM2 is restarted only when that file changed.
#
# Usage:
#   ./sync_pi.sh              # deploy origin/master
#   ./sync_pi.sh --dry-run    # show what would change, write nothing
#   ./sync_pi.sh --branch X   # deploy origin/X
#   ./sync_pi.sh --allow-unpushed
#
# Environment overrides:
#   PI_HOST   default invitro@10.92.128.23
#   PI_REPO   default https://github.com/lordofthesticks0/rusim-monorepo.git
#   PI_BRANCH default master
#   PI_PM2    default water-ui
#
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$SCRIPT_DIR"

PI_HOST="${PI_HOST:-invitro@10.92.128.23}"
PI_REPO="${PI_REPO:-https://github.com/lordofthesticks0/rusim-monorepo.git}"
PI_BRANCH="${PI_BRANCH:-master}"
PI_PM2="${PI_PM2:-water-ui}"

SPARSE_PATH="legacy/pi/water-control-system"
SRC_DIR="rusim-src"            # relative to $HOME on the Pi
DEST_DIR="water-control-system" # relative to $HOME on the Pi

DRY_RUN=0
ALLOW_UNPUSHED=0

usage() {
  cat <<'EOF'
sync_pi.sh - mirror legacy/pi/water-control-system from origin onto the Pi.

Usage:
  ./sync_pi.sh                 deploy origin/master
  ./sync_pi.sh --dry-run       show what would change, write nothing
  ./sync_pi.sh --branch X      deploy origin/X
  ./sync_pi.sh --allow-unpushed
                               deploy even if local commits are not pushed

Environment overrides:
  PI_HOST   default invitro@10.92.128.23
  PI_REPO   default https://github.com/lordofthesticks0/rusim-monorepo.git
  PI_BRANCH default master
  PI_PM2    default water-ui
EOF
  exit "${1:-0}"
}

while [ $# -gt 0 ]; do
  case "$1" in
    -n|--dry-run) DRY_RUN=1; shift ;;
    -b|--branch)  PI_BRANCH="${2:?--branch needs a value}"; shift 2 ;;
    --allow-unpushed) ALLOW_UNPUSHED=1; shift ;;
    -h|--help)    usage 0 ;;
    *) echo "unknown argument: $1" >&2; usage 1 ;;
  esac
done

say()  { printf '\033[1m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[33mwarning:\033[0m %s\n' "$*" >&2; }

# ---- local checks -----------------------------------------------------------

if command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  LOCAL_BRANCH=$(git branch --show-current)
  UNPUSHED=0
  if [ "$LOCAL_BRANCH" != "$PI_BRANCH" ]; then
    warn "local branch is '$LOCAL_BRANCH', deploying 'origin/$PI_BRANCH'"
  fi
  if [ -n "$(git status --porcelain -- "$SPARSE_PATH")" ]; then
    warn "uncommitted changes under $SPARSE_PATH will NOT be deployed"
  fi
  if git rev-parse --verify --quiet "origin/$PI_BRANCH" >/dev/null; then
    if [ -n "$(git log --oneline "origin/$PI_BRANCH..HEAD" -- "$SPARSE_PATH")" ]; then
      UNPUSHED=1
      warn "local commits touching $SPARSE_PATH are not pushed to origin/$PI_BRANCH"
    fi
  fi
  if [ "$UNPUSHED" = 1 ] && [ "$DRY_RUN" != 1 ] && [ "$ALLOW_UNPUSHED" != 1 ]; then
    echo "error: origin/$PI_BRANCH does not contain your latest changes to $SPARSE_PATH." >&2
    echo "       Deploying it would roll the Pi back. Push first, or pass --allow-unpushed" >&2
    echo "       after checking ./sync_pi.sh --dry-run." >&2
    exit 1
  fi
fi

if ! ssh -o BatchMode=yes -o ConnectTimeout=10 "$PI_HOST" true 2>/dev/null; then
  echo "error: cannot SSH to $PI_HOST" >&2
  exit 1
fi

# ---- remote work ------------------------------------------------------------

ssh -o BatchMode=yes -o ConnectTimeout=10 "$PI_HOST" \
  bash -s -- "$PI_REPO" "$PI_BRANCH" "$SPARSE_PATH" "$SRC_DIR" "$DEST_DIR" "$PI_PM2" "$DRY_RUN" <<'REMOTE'
set -euo pipefail

REPO_URL=$1
BRANCH=$2
SPARSE_PATH=$3
SRC_DIR=$4
DEST_DIR=$5
PM2_APP=$6
DRY_RUN=$7

abspath() { case "$1" in /*) printf '%s' "$1" ;; *) printf '%s/%s' "$HOME" "$1" ;; esac; }
SRC_DIR=$(abspath "$SRC_DIR")
DEST_DIR=$(abspath "$DEST_DIR")

say()  { printf '\033[1m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[33mwarning:\033[0m %s\n' "$*" >&2; }

command -v git   >/dev/null || { echo "error: git is not installed on the Pi" >&2; exit 1; }
command -v rsync >/dev/null || { echo "error: rsync is not installed on the Pi" >&2; exit 1; }
[ -d "$DEST_DIR" ] || { echo "error: deploy directory $DEST_DIR does not exist" >&2; exit 1; }

if [ -d "$SRC_DIR" ] && [ ! -d "$SRC_DIR/.git" ]; then
  echo "error: $SRC_DIR exists but is not a git clone; move it aside" >&2
  exit 1
fi
if [ ! -d "$SRC_DIR/.git" ]; then
  say "cloning $REPO_URL to $SRC_DIR"
  git clone --quiet "$REPO_URL" "$SRC_DIR"
  git -C "$SRC_DIR" sparse-checkout init --cone
  git -C "$SRC_DIR" sparse-checkout set "$SPARSE_PATH"
fi

say "fetching origin/$BRANCH"
git -C "$SRC_DIR" fetch --quiet --prune origin \
  "+refs/heads/$BRANCH:refs/remotes/origin/$BRANCH"
git -C "$SRC_DIR" checkout --quiet -B "$BRANCH" "origin/$BRANCH"
git -C "$SRC_DIR" reset --hard --quiet "origin/$BRANCH"
git -C "$SRC_DIR" sparse-checkout set "$SPARSE_PATH"

COMMIT=$(git -C "$SRC_DIR" rev-parse --short HEAD)
SUBJECT=$(git -C "$SRC_DIR" log -1 --format=%s)
SRC="$SRC_DIR/$SPARSE_PATH"

[ -f "$SRC/web_control.py" ] || {
  echo "error: $SRC/web_control.py missing after checkout" >&2
  exit 1
}

# Refuse to deploy Python that does not compile.
python3 -c 'import sys; compile(open(sys.argv[1], encoding="utf-8").read(), sys.argv[1], "exec")' \
  "$SRC/web_control.py" || { echo "error: web_control.py failed syntax check; nothing written" >&2; exit 1; }

OLD_HASH=$(sha256sum "$DEST_DIR/web_control.py" 2>/dev/null | cut -d' ' -f1 || true)

RSYNC_OPTS=(-a --delete --itemize-changes --omit-dir-times
  --exclude=.git/
  --exclude=logs/
  --exclude=__pycache__/
  --exclude='*.pyc')
if [ "$DRY_RUN" = "1" ]; then
  RSYNC_OPTS+=(--dry-run)
fi

set +e
CHANGES=$(rsync "${RSYNC_OPTS[@]}" "$SRC"/ "$DEST_DIR"/)
RSYNC_RC=$?
set -e
if [ "$RSYNC_RC" -ne 0 ]; then
  echo "error: rsync failed with exit code $RSYNC_RC" >&2
  exit "$RSYNC_RC"
fi

if [ -z "$CHANGES" ]; then
  say "already up to date at $COMMIT ($SUBJECT)"
  exit 0
fi

if [ "$DRY_RUN" = "1" ]; then
  say "would deploy $COMMIT ($SUBJECT)"
else
  say "deployed $COMMIT ($SUBJECT)"
fi
printf '%s\n' "$CHANGES"

if [ "$DRY_RUN" = "1" ]; then
  say "dry run: no files written, no services restarted"
  exit 0
fi

NEW_HASH=$(sha256sum "$DEST_DIR/web_control.py" | cut -d' ' -f1)
if [ "$OLD_HASH" != "$NEW_HASH" ]; then
  if command -v pm2 >/dev/null && pm2 describe "$PM2_APP" >/dev/null 2>&1; then
    say "restarting PM2 app $PM2_APP"
    pm2 restart "$PM2_APP" --silent
    sleep 1
    PID=$(pm2 pid "$PM2_APP" 2>/dev/null | head -1 || true)
    say "PM2 $PM2_APP pid: ${PID:-unknown}"
  else
    warn "web_control.py changed but PM2 app '$PM2_APP' was not found; restart it yourself"
  fi
fi

if printf '%s\n' "$CHANGES" | grep -q 'templates/index.html'; then
  warn "templates/index.html changed; reload the kiosk browser to pick it up"
fi
REMOTE
