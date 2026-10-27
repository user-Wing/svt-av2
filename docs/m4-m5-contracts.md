# M4→M5生产数据、语法和高级参数契约

> 2026-10-16用户再次修订：M5.3正在开放非无损CQP1..255（10bit实际qindex=QP+48），并接入VMAF/独立长时间批测，精确当前合同见[m53-validation-contracts.md](m53-validation-contracts.md)。此前QP100限制按历史阶段阅读。后续只开发CQP和CRF，取消CBR、2-pass及VBR/VBV独立码率目标模式。

> **2026-10-16最新续行**：M5.1/M5.2 DONE；内容分区及模式四组合已完成矩阵、实际树trace、跨平台、长链、检测器与单点测量，见[当前集成报告](task-reports/M5.2-integration.md)。新增partition_selection/--partition-select，精确子集见[m52合同](m52-partition-contracts.md)。正式仍0.0.4-m4，M5.3更广内容/组合评估及正式冻结待做。下文M5.1和开发前范围描述按其历史时点阅读。

> **2026-10-16当前状态**：M5.1的DC/V/H已支持ALL-INTRA及LOW_DELAY_P intra fallback，规范/独立mode trace/API专项、144矩阵、24大尺寸、三格式AI/P长链、严格跨平台/检测器及完整M4关闭156指纹均终验通过，见[M5.1集成报告](task-reports/M5.1-integration.md)。`--intra-select dc|adaptive`与公共config尾字段intra_mode_selection需使用匹配头文件重编。partition/组合未完成，正式仍0.0.4-m4。最初“无开关/仅AI”是历史。

日期2026-10-16；状态：M5执行合同，M5.1 DONE，M5整体未验收。当前以task-reports/M5.1-integration.md为准，M5-audit-20261016.md是本轮开发前审计。0.0.5-m5范围为DC/V/H的AI/P接线、一个合法内容可变partition子集、二者独立/组合终验；原12工具族为后续待办，RA需独立参考结构合同。参见../M4-M5-AI-TASKS/README.md。

## 1. 不变项

保留M4三格式8bit420、10bit420、10bit444；QP100（8bit qindex100、10bit qindex148）、threads1；single tile、SB64、8倍数尺寸至1920×1080。默认ALL-INTRA、显式LOW_DELAY_P，保留已冻结M4禁用路径的流和recon hash。不能扩8bit444、422、12bit、B帧、任意QP、RD/preset、SIMD、多线程。已存在的FFmpeg-M4最小包默认不更动。

## 2. 数值与生命周期

- 8bit每像素uint8；10bit u16le，数据指针不要求2字节对齐，stride单位是byte且可为奇数。prediction uint16，残差int16，coeff/qcoeff/dqcoeff int32，必要乘加int64。Clip到相应bitdepth。
- 源图、当前recon、committed reference必须互不别名。帧内邻居取**已重建**像素，不取source的未编码像素；420/444色度块和Y mode条件需按规范映射。
- 单一已选择mode从frame core一直传到tile writer；writer不得重算候选或将V/H预测结果发成DC syntax。qcoeff快照有效期覆盖同步序列化。
- packet完整成功→prepare/commit reference一次→公开packet/recon；get_packet重试不再次刷新，不回填decoder结果。

## 3. 单feature放行表

| 必填项 | 原则 | 第一个V/H feature现状 |
| --- | --- | --- |
| 规范节页 | 固定AV2 v1.0.0，标明条件与bit数 | 已填docs/m5-mode-syntax-research.md精确页/节及条件 |
| 固定AVM decoder路径/行 | 禁使用随HEAD变化的junction代替 | Y/UV、P intra、inter邻居DC、palette条件及预测均有固定tag路径 |
| scalar算法 | 非自证、8/10bit、邻居/clip/stride | T038已实现并有2000组合测试 |
| Y/UV mode符号和CDF | 模式索引不能靠猜 | AI/P真实Y列表及方向Y对应UV_DC已接；独立12例817 Y/UV及39次inter邻居通过 |
| encoder内部recon | 与最终syntax相同模式 | AI/P同一已选mode贯通；P首key和后续intra均支持 |
| fixed oracle和逐样本 | 先冻结内部recon再独立解码，零失配 | 本轮144模式+24大尺寸、三格式AI/P×1000及sanitizer通过 |
| off/on | off与M4指纹相同，on真实编码新模式 | 新API专项通过；48受控非flat真实改变；M4关闭156项SHA一致 |
| 质量性能 | 同素材同QP、时间/码率/PSNR | 已有模式mixed单点3次重复实测；完整多场景与分区组合归M5.3，SAD不等于压缩收益 |

对新partition同样逐项填写子树/coeff顺序、chroma mapping、implied与explicit分裂的规范表。

## 4. 高级参数合同

M5只给真正实现的工具启用/禁用开关。不要在未通过G2/G3前新增public no-op选项；无依赖时启用要求明确错误而不是悄悄DC fallback。需要增加公共配置时在尾部追加，保留所有已存在enum numeric值，说明旧调用者需重新编译；set_parameter/get_config/init和CLI必须同源。预置preset值不代表preset控制策略已实现；本轮不做CRF/VBR/CBR。

## 5. 验收条件

G0源冻结→G1标量→G2 bitstream合规→G3 AI/P接线与真实on/off→G4本版内容可变partition→G5性能质量→G6全平台/检测器/长链与冻结。两工具独立/同开/全关均须验证。只读规范SHA256=e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f；固定oracle SHA256=110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac。报告源/流/recon SHA和测试实际exit，不填预设PASS。

**只有M5整体终验通过，才能修改当前正式版本0.0.4-m4。** 当前开放项不可作为已完成功能。
