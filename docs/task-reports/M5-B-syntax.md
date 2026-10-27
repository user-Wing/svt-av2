# M5.1 B 窗：P intra 模式语法交付

日期：2026-10-16。状态：READY_FOR_INTEGRATION。B 已停止生产、测试及工具写入。仅交付 M5.1 模式子集；M5.2 partition、M5.3组合终验不在本轮实现，正式版本保持0.0.4-m4。

## 实现

属主生产仅 `Source/Lib/Bitstream/tile_writer.c`。解除 m5_intra_modes 与 P modes 组合的拒绝，P intra 读取既有 coded_blocks[Y].intra_mode 并使用同一mode编码；不重新决定预测、不更改qcoeff、不调用外部编码器。每个 inter leaf 覆盖对应above/left mode状态为DC，符合 fixed blockd.c av2_get_joint_mode(inter)=DC。P 的 palette false仅在Y=DC时写，V/H跳过，符合fixeddecodemv.c1052与规范§5.20.8.1。

A 的result.intra_mode/config.m5_select_intra由A负责，Codec/API/keyframe由00接线，B未改其文件；未改公共include/CMake/版本。关闭m5路径不读取新增mode元数据，旧M4模式/字节条件不变。完整off指纹由00验证，不能把B局部测试代替完整48AI/108P。

## 规范与独立证据

完整表见 `docs/m5-mode-syntax-research.md`：固定PDF章节/页、固定AVM commit源路径/行、Y/UV CDF/列表、邻居inter重置、DPCM/FSC/MRL/CFL/MHCCP/DIP/palette工具条件均已列明。规范与固定Windows decoder SHA本轮实测同合同，未修改不可变oracle或spec。

`tools/m5_mode_trace.py` 读取独立固定tag诊断decoder实际值，不消费encoder自报mode。根据完成block逐4×4地图独立重建邻居和ctx，按规范±1..4构造模式列表，核set/index/joint/Y mode/delta及UV ctx/index/DC。selftest9对邻居、8×8/大块固定锚点及两种损坏输入检测通过。

诊断12例（3格式×AI/P×directional/mixed，176×144×3f）全部通过：817个真实Y模式、817个UV，39次intra取inter邻居，三类ctx均覆盖。10bit420 P directional中首key后第1索引帧23V/1H/2DC，第2帧24H/2DC，实际包含2 inter，证明P fallback并非只在首key生效。逐例诊断raw与原先内部raw SHA相等；不可变Windows oracle的对应原流验收由00独立完成。

摘要 `artifacts/m5-b/mode-trace-matrix.json`；每例原始 `*176x144_3f*.trace.log` 与 `.debug.yuv`。诊断copy位于artifacts/m4-a/debug-oracle-source，A已明确本轮不使用该copy；仅加M5MODE/M5UV/M5BLOCK trace，补丁 `artifacts/m5-b/m5-decoder-trace.patch` 记录相对旧诊断copy变化，当前source/binary SHA在矩阵JSON。该诊断程序不代替不可变验收oracle。

诊断隔离补记：本轮原实际执行路径保留记载，完整1520文件源码、M4/M5补丁、原构建cache证据及运行程序已独立保存到 `artifacts/m5-b/diagnostic-decoder`。程序副本与原程序SHA完全相同：`273402b0db584ce922e5020879eb97dd245003fb3b32525f5600126760e9d6ab`；decodemv源SHA `bffaa5038331f9565515eb1389c0801d6539a45553adf0287389e9ac45967312`；完整source-manifest SHA `cfe0a1cfe5cd6a147c8cc0ed2fea0c1826ff640b97b46b74cc93346412cbe6b8`。复制后使用新保存路径重新运行12例，raw SHA及所有mode/context统计均匹配，见 `diagnostic-decoder/replay/verification.json`；复现步骤见该目录README。矩阵JSON追加原执行/新保存位置和相同hash，不改原12例结论。存在M5前decodemv文本快照，但无可验证的原M4诊断binary备份，故未猜测重建或局部恢复旧目录；正式固定oracle/spec及artifacts/m4-root保持不变。

## 测试与冻结

既有 `test_tile_writer` 新增P V/inter/H混合链、inter毒化intra metadata不改变输出、非法intra mode明确拒绝、disabled路径忽略metadata。未新增CMake target。

- MSVC Release /WX 初轮完整CTest36/36通过（ctest-r2.log）；最终clean-first构建与完整CTest36/36再次通过，日志 msvc-final-clean.log / msvc-final-ctest.log。
- GCC Release Werror 新配置全构建、完整CTest35/35通过（gcc-configure.log、gcc-build.log、gcc-ctest.log）。Windows额外runner_oracle_smoke目标不在Linux数中。
- 独立mode工具12例PASS、self-test PASS；不是声称完整144模式矩阵已经由B执行。

冻结清单 `artifacts/m5-b/source-manifest.json`，SHA256 `775A0AC3BBC9197DA232F954D48C5D00EC1D3187ADA6DAFE7D2F4C7C01AC9521`，含tile_writer.c、test_tile_writer.c及m5_mode_trace.py；报告文件不计入生产清单。tile_writer.c SHA `9393AEB116C8959101D078415AD25F090C7F64507D9967FA8C979A2976C3F750`。

限制：Y候选仅DC/V/H，UV仅DC，选择仍是有界SAD，不是完整RD或压缩收益承诺；本轮无partition或未来FFmpeg/preset参数扩展。最终144矩阵/off回归/API/平台与长链、阶段状态由00汇总。
