#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
gate_a="${1:?usage: build-full-stack-after-gate-a.sh <gate-a.json>}"
python3 - "$gate_a" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
if data.get("gate") != "A" or data.get("pass") is not True:
    raise SystemExit("refusing full-stack build: verified Gate A result required")
PY
source "$root/integration/pins.env"
echo "Gate A verified; full-stack fetch/build is now permitted."
"$root/scripts/fetch-pinned.sh"
build="$root/build/upstream"
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y meson ninja-build
meson setup "$build/open5gs" "$root/vendor/open5gs" --buildtype=release
ninja -C "$build/open5gs"
(cd "$root/vendor/software-ue" && ./cmake_targets/build_oai --ninja --nrUE -w SIMU)
