#!/usr/bin/env bash
#
# gen-compile-db.sh - build one merged compile_commands.json for the PlatformIO
# environments in this repository.
#
# Why this exists: `pio run -e <env> -t compiledb` writes compile_commands.json
# for exactly ONE environment. PlatformIO runs a separate SCons process per
# environment and SCons opens the output file with mode "w", so each run
# overwrites the previous one - passing several -e flags does not merge. A
# single environment's database only gives clangd correct flags for that
# environment's sources, so everything else (other firmware/ directories,
# legacy/slave) falls back to guessed flags and unresolved headers.
#
# This script runs the target once per environment, keeps each environment's
# snapshot, and concatenates them into a single database.
#
# Usage:
#   ./gen-compile-db.sh                    # every [env:*] in platformio.ini
#   ./gen-compile-db.sh legacy-slave blink-test
#
# It never compiles firmware: the compilation database target rebuilds its
# entries regardless of whether the objects are up to date, so a fully cached
# or never-built environment costs about the same - only SCons graph setup.
#
# Exit status: 0 when every requested environment produced entries, 1 when at
# least one failed (the merged database still contains the ones that succeeded).

set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR" || exit 1

DB="$PROJECT_DIR/compile_commands.json"

if (($#)); then
  ENVS=("$@")
else
  # Sections are returned as ["env:<name>", [[option, value], ...]]; take the
  # name from every env section, skipping the shared [env] section.
  mapfile -t ENVS < <(pio project config --json-output | python3 -c '
import json, sys
for section, _ in json.load(sys.stdin):
    if section.startswith("env:"):
        print(section[4:])
')
fi

if ((${#ENVS[@]} == 0)); then
  echo "No [env:*] sections found in platformio.ini." >&2
  exit 1
fi

WORKDIR="$(mktemp -d /tmp/opencode/gen-compile-db.XXXXXX)" || exit 1
trap 'rm -rf "$WORKDIR"' EXIT

# A failed run may leave a truncated file behind; keep the old database so it
# can be restored when nothing at all succeeds.
[[ -f "$DB" ]] && cp "$DB" "$WORKDIR/previous.json"

SUCCEEDED=()
FAILED=()

for env in "${ENVS[@]}"; do
  printf '%-28s ' "[$env]"
  if pio run -e "$env" -t compiledb >"$WORKDIR/$env.log" 2>&1; then
    count="$(python3 -c 'import json, sys; print(len(json.load(open(sys.argv[1]))))' "$DB" 2>/dev/null || echo '?')"
    # pio just overwrote the root file with this environment's entries: stash
    # them before the next environment does the same.
    mv "$DB" "$WORKDIR/$env.json"
    echo "ok, $count entries"
    SUCCEEDED+=("$env")
  else
    echo "FAILED"
    sed 's/^/    /' "$WORKDIR/$env.log" | tail -n 15
    FAILED+=("$env")
  fi
done

if ((${#SUCCEEDED[@]} == 0)); then
  [[ -f "$WORKDIR/previous.json" ]] && cp "$WORKDIR/previous.json" "$DB"
  echo "No environment succeeded; compile_commands.json left unchanged." >&2
  exit 1
fi

python3 - "$WORKDIR" "$DB" "${SUCCEEDED[@]}" <<'PY'
import json
import os
import sys

workdir, output = sys.argv[1], sys.argv[2]
envs = sys.argv[3:]

entries = []
seen = set()
for env in envs:
    with open(os.path.join(workdir, f"{env}.json")) as fh:
        for entry in json.load(fh):
            # The same source is compiled once per environment with different
            # flags (different -mmcu, different lib_deps). Keep every distinct
            # command; only drop byte-identical repeats.
            key = (entry["file"], entry["command"], entry.get("output", ""))
            if key in seen:
                continue
            seen.add(key)
            entries.append(entry)

# Stable order, so the database does not churn between runs and clangd picks
# the same command for a duplicated file every time.
entries.sort(key=lambda e: (e["file"], e.get("output", "")))

with open(output, "w") as fh:
    json.dump(entries, fh, indent=4, sort_keys=True)
    fh.write("\n")

print(f"\nWrote {len(entries)} entries from {len(envs)} environment(s):")
for env in envs:
    print(f"  {env}")
PY

echo
echo "Restart clangd (reload the editor window) to pick up the new database."

if ((${#FAILED[@]})); then
  echo "Missing entries for: ${FAILED[*]}" >&2
  exit 1
fi
