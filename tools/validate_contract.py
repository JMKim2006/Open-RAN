import json, pathlib
p=pathlib.Path(__file__).parents[1]/"contracts/orqest-contract.json"
d=json.loads(p.read_text())
assert "EXPERIMENTAL" in d["$comment"]
k=d["kpm_ul_d6"]; assert "21 " in k["ORQEST.DesignUpper"] and "6 " in k["ORQEST.Response"]
rc=d["rc_control_style_2_dl"]["ORQEST.InstallSnapshot"]
assert "exclusive" in k["ORQEST.Cutoff"] and len(rc)==6
print("contract: PASS")
