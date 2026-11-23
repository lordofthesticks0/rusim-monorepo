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
# legacy/pi/water-control-system and legacy/master. logs/ and the deploy
# directory's stale .git are left untouched. web_control.py is syntax-checked
# before anything is written and PM2 is restarted only when that file's content
# changed.
#
# When legacy/master/master.cpp differs from the last uploaded build, the
# script compiles it with arduino-cli on the Pi, stops the PM2 app to free the
# serial port, uploads to the master Arduino, and restarts PM2. The hash of the
# last successful upload is kept in
# ~/.cache/water-control-system/master-flash.sha256.
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
#   PI_HOST         default invitro@10.92.128.23
#   PI_REPO         default https://github.com/lordofthesticks0/rusim-monorepo.git
#   PI_BRANCH       default master
#   PI_PM2          default water-ui
#   PI_ARDUINO_PORT default auto-detect /dev/ttyUSB* or /dev/ttyACM*
#   PI_ARDUINO_FQBN default arduino:avr:mega
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
PI_ARDUINO_PORT="${PI_ARDUINO_PORT:-}"
PI_ARDUINO_FQBN="${PI_ARDUINO_FQBN:-arduino:avr:mega}"

SPARSE_PATH="legacy/pi/water-control-system"
MASTER_PATH="legacy/master"
SRC_DIR="rusim-src"             # relative to $HOME on the Pi
DEST_DIR="water-control-system" # relative to $HOME on the Pi

DRY_RUN="${PI_DRY_RUN:-0}"
ALLOW_UNPUSHED="${PI_ALLOW_UNPUSHED:-0}"
LOCAL_MODE="${PI_LOCAL:-0}"
FLASH_MASTER="${PI_FLASH_MASTER:-1}"
FLASH_FORCE="${PI_FLASH_FORCE:-0}"
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
  ./sync_pi.sh --no-flash-master
                               deploy files but do not touch the Arduino
  ./sync_pi.sh --force-flash-master
                               upload master.cpp even if its hash is unchanged
  ./sync_pi.sh --install       copy this script to the Pi's home

Usage (on the Pi):
  ./sync_pi.sh --local         run the deploy locally (automatic on the Pi)

Environment overrides:
  PI_HOST         default invitro@10.92.128.23
  PI_REPO         default https://github.com/lordofthesticks0/rusim-monorepo.git
  PI_BRANCH       default master
  PI_PM2          default water-ui
  PI_ARDUINO_PORT default auto-detect /dev/ttyUSB* or /dev/ttyACM*
  PI_ARDUINO_FQBN default arduino:avr:mega
EOF
  exit "${1:-0}"
}

while [ $# -gt 0 ]; do
  case "$1" in
    -n|--dry-run) DRY_RUN=1; shift ;;
    -b|--branch)  PI_BRANCH="${2:?--branch needs a value}"; shift 2 ;;
    --allow-unpushed) ALLOW_UNPUSHED=1; shift ;;
    --no-flash-master) FLASH_MASTER=0; shift ;;
    --force-flash-master) FLASH_FORCE=1; shift ;;
    --install)    INSTALL=1; shift ;;
    --local)      LOCAL_MODE=1; shift ;;
    -h|--help)    usage 0 ;;
    *) echo "unknown argument: $1" >&2; usage 1 ;;
  esac
done

say()  { printf '\033[1m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[33mwarning:\033[0m %s\n' "$*" >&2; }

# ---- master firmware flashing ----------------------------------------------

ensure_arduino_toolchain() {
  command -v arduino-cli >/dev/null 2>&1 || {
    warn "arduino-cli is not installed; cannot flash the master"
    return 1
  }

  if ! arduino-cli core list 2>/dev/null | awk '{print $1}' | grep -qx 'arduino:avr'; then
    say "installing arduino:avr core"
    arduino-cli core update-index --silent 2>/dev/null || true
    arduino-cli core install arduino:avr@1.8.6 || {
      warn "failed to install the arduino:avr core"
      return 1
    }
  fi

  local aj_version
  aj_version=$(arduino-cli lib list 2>/dev/null | awk '$1 == "ArduinoJson" {print $2; exit}')
  if [ -z "$aj_version" ]; then
    say "installing ArduinoJson 6.21.6"
    arduino-cli lib install "ArduinoJson@6.21.6" || {
      warn "failed to install ArduinoJson 6.21.6"
      return 1
    }
  elif [ "${aj_version%%.*}" != "6" ]; then
    warn "ArduinoJson $aj_version is installed; master.cpp needs version 6.x"
    return 1
  fi
}

find_arduino_port() {
  local port found=()

  if [ -n "$PI_ARDUINO_PORT" ]; then
    [ -e "$PI_ARDUINO_PORT" ] || {
      warn "PI_ARDUINO_PORT=$PI_ARDUINO_PORT does not exist"
      return 1
    }
    printf '%s\n' "$PI_ARDUINO_PORT"
    return 0
  fi

  for port in /dev/ttyUSB* /dev/ttyACM*; do
    [ -e "$port" ] && found+=("$port")
  done

  case ${#found[@]} in
    0)
      warn "no /dev/ttyUSB* or /dev/ttyACM* port found; master firmware not flashed"
      return 1
      ;;
    1)
      printf '%s\n' "${found[0]}"
      ;;
    *)
      warn "multiple serial ports found (${found[*]}); set PI_ARDUINO_PORT"
      return 1
      ;;
  esac
}

master_flash_hash() {
  local src=$1
  {
    sha256sum "$src" | cut -d' ' -f1
    printf '%s\n' "$PI_ARDUINO_FQBN"
  } | sha256sum | cut -d' ' -f1
}

FLASH_PM2_STOPPED=0

flash_pm2_cleanup() {
  if [ "$FLASH_PM2_STOPPED" = 1 ]; then
    say "restarting PM2 app $PI_PM2"
    if ! pm2 start "$PI_PM2" --silent; then
      warn "could not restart PM2 app '$PI_PM2'; start it manually"
    fi
    FLASH_PM2_STOPPED=0
  fi
}

flash_master() {
  local src=$1 commit=$2
  local state_file="$HOME/.cache/water-control-system/master-flash.sha256"
  local wanted current port build_root build_dir rc attempt pid

  [ -f "$src" ] || {
    warn "missing $src; cannot flash the master"
    return 1
  }

  wanted=$(master_flash_hash "$src")
  current=$(cat "$state_file" 2>/dev/null || true)

  if [ "$FLASH_FORCE" != 1 ] && [ "$wanted" = "$current" ]; then
    say "master firmware already flashed at $commit; skipping"
    return 0
  fi

  if [ "$DRY_RUN" = 1 ]; then
    say "would flash master firmware from $MASTER_PATH/master.cpp ($commit)"
    return 0
  fi

  ensure_arduino_toolchain || return 1
  port=$(find_arduino_port) || return 0

  build_root=$(mktemp -d "${TMPDIR:-/tmp}/rusim-master.XXXXXX") || {
    warn "could not create a temporary build directory"
    return 1
  }
  mkdir -p "$build_root/master"
  cp "$src" "$build_root/master/master.ino"
  build_dir="$build_root/build"

  say "compiling master firmware for $PI_ARDUINO_FQBN"
  if ! arduino-cli compile --fqbn "$PI_ARDUINO_FQBN" \
      --build-path "$build_dir" "$build_root/master"; then
    rm -rf "$build_root"
    warn "master firmware compile failed; nothing flashed"
    return 1
  fi

  if command -v pm2 >/dev/null 2>&1 && pm2 describe "$PI_PM2" >/dev/null 2>&1; then
    pid=$(pm2 pid "$PI_PM2" 2>/dev/null | head -1 || true)
    if [ -n "$pid" ] && [ "$pid" != "0" ]; then
      FLASH_PM2_STOPPED=1
      trap flash_pm2_cleanup EXIT
      trap 'flash_pm2_cleanup; exit 130' INT TERM
      say "stopping PM2 app $PI_PM2 to free $port"
      if ! pm2 stop "$PI_PM2" --silent; then
        warn "could not stop PM2 app '$PI_PM2'; not flashing"
        FLASH_PM2_STOPPED=0
        trap - EXIT INT TERM
        rm -rf "$build_root"
        return 1
      fi
    fi
  fi

  if command -v fuser >/dev/null 2>&1; then
    for attempt in 1 2 3 4 5; do
      fuser -s "$port" 2>/dev/null || break
      sleep 1
    done
    if fuser -s "$port" 2>/dev/null; then
      warn "$port is still open; not flashing"
      flash_pm2_cleanup
      trap - EXIT INT TERM
      rm -rf "$build_root"
      return 1
    fi
  else
    sleep 1
  fi

  say "uploading master firmware to $port"
  rc=1
  for attempt in 1 2; do
    if arduino-cli upload --fqbn "$PI_ARDUINO_FQBN" --port "$port" \
        --input-dir "$build_dir" --verify; then
      rc=0
      break
    fi
    warn "upload attempt $attempt to $port failed"
    [ "$attempt" -eq 1 ] && sleep 2
  done

  # Give the board time to leave the bootloader before Flask reopens the port.
  sleep 2
  flash_pm2_cleanup
  trap - EXIT INT TERM

  if [ "$rc" -ne 0 ]; then
    rm -rf "$build_root"
    warn "master firmware upload failed"
    return 1
  fi

  mkdir -p "$(dirname "$state_file")"
  printf '%s\n' "$wanted" > "$state_file.tmp"
  mv "$state_file.tmp" "$state_file"
  rm -rf "$build_root"
  say "flashed master firmware at $commit"
  return 0
}

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
  local src relative_src master_src dest pm2_app dry_run old_hash new_hash rc changes commit subject pid
  local sketch_dest

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
    git -C "$src" sparse-checkout set "$sparse_path" "$MASTER_PATH"
  fi

  say "fetching origin/$branch"
  git -C "$src" fetch --quiet --prune origin \
    "+refs/heads/$branch:refs/remotes/origin/$branch"
  git -C "$src" checkout --quiet -B "$branch" "origin/$branch"
  git -C "$src" reset --hard --quiet "origin/$branch"
  git -C "$src" sparse-checkout set "$sparse_path" "$MASTER_PATH"

  commit=$(git -C "$src" rev-parse --short HEAD)
  subject=$(git -C "$src" log -1 --format=%s)
  relative_src="$src/$sparse_path"
  master_src="$src/$MASTER_PATH/master.cpp"

  [ -f "$relative_src/web_control.py" ] || {
    echo "error: $relative_src/web_control.py missing after checkout" >&2
    exit 1
  }
  [ -f "$master_src" ] || {
    echo "error: $master_src missing after checkout" >&2
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
    --exclude='*.pyc'
    --exclude=water_control_arduino/water_control_arduino.ino)
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

  # The Pi's sketch copy mirrors master.cpp, not the historical sketch in the
  # repository. It is excluded from rsync so it is not rewritten every run.
  sketch_dest="$dest/water_control_arduino/water_control_arduino.ino"
  if [ ! -f "$sketch_dest" ] || ! cmp -s "$master_src" "$sketch_dest"; then
    if [ "$dry_run" = "1" ]; then
      changes+="${changes:+$'\n'}>f+++++++++ water_control_arduino/water_control_arduino.ino (from $MASTER_PATH/master.cpp)"
    else
      mkdir -p "$(dirname "$sketch_dest")"
      install -m 644 "$master_src" "$sketch_dest"
      changes+="${changes:+$'\n'}>f+++++++++ water_control_arduino/water_control_arduino.ino (from $MASTER_PATH/master.cpp)"
    fi
  fi

  if [ -z "$changes" ]; then
    say "already up to date at $commit ($subject)"
  else
    if [ "$dry_run" = "1" ]; then
      say "would deploy $commit ($subject)"
    else
      say "deployed $commit ($subject)"
    fi
    printf '%s\n' "$changes"
  fi

  if [ "$dry_run" = "1" ]; then
    if [ "$FLASH_MASTER" = 1 ]; then
      flash_master "$master_src" "$commit" || true
    else
      say "skipping master firmware flash (--no-flash-master)"
    fi
    say "dry run: no files written, no services restarted, nothing flashed"
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

  if [ "$FLASH_MASTER" = 1 ]; then
    flash_master "$master_src" "$commit" || return 1
  else
    say "skipping master firmware flash (--no-flash-master)"
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
  if [ -n "$(git status --porcelain -- "$SPARSE_PATH" "$MASTER_PATH")" ]; then
    warn "uncommitted changes under $SPARSE_PATH or $MASTER_PATH will NOT be deployed"
  fi
  if git rev-parse --verify --quiet "origin/$PI_BRANCH" >/dev/null; then
    if [ -n "$(git log --oneline "origin/$PI_BRANCH..HEAD" -- "$SPARSE_PATH" "$MASTER_PATH")" ]; then
      UNPUSHED=1
      warn "local commits touching $SPARSE_PATH or $MASTER_PATH are not pushed to origin/$PI_BRANCH"
    fi
  fi
  if [ "$UNPUSHED" = 1 ] && [ "$DRY_RUN" != 1 ] && [ "$ALLOW_UNPUSHED" != 1 ]; then
    echo "error: origin/$PI_BRANCH does not contain your latest changes to $SPARSE_PATH or $MASTER_PATH." >&2
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
      PI_ARDUINO_PORT="$PI_ARDUINO_PORT" PI_ARDUINO_FQBN="$PI_ARDUINO_FQBN" \
      PI_DRY_RUN="$DRY_RUN" PI_ALLOW_UNPUSHED="$ALLOW_UNPUSHED" PI_LOCAL=1 \
      PI_FLASH_MASTER="$FLASH_MASTER" PI_FLASH_FORCE="$FLASH_FORCE" \
  bash -s < "$SCRIPT_PATH"
