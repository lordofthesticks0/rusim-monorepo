#!/usr/bin/env bash
#
# sync_pi.sh - mirror legacy/pi/water-control-system from the monorepo's
# origin branch onto the live Raspberry Pi.
#
# Runs in two modes with the same code:
#   - from the development machine (default): checks the local repo, SSHes to
#     the Pi, and runs this script there;
#   - on the Pi (automatic or --local): fetches origin and rsyncs into
#     ~/water-control-system directly.
#
# The Pi keeps a clone at ~/rusim-src with a sparse checkout of
# legacy/pi/water-control-system. logs/ and the deploy directory's stale .git
# are left untouched. web_control.py is syntax-checked before anything is
# written and PM2 is restarted only when that file's content changed.
#
# Usage (development machine):
#   ./sync_pi.sh                 deploy origin/master
#   ./sync_pi.sh --dry-run       show what would change, write nothing
#   ./sync_pi.sh --branch X      deploy origin/X
#   ./sync_pi.sh --install       copy this script to the Pi's home
#
# Usage (on the Pi, ~/sync_pi.sh):
#   ./sync_pi.sh --local         run the deploy locally (automatic on the Pi)
#
# Environment overrides:
#   PI_HOST   default invitro@10.92.128.23
#   PI_REPO   default https://github.com/lordofthesticks0/rusim-monorepo.git
#   PI_BRANCH default master
#   PI_PM2    default water-ui
#
set -euo pipefail

if [ -n "${BASH_SOURCE[0]:-}" ] && [ -f "${BASH_SOURCE[0]}" ]; then
  SCRIPT_PATH=$(readlink -f "${BASH_SOURCE[0]}")
  SCRIPT_DIR=$(cd "$(dirname "$SCRIPT_PATH")" && pwd)
  cd "$SCRIPT_DIR"
else
  SCRIPT_PATH=""
  SCRIPT_DIR=$PWD
fi

PI_HOST="${PI_HOST:-invitro@10.92.128.23}"
PI_REPO="${PI_REPO:-https://github.com/lordofthesticks0/rusim-monorepo.git}"
PI_BRANCH="${PI_BRANCH:-master}"
PI_PM2="${PI_PM2:-water-ui}"

SPARSE_PATH="legacy/pi/water-control-system"
SRC_DIR="rusim-src"             # relative to $HOME on the Pi
DEST_DIR="water-control-system" # relative to $HOME on the Pi

DRY_RUN="${PI_DRY_RUN:-0}"
ALLOW_UNPUSHED="${PI_ALLOW_UNPUSHED:-0}"
LOCAL_MODE="${PI_LOCAL:-0}"
INSTALL=0

usage() {
  cat <<'EOF'
sync_pi.sh - mirror legacy/pi/water-control-system from origin onto the Pi.

Usage (development machine):
  ./sync_pi.sh                 deploy origin/master
  ./sync_pi.sh --dry-run       show what would change, write nothing
  ./sync_pi.sh --branch X      deploy origin/X
  ./sync_pi.sh --allow-unpushed
                               deploy even if local commits are not pushed
  ./sync_pi.sh --install       copy this script to the Pi's home

Usage (on the Pi):
  ./sync_pi.sh --local         run the deploy locally (automatic on the Pi)

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
    --install)    INSTALL=1; shift ;;
    --local)      LOCAL_MODE=1; shift ;;
    -h|--help)    usage 0 ;;
    *) echo "unknown argument: $1" >&2; usage 1 ;;
  esac
done

say()  { printf '\033[1m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[33mwarning:\033[0m %s\n' "$*" >&2; }

# On the Pi, run locally without the SSH hop.
if [ "$LOCAL_MODE" != 1 ] && [ "$(hostname -s 2>/dev/null || true)" = "invitrocontroller" ]; then
  LOCAL_MODE=1
fi

# ---- install ----------------------------------------------------------------

if [ "$INSTALL" = 1 ]; then
  if [ "$LOCAL_MODE" = 1 ]; then
    echo "error: --install runs from the development machine, not the Pi" >&2
    exit 1
  fi
  [ -n "$SCRIPT_PATH" ] || { echo "error: --install requires running from a file" >&2; exit 1; }
  say "installing $SCRIPT_PATH to $PI_HOST:~/sync_pi.sh"
  ssh -o BatchMode=yes -o ConnectTimeout=10 "$PI_HOST" \
    'cat > "$HOME/.sync_pi.sh.new" && chmod 755 "$HOME/.sync_pi.sh.new" \
     && mv "$HOME/.sync_pi.sh.new" "$HOME/sync_pi.sh" && echo installed' < "$SCRIPT_PATH"
  exit 0
fi

# ---- deploy (runs in both modes) --------------------------------------------

deploy_local() {
  local repo_url=$PI_REPO branch=$PI_BRANCH sparse_path=$SPARSE_PATH
  local src relative_src dest pm2_app dry_run old_hash new_hash rc changes commit subject pid

  src=$(case "$SRC_DIR" in /*) printf '%s' "$SRC_DIR" ;; *) printf '%s/%s' "$HOME" "$SRC_DIR" ;; esac)
  dest=$(case "$DEST_DIR" in /*) printf '%s' "$DEST_DIR" ;; *) printf '%s/%s' "$HOME" "$DEST_DIR" ;; esac)
  dry_run=$DRY_RUN
  pm2_app=$PI_PM2

  command -v git   >/dev/null || { echo "error: git is not installed" >&2; exit 1; }
  command -v rsync >/dev/null || { echo "error: rsync is not installed" >&2; exit 1; }
  [ -d "$dest" ] || { echo "error: deploy directory $dest does not exist" >&2; exit 1; }

  if [ -d "$src" ] && [ ! -d "$src/.git" ]; then
    echo "error: $src exists but is not a git clone; move it aside" >&2
    exit 1
  fi
  if [ ! -d "$src/.git" ]; then
    say "cloning $repo_url to $src"
    git clone --quiet "$repo_url" "$src"
    git -C "$src" sparse-checkout init --cone
    git -C "$src" sparse-checkout set "$sparse_path"
  fi

  say "fetching origin/$branch"
  git -C "$src" fetch --quiet --prune origin \
    "+refs/heads/$branch:refs/remotes/origin/$branch"
  git -C "$src" checkout --quiet -B "$branch" "origin/$branch"
  git -C "$src" reset --hard --quiet "origin/$branch"
  git -C "$src" sparse-checkout set "$sparse_path"

  commit=$(git -C "$src" rev-parse --short HEAD)
  subject=$(git -C "$src" log -1 --format=%s)
  relative_src="$src/$sparse_path"

  [ -f "$relative_src/web_control.py" ] || {
    echo "error: $relative_src/web_control.py missing after checkout" >&2
    exit 1
  }

  # Refuse to deploy Python that does not compile.
  python3 -c 'import sys; compile(open(sys.argv[1], encoding="utf-8").read(), sys.argv[1], "exec")' \
    "$relative_src/web_control.py" || {
      echo "error: web_control.py failed syntax check; nothing written" >&2
      exit 1
    }

  old_hash=$(sha256sum "$dest/web_control.py" 2>/dev/null | cut -d' ' -f1 || true)

  local opts=(-a --delete --itemize-changes --omit-dir-times
    --exclude=.git/
    --exclude=logs/
    --exclude=__pycache__/
    --exclude='*.pyc')
  if [ "$dry_run" = "1" ]; then
    opts+=(--dry-run)
  fi

  set +e
  changes=$(rsync "${opts[@]}" "$relative_src"/ "$dest"/)
  rc=$?
  set -e
  if [ "$rc" -ne 0 ]; then
    echo "error: rsync failed with exit code $rc" >&2
    exit "$rc"
  fi

  if [ -z "$changes" ]; then
    say "already up to date at $commit ($subject)"
    return 0
  fi

  if [ "$dry_run" = "1" ]; then
    say "would deploy $commit ($subject)"
  else
    say "deployed $commit ($subject)"
  fi
  printf '%s\n' "$changes"

  if [ "$dry_run" = "1" ]; then
    say "dry run: no files written, no services restarted"
    return 0
  fi

  new_hash=$(sha256sum "$dest/web_control.py" | cut -d' ' -f1)
  if [ "$old_hash" != "$new_hash" ]; then
    if command -v pm2 >/dev/null && pm2 describe "$pm2_app" >/dev/null 2>&1; then
      say "restarting PM2 app $pm2_app"
      pm2 restart "$pm2_app" --silent
      sleep 1
      pid=$(pm2 pid "$pm2_app" 2>/dev/null | head -1 || true)
      say "PM2 $pm2_app pid: ${pid:-unknown}"
    else
      warn "web_control.py changed but PM2 app '$pm2_app' was not found; restart it yourself"
    fi
  fi

  if printf '%s\n' "$changes" | grep -q 'templates/index.html'; then
    warn "templates/index.html changed; reload the kiosk browser to pick it up"
  fi
}

# ---- mode dispatch ----------------------------------------------------------

if [ "$LOCAL_MODE" = 1 ]; then
  deploy_local
  exit 0
fi

# Development-machine mode: check the local repo, then run this script on the Pi.

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
else
  warn "not inside a git repository; skipping the local push checks"
fi

if ! ssh -o BatchMode=yes -o ConnectTimeout=10 "$PI_HOST" true 2>/dev/null; then
  echo "error: cannot SSH to $PI_HOST" >&2
  exit 1
fi

[ -n "$SCRIPT_PATH" ] || { echo "error: cannot locate this script to run it remotely" >&2; exit 1; }

say "running on $PI_HOST"
exec ssh -o BatchMode=yes -o ConnectTimeout=10 "$PI_HOST" \
  env PI_REPO="$PI_REPO" PI_BRANCH="$PI_BRANCH" PI_PM2="$PI_PM2" \
      PI_DRY_RUN="$DRY_RUN" PI_ALLOW_UNPUSHED="$ALLOW_UNPUSHED" PI_LOCAL=1 \
  bash -s < "$SCRIPT_PATH"
