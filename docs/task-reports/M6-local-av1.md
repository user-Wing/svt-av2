# M6 用户本地 AV1 对照测量工具

2026-10-17：完成测量工具，正式三片长测由总控调度，本报告不预填长测结果。

## 接口与依赖

`tools/m6_av1_reference.py` 使用媒体预处理生成的顶层列表 `pairs.json`，每行包含 `scene/source_path/source_sha256/av1_path/reference_y4m/frames/fps/width/height/depth/chroma`；支持可选 `video_payload_bytes/av1_sha256/frame_alignment`。

已实测现有 FFmpeg 支持 H264 软件解码、AV1 libdav1d/libaom 解码、libvmaf、SVT-AV1 v4.2.0 编码。无需为本次输入重新编译 FFmpeg。1 帧烟测证实 `-threads 1 -svtav1-params lp=1` 生效，日志报告 `Level of Parallelism: 1`。

```powershell
python tools/m6_av1_reference.py --pairs PAIRS_JSON --ffmpeg FFMPEG_EXE --ffprobe FFPROBE_EXE --vmaf-model VMAF_JSON --out-dir EXISTING_POINTS
python tools/m6_av1_reference.py --pairs PAIRS_JSON --ffmpeg FFMPEG_EXE --ffprobe FFPROBE_EXE --vmaf-model VMAF_JSON --out-dir FRESH_CURVE --crfs 28,32,36,40,44,48 --preset 6
```

第二条命令只生成新鲜 AV1 曲线，不混入既有成片点；两条命令必须使用独立输出目录。重复执行加 `--resume`，冻结配置、媒体、模型、程序及其同目录 DLL、测量脚本或保留日志发生变化会拒绝复用或重新测量。

## 度量合同

- 与 AV2 使用同一 H264 原片解码后的 10bit420 canonical Y4M，无缩放；8bit 源升至 10bit 不增加原始精度，H264 源也并非无损母版。
- AV1 解码使用 `-fps_mode passthrough`，不补帧、不丢帧，严格核对帧数、格式、分辨率和帧率；PSNR 按完整帧索引及三个原生平面计算，VMAF 读取双方 canonical 帧序列，重置 PTS。
- 码量为 `ffprobe v:0 packet.size` 之和，排除音频与容器，另报容器总字节。时长统一按 `frames/nativefps`，保留原始 PTS 对齐与尾端间隙说明，避免将容器时长与编码帧率混用。
- 用户既有 SVT-AV1 p6 CRF36 点保留 `encoding_seconds/median_seconds/fps=None`，不虚构原编码耗时、线程或结构元数据。用户文件名和说明提供 preset/CRF 来源。
- 可选新鲜曲线为 p6、单线程 `lp=1`、默认 RA 结构、每点编码一次；其时间包含 Y4M 读取与容器写入，不含 AV2 所需的重建导出，耗时比较需注明该差异。RA 对比当前 AV2 AI/低延迟 P 属于不同配置比较，不能泛化为 AV2 标准优势。
- 每片仅一个既有 AV1 点不能计算 AV1/AV2 BD-rate。新鲜曲线与既有成片分开，只有共同质量区间、足够有效单调点时才可计算 BD-rate。

输出 `points.json`、`summary.json`、`run-manifest.json`、`README.md` 与各点保留日志。点数据含 `origin`（`user-existing/fresh-single-thread`）、`bytes/bitrate_kbps/vmaf/quality.psnr_yuv_db/duration_seconds/frames/source_sha256/canonical_sha256/av1_sha256` 等，可由总报告叠加到 AV2 体积—质量曲线。

## 已执行短验

4 项单测通过，覆盖已知平面误差、缺帧/多帧/格式拒绝、依赖变更与损坏产物拒绝复用。

微型 H264→canonical→AV1 完整链路分别验证既有点和新鲜 CRF36 点，两者均 PASS，随后各自 `--resume` PASS。短验证据位于 `C:/Private/ffmpeg-native-m6-tests/av1-reference-pilot-e8db5ba8`；该微型素材仅证明工具接线，不作为实际压缩收益。
