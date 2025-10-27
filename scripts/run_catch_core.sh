#!/bin/sh
set -eu

: "${AGPM_FAST_TESTS:=1}"
export AGPM_FAST_TESTS

BIN=${1:-./tests/agpm_tests}

# Build a list of tests excluding those tagged [tui] or [cli]
tests_list=$("${BIN}" --list-tests -v high | awk '
  BEGIN { curr=""; skip=0; }
  /^All available test cases:/ { next }
  /^[[:digit:]]+ test cases$/ { if (curr!="" && skip==0) print curr; exit }
  /^  \/.*/ { next } # absolute path line
  /^      \[.*\]/ { if ($0 ~ /\[tui\]/ || $0 ~ /\[cli\]/) skip=1; next }
  /^    / { next }   # deeper-indented file or path info
  /^  [^ ]/ {
    name=substr($0,3);
    if (curr!="" && skip==0) print curr;
    curr=name; skip=0; next
  }
  END { if (curr!="" && skip==0) print curr; }
')

fail=0
tmpfile=$(mktemp)
printf "%s\n" "$tests_list" > "$tmpfile"
while IFS= read -r name; do
  [ -z "$name" ] && continue
  echo "[RUN] $name"
  if "$BIN" "$name" -v quiet; then
    echo "[ OK] $name"
  else
    echo "[FAIL] $name" >&2
    fail=1
  fi
done < "$tmpfile"
rm -f "$tmpfile"

exit $fail
