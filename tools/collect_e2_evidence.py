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


def tshark_count(pcap, display_filter):
    """Count decoded E2AP frames matching an explicit protocol predicate."""
    result = subprocess.run(
        [
            "tshark", "-r", str(pcap), "-Y", display_filter,
            "-T", "fields", "-e", "frame.number",
        ],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return 0, result.stderr.strip()
    return len([line for line in result.stdout.splitlines() if line.strip()]), ""

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
decoded_text = ""
counts = {
    "e2_setup_request": 0,
    "e2_setup_response": 0,
    "ric_subscription_request": 0,
    "ric_subscription_response": 0,
    "ric_indication": 0,
}
filter_errors = {}
if re.search(r"\be2ap\.", fields, re.I) and pcap.exists():
    result = subprocess.run(
        ["tshark", "-r", str(pcap), "-Y", "e2ap", "-V"],
        capture_output=True, text=True
    )
    decoded_text = result.stdout + result.stderr
    decoded.write_text(decoded_text)
    # E2AP v2/v3 procedure codes are stable: E2 Setup=1, RIC Indication=5,
    # and RIC Subscription=8.  Pair the code with the PDU choice so one
    # observed packet cannot be mistaken for a complete exchange.
    predicates = {
        "e2_setup_request": (
            "e2ap.procedureCode == 1 && e2ap.initiatingMessage_element"
        ),
        "e2_setup_response": (
            "e2ap.procedureCode == 1 && e2ap.successfulOutcome_element"
        ),
        "ric_subscription_request": (
            "e2ap.procedureCode == 8 && e2ap.initiatingMessage_element"
        ),
        "ric_subscription_response": (
            "e2ap.procedureCode == 8 && e2ap.successfulOutcome_element"
        ),
        "ric_indication": (
            "e2ap.procedureCode == 5 && e2ap.initiatingMessage_element"
        ),
    }
    for name, display_filter in predicates.items():
        counts[name], error = tshark_count(pcap, display_filter)
        if error:
            filter_errors[name] = error

e2_setup = counts["e2_setup_request"] > 0 and counts["e2_setup_response"] > 0
kpm_subscription = (
    counts["ric_subscription_request"] > 0
    and counts["ric_subscription_response"] > 0
)
kpm_indication = counts["ric_indication"] > 0
evidence = {
    "stock_ocudu": True,
    "e2_setup": e2_setup,
    "kpm_subscription": kpm_subscription,
    "kpm_indication": kpm_indication,
    "nonempty_e2ap_pcap": pcap.exists() and pcap.stat().st_size > 24,
    "tshark_e2ap_supported": bool(re.search(r"\be2ap\.", fields, re.I)),
    "e2ap_frame_counts": counts,
    "e2ap_filter_errors": filter_errors,
}
(out / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
print(json.dumps(evidence, indent=2))
