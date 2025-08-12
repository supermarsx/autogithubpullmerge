#!/usr/bin/env bash
set -e
# Install packages required for building agpm including ncurses for the TUI
sudo apt-get update
sudo apt-get install -y build-essential cmake git \
    libcurl4-openssl-dev libpsl-dev libsqlite3-dev libspdlog-dev libncurses-dev \
    libev-dev libc-ares-dev zlib1g-dev libbrotli-dev libssl-dev \
    libngtcp2-dev libnghttp3-dev libsystemd-dev libjansson-dev \
    libevent-dev libxml2-dev libjemalloc-dev
