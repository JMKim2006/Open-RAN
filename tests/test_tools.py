import json, pathlib, subprocess, sys, tempfile, unittest
ROOT=pathlib.Path(__file__).parents[1]
class Tools(unittest.TestCase):
 def gate(self,gate,data):
  with tempfile.NamedTemporaryFile("w",suffix=".json",delete=False) as f:
   json.dump(data,f); p=f.name
  return subprocess.run([sys.executable,ROOT/"tools/evidence_gate.py","--gate",gate,"--evidence",p],capture_output=True,text=True)
 def test_stock_never_passes_b(self):
  all_true={k:True for k in ["e2_setup","kpm_subscription_or_indication","nonempty_e2ap_pcap","ORQEST.Cutoff","ORQEST.Count","ORQEST.InstallSnapshot","ORQEST.ActiveVersion","no_history_holes","no_accounting_errors","no_malformed_accepted"]}
  all_true["stock_ocudu"]=True; self.assertNotEqual(self.gate("B",all_true).returncode,0)
 def test_gate_a(self):
  self.assertEqual(self.gate("A",{"e2_setup":True,"kpm_subscription_or_indication":True,"nonempty_e2ap_pcap":True}).returncode,0)
if __name__=="__main__": unittest.main()
