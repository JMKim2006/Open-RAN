#!/usr/bin/env python3
import argparse, json, pathlib, sys
ap=argparse.ArgumentParser();ap.add_argument("--evidence",type=pathlib.Path,required=True);ap.add_argument("--gate",choices=["A","B"],required=True);a=ap.parse_args()
e=json.loads(a.evidence.read_text())
A=["e2_setup","kpm_subscription_or_indication","nonempty_e2ap_pcap"]
B=A+["ORQEST.Cutoff","ORQEST.Count","ORQEST.InstallSnapshot","ORQEST.ActiveVersion","no_history_holes","no_accounting_errors","no_malformed_accepted"]
need=A if a.gate=="A" else B
missing=[x for x in need if not e.get(x,False)]
if a.gate=="B" and e.get("stock_ocudu",False): missing.append("overlay implementation (stock OCUDU forbidden)")
print(json.dumps({"gate":a.gate,"pass":not missing,"missing":missing},indent=2));sys.exit(bool(missing))
