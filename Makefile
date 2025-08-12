OS := $(shell uname 2>/dev/null || echo Windows_NT)

ifeq ($(OS),Windows_NT)
    SHELL := cmd.exe
    .SHELLFLAGS := /C
    BUILD_SCRIPT := scripts\\build_win.bat
    INSTALL_SCRIPT := scripts\\install_win.bat
else ifeq ($(OS),Darwin)
    BUILD_SCRIPT := bash scripts/build_mac.sh
    INSTALL_SCRIPT := bash scripts/install_mac.sh
else
    BUILD_SCRIPT := bash scripts/build_linux.sh
    INSTALL_SCRIPT := bash scripts/install_linux.sh
endif

.PHONY: all build install

all: build

build:
	$(BUILD_SCRIPT)

install:
	$(INSTALL_SCRIPT)
