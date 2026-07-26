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
    # E42 setup alone is insufficient. Require the DU-side E2 association and
    # a RIC/xApp view of an E2 node that advertised the KPM RAN function.
    "e2_setup": bool(
        re.search(r"E2: Connection to Near-RT-RIC .* established", ocudu, re.I)
        and re.search(r"Registered E2 Nodes = [1-9]", xapp)
        and re.search(r"ran func id = 2", xapp, re.I)
    ),
    # Loading the KPM plugin is neither a subscription nor an indication.
    "kpm_subscription": bool(
        re.search(r"Registered E2 Nodes = [1-9]", xapp)
        and re.search(r"Successfully subscribed to RAN_FUNC_ID 2", xapp, re.I)
    ),
    "kpm_indication": bool(
        re.search(r"\bKPM ind_msg latency\b", xapp)
    ),
    "nonempty_e2ap_pcap": pcap.exists() and pcap.stat().st_size > 24,
    "tshark_e2ap_supported": bool(re.search(r"\be2ap\.", fields, re.I)),
}
(out / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
print(json.dumps(evidence, indent=2))
