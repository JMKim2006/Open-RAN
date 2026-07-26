#!/usr/bin/env python3
import json
import pathlib
import re
import subprocess
import sys

out = pathlib.Path(sys.argv[1])
def read_log(name):
    path = out / "logs" / name
    return path.read_text(errors="replace") if path.exists() else ""

ocudu = read_log("ocudu.log") + read_log("ocudu-file.log")
flexric = read_log("flexric.log")
xapp = read_log("kpm-xapp.log")
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
    # E42 xApp/RIC setup is not E2AP E2 Setup. Require evidence at both E2 peers.
    "e2_setup": bool(
        re.search(r"E2.?SETUP|E2 Setup", ocudu, re.I)
        and re.search(r"E2.?SETUP|E2 Setup", flexric, re.I)
    ),
    # Loading the KPM plugin is not a subscription or indication.
    "kpm_subscription_or_indication": bool(
        re.search(r"Registered E2 Nodes = [1-9]", xapp)
        and re.search(r"KPM.*(?:subscription|indication)|(?:subscription|indication).*KPM", xapp, re.I)
    ),
    "nonempty_e2ap_pcap": pcap.exists() and pcap.stat().st_size > 24,
    "tshark_e2ap_supported": bool(re.search(r"\be2ap\.", fields, re.I)),
}
(out / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
print(json.dumps(evidence, indent=2))
