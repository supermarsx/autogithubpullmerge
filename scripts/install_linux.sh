#!/usr/bin/env bash
set -e
# Install packages required for building agpm including ncurses for the TUI
sudo apt-get update
sudo apt-get install -y build-essential cmake git libcurl4-openssl-dev libpsl-dev libsqlite3-dev libspdlog-dev libncurses-dev
