> **2026-10-24 顺序覆盖**：现行执行 [M6-I 单图合同](m6-intra-contract.md) 和用户指定 `04-M6-intra-only-first.md`。I-GATE 前暂停下方旧亚像素/帧间顺序，先单图体积−15%目标，再将耗时压到约2×。本文件其余内容保留为历史设计。

# M6-C：单线程压缩竞争力执行合同

2026-10-18。用户指定以 `../../M5-M6-AI-TASKS/03-M6-single-thread-compression-roadmap.md` 调整方向。本合同覆盖旧计划将 preset 排在内部多线程前、以及将基础 RD 通过当作压缩能力完成的安排；旧合同和实验保留。正式版本仍为 0.0.5-m5。

## 目标与顺序

先 M6-C0 诊断冻结 → C1 自适应 CDF/RD 事务 → C2 亚像素 → 后续按真实收益推进预测、分区、变换/量化、参考和时域工具 → M6-Q 单线程竞争力验收 → 内部多线程 → preset → CRF。保持 CQP 1..255、threads=1。未达到 Q2 不转入线程或产品参数开发。

Q1 消除常见数量级差距；Q2 采用路线图建议阈值：同源同结构 SVT-AV1 对照，多数预先冻结真实视频有效 VMAF BD-rate ≤ +25%，重要题材不得持续 > +100%，同时报告 PSNR-Y/U/V、holdout、负收益与单线程成本。Q3 再争取广泛 holdout 接近持平或负 BD-rate。当前均未达到；三片短样本不能代表全套验收。

## 本轮 C0/C1 与文件属主

基线冻结于 `C:/Private/ffmpeg-native-m6-tests/optimization-C1-20261018/root/baseline-source`，271 文件清单 `baseline-manifest.json` SHA256 为 `a2a91d04d5c5b495d9b5a06171e7c6a5c29fcd564f00555665b44f4a1a2c856c`。不得覆盖旧 R5/R6/R7 或 campaign 快照。

- 00：公共 API/Codec/CLI、全部 CMake、现有 API 测试、公共文档、最终签收。
- A（c1_core）：entropy_encoder、entropy_models、tile_writer 的实现和内部头、rd_frame、独立新测试 `test_adaptive_rd.c`。跨模块由同一写者完成，避免共享事务文件并发修改。
- B（c1_validation）：只读生产源码、fixed AVM 独立概率/更新 oracle、独立验证与后续真实曲线脚本。新产物仅写本阶段 b 目录。

第一交付优先实现 adjusted probability 的 Q12 rate，旧 static raw 成本逐值保留。只有 committed/scratch bank、候选回滚、赢家重放、partial 和正式 writer 逐 SB 一致性均经过独立验证，00 才放开 `entropy-adapt on + 基础 RD`。若额度不足以验证完整事务，公共组合继续拒绝，停在已验证内核，不标记 C1 完成。

完整 C1 首期只允许 coeff-opt/motion-opt off，不改候选、lambda、训练参数或源。Q12=4096/bit，lambda Q8 与原 D/J 量纲不变。R 是共享语法驱动的估计，不等于最终 flush 字节。default off 和 R7 RD-off 各自保留指纹。fixed AVM v1.0.0 及规范为唯一只读 oracle。

## 验证与资源

先概率全 symbol/count 边界及非法模型差分，再双 bank A→B→A、失败回滚、重放、partial、双实例和容量重试；三格式 AI/P 真实流独立解码逐样本；MSVC/GCC/Clang strict 和 ASan/UBSan/LSan。未执行的项保留 PENDING，不以 Windows 结果代替 Linux。

效果沿用原生 2D/3D/Real 帧 48..71、24 帧、QP80/100/120/140，不换段、不缩放、不调 lambda。baseline 为同工具 static 基础 RD，candidate 为 adaptive 基础 RD；另报告同源 RD-off，不能乘算 R5/R6/R7 收益。完整质量与 holdout 放在短验之后。

原 campaign 启动核验为 RUNNING/real-quality，真实 PID17392、batch20600、计划任务 Running；R6 joint-speed 为 PASS。每次计算前重新核验，isolated-speed 禁新增编译/编码/解码/VMAF，不杀、不重启旧任务。

用户澄清本轮额度即将耗尽，需续费后在 2–4 天后接续开发。本轮不启动新增原生大矩阵；完成可验证最小交付、保存准确 PENDING 项，并安排三天后的本聊天接续。原长测按现有托管继续。
