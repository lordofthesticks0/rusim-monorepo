# Factory flashing tool (`scripts-hardware/flash-factory.ts`)

Scope: the Bun script that writes a prebuilt full-flash image to the ESP32-S3
display board. It exists because building the factory image from source is slow
and because the same image has to go onto boards that are not physically
attached to the development machine's build environment.

## 1. What it flashes

| Property | Value |
|---|---|
| Default image | `.backup/display_factory.bin` |
| Image size | 16 MiB (16,777,216 bytes) |
| Flash offset | `0x0` |
| Chip | `esp32-s3` |
| Baud | 921600, retried at 460800 |
| Flash mode | `dio`, 80 MHz, 16 MB |
| Tool | `esptool.py` from `platformio/tool-esptoolpy@^2.41100.260830` |

Because the offset is `0x0` and the image is a full-flash dump, this overwrites
the entire 16 MB flash: bootloader, partition table, application, LittleFS, and
NVS keys. It is not a partial application update. Anything stored only on the
board is destroyed.

The image is the same for `[env:display]` and `[env:display-v9]`, because both
environments target `esp32-s3-devkitc-1` with a 16 MB flash. It was captured as a
dump rather than rebuilt, so it also carries the partition layout and boot
settings the board shipped with.

## 2. Running it

```bash
bun scripts-hardware/flash-factory.ts
bun scripts-hardware/flash-factory.ts --port /dev/ttyUSB0
bun scripts-hardware/flash-factory.ts --list-ports
bun scripts-hardware/flash-factory.ts --help
```

Run it from anywhere; the default image path is resolved from the script's own
location, not the working directory.

| Option | Default | Effect |
|---|---|---|
| `--binary <path>` | `.backup/display_factory.bin` | Image to write |
| `--port <port>` | autodetected | Serial port |
| `--baud <baud>` | 921600 | Initial baud; retried at 460800 |
| `--chip <chip>` | `esp32-s3` | Chip passed to esptool |
| `--list-ports` | — | Print all devices and the filtered candidates as JSON, then exit |
| `-h`, `--help` | — | Usage text |

## 3. Port selection

`--port` skips all of this.

Otherwise the script runs `pio device list --json-output` and filters the result:

1. Entries without a `port` field are dropped.
2. Entries under `/dev/ttyS` are dropped. These are internal UARTs, not USB
   adapters.
3. Entries with no `hwid`, or with `hwid` exactly `n/a`, are dropped.

If one candidate survives, it is used. If several survive, they are ranked by a
score that favours the adapters used in this lab:

| Signal | Score |
|---|---|
| `hwid` or description contains `1a86:7523` | +3 |
| description or `hwid` matches CH34, CH341, CP210, FTDI, Silicon Labs, Espressif, esp32, or "usb serial" | +2 |
| port path contains `ttyUSB` or `ttyACM` | +1 |

Each ranked candidate is then probed with `esptool.py chip_id` at 115200 baud.
The first port where an ESP answers is used. The chip name passed to `--chip`
selects the probe's `--chip` argument, so a wrong `--chip` value makes the probe
fail even when the port is correct.

If nothing is usable, or nothing answers the probe, the script exits non-zero
with the list of ports it saw and asks for `--port`.

`--list-ports` is the quickest way to see what the script would consider:

```bash
bun scripts-hardware/flash-factory.ts --list-ports
```

## 4. Flashing and retry

`flash()` first checks that the image exists, and warns if its size is not
16 MiB. The warning does not stop the flash.

The write command is:

```
pio pkg exec -p platformio/tool-esptoolpy@^2.41100.260830 -- \
  esptool.py --chip esp32-s3 --port <port> --baud <baud> \
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 <binary>
```

`-z` compresses the data on the wire. `--flash_mode dio`, `--flash_freq 80m`,
and `--flash_size 16MB` must match the physical parts, or the image will boot and
then misread itself.

If the write exits non-zero, the script retries once at 460800 baud. A failure at
921600 on a long or poor cable is common; a failure at both rates is a different
problem. If the second attempt also fails, the script throws and exits 1.

The tool prints the full esptool command before running it, so the exact
invocation can be copied and re-run by hand.

## 5. Requirements

- `bun`, because the script uses `Bun.spawn`, `Bun.spawnSync`, `Bun.file`, and
  `import.meta.dir`. Node will not run it.
- `pio` on `PATH`, for both `pio device list` and `pio pkg exec`.
- The esptool package installed under that PlatformIO version, which
  `pio pkg exec` fetches on demand.

## 6. Local assets

`display_factory.sha256` sits next to the image and holds its digest:

```
90bdef73475e88624c327f668a7c957fdbc76231999ee0ed75e4f3abdb65c03b
```

Verify before flashing a copy you did not just produce:

```bash
cd .backup && sha256sum -c display_factory.sha256
```

`.backup/` is listed in the root `.gitignore`, so neither the image nor its
digest is committed. The 16 MiB binary is present on the development machine
only, and the flashing script has nothing to write on a fresh clone until someone
captures the image again. Treat the capture as the irreplaceable step: see
`SUNTON-luna.md` for how the factory image is produced on the display board.

## 7. Known gaps

1. **No post-flash verification.** The script does not run `chip_id` or read back
   the image afterwards, so a zero exit code means esptool accepted the write, not
   that the board boots.
2. **The image is not committed.** `.backup/` is git-ignored, so the binary and
   its digest exist only on the development machine. A fresh clone has nothing to
   flash until the image is captured again. This is deliberate: 16 MiB does not
   belong in Git. See §6.

Two earlier problems have been fixed. The usage text named
`bun scripts/flash-factory.ts`, which is not where the file lives, and
`REPO_ROOT` only recognised a `/scripts` suffix, so it fell back to
`process.cwd()` and produced a wrong default image path whenever the script was
run from outside the repo root. Both now refer to `scripts-hardware/`, and the
repository root is taken from the script's own location.

## 8. Related documents

- `platformio-environments.md` — the display environments this image matches, and
  why a full-flash dump is the right artefact for them.
- `display-docs.md` — the UI that runs on the flashed board.
- `SUNTON-luna.md` — factory firmware and flashing for the display board.
- `subsystem-index.md` — the rest of the repo.

---
Content generated by `opencode/space-bunny-free` in `opencode`.