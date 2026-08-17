#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/evidence/hosted-overlay}"
mkdir -p "$out/logs"
exec > >(tee "$out/logs/overlay.log") 2>&1
echo "Gate B is intentionally blocked until the bounded OCUDU KPM/RC patch applies"
echo "to the pinned source and the hosted Gate A artifact has passed."
python3 - "$out/evidence.json" <<'PY'
import json, pathlib, sys
pathlib.Path(sys.argv[1]).write_text(json.dumps({
  "stock_ocudu": False,
  "e2_setup": False,
  "kpm_subscription_or_indication": False,
  "nonempty_e2ap_pcap": False,
  "ORQEST.Cutoff": False,
  "ORQEST.Count": False,
  "ORQEST.InstallSnapshot": False,
  "ORQEST.ActiveVersion": False,
  "no_history_holes": False,
  "no_accounting_errors": False,
  "no_malformed_accepted": False
}, indent=2) + "\n")
PY
python3 "$root/tools/evidence_gate.py" --gate B --evidence "$out/evidence.json" \
  | tee "$out/gate-b.json"
