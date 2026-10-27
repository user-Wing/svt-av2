"""Audit and plot the user-source M6/AV1 continuation without mixing references."""
import argparse
import json
import math
from pathlib import Path

from audit_m6_batch import audit
from m53_batch import atomic_json, bd_rate, digest
from m6_av1_reference import reusable as av1_reusable, validate as av1_validate


def av1_points(folder):
    meta = json.loads((folder / 'run-manifest.json').read_text())
    summary = json.loads((folder / 'summary.json').read_text())
    run_id = digest(folder / 'run-manifest.json')
    assert summary['status'] == 'PASS' and summary['run_id'] == run_id
    av1_validate(meta)
    rows = json.loads((folder / 'points.json').read_text())
    assert len(rows) == summary['jobs'] == summary['passed']
    for row in rows:
        name = row['scene'] + (f"-crf{row['crf']}" if row['origin'] == 'fresh-single-thread' else '')
        result = folder / name / 'result.json'
        assert av1_reusable(result, run_id) and json.loads(result.read_text()) == row
        assert all(c['exit'] == 0 for c in row['commands'])
        vmaf_path = next(Path(x['path']) for x in row['retained_artifacts'] if Path(x['path']).name == 'vmaf.json')
        metrics = json.loads(vmaf_path.read_text())
        assert len(metrics['frames']) == row['frames']
        assert metrics['pooled_metrics']['vmaf']['mean'] == row['vmaf']
        for mse, psnr in zip(row['quality']['mse_yuv'], row['quality']['psnr_yuv_db']):
            assert psnr is None if mse == 0 else math.isclose(psnr, 10*math.log10(1023**2/mse), abs_tol=1e-8)
    return rows


def collect(run):
    config = json.loads((run / 'campaign-config.json').read_text())
    out = run / 'reports'
    out.mkdir(exist_ok=True)
    points, reports = [], []
    for stage in config['stages']:
        if not stage.get('batch_directory'):
            continue
        folder = Path(stage['batch_directory'])
        verified = audit(folder)
        reports.append(dict(batch=stage['name'], **verified))
        meta = json.loads((folder / 'run-manifest.json').read_text())
        for row in json.loads((folder / 'acceptance.json').read_text()):
            source = meta['sources'][row['job']['source']]
            points.append(dict(batch=stage['name'], scene=source['scene'],
                source_label=Path(source['path']).stem, canonical_sha256=source['sha256'],
                width=source['width'], height=source['height'], depth=source['depth'], chroma=source['chroma'],
                **row['job'], frames=row['frames'], bytes=row['bytes'], bitrate_kbps=row['bitrate_kbps'],
                vmaf=row['vmaf'], psnr_yuv_db=row['quality']['psnr_yuv_db'],
                median_seconds=row['median_seconds'], seconds_min=row['seconds_min'], seconds_max=row['seconds_max'],
                fps=row['fps'], peak_rss_bytes=row['peak_rss_bytes'], jobs=meta['jobs'], repeats=meta['repeats']))
    existing = av1_points(run / 'av1-existing')
    fresh = av1_points(run / 'av1-fresh')
    atomic_json(out / 'av2-points.json', points)
    atomic_json(out / 'av1-points.json', existing + fresh)
    lines = ['# 本地原片 AV2 / AV1 对照', '',
        '三组H264原片解码后的相同帧序列为参考，完整1080p、不缩放或抽帧；统一10bit420，8bit升位深不增加原始精度。Y4M按帧序列与标称帧率归一，原容器尾部时间戳间隙见pairs.json。', '',
        'AV1用户点为现成p6 CRF36，原始耗时和线程设置未知；fresh曲线是p6、lp1的新单次编码，默认RA。AV2是当前低延迟P或AI。比较结论限这些实现/配置与三片，不代表编码标准总体优劣。', '',
        '完整质量批次并发运行，耗时不能用于单路排名。isolated-speed为每片前24帧、jobs1、三次重复，不能把这段时间当成完整片耗时；AV2 CLI还包含导出重建，AV1编码时间没有该项。', '']
    for report in reports:
        lines.append(f"- {report['batch']}: {report['jobs']}项审计通过，{report['total_frames']}帧，重复{report['repeats']}次。")
    comparisons, figures = [], []
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    for batch in ['real-quality', 'real-ai', 'isolated-speed']:
        selected = [p for p in points if p['batch'] == batch]
        for scene in sorted({p['scene'] for p in selected}):
            group = [p for p in selected if p['scene'] == scene]
            if not group:
                continue
            lines += ['', f'## {batch} / {scene}', '',
                '| 编码模式 | QP/CRF | 视频MB | kb/s | VMAF | PSNR-Y | 编码秒数 |',
                '|---|---:|---:|---:|---:|---:|---|']
            fig, axes = plt.subplots(1, 2, figsize=(11, 4.3))
            for mode in sorted({p['rd_mode'] for p in group}):
                curve = sorted([p for p in group if p['rd_mode'] == mode], key=lambda p: p['qp'])
                axes[0].plot([p['bitrate_kbps'] for p in curve], [p['vmaf'] for p in curve], 'o-', label='AV2 ' + mode)
                y = 'median_seconds' if batch == 'isolated-speed' else 'psnr_yuv_db'
                axes[1].plot([p['vmaf'] for p in curve],
                    [p[y] if y != 'psnr_yuv_db' else p[y][0] for p in curve], 'o-', label='AV2 ' + mode)
                for p in curve:
                    psnr = 'inf' if p['psnr_yuv_db'][0] is None else f"{p['psnr_yuv_db'][0]:.3f}"
                    timing = f"{p['median_seconds']:.3f} ({p['seconds_min']:.3f}–{p['seconds_max']:.3f})" if batch == 'isolated-speed' else '并发测量，不作速度排名'
                    lines.append(f"| AV2 {mode} | {p['qp']} | {p['bytes']/1e6:.3f} | {p['bitrate_kbps']:.2f} | {p['vmaf']:.3f} | {psnr} | {timing} |")
            if batch != 'isolated-speed':
                reference = next(p for p in existing if p['scene'] == scene)
                assert all(p['canonical_sha256'] == reference['canonical_sha256'] and p['frames'] == reference['frames'] for p in group)
                av1_curve = sorted([p for p in fresh if p['scene'] == scene], key=lambda p: p['crf'])
                assert all(p['canonical_sha256'] == reference['canonical_sha256'] for p in av1_curve)
                axes[0].plot([p['bitrate_kbps'] for p in av1_curve], [p['vmaf'] for p in av1_curve], 's--', label='SVT-AV1 p6 fresh')
                axes[0].scatter([reference['bitrate_kbps']], [reference['vmaf']], marker='*', s=130, label='User AV1 p6 CRF36')
                for p in [reference] + av1_curve:
                    psnr = 'inf' if p['quality']['psnr_yuv_db'][0] is None else f"{p['quality']['psnr_yuv_db'][0]:.3f}"
                    lines.append(f"| AV1 {p['origin']} | {p['crf']} | {p['bytes']/1e6:.3f} | {p['bitrate_kbps']:.2f} | {p['vmaf']:.3f} | {psnr} | {'未知' if p['median_seconds'] is None else '新单次编码 '+str(round(p['median_seconds'],3))} |")
                pruned = [p for p in group if p['rd_mode'] == 'pruned']
                dominates = [p['qp'] for p in pruned if p['bytes'] < reference['bytes'] and p['vmaf'] >= reference['vmaf']]
                result = bd_rate([(p['vmaf'], p['bytes']) for p in av1_curve], [(p['vmaf'], p['bytes']) for p in pruned])
                comparisons.append(dict(batch=batch, scene=scene, metric='VMAF', reference='fresh SVT-AV1 p6 RA', test='AV2 pruned',
                    **result, qps_smaller_and_vmaf_at_least_user_av1=dominates))
                lines += ['', '相对用户既有AV1点，同时更小且VMAF不低的已测AV2 QP：'+(str(dominates) if dominates else '没有。'),
                    '相对fresh AV1曲线的VMAF BD-rate：'+(f"{result['percent']:.3f}%" if result['percent'] is not None else 'N/A，'+result['reason'])+'。']
            axes[0].set_xlabel('Video bitrate (kb/s)'); axes[0].set_ylabel('VMAF')
            axes[0].set_xscale('log')
            axes[1].set_xlabel('VMAF'); axes[1].set_ylabel('Encode time (s), first 24 frames' if batch == 'isolated-speed' else 'PSNR-Y (dB)')
            for ax in axes:
                ax.grid(alpha=.3); ax.legend()
            fig.suptitle(batch + ' / ' + scene); fig.tight_layout()
            name = batch + '-' + scene + '.svg'
            fig.savefig(out / name); plt.close(fig)
            figures.append(name); lines += ['', f'![{batch} {scene}]({name})']
    atomic_json(out / 'cross-codec-comparison.json', comparisons)
    (out / 'README.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    atomic_json(out / 'summary.json', dict(status='PASS', av2_points=len(points), existing_av1_points=len(existing),
        fresh_av1_points=len(fresh), figures=figures, note='Complete source-specific data; independent M6 release review pending.'))
    print('LOCAL REPORT PASS', len(points), 'AV2 points', len(figures), 'figures')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--campaign', type=Path, required=True)
    collect(parser.parse_args().campaign.resolve())
