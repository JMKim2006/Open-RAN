#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/build/upstream"
mkdir -p "$build"
cmake -S "$root/vendor/ocudu" -B "$build/ocudu" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build "$build/ocudu" --target gnb --parallel 2
cmake -S "$root/vendor/flexric" -B "$build/flexric" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DXAPP_DB=SQLITE3_XAPP \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build "$build/flexric" --parallel 2
sudo cmake --install "$build/flexric"
