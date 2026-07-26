#!/usr/bin/env python3
import argparse, json, pathlib, subprocess, sys
ap=argparse.ArgumentParser();ap.add_argument("tree",type=pathlib.Path);ap.add_argument("--spec",type=pathlib.Path,default=pathlib.Path("integration/ocudu-extension-points.json"));a=ap.parse_args()
spec=json.loads(a.spec.read_text()); failures=[]
head=subprocess.run(["git","-C",a.tree,"rev-parse","HEAD"],capture_output=True,text=True)
if head.returncode or head.stdout.strip()!=spec["revision"]: failures.append("OCUDU HEAD is not the pinned revision")
for item in spec["points"]:
 p=a.tree/item["path"]
 if not p.is_file(): failures.append(f"missing {p}"); continue
 blob=subprocess.run(["git","-C",a.tree,"hash-object",item["path"]],capture_output=True,text=True)
 if blob.returncode or blob.stdout.strip()!=item["blob_sha"]: failures.append(f"changed extension point {p}")
if failures: print(*failures,sep="\n",file=sys.stderr);sys.exit(1)
print("OCUDU source-tree extension points: PASS")
