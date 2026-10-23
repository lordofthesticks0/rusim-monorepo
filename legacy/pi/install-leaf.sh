#!/usr/bin/env bash
# Build the `leaf` terminal reader for the Raspberry Pi on this machine and
# install it to ~/.cargo/bin/leaf on the Pi.
#
# The Pi (1 GB RAM, Cortex-A53) is too slow to compile this crate: it builds
# MuPDF from C source and compiles roughly 200 Rust dependencies. The build
# below runs on the x86_64 host inside Docker and takes about two minutes.
#
# Usage: ./install-leaf.sh
#   PI_HOST        target host          (default invitro@10.92.128.23)
#   CRATE_VERSION  leaf-reader version  (default 0.2.0)
set -euo pipefail

PI_HOST=${PI_HOST:-invitro@10.92.128.23}
CRATE_VERSION=${CRATE_VERSION:-0.2.0}
TARGET=aarch64-unknown-linux-gnu
IMAGE=leaf-cross-aarch64:2
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
CACHE_DIR=${XDG_CACHE_HOME:-$HOME/.cache}/leaf-cross
SOURCE_DIR=$CACHE_DIR/src/leaf-reader-$CRATE_VERSION
BINARY=$CACHE_DIR/target/$TARGET/release/leaf

echo "==> Building toolchain image $IMAGE"
docker build -f "$SCRIPT_DIR/leaf-cross.Dockerfile" -t "$IMAGE" "$SCRIPT_DIR"

echo "==> Fetching leaf-reader $CRATE_VERSION"
mkdir -p "$CACHE_DIR/src" "$CACHE_DIR/target"
if [ ! -d "$SOURCE_DIR" ]; then
    tmp=$(mktemp -d)
    curl -fsSL "https://static.crates.io/crates/leaf-reader/leaf-reader-$CRATE_VERSION.crate" \
        -o "$tmp/leaf.crate"
    tar xzf "$tmp/leaf.crate" -C "$CACHE_DIR/src"
    rm -rf "$tmp"
fi

echo "==> Adding Rust target $TARGET"
rustup target add "$TARGET" >/dev/null

echo "==> Cross-compiling"
# The std toolchain is mounted from the host instead of being downloaded again.
# LD is overridden because MuPDF's makefile shells out to $(LD) -r -b binary to
# embed its default fonts; the host ld would emit x86_64 objects.
docker run --rm \
    -u "$(id -u):$(id -g)" \
    -v "$HOME/.rustup:/rustup" \
    -v "$HOME/.cargo:/cargo" \
    -v "$SOURCE_DIR:/work" \
    -v "$CACHE_DIR/target:/cache" \
    -w /work \
    -e HOME=/tmp \
    -e RUSTUP_HOME=/rustup \
    -e CARGO_HOME=/cargo \
    -e CARGO_TARGET_DIR=/cache \
    -e PATH=/cargo/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    -e PKG_CONFIG_ALLOW_CROSS=1 \
    -e PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig \
    -e PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig \
    -e CC_aarch64_unknown_linux_gnu=aarch64-linux-gnu-gcc \
    -e CXX_aarch64_unknown_linux_gnu=aarch64-linux-gnu-g++ \
    -e AR_aarch64_unknown_linux_gnu=aarch64-linux-gnu-ar \
    -e RANLIB_aarch64_unknown_linux_gnu=aarch64-linux-gnu-ranlib \
    -e LD=aarch64-linux-gnu-ld \
    -e CARGO_TARGET_AARCH64_UNKNOWN_LINUX_GNU_LINKER=aarch64-linux-gnu-gcc \
    -e 'BINDGEN_EXTRA_CLANG_ARGS_aarch64-unknown-linux-gnu=--target=aarch64-linux-gnu --sysroot=/usr/aarch64-linux-gnu -I/usr/aarch64-linux-gnu/include' \
    -e 'BINDGEN_EXTRA_CLANG_ARGS_aarch64_unknown_linux_gnu=--target=aarch64-linux-gnu --sysroot=/usr/aarch64-linux-gnu -I/usr/aarch64-linux-gnu/include' \
    "$IMAGE" bash -lc "cargo build --release --target $TARGET"

echo "==> Installing to $PI_HOST:~/.cargo/bin/leaf"
scp -q "$BINARY" "$PI_HOST:~/.cargo/bin/leaf"
ssh "$PI_HOST" 'chmod 755 ~/.cargo/bin/leaf'

echo "==> Verifying"
ssh "$PI_HOST" '~/.cargo/bin/leaf --help | head -1'
