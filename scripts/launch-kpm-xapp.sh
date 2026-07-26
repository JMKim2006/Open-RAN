#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$1"
xapp="$(find "$root/build/upstream/flexric" -type f \
  \( -name 'xapp_kpm_moni' -o -name 'xapp_kpm*monitor*' \) -perm -111 -print -quit)"
test -n "$xapp"
exec timeout 60 stdbuf -oL -eL gdb -q -batch \
  -ex run -ex "thread apply all bt" --args "$xapp" \
  2>&1 | tee "$out/logs/kpm-xapp.log"
