#!/usr/bin/env bash
# Utility helpers for build and compile scripts

ensure_tool() {
	local tool="$1"
	if command -v "$tool" >/dev/null 2>&1; then
		return
	fi
	echo "[WARN] $tool not found in PATH. Attempting auto-detect..."
	local found
	found=$(find /usr/local/bin /usr/bin "$HOME/.local/bin" "$HOME/bin" -maxdepth 3 -type f -name "$tool" 2>/dev/null | head -n 1)
	if [[ -n "$found" ]]; then
		local dir
		dir=$(dirname "$found")
		echo "[INFO] Adding $dir to PATH for $tool"
		export PATH="$dir:$PATH"
	else
		echo "[ERROR] $tool not found. Please install $tool."
		exit 1
	fi
}
