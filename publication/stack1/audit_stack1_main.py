from pathlib import Path
import datetime,hashlib,json,subprocess,sys
root=Path(__file__).resolve().parent
expected=[116,113,110,108,106,105,104,102,100,96]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
basejob=None;rows=[]
for q in expected:
 p=root/f"native-stack1-2D-q{q}/result.json";r=json.loads(p.read_text())
 jp=root/f"native-stack1-2D-q{q}.job.json";j=json.loads(jp.read_text())
 assert r["status"].startswith("PASS") and r["fixed_decoder_exact"] and r["performance_scope"]=="NOT_MEASURED"
 assert r["q"]==j["q"]==q and r["job_sha256"]==sha(jp)
 assert r["runner_sha256"]==j["runner_sha256"]=="6bd2f136ded8dbbfd10aaba0c96da5f9d12f44ff74fbf1324fb555507eba9151"
 cmd=j["encode"].copy()
 for k in ("--qp","--output","--export-recon"):cmd[cmd.index(k)+1]="VARIABLE"
 stable=dict(identities=j["identities"],encode=cmd,models=r["models"],decoder=j["decode"][0])
 if basejob is None:basejob=stable
 else:assert stable==basejob,q
 rows.append(dict(q=q,result=str(p),result_sha256=sha(p),job=str(jp),job_sha256=sha(jp)))
assert sha(root/"build-stack1/bin/SvtAv2EncApp.exe")=="4f4a97923ea811e11d290898bb798f26a8cd99be67edf4134760632ce47d21ea"
assert sha(root/"stack1-source-manifest.json")=="5d54f00354a62a558b36bfb7580f454df6193bbf2966b5f6835caf0e92b33e4f"
before=set(root.glob("curve-analysis-*.json"))
p=subprocess.run([sys.executable,str(root/"analyze_curves.py")],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
stamp=datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
(root/f"stack1-analysis-{stamp}.log").write_bytes(p.stdout+b"\n"+p.stderr);p.check_returncode()
created=set(root.glob("curve-analysis-*.json"))-before;assert len(created)==1,created
path=created.pop();whole=json.loads(path.read_text());entry=whole["candidates"]["stack1"]
assert set(expected)<={p["q"] for p in entry["points"]} and entry["band_coverage_pass"]
for method in ("pchip","linear"):
 x=entry["comparisons"]["v1"][method]["aom"]["primary_band"]
 assert x["domain"]==[93,95] and len(x["five_matched"])==5
report=dict(status="PASS_MAIN_COVERAGE_IDENTITIES_WIDE_PENDING",performance_scope="NOT_MEASURED",
 expected_main_qps=expected,identity_audit=basejob,main_results=rows,
 whole_analysis=str(path),whole_analysis_sha256=sha(path),analysis_script_sha256=sha(root/"analyze_curves.py"),
 stack1=entry,wide_gate_complete=False,targets_achieved=False)
out=root/f"stack1-main-audit-{stamp}.json";out.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
print(out,flush=True)
print(json.dumps(dict(qps=[p["q"] for p in entry["points"]],inversions=entry["quality_inversions"],
 main={m:entry["comparisons"]["v1"][m]["aom"]["primary_band"] for m in ("pchip","linear")}),indent=2),flush=True)
