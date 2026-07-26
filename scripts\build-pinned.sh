#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; B="$ROOT/build/upstream"
mkdir -p "$B"
cmake -S "$ROOT/vendor/ocudu" -B "$B/ocudu" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$B/ocudu" --parallel
cmake -S "$ROOT/vendor/flexric" -B "$B/flexric" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$B/flexric" --parallel
meson setup "$B/open5gs" "$ROOT/vendor/open5gs" --buildtype=release
ninja -C "$B/open5gs"
(cd "$ROOT/vendor/software-ue" && ./cmake_targets/build_oai --ninja --nrUE -w SIMU)
