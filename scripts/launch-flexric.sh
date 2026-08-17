#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$1"
ric="$(find "$root/build/upstream/flexric" -type f -name nearRT-RIC -perm -111 -print -quit)"
test -n "$ric"
# Do not wrap an ASan-linked binary with stdbuf: libstdbuf is injected through
# LD_PRELOAD ahead of libasan and causes the RIC to terminate before E2 Setup.
exec timeout 150 "$ric" 2>&1 | tee "$out/logs/flexric.log"
