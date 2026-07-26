#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
echo "Compatibility entry point: running the hosted minimal Gate A experiment."
exec "$root/scripts/run-hosted-minimal-e2.sh" "${1:-$root/evidence/hosted-minimal-e2}"
