#!/usr/bin/env bash
set -e
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libcurl4-openssl-dev \
    libsqlite3-dev \
    libcli11-dev \
    libyaml-cpp-dev \
    nlohmann-json3-dev
