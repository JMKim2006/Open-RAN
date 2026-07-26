#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
echo "Compatibility entry point: building only the Gate A minimal OCUDU/FlexRIC set."
exec "$root/scripts/build-hosted-minimal.sh"
