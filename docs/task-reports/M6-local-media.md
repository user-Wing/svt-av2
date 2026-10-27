# M6 用户本地素材预处理

2026-10-17。原始六个 MKV 只读，派生文件统一放在 `C:/Private/ffmpeg-native-m6-local-media`。现有 `C:/PortableSoft/FFmpegFreeUI ReadyToRun x64/ffmpeg.exe` 已包含软件 H264、AV1 解码和 libvmaf，无须为解码重新编译 FFmpeg。

| 类型 | H264 参考像素 | SVT-AV1 对照片像素 | 尺寸 | 帧数 | 标称帧率 |
|---|---|---|---|---:|---|
| 2D | yuv420p | yuv420p10le | 1920×1080 | 383 | 24000/1001 |
| 3D | yuv420p | yuv420p10le | 1920×1080 | 378 | 24000/1001 |
| Real | yuv420p | yuv420p10le | 1920×1080 | 390 | 24000/1001 |

`tools/prepare_m6_local.py` 逐帧探测与配对。三对帧数相等、逐帧呈现时间差均不超过1毫秒。两端都在尾部存在时间戳间隙：2D 最后间隔0.125秒，3D/Real 约0.083秒。相应原始时间戳完整落盘；规范参考按解码帧序列和标称帧率比较，每帧保留一次，不为间隙复制或删除帧。由此统一使用 `frames / fps` 计算 AV1/AV2 视频负载码率；原容器时长和大小单独记录，不能混为裸视频大小或播放时间完全相同。

主参考统一原分辨率10bit420 Y4M；8bit 升10bit只改变存储精度，不增加原始精度。原H264也是经过压缩的视频，所以质量以其解码帧为参考，并非相机母版。3D/Real 带 BT.709 limited 元数据；2D 缺少矩阵/原色/传递标签，源还缺range标签，保留缺失事实，不静默猜测或进行色彩矩阵变换。

输出：

- `local-corpus.json`：三片完整序列；无缩放、无降帧。
- `pilot-corpus.json`：每片前3帧，原1920×1080/10bit420。
- `speed-corpus.json`：每片前24帧，原尺寸，约1.001秒；仅供隔离单路计时，不能代替完整序列时间。
- `small-corpus.json`：每片前5帧，256×144，8bit420/10bit420/10bit444，Lanczos保比例缩放加居中黑边，仅用于小尺寸参数正确性。
- `pairs.json`：顶层列表，含scene/source_path/av1_path/reference_y4m/frames/fps/width/height/depth/chroma，以及原文件SHA256、逐帧probe、源与AV1逐包视频payload字节数、配对对齐合同。
- 各Y4M同名JSON：完整原文件SHA、源探测、派生命令、工具SHA、派生SHA和元数据；`prepare-status.json`给出完成/失败状态。

CLI：

```powershell
$env:PYTHONPATH=(Resolve-Path build/m5-root/pydeps).Path
python tools/prepare_m6_local.py --input-dir ../Test-Video/1080p-15s --root C:/Private/ffmpeg-native-m6-local-media --ffmpeg 'C:/PortableSoft/FFmpegFreeUI ReadyToRun x64/ffmpeg.exe' --ffprobe 'C:/PortableSoft/FFmpegFreeUI ReadyToRun x64/ffprobe.exe'
```

加 `--pilot-only` 仅生成短验及小图素材，清单文件带 `-preflight`，不会冒充完整参考。2026-10-17 本轮此短预检已通过三片；尚未在预检阶段生成完整帧派生，完整准备交后台统一阶段执行。重复执行会复核输入、工具及已有派生SHA，发现改变即拒绝复用。
