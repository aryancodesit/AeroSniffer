#!/usr/bin/env bash
set -e
if ! command -v python3 &>/dev/null; then
    echo "error: python3 not found — install Python 3.11+ from https://python.org"
    exit 1
fi
cd "$(dirname "$0")/.."
exec python3 tools/releng.py install
