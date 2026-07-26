#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
source "$root/integration/pins.env"
mkdir -p "$root/vendor"
fetch() {
  local name="$1" repo="$2" rev="$3" dir="$root/vendor/$name"
  git init "$dir"
  git -C "$dir" remote get-url origin >/dev/null 2>&1 || git -C "$dir" remote add origin "$repo"
  git -C "$dir" fetch --depth 1 origin "$rev"
  git -C "$dir" checkout --detach "$rev"
  test "$(git -C "$dir" rev-parse HEAD)" = "$rev"
}
fetch ocudu "$OCUDU_REPO" "$OCUDU_REV"
fetch flexric "$FLEXRIC_REPO" "$FLEXRIC_REV"
python3 "$root/tools/verify_ocudu_tree.py" "$root/vendor/ocudu" \
  --spec "$root/integration/ocudu-extension-points.json"
