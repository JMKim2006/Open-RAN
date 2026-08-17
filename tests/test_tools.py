import json, os, pathlib, subprocess, sys, tempfile, unittest
ROOT=pathlib.Path(__file__).parents[1]
class Tools(unittest.TestCase):
 def gate(self,gate,data):
  with tempfile.NamedTemporaryFile("w",suffix=".json",delete=False) as f:
   json.dump(data,f); p=f.name
  return subprocess.run([sys.executable,ROOT/"tools/evidence_gate.py","--gate",gate,"--evidence",p],capture_output=True,text=True)
 def test_stock_never_passes_b(self):
  all_true={k:True for k in ["e2_setup","kpm_subscription","kpm_indication","nonempty_e2ap_pcap","ORQEST.Cutoff","ORQEST.Count","ORQEST.InstallSnapshot","ORQEST.ActiveVersion","no_history_holes","no_accounting_errors","no_malformed_accepted"]}
  all_true["stock_ocudu"]=True; self.assertNotEqual(self.gate("B",all_true).returncode,0)
 def test_gate_a(self):
  self.assertEqual(self.gate("A",{"e2_setup":True,"kpm_subscription":True,"kpm_indication":True,"nonempty_e2ap_pcap":True}).returncode,0)
 def test_g3_requires_each_observation(self):
  complete={"e2_setup":True,"kpm_subscription":True,"kpm_indication":True,"nonempty_e2ap_pcap":True}
  self.assertEqual(self.gate("G3",complete).returncode,0)
  for key in complete:
   incomplete=dict(complete); incomplete[key]=False
   self.assertNotEqual(self.gate("G3",incomplete).returncode,0,key)
 def test_historical_g0_is_invalidated(self):
  result=self.gate("G0",json.loads((ROOT/"evidence/g0-public-stack.json").read_text()))
  self.assertNotEqual(result.returncode,0)
 def test_e2ap_collector_uses_procedure_and_pdu_fields(self):
  with tempfile.TemporaryDirectory() as td:
   root=pathlib.Path(td); out=root/"evidence"; (out/"logs").mkdir(parents=True)
   (out/"e2ap.pcap").write_bytes(b"p"*64)
   fake=root/"tshark"
   fake.write_text("""#!/usr/bin/env python3
import sys
if '-G' in sys.argv:
 print('F\\tprocedureCode\\te2ap.procedureCode')
 raise SystemExit(0)
if '-V' in sys.argv:
 print('decoded E2AP')
 raise SystemExit(0)
flt=sys.argv[sys.argv.index('-Y')+1]
counts={
 'e2ap.procedureCode == 1 && e2ap.initiatingMessage_element': 1,
 'e2ap.procedureCode == 1 && e2ap.successfulOutcome_element': 1,
 'e2ap.procedureCode == 8 && e2ap.initiatingMessage_element': 1,
 'e2ap.procedureCode == 8 && e2ap.successfulOutcome_element': 1,
 'e2ap.procedureCode == 5 && e2ap.initiatingMessage_element': 3,
}
for i in range(counts.get(flt,0)): print(i+1)
""")
   fake.chmod(0o755)
   env=dict(os.environ); env["PATH"]=str(root)+os.pathsep+env["PATH"]
   result=subprocess.run([sys.executable,ROOT/"tools/collect_e2_evidence.py",out],env=env,capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stderr)
   evidence=json.loads((out/"evidence.json").read_text())
   self.assertTrue(evidence["e2_setup"])
   self.assertTrue(evidence["kpm_subscription"])
   self.assertTrue(evidence["kpm_indication"])
   self.assertEqual(evidence["e2ap_frame_counts"]["ric_indication"],3)
if __name__=="__main__": unittest.main()
