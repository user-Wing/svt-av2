"""Find the least rate for dual-quality or primary-V1 constraints without extrapolation."""
import math
import numpy as np
from scipy.interpolate import PchipInterpolator
from scipy.optimize import brentq

class QualityCurve:
    def __init__(self,rows,method):
        self.rows=sorted(rows,key=lambda p:p['bytes'])
        self.x=np.log([r['bytes'] for r in self.rows])
        assert len(self.x)>=2 and np.all(np.diff(self.x)>0)
        self.y={m:np.array([r[m] for r in self.rows]) for m in ['v1','neg']}
        self.f={m:PchipInterpolator(self.x,y,extrapolate=False) if method=='pchip'
                else lambda x,y=y:np.interp(x,self.x,y,left=np.nan,right=np.nan) for m,y in self.y.items()}

    def scores(self,x):
        assert self.x[0]-1e-10<=x<=self.x[-1]+1e-10
        return {m:float(f(x)) for m,f in self.f.items()}

    def required(self,targets):
        # PCHIP is monotone within each segment, even if the complete curve reverses.
        # Intersect each metric's feasible interval; never sort points by quality.
        for i in range(len(self.x)-1):
            left,right=float(self.x[i]),float(self.x[i+1])
            lo,hi=left,right
            for m,target in targets.items():
                a,b=self.y[m][i],self.y[m][i+1]
                if max(a,b)<target-1e-10:
                    lo,hi=1.0,0.0;break
                if min(a,b)>=target:continue
                if min(a,b)==max(a,b):continue
                crossing=brentq(lambda x:float(self.f[m](x))-target,left,right) if min(a,b)<target<max(a,b) else (left if abs(a-target)<1e-10 else right)
                if a<b:lo=max(lo,crossing)
                else:hi=min(hi,crossing)
            if lo<=hi+1e-10:
                scores=self.scores(lo)
                assert all(scores[m]>=target-1e-8 for m,target in targets.items())
                return {'bytes':math.exp(lo),'scores':scores,
                        'binding_metrics':[m for m,target in targets.items() if abs(scores[m]-target)<1e-7],
                        'clamped_at_lowest_measured_rate':bool(abs(lo-self.x[0])<1e-10)}
        return None

def compare_joint(candidate,reference,method,band=(93,97),constraints=('v1','neg')):
    low,high=band
    assert low<high
    assert constraints in [('v1','neg'),('v1',)]
    cand,ref=QualityCurve(candidate,method),QualityCurve(reference,method)
    assert np.all(np.diff(ref.y['v1'])>0), 'Reference V1 must increase with rate'
    reference_covered=ref.y['v1'][0]<=low and ref.y['v1'][-1]>=high
    direct=[]
    for row in reference:
        if not low<=row['v1']<=high:continue
        required=cand.required({m:row[m] for m in constraints})
        direct.append({'aom_q':row['q'],'reference_v1':row['v1'],'reference_bytes':row['bytes'],'targets':{m:row[m] for m in ['v1','neg']},
                       'candidate':required,'ratio':required['bytes']/row['bytes'] if required else None})
    # Uniform V1 grid; reference NEG is evaluated on the SAME reference-rate curve.
    samples=[]
    for target_v1 in np.linspace(low,high,801):
        if not ref.y['v1'][0]<=target_v1<=ref.y['v1'][-1]:continue
        anchor=ref.required({'v1':float(target_v1)})
        if anchor is None:continue
        required=cand.required({m:anchor['scores'][m] for m in constraints})
        samples.append({'reference_v1':float(target_v1),'reference_bytes':anchor['bytes'],
            'targets':anchor['scores'],'candidate':required,'ratio':required['bytes']/anchor['bytes'] if required else None})
    checked=samples+direct
    covered=bool(reference_covered) and len(samples)==801 and all(p['ratio'] is not None for p in checked)
    summary={'band':[low,high],'enforced_metrics':list(constraints),'sample_count':len(samples),'full_coverage':covered,
             'direct_aom_points':direct,'samples':samples}
    if covered:
        ratios=np.array([p['ratio'] for p in samples]);q=np.array([p['reference_v1'] for p in samples])
        mean=math.exp(float(np.trapezoid(np.log(ratios),q)/(q[-1]-q[0])))
        summary.update(mean_ratio=mean,worst_sample=max(checked,key=lambda p:p['ratio']),
            mean_at_most_095=mean<=.95,mean_at_most_090=mean<=.90,
            mean_at_most_085=mean<=.85,mean_at_most_080=mean<=.80,
            every_sample_at_most_095=all(p['ratio']<=.95 for p in checked),every_sample_at_most_090=all(p['ratio']<=.90 for p in checked),
            every_sample_at_most_085=all(p['ratio']<=.85 for p in checked),every_sample_at_most_080=all(p['ratio']<=.80 for p in checked))
        if constraints==('v1',):
            delta=lambda p:p['candidate']['scores']['neg']-p['targets']['neg']
            summary['neg_delta_at_selected_rate']={
                'mean':float(np.trapezoid([delta(p) for p in samples],q)/(q[-1]-q[0])),
                'worst_sample':min(delta(p) for p in checked),
                'every_sample_nonnegative':all(delta(p)>=-1e-8 for p in checked)}
    return summary

if __name__=='__main__':
    rows=[{'q':3,'bytes':100,'v1':94,'neg':90},{'q':2,'bytes':200,'v1':93,'neg':92},{'q':1,'bytes':400,'v1':95,'neg':91}]
    curve=QualityCurve(rows,'linear')
    assert abs(curve.required({'v1':94,'neg':91.5})['bytes']-math.sqrt(200*400))<1e-8
    assert curve.required({'v1':95,'neg':92}) is None
    assert curve.required({'v1':92,'neg':89})['bytes']==math.exp(math.log(100))
    for method in ['linear','pchip']:
        monotone=[{'q':3,'bytes':100,'v1':92,'neg':90},{'q':2,'bytes':200,'v1':94,'neg':92},{'q':1,'bytes':400,'v1':98,'neg':96}]
        result=compare_joint(monotone,monotone,method)
        assert result['full_coverage'] and abs(result['mean_ratio']-1)<1e-10
        assert abs(result['worst_sample']['ratio']-1)<1e-10
    assert not compare_joint(monotone,monotone[1:],'linear')['full_coverage']
    reduced_neg=[dict(p,neg=p['neg']-5) for p in monotone]
    for method in ['linear','pchip']:
        single=compare_joint(reduced_neg,monotone,method,constraints=('v1',))
        assert single['full_coverage'] and abs(single['mean_ratio']-1)<1e-10
        assert abs(single['neg_delta_at_selected_rate']['mean']+5)<1e-10
        assert not single['neg_delta_at_selected_rate']['every_sample_nonnegative']
        assert not compare_joint(reduced_neg,monotone,method)['full_coverage']
    print('PASS: joint constraints, nonmonotone intervals, no extrapolation, self-comparison')
