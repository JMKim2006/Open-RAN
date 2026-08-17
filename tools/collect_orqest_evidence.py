#!/usr/bin/env python3
import argparse
import hashlib
import json
import pathlib
import subprocess

ap=argparse.ArgumentParser()
ap.add_argument("evidence",type=pathlib.Path)
a=ap.parse_args()
out=a.evidence
def read_jsonl(name,process):
    rows=[]
    path=out/name
    if not path.exists(): return rows
    for line in path.read_text(errors="replace").splitlines():
        try:
            row=json.loads(line);row["process"]=process;rows.append(row)
        except json.JSONDecodeError:
            pass
    return rows
ric=read_jsonl("ric-timeline.jsonl","orqest-ric")
du=read_jsonl("du-timeline.jsonl","orqest-du")
rows=ric+du
order={
 "ric_listen":0,"sctp_association":1,"cumulative_report_sent":2,
 "report_accepted":3,"source_complete_prefix":4,"snapshot_sent":5,
 "snapshot_received":6,"snapshot_installed":7,"negative_snapshot_validation":8,
 "du_local_matching":9,"active_version_report_sent":10,
 "active_version_ack":11,"du_complete":12,"ric_complete":13,
}
rows.sort(key=lambda x:(order.get(x.get("event"),99),x.get("process","")))
(out/"timeline.json").write_text(json.dumps(rows,indent=2)+"\n")
pcap=out/"orqest-sctp.pcap"
pcap_nonempty=pcap.exists() and pcap.stat().st_size>24
decoded=""
if pcap_nonempty:
    p=subprocess.run(["tshark","-r",str(pcap),"-Y","sctp","-T","fields",
      "-e","frame.number","-e","frame.time_relative","-e","ip.src","-e","ip.dst",
      "-e","sctp.srcport","-e","sctp.dstport"],capture_output=True,text=True)
    decoded=p.stdout+p.stderr
(out/"sctp-tshark.txt").write_text(decoded)
def one(event,process=None):
    return next((x for x in rows if x.get("event")==event and (process is None or x.get("process")==process)),{})
neg=one("negative_snapshot_validation","orqest-du")
install=one("snapshot_installed","orqest-du")
match=one("du_local_matching","orqest-du")
ric_done=one("ric_complete","orqest-ric")
e={
 "prototype":"standards-facing multi-process wire-contract validation over actual SCTP",
 "experimental_fields_standardized_oran":False,
 "actual_sctp_association":bool(one("sctp_association","orqest-ric") and one("sctp_association","orqest-du")),
 "nonempty_sctp_pcap":pcap_nonempty,
 "du_cumulative_report":bool(one("cumulative_report_sent","orqest-du")),
 "compatibility_filtering":one("report_accepted","orqest-ric").get("compatibility_qualified") is True,
 "source_complete_prefix":bool(one("source_complete_prefix","orqest-ric")),
 "source_specific_provenance":one("snapshot_sent","orqest-ric").get("source_cutoffs")=={"du-a":4},
 "snapshot_generated":one("snapshot_sent","orqest-ric").get("version")==1,
 "atomic_installation":install.get("version")==1,
 "active_version_ack":one("active_version_ack","orqest-ric").get("accepted") is True,
 "prefix_suffix_disjoint":install.get("prefix_suffix_disjoint") is True,
 "no_double_counting":install.get("total_count")==7,
 "no_permanent_history_holes":install.get("history_complete") is True,
 "stale_rejected":neg.get("stale_rejected") is True,
 "regressive_rejected":neg.get("regressive_rejected") is True,
 "incompatible_rejected":neg.get("incompatible_rejected") is True,
 "invalid_crc_rejected":neg.get("crc_invalid_rejected") is True,
 "malformed_rejected":neg.get("malformed_rejected") is True,
 "queue_state_du_local":match.get("queue_state_local_only") is True and ric_done.get("queue_state_received") is False,
 "ric_no_per_tti_assignment":ric_done.get("per_tti_assignment_selected") is False,
 "matching_capacity_valid":match.get("capacity_valid") is True,
}
e["G1_pass"]=all(e[k] for k in ["actual_sctp_association","nonempty_sctp_pcap","du_cumulative_report",
 "compatibility_filtering","source_complete_prefix","source_specific_provenance",
 "invalid_crc_rejected","malformed_rejected"])
e["G2_pass"]=e["G1_pass"] and all(e[k] for k in ["snapshot_generated","atomic_installation",
 "active_version_ack","prefix_suffix_disjoint","no_double_counting","no_permanent_history_holes",
 "stale_rejected","regressive_rejected","incompatible_rejected","queue_state_du_local",
 "ric_no_per_tti_assignment","matching_capacity_valid"])
(out/"evidence.json").write_text(json.dumps(e,indent=2)+"\n")
with (out/"sha256-manifest.txt").open("w") as f:
    for path in sorted(out.rglob("*")):
        if path.is_file() and path.name!="sha256-manifest.txt":
            f.write(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(out).as_posix()}\n")
print(json.dumps(e,indent=2))
