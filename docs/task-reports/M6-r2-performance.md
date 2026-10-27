# M6 第二轮：保持决策一致的性能优化

2026-10-17，00 主调度及 A/B 两个子智能体执行。本轮有界优化已由 00 独立验收通过；**M6 最终验收仍待独立长测及结果审计，正式版本保持 0.0.5-m5**。原指南 M6.2（AI/P 完整块 RD）此前已实现，本报告不是该步骤的首次开发或 M6 发布。

## 改动与证据

生产代码仅修改 `Source/Lib/Encoder/rd_frame.c`：

- exhaustive 先检查整条路径的模式合法性，避免在后叶拒绝前进行无效的前缀变换；合法候选、顺序和 ID 不变。
- 重建只获取预测，不再计算无人使用的 SAD；pruned/partial 的 SAD 排序保持。
- SB 可见区备份与恢复改为按行复制原始字节，沿用已有缓冲容量，不覆盖 padding。

未改 lambda、候选全集、剪枝策略、整数 ME、公开接口、默认 off 或内部线程。新增 `tools/m6_r2_analyze.py` 和 `tools/m6_r2_regress.py`，用于只读分析及有界差分验证。A 独占 RD 源码，B 独占新分析/验证工具，00 负责公共文件和独立验收；[A 报告](M6-r2-A-performance.md)、[B 报告](M6-r2-B-validation.md)保留详细复现证据。

A 对原合成 `test_m6_rd` 的画像显示 build_path 累计 2.740 秒，其中 reconstruct_leaf 为 2.702 秒；region_copy 为 0.127 秒。嵌套时间不可相加，这不是 1080p 视频总体热点结论。B 的既有1512项小图分析显示，在 partition/intra 工具组合 `10` 的 AI 配置中，两个 RD 模式每帧同为20个合法候选，但 exhaustive/pruned 平均 forward 为2216.71/536.71；结合源码，证实存在非法路径前缀的重复计算。

00 另做 rate session 全帧分区计划扫描的局部检查实验。50万次试探的前后计时未能排除环境波动，该候选未合入；`tile_writer.c` 和对应单测均恢复并核对原 SHA。未为本次工作扩大策略范围。

## 独立验收

00 启动时逐项核对170源与上一轮短验清单，全部匹配；修改前源码保存在外置 `root/baseline-source`。验收时170项中仅 rd_frame.c 变化，新增两个验证工具另计入172项源清单。

| 验证 | 实际结果 |
| --- | --- |
| 00 新目录 Windows Release 严格构建/CTest | 45/45 PASS |
| 00 新目录 GCC Release 严格构建/CTest | 44/44 PASS |
| GCC Debug ASan/UBSan/LSan，RD/rate/API | 3/3 PASS |
| 00 相同 observe 探针链接优化前后核心 | 2319 条候选/提交事件逐字节一致 |
| B 三格式 AI/P 有界差分，三帧/配置 | 54/54 完整码流及重建一致 |
| B 固定 AVM 重建核验 | 54/54 逐字节一致 |
| B off 对冻结 M5 | 18/18 码流及重建一致 |
| 00 自己新构建 Windows/GCC 各三例重放 | 6/6 流、重建、决策统计一致 |

候选事件比较包含 SB、候选 ID、D、R、J、提交标志，完整日志 SHA256 为 `29ca98164c5eae015f3ea00bac21bfb31354f1e1d8659a3effab774f40053d2b`。原有核心8813项检查仍覆盖换序、最低J、失败回滚与容量重试。B矩阵采用格式/尺寸/QP/工具分配覆盖，并非全部笛卡尔积；00额外重放覆盖三格式 P、full/pruned、partial 及 GCC 跨平台一致性。

唯一允许变化的统计是实际 `forward` 调用数。B 的108条 RD 帧记录中78条不变、30条下降，总计35622→23445，无增加；candidate/replay/winner/D/R/J/lambda/full/partial 全部不变。未把完整流一致冒称为额外导出了逐元素 coefficient 数组，也未把字节复制的 stride/非对齐代码审查冒称新增 API 实验。

00 最终 Windows encoder SHA256 为 `8f39b602ab05de7ba7aa4826f91765727ff33aee1caa63b3eddbce61216f253e`；GCC encoder 为 `d871ac4e3c972d4515f5fa394d8f5d312736bca4e79ba05fefe35395449249b8`。构建目录、日志、程序和源码指纹均独立外置，不覆盖共享运行依赖。

## 性能与效果边界

A 在仅 rd_frame.c 不同的副本上交替运行三轮合成核心测试，中位耗时3.677394→2.986642秒，减少约18.8%。当时后台 real-quality 仍运行，**仅作为合成测试方向性观察，不能承诺真实视频或隔离单路提升18.8%**。本轮不另开长测矩阵；优化后1080p隔离速度尚未测量。

旧1512项小图的504组 full/pruned 中438组流和重建相同，66组不同；pruned-full VMAF差范围−1.152581..+0.301030，完整流字节差−337..+273。它们只描述原缩小图片段，不代表本轮优化的质量变化，不作为新策略取舍或lambda调参依据。各帧前序赢家不同后，整帧J差也不能当作同入状态的局部剪枝损失。

## 独立长测与后续交接

北京时间2026-10-17 22:32左右只读复核：`campaign-local-r2-20261017` 仍为 RUNNING/real-quality，9/17阶段完成，完整质量24/108项PASS；其余项未宣布完成。76项冻结文件（含原六文件）及配置SHA全部匹配。总控未重启、停止或修改；冻结批次仍使用优化前程序，不会自动采纳本轮构建。此处是读取时点，后续请重新读实际状态。

进入 isolated-speed 时暂停一切新增编译与计算验证；仅继续代码/文档阅读。长测完成后独立审计真实效果、冻结指纹与逐项结果，再讨论M6整体放行，不能以进程消失或脚本PASS代替验收。当前三类本地素材无独立holdout，结构与时间口径不同的AV1比较不作为标准总体优势结论。

全部新增较大产物在 `C:/Private/ffmpeg-native-m6-tests/optimization-r2-20261017`。00入口 `root/final-verification.json`、`root/source-manifest.json`、`root/campaign-frozen-integrity.json`；A/B入口分别见其报告。本轮不改正式发布目录、不操作FFmpegFreeUI或OpenCode，也不向已交接停止的旧会话发送消息。
