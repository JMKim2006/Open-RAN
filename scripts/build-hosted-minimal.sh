#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/build/upstream"
mkdir -p "$build"
git -C "$root/vendor/flexric" apply --check \
  "$root/integration/patches/flexric-gcc14-kpm-v3.patch"
git -C "$root/vendor/flexric" apply \
  "$root/integration/patches/flexric-gcc14-kpm-v3.patch"
if ! find "$build/ocudu" -type f \( -name gnb -o -name ocudu_gnb \) -perm -111 \
  -print -quit | grep -q .; then
  cmake -S "$root/vendor/ocudu" -B "$build/ocudu" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
  cmake --build "$build/ocudu" --target gnb --parallel 4
else
  echo "Reusing cached pinned OCUDU gNB build without rebuilding"
fi
cmake -S "$root/vendor/flexric" -B "$build/flexric" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DXAPP_DB=SQLITE3_XAPP -DUNIT_TEST=FALSE \
  -DE2AP_VERSION=E2AP_V3 -DKPM_VERSION=KPM_V3_00 \
  -DCMAKE_C_FLAGS_DEBUG="-g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build "$build/flexric" --parallel 4
sudo cmake --install "$build/flexric"
