#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$1"
gnb="$(find "$root/build/upstream/ocudu" -type f \( -name gnb -o -name ocudu_gnb \) -perm -111 -print -quit)"
test -n "$gnb"
exec sudo timeout 140 stdbuf -oL -eL "$gnb" -c "$root/integration/gnb-hosted-testmode.yml" \
  2>&1 | tee "$out/logs/ocudu.log"
