#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/build/upstream"
mkdir -p "$build"
for patch in \
  flexric-gcc14-kpm-v3.patch \
  flexric-kpm-safe-transition.patch \
  flexric-f1c08ed-kpm-xapp-no-trampolines.patch; do
  git -C "$root/vendor/flexric" apply --check "$root/integration/patches/$patch"
  git -C "$root/vendor/flexric" apply "$root/integration/patches/$patch"
done
if ! find "$build/ocudu" -type f \( -name gnb -o -name ocudu_gnb \) -perm -111 \
  -print -quit | grep -q .; then
  cmake -S "$root/vendor/ocudu" -B "$build/ocudu" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
  cmake --build "$build/ocudu" --target gnb --parallel 4
else
  echo "Reusing cached pinned OCUDU gNB build without rebuilding"
fi
base_flags="-g -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls -D_GLIBCXX_ASSERTIONS -fsanitize=address,undefined"
strict_flags="-Wall -Wextra -Werror -Wtrampolines -Werror=trampolines $base_flags"
cmake -S "$root/vendor/flexric" -B "$build/flexric" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DXAPP_DB=SQLITE3_XAPP -DUNIT_TEST=FALSE \
  -DE2AP_VERSION=E2AP_V3 -DKPM_VERSION=KPM_V3_00 \
  -DCMAKE_C_FLAGS_DEBUG="$base_flags" -DCMAKE_CXX_FLAGS_DEBUG="$base_flags" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build "$build/flexric" --target nearRT-RIC --parallel 4
cmake -S "$root/vendor/flexric" -B "$build/flexric" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DXAPP_DB=SQLITE3_XAPP -DUNIT_TEST=FALSE \
  -DE2AP_VERSION=E2AP_V3 -DKPM_VERSION=KPM_V3_00 \
  -DCMAKE_C_FLAGS_DEBUG="$strict_flags" -DCMAKE_CXX_FLAGS_DEBUG="$strict_flags" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build "$build/flexric" --target xapp_kpm_moni kpm_sm --parallel 4
xapp="$(find "$build/flexric" -type f -name xapp_kpm_moni -perm -111 -print -quit)"
test -n "$xapp"
readelf -W -l "$xapp" | grep 'GNU_STACK' > "$build/flexric/kpm-xapp-gnu-stack.txt"
if grep -Eq 'GNU_STACK.*RWE' "$build/flexric/kpm-xapp-gnu-stack.txt"; then
  echo "Executable GNU_STACK rejected" >&2
  exit 1
fi
sudo install -d /usr/local/lib/flexric /usr/local/etc/flexric
sudo find "$build/flexric" -type f -name 'libkpm_sm.so' \
  -exec install -m 0755 {} /usr/local/lib/flexric/libkpm_sm.so \;
sudo install -m 0644 "$root/vendor/flexric/flexric.conf" \
  /usr/local/etc/flexric/flexric.conf
