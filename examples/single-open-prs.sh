#!/usr/bin/env bash
set -euo pipefail

if [ $# -ne 1 ]; then
  echo "Usage: $0 OWNER/REPO" >&2
  exit 1
fi

exec autogithubpullmerge --single-open-prs "$1"

