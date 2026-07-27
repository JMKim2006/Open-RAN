#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$1"
xapp="$(find "$root/build/upstream/flexric" -type f \
  \( -name 'xapp_kpm_moni' -o -name 'xapp_kpm*monitor*' \) -perm -111 -print -quit)"
test -n "$xapp"
export ASAN_OPTIONS=abort_on_error=1:detect_leaks=0:symbolize=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
ulimit -c unlimited 2>/dev/null || true
{
  printf 'xapp='; printf '%q ' "$xapp"; printf '\n'
  printf 'ASAN_OPTIONS=%s\n' "$ASAN_OPTIONS"
  printf 'UBSAN_OPTIONS=%s\n' "$UBSAN_OPTIONS"
  printf 'core_limit='; ulimit -c
  printf 'core_pattern='; cat /proc/sys/kernel/core_pattern
} > "$out/xapp-command.txt"
ldd "$xapp" > "$out/loaded-shared-libraries.txt" 2>&1

set +e
timeout 60 stdbuf -oL -eL "$xapp" \
  > >(tee "$out/logs/kpm-xapp-asan-ubsan.log") 2>&1
asan_rc=$?
timeout 60 stdbuf -oL -eL gdb -q -batch \
  -ex "set pagination off" \
  -ex run \
  -ex "info sharedlibrary" \
  -ex "thread apply all bt full" \
  --args "$xapp" \
  > >(tee "$out/logs/kpm-xapp-gdb.log") 2>&1
gdb_rc=$?
set -e
cat "$out/logs/kpm-xapp-asan-ubsan.log" "$out/logs/kpm-xapp-gdb.log" \
  > "$out/logs/kpm-xapp.log"
find . /tmp -maxdepth 2 -type f -name 'core*' -exec cp -n {} "$out/" \; 2>/dev/null || true
printf 'asan_exit=%s\ngdb_exit=%s\n' "$asan_rc" "$gdb_rc" > "$out/xapp-exit-codes.txt"
exit "$asan_rc"
