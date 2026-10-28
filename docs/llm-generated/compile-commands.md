# compile_commands.json and gen-compile-db.sh

How this repository produces the compilation database that clangd reads, why a
single PlatformIO command is not enough, and what the database can and cannot
fix.

## 1. What the file is

`compile_commands.json` at the repository root is a **compilation database**, a
JSON array with one entry per compiled source file. Each entry records the exact
compiler invocation that produced one object file:

| Field | Meaning |
|---|---|
| `command` | The full `avr-g++` / `xtensa-esp32-elf-g++` command line |
| `directory` | Working directory the command runs from, so relative `-I` paths resolve |
| `file` | The source file, the key tools look a file up by |
| `output` | The `.o` file the command produces |

Nothing executes this file. clangd (and `clang-tidy`) read it to learn, for each
source file, which include paths (`-I`), macros (`-D`), C/C++ standard and target
(`-mmcu=atmega2560`) apply. Without an entry, clangd guesses flags and headers
such as `<OneWire.h>` show as unresolved.

## 2. Why one PlatformIO command does not produce it

`pio run -e <env> -t compiledb` writes `compile_commands.json` for **one**
environment. Two facts about PlatformIO turn that into a trap:

1. PlatformIO runs a **separate SCons process per environment**
   (`platformio/run/cli.py` loops `process_env` over the selected environments).
2. SCons' `compilation_db` tool opens the output file with mode `"w"`
   (`SCons/Tool/compilation_db.py`, `write_compilation_db`), and the entry list
   it writes is a per-process global.

So `pio run -e <env-a> -e <env-b> -t compiledb` does not merge the two
environments. Each process writes its own entries and the last environment
processed leaves its file behind. Before this repository had a generator
script, the checked state contained entries from only one environment, which is
why `legacy/slave/new-slave.cpp` had no entry and clangd could not resolve
`OneWire.h`, `DallasTemperature.h`, `HX711.h` or `ArduinoJson.h`.

## 3. The generator script

`gen-compile-db.sh` at the repository root runs the `compiledb` target once per
environment, keeps each environment's snapshot, and concatenates them.

```bash
./gen-compile-db.sh                            # every [env:*] in platformio.ini
./gen-compile-db.sh legacy-slave network-master # only these environments
```

Behaviour:

- Environment list comes from `pio project config --json-output` when no
  arguments are given, so a new `[env:*]` section is picked up automatically.
- **It never compiles firmware.** The database target rebuilds its entries on
  every run (`AlwaysBuild` plus a decider that always returns true) using only
  SCons graph setup.  A fully cached environment and a never-built one cost about
  the same: the full 16-environment run takes roughly 13 seconds.
- Entries are deduplicated on the exact `(file, command, output)` tuple, then
  sorted by `(file, output)` so the database does not churn between runs and
  clangd sees a stable order.
- If an environment fails (missing platform package, broken `platformio.ini`),
  its log is printed, the remaining environments still contribute, and the exit
  status is 1. If nothing succeeds, the previous `compile_commands.json` is
  restored unchanged.

Regenerate after any change to `platformio.ini` — new environment, changed
`lib_deps`, changed `build_flags` — and after adding a new directory under
`firmware/`. Ordinary source edits do not need it. Reload the editor window
after regenerating; clangd reads the file once at startup.

## 4. What the database fixes, and what it does not

Verified with `clangd --check` (clangd 22) after a full `./gen-compile-db.sh`
run.

**ATmega environments are clean.** Every `atmelavr` file parses with no
diagnostics. `legacy/slave/new-slave.cpp` resolves to the real AVR target
(`-triple avr`, `-target-cpu atmega2560`) with all four library include paths:

```
-I .pio/libdeps/legacy-slave/ArduinoJson/src
-I .pio/libdeps/legacy-slave/HX711
-I .pio/libdeps/legacy-slave/DallasTemperature
-I .pio/libdeps/legacy-slave/OneWire
```

ArduinoJson contributes include paths only; it is header-only and so has no
entries of its own in the database.

**ESP32 environments are only partly clean, and that is not fixable here.**
clang has no xtensa backend, so for `display-v9`, `network-master` and the
`wt32-eth01` environments it drops the xtensa target flags and compiles toward
the host. Include paths, defines and the C++ standard are correct, but three
classes of diagnostic remain:

- Unknown-argument warnings for xtensa-only flags (`-mlongcalls`,
  `fstrict-volatile-bitfields`).
- `'machine/endian.h' file not found`: ESP-IDF headers assume a newlib
  sysroot that host clang does not have.
- A cascade of `no matching member function for call to 'println'` and
  `cannot initialize object parameter of type 'Print'`, which follows from the
  Arduino `Print` header failing to parse against host libc headers.

Treat those as noise. Errors inside `~/.platformio/packages/framework-arduinoespressif32/`
are noise too. Errors in the project's own `.cpp` and `.h` files under
`firmware/` are real.

**Duplicate files.** Shared framework source can appear once per environment,
with that environment's flags. clangd uses one matching command per file; the
sort keeps that choice stable across runs. If completion looks wrong, check
which environment's entry clangd picked before blaming the code.

**Files outside every environment** — `legacy/pi/` (shell scripts and the
mirrored `water-control-system/` tree, which includes the Arduino sketch and
`web_control.py`) — get no entry and therefore guessed flags. `legacy/master/`
is covered by `[env:legacy-master]`, and `legacy/slave/` by `[env:legacy-slave]`.

## 5. Git status

`compile_commands.json` is a build artifact regenerated by the script; it never
needs to be committed. [`.gitignore`](../../.gitignore) lists it, and the file
is untracked in a clean checkout.

---

Content generated by `openai/gpt-6-codex` in `harness`.
