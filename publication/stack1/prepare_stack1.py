from pathlib import Path
import hashlib,json,shutil,subprocess,sys
root=Path(__file__).resolve().parent;dst=root/"stack1-source";assert not dst.exists()
def rows(v):
 r=json.loads((root/f"{v}-source-manifest.json").read_text())
 for x in r: assert hashlib.sha256((root/f"{v}-source"/x["path"]).read_bytes()).hexdigest()==x["sha256"]
 return {x["path"]:x["sha256"] for x in r}
base=rows("rdoqbound");db=rows("deblock3");tu=rows("tuple3");rows("sharedq");rows("cache8")
for rel in db:
 q=dst/rel;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/"deblock3-source"/rel,q)
for rel in tu:
 if tu[rel]==base.get(rel):continue
 assert db.get(rel)==base.get(rel) or db.get(rel)==tu[rel],("merge conflict",rel)
 q=dst/rel;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/"tuple3-source"/rel,q)
p=dst/"Source/Lib/Encoder/rd_frame.c";s=p.read_text()
# Apply the already verified cache8 replacement without replaying its source clone.
script=(root/"prepare_cache8.py").read_text()
a=script.index("def rep(");z=script.index('\np.write_text(s,encoding=',a)
ns={"s":s};exec(script[a:z],ns);s=ns["s"]
s="#ifndef M6_TREE_SHARED\n#define M6_TREE_SHARED 0\n#endif\n"+s
needle="    uint64_t treeq_plan_commits, treeq_plan_repairs;"
assert s.count(needle)==1;s=s.replace(needle,needle+"\n    uint64_t shared_f, shared_g;")
shared=(root/"sharedq-source/Source/Lib/Encoder/rd_frame.c").read_text()
a=shared.index("#if M6_TREE_RDOQ && M6_TREE_SHARED\n/* F is")
z=shared.index("static SvtAv2Error refine_tree_finalists(RdWork*",a)
helper=shared[a:z]
needle="static SvtAv2Error refine_tree_finalists(RdWork*"
assert s.count(needle)==1;s=s.replace(needle,helper+needle)
a=s.index(needle);z=s.index("/* Keep equivalent old geometries",a)
body=s[a:z]
start=body.index("    SvtAv2Error error=refine_tree_finalists_once(")
end=body.index("#if M6_TOOL_REPEAT\n",start)
old=body[start:end]
new='''#if M6_TREE_SHARED
    SvtAv2Error error=refine_tree_finalists_shared(work,sb,partial,seeds,count,&winner_decision,&winner);
    SvtAv2RdFrameStats aggregate=work->stats;
#else
'''+old+"#endif\n"
body=body[:start]+new+body[end:]
s=s[:a]+body+s[z:]
needle="    free(work->transform_cache);";assert s.count(needle)==1
s=s.replace(needle,'''#if M6_TREE_RDOQ && M6_TREE_SHARED
    if (work->transform_cache) fprintf(stderr,"shared_tree F=%llu G=%llu\\n",
        (unsigned long long)work->transform_cache->shared_f,
        (unsigned long long)work->transform_cache->shared_g);
#endif
'''+needle)
p.write_text(s,encoding="utf-8",newline="\n")
subprocess.run([sys.executable,str(root/"finish_manifest.py"),"stack1"],check=True)
flags="/DM6_TX_CACHE=1 /DM6_TX_CACHE_SLOTS=8192 /DM6_FLIP_TX=1 /DM6_JOINT_Y_TX=1 /DM6_ROOT_ASYM=1 /DM6_Y_BOUND=1 /DM6_RDOQ_WIDE=1 /DM6_TREE_RDOQ=1 /DM6_TOOL_REPEAT=1"
ps=r'''$ErrorActionPreference='Stop'
. 'C:\Private\ffmpeg-native\deps-full\AV2\activate.ps1' -Quiet
foreach ($arm in 'prod','ref','nodb','check') {
 $shared=if ($arm -eq 'ref') {0} else {1}
 $ways=if ($arm -eq 'ref') {1} else {8}
 $db=if ($arm -eq 'nodb') {0} else {1}
 $keep=if ($arm -eq 'check') {1} else {0}
 $build=if ($arm -eq 'prod') {'build-stack1'} else {"build-stack1-$arm"}
 & $env:AV2_CMAKE -S .\stack1-source -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSVT_AV2_WARNINGS_AS_ERRORS=ON -DSVT_AV2_BUILD_SHARED=OFF -DSVT_AV2_BUILD_TESTS=ON "-DCMAKE_C_FLAGS=FLAGS /DM6_TREE_SHARED=$shared /DM6_TX_CACHE_WAYS=$ways /DM6_DEBLOCK=$db /DM6_TREE_RETAIN_OLD=$keep /DM6_REPEAT_RETAIN_OLD=$keep" > "configure-stack1-$arm.log" 2>&1
 if ($LASTEXITCODE -ne 0) {exit $LASTEXITCODE}
 & $env:AV2_CMAKE --build $build --target SvtAv2EncApp --parallel 2 > "build-stack1-$arm.log" 2>&1
 if ($LASTEXITCODE -ne 0) {exit $LASTEXITCODE}
}
exit 0
'''.replace("FLAGS",flags)
(root/"build_stack1.ps1").write_text(ps,encoding="utf-8",newline="\n")
s=(root/"build_corepack_gcc.sh").read_text().replace("corepack","stack1")
s=s.replace("-DM6_DEBLOCK=1'","-DM6_DEBLOCK=1 -DM6_TREE_RDOQ=1 -DM6_TOOL_REPEAT=1 -DM6_TREE_SHARED=1'")
(root/"build_stack1_gcc.sh").write_text(s,encoding="utf-8",newline="\n")
s=(root/"validate_sharedq.py").read_text().replace('variant="sharedq"','variant="stack1"')
a=s.index("programs=");z=s.index("\nlinux=",a)
s=s[:a]+'''programs={"baseline":str(root/"build-tuple3/bin/SvtAv2EncApp.exe"),"nodb":str(root/"build-stack1-nodb/bin/SvtAv2EncApp.exe"),"ref":str(root/"build-stack1-ref/bin/SvtAv2EncApp.exe"),"rollback_old":str(root/"build-corepack/bin/SvtAv2EncApp.exe"),"rollback":str(root/"build-stack1-check/bin/SvtAv2EncApp.exe"),variant:str(root/"build-stack1/bin/SvtAv2EncApp.exe")}'''+s[z:]
s=s.replace('assert row["outputs"]["baseline"]==row["outputs"]["off"]==row["outputs"][variant]',
'''assert row["outputs"]["baseline"]==row["outputs"]["nodb"]
        assert row["outputs"]["ref"]==row["outputs"][variant]''')
(root/"validate_stack1.py").write_text(s,encoding="utf-8",newline="\n")
p=root/"run_points.py";s=p.read_text();needle='"fine2","corepack"';assert needle in s
p.write_text(s.replace(needle,'"stack1",'+needle),encoding="utf-8",newline="\n")
print("stack1 frozen: tuple3 decisions, shared F/G, cache8, deblock3; ref/nodb/rollback controls")
