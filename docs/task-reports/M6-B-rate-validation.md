# M6 B：语法 rate、事务与批测交付

2026-10-17，状态 `READY_FOR_INTEGRATION`。本报告不代表 M6 DONE；正式矩阵、真实视频多点测量、跨平台和长链由 00 统一批量放行。本轮未启动长测。

## 生产 rate 与事务

`entropy_encoder.c/h` 增加 rate-only 通道，复用每一个现有 `encode_symbol/literal` 调用。R 为 forward CDF 所选概率区间的 `-log2(p)` Q12 向上截断估计，4096 单位/bit；literal 精确按位计入。整数逐次平方实现，不依赖 libm。符号成本与实际 range coder 的概率量化、Prob_Inc、终止和 carry 存在差别，不能称为最终字节精确增量。固定帧头/OBU 不计入局部 R，效果测量统计完整 AV2 字节。

`tile_writer.c/h` 的 opaque `SvtAv2TileRateSession` 每帧初始化一次，按 raster SB 顺序执行 trial/commit。生产计量直接调用 `tile_tree_write`，覆盖分区、intra/inter、MV、UV、skip、EOB、系数扩展及符号。固定邻居数组按值试探；motion 共享数组仅备份当前 SB 有效的最多 16×16 cells，并在非提交或失败时恢复。无每候选 malloc、不重放整帧前缀、不触碰 arithmetic 输出缓冲。commit 仅成功后推进一次。`svt_av2_tile_rate_frame` 独立全帧重放用于核对各 SB R 之和。

接口已与 A/00 对接，定义在 `Source/Lib/Bitstream/tile_writer.h`。RD off 的旧正式 writer 默认关闭计量，不改变原算术编码步骤。

## 已执行验证

- MSVC `/W4 /WX` 和 GCC `-Wall -Wextra -Wpedantic -Wshadow -Werror` 独立重建通过；`test_m6_rate` 两平台均 **98534 checks / 0 failures**。
- 全部 32768 个合法概率对 libm 独立真值误差小于 1.001 Q12 单位，单调、0 概率和非法范围拒绝。
- 三格式 AI/P 双 SB 的 A→B→A、Y/运动写入后 U 几何故障回滚、干净会话对照、重复 commit 拒绝；完整四32分区和 56×40 partial 原树；SB 成本之和与整帧同语法重放一致。
- `tools/test_m6_batch.py` 的 **8 项自测通过**：缺失/错模式/错帧/无重放 trace、BD 非法曲线、冻结依赖变化、缓存损坏/不同 RD 模式、矩阵 RD 维度。
- `artifacts/m6-rate/smoke-final`：M5 / M6 off / exhaustive / pruned 共4项，三次编码确定性、固定 AVM 解码逐样本重建及 VMAF通过，独立审计通过。
- 同目录 `resume-proof.json`：不变4项全部复用；破坏一个保留码流只重跑一项；QP/config 改变拒绝复用。旧故障尝试保留供追溯。
- A 对生产概率通道、TileTree 和 motion 回滚进行了只读交叉审查，未发现阻塞缺陷。

## 批测接口

新增 `tools/m6_batch.py`，保留 M5.3 低层读取、逐帧比较、VMAF、PCHIP、进程采样和锁的实现，未修改旧 M5.3 工具。

```powershell
python tools/m6_batch.py --app NEW.exe --baseline-app M5.exe --decoder avmdec.exe --corpus corpus.json --out-dir results --qps 40,80,100,160,220 --predictions all-intra,low-delay-p --tools 11 --rd-modes pruned --jobs 1 --repeats 3 --ffmpeg ffmpeg.exe --vmaf-model model.json --contract docs/m6-rd-contracts.md
python tools/audit_m6_batch.py --out-dir results
```

`--baseline-app` 自动增加 `rd_mode=m5` 任务，不给旧程序传 `--rd-mode`；其余 `--rd-modes off,exhaustive,pruned`。`--resume` 要求配置/源/脚本/程序/DLL/模型/合同/来源记录指纹完全相同。

manifest schema=6，沿用 sources/app/decoder/ffmpeg/vmaf_model/qps/predictions/tools/repeats/jobs，增加 baseline_app/rd_modes/frozen_files。job 增加 `rd_mode`。产物为 run-manifest.json、cases/ID/result.json、acceptance.json、summary.json、resource-estimate.json 和 audit.json。审计重算覆盖、trace、指标及 BD-rate，不接受 RUNNING 或冻结依赖错误。

每点包含完整字节/码率、实际 qindex、Y/U/V PSNR、VMAF、三次 CLI 总时间/中位数/范围、fps、采样峰值 RSS 及各帧 RD 统计。CLI 编码时间包括输入读取和 recon 导出，排除 AVM/VMAF；本工具未提供 API 核心耗时。jobs=1 仅保证当前 runner 内单路，总控仍须确保没有其他编码负载。成功后保留码流/JSON/log，逐帧核验后清除 recon/decoded 和重复流。

## 外置真实视频准备

`tools/m6_real_sources.json` 固定九个 Netflix 官方 QP15 参考文件，下载总量 **826908953 字节**。六 test、两 holdout 和一个独立 calibration；选择在正式测试前冻结，不按编码结果挑样本。每个对象固定官方 S3 size/ETag；官方未提供 SHA256，先验证 HTTPS 对象和 multipart ETag，再冻结本地 SHA256。不能把 multipart ETag 误称为官方 SHA256。

来源：[Netflix Open Content](https://opencontent.netflix.com/)，其主页声明 CC BY 4.0；Cosmos 同时记录 [Blender 的 CC BY 3.0](https://gooseberry.blender.org/license/)。原始对象的 QP15 码流已有压缩，所有被测编码器均使用同一解码后的 Y4M，结果只表示该输入的再编码性能。不是无损相机母版的压缩性能。

```powershell
python tools/prepare_m6_real.py --root C:/Private/ffmpeg-native-m6-media --ffmpeg ffmpeg.exe --ffprobe ffprobe.exe
```

完整输出：

- `real720-corpus.json`：八片（六 test、两 holdout），各 1280×720 / 25fps / 125帧 / 10bit420。
- `calibration-corpus.json`：独立一片，同规格，不与测试片重合。
- `pilot-corpus.json`：前两 test 各1秒720p，用于成本/时间预估。
- `small-corpus.json`：前两 test 各 128×128 / 5帧 / 三格式，穷举和剪枝对照。

原比例 Lanczos 缩放、8倍数对齐与居中黑边均写入转换记录；SDR BT.709 limited 是显式解释，HDR PQ/HLG 输入直接拒绝。记录 ffprobe、转换命令、输入/派生 hash、范围和色彩假设。420→444 不增加原始色度信息，8→10bit 不增加原始精度；另需 00 的原生图形专项，不能用升采样实拍片替代。

`--only narrator --pilot-only` 已真实短验通过，缓存重跑通过，尚未准备其余八源。Netflix Narrator 原始 QP15 文件 SHA256 `092e14cc628f31a6b33301eb06d6fc1708a5337da7cceb5f1b9b05f9646ac3a8`，实际原片为 10bit420，720p1秒派生 SHA256 `42df2fada2247ec655082a17843061a9c5c455b5781c71aa01321dbc65937e6b`。预检清单命名为 `pilot-preflight-corpus.json` / `small-preflight-corpus.json`，不会覆盖正式清单。

其余素材的相机移动/低照分类仍待下载后的缩略图复核。不能把拟定标签当成六类已经覆盖；若不满足，00 后续应在正式测量前换成可核验来源并重新冻结，而不是测完再改。下载有 Range 断点和可恢复网络错误最多3次重试；hash/ETag/config 不符立即失败。失败写 prepare-status=FAIL，绝不沿用旧 PASS。

## 交接与限制

源码清单在 `artifacts/m6-rate/source-manifest.json`。00 负责快照总控、自动曲线、跨平台重放、完整正确性与长测，不应把本模块短验扩写成 M6 全部完成。M5 正式包、旧候选证据、oracle 和规范未改。本轮未启动完整素材准备或长时间编码矩阵。
