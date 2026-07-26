#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=/dev/null
source "$ROOT/integration/pins.env"
mkdir -p "$ROOT/vendor"
fetch() {
  local name="$1" repo="$2" rev="$3" dir="$ROOT/vendor/$name"
  if [[ ! -d "$dir/.git" ]]; then git clone --filter=blob:none "$repo" "$dir"; fi
  git -C "$dir" fetch --depth 1 origin "$rev"
  git -C "$dir" checkout --detach "$rev"
  test "$(git -C "$dir" rev-parse HEAD)" = "$rev"
}
fetch ocudu "$OCUDU_REPO" "$OCUDU_REV"
fetch flexric "$FLEXRIC_REPO" "$FLEXRIC_REV"
fetch open5gs "$OPEN5GS_REPO" "$OPEN5GS_REV"
fetch software-ue "$SOFTWARE_UE_REPO" "$SOFTWARE_UE_REV"
python3 "$ROOT/tools/verify_ocudu_tree.py" "$ROOT/vendor/ocudu" --spec "$ROOT/integration/ocudu-extension-points.json"
