"""Fixed AVM decoding of concatenated independent still streams, per-frame proof."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import re
import subprocess


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    p=argparse.ArgumentParser()
    for key in ['source','decoder','out']:
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--expected-count',type=int,required=True)
    p.add_argument('--expected-case-list',type=Path)
    p.add_argument('--group-regex',default=r'^(f[0-2]-s\d+)-')
    p.add_argument('--jobs',type=int,choices=range(1,5),default=4)
    p.add_argument('--batch-size',type=int,default=256)
    p.add_argument('--case-list',type=Path,help='Optional selected case stems; probe only, expected-count must match selection.')
    a=p.parse_args()
    a.source,a.decoder,a.out=a.source.resolve(),a.decoder.resolve(),a.out.resolve()
    assert 1<=a.batch_size<=512
    paths=sorted(a.source.glob('*.av2'))
    if a.case_list:
        cases=json.loads(a.case_list.read_text(encoding='utf-8'))
        paths=[a.source/(case+'.av2') for case in cases]
    assert len(paths)==a.expected_count and len({x.stem for x in paths})==len(paths)
    if a.expected_case_list:
        expected=json.loads(a.expected_case_list.read_text(encoding='utf-8'))
        assert {x.stem for x in paths}==set(expected) and len(expected)==len(paths)
    grouped={}
    for path in paths:
        match=re.search(a.group_regex,path.stem)
        assert match,('filename group mismatch',path.name)
        key=match.group(1)
        assert re.match(r'^f[0-2]-',key),('format must be f0=8b420/f1=10b420/f2=10b444',key)
        assert path.with_suffix('.yuv').is_file(),('missing expected recon',path)
        grouped.setdefault(key,[]).append(path)
    a.out.mkdir(parents=True,exist_ok=False)
    batches=[(group,n//a.batch_size,items[n:n+a.batch_size]) for group,items in grouped.items()
             for n in range(0,len(items),a.batch_size)]
    result=dict(status='RUNNING',source=str(a.source),decoder_path=str(a.decoder),
                decoder_sha256=digest(a.decoder.read_bytes()),script_sha256=digest(Path(__file__).read_bytes()),
                expected_count=a.expected_count,jobs=a.jobs,batch_size=a.batch_size,
                method='Complete independent still streams retain every sequence header; exact per-frame raw slices are compared. Grouped by caller-verified stable format/dimensions.',
                group_regex=a.group_regex,batches=[],rows=[])
    def check(batch):
        group,n,items=batch
        tag=f'{group}-batch{n:04d}'
        stream,raw=a.out/(tag+'.av2'),a.out/(tag+'.decoded.yuv')
        record=dict(batch=tag,cases=[x.stem for x in items])
        rows=[]
        try:
            encoded=[x.read_bytes() for x in items]
            expected=[x.with_suffix('.yuv').read_bytes() for x in items]
            assert len({len(x) for x in expected})==1,('group frame lengths differ',group)
            data=b''.join(encoded)
            stream.write_bytes(data)
            command=[str(a.decoder),'--threads=1','--rawvideo',
                     '--output-bit-depth='+('8' if group.startswith('f0-') else '10'),'-o',str(raw),str(stream)]
            record.update(command=command,stream_sha256=digest(data))
            with stream.with_suffix('.decode.log').open('wb') as log:
                run=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=300)
            record['decoder_exit']=run.returncode
            assert run.returncode==0,('decoder exit',run.returncode)
            decoded=raw.read_bytes()
            offset=0
            for path,bitstream,recon in zip(items,encoded,expected):
                actual=decoded[offset:offset+len(recon)]
                row=dict(case=path.stem,batch=tag,decoded_byte_offset=offset,decoded_bytes=len(actual),
                         expected_bytes=len(recon),stream_sha256=digest(bitstream),recon_sha256=digest(recon),
                         decoded_sha256=digest(actual),status='PASS' if actual==recon else 'FAIL')
                if actual!=recon:
                    row['first_mismatch_byte']=next((i for i,(x,y) in enumerate(zip(actual,recon)) if x!=y),None)
                rows.append(row)
                offset+=len(recon)
            record.update(decoded_sha256=digest(decoded),decoded_bytes=len(decoded),expected_bytes=offset,
                          status='PASS' if offset==len(decoded) and all(r['status']=='PASS' for r in rows) else 'FAIL')
        except Exception as error:
            record.update(status='FAIL',error=str(error))
        (a.out/(tag+'.result.json')).write_text(json.dumps(dict(record=record,rows=rows),indent=2),encoding='utf-8')
        return record,rows
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        for future in as_completed([pool.submit(check,batch) for batch in batches]):
            record,rows=future.result()
            result['batches'].append(record)
            result['rows'].extend(rows)
            print(record['status'],record['batch'],len(rows),record.get('error',''),flush=True)
    result['rows'].sort(key=lambda x:x['case'])
    result['status']='PASS' if len(result['rows'])==a.expected_count and all(b['status']=='PASS' for b in result['batches']) else 'FAIL'
    result['frames']=len(result['rows'])
    result['failed_batches']=sum(b['status']!='PASS' for b in result['batches'])
    (a.out/'result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(result['status'],len(result['rows']),'frames,',result['failed_batches'],'failed batches',flush=True)
    return result['status']!='PASS'


if __name__=='__main__':
    raise SystemExit(main())
