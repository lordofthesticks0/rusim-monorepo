# Cross-compile environment for `leaf-reader` -> Raspberry Pi (aarch64, Debian 13).
#
# The Pi is a 1 GB RAM quad Cortex-A53; building rustc + MuPDF there takes hours.
# This image builds the same binary on an x86_64 machine in about two minutes.
# Ubuntu 24.04 provides glibc 2.39, which is older than the Pi's 2.41, so the
# resulting binary runs there without complaints.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# The mupdf crate links against fontconfig, so arm64 development packages must
# be available. They only exist under the ports.ubuntu.com archive, hence the
# explicit multiarch sources below.
RUN dpkg --add-architecture arm64 \
    && rm -f /etc/apt/sources.list /etc/apt/sources.list.d/*.sources \
    && printf '%s\n' \
        'deb [arch=amd64] http://archive.ubuntu.com/ubuntu noble main universe' \
        'deb [arch=amd64] http://archive.ubuntu.com/ubuntu noble-updates main universe' \
        'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports noble main universe' \
        'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports noble-updates main universe' \
        > /etc/apt/sources.list

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        gcc-aarch64-linux-gnu \
        g++-aarch64-linux-gnu \
        binutils-aarch64-linux-gnu \
        clang \
        libclang-dev \
        make \
        cmake \
        pkg-config \
        ca-certificates \
        git \
        libfontconfig1-dev:arm64 \
        libfreetype-dev:arm64 \
        libexpat1-dev:arm64 \
    && rm -rf /var/lib/apt/lists/*

# pkg-config must report target libraries only, never the amd64 ones.
ENV PKG_CONFIG_ALLOW_CROSS=1
ENV PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
ENV PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig
