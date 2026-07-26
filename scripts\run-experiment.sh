#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; OUT="${1:-$ROOT/evidence/$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "$OUT/logs"
command -v docker >/dev/null && docker info >/dev/null || { echo "Docker unavailable"; exit 3; }
grep -qw sctp /proc/net/protocols || { echo "SCTP unavailable"; exit 4; }
"$ROOT/scripts/fetch-pinned.sh"
"$ROOT/scripts/build-pinned.sh"
for component in ocudu flexric open5gs software-ue; do
  test -d "$ROOT/vendor/$component/.git" || { echo "missing pinned $component source"; exit 5; }
  git -C "$ROOT/vendor/$component" rev-parse HEAD >>"$OUT/commits.txt"
  git -C "$ROOT/vendor/$component" status --porcelain=v1 >>"$OUT/dirty-trees.txt"
done
if [[ ! -x "$ROOT/integration/run-stock-e2-local.sh" ]]; then
  echo "No reviewed site-specific stock-E2 deployment adapter; refusing to fabricate evidence." | tee "$OUT/summary.txt"
  find "$OUT" -type f -print0 | sort -z | xargs -0 sha256sum >"$OUT/SHA256SUMS"
  exit 6
fi
"$ROOT/integration/run-stock-e2-local.sh" "$OUT" 2>&1 | tee "$OUT/logs/orchestrator.log"
find "$OUT" -type f -print0 | sort -z | xargs -0 sha256sum >"$OUT/SHA256SUMS"
