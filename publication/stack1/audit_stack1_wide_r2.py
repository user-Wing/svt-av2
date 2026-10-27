from pathlib import Path
import datetime,hashlib,json,math,os,subprocess,sys,traceback
root=Path(__file__).resolve().parent
frozen=root.parent/"speed-r3-v1-20261025/ccso-auto-fullcurve-r1/curve-inputs.json"
crossroot=root.parent/"speed-r3-v1-20261025/ccso-auto-followup-r1"
qps=[192,190,186,160,130,116,113,110,108,106,105,104,102,100,96,90,70]
methods=("pchip","linear");metrics=("v1","neg")
pins=dict(frozen="1b943cd6a1a6e4c81b76bba072ed784474bd4b7051e19a8b6493bd74aab91162",
 runner="6bd2f136ded8dbbfd10aaba0c96da5f9d12f44ff74fbf1324fb555507eba9151",
 encoder="4f4a97923ea811e11d290898bb798f26a8cd99be67edf4134760632ce47d21ea",
 manifest="5d54f00354a62a558b36bfb7580f454df6193bbf2966b5f6835caf0e92b33e4f",
 decoder="110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac",
 vmaf="1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",
 v1="cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",
 neg="d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f",
 source="5536d9a3296378f3a073513eff15d211550b0d677e6e8c90596af60781f4eefd",
 raw="c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09")
stamp=datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
out=root/f"stack1-wide-audit-{stamp}.json"
report=dict(status="RUNNING",performance_scope="NOT_MEASURED",formal_project_promotion=False,
 targets_achieved=False,cross_same_quality_acceptance_complete=False,holdout_used=False)
support={};cache={}
def norm(p):return os.path.normcase(os.path.abspath(str(p)))
def sha(p,expected=None):
 p=Path(p);s=p.stat();key=(norm(p),s.st_size,s.st_mtime_ns)
 if key not in cache:
  h=hashlib.sha256()
  with p.open("rb") as f:
   for chunk in iter(lambda:f.read(8388608),b""):h.update(chunk)
  cache[key]=h.hexdigest()
 digest=cache[key];support[str(p)]=dict(sha256=digest,bytes=s.st_size)
 if expected is not None:assert digest==expected,("SHA",str(p),digest,expected)
 return digest
def load(p,expected=None):sha(p,expected);return json.loads(Path(p).read_text(encoding="utf-8-sig"))
def arg(cmd,k):assert cmd.count(k)==1,(k,cmd);return cmd[cmd.index(k)+1]
def mask(cmd,keys):
 c=list(cmd)
 for k in keys:arg(c,k);c[c.index(k)+1]="VARIABLE:"+k
 return c
def close(a,b):return math.isfinite(float(a)) and abs(float(a)-float(b))<1e-6
def verify(family,q):
 stem=f"native-stack1-{family}-q{q}";d=root/stem;jp=root/(stem+".job.json")
 r=load(d/"result.json");j=load(jp)
 assert r["status"]=="PASS_NEW_SINGLE_FRAME_FIXED_DECODE_V1_NEG" and r["fixed_decoder_exact"]
 assert r["performance_scope"]=="NOT_MEASURED" and j["kind"]=="av2"
 assert r["profile"]==j["profile"] and r["q"]==j["q"]==q
 if family=="2D":assert r["profile"]=="AgentDock-r5-stack1-mixed8"
 assert r["job_sha256"]==sha(jp) and r["runner_sha256"]==j["runner_sha256"]==pins["runner"]
 assert norm(j["out"])==norm(d) and r["models"]==j["models"]
 enc=j["encode"];dec=j["decode"];score=r["score"]["command"]
 assert enc==r["encode"]["command"] and dec==r["decode"]["command"]
 assert all(r[k]["exit_code"]==0 for k in ("encode","decode","score"))
 identities={norm(x["path"]):sha(x["path"],x["sha256"]) for x in j["identities"]}
 assert len(identities)==len(j["identities"])
 assert len(identities)==10 if family=="2D" else len(identities)>=10
 for path,pin in ((enc[0],"encoder"),(dec[0],"decoder"),(j["vmaf"],"vmaf")):
  assert identities[norm(path)]==sha(path,pins[pin])
 assert pins["manifest"] in identities.values()
 source=arg(enc,"--input");raw=j["source_raw"]
 for path in (source,raw):assert identities[norm(path)]==sha(path)
 if family=="2D":sha(source,pins["source"]);sha(raw,pins["raw"])
 assert int(arg(enc,"--qp"))==q and "--still-picture" in enc
 assert norm(arg(enc,"--output"))==norm(d/"stream.obu")
 assert norm(arg(enc,"--export-recon"))==norm(d/"recon.yuv")
 assert norm(arg(dec,"-o"))==norm(d/"decoded.yuv") and norm(dec[-1])==norm(d/"stream.obu")
 assert "--output-bit-depth=10" in dec and "--rawvideo" in dec
 assert sha(d/"stream.obu",r["stream_sha256"]) and (d/"stream.obu").stat().st_size==r["bytes"]
 sha(d/"recon.yuv",r["decoded_sha256"]);sha(d/"decoded.yuv",r["decoded_sha256"])
 assert norm(score[0])==norm(j["vmaf"]) and norm(arg(score,"--reference"))==norm(raw)
 assert norm(arg(score,"--distorted"))==norm(d/"decoded.yuv")
 assert arg(score,"--width")=="1920" and arg(score,"--height")=="1080"
 assert arg(score,"--bitdepth")=="10" and arg(score,"--pixel_format")=="420"
 def local(p):return Path(p) if Path(p).is_absolute() else d/p
 assert norm(local(arg(score,"--output")))==norm(d/"metrics.json")
 specs=[score[i+1] for i,k in enumerate(score[:-1]) if k=="--model"];assert len(specs)==2
 for metric in metrics:
  m=j["models"][metric];assert m["sha256"]==pins[metric]
  assert identities[norm(m["path"])]==sha(m["path"],pins[metric])
  suffix=":name="+metric;spec=[s for s in specs if s.startswith("path=") and s.endswith(suffix)]
  assert len(spec)==1;sha(local(spec[0][5:-len(suffix)].replace("\\:",":")),pins[metric])
 m=load(d/"metrics.json",r["metric_json_sha256"]);assert len(m["frames"])==1
 values=m["frames"][0]["metrics"]
 for k in metrics:assert close(values[k],r[k]),(stem,k)
 for prefix in ("speed_chroma_u_","speed_chroma_v_","speed_chroma_uv_"):
  keys=[k for k in r["features"] if k.startswith(prefix)];assert keys
  for k in keys:assert close(values[k],r["features"][k])
 stable=dict(identities=sorted(identities.items()),encode=mask(enc,("--qp","--output","--export-recon")),
  models=r["models"],decoder=dec[0],source_raw=raw,vmaf=j["vmaf"])
 shared=dict(identities=sorted((p,h) for p,h in identities.items() if p not in (norm(source),norm(raw))),
  encode=mask(enc,("--input","--qp","--output","--export-recon")),models=r["models"],decoder=dec[0],vmaf=j["vmaf"])
 point={k:r[k] for k in ("q","bytes","v1","neg")}|dict(result=str(d/"result.json"),sha256=sha(d/"result.json"))
 return point,stable,shared
try:
 base=load(frozen,pins["frozen"]);sha(root/"point_runner.py",pins["runner"])
 manifest=load(root/"stack1-source-manifest.json",pins["manifest"]);assert len(manifest)==458
 for item in manifest:sha(root/"stack1-source"/item["path"],item["sha256"])
 assert {int(p.parent.name.rsplit("q",1)[1]) for p in root.glob("native-stack1-2D-q*/result.json")}==set(qps)
 points=[];stable0=shared0=None
 for q in qps:
  point,stable,shared=verify("2D",q)
  if stable0 is None:stable0=stable;shared0=shared
  assert stable==stable0,("non-QP parameters",q);points.append(point)
 report["points"]=points;report["stable_parameters"]=stable0;report["all_17_points_preserved"]=True
 analyzer=root/"analyze_curves.py";script_sha=sha(analyzer)
 p=subprocess.run([sys.executable,str(analyzer)],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=180)
 log=root/f"stack1-wide-analysis-{stamp}.log";log.write_bytes(p.stdout+b"\n"+p.stderr);p.check_returncode()
 sha(analyzer,script_sha);sha(log)
 path=Path(p.stdout.decode("utf-8-sig").splitlines()[0]);analysis=load(path);entry=analysis["candidates"]["stack1"]
 assert len(entry["points"])==17 and {p["q"] for p in entry["points"]}==set(qps)
 for point in points:
  matching=[p for p in entry["points"] if p["q"]==point["q"]];assert len(matching)==1
  for k in ("bytes","v1","neg","sha256"):assert matching[0][k]==point[k]
 report["analysis"]=dict(path=str(path),sha256=sha(path),script_sha256=script_sha,baseline_check=analysis["baseline_check"])
 domains={k:[min(r[k] for r in base["r4c9"]),max(r[k] for r in base["r4c9"])] for k in metrics}
 inversions={k:[[a["q"],b["q"]] for a,b in zip(points,points[1:]) if b[k]<=a[k]] for k in metrics}
 assert inversions==entry["quality_inversions"] and [192,190] in inversions["neg"]
 report["quality_inversions"]=inversions;report["quality_inversion_free"]=not any(inversions.values())
 report["interpolation_policy"]="All17 raw points retained, sorted by measured quality for PCHIP/linear logbytes. No extrapolation or monotonic repair. NEG192->190 is below the original r4c low endpoint but may affect the adjacent PCHIP derivative."
 assert entry["band_coverage_pass"] and sum(93<=p["v1"]<=95 for p in points)>=4
 main={m:entry["comparisons"]["v1"][m]["aom"]["primary_band"] for m in methods}
 for m,c in main.items():
  assert c["domain"]==[93,95] and [r["quality"] for r in c["five_matched"]]==[93,93.5,94,94.5,95]
 report["main_vs_aom"]=main;wide={};passed=True
 for k in metrics:
  domain=domains[k];assert min(p[k] for p in points)<=domain[0] and max(p[k] for p in points)>=domain[1]
  wide[k]={}
  for method in methods:
   c=entry["comparisons"][k][method]["original_r4c"]["common"];assert c["domain"]==domain,(k,method,c["domain"],domain)
   assert all(math.isfinite(c[x]) for x in ("ratio","worst_ratio","worst_quality"))
   gate=c["worst_ratio"]<=1.01;passed=passed and gate;wide[k][method]=c|dict(numeric_gate_pass=gate,limit=1.01)
 report["wide_vs_original_r4c"]=wide;report["wide_numeric_gate_pass"]=passed
 report["primary_2d_numeric_targets"]={str(t):all(main[m]["ratio"]<=t for m in methods) for t in (1.0,0.85)}
 old=[("3D",110,98476,94.765919,91.315009,"2b7caf83041dcd9205eb8a0e677a7a7ab6628bf08811b1866079d3054609e90b"),
  ("Real",100,79329,94.736682,92.611070,"b870b2f2c1dea37970f112cef7a7e27f8ad3d8438643838070b9af4f294f8636")]
 oldpaths=list(crossroot.rglob("result.json"));cross={}
 for family,q,n,v1,neg,digest in old:
  current,_,shared=verify(family,q)
  for k in ("encode","models","decoder","vmaf"):assert shared[k]==shared0[k],(family,k)
  required=dict(shared0["identities"]);present=dict(shared["identities"])
  assert all(present.get(p)==h for p,h in required.items()),(family,"common required identity drift")
  paths=[p for p in oldpaths if sha(p)==digest];assert len(paths)==1,(family,paths)
  raw=load(paths[0],digest);ref=dict(q=q,bytes=n,v1=v1,neg=neg)
  for k,val in ref.items():
   if k in raw:assert close(raw[k],val),(family,k)
  cross[family]=dict(current=current,frozen_measurements=ref,frozen_result=str(paths[0]),frozen_sha256=digest,
   frozen_raw=raw,raw_byte_ratio=current["bytes"]/n,primary_delta=current["v1"]-v1,neg_delta=current["neg"]-neg,
   extra_audit_identities=[(p,h) for p,h in present.items() if p not in required],
   scope="Same-QP diagnostic only; no matched-quality curve or old/new full input/configuration acceptance.")
 report["cross_diagnostics"]=cross
 report["status"]="PASS_AUDIT_COMPLETED";report["candidate_review"]="NUMERIC_WIDE_PASS_WITH_RETAINED_INVERSION_AND_CROSS_SINGLE_POINT_LIMITATION" if passed else "NUMERIC_WIDE_FAIL"
except BaseException:
 report["status"]="FAIL";report["error"]=traceback.format_exc()
report["support_files"]=support
out.write_text(json.dumps(report,indent=2,ensure_ascii=False,allow_nan=False)+"\n",encoding="utf-8")
print(out,flush=True)
print(json.dumps(dict(status=report["status"],review=report.get("candidate_review"),main={k:v["ratio"] for k,v in report.get("main_vs_aom",{}).items()},wide={k:{m:v["worst_ratio"] for m,v in row.items()} for k,row in report.get("wide_vs_original_r4c",{}).items()},inversions=report.get("quality_inversions"),targets=report.get("primary_2d_numeric_targets"),error=report.get("error")),indent=2),flush=True)
if report["status"]=="FAIL":raise SystemExit(1)
