# Pi sync script — `sync_pi.sh`

Manual deploy of `legacy/pi/water-control-system` from the monorepo's `origin`
branch to the live Raspberry Pi, plus an automatic compile-and-upload of
`legacy/master/master.cpp` to the master Arduino when that file changes.

The same script runs in two modes:

- **Development machine (default):** checks the local repo, SSHes to the Pi,
  and runs the script there.
- **On the Pi (automatic, or `--local`):** fetches `origin`, rsyncs into
  `~/water-control-system`, and flashes the Arduino directly, without the SSH
  hop.

The Pi keeps a clone at `~/rusim-src` with a sparse checkout of
`legacy/pi/water-control-system` and `legacy/master`. The local working tree is
never copied. Only commits that exist on `origin/master` can be deployed.

## Usage

On the development machine:

```bash
./sync_pi.sh                 # deploy origin/master and flash a changed master.cpp
./sync_pi.sh --dry-run       # show changes, write nothing, flash nothing
./sync_pi.sh --branch X      # deploy origin/X
./sync_pi.sh --allow-unpushed
./sync_pi.sh --no-flash-master
                             # deploy files, leave the Arduino alone
./sync_pi.sh --force-flash-master
                             # upload master.cpp even if its hash is unchanged
./sync_pi.sh --install       # copy this script to /home/invitro/sync_pi.sh
```

On the Pi:

```bash
cd ~
./sync_pi.sh --dry-run
./sync_pi.sh                 # --local is implied by the hostname
./sync_pi.sh --local         # force local mode on any host
```

Environment overrides:

| Variable | Default |
|---|---|
| `PI_HOST` | `invitro@10.92.128.23` |
| `PI_REPO` | `https://github.com/lordofthesticks0/rusim-monorepo.git` |
| `PI_BRANCH` | `master` |
| `PI_PM2` | `water-ui` |
| `PI_ARDUINO_PORT` | auto-detect `/dev/ttyUSB*` or `/dev/ttyACM*` |
| `PI_ARDUINO_FQBN` | `arduino:avr:mega` |

Example:

```bash
PI_HOST=invitro@10.92.128.23 ./sync_pi.sh --dry-run
```

## What it does

Development-machine mode:

1. Checks the local repository:
   - warns if the current branch differs from the deploy branch;
   - warns if `legacy/pi/water-control-system` has uncommitted changes;
   - detects local commits touching that path which are not on
     `origin/<branch>`.
2. If local commits are unpushed, a real run **aborts**. This prevents
   deploying an older `origin` and rolling the Pi back. `--dry-run` still
   proceeds so the rollback is visible; `--allow-unpushed` bypasses the guard.
3. Sends this script over one SSH session and runs it on the Pi with
   `PI_LOCAL=1`.

Both modes then do the same work on the Pi:

1. Clones `PI_REPO` to `~/rusim-src` on first use, with a cone-mode sparse
   checkout of `legacy/pi/water-control-system` and `legacy/master`.
2. Fetches `origin/<branch>` and hard-resets the checkout to it.
3. Syntax-checks the checkout's `web_control.py` with `compile()`. On failure
   it exits before writing anything.
4. Rsyncs the tree to `~/water-control-system`.
5. Copies `legacy/master/master.cpp` over
   `~/water-control-system/water_control_arduino/water_control_arduino.ino`.
   That path is excluded from rsync so the copy is not rewritten every run.
6. If `web_control.py` content changed, restarts the PM2 app `water-ui`.
7. If `templates/index.html` changed, prints a reminder to reload the kiosk.
8. Compiles and uploads the master firmware when `master.cpp` or the FQBN
   differs from the last successful upload (see below).

Local mode skips the push checks because the Pi has no development repository.
It deploys whatever is on `origin/<branch>`.

## Master firmware flashing

The source of truth is `legacy/master/master.cpp` (PlatformIO environment
`[env:legacy-master]`). The script flashes it automatically when its content or
the FQBN changes. The hash of the last successful upload is stored in
`~/.cache/water-control-system/master-flash.sha256`, outside the rsync target.

On a hash mismatch the script:

1. Checks for `arduino-cli` and the `arduino:avr` core. Installs the core if it
   is missing. ArduinoJson must be 6.x because `master.cpp` uses
   `StaticJsonDocument` and `serialized()`; the script installs 6.21.6 when the
   library is absent and refuses to flash if a 7.x version is installed.
2. Detects the serial port. With no `PI_ARDUINO_PORT`, it requires exactly one
   of `/dev/ttyUSB*` or `/dev/ttyACM*`.
3. Compiles `master.cpp` in a temporary directory as `master/master.ino` with
   `arduino-cli compile --fqbn "$PI_ARDUINO_FQBN"`.
4. Stops the PM2 app `water-ui` so Flask releases the serial port, then waits up
   to five seconds for `fuser` to report the port free.
5. Uploads with `arduino-cli upload --verify` and retries once on failure.
6. Sleeps two seconds, restarts PM2, and writes the new hash only after a
   successful upload.

A failed compile or upload leaves the stored hash unchanged, so the next sync
retries. A missing port prints a warning and lets the file deploy finish; the
hash also stays unchanged.

`--force-flash-master` ignores the stored hash. `--no-flash-master` skips the
flashing step entirely. `--dry-run` reports whether a flash is due without
installing tools, stopping services, or touching the board.

Flashing resets the Arduino. `setup()` calls `forceAllOff()` immediately, but
the circulation pump stops for the duration of the upload. The master source
implements water-level sensing with dry-run cutoff protection.

## What is and is not touched

| Path | Behaviour |
|---|---|
| `web_control.py`, `templates/`, `.gitignore` | mirrored from `origin` |
| `water_control_arduino/water_control_arduino.ino` | excluded from rsync, then overwritten from `legacy/master/master.cpp` |
| `~/water-control-system/logs/` | excluded, never read or deleted |
| `~/water-control-system/.git/` | excluded and protected from `--delete` |
| `~/.cache/water-control-system/master-flash.sha256` | flash state, written only on a successful upload |
| `__pycache__/`, `*.pyc` | excluded |

Rsync runs with `--delete`, so tracked files removed from the repo are removed
from the Pi. Excluded paths are protected. Anything else in
`~/water-control-system` that is not in the repo will be deleted; the current
directory contains only the tracked files, `logs/` and `.git/`.

## Prerequisites

- SSH key access to `PI_HOST` (the script uses `BatchMode=yes`, so it never
  prompts for a password).
- `git` and `rsync` on the Pi (both present).
- The Pi can reach GitHub over HTTPS (verified).
- For the restart step: PM2 running as the SSH user with an app named
  `water-ui`.
- For flashing: `arduino-cli` on the Pi, the `arduino:avr` core, and the user in
  the `dialout` group. ArduinoJson 6.x is installed on demand. The master
  Arduino must be connected over USB as the only `/dev/ttyUSB*` or
  `/dev/ttyACM*` device, or `PI_ARDUINO_PORT` must name it.

## First run

The first real run creates `~/rusim-src` on the Pi. The clone is public, so no
credentials are stored. Expect a dry run first:

```bash
./sync_pi.sh --dry-run
./sync_pi.sh
```

Install the Pi-side copy once, and again whenever this script changes:

```bash
./sync_pi.sh --install
```

That writes `/home/invitro/sync_pi.sh` over SSH. The development-machine run
does not need the installed copy; it sends the current script to the Pi each
time.

## Failure behaviour

| Failure | Result |
|---|---|
| SSH unreachable | aborts locally before touching the Pi |
| `git clone`/`fetch` fails | checkout unchanged, deploy dir untouched |
| `web_control.py` does not compile | aborts before rsync |
| `rsync` returns non-zero | script exits with the rsync code |
| PM2 app missing | files still deploy, warning printed |
| no Arduino serial port | file deploy completes, flash skipped, hash unchanged |
| ArduinoJson 7 installed | flash aborts with a version warning |
| master compile fails | flash aborts, nothing uploaded, hash unchanged |
| upload fails twice | PM2 is restarted, script exits non-zero, hash unchanged |
| port still held after PM2 stops | flash aborts, PM2 is restarted, script exits non-zero |
| run outside a git repo (dev machine) | warning, push checks skipped |

The checkout reset is destructive only to `~/rusim-src`; it is a cache and can
be deleted at any time.

## Rollback

Deploy an older commit by checking it out on a branch and pushing that branch,
then running `./sync_pi.sh --branch <branch>`. The script always deploys a
branch tip from `origin`, not an arbitrary local commit.

## Limitations

- Deploys only what is pushed. Uncommitted edits stay local.
- No automatic trigger. Run it yourself after a push.
- The Pi's installed copy at `~/sync_pi.sh` does not update itself. Run
  `./sync_pi.sh --install` after changing the script.
- Local mode on the Pi cannot detect unpushed development commits; it trusts
  `origin`.
- Flashing is automatic when the source hash changes, but it is not a
  correctness check. Replacing the board with identical firmware does not
  change the hash, so use `--force-flash-master` after a hardware swap or a
  failed manual flash.
- The hash covers `master.cpp` and the FQBN only. Changing the compiler,
  libraries, or board variant without changing either will not trigger a
  reflash.
- The board must be connected and idle for the upload. Flashing resets it and
  stops the circulation pump for several seconds; it does not turn the heater
  on, because `setup()` calls `forceAllOff()`.
- The water-level probe is implemented in `master.cpp` with time-gated debouncing
  and dry-run cutoff protection (trips after 5 consecutive readings below threshold).
- `templates/index.html` changes are served on the next page load; the kiosk
  browser must be reloaded manually.
- The Pi's old `~/water-control-system/.git` still points at
  `DextroColdwood/water-control-system`. After a sync that repository will show
  local modifications. It is no longer the source of truth.

---
Content generated by `opencode/deepseek-v4.1-flash-free` in `opencode`.
