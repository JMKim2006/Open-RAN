#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$1"
xapp="$(find "$root/build/upstream/flexric" -type f \
  \( -name 'xapp_kpm_moni' -o -name 'xapp_kpm*monitor*' \) -perm -111 -print -quit)"
test -n "$xapp"
export ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:symbolize=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
ulimit -c 0
{
  printf 'xapp='; printf '%q ' "$xapp"; printf '\n'
  printf 'ASAN_OPTIONS=%s\n' "$ASAN_OPTIONS"
  printf 'UBSAN_OPTIONS=%s\n' "$UBSAN_OPTIONS"
  printf 'core_limit='; ulimit -c
} > "$out/xapp-command.txt"
if command -v ldd >/dev/null 2>&1; then
  ldd "$xapp" > "$out/loaded-shared-libraries.txt" 2>&1
else
  cp "$root/build/upstream/flexric/examples/xApp/c/monitor/CMakeFiles/xapp_kpm_moni.dir/link.txt" \
    "$out/xapp-link-command.txt" 2>/dev/null || true
fi

set +e
timeout 60 "$xapp" > "$out/logs/kpm-xapp.log" 2>&1
xapp_rc=$?
set -e
signal=0
if (( xapp_rc > 128 )); then
  signal=$((xapp_rc - 128))
fi
cp "$out/logs/kpm-xapp.log" "$out/logs/kpm-xapp-asan-ubsan.log"
printf 'exit_code=%s\nsignal=%s\n' "$xapp_rc" "$signal" > "$out/xapp-exit-codes.txt"
exit "$xapp_rc"
