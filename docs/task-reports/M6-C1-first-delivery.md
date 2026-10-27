# M6-C1 首轮交付与续接记录

2026-10-18。用户已指定压缩路线图并澄清：本轮额度将耗尽，续费后于 2–4 天再接续开发。本轮按 [M6-C 合同](../m6-compression-contract.md) 收敛到 C1-A1，不启动新的原生大矩阵。正式版本仍 0.0.5-m5。

## 已冻结方向和边界

单线程压缩竞争力 → 内部多线程 → preset → CRF。Q1/Q2/Q3 未达到。当前未实现的基础能力不能用新开关、单测或局部 J 掩盖；达到成熟编码器水平仍须同源、同结构、多 QP、完整长片和独立 holdout 的 BD-rate 证据。

00 负责合同、CMake 和最终验收；A 只改 entropy_encoder.c/h、entropy_models.c 和新增 test_adaptive_rd.c；B 只写外置独立 fixed AVM oracle。公共 API、Codec、CLI、lambda、候选及训练参数本轮不变。完整 entropy-adapt+RD 仍由原配置校验拒绝，tile session 也未放开。

基线快照 271 文件和清单保存在 `C:/Private/ffmpeg-native-m6-tests/optimization-C1-20261018/root/baseline-source`、`baseline-manifest.json`。开发树没有 Git，本轮用源码快照、manifest 和差异补丁恢复。

## C1-A1 的实现范围

为实际可达 N=2..8 模型加入 fixed AVM 的位置调整概率，再沿用现有整数 Q12 log 计价；允许相邻 raw CDF 边界相等，拒绝非法模型。只在绑定独立模型 bank 的 rate-only 路径使用新成本，成功后更新同一模型；静态 raw 成本与正式 writer 逻辑保留。

这只建立概率/成本内核，不是 RD 事务交付，也不是压缩率改进结论。C1-A2 committed/scratch、逐 SB writer/rate 对齐、重建和邻居回滚、赢家重放、partial、API 重试、真实曲线均待续接。

## 验收状态

00 已独立复验：MSVC `/W4 /WX` 全量 CTest **50/50**，GCC strict 全量 **49/49**，ASan/UBSan/LSan 相关机制/API **3/3**，全部 PASS。新增内核单测 3,706,052 checks、0 fail。当前 WSL 未发现 Clang 可执行文件，本轮不新增依赖，Clang 专项仍 PENDING，不能称全平台门槛完成。

B 的 fixed AVM 原始 `get_adjusted_prob`/`update_cdf` 差分，经 00 再次运行：8,232,960 个概率值、1,966,080 次更新、105,656 次 raw 相等边界、3,840 次完整 bank 比较，errors=0。原函数、默认模型、生产代码、测试程序均绑定 SHA；oracle 只服务测试，不进入编码器依赖。

00 新鲜运行三格式 × AI/P × static RD-off/static pruned/adaptive RD-off 共 **18 个四帧用例**；新 Windows/GCC 与 R7 冻结基线完整流、重建均相同，固定 AVM 逐样本匹配。包含 72×80 partial；这是既有公开路径兼容性，不能当作未实现的 adaptive RD partial 事务通过。

本轮只签收 **C1-A1 概率内核机制通过**。Clang、C1-A2、完整自适应 RD 码流/异常事务、原生质量、速度、完整长链与 holdout 一律 PENDING。无新增 BD-rate 成绩，不宣称压缩率已达到成熟编码器水平。

初始 C0 审计 `root/c0-audit.json`：原 campaign 76 个冻结文件 MATCH、配置 SHA256 `b16e3f8137b9765ec34358cdba9acc5207d57ea87bef76daddf276aeee7953a2` 不变；原质量 105/108 PASS、0 FAIL，真实总控 PID17392/计划任务仍运行，阶段 real-quality。本数字是检查时点的快照；续接须重读 JSON、计划任务与真实进程，不重启正常批次。

## 三天后的续接入口

已创建当前聊天的一次接续任务 `svt-av2`，北京时间 **2026-10-21 16:00**。任务先读本报告、合同与外置证据，从未验收项继续。机器和应用须届时可用；若额度仍不足，需要用户续费后继续。

1. 核验当前源码/程序 SHA 和旧长测状态；如果 isolated-speed 活跃，只读审计，停止新增计算。
2. 先补齐 A1 留项，再按 `next-A-adaptive-rd-design.md` 完成 committed/scratch bank 和候选事务，公共组合仍保持拒绝直到正确性门槛通过。
3. 限 coeff/motion off 的最小组合接线，做三格式 AI/P、partial、重试、fixed AVM 逐样本和多平台严格验证。
4. 沿用原生三片 24 帧四 QP 测真实曲线，含 RD-off 和负收益；然后按压缩缺口决定 C2 等后续工作，不直接转 preset/多线程/CRF。
