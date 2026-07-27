#!/usr/bin/env python3
import argparse
import json
import pathlib
import sys

ap=argparse.ArgumentParser()
ap.add_argument("--evidence",type=pathlib.Path,required=True)
ap.add_argument("--gate",choices=["G0","G1","G2","G3","A","B"],required=True)
a=ap.parse_args()
e=json.loads(a.evidence.read_text())
requirements={
 "G0":["pinned_ocudu","pinned_flexric","actual_sctp_association","e2_setup",
       "kpm_rc_ran_function_registration","nonempty_e2ap_pcap"],
 "G1":["actual_sctp_association","nonempty_sctp_pcap","du_cumulative_report",
       "compatibility_filtering","source_complete_prefix","source_specific_provenance",
       "invalid_crc_rejected","malformed_rejected"],
 "G2":["G1_pass","snapshot_generated","atomic_installation","active_version_ack",
       "prefix_suffix_disjoint","no_double_counting","no_permanent_history_holes",
       "stale_rejected","regressive_rejected","incompatible_rejected",
       "queue_state_du_local","ric_no_per_tti_assignment","matching_capacity_valid"],
 "G3":["e2_setup","kpm_subscription","kpm_indication","nonempty_e2ap_pcap"],
 # Legacy aliases retained only for old evidence readers.
 "A":["e2_setup","kpm_subscription","kpm_indication","nonempty_e2ap_pcap"],
 "B":["e2_setup","kpm_subscription","kpm_indication","nonempty_e2ap_pcap",
      "ORQEST.Cutoff","ORQEST.Count","ORQEST.InstallSnapshot","ORQEST.ActiveVersion",
      "no_history_holes","no_accounting_errors","no_malformed_accepted"],
}
missing=[k for k in requirements[a.gate] if e.get(k) is not True]
if a.gate=="G0" and (e.get("kpm_subscription") or e.get("kpm_indication")):
    missing.append("G0 must not claim KPM subscription/indication")
if a.gate=="B" and e.get("stock_ocudu",False):
    missing.append("overlay implementation (stock OCUDU forbidden)")
result={"gate":a.gate,"pass":not missing,"missing":missing}
print(json.dumps(result,indent=2))
sys.exit(0 if not missing else 1)
