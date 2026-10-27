# M5.2 最小内容自适应分区合同

日期：2026-10-16；状态M5.2 DONE，独立验收与源码冻结见task-reports/M5.2-integration.md、artifacts/m52-root/final-verification.json。用户授权沿用00+A+B的1+2模式，本轮只完成M5.2，M5.3及正式M5发布仍待后续。

## 子集与规范

仅完整可见的64×64超块按内容选择0=旧树NONE，1=四个32×32方形叶。1用合法的64×64 HORZ、两个64×32 VERT、四个32×32 NONE实现，叶次序TL/TR/BL/BR；不加入四分syntax新符号。所有其他节点沿M5.1固定规则，partial SB的决策必须为0。420叶各有16×16 U/V，444为32×32；每叶Y/U/V系数顺序保持，单tile/SB64/8倍数尺寸至1080p不扩。

固定规范v1.0.0 §5.20.3.1 PDF p148–152、§5.20.3.2 p155–158；固定AVM commit966a7d7cd6fcf60360caf5dc413b2aeeb65e144d的decodeframe.c:1728–1847、blockd.h:966及av2_common_int.h:4037–4141确认64无do_square_split，ext-part关闭下读取binary do_split与rect_type。精确CDF/context、边界与逐叶trace由B在m52-partition-syntax.md补齐。固定验收oracle与规范只读。

## 内容成本

每完整SB分四象限，各1024个亮度样本，和为S0..S3，总和S。计算uint64 score=4×Σ(Si²)−S²，等于“父块均值SSE减去四子块均值SSE”乘4096。阈值=4096²×16×2^(2×(bit_depth−8))，严格大于才选四分，平局保留NONE。0/max与10bit极值均须有独立精确整数参考测试，不用float或带溢出的32bit乘法。

只读source亮度，每帧在编码前选一次；不读取decoder或未重建邻居来作预测。这里是有界内容代理和固定分裂惩罚，不是完整RD，也不承诺码率改善。

## 共享事实与生命周期

- Common提供select_quads、coded_type_plan、traverse_coded_plan。NULL/0严格走旧树；选择器先验证容量和几何，partial byte恒0；非法byte、计划长度及partial split须明确拒绝。
- 每SB一个uint8决策，索引sb_row×sb_cols+sb_col。Codec持有可复用数组，每帧填充一次，AI/P core和writer读取同一份，不重算内容决策。core config和private frame header config尾部使用partition_decisions/partition_decision_count。
- Core采用计划遍历并产生实际叶geometry、唯一mode及qcoeff；Codec复制每叶数据，writer用同一geometry helper序列化。计划/系数在同步packet容量重试期间有效；重试不得重新决定或重复提交reference。
- 公共config尾追加partition_selection=0 fixed/1 adaptive，parameter枚举尾追加，CLI --partition-select fixed|adaptive，默认fixed。新开关与intra-select dc/adaptive四组合均须验证。非法值不改配置，调用方使用匹配头文件重编。

## 文件属主与验收

00负责Codec/公开头/CLI/keyframe/CMake/独立验证；A负责Common geometry与AI/P core及其单测；B负责tile writer、private frame header、规范和独立decoder trace。独立build/artifacts目录m52-root、m52-a、m52-b，A/B直接交流抄送00；不改M4/M51冻结编码产物、不改旧诊断副本/不可变oracle、不操作3fui。

需证明同尺寸同边界的flat与quadrant内容产生不同非边界树，并从独立decoder实际partition/叶geometry核对四分次序与可见coverage。三格式AI/P、四开关组合、矩形/partial边界、大尺寸、非平坦色度、容量/PTS/EOS、M4全关156项、M51 fixed-partition回归、严格MSVC/GCC/Clang、sanitizer长链/泄漏、跨平台确定性和单点质量/时间/内存均留证据。M5.3仍承担完整组合场景与正式版本冻结。
