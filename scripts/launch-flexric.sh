#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$1"
ric="$(find "$root/build/upstream/flexric" -type f -name nearRT-RIC -perm -111 -print -quit)"
test -n "$ric"
exec timeout 150 stdbuf -oL -eL "$ric" 2>&1 | tee "$out/logs/flexric.log"
