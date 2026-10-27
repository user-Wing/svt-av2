# M5.3 B：多QP系数语法交付

日期2026-10-16。状态 READY_FOR_INTEGRATION。B生产/测试/独立工具已停写；正式版本与完整M5.3验收由00处理。本轮仅CQP，不实现CRF、CBR、VBR、2pass、完整RD或额外工具。

## 1. 合同与规范核对

公开user QP1..255；8bit actual qindex=QP，10bit actual=QP+48（49..303）。固定AVM tag `966a7d7cd6fcf60360caf5dc413b2aeeb65e144d` 的 av2/av2_cx_iface.c:1291–1299 使用(bit_depth−8)×MAXQ_OFFSET（24）的偏移。用户QP映射是本项目CQP合同；AV2语法本身编码actual qindex。

|规则|固定规范位置|固定tag源码|本期处理|
|---|---|---|---|
|qindex字段位数|§5.18 quantization_params，p118|av2/decoder/decodeframe.c quantization_params|8bit f(8)，10bit f(9)，303完全可表示；不改变原字段位置|
|coeff CDF初始化|init_coeff_cdfs语义，p333（§6.18帧头初始化）|av2/common/entropy.c:26–75 get_q_ctx/av2_default_coef_probs|直接actual qindex≤90→0、≤140→1、≤190→2、其余3；不按bitdepth减48|
|每帧默认CDF|帧头初始化，p108|decodeframe.c PRIMARY_REF_NONE/disable_cross_frame_cdf_init|本期仍关闭CDF更新、PRIMARY_REF_NONE，按当前actual qindex初始化，不沿前帧CDF|
|nonlossless|帧头LosslessArray/CodedLossless，p108–109|decodeframe.c:9215附近 qindex==0条件|segmentation和delta_q关闭，所有支持actual qindex>0，coded_lossless恒0；QP1不是lossless|
|high range escape|§5.20.7.28 read_quant，p231；系数语义p371|av2/decoder/decodetxb.c:907 level>>20检查|Rice/Exp-Golomb已有能力，允许level≤1048575；超过20bit明确拒绝|
|inverse quant|§7.14.2/.4，p515–519|decodetxb.c:916–926|A负责abs(level)*dequant&0xffffff，再round/txscale/sign/clip；B只序列化真实qcoeff|

CDF边界：8bit用户90/91、140/141、190/191；10bit用户42/43、92/93、142/143。10bit QP100仍actual148/ctx2，8bitQP100仍actual100/ctx1。不能把10bit低user QP的CDF错误按减48后的值选。

## 2. 写端实现

- 固定tag entropy_inits_coeffs.h SHA `ff9a7790220e438d4e481c9278541ebfe29d0d22451d9d3bb3b9cf6847d8de12` 抽取22张系数表缺少的q0/q3；保留原q1及_q2数组符号、数值与顺序。
- artifacts/m53-b/extend_cdfs.py逐值核验22×2张既有q1/q2表与固定tag完全相同后才生成，原始摘要cdf-extraction.json。新增每表_by_qctx[4]指针索引，coeff的skip/EOB/base/BR/DCsign全统一按显式ctx选择。
- `svt_av2_coeff_q_context(actual_qindex)`实现固定阈值，tilewriter不再按位深硬选1/2；入口限定支持的8bit1..255、10bit49..303。
- raw frame_header本已有8/9bit合法字段容量，无需新增帧头位；保留非零actual qindex语法检查。公共QP/Codec/keyframe由00持有，A负责量化范围与producer 20bit cap，B未改这些模块。
- 原不带qctx的coeff wrapper及M1 minimal block回归仍保留历史ctx1/固定向量；当前生产带coeff的路径显式使用四类ctx，不能把历史M1入口误认为CQP实际路径。
- 写端max level原已支持2^20−1及adaptive Rice/Exp-Golomb，本轮无需改逃逸算法；补覆盖此前producer8191上限以上的测试与真实decoder证据。QP100表值严格保留，整体流/recon旧指纹由00最终回归判定。

## 3. 测试与独立真实syntax证据

单测在既有test_coeff_syntax/test_tile_writer添加：actual qindex各阈值/两侧与303、四类EOB16固定首boundary（1946/1839/3946/5029）、4ctx×3plane×intra/inter×8192/26000/1048575正负高幅与尾EOB、tile合法端点/边界和越界拒绝。旧qctx0拒绝测试改为非法ctx4，避免继续锁死新增类别。

本轮B严格构建实际日志：MSVC Release /WX clean-first完整CTest39/39 PASS；GCC Release Werror全新配置完整CTest38/38 PASS。msvc-final-clean.log/msvc-final-ctest.log、gcc-configure.log/gcc-build.log/gcc-ctest.log均在artifacts/m53-b。00随后如注册新自测增加测试数，不冒写为B旧执行数量。首轮因旧API/transform_path对QP非100拒绝期望过期出现两失败，已由00修订，最终完整测试全绿。

`tools/m53_qp_trace.py`只读私有固定tag诊断decoder的实际M53FRAME/M53CDF/M53LEVEL，校验QP→qindex、8/10bit、coded_lossless0、四类CDF默认初始化/EOB16 inverse boundary、每帧初始化记录、高幅level20bit范围。四种损坏输入负例self-test通过。既有mode/partition独立检查同时复用；不以编码器日志自报作为语法真值。

### 跨CDF边界矩阵

直接取00已由不可变Windows oracle通过的boundary360保留流，B另用新私有诊断decoder重放全部360例：15个user QP（1,42,43,90,91,92,93,100,140,141,142,143,190,191,255）×三格式×AI/P×四工具组合，1080帧全部PASS。每例实际header qindex/CDF组、mode/partition与诊断raw SHA均核，所有ctx0/1/2/3有实际覆盖；诊断raw等于编码器预先生成的internal SHA。`boundary-trace-matrix.json`及boundary-traces逐例原始log/json/raw保留。该cut素材未出现>8191 level，未将此矩阵伪称高range证据。

### 独立高幅探针

另生成三格式64×64黑→白→黑3帧，QP1，AI/P共6例。用B新二进制真实编码，先输出internal，再分别不可变Windows oracle和新诊断decoder解码；6例全部三份raw SHA相等。实际decoder读到level13107（正/负sign均出现），超过旧producer8191cap且无corrupt；不是仅把单测成功当作20bit生产证据。原始输入、stream、三份raw和日志在large-level，摘要verification.json。实际生产样例最大13107；26000和1048575仅属于写端单测，不冒称这些更大级别已从真实frame core产生。

## 4. 证据隔离、复现与冻结

本轮诊断只在新 `artifacts/m53-b/diagnostic-decoder/source` 复制并加trace，编译到同目录build/avmdec。补丁m53-trace.patch记录相对只读M52源的entropy.c/decodeframe.c/decodetxb.c变化；未修改不可变oracle/spec或旧M4/M51/M52诊断。完整source-manifest.json及provenance.json保留。

私有诊断binary SHA `fa15802a25067b8656ee1eba12f4d5494bcd06e51c2e77a4461519023367d3f3`；完整诊断source清单SHA `77217697375196d8682734cbc4c7c6d1f5c7e92f083f24f89fec117272260f11`。复现可在Windows项目根运行Python311的artifacts/m53-b/trace_boundary.py、large_level_probe.py；明确调用新M53私有decoder。源码/build配置和原始build日志也保留。工具单独用法：`python tools/m53_qp_trace.py <trace.log> --qp 43 --depth 10 --frames 3`，高幅探针加--require-high-range。

生产/tests/tool十文件冻结清单：artifacts/m53-b/source-manifest.json，SHA256 `B9CA3E23F37C4195B4C95AA8C011DE7843BF41AF1AC405A162283B4D4E0AEFC1`。boundary摘要SHA `02b172542a8db2ebd5e41dbafd119e3b4a75460486c54c8f571bc1bf9b1dd669`；高幅摘要SHA `f3c3890c01620687583a76ae8c44b08b324e971e8fa2e0ad4af11a82bb3bbf53`。final-evidence.json集中保存全部hash、旧诊断hash复核及新保存路径。

固定oracle仍SHA `110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac`，规范仍SHA `e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f`。B不改正式版本，不给出全QP质量单调性/完整RD或未来CRF承诺；00负责最终多平台、安全/长链、QP100旧指纹、质量性能测量和M5.3整合结论。
