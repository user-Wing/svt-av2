> **2026-10-25 已恢复速度优化（进行中）**：以已评分 r4c 为体积基线，同画质体积最多回退 1%，优先接近 AOM 单线程 2×耗时。真实熵前缀候选已通过本开发帧原生六点质量审计（相对 r4c −3.537%），更快候选继续测试；MAIN 只合入已验证的等价优化，正式速度尚未达标，见[速度优化进展](docs/task-reports/M6-intra-speed-20261025.md)。下方暂停段落为上一轮历史状态。

> **2026-10-25 已按用户要求暂停**：仅收齐最早r4c六点并通过独立审计，较晚r4d/r4e已中断保留证据，子智能体及测试均结束。当前最好完整2D对AOM的VMAF BD体积为+11.8334%，直接对最初directional省34.4712%；仍未追平。单帧开发CPU2306～3817秒，对照AOM约7～17秒；VMAF90粗略同质量CPU约269倍，2倍目标远未达到。见[速度与体积收尾报告](docs/task-reports/M6-intra-pause-20261025.md)。正式仍0.0.5-m5，I-GATE/holdout20未执行。

> **2026-10-24 单帧压缩第二轮**：执行 [intra-only 合同](docs/m6-intra-contract.md)。新增默认关闭的 `--partition-select fine`（64/32/16，逐树 Y/UV RD）及 `--intra-select directional`（六种零角度偏移斜向亮度 IDIF）。沿用真正 single-picture 和单图 CDF/RD，已完成七张开发/标定图132点质量测量；[本轮报告](docs/task-reports/M6-intra-partition-directional-20261024.md)。AOM 85% 体积尚未达标，2×耗时尚未验收，正式仍0.0.5-m5。

> **2026-10-18：压缩竞争力优先。** 当前以 [单线程压缩路线图](../M5-M6-AI-TASKS/03-M6-single-thread-compression-roadmap.md) 和 [M6-C 合同](docs/m6-compression-contract.md) 为准：压缩能力验收后再做内部多线程、preset、CRF。C1 概率成本内核已通过首轮机制验证，完整自适应 RD 组合仍拒绝；不表示已接近成熟 AV1。正式版本保持 0.0.5-m5。

# SVT-AV2

> **2026-10-18基础能力阶段评估**：按用户指定先补基础、后评估AVM新工具，00+A/B完成本轮。R5运动候选RD、R6有界±16、R7帧内CDF自适应（仅RD-off）均完成三源原生1080p连续1.001秒、四QP的12/12配对质量验收及独立审计；独立运动27次与CDF18次计时全部通过。R5相对旧RD小幅改善、墙时增加17%至30%；R6质量收益近零、再增加约2%至8%墙时；R7同重建省约7%至20%、墙时增加约13%至17%。基础RD在2D/Real仍落后非RD，整体差距未解决，所有候选默认off。见[阶段评估](docs/task-reports/M6-basic-capability-assessment.md)和[基础能力进展](docs/task-reports/basic-capability-progress.md)。原冻结大测试继续，正式仍0.0.5-m5。

> **2026-10-18压缩内核实验**：已否决粗系数清零，新增默认关闭的单平面尾部系数RD选择。严格构建、解码与跨平台短验通过；三帧小图有有限VMAF改进，但PSNR混合、2D平均收益对插值敏感，尚不能外推完整视频或补上8倍差距。见[第四轮评估](docs/task-reports/M6-r4-assessment.md)。原冻结长测继续，09:20左右66/108完整质量PASS；正式仍0.0.5-m5。

> **2026-10-18后台托管更新**：前次恢复后batch再次异常退出（无新重启，根因未定），完整质量65/108 PASS保留。现使用无执行时限的Windows手动计划任务托管原冻结配置，总控PID17392；见[后台托管报告](docs/task-reports/M6-background-host-20261018.md)。正式仍0.0.5-m5，测试未全完。

> **2026-10-18测试恢复**：系统意外重启中断原r2；已有完整质量64/108及前9阶段证据已核验，原冻结配置于07:53:30独立恢复，新总控PID11508。见[恢复报告](docs/task-reports/M6-recovery-20261018.md)。测试未全完，正式仍0.0.5-m5。

> **2026-10-17压缩效率调查**：已有同源2D点显示当前实现严重落后AV1；不把等价提速称为体积问题解决。已增加保持流/recon的外置分类诊断与量化预检优化，独立短验通过，见[后续评估](docs/task-reports/M6-r3-assessment.md)。压缩率根因及新策略仍待受控验证，原冻结长测继续，正式0.0.5-m5未变。

> **2026-10-17 M6第二轮性能优化**：省去非法路径前缀变换及无用SAD，按行复制SB备份；有界回归保持候选决策、码流和重建一致，Windows45/GCC44与sanitizer专项3通过。合成核心计时约减少18.8%，真实隔离速度仍未测量。见[第二轮报告](docs/task-reports/M6-r2-performance.md)。原冻结长测继续，M6未最终验收，正式仍0.0.5-m5。

> **2026-10-17 M6 RUNNING**：有界RD已接线，默认off；Windows45/GCC44严格CTest及12条固定AVM短流通过。全QP、跨平台/检测器、1000帧长链及真实视频效果集中交后台批次，尚未全部验收。见[M6集成交接](docs/task-reports/M6-integration.md)；正式版本仍0.0.5-m5，当前开发构建的RD属于实验功能。

> **2026-10-17：M5 DONE，正式本地版本0.0.5-m5。** M5.3四平台240项长测与独立审计通过；发布仅改版本元数据，140源反替换审计、Windows40/Linux各39项严格CTest、四平台各360项流/重建指纹回归通过。见[发布收尾报告](docs/task-reports/M5-release.md)，可用程序与说明在[dist/M5](dist/M5/README.md)。旧M4-ffmpeg-min包未更新。下一阶段[M6指南](../M5-M6-AI-TASKS/README.md)已准备，本轮未启动M6。以下RUNNING和旧版本描述按历史时点阅读。

> 2026-10-16最新续行：**M5.3 RUNNING**。非无损CQP已扩至1..255，三格式AI/P和两工具四组合支持；10bit实际qindex=QP+48，QP0不支持。多点PSNR/VMAF评测、断点批测与Windows/GCC/Clang/SAN自动长测脚本已接入，见[批量测试教程](docs/M53-批量测试教程.md)及[当前合同](docs/m53-validation-contracts.md)。正式仍0.0.4-m4，长测及最终验收前不发布M5。后续仅CQP+CRF，取消CBR/2-pass/VBR/VBV码率模式；内部并行仍属M8。下文旧QP100限制按历史阶段阅读。

> 2026-10-16续行：**M5.2 DONE**。新增`--partition-select fixed|adaptive`（默认fixed），按内容选择完整64×64超块保留旧树或四个32×32叶，AI/P与intra-select四组合均终验通过。144矩阵576流、24大尺寸、跨平台/1000帧/sanitizer/泄漏及90流真实partition trace已冻结，严格CTest为MSVC38、Linux三配置各37。报告与使用见[M5.2集成终验](docs/task-reports/M5.2-integration.md)。M5.3及正式M5发布仍待后续，以下M5.1描述按历史阶段阅读，正式版本仍0.0.4-m4。

> 2026-10-16：**M5.1 DONE**。`--intra-select adaptive`已支持AI和LOW_DELAY_P的Y DC/V/H候选，UV保持DC。144项模式矩阵、24项大尺寸、M4关闭156项指纹、三格式AI/P各1000帧及sanitizer长链通过；MSVC/GCC/Clang码流逐样本一致。正式版本仍0.0.4-m4，M5整体尚缺内容partition及组合终验。见[M5.1终验与用法](docs/task-reports/M5.1-integration.md)。

> 本轮1+2智能体执行后的最终严格CTest：MSVC 37/37，GCC/Clang/GCC sanitizer各36/36；独立decoder trace核验817组Y/UV记录及39次inter邻居，Valgrind API测试0错误/0泄漏。此前[开发前审计](docs/task-reports/M5-audit-20261016.md)与[第一轮AI报告](docs/task-reports/M5-mode-selection.md)保留历史；下一步按[开发指南](../M4-M5-AI-TASKS/README.md)及[契约](docs/m4-m5-contracts.md)做M5.2。

> 2026-10-11：M4 `0.0.4-m4` 已完成。支持8-bit420、10-bit420/444的默认ALL-INTRA及显式LOW_DELAY_P；108项P矩阵、48项AI回归、三配置各1000帧与跨平台/检测器通过。见 [M4验收报告](docs/task-reports/M4-integration.md)。

> 后续按用户要求完成 [M4 最小 FFmpeg 包](dist/M4-ffmpeg-min/README.md)：Windows x64 `libsvtav2` 编码与 `libavm-av2` 解码、Y4M/RAW/IVF、中文教程和一键验证；六组主测试逐字节匹配内部重建与固定参考解码，见 [编解码证明](dist/M4-ffmpeg-min/PROOF.md)。

Independent AV2 production encoder implementation.

> **正式基线M4，工作树为M5实验开发。** 公共API与CLI共用真实编码和内部重建路径，输出Annex B AV2 elementary stream。LOW_DELAY_P首帧key、后续单向P，只参考上一张内部重建；亮度整数运动搜索和420必要色度插值已实现。输入8-bit420或10-bit420/444；尺寸须为8的倍数且不超过1920×1080，用户QP100、threads1。10-bit以u16le样本存储，byte stride支持非对齐和奇数步长。默认仍为ALL-INTRA。M5新增config尾字段要求调用方重编，不能因为版本字符串相同就与旧M4库混用ABI。

## M3执行与接口

[M2→M3任务包](../M2-M3-AI-TASKS/README.md)由本Codex主会话和两个子智能体执行完毕。10-bit420→10-bit444已通过；8-bit444与8/10-bit422本期不支持。布局/ABI约定见 [M3契约](docs/m2-m3-contracts.md)，复验命令见 [最终报告](docs/task-reports/M3-integration.md)。

## M4执行与接口

[M3→M4任务包](../M3-M4-AI-TASKS/README.md)由Codex主窗口加两个子智能体完成，T027–T036已终验DONE。[M4接口契约](docs/m3-m4-contracts.md)记录参考所有权、MV单位、语法和容量重试。公共配置新增prediction_structure，调用方须使用本版头文件重新编译。配置不扩8-bit444/422、B帧、多参考、亮度subpel搜索、完整RD/SIMD、多线程或FFmpeg wrapper。

## Layout

```text
include/svt_av2.h          public C API
Source/Lib/Common/         build info, geometry helpers, Y4M reader
Source/Lib/Codec/          encoder context and public API state machine
Source/Lib/Bitstream/      bit/OBU writers, sequence/frame headers, keyframe entry
Source/Lib/EntropyCoding/  AV2 symbol (range) encoder and block writer
Source/Lib/Encoder/        reference storage, integer ME and frame cores
Source/Lib/Prediction/     scalar intra and inter prediction
app/SvtAv2EncApp/          command line front end (Y4M in, AV2 stream out)
tests/unit/                unit tests (registered by T002 via tests/CMakeLists.txt)
cmake/                     compiler flags and AVM oracle discovery
```

## Requirements

- CMake >= 3.16
- Ninja
- A C11 compiler: MSVC x64 (Windows) or GCC / Clang (Linux)

No third-party runtime is required. The AVM reference tools (`avmdec` /
`avmenc`) are treated as an external, read-only oracle; they are only located
and recorded, never built here.

完整独立验收工具使用Python和NumPy，属于测试依赖；生产C编码器不依赖它们。固定oracle保持只读，M4诊断另用独立副本，见最终报告。

## Build and test (Windows, MSVC x64)

Load the workspace environment first (it injects MSVC, cmake and ninja):

```powershell
. C:\Private\ffmpeg-native\deps-full\AV2\activate.ps1
```

Then configure, build and test. Use a per-task build directory so that
parallel sessions never share a cache:

```powershell
cmake -S svt-av2 -B svt-av2/build/T001/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build svt-av2/build/T001/Release
ctest --test-dir svt-av2/build/T001/Release --output-on-failure
```

Debug build:

```powershell
cmake -S svt-av2 -B svt-av2/build/T001/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build svt-av2/build/T001/Debug
ctest --test-dir svt-av2/build/T001/Debug --output-on-failure
```

### Useful options

| Option | Default | Meaning |
|---|---|---|
| `SVT_AV2_BUILD_APPS` | `ON` | Build `SvtAv2EncApp` |
| `SVT_AV2_BUILD_TESTS` | `ON` | Build unit tests and register CTest cases |
| `SVT_AV2_BUILD_SHARED` | `ON` | Also build the shared library variant |
| `SVT_AV2_ENABLE_SANITIZERS` | `OFF` | ASan/UBSan (GCC/Clang only) |
| `SVT_AV2_WARNINGS_AS_ERRORS` | `OFF` | Treat compiler warnings as errors |
| `SVT_AV2_ORACLE_DIR` | auto | Directory holding `avmdec` / `avmenc` |

### Build and test (Linux, GCC / Clang)

```bash
cmake -S svt-av2 -B svt-av2/build/T001/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build svt-av2/build/T001/Release
ctest --test-dir svt-av2/build/T001/Release --output-on-failure
```

## 运行编码器（M4）

```powershell
svt-av2\build\m4-root\Windows\bin\Release\SvtAv2EncApp.exe --input input.y4m --output output.av2 --frames 17 --pred-structure low-delay-p --export-recon internal.yuv
```

省略 `--output` 时只读取 Y4M 元信息。`--frames 0` 不编码；省略 `--frames` 时处理全部输入帧。每帧内部重建按紧密排列的 Y/U/V 顺序写入 `--export-recon` 文件。

编码支持progressive、8-bit420及10-bit420/444、QP100、单线程。`--pred-structure all-intra`或省略该选项使用默认AI；`low-delay-p`显式启用P链。其他QP、位深或色度格式会明确拒绝，不做隐式转换。

完整验收示例（从 AV2 目录运行，输出目录须未使用过）：

```powershell
python svt-av2/tools/verify_m4.py --app svt-av2/build/m4-root/Windows/bin/Release/SvtAv2EncApp.exe --decoder AVM-AV2-TEST/tools/bin/avmdec.exe --out-dir svt-av2/artifacts/my-m4-check
```

连续运行验收增加`--sizes 64x64 --frames 1000 --patterns mixed`，静止码率/质量对照使用`--rate`。Linux sanitizer版本运行时启用`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`与`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`。原M3默认AI验收继续使用verify_m3.py，独立运动/参考刷新验收使用verify_m4_motion.py。

## License

See `LICENSE`.
