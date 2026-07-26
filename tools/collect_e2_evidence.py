#!/usr/bin/env python3
import json
import pathlib
import re
import subprocess
import sys

out = pathlib.Path(sys.argv[1])
logs = "\n".join(p.read_text(errors="replace") for p in (out / "logs").glob("*.log"))
pcap = out / "e2ap.pcap"
decoded = out / "e2ap-tshark.txt"
fields = ""
try:
    fields = subprocess.run(
        ["tshark", "-G", "fields"], capture_output=True, text=True, check=True
    ).stdout
except (OSError, subprocess.CalledProcessError):
    pass
if re.search(r"\be2ap\.", fields, re.I) and pcap.exists():
    result = subprocess.run(
        ["tshark", "-r", str(pcap), "-Y", "e2ap"],
        capture_output=True, text=True
    )
    decoded.write_text(result.stdout + result.stderr)
evidence = {
    "stock_ocudu": True,
    "e2_setup": bool(re.search(r"E2 Setup|E2_SETUP|setup response", logs, re.I)),
    "kpm_subscription_or_indication": bool(
        re.search(r"KPM.*(?:subscription|indication)|(?:subscription|indication).*KPM", logs, re.I)
    ),
    "nonempty_e2ap_pcap": pcap.exists() and pcap.stat().st_size > 24,
    "tshark_e2ap_supported": bool(re.search(r"\be2ap\.", fields, re.I)),
}
(out / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
print(json.dumps(evidence, indent=2))
