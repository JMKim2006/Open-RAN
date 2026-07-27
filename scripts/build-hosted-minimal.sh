#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/build/upstream"
mkdir -p "$build"
git -C "$root/vendor/flexric" apply --check \
  "$root/integration/patches/flexric-gcc14-kpm-v3.patch"
git -C "$root/vendor/flexric" apply \
  "$root/integration/patches/flexric-gcc14-kpm-v3.patch"
git -C "$root/vendor/flexric" apply --check \
  "$root/integration/patches/flexric-kpm-safe-transition.patch"
git -C "$root/vendor/flexric" apply \
  "$root/integration/patches/flexric-kpm-safe-transition.patch"
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
  -DCMAKE_C_FLAGS_DEBUG="-g -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls -D_GLIBCXX_ASSERTIONS -fsanitize=address,undefined" \
  -DCMAKE_CXX_FLAGS_DEBUG="-g -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls -D_GLIBCXX_ASSERTIONS -fsanitize=address,undefined" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build "$build/flexric" \
  --target nearRT-RIC xapp_kpm_moni kpm_sm --parallel 4
sudo install -d /usr/local/lib/flexric /usr/local/etc/flexric
sudo find "$build/flexric" -type f -name 'libkpm_sm.so' \
  -exec install -m 0755 {} /usr/local/lib/flexric/libkpm_sm.so \;
sudo install -m 0644 "$root/vendor/flexric/flexric.conf" \
  /usr/local/etc/flexric/flexric.conf
