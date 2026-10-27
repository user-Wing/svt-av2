"""Collect audited M6 points and render standalone rate/quality/time figures."""
import argparse
import json
from pathlib import Path

from audit_m6_batch import audit
from m53_batch import atomic_json


def collect(campaign):
    config = json.loads((campaign / 'campaign-config.json').read_text())
    output = campaign / 'reports'
    output.mkdir(exist_ok=True)
    all_points, bd, reports = [], [], []
    for stage in config['stages']:
        if stage.get('batch_directory'):
            batch = Path(stage['batch_directory'])
            verification = audit(batch)
            meta = json.loads((batch / 'run-manifest.json').read_text())
            summary = json.loads((batch / 'summary.json').read_text())
            reports.append(dict(stage=stage['name'], **verification))
            for row in json.loads((batch / 'acceptance.json').read_text()):
                source = meta['sources'][row['job']['source']]
                times = row['encoding_seconds']
                point = dict(batch=stage['name'],
                    source_sha256=source['sha256'], scene=source.get('scene'),
                    role=source.get('split', source.get('role', 'synthetic')), category=source.get('category'),
                    width=source['width'], height=source['height'],
                    frames=row['frames'], depth=source['depth'], chroma=source['chroma'],
                    **row['job'], bytes=row['bytes'], bitrate_kbps=row['bitrate_kbps'],
                    vmaf=row['vmaf'], psnr_yuv_db=row['quality']['psnr_yuv_db'],
                    median_seconds=row['median_seconds'], seconds_min=min(times), seconds_max=max(times),
                    fps=row['fps'], peak_rss_bytes=row['peak_rss_bytes'],
                    candidates=sum(x['candidates'] for x in row['rd_frames']),
                    forward_calls=sum(x['forward'] for x in row['rd_frames']),
                    distortion=sum(x['D'] for x in row['rd_frames']),
                    rate_q12=sum(x['Rq12'] for x in row['rd_frames']),
                    cost=sum(x['J'] for x in row['rd_frames']))
                # Preserve the human-readable source name and separate its index.
                point['source_index'] = row['job']['source']
                point['source'] = Path(source['path']).stem
                all_points.append(point)
            bd.extend(dict(batch=stage['name'], **r) for r in summary['bd_rate'])
    atomic_json(output / 'points.json', all_points)
    atomic_json(output / 'bd-rate.json', bd)
    lines = ['# M6 批次自动效果报告', '',
        '本报告表示已完成的测试数据，M6正式验收仍待AI独立复核。真实输入来源、既有压缩、升位深/色度转换等限制以媒体manifest为准。', '',
        '编码时间包含CLI读写与重建导出，不包含固定AVM解码或VMAF；并发批次不能用于单路速度排名。不同素材、分辨率和预测结构分别比较。', '']
    for report in reports:
        lines.append(f"- {report['stage']}: {report['jobs']}项，通过审计；{report['total_frames']}帧，重复{report['repeats']}次。")
    plots = []
    for batch_name in ['real-quality-speed', 'small-exhaustive']:
        points = [p for p in all_points if p['batch'] == batch_name]
        groups = sorted({(p['source'], p['prediction'], p['partition'], p['intra']) for p in points})
        if not groups:
            continue
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        for number, (name, prediction, partition, intra) in enumerate(groups):
            group = [p for p in points if (p['source'], p['prediction'], p['partition'], p['intra']) == (name, prediction, partition, intra)]
            lines += ['', f'## {batch_name}: {name} / {prediction}', '',
                '| 模式 | QP | 体积MB | kb/s | VMAF | PSNR-Y | 秒（中位数） | 秒（最小–最大） |',
                '|---|---:|---:|---:|---:|---:|---:|---|']
            for p in sorted(group, key=lambda p: (p['rd_mode'], p['qp'])):
                v = 'N/A' if p['vmaf'] is None else f"{p['vmaf']:.3f}"
                psnr = 'inf' if p['psnr_yuv_db'][0] is None else f"{p['psnr_yuv_db'][0]:.3f}"
                lines.append(f"| {p['rd_mode']} | {p['qp']} | {p['bytes']/1e6:.3f} | {p['bitrate_kbps']:.1f} | {v} | {psnr} | {p['median_seconds']:.3f} | {p['seconds_min']:.3f}–{p['seconds_max']:.3f} |")
            fig, axes = plt.subplots(1, 3, figsize=(15, 4.2))
            for mode in sorted({p['rd_mode'] for p in group}):
                curve = sorted([p for p in group if p['rd_mode'] == mode], key=lambda p: p['qp'])
                for ax, xkey, ykey in [(axes[0], 'bitrate_kbps', 'vmaf'),
                                      (axes[1], 'median_seconds', 'vmaf'),
                                      (axes[2], 'bitrate_kbps', 'median_seconds')]:
                    valid = [p for p in curve if p[ykey] is not None]
                    ax.plot([p[xkey] for p in valid], [p[ykey] for p in valid], 'o-', label=mode)
            for ax, xlabel, ylabel in zip(axes, ['Bitrate (kb/s)', 'Encode time (s)', 'Bitrate (kb/s)'], ['VMAF', 'VMAF', 'Encode time (s)']):
                ax.set_xlabel(xlabel); ax.set_ylabel(ylabel); ax.grid(alpha=0.3); ax.legend()
            fig.suptitle(name + ' / ' + prediction)
            fig.tight_layout()
            filename = f'{batch_name}-{number:02}.svg'
            fig.savefig(output / filename); plt.close(fig)
            lines += ['', f'![{name} {prediction}]({filename})']
            plots.append(filename)
    valid = [r['percent'] for r in bd if r['percent'] is not None]
    lines += ['', '## 多点数据', '',
        f'BD-rate记录共{len(bd)}条，其中{len(valid)}条有效，其余保留N/A原因；不同片/结构/工具/度量不可混成一条总收益。', '',
        '全部点见points.json，多点积分结果及原因见bd-rate.json。当前图为实测离散QP点连线，不代表在中间QP新增测量。']
    (output / 'README.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    atomic_json(output / 'summary.json', dict(status='PASS', batches=reports,
        points=len(all_points), bd_records=len(bd), valid_bd_records=len(valid), plots=plots))
    print('REPORT PASS', len(all_points), 'points', len(plots), 'figures')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--campaign', type=Path, required=True)
    collect(parser.parse_args().campaign.resolve())
