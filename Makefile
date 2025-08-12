OS := $(shell uname 2>/dev/null || echo Windows_NT)

ifeq ($(OS),Windows_NT)
    SHELL := cmd.exe
    .SHELLFLAGS := /C
    UPDATE_DEPS := scripts\\update_libs.bat
    BUILD_SCRIPT := scripts\\build_win.bat
    INSTALL_SCRIPT := scripts\\install_win.bat
else ifeq ($(OS),Darwin)
    UPDATE_DEPS := bash scripts/update_libs.sh
    BUILD_SCRIPT := bash scripts/build_mac.sh
    INSTALL_SCRIPT := bash scripts/install_mac.sh
else
    UPDATE_DEPS := bash scripts/update_libs.sh
    BUILD_SCRIPT := bash scripts/build_linux.sh
    INSTALL_SCRIPT := bash scripts/install_linux.sh
endif

.PHONY: all update-deps build install

all: build

update-deps:
	$(UPDATE_DEPS)

build:
	$(BUILD_SCRIPT)

install:
	$(INSTALL_SCRIPT)
