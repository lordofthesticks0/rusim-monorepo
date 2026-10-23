# `leaf` terminal reader on the Pi

`leaf` (crate `leaf-reader`) is a terminal reader for PDFs, markdown files and web
articles. Version 0.2.0 was installed on `invitrocontroller` on 2026-10-23.

| Item | Value |
|---|---|
| Binary | `/home/invitro/.cargo/bin/leaf` |
| Size | 19 MB, ELF aarch64, dynamically linked |
| Crate version | `leaf-reader` 0.2.0 (bin name `leaf`) |
| Reading progress database | `~/.local/share/leaf/leaf.db` (SQLite) |
| Built on | the x86_64 development machine, not on the Pi |
| Rebuild | `legacy/pi/install-leaf.sh` |

## Why it is not compiled on the Pi

`leaf-reader` builds MuPDF from C source (`mupdf-sys`) and compiles about 200
Rust crates. On the Pi's quad-core Cortex-A53 with 1 GB of RAM this takes hours,
and the 1 GB limit makes it likely to fail outright. The same build takes about
two minutes on the development machine.

## How the cross-compile works

The build runs inside Docker so the host does not need an aarch64 sysroot.

| Piece | Choice | Reason |
|---|---|---|
| Build container | `ubuntu:24.04` (`legacy/pi/leaf-cross.Dockerfile`) | glibc 2.39, older than the Pi's 2.41, so the binary needs nothing newer than what the Pi has |
| C compiler | `aarch64-linux-gnu-gcc` / `g++` / `ar` / `ranlib` / `ld` from `gcc-aarch64-linux-gnu` | MuPDF and bundled SQLite are C code |
| Bindings | `clang` + `libclang-dev` (LLVM 18) | `mupdf-sys` generates its bindings with bindgen 0.72 at build time, and bindgen needs libclang 9 or later |
| Target headers | `--sysroot=/usr/aarch64-linux-gnu` in `BINDGEN_EXTRA_CLANG_ARGS` | bindgen would otherwise parse the amd64 headers |
| Target libraries | `libfontconfig1-dev:arm64`, `libfreetype-dev:arm64`, `libexpat1-dev:arm64` via arm64 multiarch | the `mupdf` crate links `libfontconfig.so.1`; the Pi already has the runtime versions |
| Rust toolchain | the host's `~/.rustup`, mounted read-write | avoids downloading a second toolchain and lets `rustup target add aarch64-unknown-linux-gnu` write to the same place |

Two details break the build if they are missed:

1. **`LD` must be the aarch64 linker.** MuPDF's `Makefile` turns its bundled
   default fonts into object files with `$(LD) -r -b binary`. GNU make defaults
   `LD` to the host `ld`, which emits x86_64 objects and makes the final link
   fail. The script passes `LD=aarch64-linux-gnu-ld`.
2. **`PKG_CONFIG_LIBDIR` must point at the arm64 directory only.**
   With multiarch enabled, amd64 and arm64 `.pc` files are both installed.
   Restricting the search path to `/usr/lib/aarch64-linux-gnu/pkgconfig` stops
   fontconfig from resolving to the host library.

The first attempt used the `cross` tool with its `aarch64-unknown-linux-gnu:0.2.5`
image. That image is built on Ubuntu 16.04 and only contains libclang 3.8, which
bindgen 0.72 refuses. Pointing `cross` at a newer image would have needed the
same Dockerfile work, so the build uses a plain container instead.

## Verification

The binary was checked before and after copying it:

- `file` reports `ELF 64-bit LSB pie executable, ARM aarch64`.
- `readelf -V` shows a maximum required symbol version of `GLIBC_2.39`; the Pi
  runs glibc 2.41.
- `readelf -d` lists `libfontconfig.so.1`, `libfreetype.so.6`, `libgcc_s.so.1`,
  `libm.so.6` and `libc.so.6`. All five exist on the Pi, and SQLite is compiled
  into the binary (`rusqlite` with the `bundled` feature), so no extra package
  is needed there.
- A markdown file and a one-page PDF were opened through a pseudo-terminal on
  the Pi. The markdown body rendered with its heading, the PDF rendered its text
  and the footer read `1 / 1 · 100%`.

## Running it

`~/.bashrc` and `~/.profile` on the Pi source `~/.cargo/env`, so `leaf` is on
`PATH` in an interactive login shell. A non-interactive command such as
`ssh invitro@10.92.128.23 'leaf open x.pdf'` does **not** read `.bashrc`; use the
full path there.

```sh
leaf open paper.pdf
leaf open README.md
leaf open https://example.com/post
```

Keys: `←/→` or `h/l` or `p/n` page, `j/k` scroll, `d/u` half screen, `space/b`
screen, `g/G` first or last page, `t` go to page, `i` painted images,
`f` find, `n/N` next or previous match, drag to select and copy, `y` highlight,
`?` all keys, `q` quit.

Progress and highlights are stored per file path or URL in
`~/.local/share/leaf/leaf.db`.

## Rebuilding and removing

```sh
cd legacy/pi
./install-leaf.sh                       # builds and copies the binary
PI_HOST=invitro@10.92.128.23 ./install-leaf.sh
CRATE_VERSION=0.3.0 ./install-leaf.sh   # other release
```

The script caches the crate source and the target directory under
`~/.cache/leaf-cross`, so a rebuild after a source change takes seconds.
Remove the binary and its data with:

```sh
ssh invitro@10.92.128.23 'rm -f ~/.cargo/bin/leaf && rm -rf ~/.local/share/leaf'
```

The Docker image (`leaf-cross-aarch64:2`, about 1.4 GB) and the unused
`ghcr.io/cross-rs/aarch64-unknown-linux-gnu:0.2.5` image from the first attempt
can be deleted with `docker image rm`; the next run of the script rebuilds the
former.

---
Content generated by `opencode/deepseek-v4.1-flash-free` in `opencode`.
