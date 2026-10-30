# M6 帧间研发

帧间已有独立分支，但基础压缩能力不足，尚未接入当前帧内基线。原型包括低延迟 P、整数运动搜索、分区、熵和 RD 等；不能把实验开关当作默认能力。

2026-10-18 三源 1080p、24 帧、四 QP 验收中，R5 相对旧 RD 的 VMAF BD 约改善 6.9%/2.0%/2.7%，墙时增加 17–30%；R6 收益接近零，墙时再增约 2–8%；R7 在 RD-off 同重建下省约 7–20% 体积，墙时增加 13–17%。2D/Real 基础 RD 仍落后非 RD。见 基础能力评估（见下方历史原文）。

2026-10-28 帧间首轮报告称 0.95× AOM p0 目标未达、未给出纯帧间归一化因子；竞争负载下的 walltime 不能证明速度达标。RDOQ、DRL1 和 forced residual skip 未进入推荐配置。原始报告和数值已归并至下方；两个独立源码分支保留在 `artifacts/inter-research/`。需要先落实基础工具，并优先借鉴 SVT-AV1 帧间技术，再评估与帧内基线的接入。


## 详细历史记录与实验依据

为便于公开，历史文本中的本机路径前缀已替换为占位符；SHA-256 仍指向脱敏前的原始文件，历史命令需按实际环境调整。

以下是清理前相关报告、技术合同、任务记录和汇总数据的归并文本。每条保留来源和 SHA-256；内容按当时的时间和实验边界阅读，其中“当前”“进行中”、旧目标、旧版本、临时路径及未来计划均为历史记录，不覆盖本文开头的最新状态。旧目录及关联的大型测试产物已按本次清理要求删除，原文中的链接和路径可能失效；个别实验只有有限样本或尚未完成验收，不能推广为项目整体结论。


<details>
<summary>001 · basic-capability-contract.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\basic-capability-contract.md`  
原文件 SHA-256：`bf58e212dbb18c91b7acb02bf820a1e96ae4353ab83c5c135a0b79128e41dcfe`

# 基础编码能力补齐合同

2026-10-18，用户授权按顺序先补基础能力，再引入AVM新技术，使用其三类1080p原片；耗时过长时截连续1至2秒，压缩效果与性能独立测量，由00+A/B执行。不把当前大测试完成作为开发前置条件，正式仍0.0.5-m5。本文覆盖历史文档将策略开发等待完整长测的限制；原冻结长测/依赖/媒体/配置仍只读保留，isolated-speed时暂停新增计算。

## 顺序与归属

基础阶段依次补运动候选与RD选择、搜索/预测精度、实用帧内模式和递归分区、概率自适应与系数优化、环路处理和参考帧/时域结构。各项先锁单因素合同再实施、独立测量，不承诺每项减半，不把基础阶段首项完成称为整个基础阶段完成。之后再评估AV2新增的变换、量化及预测等高级工具；具体工具是否引入由正确性与收益证据决定。

R5/R6运动两项冻结后，利用其原生质量长窗口，下一项优先做可与决策完全分离的基础概率自适应首关，合同`basic-entropy-contract.md`；帧内/分区和其它基础项仍保留待办。这是基础项内部的执行安排，仍不引入高级工具或声称基础已补齐。

当前R5只做第一项，避免同时改多个能力无法归因。A唯一写ME/rd_frame内核和新机制测试；00唯一写公共API/Codec/CLI/CMake、API测试与集成报告；B唯一写新原生片段评测/速度工具和独立报告。继续使用现有两个子智能体，不使用OpenCode/FFmpegFreeUI或向旧会话发消息。

## R5冻结算法：有界整数MV RD

范围仍8bit420、10bit420/444、AI/单参考P、原QP/原lambda/原模式与分区语法；不扩±8搜索范围、不新增亚像素或参考帧。新helper在相同289个整数MV里按(SAD,|x|+|y|,y,x)稳定排序，精确保留前三名，另补零MV并去重，最多4个。slot0必须与旧单赢家逐字段一致，旧入口及flag0路径保持不变。top3的早停阈值使用当前第三名SAD，不能用旧第一名阈值得到不精确排名。

首先完整执行旧RD取得同入口赢家。仅完整64SB、P帧做单轮raster叶坐标精化：固定赢家树，每叶依次试候选MV并将该叶设inter（包括原intra叶）；其它叶使用当前已接受的模式/MV。每trial从同SB入口重建全部叶，后继预测/transform/quant随prefix重算，按真实像素D、共享语法R和原整数J比较，等J选较低稳定ID。最多4叶×4候选=16额外完整path，允许跳过完全相同的当前解，不声称完整扩展域穷举。AI和partial不增加试探。

候选ID低9bit保留树/模式，bit9..20留给原coeff policy，bit21..28每叶2bit表示MV slot。最终保存真实模式和MV选择并完整重放hash/D/R后仅commit一次。试探不能写输出码流、泄漏rate状态或修改源/参考；失败/容量不足必须能干净重试。局部不劣只约束同一SB入口，不外推后续参考链或整片质量。

公共config尾追加`motion_optimization`，参数`SVT_AV2_PARAM_MOTION_OPTIMIZATION`，CLI `--motion-opt off|rd`，默认off。值1要求rd_mode为1/2且与coeff_optimization互斥；初始化后不可改。公共和内部统计尾追加motion_trials/motion_accepts，CLI仅开启时另打印motion统计行；旧rd行不改。调用方全部重编，不混用旧ABI。允许AI配置但应无额外候选，以便统一调用。新产物在optimization-r5-20261018，R4的176源/程序保留，正式包不改。

## 事前样本与验收

B在编码前冻结三源相同规则：用户原片解码得到的local Y4M，1920×1080、10bit420、24000/1001，0基索引48..71共24帧、1.001秒。不得按画面内容或编码结果换起点，源片和完整Y4M只读。片段保存原H264与完整Y4M/逐帧payload SHA、fps/帧索引，独立重读验证。若后续需2秒另立预算，不因结果换段。

主要效果集合：三源、QP80/100/120/140、low-delay-P、adaptive intra/partition、pruned、coeff-opt off、motion-opt off/rd，共24次编码。基线为R4最终WindowsV2程序，新程序default off须与其流/recon/原rd统计一致。每流固定AVM逐可见样本匹配内部重建，独立全帧PSNR与VMAF、完整字节；有效单调曲线才报PCHIP BD，另报线性插值敏感性与直接实测支配点。少字节伴画质损失不能当等质改进，不按三片调lambda/删坏点。合成机制验证另用于可控位移、格式/边界及错误路径。

速度使用同片段QP100/P与上述工具，单编码进程，不混入解码/VMAF；暖机后至少三次基线/候选配对，交替顺序，记录墙时/进程CPU/帧率和后台负载。速度窗口内00/A/B停止其它新增编译/评测，原长测继续，因此该轮为独立单路配对测量而非整机空闲隔离；不能据此给无背景负载的最终速度结论。若后台负载不稳定则保留原始数据并标限制，不挑最快一次。

机制门槛包括完整289候选独立全SAD排序、slot0旧值/补零/去重/稳定ID、真实accept、逐SB最小J/ID、重放/反序/只读源参考/失败重试、partial/AI跳过、原默认off兼容、MSVC/GCC严格CTest和检测器专项。每次计算前读取原campaign真实stage，j2构建，不覆盖冻结长测。native每条可能数分钟，首次编码前明确：质量阶段最多2个编码子进程并行（实测有约15GiB可用内存），速度仍严格jobs1；资源紧张时降并发，不删样本。

## R6事前冻结：保留R5的有界±16整数扩展

R5已完成严格机制与解码短验，180源/程序冻结在optimization-r5-20261018/root/candidate-source与source-manifest.json，native质量/独立速度仍待完成。R6独立目录、程序和数据，不覆盖R5，也不预设其效果。此修订在查看R5首对候选效果之前确定，选择范围来自整数语法边界和搜索预算，不依三片调参。

先完整运行R5算法并保留其树/模式/MV/成本/hash作为incumbent。随后对完整P超块的每叶，搜索[-16,16]的步4网格且跳过[-8,8]旧域，共56外圈位置；围绕固定的最佳粗点试±2整数邻域，跳过旧域/越界/已测粗点，最多24新位置，总新增SAD位置≤80/叶。对实际已测的新外域按原稳定排序保留最多两个MV，不声称完整±16最优。不得在细化过程中移动中心或反复迭代，避免隐式扩大预算。

固定R5赢家树，以R5各叶MV为slot0 incumbent，wide候选为slot1/2，单raster每叶试两个wide MV并设inter，最多8次额外完整path。其余叶保持当前已接受模式/MV，恢复同入口并完整重建/真实D/R/J/重放/失败恢复合同不变。R5赢家仍参与最终比较；AI和partial继续原路径。ID bit29标记wide阶段，bit21..28用每叶2bit标记0=R5 incumbent、1/2=外域候选；低9bit为树/模式，coeff位仍空。R5原赢家ID/hash不改，wide阶段hash必须反映其真实MV与阶段policy，不能用新的slot解释旧ID。

现有AVM onepel shell set0的7类覆盖class0..6、shell0..127。MV±16时相同范围邻居的每轴delta≤32、shell≤64，仍在已有编码支路内；只把实现guard扩大为MV±16/delta±32，新增极限/边界固定AVM逐像素证明。±32极端shell128会进入当前未实现set1分支，本轮禁止盲目扩到该范围。亚像素涉及精度信令、不同CDF、MV单位和插值链，本轮不混入。

motion_optimization扩为0/1/2，CLI `--motion-opt off|rd|wide`，2=完整R5+wide阶段，仍要求RD且coeff互斥；默认off。内部/公开统计尾加wide_trials/wide_accepts，CLI另记wide行，原rd/motion行格式不改。A写ME、rd_frame、motion_syntax及相应新/必要范围测试；00写API/Codec/CLI/CMake/API测试；B固定同一native24片段与4QP，在R5评测之后执行R6候选编码，复用经SHA核验的R5候选作为质量基线以省重复编码。独立速度仍新鲜配对R5与R6，不复用旧耗时。没有运行结果前不声称压缩或速度改善。

## 首次正式速度测量前的共同窗口修订

原R5/R6分别ABBAAB会重复测中间R5程序。由于1080p每条编码数分钟，首次正式速度数据尚未产生前，00+B冻结改用一个三版本共同窗口，以减少25%重复运行：每源三轮顺序ABC、BCA、CAB，A=R4 motion默认off（旧CLI不加新flag）、B=R5显式motion rd、C=R6显式motion wide。三版本各三次鲜测，三源共27次；每版每源预先两帧暖机且不进入统计。源仍同native24、QP100/P/原工具，单路只编码码流，不导出recon/解码/VMAF。

每轮三次在同一顺序窗口测量，分别计算B/A、C/B、C/A的墙时与进程CPU比，保留三轮全部值、中位数、min/max和完整背景负载采样。共同使用同轮B是事前定义的实验设计，不是复用旧时段耗时；三轮平衡位置，不按结果调整顺序。期间00/A/B停止所有其他新增计算，原冻结长测仍继续，因此仍不能称整机隔离。

原native素材manifest与质量配置不修改，旧ABBAAB字段作为历史协议保留，新joint协议单独绑定其SHA、三程序与具体参数/顺序，代码为`m6_r6_joint_speed.py`。该修订只为控制重复成本，不使用已产生的质量结果挑选速度样本或程序。原两项单独速度脚本保留供复现原协议，此轮以共同窗口结果为准。正式窗口必须等R5/R6质量与00必要计算全部结束并明确通知后开始。

</details>


<details>
<summary>002 · basic-entropy-contract.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\basic-entropy-contract.md`  
原文件 SHA-256：`c0a324b59ac16a8f181fe8cd418eb24bad1b1015df5217d0be2a5661d41038ff`

# R7基础概率自适应：首关合同

2026-10-18，用户授权先补基础能力再AVM高级技术，00+A/B。R5/R6运动能力源码/程序已冻结，原1080p质量评测仍运行。此项按基础概率编码能力推进，不改在测程序，也不把首关当完整基础阶段完成。正式仍0.0.5-m5。

## 首关范围与归因

新增frame/tile内CDF自适应，继续PRIMARY_REF_NONE，每帧从固定AVM当前QP的规范默认模型初始化，不引入帧间CDF继承、新预测/变换或量化工具。严格限定`rd_mode=off`，原模式/MV/qcoeff/recon决策不得改变；只改变正式writer的概率更新和相应header信令。成功时源、模式、MV、系数和内部重建应与同配置基线一致，固定AVM解码须逐样本一致。相同重建下的字节变化可直接归因于熵编码，不靠插值等质推断。

不能只翻disable_cdf_update位。所有实际reachable `read_symbol`模型必须使用独立语义ID、完整初始边界、count和三个规范训练rate offset。不能按CDF数值合并模型，也不能拿临时栈指针当模型身份；palette、inter_ext_tx等初值相同的模型仍须按其真实语义上下文分开。旁路literal/sign/Rice部分不得错误更新模型。

模型数据从固定只读AVM的entropy_inits_coeffs/modes/mv及prob.h结构化导出，按实际row的CDFn/PARAn处理，不按数组最大CDF_SIZE猜测。必须有独立C宏展开extractor逐word核对边界与offset，保存固定源码/输出指纹；生产仅使用自有C11实现和冻结常量，不链接或调用AVM编码器。模型集合提供显式symbol-site/family/axes闭合清单，未知模型在开启时明确拒绝，不静默回落固定概率。

成功symbol先用旧模型编码再按规范count/time-interval/rate更新；失败symbol不更新，容量重试从全新bank开始。每次正式tile/frame写入fresh bank，不能跨帧或跨实例泄漏模型。raw CDF经过整数更新可能出现相邻边界相等，正式range coder须按规范位置padding处理，禁止epsilon夹CDF来掩盖模型错误。首关不接rate-only，tile_rate_create/frame在entropy开启时明确拒绝；后续另行解决自适应率估计与RD事务回滚，不能冒称首关支持RD组合。

## 接线与归属

00唯一写公共API/Codec/CLI、所有CMake注册及API测试。config尾追加`entropy_adaptation`，参数`SVT_AV2_PARAM_ENTROPY_ADAPTATION`，CLI `--entropy-adapt off|on`，默认off；1要求rd_mode=0，初始化后不可变。frame_header config尾追加同名字段，默认off，Codec在实际packet header配置处传入。所有调用者须重新编译。

A唯一写新增entropy_models.c/h、entropy_model_data.c/h、生成器/模型机制测试，以及必要的entropy_encoder、coeff_syntax、motion_syntax、tile_writer和frame_header接线。新增module文件名变动先通知00。旧off完整流/recon/既有统计保持，旧最小writer若无法完整覆盖开启路径必须拒绝该不支持组合，不生成部分适应的非法流。A不能改公共API/CMake/Codec/CLI。

B负责原生效果与独立计时。新产物全部外置optimization-r7-20261018；R6的184源与完整candidate-source作为只读基线，不覆盖R5/R6任何已冻结程序、评测工具/配置或正在执行的质量任务。新计算仍先查旧campaign stage，isolated-speed禁止新增计算；三版本motion速度窗口若开始，本项暂停所有新增编译/编码/解码。

## 验收

先验证模型数据独立提取、语义闭合及更新算法：N=2..8、规范rate triplet、count15/16/31/32切换、长偏置序列与raw边界相等、非法模型、失败/重试、双实例和跨帧fresh。和固定AVM update_cdf的独立差分只用于测试，不成为生产依赖。然后全支持格式、AI/P、边界尺寸、多个QP、充足symbols超过32次，固定AVM逐样本重建及内部trace确认编码决策不变；关闭兼容、跨平台严格构建、检测器专项。

真实样本仍使用事前冻结的三类1920×1080/10bit420原片帧48..71/24帧/1.001秒，不换段不缩放；QP80/100/120/140、P、adaptive intra/partition、RD/coeff/motion全off。基线R6 entropy默认off，新程序entropy on，12配对/24编码。逐流比全重建指纹与固定AVM原样本；只有完全一致才报告同质量字节节省，任何不一致直接失败。可计算一次基线质量供说明，无需对相同像素重复VMAF或将其当新视觉改善。

速度与效果分开，针对相同RD-off配置使用独立单路新鲜三配对、暖机、进程CPU/墙时/后台负载记录。它不与R5/R6的RD-on耗时直接相除；原生质量完成前不宣称压缩或速度收益。不得因为首次效果弱或失败就按三片改训练offset或lambda。

</details>


<details>
<summary>003 · m6-compression-contract.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\m6-compression-contract.md`  
原文件 SHA-256：`2e7b8468fa7e5ffcb818e54b34255c454429bb98edd755753d5507054e05df51`

> **2026-10-24 顺序覆盖**：现行执行 [M6-I 单图合同](m6-intra-contract.md) 和用户指定 `04-M6-intra-only-first.md`。I-GATE 前暂停下方旧亚像素/帧间顺序，先单图体积−15%目标，再将耗时压到约2×。本文件其余内容保留为历史设计。

# M6-C：单线程压缩竞争力执行合同

2026-10-18。用户指定以 `../../M5-M6-AI-TASKS/03-M6-single-thread-compression-roadmap.md` 调整方向。本合同覆盖旧计划将 preset 排在内部多线程前、以及将基础 RD 通过当作压缩能力完成的安排；旧合同和实验保留。正式版本仍为 0.0.5-m5。

## 目标与顺序

先 M6-C0 诊断冻结 → C1 自适应 CDF/RD 事务 → C2 亚像素 → 后续按真实收益推进预测、分区、变换/量化、参考和时域工具 → M6-Q 单线程竞争力验收 → 内部多线程 → preset → CRF。保持 CQP 1..255、threads=1。未达到 Q2 不转入线程或产品参数开发。

Q1 消除常见数量级差距；Q2 采用路线图建议阈值：同源同结构 SVT-AV1 对照，多数预先冻结真实视频有效 VMAF BD-rate ≤ +25%，重要题材不得持续 > +100%，同时报告 PSNR-Y/U/V、holdout、负收益与单线程成本。Q3 再争取广泛 holdout 接近持平或负 BD-rate。当前均未达到；三片短样本不能代表全套验收。

## 本轮 C0/C1 与文件属主

基线冻结于 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-C1-20261018/root/baseline-source`，271 文件清单 `baseline-manifest.json` SHA256 为 `a2a91d04d5c5b495d9b5a06171e7c6a5c29fcd564f00555665b44f4a1a2c856c`。不得覆盖旧 R5/R6/R7 或 campaign 快照。

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

</details>


<details>
<summary>004 · m6-r4-coeff-contract.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\m6-r4-coeff-contract.md`  
原文件 SHA-256：`c1994fe940f6eb5e0a0524381f7040a5a59fbc3bb4804967d4c33ebf27fd4b72`

# M6 后续实验合同：有界系数 RD 精化

2026-10-18，用户要求在冻结长测并行期间继续实际压缩效率优化。本合同只约束新隔离候选；原 `m6-rd-contracts.md`、长测快照/程序/配置及其结果不变。正式仍0.0.5-m5，不预设每轮减半或总体收益。

## 接口与属主

- 00：公共API/Codec/CLI、CMake、API测试、合同和独立集成验收。
- A：rd_frame.c/h与新test_m6_coeff.c；B：新m6_r4评测工具、预冻结样本、独立AVM/效果验证。唯一文件写者不重叠。
- 公共config尾追加 `coeff_optimization`，参数 `SVT_AV2_PARAM_COEFF_OPTIMIZATION`，CLI `--coeff-opt off|rd`，默认off；值1要求rd_mode为exhaustive或pruned，rd-mode off下明确拒绝。不允许运行中切换。public/internal RD stats尾追加coeff_trials/coeff_accepts，CLI仅开启时新增独立coeff统计行。全部调用者重新编译，不混用旧DLL。
- build/artifacts外置于 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r4-20261018/{root,a,b}`；原173源及完整源码副本在root/baseline-source冻结。

## 冻结算法

格式仍8bit420、10bit420/444；AI、单参考P、CQP1..255。原lambda、模式候选、ME、合法树、三平面D与共享语法Q12 rate不变。不增加新运动候选、RA、多参考、亮度subpel、内部并行或新码率控制。

先完整运行旧RD，选出原SB的树与模式赢家；仅对完整64SB在固定赢家树/模式上做单轮raster叶坐标精化。每叶原始量化策略0，再试策略1（各平面保留DC、AC全零）及策略2（全部系数为零），最多4叶、8次额外完整路径试探。partial保持原算法，不扩大边缘域。不是完整RDOQ、全系数穷举或全局最优。

每次从同一SB入重建恢复，按既定模式和当前各叶policy重新预测/transform/quant，应用相应qcoeff/dqcoeff置零后inverse/clip；后继叶的预测与量化必须随前叶重建重新计算。不得只改最终系数后沿用旧重建/SSE，也不得缓存跨prefix的intra预测。所有试探走同一真实SB语法rate事务，无输出提交。

以完整SB的D/R/J比较，cost更小优先、等J取更小稳定候选ID。ID低9bit保留原树/模式ID，bit9起每叶2bit编码policy；policy0仍是原候选ID。每步当前解必须保留，因此从相同入状态观察到的精化J不得大于旧赢家。该保证不外推为整帧/后续参考链J或VMAF更优。

最终使用所选模式和policy重放，核对coeff/recon hash、D/R后只提交一次。policy0/off保留旧完整流/recon/既有统计；开启后candidate_hook同时记录新增候选，candidate计数如实增加，coeff_trials/coeff_accepts独立记录。路径失败/回滚/容量重试不得泄漏状态。完整零系数不新增wire工具，正式writer从最终qcoeff按原规范生成码流。

`rd-mode exhaustive`在开启精化时仍只表示原模式/树域穷举，附加系数域是上述坐标法；不声称完整扩展域穷举，也不声称开启后pruned在整段的J一定高于exhaustive。

## 验收与预算

先单测同入状态J不增、额外候选ID/policy/重放、换序确定性、源/参考只读、失败回滚及partial跳过。再默认off兼容、三格式AI/P和边界的固定AVM逐样本重建、API/CLI拒绝、MSVC/GCC严格构建与sanitizer专项。

B在首次测量前冻结6个小图源（3个独立确定性合成样例、3片现有small的10bit420前3帧）、QP80/100/140/220、AI/P和off/rd精化，共96次小编码，单编码进程；另三片native各首帧QP100作6次基线/候选补充。至少4有效QP、严格单调且有共同质量区间才报BD，其余明确N/A；不以同QP减字节直接冒充等质收益，不按结果改源/QP/lambda。

若实测没有压缩价值或代价失衡，保留否决证据，不因已实现而默认启用。较大产物外置并有限保留，不开新长测矩阵。每次计算前查原总控；isolated-speed期间暂停全部新增编译/计算。默认off、正式包和冻结长测不改，M6最终放行另行独立审计。

## V1否决与V2冻结修订（首次V2编码之前）

V1粗清零在三条真实small/P的四点VMAF曲线全部变差，不能推广。V1源码独立冻结在root/v1-source，程序/矩阵/评分在b/v1-evidence；以上算法段保留为V1证据，不覆盖或改写失败数据。

V2替换附加系数域：固定原树/模式，按raster叶序、Y/U/V平面各试一次，将该平面normative DCT_DCT scan末尾最后一个非零AC的幅值减1、保留符号；EOB<=1不改DC。每个叶/平面最多操作一个系数，不重复降低同位置，不批量清零。qcoeff改后调用原quant_params_init_tx_bd/dequantize_block完整重算dqcoeff，含TX尺度与clip。每叶3bit表示三平面是否应用操作，候选ID仍从bit9起，最多12bit policy；完整SB至多12次额外整路径试探。原policy0、默认off和partial不变。其余完整恢复/后继重预测/真实D/R/J/重放/失败回滚合同沿用，lambda不改。测试须独立核验末尾系数的精确一级变化和其余系数保留。

V2独立构建与结果目录，不覆盖V1程序。B复用相同冻结9源/4QP/AI-P集合，只再运行这一轮；同样计算默认off兼容和AVM逐样本重建、真实small/P四点VMAF。此策略可能只有微小收益，局部J下降仍不等于等质压缩率改善；若多数曲线继续退步，不以实验默认off为借口宣称优化有效。

</details>


<details>
<summary>005 · m6-rd-contracts.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\m6-rd-contracts.md`  
原文件 SHA-256：`4cefd1808b4b498de60974de311df8a068b4389d28d7484972cead5de9628349`

# M6 有界 RD 执行合同

> 2026-10-17用户本地素材续行：用户现提供三组1080p H264原片及对应AV1 p6 CRF36成片，授权替换失败的在线素材准备并启动本地集中批测。编码器算法/既有r1证据不变。具体配置与帧序列、AV1比较及时间口径见docs/task-reports/M6-local-campaign.md（相对项目根）。此次使用三类本地片段，无独立holdout，不声称原六类+holdout已覆盖；不以该差异阻塞用户指定的测试。

2026-10-17：用户已授权启动，00主窗口+A/B两个子智能体；需要长时间计算时集中成一个独立可恢复批次，启动后结束AI会话，待用户下轮集中验收。当前M6 RUNNING，不提前发布0.0.6。

## 基线与属主

M5正式140源、5程序已在本轮全部核对SHA，PASS记录及源快照在`artifacts/m6-root/baseline/`；正式M5源清单SHA为f303fe31348c1b72bbc4d37417127e43e9fb35bc4b2a4884860a0ca2493654ca。旧程序、长测、oracle/spec只读，历史媒体按清单定向恢复。

- 00：include/svt_av2.h，Codec/encoder.c/codec.h，CLI，全部CMake及公共文档，API测试，集成验收及批次总控。
- A（m6_core）：Encoder新增rd模块及其头/单测，必要的intra/inter局部支持；优先独立rd_frame路径，off仍用旧core。
- B（m6_rate）：EntropyCoding rate通道及单测、Bitstream/tile_writer.c/h的同语法SB试探session，新的m6评测工具和真实素材清单。
- A/B直接交流接口并抄00；keyframe/geometry暂不修改，需要时00指定唯一属主。独立build/artifacts目录m6-a/m6-b/m6-root。

## 冻结范围

8bit420、10bit420/444，AI及上一张已提交重建单参考P，现有整数ME获胜一个MV，Y DC/V/H与UV DC，现有TX及量化路径。完整SB候选为原64树与四个32叶；partial SB沿用合法边界树。QP1..255，10bit实际+48；QP0不支持。不增加RA/B/多参考/subpel亮度/额外工具族/preset/内部并行/CRF，后续只做CQP与CRF。

CLI `--rd-mode off|exhaustive|pruned`，默认off。公共配置追加uint32_t rd_mode，0/1/2，运行后不得切换，所有调用方按新头重编。旧intra/partition开关决定候选是否开放，RD不得暗中打开关闭的工具；off默认及四工具组合完整流/recon保持。

## 候选与状态

每SB从相同入重建和同语法上下文试探完整树。各叶先预测，再实际transform/quant/inverse/clip，D只能取可见重建误差。候选包含mode/MV、packed coeff、recon、partition、D/R/J，赢家仅提交一次。TX64有效packed1024系数不当作4096。

完整SB exhaustive为合同候选组合内不剪枝，AI最多81、P最多256四叶路径，另比较原树。ME只是旧SAD预筛获胜MV，不是289MV完整RD或全帧全序列最优。遍历顺序固定，等J按候选ID最小者；pruned每个叶按当前路径邻居的预测SAD保留前2个候选、固定ID解同分，四叶至多16条路径；原树仍比较全部合法候选。两模式使用相同完整评分，pruned是同全集的确定性子集，不能以exhaustive名义实现贪心。

partial SB可能有64叶，本轮明确只保留M5模式/MV预筛产生的一条完整合法路径；重建和语法rate仍真实计算，但不声称partial全模式RD或局部最优。边缘全模式RD后续另立合同，不增加本轮指数搜索。此条为执行时的有界收敛，覆盖任务书中未明确的partial模式搜索范围。

source与上帧reference只读。候选试探必须隔离本SB三平面recon和所有可见状态；writer session隔离mode/partition above-left、CoeffNeighbours、motion cells/游标、coder状态及输出缓冲。允许独立rate-only session无输出缓冲，从真实writer同一symbol路径累计成本；formal writer最后仍从提交的coeff/mode/tree生成码流。候选失败/回滚不得泄漏状态。整帧成功写入后才更新reference，packet容量重试不重复决策提交。

## 成本量纲与验收纪律

采用固定点语法R（Q12 bit，即4096/bit）及lambda Q8，生产决策不使用浮点log或libm。令s=2*(bit_depth-8)，w在420为4、444为1：`D=round((4*SSE_Y+w*(SSE_U+SSE_V))/2^s)`。420/444的色度总采样权重一致，不计padding。

量化参数ac来自本编码器实际QTX表。QTX本身含QUANT_TABLE_BITS=3且transform系数额外Q3，因此归一8bit的像素域步长为`q=ac/(64*2^(bit_depth-8))`。固定lambda系数4，`lambda_Q8=round(ac*ac/2^(2+s))`，`J=D+round(lambda_Q8*R_Q12/2^20)`。不再重复施加TX scale或位深比例。最坏值与乘积溢出边界由A的独立单测证明，非法rate或算术范围明确失败，不用饱和隐藏问题；必要时拆分乘法保留精确舍入。当前公式在正式测试前冻结，不按测试片调参。

R来自真实mode/partition/MV/skip/EOB/level/sign/literal共享语法，不以非零数或局部offs差代替。估计bit不宣称最终字节精确相等，最终效果统计完整流字节。

先必要短单测、独立解码和重建对比，再冻结一批大规模运行。三格式AI/P、QP端点/CDF边界、非整SB、三种RD、工具开关、回滚与API容量重试为正确性覆盖；全255QP、多平台/检测器、真实视频多点效果和长链留在集中批次。任何未实际跑完的项目保持PENDING。

真实视频至少六类加独立holdout，素材来源/转换/帧数/工具/模型hash冻结；标定集与测试集不重合。至少4有效QP和共同质量区间才能BD-rate，否则明确N/A，失败另外FAIL。速度用jobs1、三次中位数与波动；并发正确性吞吐不作单路速度。大批启动前先1秒试跑估算时间与磁盘，保存快照、状态、日志、逐case原子结果、锁与resume校验；保留流/指标/指纹，验证后清理raw，控制工作区低于30GB，新增大素材放工作区外专用目录。

最终M6 DONE需独立机制/规范/效果证据齐全，不能只凭局部J更小或某片高VMAF放行。当前默认off保持实验边界，未实测收益不承诺。

</details>


<details>
<summary>006 · basic-capability-progress.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\basic-capability-progress.md`  
原文件 SHA-256：`e87a89469deec2296bd9e0e8f846233bfd437a4606251d01d027e774c63f61dc`

# 基础能力补齐：集成进展

2026-10-18，00+A/B，本轮R5/R6/R7代码、质量与独立性能验收完成，M6总体仍RUNNING，正式0.0.5-m5不变。用户授权先补基础能力，再引入AVM新技术，使用其1080p原片；长测结束不作为开发前置。执行合同为`docs/basic-capability-contract.md`，最终结论见`M6-basic-capability-assessment.md`。本文不把本轮三项候选称为整个基础阶段完成。

## 阶段R5：整数运动候选RD

代码已完成，默认关闭的`--motion-opt rd`将原±8整数搜索的精确SAD前三名和零MV去重后交给真实重建D/共享语法R比较。先保留原RD赢家，随后固定树逐叶尝试inter，允许原intra叶改inter，最多16额外完整路径；每次重建全部叶并正确回滚、重放。系数优化互斥，原lambda不改。

00独立严格Windows47/47、GCC46/46、ASan/UBSan/LSan四项通过；最终新增机制测试8658/0，原RD8813/0、coeff231084/0。6组默认off与R4基线的流/recon/原RD trace一致；三格式AI/P的6组motion实际编码MSVC/GCC完整流/recon/trace一致，固定AVM逐可见样本一致。A报告`R5-A-motion-rd.md`。

180源及完整候选源码冻结在`optimization-r5-20261018/root/source-manifest.json`与`candidate-source`，原176源中9项改变；Windows程序SHA256 `d6e2c1c1f9ce20d057b177eb9f66b15f0d6561d5421410bb8db471678b6fd705`已由B独立复制固定。后续源码开发不覆盖它。

原生质量已12/12配对、24条完整AVM/PSNR/VMAF通过，独立共同性能亦已完成。B事前统一选三类原生1920×1080、10bit420、24000/1001的帧48..71，24帧约1.001秒；逐帧payload与原完整解码文件相同，原H264和local Y4M只读。三源、QP80/100/120/140、P/pruned/adaptive、coeff-off全部保留。2D/3D/Real的PCHIP VMAF BD为−6.892/−2.018/−2.684%，PSNR-Y BD为−15.511/−1.635/−2.498%；00独立分段线性VMAF为−6.330/−1.798/−2.343%，PSNR-Y为−14.979/−1.744/−2.699%，方向一致但不外推其它片段。少数单点VMAF小降仍如实保存。

00核验全部24保留流的SHA/字节、源/程序/脚本/评分依赖、24帧指标及SSE重算PSNR通过，另三源各基线/候选六条重新固定AVM解码的raw SHA与已保存内部重建指纹一致。证据`optimization-r5-20261018/root/native-quality-audit.json`及`native-avm-spotcheck/verification.json`。首条2D/QP80基线编码218.87秒属于含recon导出的质量耗时，不作速度结论。

质量首次首对顺序预检后最多两路，其余所有源/QP保持冻结。正式速度在首次数据产生前已修订为R4/R5/R6共同窗口：QP100，单路，每源三版各两帧暖机后ABC/BCA/CAB三轮，记录进程CPU/墙时/后台负载，不导出recon或运行解码/VMAF。该窗口暂停全部新增编译/评测，原冻结长测继续，所以不声称整机隔离。事前冻结joint-protocol另绑三程序与参数，旧ABBAAB工具仅保留复现。报告`M6-r5-B-validation.md`及外置b目录保存独立原始数据。

## 阶段R6：有界±16整数搜索

在R5候选效果尚未产生之前，依固定AVM整数语法边界和成本预算确定下一项：保留完整R5赢家，增加外圈步4粗网格56点、固定最佳粗点±2局部细化最多24点，每叶最多80新增SAD、两条外域MV候选，后置最多8完整RD路径。不是完整±16全搜，也不引入亚像素或新的wire工具。

本实现MV±8/delta±16是人工guard。固定AVM的onepel shell set0覆盖class0..6；MV±16和同范围邻居产生最大每轴delta32、shell64，现有编码支路可覆盖。±32最坏shell128需要当前未实现的set1分支，禁止只放宽guard；具体行证据见`R6-A-motion-range-design.md`。R6需要额外极限码流/固定AVM逐样本验证，而不只是范围函数返回成功。

R6使用`--motion-opt wide`、独立源码/程序/构建与结果目录`optimization-r6-20261018`，不覆盖R5正在测试的程序或数据。184源/完整源码及程序已冻结，Windows程序SHA256 `c76e193b26be5a300684df3ff1e88862facbad85eb8be209c78d05b1c62bed2f`。00独立严格Windows48/48、GCC47/47、ASan/UBSan/LSan五项通过；新增wide机制10295/0，六组off对R4和六组rd对R5流/recon/trace不变。三格式AI/P跨平台及固定AVM验证通过，另三格式受控+12位移均实际接受32次wide候选；正负极限MV/delta/shell六条非平坦流由00再次AVM逐样本确认。详见`R6-A-wide-motion.md`。

R6原生质量12/12通过，复用经指纹核验的R5候选质量点作基线，仅编码12个新候选。2D/3D/Real的PCHIP VMAF BD为−0.079439/−0.072969/−0.118122%，独立分段线性为−0.020987/−0.089294/+0.019399%；PSNR-Y PCHIP为−0.313748/−0.007229/−1.118874%，线性为−0.271984/+0.004310/−0.828004%。VMAF整体接近零，Real对积分方法变号，不能宣称稳健原生收益。全部点包括微小退步均保留。

00全部源/程序/依赖/流/指标审计、SSE重算PSNR及三源QP100 fresh AVM raw SHA复核通过，证据`optimization-r6-20261018/root/native-quality-audit.json`与`native-avm-spotcheck/verification.json`。最终quality result SHA256为`00b78e58d0d4f23361529bf4e71d6d3ae1740b79d169b1811f479d8aebaf3134`。独立共同速度窗口已完成，不能复用质量耗时。

## 阶段R7：基础概率自适应首关

依`basic-entropy-contract.md`，在R5/R6已冻结、质量长窗口继续时补帧内CDF更新，先限RD-off，保留PRIMARY_REF_NONE和逐帧默认模型，不混入新预测/量化工具。要求所有可达语义模型完整接线，先编码后规范更新，容量重试/帧/实例均fresh，未知模型拒绝；不能靠翻header位或相同CDF合并模型。独立宏展开核对默认边界、count与训练rate offset，单独验证规范更新，再通过固定AVM逐样本验收。

代码及接线已完成，默认关闭`--entropy-adapt off|on`。独立AVM宏展开3840组/31,440有效word一致；直接固定AVM update_cdf差分3,932,160步0差异。00严格Windows49/49、GCC48/48、ASan/UBSan/LSan六项通过。事前冻结96组小片矩阵覆盖三格式AI/P、每格式四qctx、四工具组合、完整与边界尺寸：off对R6完整流/重建一致；on的Windows/GCC完整流/重建一致，固定AVM逐样本一致。新机制检查3,934,722/0。报告`R7-A-entropy-adaptation.md`区分960物理bank行与实际可达上下文，并列出全部正式symbol-site闭合表。

192源/完整源码已冻结在`optimization-r7-20261018/root/source-manifest.json`和`candidate-source`；Windows程序SHA256 `3d9b45c047edd3682c158426827e1ed10840a4e72aa3cde6b9f52394550e8dd9`，B已复制独立固定。原生12/12配对、24流固定AVM通过，12点两端完整recon SHA、SSE和PSNR完全相同；候选VMAF仅复用相同像素的baseline评分。按QP80/100/120/140，2D字节减少8.570/9.630/12.072/17.619%，3D减少17.514/20.029/19.008/19.477%，Real减少8.074/6.989/8.429/11.419%。这属于RD-off纯熵收益，不是新视觉提升或与RD-on共同开启的结论。

00完整指纹/指标审计与三源QP100 fresh固定AVM raw SHA再次校验通过，见`optimization-r7-20261018/root/native-quality-audit.json`和`native-avm-spotcheck/verification.json`；B冻结报告`M6-r7-B-validation.md`。R6完整质量已通过。共同运动速度及R7的RD-off新鲜配对均已完成；不与RD-on运动窗口直接相除。

同QP的RD-off与RD-on质量/字节存在显著差异，不能把“新增RD”本身当作必然改善。完整四点曲线以M5 static RD-off为参照：2D/3D/Real的原pruned VMAF BD为+10.284/−4.173/+28.703%，R5运动RD为+2.440/−5.961/+25.428%；独立分段线性分别为+9.758/−4.256/+24.956%与+2.564/−5.867/+22.052%，方向一致。这说明R5改进了既有RD路径，但该路径在2D和Real仍落后于RD-off。各比较仅用各自公共质量区间，不将不同区间BD相乘。

A只读单位/候选审计`next-A-rd-baseline-audit.md`未发现固定点单位错误，不据三片调整lambda。外置54个单SB同入口锚用例覆盖三格式、AI/P、三QP和三种预定义纹理：原M5策略在exhaustive和pruned中均实际找到，几何/模式/MV/系数/重建以及D/R/J逐项一致，赢家J不高于锚；另有3个P/平坦加稀疏噪声用例中exhaustive优于pruned，证明局部包含原策略与裁剪损失可以同时存在。严格Windows/GCC及ASan/UBSan/LSan三方记录一致；证据`optimization-r7-20261018/a/anchor-proof`与`root/anchor-check/verification.json`。单SB证明不外推整帧参考与上下文分岔后的曲线。完整42项profile曲线比较已落`optimization-r7-20261018/root/profile-comparison.json`；独立性能全部完成并经00复核。

## 独立性能最终结果

共同窗口27正式/9暖机和R7窗口18正式/6暖机全部PASS。QP100三配对墙时中位增量按2D/3D/Real：R5对原pruned为+17.33/+29.98/+17.25%，R6对R5为+5.31/+7.90/+1.87%，R7 on对RD-off静态CDF为+12.55/+13.20/+16.94%。进程CPU配对中位增量分别为+17.08/+27.95/+19.04%、+3.07/+8.87/+1.33%、+12.08/+13.97/+11.96%。全部范围和45条原始正式记录保留；R6的2D/Real单轮CPU范围跨1，墙时则三轮均增加，不能声称每次CPU都变慢。

00独立核验全部45条正式流SHA/字节均匹配已验收的QP100质量流、程序/脚本/合同/协议一致，并复算配对、median/min/max全部通过。证据`optimization-r7-20261018/root/motion-speed-audit.json`和`entropy-speed-audit.json`。后台整机CPU估计中位约18.2%至19.3%，原大测持续，所以非整机隔离。R6近零质量收益但明确墙时成本，不支持默认升级；R7真实同像素压缩收益需权衡约13%至17%墙时成本。

## 同片段SVT-AV1参照

00另冻结同native24、既有协议六CRF28/32/36/40/44/48、preset6、lp1/threads1的18点新鲜参照，编码/24帧解码/PSNR/VMAF全部PASS；证据`optimization-r6-20261018/root/native-av1`。使用视频packet payload，不将旧15秒8倍比值直接套到新1秒，也不将此质量工作流耗时当独立速度。SVT-AV1默认RA与本实现低延迟P的结构差异保留说明，不能据此归因所有差距。

固定2D/QP100 R5点562387字节/VMAF92.948348，SVT-AV1 CRF48为44321字节/93.399432，直接实测后者画质更高且R5体积约为其12.7倍；这只适用于该片该点，不冒称所有场景统一比值。00解析R5三源QP100真实OBU：2D key占12.4%、Real占7.2%、3D占43.9%，因此2D/Real大部分码量在P帧，不能仅以1秒key开销解释差距。运动小改尚不足弥合根本差距，后续基础项继续保留。

## 后续基础与测试保护

亚像素、实用帧内预测/递归分区、CDF自适应与RD组合、完整系数优化、环路和参考/时域结构仍属后续基础项，未完成。亚像素需要精度信令、对应CDF、MV单位及插值链一起接入；CDF自适应与RD组合需要模型状态事务回滚。完成基础后再评估AVM高级工具，不用新工具掩盖基础缺失。

原`campaign-local-r2-20261017`由既有计划任务和冻结程序继续运行；新构建j2、质量最多2路、速度单路，进入其isolated-speed即暂停新增计算，本轮无冲突。额外负载可能延长质量测试，不承诺零资源影响。00最终核验192源、三阶段冻结程序和原长测76冻结文件/配置一致，见`optimization-r7-20261018/root/final-integrity.json`；14:00真实host/总控/batch及计划任务存活，唯一94/108质量任务PASS、无FAIL。新测量进程已结束。正式发布与M6整体最终验收仍待原长测和后续基础工作。

</details>


<details>
<summary>007 · M6-A-rd-core.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-A-rd-core.md`  
原文件 SHA-256：`fc8fe76f5491ed6fc9b62e72d8ce923222a0659eb6e898875dc972c869a83580`

# M6 A：有界 RD 核心交付

2026-10-17，状态 **READY_FOR_INTEGRATION**。主窗口独立解码、平台矩阵和真实视频效果验收尚未全部完成，不代表 M6 DONE。

## 实现

新增 `Source/Lib/Encoder/rd_frame.c/h`，生产 `off` 不进入此模块，旧 intra/inter frame core 未修改。Codec、公共 API、CLI、CMake 和 geometry 单 SB 入口由 00 集成；真实语法 rate session 由 B 提供。

完整 SB 枚举原树和可选四 32 树。Y 候选 DC/V/H（依旧开关和邻居合法性）及 P 的旧整数 ME 获胜 MV，UV DC。内部完整邻居可用时 AI 四叶 81 条、P 四叶 256 条，另有原树 3/4 条。ME 不属于全部 MV 穷举。Pruned 只在四叶树每叶按当前路径重建邻居的三平面预测 SAD 保留前两个，最终仍完整变换、量化、逆变换、裁剪和同语法 rate 评分；原树不剪枝。Partial SB 固定合法树，按 M5 预筛生成单一路径，明确不宣称其全模式 RD。

每个候选前恢复当前可见 SB 的三平面重建。候选 coeff 存于一次帧级分配的 scratch，TX64 仅复制 1024 个 packed coeff。语法上下文回滚交 B 的同 writer session。赢家以固定模式 ID 重放，检查完整 coeff/重建 hash、D 和 R 与试探相同，然后只提交一次。任意错误不返回有效输出计数；重试重新 reset 帧。源码和上一张 committed reference 全程只读，reference 提交仍由 Codec 在 packet 成功后完成。

内部可选 `candidate_hook` 输出每次 trial 的 SB、稳定候选 ID、D/R/J 以及 winner；`reverse_candidates` 只供换序验收。正式 CLI 保持零初始化。统计记录候选数、实际 forward 次数、replay 次数、总 D/R/J、lambda、完整/partial SB、ME 实际搜索候选数和 winner hash。

## 整数量纲与边界

使用合同 D：420 为 `4*(SSE_Y+SSE_U+SSE_V)`，444 为 `4*SSE_Y+SSE_U+SSE_V`，10-bit 统一四舍五入除 16。只计可见重建像素，不计 padding。三平面总采样权重一致。

实际量化表 8-bit q255 的 ac=61440，10-bit q303 的 ac=245760；除 QTX 表 8 和 transform Q3 的 8，归一 8-bit 像素步长为 `ac/(64*2^(bd-8))`。固定系数 4，lambda_Q8 在两端均为 943718400；QP1 8-bit ac=40、lambda_Q8=400。没有额外施加 TX scale 或重复位深比例。

64×64 10-bit 三平面 D 上界为 1607468544，小于 2^31。R 为真实语法 Q12 估计 bit。`J=D+round(lambda_Q8*R_Q12/2^20)` 使用分解乘法，准确保留舍入，避免直接 uint64 乘法溢出；只有最终结果真正超出 uint64 才拒绝，不饱和。单测以 R=2^40 制造直接乘法溢出并验证正确的除后结果。

## 必要短验证

MSVC `/W4 /WX` 和 GCC `-Wall -Wextra -Wpedantic -Wshadow -Werror` 独立构建均通过，`test_m6_rd` 两平台均 **8813 checks, 0 failures**。覆盖：

- QP1/255、10-bit实际 q303 和整数舍入/溢出。
- 64×64、128×128 AI/P，内部 SB 精确 81/256 组合与原树之和。
- 136×72 10-bit420、120×120 10-bit444 P 的 partial backup、合法边界树。
- 每个 SB 穷举最低 J 与稳定 ID，换序后 coeff/recon/winner 一致。
- 第一 SB 的 pruned 每个候选属于 exhaustive 集合且成本逐项相等。
- 从完整重建独立逐像素计算的 D 等于决策累计 D，整帧 writer 重放 R 等于 SB 累计 R。
- 容量失败及第 5 次 transform 注入错误后重试等于干净路径。
- 源和 committed reference 含 padding 指纹不变。

命令：

```powershell
. ../activate.ps1 -Quiet
cmake -S . -B build/m6-a/Windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DSVT_AV2_WARNINGS_AS_ERRORS=ON -DSVT_AV2_BUILD_TESTS=ON
cmake --build build/m6-a/Windows --target test_m6_rd -j 8
build/m6-a/Windows/bin/test_m6_rd.exe
```

GCC 在 `ARXChem-Ubuntu-22.04-WSL1` 的 `/root/svt-av2-m6-a/GCC` 以同一源码构建。测试日志及交付 SHA 在 `artifacts/m6-a/{Windows-test.log,GCC-test.log,delivery-hashes.json}`；GCC executable SHA256 为 `ab32d395f28bdcddb31d020db94bc76f37052af3c2c1622d84a6aa51d4ccf6af`。

本执行窗口未启动全 QP、大视频或多平台长矩阵；这些留给 00 冻结后的统一无人值守批次。

</details>


<details>
<summary>008 · M6-B-rate-validation.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-B-rate-validation.md`  
原文件 SHA-256：`f6241c978dd02d5505ce90840830c0ce0575cea75974d75f999cc9dd8b4eab0a`

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
python tools/prepare_m6_real.py --root <LOCAL_PRIVATE>/ffmpeg-native-m6-media --ffmpeg ffmpeg.exe --ffprobe ffprobe.exe
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

</details>


<details>
<summary>009 · M6-background-host-20261018.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-background-host-20261018.md`  
原文件 SHA-256：`97272ae41f0d35f50d1ccefdeaa16c9948f6f8553b32f006967fb676c5dddd9a`

# M6 后台批处理再次退出：证据与计划任务托管

2026-10-18。用户反馈恢复后的后台进程再次丢失；本轮排查并更换外部托管方式，不修改编码器或冻结配置。正式仍0.0.5-m5，M6未最终验收。

## 本次退出与上次重启不同

机器LastBootUpTime仍为04:52:25，07:50以后未读到新的41/6008/1074记录。前次恢复总控11508实际记录了FAIL/real-quality：其子批处理18600于北京时间08:12:56.451以状态码1退出。总控stderr保留CalledProcessError；batch本身没有Python traceback、没有FAIL case，summary仍停在RUNNING。质量日志最后一条PASS在08:07:10。

65个完整质量结果均PASS，较前次恢复前64个新增3D/M5 QP248一项，证明恢复后确实继续运行过。现有证据不支持“本次又重启”或“已定位编码器崩溃”；也不足以断言是Codex会话清理、哪个进程调用了终止、或Python内部退出。相应窗口未读到可解释退出的Application错误、Defender阻断或安全日志进程退出事件；系统TLS警告与此无已证实因果关系。

前次“已确认启动”判断有实际进程和输出证据，但未能保证之后长期存活。本轮不能只重复同一直接启动方式。

## 保留与复核

证据目录：`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/recovery-local-r2-20261018-0823`。已保存FAIL状态、旧launch、batch summary、stdout/stderr和完整质量日志，再独立检查所有已有case保留产物SHA、run_id/job、退出码及RD trace。

- 完整质量65/108 PASS：2D36、3D29、Real0，没有FAIL结果；43项未完成。
- pilot6、小图1512、筛查576、AV1既有3/fresh18全部有效。
- 76项冻结文件及配置SHA全部匹配，原run_id未变。
- 原独占锁均可获取，不删除锁文件，不删除任何attempt。未完成项将另建attempt重跑，65个已完成结果复用。

## 新的外部托管

创建一次性、手动启动的Windows计划任务 `SVT-AV2-M6-local-r2-20261017`，使用当前用户Interactive登录类型，无密码保存。先以同一外部host执行2秒probe，计划任务结果0；确认probe的父进程是计划任务服务的svchost.exe，再切换正式action。并未把IsProcessInJob=true误解为完全脱离所有Windows Job，也不以该读数认定旧退出原因。

正式host是证据目录内 `task_host.py`：直接运行原冻结m6_campaign.py和原campaign-config.json；仅追加Python非缓冲输出及faulthandler，保存独立stdout/stderr、host/controller PID，并在控制器退出时记录exit code及时间。它等待控制器，不运行额外编码负载。没有改snapshot、模型、媒体、共享依赖、QP、jobs或timeout；未替换r2/r3优化后编码程序。

任务设置：ExecutionTimeLimit=PT0S（不设运行时限）、Priority4、MultipleInstances=IgnoreNew，允许电池启动且不因切换电池停止。不设置自动重试，防止真正错误被反复重跑掩盖；没有新增周期触发或自动开机触发。任务XML、probe和host记录均保留在证据目录。

北京时间08:24:20启动：host PID21732、总控PID17392，任务状态Running，原配置继续从头核验并复用已有结果。日志为原批目录 `20261018-082420-scheduler.stdout.log` / `.stderr.log`，原launch已另存，现launch关联新总控和任务名。

08:26:29复核：前9阶段再次PASS，RUNNING/real-quality，新batch PID20600、run_id与原批一致，3个编码子进程14904/4404/1652实际运行，分别续跑3D/pruned QP232/240的attempt-0003及3D/off QP248的attempt-0002。65个PASS保留复用。计划任务State=Running，LastTaskResult=267009（0x41301，正在运行，不是失败返回码）；stderr为空。该确认不能保证未来永不退出，若再退出应检查scheduler-exit.json、计划任务结果和新未缓冲日志，不能先归因或盲重启。

用户随后报告桌面出现Python控制台。已针对已核对host PID21732附着其控制台并隐藏窗口（句柄920034，visible true→false），host/总控/batch/3编码子进程PID全部保持，未中止或重启测试。证据console-hidden.json。后续计划任务action改用pythonw.exe，host启动实际python.exe控制器时设置STARTF_USESHOWWINDOW/SW_HIDE，让后续子进程继承隐藏控制台；本次正在运行的测试不重新启动。计划任务XML已同步保存。

后续查看：

```powershell
Get-ScheduledTask -TaskName SVT-AV2-M6-local-r2-20261017
Get-ScheduledTaskInfo -TaskName SVT-AV2-M6-local-r2-20261017
Get-Content <LOCAL_PRIVATE>\ffmpeg-native-m6-tests\campaign-local-r2-20261017\campaign-status.json
# 确认没有存活总控/编码子进程且解决失败原因后，手动续跑同一托管任务：
Start-ScheduledTask -TaskName SVT-AV2-M6-local-r2-20261017
```

仍以任务状态、真实进程、总控/子阶段状态与case结果共同判断，不能因一个RUNNING文本就断言持续正常。进入isolated-speed禁止其他编译/计算；全部完成后仍须独立审计，不自动发布M6。

</details>


<details>
<summary>010 · M6-basic-capability-assessment.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-basic-capability-assessment.md`  
原文件 SHA-256：`bdf32633bab0bbde308d6bd79bf16d3ef4a531d42a1725aef89431a452fa9078`

# M6基础能力阶段评估

2026-10-18，00+A/B。本轮三项候选的代码、质量和独立性能验收完成；基础阶段尚未完成，正式版本保持0.0.5-m5。

## 范围与证据

使用用户提供的2D、3D、Real原生1920×1080、10bit420素材。事前统一截取帧48..71，24000/1001、24帧约1.001秒；逐帧payload与完整解码素材一致，没有缩放或按效果挑片。质量固定QP80/100/120/140，所有点保留。解码器与VMAF依赖均独立冻结。详细执行记录见`basic-capability-progress.md`和各A/B报告。

R5/R6/R7分别保留独立完整源码、程序、清单、原始流和结果，外置于`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r{5,6,7}-20261018`；仓库无Git，各阶段相邻源码差异保存在`root/source-changes.patch`。未覆盖原大测试的冻结程序。

## 当前质量结论

| 候选 | 功能 | 原生质量结论 | 独立墙时增量：2D/3D/Real |
|---|---|---|---|
| R5 | 原±8整数搜索的多个候选参与真实RD | 12/12配对通过；相对原pruned，2D/3D/Real的VMAF BD为−6.892/−2.018/−2.684% | 相对原pruned：+17.33/+29.98/+17.25% |
| R6 | 保留R5赢家，有限预算搜索±16外域 | 12/12通过；2D/3D/Real VMAF BD为−0.079/−0.073/−0.118%，均近零 | 相对R5：+5.31/+7.90/+1.87% |
| R7 | 帧内CDF规范更新，首关仅RD-off | 12/12配对通过；相同重建像素下2D省8.570–17.619%、3D省17.514–20.029%、Real省6.989–11.419% | 相对RD-off静态CDF：+12.55/+13.20/+16.94% |

墙时为QP100新鲜三配对比值的中位数，质量为完整四QP；基线不同，不能直接相乘。R6本轮不支持默认升级；R7有明确同像素压缩收益，但也有计算成本。R5/R6均不是亚像素搜索，R6不是完整±16穷举；R7未引入跨帧模型继承，当前不支持与RD组合。所有候选保持默认关闭，相对SVT-AV1的整体差距仍未解决。

00另行审计全部保留流、源、程序、脚本及24帧指标，按SSE重算PSNR，并对R5/R6/R7原生流重新AVM解码核对重建指纹，全部通过。R6的Real VMAF BD改用分段线性积分为+0.019%，不支持稳健收益结论。严格Windows/GCC以及ASan/UBSan/LSan验收已通过；小片实际码流验证覆盖默认off兼容、跨编译器一致和固定AVM逐样本一致。

## 需要优先解决的基础问题

“R5相对既有RD有改善”不能解释为“RD已经优于原M5非RD路径”。同片完整四点曲线，以static RD-off为参照，原pruned的2D/3D/Real VMAF BD为+10.284/−4.173/+28.703%，R5为+2.440/−5.961/+25.428%。独立分段线性积分方向一致。2D和Real仍明显退步；所有候选继续默认关闭。

只读审计未找到lambda固定点单位错误。54个外置同入口单SB用例证明M5策略确实进入exhaustive/pruned候选，实际几何、模式、MV、系数、像素及D/R/J一致，RD赢家不差于该锚；但有3个用例的候选裁剪漏掉更低J路径。局部候选包含关系不能保证整帧/时序上下文分岔后的BD改善，不能据三条曲线直接改lambda。

下一步基础工作应先打通CDF自适应与RD的状态事务及码率一致性，再评估候选裁剪与整帧参考传播；半像素插值、帧内预测、递归分区、系数优化和参考/时域结构仍需补齐。A已提交`next-A-adaptive-rd-design.md`和`next-A-subpel-design.md`，两者均未实施。半像素需要精度信令、色度相位、对应CDF和shell set1一起接入，不能只放宽MV guard。

## 与SVT-AV1的差距

同一native24片段另测SVT-AV1 preset6、lp1/threads1，固定六CRF28..48共18点全部通过。使用视频packet payload，保留SVT-AV1默认RA与当前低延迟P的结构差异。原15秒测试的8倍比值不套用本轮短片。

直接观察：2D的R5 QP100为562387字节、VMAF92.948348，而SVT-AV1 CRF48为44321字节、VMAF93.399432；该点R5约12.7倍体积且画质更低。R5对应2D/Real的关键帧仅占12.4%/7.2%，大部分码量仍在P帧，不能仅用1秒片段关键帧开销解释。其它场景及质量区间单独比较，禁止外推单一倍数。

完整profile比较保存在`optimization-r7-20261018/root/profile-comparison.json`。当前R7自适应CDF/RD-off相对SVT-AV1的VMAF BD码率增量为2D+916.805%、3D+208.726%、Real+354.006%，即公共区间log-rate平均约10.17/3.09/4.54倍；独立线性积分约10.45/3.22/4.79倍。公共VMAF区间分别为[93.399432,95.432640]、[94.982864,96.071755]、[88.687419,93.428986]，前两组很窄，不能当成整段/所有质量的统一倍数。它们说明本轮熵收益远不足弥合差距；不将官方AVM的外部压缩率说法当成本项目已测事实。

## 独立性能协议与原长测保护

运动窗口固定R4/R5/R6三版本、QP100，三源分别两帧暖机后ABC/BCA/CAB三轮，共27条新鲜计时；R7另在RD-off进行ABBAAB，共18条新鲜计时。计时只读输入和写码流，记录墙时、进程CPU和后台负载，不导出recon、不解码、不计算VMAF。全部计时流需与已验收的QP100质量流指纹相同。

两窗口全部通过：运动27正式+9暖机，CDF18正式+6暖机。00独立复算三轮/配对统计、程序/脚本/合同/协议指纹，全部45条正式流的SHA和字节与已验收的QP100质量流匹配。证据为`optimization-r7-20261018/root/motion-speed-audit.json`、`entropy-speed-audit.json`；所有原始记录和范围在B报告与各b目录保留。

| 功能增量 | 2D墙时范围 / CPU中位增量 | 3D墙时范围 / CPU中位增量 | Real墙时范围 / CPU中位增量 |
|---|---|---|---|
| R5 / 原pruned | +16.82至19.90% / +17.08% | +29.53至30.33% / +27.95% | +16.29至17.68% / +19.04% |
| R6 / R5 | +3.37至7.10% / +3.07% | +7.64至8.94% / +8.87% | +1.65至1.91% / +1.33% |
| R7 on / RD-off静态CDF | +12.37至13.80% / +12.08% | +12.91至13.82% / +13.97% | +12.84至18.50% / +11.96% |

速度窗口暂停所有新增编译/编码/评分计算，原冻结大测试继续，因此不称整机隔离；样本估计的后台整机CPU中位约18.2%至19.3%。R6的2D/Real单轮CPU比值范围跨过1，不能说每次CPU都增加；墙时三轮均增加。原长测进入isolated-speed即暂停新增计算，实际没有冲突。质量耗时不用于性能比较，R7速度不与RD-on运动耗时直接相除。

00最终重核192源、三阶段冻结程序、原长测76冻结文件及配置全部一致，见`optimization-r7-20261018/root/final-integrity.json`。14:00原计划任务、host、总控和batch真实进程均存活，完整质量94/108唯一任务PASS、无FAIL；原大测试仍未结束。本轮不修改正式发布或原在测程序。

</details>


<details>
<summary>011 · M6-C1-first-delivery.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-C1-first-delivery.md`  
原文件 SHA-256：`9ba24ae624de72fd8b8d155d6c87c9cddf9e5d33df6d28a8a27e50fe76887807`

# M6-C1 首轮交付与续接记录

2026-10-18。用户已指定压缩路线图并澄清：本轮额度将耗尽，续费后于 2–4 天再接续开发。本轮按 [M6-C 合同](../m6-compression-contract.md) 收敛到 C1-A1，不启动新的原生大矩阵。正式版本仍 0.0.5-m5。

## 已冻结方向和边界

单线程压缩竞争力 → 内部多线程 → preset → CRF。Q1/Q2/Q3 未达到。当前未实现的基础能力不能用新开关、单测或局部 J 掩盖；达到成熟编码器水平仍须同源、同结构、多 QP、完整长片和独立 holdout 的 BD-rate 证据。

00 负责合同、CMake 和最终验收；A 只改 entropy_encoder.c/h、entropy_models.c 和新增 test_adaptive_rd.c；B 只写外置独立 fixed AVM oracle。公共 API、Codec、CLI、lambda、候选及训练参数本轮不变。完整 entropy-adapt+RD 仍由原配置校验拒绝，tile session 也未放开。

基线快照 271 文件和清单保存在 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-C1-20261018/root/baseline-source`、`baseline-manifest.json`。开发树没有 Git，本轮用源码快照、manifest 和差异补丁恢复。

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

</details>


<details>
<summary>012 · M6-integration.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-integration.md`  
原文件 SHA-256：`7c90516111c96738835f8b7cc4aef312a6720c94fc40ace2a8e6bb19f5750e4b`

# M6 有界 RD 集成与集中批测交接

> **2026-10-17压缩效率主线与后续短验**：同源2D直接支配点显示pruned100比质量更高的fresh AV1 CRF44大8.75倍；原18.8%合成耗时下降不能解决体积。已做外置三帧分类诊断（流/recon与冻结pilot一致）并合入量化峰值溢出预检等价优化，00 Windows45/GCC44/SAN3、B54流/AVM及18off/M5通过。详见[M6后续评估](M6-r3-assessment.md)。22:50原real-quality26/108 PASS，9/17阶段；76冻结文件及配置SHA匹配，未最终验收或发布。

> **2026-10-17 第二轮性能优化已独立短验通过**：仅RD核心提前拒绝非法路径、删除无用SAD、按行复制备份；合法候选/评分/码流/重建保持。00严格Windows45/GCC44、sanitizer专项3、2319条候选事件前后一致，B54流及AVM重建通过。合成核心三轮中位耗时约减少18.8%，不是隔离真实视频速度结论。详见[M6第二轮性能报告](M6-r2-performance.md)。外部原长测继续使用旧冻结程序；22:32左右real-quality为24/108 PASS，9/17阶段完成，未最终验收，正式仍0.0.5-m5。

> **本地原片r2续行优先**：用户已提供三H264原片及对应AV1成片，授权替换在线素材续跑。<LOCAL_PRIVATE>\ffmpeg-native-m6-tests\campaign-local-r2-20261017于2026-10-17 17:15:34（北京时间）以PID5284独立启动，17阶段。详见[M6本地原片测试](M6-local-campaign.md)。保留r1 FAIL及已通过矩阵，新批仅补真实效果/参数/速度部分；当前M6未最终验收，不以启动代替PASS。

> **2026-10-17验收续行优先：后台已在15:42:59于prepare-real停止，状态FAIL。** Boxing仅4.233333秒，固定5秒素材准备检查失败；编码测试前19/28阶段通过，独立复核20796任务/20916次编码及指纹全部通过。真实视频、单路性能与最终报告未运行，M6只阶段性验收通过，正式仍0.0.5-m5。详情[M6验收分析](M6-review-20261017.md)，机器证据artifacts/m6-review/partial-verification.json。本轮未改编码器或重启长测，后续应新r2接续真实素材阶段，勿原样重复r1或改写冻结证据。下方已启动/RUNNING为历史时点。

> **集中批次已于2026-10-17 13:15:58（北京时间）独立启动，PID9936。** 启动后已核实总控RUNNING/build-GCC，冻结源构建103个目标完成，stderr为空；其余28阶段按配置依次执行，AI按用户要求在此暂停。总控状态只读C:\Private\ffmpeg-native-m6-tests\campaign-r1-20261017\campaign-status.json。170项当前开发源与快照逐项MATCH，423项快照/依赖已校验；短验源清单SHA为08211aabbd1bac49a2a5153995096adbd241fb792c67e06579bcdec0caf1b286。启动不代表长测已通过。

2026-10-17，00主窗口+A=m6_core+B=m6_rate。**M6 RUNNING，开发接线和必要短验通过，完整长测及效果验收PENDING。正式版本仍0.0.5-m5，不发布M6。** 用户要求遇到长时间计算集中启动后暂停AI会话，全部任务完成后再集中审计。

## 已实现与短验

- 新增独立RD frame路径；off仍走原M5核心。公开参数SVT_AV2_PARAM_RD_MODE/配置rd_mode，CLI `--rd-mode off|exhaustive|pruned`，默认off；运行后修改拒绝。API增加最后完成帧RD统计，只用于可审计测量。
- 完整SB原树与四32树的真实transform/quant/inverse/clip重建D，实际量化表推导固定点lambda，同writer语法概率rate；完整候选组合与每叶前2候选剪枝。M5工具开关限定候选。Partial SB只保留M5预筛单路径，未冒充边缘全模式RD。
- 每候选回滚SB重建及语法上下文；赢家重放核对coeff/recon/D/R后提交。参考帧仍在正式码流成功后提交，packet扩容不重复RD。使用整数成本和溢出安全乘积。
- A的MSVC/GCC独立核心测试各8813 checks、0 failures；B rate/session各98534 checks、0 failures，包含候选换序、完整81/256域、最低J、剪枝子集、故障回滚、完整帧R与SB之和。
- 00独立Windows严格CTest **45/45**、GCC **44/44**。三格式AI/P×exhaustive/pruned **12条短流**固定AVM逐样本重建全部一致，证据artifacts/m6-root/smoke。
- 四模式小批次、三次编码及VMAF与审计、真实断点复用/单条损坏重跑/参数改变拒绝已通过。00另验证GCC四模式流/recon一致，跨平台缓存3条复用、1条破坏后重跑通过；自动Markdown/JSON/SVG报告实际生成通过。证据artifacts/m6-root/final-tool-proof、cross-proof和artifacts/m6-rate。
- 真实输入Narrator官方QP15完整源及720p/小图转换已短验。其余源下载与转换留后台；部分题材标签仍待缩略图复核，尚未宣称六类内容已验证。

合同：[m6-rd-contracts.md](../m6-rd-contracts.md)。模块报告：[A](M6-A-rd-core.md)、[B](M6-B-rate-validation.md)。短验最终指纹入口artifacts/m6-root/short-verification.json；不以这里的短验代替完整M6验收。

## 集中批次

批次目录：`<LOCAL_PRIVATE>\ffmpeg-native-m6-tests\campaign-r1-20261017`。
真实素材与转换目录：`<LOCAL_PRIVATE>\ffmpeg-native-m6-media`。当前工作区约29.27GB，大批结果和新增大素材均放工作区外。

总控先冻结source/工具/Windows程序/M5基线/oracle/模型和配置SHA；Linux三个配置从冻结源构建。自动依次执行：

| 项目 | 独立任务数 | 说明 |
|---|---:|---|
| 主边界矩阵 | 4320 | 三格式×三场景×15QP×AI/P×四工具×M5/off/full/pruned |
| 非整SB | 288 | 136×72、三格式、QP1/100/255、两结构、四工具、四模式 |
| 全QP | 3060 | 255QP、三格式、AI/P、full/pruned、两工具开启 |
| Linux完整流重放 | 12960 | GCC/Clang/SAN各重放主矩阵4320项 |
| 720p/1080p | 各54 | 三格式、三QP、AI/P、M5/off/pruned，含VMAF |
| 1000帧长链 | 60 | 三格式cut/noise、五QP、AI/P、pruned，每项3次 |
| 真实片pilot | 4 | 两片各1秒、M5/pruned、P；写入粗略耗时预算 |
| 真实片小图full对照 | 180 | 两片×三格式、五QP、AI/P、M5/full/pruned，每项3次 |
| 真实片主要曲线 | 160 | 六片测试+两片holdout、五QP、AI/P、M5/pruned，每项3次，jobs1 |

合计约**21140个任务、21940次编码**（含重复）；不把同源不同平台重复计作独立素材。下载约827MB，派生Y4M和临时raw另有预算。真实片使用固定高质量已压缩参考，不是摄影机无损母版；各种位深/色度转换明确标注。速度三次中位数及波动，包含CLI读写/重建导出，排除decoder/VMAF，真实速度阶段无其他批次并发。

每个batch完成后独立审计；错误停止后续阶段，不伪装PASS。仅复用输入、程序、合同、配置和保留产物指纹匹配的结果。每项保留首流/指标/JSON/log，成功验证后删除raw和重复流。最终reports/README.md、points.json、bd-rate.json及SVG提供每片体积—VMAF—时间曲线；非单调/无共同质量区间写N/A，实际失败另写FAIL。批次PASS只表示脚本运行完成，M6发布仍待AI独立验收。

## 查看和恢复

```powershell
Get-Content <LOCAL_PRIVATE>\ffmpeg-native-m6-tests\campaign-r1-20261017\campaign-status.json
# 读取总控progress字段对应的summary.json；逐项完成数可查看该batch/cases/*/result.json。
# 需要恢复时使用原配置与原快照，勿重新prepare或更改QP/模式：
& <LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tools\Start-M6Campaign.ps1 -RunRoot <LOCAL_PRIVATE>\ffmpeg-native-m6-tests\campaign-r1-20261017
```

恢复会重新验证所有冻结依赖；批测自行复用有效PASS，损坏或未完成项重跑。运行中不得删除本批快照、素材或build/m6-root/pydeps运行依赖；源码后续改动不会改变外部快照。无需AI持续在线或轮询，不启动自动消息通知。用户下轮要求汇报时先查总控和逐项结果，再做最终收集；不要看到进程消失就推定成功。

</details>


<details>
<summary>013 · M6-local-av1.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-local-av1.md`  
原文件 SHA-256：`c536e8de4abb37d0282f875f7e02faa8a83e54f0755c7957e7e268ff88050c16`

# M6 用户本地 AV1 对照测量工具

2026-10-17：完成测量工具，正式三片长测由总控调度，本报告不预填长测结果。

## 接口与依赖

`tools/m6_av1_reference.py` 使用媒体预处理生成的顶层列表 `pairs.json`，每行包含 `scene/source_path/source_sha256/av1_path/reference_y4m/frames/fps/width/height/depth/chroma`；支持可选 `video_payload_bytes/av1_sha256/frame_alignment`。

已实测现有 FFmpeg 支持 H264 软件解码、AV1 libdav1d/libaom 解码、libvmaf、SVT-AV1 v4.2.0 编码。无需为本次输入重新编译 FFmpeg。1 帧烟测证实 `-threads 1 -svtav1-params lp=1` 生效，日志报告 `Level of Parallelism: 1`。

```powershell
python tools/m6_av1_reference.py --pairs PAIRS_JSON --ffmpeg FFMPEG_EXE --ffprobe FFPROBE_EXE --vmaf-model VMAF_JSON --out-dir EXISTING_POINTS
python tools/m6_av1_reference.py --pairs PAIRS_JSON --ffmpeg FFMPEG_EXE --ffprobe FFPROBE_EXE --vmaf-model VMAF_JSON --out-dir FRESH_CURVE --crfs 28,32,36,40,44,48 --preset 6
```

第二条命令只生成新鲜 AV1 曲线，不混入既有成片点；两条命令必须使用独立输出目录。重复执行加 `--resume`，冻结配置、媒体、模型、程序及其同目录 DLL、测量脚本或保留日志发生变化会拒绝复用或重新测量。

## 度量合同

- 与 AV2 使用同一 H264 原片解码后的 10bit420 canonical Y4M，无缩放；8bit 源升至 10bit 不增加原始精度，H264 源也并非无损母版。
- AV1 解码使用 `-fps_mode passthrough`，不补帧、不丢帧，严格核对帧数、格式、分辨率和帧率；PSNR 按完整帧索引及三个原生平面计算，VMAF 读取双方 canonical 帧序列，重置 PTS。
- 码量为 `ffprobe v:0 packet.size` 之和，排除音频与容器，另报容器总字节。时长统一按 `frames/nativefps`，保留原始 PTS 对齐与尾端间隙说明，避免将容器时长与编码帧率混用。
- 用户既有 SVT-AV1 p6 CRF36 点保留 `encoding_seconds/median_seconds/fps=None`，不虚构原编码耗时、线程或结构元数据。用户文件名和说明提供 preset/CRF 来源。
- 可选新鲜曲线为 p6、单线程 `lp=1`、默认 RA 结构、每点编码一次；其时间包含 Y4M 读取与容器写入，不含 AV2 所需的重建导出，耗时比较需注明该差异。RA 对比当前 AV2 AI/低延迟 P 属于不同配置比较，不能泛化为 AV2 标准优势。
- 每片仅一个既有 AV1 点不能计算 AV1/AV2 BD-rate。新鲜曲线与既有成片分开，只有共同质量区间、足够有效单调点时才可计算 BD-rate。

输出 `points.json`、`summary.json`、`run-manifest.json`、`README.md` 与各点保留日志。点数据含 `origin`（`user-existing/fresh-single-thread`）、`bytes/bitrate_kbps/vmaf/quality.psnr_yuv_db/duration_seconds/frames/source_sha256/canonical_sha256/av1_sha256` 等，可由总报告叠加到 AV2 体积—质量曲线。

## 已执行短验

4 项单测通过，覆盖已知平面误差、缺帧/多帧/格式拒绝、依赖变更与损坏产物拒绝复用。

微型 H264→canonical→AV1 完整链路分别验证既有点和新鲜 CRF36 点，两者均 PASS，随后各自 `--resume` PASS。短验证据位于 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/av1-reference-pilot-e8db5ba8`；该微型素材仅证明工具接线，不作为实际压缩收益。

</details>


<details>
<summary>014 · M6-local-campaign.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-local-campaign.md`  
原文件 SHA-256：`49ab409aa85d37ef225895c18e89e34e4e03b1e510399ad025352f992b2a77cf`

# M6 本地H264原片集中测试

> 已于北京时间2026-10-17 17:15:34独立启动，PID5284，17阶段。启动后总控RUNNING/prepare-local，2D完整383帧Y4M已成功生成，stderr为空；76项冻结依赖校验通过。此为运行交接，不是完整测试通过声明。

2026-10-17，用户提供AV2/Test-Video/1080p-15s的三组H264原片与SVT-AV1 p6 CRF36成片，授权替换在线素材继续测试，正常开始后台长跑后结束AI会话。沿用00+A/B：A准备本地素材，B实现AV1对照，00负责独立链路检查、总控和最终报告。

## 参考与依赖

全部1920×1080，24000/1001 fps；2D/3D/Real分别383/378/390帧，H264原片8bit420，AV1成片10bit420。三对按序号帧数一致，PTS仅存在最多1ms的容器舍入差；2D等尾部存在时间戳间隙。统一解码帧序列为参考，保留全部帧，不循环、不dup/drop，Y4M按标称fps归一；帧序列时长用于码率，原容器时长/PTS另行保留。

完整与pilot/speed均不缩放，统一420p10；8bit升10bit不增加原始精度。2D缺少部分色彩元数据，冻结原probe并保留YUV码值，不冒充已确认BT.709。小图专项使用256×144及三格式，明确缩放与padding，不把小图得分当1080p得分。

现有完整版FFmpeg已实测支持H264软件解码、AV1软件解码、SVT-AV1编码与libvmaf，无须新增编译依赖。FFmpeg/ffprobe、M6 encoder及M5基线、固定AVM decoder、VMAF模型与脚本均复制到独立快照并绑定SHA；原六文件只读，派生媒体在C:\Private\ffmpeg-native-m6-local-media。

已完成三片短解码预检、4项AV1工具自测，以及同一小图的H264参考→既有AV1和fresh六CRF→AV2三模式→AVM重建→审计→SVG/JSON/Markdown完整链路，全部PASS。小图证明在C:\Private\ffmpeg-native-m6-tests\local-report-proof-20261017，仅用于工具链验收，不是完整三片效果结论。M6编码器算法未修改，沿用r1已通过的170源/程序与20796任务证据。

## 集中配置

目录：<LOCAL_PRIVATE>\ffmpeg-native-m6-tests\campaign-local-r2-20261017。

| 阶段 | 配置 | 任务数 |
|---|---|---:|
| 原片pilot | 三片各3帧、QP100/P、M5/pruned | 6 |
| 用户AV1点 | 三段现成p6 CRF36，对完整原片测VMAF/PSNR和视频packet净码量 | 3 |
| fresh AV1曲线 | 同一完整参考，p6、CRF28/32/36/40/44/48，lp1、单次 | 18 |
| 小图穷举对照 | 三片×三格式×7QP×AI/P×四工具×off/full/pruned | 1512 |
| 1080p参数筛查 | 三片各3帧×12QP×AI/P×四工具×off/pruned | 576 |
| 完整1080p主曲线 | 三片全帧×12QP×P×M5/off/pruned，jobs3单次 | 108 |
| 完整AI补充 | 三片全帧×4QP×AI×M5/pruned，jobs3单次 | 24 |
| 独立速度样例 | 每片前24帧×QP80/160/240×P×M5/pruned，jobs1重复3次 | 18 |

12个QP：40/80/100/140/180/200/216/224/232/240/248/255。四工具是DC/adaptive intra与fixed/adaptive partition的组合；不用尚未实现实际策略的preset枚举凑数。小图包含exhaustive及8bit420/10bit420/444，完整曲线主测10bit420。共2244个AV2配置任务，加AV1度量/曲线共2265项；含重复总计2298次编码，其中现成AV1只解码评分、不算新编码。

完整片质量曲线并发运行，其时间不能当单路性能；24帧速度样例只对该段报告三次中位数及波动，不伪装成完整15–16秒耗时。AV2时间含导出重建，fresh AV1不含，速度表注明差异；用户成片原编码时间未知，保留null。

## AV1/AV2结论边界

参考为H264原片解码画面，避免以已经AV1有损压缩的成片当真值。现成AV1只提供每片一个点；另测fresh AV1六点后，才在严格单调且有共同质量区间时计算BD-rate，其他情况写N/A。完整片报告叠加AV1点和曲线，检查是否存在AV2体积更小且VMAF不低于用户成片的已测点；未测中间QP不捏造为实测。

fresh AV1默认RA，AV2当前只有AI/单参考低延迟P，参考结构不同。这是当前实现与配置在三片上的比较，不推断编码标准总体优势。用户三类片段替代此次在线素材计划，未添加独立holdout，不宣称原六类加holdout门槛已满足。

## 状态与恢复

总控campaign-status.json为真实状态，逐项结果cases/*/result.json支持有效指纹复用。任何配置或保留文件变化拒绝误用旧PASS；成功后清除大raw与重复流。r1 FAIL与冻结源码/程序和全部已验收测试保持原状，不重跑其大矩阵。

```powershell
Get-Content <LOCAL_PRIVATE>\ffmpeg-native-m6-tests\campaign-local-r2-20261017\campaign-status.json
& <LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tools\Start-M6Campaign.ps1 -RunRoot <LOCAL_PRIVATE>\ffmpeg-native-m6-tests\campaign-local-r2-20261017
```

恢复仍使用相同冻结配置。完整输出在reports/README.md、av2-points.json、av1-points.json、cross-codec-comparison.json与SVG。总控PASS后仍需下一次AI独立验收，正式M6版本不自动发布。

</details>


<details>
<summary>015 · M6-local-media.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-local-media.md`  
原文件 SHA-256：`63cfa36bdbf0cf82c2f2dba2adf90313a58dff06a1d6412ee8d958b26ab8b44b`

# M6 用户本地素材预处理

2026-10-17。原始六个 MKV 只读，派生文件统一放在 `<LOCAL_PRIVATE>/ffmpeg-native-m6-local-media`。现有 `C:/PortableSoft/FFmpegFreeUI ReadyToRun x64/ffmpeg.exe` 已包含软件 H264、AV1 解码和 libvmaf，无须为解码重新编译 FFmpeg。

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
python tools/prepare_m6_local.py --input-dir ../Test-Video/1080p-15s --root <LOCAL_PRIVATE>/ffmpeg-native-m6-local-media --ffmpeg 'C:/PortableSoft/FFmpegFreeUI ReadyToRun x64/ffmpeg.exe' --ffprobe 'C:/PortableSoft/FFmpegFreeUI ReadyToRun x64/ffprobe.exe'
```

加 `--pilot-only` 仅生成短验及小图素材，清单文件带 `-preflight`，不会冒充完整参考。2026-10-17 本轮此短预检已通过三片；尚未在预检阶段生成完整帧派生，完整准备交后台统一阶段执行。重复执行会复核输入、工具及已有派生SHA，发现改变即拒绝复用。

</details>


<details>
<summary>016 · M6-r2-A-performance.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r2-A-performance.md`  
原文件 SHA-256：`0e9bd184978519d6d7e2fa9c5a0e74cf72df81367caa89e4a980cc9c2da3d58b`

# M6 第二轮 A：消除 RD 无效计算

2026-10-17，状态 **READY_FOR_INTEGRATION**。仅本轮局部性能优化交付；M6完整质量、速度和发布仍须00最终审计，正式版本仍0.0.5-m5。

## 范围与诊断

生产仅修改`Source/Lib/Encoder/rd_frame.c`。候选集合、顺序、ID、lambda、合法候选的预测/变换/量化/重建和评分保持。未改剪枝策略、ME搜索域、并行方式或off路径。长测、快照、媒体、模型和依赖均保持只读；计算前检查总控，所有本次计算开始时均为real-quality，未触碰isolated-speed。

外置baseline来自00已逐项匹配170源的`optimization-r2-20261017/root/baseline-source`。在单独profile-source加入QueryPerformanceCounter函数包裹，仅运行现有合成`test_m6_rd`，得到8813 checks、0 failures：

| 区域 | 累计秒 |
| --- | ---: |
| build_path | 2.739956 |
| reconstruct_leaf，包含predict | 2.702423 |
| predict，包含SAD | 0.220785 |
| rank_modes，包含predict | 0.035856 |
| region_copy | 0.127153 |
| geometry_for_sb，包含ME | 0.016927 |
| path_hash | 0.009218 |

这些值互相包含，不能相加计算总百分比，探针本身也有开销；仅是该合成机制测试的画像，不能代表真实1080p总体热点。原始探针脚本、外置探针源及profile-test.log均保留。

B的已有1512条小图结果进一步给出实际重复计算线索：tools=10的AI每帧有效候选同为20，但exhaustive平均2216.7次forward、pruned536.7次。源码定位确认exhaustive原先逐叶重建后才发现后面叶的模式不合法；本轮未把它猜成剪枝本身的开销。

## 最小改动

1. exhaustive且非partial、非赢家重放时，先按同一ordinal检查全部叶的模式合法性。任何叶非法便直接跳过，不再对非法路径的前缀做预测/变换。legal_mode只依赖帧/叶位置/开关，不依赖候选重建，因此合法候选完全不变。
2. reconstruct_leaf只需要预测，不使用predict产出的SAD；允许内部predict的SAD指针为NULL，跳过三平面SAD扫描。剪枝与partial的排序仍真实计算同一SAD。
3. SB备份和恢复按可见行memcpy，保持位深对应的原始样本字节。原uint16备份分配足以容纳8/10bit；只复制可见宽度，不覆盖帧padding。

`forward_calls`仍表示实际forward调用次数，因删除非法路径工作而减少；不伪造旧计数。其余candidate/replay/winner/D/R/J/lambda/full/partial必须保持。

## 验证与性能观察

- Windows MSVC `/W4 /WX`和GCC `-Wall -Wextra -Wpedantic -Wshadow -Werror`均全新外置构建，现有`test_m6_rd`各8813 checks、0 failures，包含换序、候选最优、源/参考只读、容量失败和第5次forward故障重试。
- B独立54条差分初验PASS：三格式AI/P、三RD、QP1/100/255、四工具、64x64/128x64/72x72；完整流、recon与决策统计一致，54条固定AVM解码重建一致，18条off与冻结M5相同。详情`b/regression-initial/result.json`；最终候选复验以B和00报告为准。
- A-only的前后源副本除rd_frame.c外全部相同。现有合成测试三轮before/after交替运行，前为3.677394/3.613297/3.813351秒，后为3.022508/2.918449/2.986642秒，中位3.677394→2.986642秒，约18.8%时间减少。六次各8813/0。后台real-quality同时运行，**仅作为合成测试方向性观察，不是隔离真实视频单路速度证明**。未启动新的长测矩阵。

全部外置证据目录：`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r2-20261017/a/`。入口`delivery-hashes.json`、`rd_frame.diff`、`paired-timing.json`、`Windows-test.log`、`GCC-test.log`、`profile-test.log`；before-build与after-build为独立A-only计时程序。未写公共CMake、README/project/changelog；由00集成统一回写。

</details>


<details>
<summary>017 · M6-r2-B-validation.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r2-B-validation.md`  
原文件 SHA-256：`77cb126743a16a7d3a4dab837db2aed2bc5f261a0e4cce6f23f6c883fe4cfb5b`

# M6 第二轮 B：独立有界回归与已有小图数据分析

2026-10-17。当前交付为 `READY_FOR_INTEGRATION`，正式版本仍为 `0.0.5-m5`。本轮不发布 M6，不修改冻结长测、媒体、模型或依赖。原 M6.2 AI/P 完整块 RD 已实现，此处是其第二轮性能优化验证。

## 独立回归

新增 `tools/m6_r2_regress.py`，固定比较冻结 M6 基线与候选的完整码流、内部重建和逐帧 RD 决策统计；唯一允许变化的是 `forward` 实际变换次数，并要求其不增加。`candidates/replay/winner/D/Rq12/J/lambdaq8/full/partial` 必须相同。流一致同时覆盖最终系数语法，不把没有单独导出的内部 coefficient 数组冒称已逐元素比较。

54 个配置覆盖三格式（8bit420、10bit420/444）、AI/P、off/exhaustive/pruned、64×64/128×64/72×72、QP1/100/255和四种工具组合；三帧确定性纹理带位移及第三帧内容变化。尺寸/QP/工具采用分配覆盖，不是全部笛卡尔积。18 个 off 配置额外与冻结 M5 流及重建比较。每条候选流调用固定 AVM 解码器，比较全部可见重建字节。计算前逐次读取总控状态，遇到 `isolated-speed` 停止后续计算。

首轮 54/54 PASS，54 条固定 AVM 解码逐字节一致，18 条 M5/off 流及重建一致。证据：`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r2-20261017/b/regression-initial/result.json`。基线为长测只读 `snapshot/current/encoder.exe`，SHA256 `16a9698dc53aa51d74221b84b387660acb9ce5763713c5650aa0c419cfa260da`；候选为 A 的 `a/Windows/bin/SvtAv2EncApp.exe`，SHA256 `2548e1020d34d05f3debafb4932fe85f085452ac43d9c4166d3fd78935480f22`。

首轮执行后工具增加邻接 DLL 指纹和 forward 不增加的显式断言。按 00 要求不重复相同源码的54条计算，离线重审原始结果及实际程序指纹：108条 RD 帧记录全部 forward 不增加，78条不变、30条下降，总计35622→23445；补记冻结 M6/M5 邻接 DLL 指纹。独立证据 `b/regression-offline-audit.json` 为 PASS，并绑定原 result SHA256 `a1f75d4edcfbe565f21f521bb9aff857c93bacfdacca06578169bfa648d4144a`。独立 Linux 编译及单测由 A/00 提供，本报告不将 Windows 短流结果外推为 Linux 验收。

## 源码交叉审查

对照 00 的 `root/baseline-source/Source/Lib/Encoder/rd_frame.c` 审查 A 的三处修改，未发现阻塞问题：

- 区域备份改为逐行 memcpy：原/目标偏移均按字节计算，行宽为 `block->w * step`，输入仍按自身 byte stride 逐行访问；memcpy 不要求样本或行指针对齐，可支持10bit奇数stride与非对齐地址。每平面 backup 原有8192字节容量覆盖最大64×64×2字节，backup仅由此函数保存/恢复，没有其他代码按旧 uint16 索引读取。只复制可见区，不写padding。
- exhaustive 无效路径预检仅调用纯读取的 `legal_mode`，判断依赖固定的叶坐标、工具开关与 reference 是否存在，不读取重建邻居。候选循环每次先恢复 SB 入状态，非法候选不进入 rate trial；合法路径顺序、ID与所有重建仍保持。因此跳过旧无效路径在后叶拒绝前做过的变换，不改变后续候选邻居或熵状态。partial/pruned/赢家重放维持原路径。
- reconstruct 传 NULL SAD 仅跳过预测后的源像素差扫描，所有预测输出和错误返回仍完成；rank_modes 仍传实际 SAD 数组。未改变候选排名。

以上 stride/unaligned 结论来自代码语义审查，54条 CLI 回归使用内部正常分配帧；不冒称另做了奇数stride API实验。00另行比较全部合法候选 observe 输出，补强局部候选级不变量。

## 已有 1512 项小图数据

`tools/m6_r2_analyze.py` 只读 `campaign-local-r2-20261017/small-exhaustive/cases/*/result.json`，确认1512项均 PASS；三片×三格式×7QP×AI/P×四工具×三RD，256×144、各5帧。分析结果在 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r2-20261017/b/small-analysis.json`。

504 组 exhaustive/pruned 配对中，438 组流与重建完全相同，66 组不同。全部差异集中于同时开放 adaptive partition 和 adaptive intra 的 `11` 组合。其余工具组合的378组全部相同。

| 结构/工具 | exhaustive 候选/帧均值 | pruned 候选/帧均值 | exhaustive forward/帧均值 | pruned forward/帧均值 | pruned/full 时间比中位数 |
|---|---:|---:|---:|---:|---:|
| AI/10 | 20 | 20 | 2216.71 | 536.71 | 0.258 |
| AI/11 | 421 | 142 | 8711.43 | 1646.43 | 0.225 |
| P/10 | 122.4 | 122.4 | 5196.60 | 1481.40 | 0.415 |
| P/11 | 1241 | 154.8 | 16273.43 | 1727.11 | 0.162 |

工具位顺序是 partition/intra。`10` 组合两模式合法候选计数完全相同，forward 却明显不同，是候选计数之外存在重复计算的真实证据。它支持 A 对无效路径预检及重复重建的进一步画像，不能单凭此表断言所有耗时都来自剪枝。

时间来自原批次 jobs4，包含 CLI 读写和导出重建，仅描述该批次记录，不能称单路速度收益。`00/01` 的 pruned/full 时间比中位数在0.998至1.007附近，说明没有可辨识收益。

pruned-full 的完整流字节差范围 −337..+273，均值 +3.752；VMAF 差范围 −1.152581..+0.301030，均值 −0.004361、中位数0。AI/11的34/63条不同、VMAF差 −0.054203..+0.123376；P/11的32/63条不同、差 −1.152581..+0.301030。这些是缩小图片段的实际效果差，不能扩写为1080p收益或标准总体优势。

现有 trace 只记录整帧总 D/R/J 和赢家摘要。先前 SB 或帧的赢家分歧会改变后续预测邻居、熵上下文和参考图，因此不能把两个整帧 J 相减称为“同一入状态的局部剪枝损失”。本轮如需精确局部 regret，应另作隔离插桩实验，不改冻结 baseline 或按这三片调 lambda。

## 边界

以上验证针对保持决策/流/重建一致的性能优化。没有新增大矩阵，没有复跑旧 r1 或在线九源；冻结本地 r2 长测继续。长测最终结果仍需 00 独立审计；不以进程退出或脚本 PASS 自动声明 M6 DONE。

</details>


<details>
<summary>018 · M6-r2-performance.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r2-performance.md`  
原文件 SHA-256：`a4982e90fe9c4ac2f9fe3427661c6c21add0d30d56aa548af6e3fcbfcec2363d`

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

全部新增较大产物在 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r2-20261017`。00入口 `root/final-verification.json`、`root/source-manifest.json`、`root/campaign-frozen-integrity.json`；A/B入口分别见其报告。本轮不改正式发布目录、不操作FFmpegFreeUI或OpenCode，也不向已交接停止的旧会话发送消息。

</details>


<details>
<summary>019 · M6-r3-A-kernel-audit.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r3-A-kernel-audit.md`  
原文件 SHA-256：`6c1c2c5c1eed1b254c359eec700ff6623443e7125ecf937dd64b0e50ca21c26c`

# M6 第三轮 A：压缩内核审计与等价量化优化

2026-10-17，状态 **READY_FOR_INTEGRATION**。先完成只读审计；00随后明确授权实施量化预检的小闭环。生产仅改`Source/Lib/Quantization/quantization.c`，新增差分测试仅在`tests/unit/test_quantization.c`，未再改RD模块。M6未最终验收，正式仍0.0.5-m5。

## 已能确定的结论

完整流逐字节等价要求意味着压缩后体积变化严格为0。本轮约20%的合成测试时间优化，不能解决5倍以上的体积差距；第三轮量化预检优化同样只减少计算。

B对现有完整2D点的只读结果：off QP100为5.288MB/VMAF93.694，而fresh AV1 CRF40为0.644MB/94.358；pruned QP100为4.583MB/93.401，而fresh AV1 CRF44为0.524MB/93.643。这些具体点同时体积和VMAF受支配，值得正面处理；但AV1 RA与当前AV2低延迟P的结构不一致，不能将其归纳为标准总体压缩率差距。B核实8条流均383帧、1key+382regular，TD+sequence仅787B，排除了这些流重复关键帧或封装作为主要体积来源。原始证据由B报告维护。

本次审计未发现足以宣称“已定位5倍差距根因”的数学/语法错误。需要把已证实的能力限制与可能影响区分开。

## 源码证据：已实现的最小能力及缺失

以下路径均相对本项目根，行号对应本报告时的当前源。

| 环节 | 已核实事实与证据 | 结论边界 |
| --- | --- | --- |
| 参考与运动 | `frame_header.c:208`实际参考数1、`:215`整数MV；`motion_estimation.h:4`半径8；`motion_estimation.c:56`对±8整像素逐点做亮度SAD，`rd_frame.c:101`只保留其一个获胜MV | 不是完整运动RD；快速运动、subpel、多参考/双向预测潜力受限。尚未量化各项在2D片的贡献；新增候选/RA不属于本轮等价优化 |
| 帧内 | `rd_frame.c:130`合法Y仅DC/V/H及ME；`:152-157`UV永远DC，`sequence_header.c:256/262`关闭边缘滤波/CfL | 不是完整AV2帧内工具；细纹理、斜向边缘、色度相关性可表达能力受限，贡献未测 |
| 分区 | `rd_frame.c:368-373`完整SB仅旧树/四32；partial仅一条路径；`frame_header.c:275`最大TX | 内容结构只能在有限块形状/模式中选择。不能把exhaustive叫完整AV2工具穷举 |
| 变换 | `transform_path.c:75`调用固定DCT；`transform.c:333/370`标量矩阵正/逆变换；`frame_header.c:282`reduced_tx_set | 缺少变换类型与独立TX大小RD。TX64仅1024 carried coeff是明确合同/解码实现，不应误当漏写3072系数修复 |
| 量化 | `quantization.h:196`固定factor48 forward、`:12`明示无perceptual/trellis；`quantization.c:379-393`独立标量level舍入；`sequence_header.c:417`关闭TCQ；`frame_header.c:258`无qmatrix，`:262`无delta_q | 已有重建与decoder一致不代表编码器已实现RDOQ；新系数优化/感知量化会改变码流，不能混进等价提速 |
| 熵概率 | `frame_header.c:205`PRIMARY_REF_NONE、`:231`disable_cdf_update=1；`coeff_syntax.c:731/791/988`有按上下文的EOB/level/sign语法 | 实际有算术编码、上下文与EOB，不是原始系数直写；缺少自适应CDF/跨帧概率继承。无法仅看源码给其bit损失百分比 |
| 环路滤波 | `frame_header.c:267`关闭deblock，`sequence_header.c:447/450/453/456`禁CDEF/GDF/restoration/CCSO | 重建缺少这些工具，码率—质量空间有限；新增会改变参考链和码流，需另立工具合同 |
| RD目标 | `rd_frame.c:15/45`冻结lambda与加权SSE；`:384`实际共享语法rate成本 | 加权SSE与VMAF目标不同；不按三片重调lambda，不以单个QP点证明lambda错误 |

注：上表只陈述本实现能力；没有把未读过的SVT-AV1源当证据，也没有给任何缺失工具指定未经实测的收益。

## 可疑点与下一步最小诊断

需要诊断而非预先定罪：残差是否主要来自大运动/亮度subpel缺失；大块DCT在局部纹理上是否导致大量非零；固定量化是否浪费低价值AC；固定CDF的符号分布失配；pruned丢掉的路径是否实质影响压缩。现有完整帧trace没有叶级模式、系数统计、分类语法成本，无法从字节数反推这些贡献。

最小闭环可以先只在外置探针源插桩，不改正式码流：对已冻结每类片段的固定少量帧，记录最终赢家每叶的尺寸/Y模式/inter/零系数/非零数/EOB/level分布，以及mode、MV、partition、coeff分类估计R；同时保留完整真实流字节和SSE。分类R仅为估计bit，不能当真实类别字节。固定帧数、程序/媒体指纹与总上限，不扩质量矩阵，isolated-speed阶段不得运行。若无这层证据，不按猜测决定新工具优先级。

## 严格等价优化候选及依赖

1. **已实现本轮小闭环：量化DC溢出预检取峰值。** 原来每个系数都调用`quantize_coeff_idx(..., pos_is_dc=1)`作预检；DC量化level在同一表上随绝对值单调非减，因此先扫描最大的原始有符号系数，再probe一次即可得到同一预检状态。`INT32_MIN`的幅值用int64，不转回不可表示的正int32。正式位置量化与末尾decoder DQ保持。
2. **UV排序复用，尚未实现。** `rank_modes`在同一叶/同一入重建下连续求DC/V/H的预测SAD；三种模式的UV都为DC，所以可只算一次UV预测/SAD，V/H仅算Y。不跨叶、不跨prefix缓存；inter必须单独重算三平面。无需改变transform回调次数。验收逐候选D/R/J/排序及完整流一致，先用外置探针测收益。
3. **生产变换参数缓存，尚未实现。** `transform_path.c:55`每次按QP/位深/尺寸初始化同一量化参数。可在同一production path state缓存当前三种scale的参数；必须按真正TX尺寸推scale，不能把TX64的32x32 carrier当TX32。只省初始化，需先量化收益再引入状态。
4. **叶级重建缓存，暂缓而非直接实现。** 四32树leaf0没有本SB内先前叶，四模式在同SB/tree的源/参考/MV/外部边界固定，可缓存每模式的三平面coeff、recon与未归一SSE；后面叶必须用全部先前模式prefix和相同入边界作为键。不能仅按(leaf,mode)缓存intra，否则邻居重建变化会破坏预测；inter预测才不依赖当前帧邻居。不能缓存完整语法R，因为熵邻居上下文不同。四叶每模式缓存最坏10bit444约72KiB（仅leaf0三平面packed coeff+recon），命中后仍复制。

叶缓存还受到内部transform函数指针可注入状态/故障的约束（`rd_frame.h`与`test_m6_rd`第5次forward故障）。未经接口明确的生产纯函数能力保证，直接减少回调次数可能跳过有状态回调的故障。应先在外置纯生产探针验证，再设计明确opt-in并保留审计/重放；当前不为缓存扩大公共API。量化当前小闭环不存在这一问题。

不直接删除最终decoder DQ：API单独接收`params.tx_scale`和`log_scale`，两者可不同；前一次未clip DQ与最终decoder DQ不保证相同。该风险在新增测试中专门覆盖。

## 本轮量化交付与验收

生产变更位于`quantization.c:453-470`，新增原算法差分oracle位于`test_quantization.c:1072`，新测试`:1090`。覆盖全部QP1..255、8/10bit、三平面、全部3×3 tx_scale/log_scale（包含不匹配）、DC=AC/AC倍增/减半、零/阈值±1/精确溢出边界/INT_MIN首尾/INT_MAX及输出sentinel。差分保留原先逐项DC预检算法，比较返回状态与全部q/dq数组（包含失败的未写区域）。

Windows MSVC `/W4 /WX`及GCC严格`-Wall -Wextra -Wpedantic -Wshadow -Werror`各全新构建，**test_quantization均31075009 checks、0 failures；test_m6_rd均8813/0**。B独立54/54条流与recon差分PASS、18条off/M5一致、54条AVM解码一致；108条RD帧包含forward的全部trace严格相同，证据`optimization-r3-20261017/b/regression/result.json`。00最终集成验收仍待执行。

外置微基准固定10bit QP100、32x32/scale1、1024混合系数、轮换三平面，200000次quantize_block：三轮交替before为1.315/1.342/1.327秒，after1.171/1.177/1.194秒，中位下降约11.3%，checksum均427001238102329。采用MSVC clock()计时；后台real-quality并发，具有调度噪声和真实资源占用，不能说零影响，也不是编码器单路实片速度。最初20000次的0.125/0.140秒样本太短且方向相反，仅保留作原始记录，不用于收益结论。

外置证据`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r3-20261017/a/`：`quantization.before.c`、`probe/bench.c`、`probe-timing.json`、Windows/GCC构建与测试log及delivery-hashes.json。本轮不启新长测，不改冻结campaign、媒体、模型、依赖或lambda。正式版不变。

</details>


<details>
<summary>020 · M6-r3-assessment.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r3-assessment.md`  
原文件 SHA-256：`f5fbe611bbb172f234c25ab11f1ee477448003db798c751b35fbcfd14a257f08`

# M6 后续评估：压缩效率主线与隔离优化

2026-10-17，00+A/B。结论：**可以继续隔离开发和有界诊断，但同码流提速不能解决体积差距；当前压缩效率差距已被同源实测确认，不能用约20%的合成耗时下降作为解决进展。** 本轮已实际完成一项量化等价优化及一条原分辨率诊断，不止制定计划。正式仍0.0.5-m5，M6最终验收未完成。

## 对体积问题的核实

完整2D为383帧、1920×1080、10bit420、24000/1001 fps。AV2与fresh AV1采用同一参考帧序列和质量链路。已完成点存在直接支配关系，无须假定QP等于CRF或插值等质：

| AV2实测 | AV2字节/VMAF | 质量更高的fresh AV1实测 | AV1净字节/VMAF | AV2体积倍数 |
| --- | --- | --- | --- | ---: |
| off QP100 | 5288140 / 93.694373 | p6 CRF40 | 644026 / 94.358019 | 8.211 |
| pruned QP100 | 4582803 / 93.400957 | p6 CRF44 | 523777 / 93.643407 | 8.750 |
| pruned QP80 | 10157606 / 95.491085 | p6 CRF28 | 1299485 / 95.804120 | 7.817 |

前两行AV1的Y/U/V PSNR也全部更高。AV1为RA，当前AV2为单参考低延迟P；差异的贡献尚未测量，但不能用它淡化当前实现严重落后。这里不声称编码标准总体优劣，也不把上述倍数叫精确等质BD-rate。完整三片主曲线尚未完成，不能外推为全部题材。

B只读解析8条完整流，均为1key+382regular，TD+sequence总共787字节；主要体积发生在后续帧载荷，排除这些流重复关键帧、反复序列头或容器开销作为主因。完整来源链、PSNR及帧大小证据见[B差距审计](M6-r3-B-gap-audit.md)。

## 已实际增加的内核诊断

新增 `tools/m6_r3_diagnostic_source.py`，将源码复制到全新外置目录后才插桩；原源码参数只读。00使用优化前已核验的源码副本构建诊断程序，对冻结2D pilot前三帧、QP100/P/adaptive intra+partition/pruned运行一次，统计最终提交模式、三平面非零系数与分类Q12语法成本。不是重新运行完整质量矩阵。

| 帧序号 | intra/inter叶数 | 全零叶数 | 位于±8搜索边界的inter叶数 | 系数语法占总估计R |
| --- | ---: | ---: | ---: | ---: |
| 0，key | 1293 / 0 | 248 | 0 | 96.56% |
| 1，P | 1145 / 265 | 360 | 78 | 95.83% |
| 2，P | 176 / 841 | 518 | 83 | 77.47% |

分区、运动/帧内帧间标志、帧内模式、Y/U/V系数六类Q12成本之和，逐帧严格等于原始总R：996198351、1146337743、182566802。分类占比是估计bit成本，不是可独立切分的真实字节比例；系数项还包含skip/EOB/level/sign等语法，不能直接叫非零level裸开销。

诊断完整流71152字节，SHA256 `34eca7f09cef5108379f0683e79ba3f4b0d9e82bee8fb79649fe573072bd002a`；recon SHA256 `3317edd792d19f9d2fd122b732b6044599493a512649b37e6b37aea1994741bc`，均与冻结pilot逐项相同。外置 `root/diagnostic-verification.json` 绑定原始日志与分类和验证。

这说明应继续调查预测、残差、变换量化和系数概率链路；不能靠消除少量帧头开销解决问题。第一张P帧81.21%叶选择intra、第二张17.31%，表明行为随帧变化；搜索边界命中也只提供调查线索。不能由三帧证明ME有bug、盲目扩大搜索半径、估计所有1080p帧的类别占比，或断言固定CDF贡献多少倍。当前仍未定位一个可以解释全部差距的具体错误。

## 代码审计与后续优先级

[A内核审计](M6-r3-A-kernel-audit.md)逐项给出源码位置。当前只有单参考/±8整数ME的一个SAD赢家、Y DC/V/H与UV DC、有限分区和DCT/标量量化；CDF更新、RDOQ及多种环路工具未实现或关闭。解码合法和内部重建一致是必要条件，不等于压缩效率已成熟，也不能把全部缺失都叫实现错误。

后续将压缩效率调查列为主线，等价提速作为辅助：

1. **先取得可归因证据。** 在外置诊断中补齐按叶尺寸/模式的残差与系数EOB/level分布，并用已有受控平移/静止样例核查预测与重建链；区别“预测残差大”和“相同残差编码成本高”。只需有界样例，不扩长测矩阵。
2. **再做单因素压缩候选。** 当前运动候选是否受SAD预筛限制、有限分区/变换是否造成残差冗余、固定概率是否显著失配，分别用相同入状态的反事实实验证明贡献。新增运动候选、CDF更新、RDOQ等会改变合同和码流，须另建明确的实验合同与隔离候选，不混入本轮等价优化或替换运行基线；不因三帧统计先指定“必然收益最大”的工具。
3. **保持压缩率门槛独立。** 机制测试全部PASS也不能称压缩问题解决。冻结大测完整结果用于多QP体积—质量评价；具体策略取舍等待该基线收齐，不能按三片调lambda。RA/多参考、新工具族、内部并行不因本轮评估自动扩入。
4. **有证据才增加等价缓存。** 同叶排序内UV DC/SAD复用可小范围试验；完整叶缓存依赖之前叶的重建与有状态transform回调，不能简单按(leaf,mode)缓存。A报告列明依赖与失败模式，尚未实现的保持待办。

## 本轮已合入的等价优化

`quantization.c` 原先为每个系数重复调用DC量化进行溢出预检。DC量化level对绝对幅值单调，现先扫描最大幅值的原始有符号系数，再只probe一次；INT32_MIN使用int64幅值。正式DC/AC位置量化、decoder DQ/clip和错误返回前不写输出的语义保持。未删除末尾DQ，因为独立API的tx_scale与log_scale可能不同。

原算法差分测试覆盖全部255QP、8/10bit、三平面、全部3×3 scale组合、DC/AC差异、阈值/溢出/INT32边界和失败输出。A在MSVC/GCC各31075009项量化检查、8813项RD检查通过。量化局部微基准三轮中位1.327→1.177秒约减少11.3%；这是后台real-quality并发下的局部观察，不是整编码收益，更没有体积收益。

00独立严格Windows CTest45/45、GCC44/44、GCC sanitizer量化/RD/API专项3/3全部通过。B对r2最终程序与新候选54/54流/recon/AVM通过，18条off/M5通过，108条RD帧包括forward的全部trace相同。00自己新构建Windows/GCC各三例完整流重放6/6通过。生产仅quantization.c变化，单测和新诊断工具另计；173项当前源清单与程序指纹已外置冻结。

## 对正在运行的大测的保护

本轮编译和产物仅在 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r3-20261017`，不重启、停止、覆盖任何冻结长测、媒体、模型、依赖和配置。开始时机器读到CPU3%、剩余物理内存约10.87GiB，仅为时点；编译并发上限2，不开大矩阵。额外计算会争用资源、可能延长质量阶段，不能承诺“零影响”；它不会改变冻结程序和结果语义。isolated-speed阶段禁止新增编译/计算。

北京时间22:50只读快照：原长测仍RUNNING/real-quality，9/17阶段完成，完整质量26/108 PASS；76项冻结文件及配置SHA重新核对全部匹配。后续状态必须重读。原长测使用旧冻结程序，其完整结果不自动代表r2/r3优化后程序的隔离速度。

00证据入口 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r3-20261017/root/final-verification.json`；本轮压缩效率审计与短验通过，M6最终验收及发布均未完成。

</details>


<details>
<summary>021 · M6-r3-B-gap-audit.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r3-B-gap-audit.md`  
原文件 SHA-256：`104ba865f66c1dcc4423397e38d615c4a69a648b582829d6da14314a779de3ca`

# M6 后续 B：AV1 差距与可归因边界审计

2026-10-17 北京时间22:41:21只读快照。总控 `RUNNING/real-quality`，25/108项已完成且均PASS：M5 9、off 9、pruned 7，当前完整片结果全部来自2D。AV1既有3/3、fresh18/18 PASS。三片完整主曲线尚未完成，不把2D结论扩写为全部题材。正式仍 `0.0.5-m5`；差距审计未编码、未重启或修改任何冻结长测文件。末节记录之后另行授权的量化等价优化短验。

## 实际差距成立，且不需要混同 QP 与 CRF

AV2 corpus的三片Y4M SHA256分别与fresh AV1的 `canonical_sha256`逐项相同；分辨率1920×1080、10bit420、帧序列及VMAF链路相同。AV2统计完整elementary stream字节，AV1统计视频packet净字节，未用容器总大小制造差距。

2D来源链交叉核对：原片 `1080p-2D.mkv` 的冻结SHA256为 `2b3b806a4e058c336b031249fba39dc07713fe35348140466a0ea0a646e73742`，媒体provenance、pairs.json和AV1记录一致；AV2实际输入Y4M及AV1 canonical均为 `70eec42e92f5e67c1f6055aa6380122c3cdf50ca3da2bfb72b8be56d50f31a22`。三方记录均383帧、24000/1001 fps，序列时长15.9742917秒。PTS尾部gap不dup/drop，按帧序列评分。本次核验冻结记录之间的一致性，没有重新读取2.3GB参考Y4M计算SHA；原始冻结文件完整性由总控/00独立指纹核验约束。

以下各行AV1实测点质量**更高**、体积更小，所以不需要假设QP100等于CRF36，也不需要把插值当实测。

| 2D AV2模式/QP | AV2字节 | AV2 VMAF | 对应实测fresh AV1 p6 | AV1净字节 | AV1 VMAF | AV2体积倍数 |
|---|---:|---:|---|---:|---:|---:|
| off/80 | 12426038 | 95.650526 | CRF28 | 1299485 | 95.804120 | 9.562 |
| pruned/80 | 10157606 | 95.491085 | CRF28 | 1299485 | 95.804120 | 7.817 |
| off/100 | 5288140 | 93.694373 | CRF40 | 644026 | 94.358019 | 8.211 |
| pruned/100 | 4582803 | 93.400957 | CRF44 | 523777 | 93.643407 | 8.750 |
| off/140 | 1812590 | 82.388542 | CRF48 | 426119 | 92.775487 | 4.254 |
| pruned/140 | 1430466 | 81.495828 | CRF48 | 426119 | 92.775487 | 3.357 |

这已支持“当前实现/配置存在严重压缩效率差距”。pruned相对off的局部改进，没有消除它。QP140行AV1质量高出约10分，不能把3.357或4.254倍叫精确等质码率比；它们是实测支配关系。

为回答等质口径，另在fresh AV1相邻实测点间按VMAF线性插值log(bytes)，不外推：off80/off100的等质AV2/AV1体积比分别约10.571/9.948，pruned80/pruned100约9.589/9.269。**这是两点局部模型估计，不是实测值、不是BD-rate，也不是完整跨编码器结论。** 主结论使用上表直接实测支配即可成立。现有用户CRF36成片2D为801583字节/VMAF94.955750，与fresh CRF36净字节和VMAF相同；但仅比较该点和QP100，不能称“等质量5倍”。

fresh AV1为RA，AV2为单参考低延迟P。结构差异会影响结果，但本审计没有测量其贡献，不能把它当作代码缺陷证据，也不能据此淡化上述实际落后。AV2当前质量阶段jobs3、耗时含recon导出；AV1单次lp1且不导出recon，不能从这里报公平的单路速度倍数。

两组关键支配点的PSNR也支持差距，而不是仅由VMAF一个指标反转：off100的Y/U/V PSNR为42.689839/48.144405/48.385265 dB，对比fresh AV1 CRF40为46.923317/50.657580/50.675817 dB；pruned100为42.187717/48.261985/48.505015 dB，对比fresh AV1 CRF44为46.367088/50.123992/50.182977 dB。均来自各自已完成result/points质量记录，三平面AV1均更高。

## 保留码流能排除哪些解释

已只读校验8条完整2D流（off/pruned，QP80/100/140/216）的SHA256并解析Annex B LEB128长度及OBU类型。每条恰好383个temporal delimiter、1个sequence header、1个closed-loop key OBU、382个regular tile group OBU，无扩展header。按temporal delimiter分帧，383帧字节求和严格等于result记录的完整流大小。

| 模式/QP | 首帧字节 | 后382帧字节 | 后续帧字节中位数 | 后续帧范围 |
|---|---:|---:|---:|---:|
| off/80 | 59134 | 12366904 | 19997 | 9888..130611 |
| pruned/80 | 53371 | 10104235 | 13392 | 7095..124129 |
| off/100 | 34615 | 5253525 | 6947.5 | 2765..79342 |
| pruned/100 | 30497 | 4552306 | 4762 | 1851..74571 |
| off/140 | 13415 | 1799175 | 2934.5 | 1642..29386 |
| pruned/140 | 11692 | 1418774 | 1877.5 | 996..26733 |
| off/216 | 2863 | 942276 | 2415 | 1791..4676 |
| pruned/216 | 1985 | 508127 | 1279 | 1029..3067 |

每条TD加sequence总共只有787字节。主要体积发生在后续regular frame/tile OBU中；不是容器封装、多写sequence或每帧都写key造成数MB差距。OBU类型不能说明P帧内部inter块比例；允许P帧选择intra块，不能把382个regular OBU冒称382帧全块inter。

QP216即使质量大幅下降仍有off945139字节/VMAF29.718108、pruned510112字节/VMAF14.473472。这提示应研究低质量端的残余码量，但现有数据无法证明它来自固定语法开销、coeff、MV、预测失配或量化映射中的哪一项。

## 现有 trace 不能完成的归因

RD逐帧trace只有 `candidates/forward/replay/D/Rq12/J/lambdaq8/full/partial/winner`。没有赢家逐叶intra/inter计数、分区/TX分布、非零系数/EOB分布、预测残差或语法类别rate。`Rq12`是共享writer路径的概率成本估计，不能直接拆成真实coeff字节。现有日志没有开启verbose逐帧packet大小；上表大小由保留流独立解析得到。

因此，当前不能定量回答“多少倍差距来自ME、UV DC、禁滤波、冻结CDF、变换、量化或系数写法”；也不能以合法解码和重建一致证明没有任何编码效率问题。合法性验证、保持码流的计算优化、真正改善压缩率，是三种独立证据。

## 建议的最小隔离诊断（尚未编码）

1. 在独立诊断构建只统计已提交赢家：按帧/YUV记录intra/inter叶数与像素面积、32/64分区、MV分布、SSE、非零数/EOB、全零TX数。将所统计的赢家数量与正式trace及geometry核对，诊断开关前后流/recon必须一致。
2. 在共享语法写入调用边界按类别统计mode/partition/MV/skip/EOB/coeff/literal的Q12估计R；分类和必须等于原始总R。把估计rate与真实OBU总字节并列，不能将它们称为精确独立字节数。
3. 仅用一个64×64或128×64、2至3帧的独立合成纹理/整数平移样例，观测ME是否选回已知位移、inter残差能否归零、提高QP时nonzero和真实流是否合理下降。先做语义核验，再考虑一小段真实输入；不新启完整矩阵、不改冻结lambda。

这些是给00/A选择的诊断设计，没有为差距归因启动编码或修改生产核心。若总控进入 `isolated-speed`，停止新增编译及计算验证，只继续读取已有证据/代码与写文档。

机器证据：`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r2-20261017/b/gap-audit.json`，含25项已完成结果、同源核验基础字段、实测支配配对、插值说明与8条流的OBU/帧大小审计。最终长测仍须独立收集审计，不宣称M6 DONE。

## 另行授权的量化等价优化验收

完成上述只读差距审计后，00授权对A的量化peak预检优化运行新的有界差分。B审查认为，原DC probe的level对绝对幅值单调，int64幅值覆盖INT_MIN，取最大幅值的原有符号系数进行一次probe，可保持原overflow拒绝与写输出前检查；正式量化、位置DC/AC选择及decoder DQ/clip均未改。

以第二轮最终 `optimization-r2-20261017/root/Windows/bin/SvtAv2EncApp.exe` 为基线，第三轮 `optimization-r3-20261017/a/Windows/bin/SvtAv2EncApp.exe` 为候选，54/54差分PASS，18条off/M5相同、54条固定AVM逐字节重建相同；108条RD帧全部trace字段（包括forward）严格相同。计算前及每次子进程前仍检查总控，处于real-quality。证据 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r3-20261017/b/regression/result.json`。这证明该有界集合的等价性，未声称改善压缩率；局部量化微基准与整编码速度由A/00另行报告。

</details>


<details>
<summary>022 · M6-r4-A-coeff-rd-v2.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r4-A-coeff-rd-v2.md`  
原文件 SHA-256：`23f778014a77de57f764cb159b3b5ca6805dcf11941c0bbd53dffb78e2878578`

# M6 隔离候选 A：V2 单平面尾部系数降一级

2026-10-18，状态 **READY_FOR_INTEGRATION**。按`docs/m6-r4-coeff-contract.md`冻结的V2修订实现。V1源码、程序、51对效果及VMAF结果保持外置冻结，V1不能作为压缩改进推广。本次是同一冻结集合的唯一再评测轮，未改lambda、QP、样本或测试预算。默认off及正式0.0.5-m5不变。

## V1否决与V2范围

B实测V1真实small P的VMAF BD为2D约+48%、3D约+6%、Real约+97%，不支持推广；不能因为其局部J下降就宣称实质压缩改进。当前采样没有旧测点直接支配，不把稀疏曲线插值当逐点证明；native单点大幅质量损失也是独立否决依据。

V2不做整叶AC清零、不成批删除所有unit系数。固定原赢家的模式/树，按叶raster、每叶Y/U/V顺序，各只试一次：依据既有DCT_DCT扫描找到当前量化输出最后一个非零AC系数，保持符号、幅值减1；若EOB<=1则不操作，不触碰DC。原|q|=1时变零并缩EOB，|q|>1时仅下降一级。其他系数原样。最多四叶×三平面，即12次完整path。

每叶3bit policy分别控制三个平面，candidate ID仍低9bit保留模式/树、bit9起编码最多12bit policy。每次使用当前已接受的prefix policy重建整条路径，因此后继叶量化/预测随前叶重建变化；没有跨prefix复用。修改q后使用现有`quant_params_init_tx_bd`和`dequantize_block`规范函数重新得到包含真实TX scale/位深/clip的dq，再inverse/clip与完整D/R评分。J和等值ID规则不变，policy0 hash不变。partial仍跳过。

单步幅度有限且逐平面独立，有更清晰的机制边界；不预设它一定有收益。若冻结集合不支持等质收益就拒绝，不靠改lambda或增删样本挽救。既有局部J单调保证仍不外推整帧、后续参考链或VMAF。

## 严格验证

仅A属主`rd_frame.c`、`test_m6_coeff.c`及本报告变化；内部头原有接口足够，无新公共API变更。

| 测试 | Windows /W4 /WX | GCC严格Werror |
| --- | ---: | ---: |
| 原test_m6_rd | 8813/0 | 8813/0 |
| V2 test_m6_coeff | 231084/0 | 231084/0 |

GCC使用`-Wall -Wextra -Wpedantic -Wshadow -Werror`，两平台独立全新目录、j2。所有计算前读取原campaign，均为real-quality，未在isolated-speed进行计算。

新增独立验证包装生产forward，保存每块应用policy前的原始qcoeff。每个committed hook在正式append之后，定位最终赢家重放的所有块，使用既有normative scan从原forward输出独立查找EOB，按candidate ID解码逐块policy，逐系数检查：被选中的平面仅原始末尾非零AC精确减一级，其余全相同，DC全相同。包含EOB<=1无操作、三格式、64TX packed1024与32/16TX。该检查不仅验证最终D/R，还直接约束变换输出变更集合。

其余机制保留并通过：同入状态候选最小J/ID、单64SB对原成本不劣、逐像素独立D、正式整帧writer R重放、默认off旧机制、partial零trial、最多12trial、候选计数增量、原候选反序后coeff/recon/hash一致、源/参考padding只读、容量错误和首个系数trial故障后的完整重试、生产变换下实际accept>0。没有自行重跑效果矩阵；效果由B/00使用同一冻结样本判断。

## 产物

外置目录`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r4-20261018/a/V2/`，含Windows/GCC构建与测试日志、`delivery-hashes.json`、相对冻结V1的`rd_frame.diff`和`test_m6_coeff.diff`。V1的A构建在其父目录，保持原程序。当前V2源码稳定，等待B效果与00集成验收；不以机制PASS替代压缩价值。

</details>


<details>
<summary>023 · M6-r4-A-coeff-rd.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r4-A-coeff-rd.md`  
原文件 SHA-256：`3f3c60253c17ed85a67749d4008114df301c7c43b92ce78829653f0a843f1b85`

# M6 隔离候选 A：有界系数 RD 精化

2026-10-18，状态 **READY_FOR_INTEGRATION**。遵循`docs/m6-r4-coeff-contract.md`；这次允许实验候选改变码流以检验压缩价值，默认关闭，正式仍0.0.5-m5。效果是否值得保留，由B预冻结样本和00独立验收决定，不能由局部J或机制单测放行。

## 实现与真实评分

A仅修改`Source/Lib/Encoder/rd_frame.c/h`，新增`tests/unit/test_m6_coeff.c`；公共API/Codec/CLI/CMake由00接线。内部`coeff_optimization=0/1`，默认0；新增实际统计`coeff_trials/coeff_accepts`。

先按原有RD合同选出完整SB的原始树/模式赢家，固定该树与模式，按raster叶序做一轮坐标精化。每叶分别试：policy1各平面只保留DC，policy2各平面全零；已有policy0原量化保留，其他叶使用目前已接受的policy。最多4叶、8条附加完整路径。partial跳过，仍走原边缘合同。不是完整RDOQ、完整系数穷举或全帧最优。

每条trial先从同一SB入口恢复三平面重建，再按固定模式重新预测、transform/quant、对本叶三平面的packed qcoeff/dqcoeff作相同置零、inverse/clip。后继叶全部随新邻居重建重新预测与量化，没有沿用旧SSE、旧预测或旧系数。DC-only实际保留每个平面的index0，TX64仍按1024 packed系数。

完整SB可见重建SSE经原D公式加权，R来自原tile_rate_trial同语法事务，lambda和整数J完全不变。仅更小J或等J更小稳定ID接受。低9bit保留原树/模式ID；bit9起每叶2bit表示policy。policy非0追加进path hash；policy0不追加零值，因此默认off的winner hash仍与旧实现相同。

试探均不提交writer状态。最后从入口按最终policy重放整path，核对D、真实语法R和包含模式/MV/coeff/recon/policy的hash后才append赢家并commit一次。失败恢复当前SB，帧输出计数清零，重试重新初始化。每次接受只证明同一入状态的当前SB J不增，不外推整帧/后续参考链J、PSNR或VMAF。

## 机制单测

MSVC `/W4 /WX`与GCC `-Wall -Wextra -Wpedantic -Wshadow -Werror`均全新外置构建（j2）：

| 检查 | Windows | GCC |
| --- | ---: | ---: |
| 原test_m6_rd | 8813/0 | 8813/0 |
| 新test_m6_coeff | 13041/0 | 13041/0 |

新增测试覆盖：

- 8bit420、10bit420/444，AI/P，64/128完整SB、136x72与120x120混合边界、56x56全partial。
- 原有QP100高纹理fixture，加低幅独立合成纹理QP80/140/220；后者确保真实生产transform下确有`coeff_accepts>0`，防止只跑未接受分支。前者无accept是实际结果，不隐藏。
- hook独立检查每个候选的整数J，并确认提交的是本SB所有已观察原候选及精化候选的最小J/最小ID；单64SB直接与相同入口的flag0成本比较。
- 额外trial不超过fullSB×8、accept不超过trial，候选增量等于trial；partial没有trial且hash与flag0相同。
- 解码提交候选ID的逐叶policy，直接检查最终输出packed qcoeff的对应AC或全系数为0；覆盖四叶ID位域合法性。
- 从输出重建独立逐像素计算D；正式整帧tile rate重放R等于累计SB R。
- 原模式候选反向枚举后，最终coeff/recon/hash/J/R保持一致；pruned仍是原模式域子集，系数精化不冒充扩展域穷举。
- 输出容量失败后可重试；单64SB故障注入定位在首个系数trial的forward调用，错误后干净重试完全恢复；源与committed reference含padding指纹保持。

首次严格MSVC发现trial_cost控制流赋值无法被编译器证明，已显式零初始化后两平台通过，未放宽告警。未改原test_m6_rd。

## 效果证据边界与产物

A没有另开效果矩阵或长测。B负责预先冻结的6小图×4QP×AI/P、固定AVM逐样本及真实bytes/D/PSNR比较，00负责默认off指纹、公共API/CLI、sanitizer和最终集成验收。只有这些结果能决定是否产生真实等质字节收益；同QP少字节不等于等质改善。

独立构建与日志位于`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r4-20261018/a/`，入口`delivery-hashes.json`、`rd_frame.diff`、`rd_frame.h.diff`、Windows/GCC编译和两项test日志。基线来自00冻结的173源`root/baseline-source`。

所有编译/测试开始前只读确认原长测处于real-quality；新增计算有实际CPU/内存占用，不宣称对后台零影响。没有触碰原campaign快照、媒体、模型、运行依赖和配置，未进入isolated-speed时继续计算；默认off与正式包不变。

</details>


<details>
<summary>024 · M6-r4-assessment.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r4-assessment.md`  
原文件 SHA-256：`e37d6dcc3c6646830f9ed3549aba4f3408b87ac0908134d5ad46cd17dfd567a2`

# M6 第四轮：实际压缩决策实验

2026-10-18。用户授权原冻结长测继续，同时推进压缩内核优化。正式版本仍为0.0.5-m5，未发布M6。本轮保留原173源基线、独立程序和失败数据，新增计算使用外置目录、每次编译最多j2、B单编码进程；进入isolated-speed即暂停新增计算，不能声称资源争用为零。

## 目标与判定

已有同源2D质量更高的AV1点仍比当前AV2小8倍以上，保持码流的提速不足以解决问题。本轮改变实际系数决策，按多QP等质量码率验收，不把同QP少字节或局部J下降当压缩收益。没有把AV2参考实现或x264的未经本地同源验证比较当成本轮结论，也没有承诺每轮减半。

新API/CLI只提供默认关闭的`--coeff-opt off|rd`实验开关，要求原RD开启；公共config/stat尾追加字段，所有调用者须重新编译。正式包、冻结campaign程序/媒体/模型/配置不改。算法与事前样本合同见`docs/m6-r4-coeff-contract.md`。

## V1已否决

固定原RD赢家树与模式，逐叶尝试三平面保留DC/全零。51个配对全部AVM重建一致，9条默认off兼容通过，但37/51以加权D上升换字节下降，真实small/P四点VMAF BD-rate为2D +48.085%、3D +6.391%、Real +96.944%。三片native首帧QP100同样伴随质量损失，Real从89.627降至82.952。上述小样本不能量化完整视频的总体退化幅度，但不足以支持推广该策略。

穷查冻结旧实测点没有发现直接支配新点的配对，不能编造逐点支配，也不能因此把退步解释成成功。V1源在`root/v1-source`，完整程序和结果在`b/v1-evidence`，不覆盖。独立报告见`M6-r4-B-validation.md`。

## V2：单平面尾部降一级

V1结果之后、首次V2编码之前，冻结更细策略：每叶依次对Y/U/V只尝试一次，将normative DCT_DCT扫描末尾最后一个非零AC幅值减1、保留符号、DC与其他系数。每SB最多12次整路径试探。每次从相同SB入口重建全部叶，后继预测/量化随已接受prefix重算；用现有规范函数重新生成dq，真实像素D和共享语法R评分，lambda保持原值。最终重放核对hash/D/R后只提交一次，partial与默认off仍走原路径。

这是有界尾部选择，不是完整RDOQ。局部J不增只约束同SB入口，不保证参考链或VMAF改善。候选ID从bit9起用每叶3bit表示平面操作，不写入码流。A报告`M6-r4-A-coeff-rd-v2.md`记录实现与逐系数独立验证。

## 独立尺度审计

00另外建立小C程序，直接链接真实transform/quantization库，分别对8/10bit、TX8/16/32/64、用户QP80/100/140、DC和首个AC注入非零level。10bit使用真实QP+48。只测逆变换残差能量，不经过源图、预测与最终像素clip。

比较`4*SSE / 2^(2*(bd-8)) / level²`与`4*(dequant_QTX/(64*2^(bd-8)))²`。固定level16的48项比值0.952936..1.705956；最大来自8bit TX64低幅DC的整数像素舍入，不能概括为全部接近1。另用level=2×side保持空间幅度的48项，比值0.959600..1.013430，未观察到系统性的8倍、位深或TX倍率错误。

该检查只覆盖两个低频基向量及上述条件，不能证明所有变换位置或量化决策正确，更不能证明lambda系数最优；所以没有据此修改原lambda或按三片调参。原始C/构建/日志及`scale-verification.json`、`scale-geometry-verification.json`保留在外置root目录。

## 效果与最终选择

V2同一冻结集合51对、9条默认off兼容、30项VMAF已完成；全部效果流由固定AVM逐样本匹配内部重建，51个baseline流/recon/原始trace与V1相同。没有修改源、QP、lambda、模型或删坏点。

| 真实small/P来源 | PSNR-Y BD-rate | 冻结PCHIP VMAF BD-rate | 独立线性log-rate积分 |
|---|---:|---:|---:|
| 2D | −0.210% | −0.481% | +1.400% |
| 3D | −0.819% | −1.585% | −1.401% |
| Real | +0.655% | −1.988% | −2.971% |

独立线性列用相同四点与相同共同质量区间，不替换事前冻结的PCHIP。2D的符号会随插值改变，不能将其平均改善称为稳健。区间很宽，包含极低质量点；四点数据不支持常用高质量区间或完整1080p视频的总体结论。Real的PSNR仍退步，synthetic-edges/AI PSNR BD为+31.135%，不能宣称各类样本/指标全面改善。

同时有无需插值的真实直接实测点：2D/P QP100从8971到8876字节、VMAF91.946371到92.045179；3D/P QP100从11414到11346字节、92.703582到92.758091；Real/P QP80从10292到10161字节、94.950708到95.024424。它们支持特定样例的有限研究价值，不覆盖其余点或后续参考链。三片native首帧QP100分别少3.80%/1.98%/3.16%字节，VMAF分别下降0.062712/0.022552/0.017608；它们不是严格等质点，不能据此声称native BD收益。

00选择保留V2为默认off的实验候选，V1粗清零不进入最终算法。不发布、不替换原长测程序，也不把约1%量级的部分小样本收益说成8倍差距已解决。当前证据不足以开启默认值；较大的缺口仍需在系数编码、预测/运动与分区能力上逐项受控验证，不能用继续批量清零或按三片调lambda挽救。

## 集成验收（完成本轮短验）

V2 Windows严格构建与46/46 CTest已通过，A Windows/GCC原RD各8813/0、新系数机制各231084/0；新测试从最终赢家重放的原始forward输出独立定位扫描末尾，检查精确一级变化、DC/其它系数不变，同时验证D/R、最小J/ID、反序确定性、partial、只读源/参考、容量与故障重试。

00独立GCC严格45/45、ASan/UBSan/LSan专项3/3通过；Windows46/46。另对三种支持格式的独立合成源、AI/P、QP100进行6组GCC实际编码，与B冻结Windows的完整流、重建及RD/coeff逐帧trace相同。未用跨平台机制测试冒充完整片效果。

最终源清单176项，原173源仅7项变化：rd_frame.c/h、公共header、Codec/CLI、API测试与测试CMake；新增test_m6_coeff.c和两项外部评测工具。原rd/quantization等其他源不变。00逐项重新核验B冻结的102条效果流、9源、程序与VMAF依赖，检查数据绑定与测试通过数。机器证据为`root/final-verification.json`及`root/source-manifest.json`。

收尾时原计划任务和总控17392仍运行在real-quality；完整质量66/108 PASS（2D36、3D30）、现有result无FAIL；76冻结项及campaign-config SHA全部匹配。该数是本轮核查时点，后续必须重读真实状态。此次短验不替代原17阶段长测和M6独立最终验收。

所有本轮产物位于`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r4-20261018`；主调度root、A a/V2、B b-v2。旧V1 Windows/GCC/SAN构建和数据保留，新V2用WindowsV2/GCCV2/SANV2。

</details>


<details>
<summary>025 · M6-r4-B-validation.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r4-B-validation.md`  
原文件 SHA-256：`b3b81806808c47a77c6ed0b18f79b6392ff621bd2ab81c4dc604f5d3354895be`

# M6 第四轮 B：系数RD候选独立验收

2026-10-18，00+A/B，当前 `READY_FOR_INTEGRATION`。结论：V1粗清零被效果审计否决；随后唯一一次V2细粒度末尾系数减一级实验，在相同冻结小图集合取得部分VMAF率失真改进及直接实测支配点，但2D四点BD符号对插值敏感，PSNR/低质量端仍混合，不能称三类均稳健改善、外推完整片或称解决AV1差距。两者机制与解码一致性均通过。本轮目标是实质编码决策改进的多QP率失真证据，不用保持码流提速冒充压缩率收益。正式仍 `0.0.5-m5`，M6没有最终发布。生产变更由A/00负责，B仅写独立工具与本报告。

## 先冻结样本与口径

候选测试前已冻结 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r4-20261018/b/manifest.json`，SHA256 `012fd7b4eaf6f86abb27836253374e9885e122a0a628a0559fe73c162a1b67eb`。QP80/100/140/220在任何候选结果产生前由00确定，不按三片调lambda、不按结果删样本或挑QP。

- 三个独立数学合成128×128、3帧样本：边缘/平滑叠加纹理（8bit420）、受控整数平移纹理（10bit420）、固定种子噪声（10bit444）。它们不来自三片原片；不同格式分配不同内容，不声称格式全笛卡尔积覆盖。
- 本地2D/3D/Real小图各256×144、10bit420、前三帧。每源4QP×AI/P×baseline/coeff-rd，合计96次小图编码。
- 三片原分辨率各1920×1080、10bit420、第一帧、QP100/P声明，baseline/coeff-rd合计6次编码。首帧实际为key，此补充只验证native空间尺度，不冒称运动链或完整片效果。
- 抽9个配置检查新程序默认 `coeff-opt off` 与第三轮最终程序流/recon/原始RD统计一致。主要效果模式为pruned，工具均adaptive intra+partition。

素材在外置B目录以新Y4M保存并绑定SHA，来源记录指向只读媒体。新工具 `tools/m6_r4_validate.py` 同时冻结程序SHA。基线指定第三轮最终程序，候选开启隔离 `--coeff-opt rd`，不改变旧默认值或冻结长测。

## 验证方法

逐项记录完整AV2字节、三平面SSE/PSNR、归一加权失真 `D=(4*SSE_Y+w*(SSE_U+SSE_V))/2^(2*(depth-8))`，420的w=4、444的w=1。这里是全可见样本的归一加权SSE，允许非整数，不冒称逐SB舍入之后的生产J。RD D/R/J/赢家记录与独立coeff trials/accepts另行保留。

每条流均由固定AVM解码，全部可见样本须与内部重建逐字节一致；成功后删除大raw/decoded，只保留指纹、码流、日志和质量结果。并列bytes与D，不能仅因码量降低或局部J下降就判定率失真收益。四QP满足点数门槛后，只有PSNR-Y与bytes严格单调且有共同质量区间才计算PCHIP log-rate BD-rate，否则明确N/A；PSNR-Y BD不等于VMAF BD或通用视觉质量结论。

B仅运行一个编码子进程，单项timeout240秒。每个编码/解码前重读冻结总控，`isolated-speed`出现时终止后续计算，不停止或修改原长测。当前读到real-quality；本轮新程序数据不能替代旧冻结长测结果。

## V1结果（否决证据保留）

51个baseline/coeff-rd配对（102次效果编码）全部AVM逐样本一致，9条默认coeff-off额外编码与baseline的流/recon/旧RD统计全部一致。全部编码单进程顺序运行，102次效果编码累计约75.31秒，未当作隔离速度测试。coeff统计共10350次trial、2028次accept；51对中13条流相同，38条流变小，其中37条归一加权D变大，仅一条同时减小字节和D。

9条兼容抽查覆盖三格式及合成/真实小图与AI/P，使用pruned路径；不是重新执行旧54条全面矩阵，也不声称其覆盖全部旧RD模式或API。公开API和其它平台机制验收归00/A。

在00追加授权下，使用固定FFmpeg/模型给三片native单帧共6条和真实small P四QP共24条评分，30项VMAF全部完成。评分前将AVM输出的 `F0:0` Y4M头改为与源相同的24000/1001时间基，像素及帧序列不变；逐项VMAF帧数必须严格等于1或3。首次尝试发现F0:0导致framesync计数错误并拒绝结果，修正后重跑全部30条评分，最终结果只采用修正后的匹配帧数数据。

### 真实small P四点曲线

| 来源 | PSNR-Y BD-rate | VMAF BD-rate |
|---|---:|---:|
| 2D-small3 | +34.005% | +48.085% |
| 3D-small3 | +0.232% | +6.391% |
| Real-small3 | +43.301% | +96.944% |

正数表示相同质量区间的估计码率**变差**。这些四点跨很宽质量区间，部分最低QP点VMAF接近0，不能把上述幅度推广到常用质量区间或完整1080p视频；但三条真实小图P曲线均不支持推广当前粗粒度清零。固定点也能看出损失：Real-small P QP100从6017降到4469字节，VMAF从91.936降到83.189；QP140从1721降到1110字节，VMAF70.710降到30.487。不能把字节下降直接当收益。

进一步穷查冻结旧点对新点的直接支配：三条真实small P VMAF曲线，以及三源AI/P PSNR-Y曲线，都没有找到“旧实测点同时字节不大且质量不低”的直接支配对。不能编造不经插值的支配证据。但这不表示候选有效，也不能把退步仅归咎于点数：当前明确观察到37个配对以加权D上升换码量下降，三条真实P VMAF四点同质曲线都变差，native同QP亦有质量下降。结论是当前粗清零缺乏可推广收益，精确退化幅度和常用质量区间仍需谨慎表述。

### 原分辨率首帧QP100

| 来源 | baseline→candidate字节 | baseline→candidate VMAF |
|---|---|---|
| 2D | 30497→29131 | 91.997487→91.007519 |
| 3D | 161473→157092 | 92.940361→91.478073 |
| Real | 76457→64105 | 89.627427→82.952079 |

这三对只有一个QP/首帧，不能计算native BD，也不能推断后续P链；它们说明实际码量减少伴随可见质量损失，尤其Real。没有按结果删样本或改QP。

### 独立合成曲线

PSNR-Y共12条四点曲线中9条可计算，8条为正；synthetic-motion/P为−8.230%，是该固定样例的局部收益。synthetic-edges/P、synthetic-noise AI/P三条因非严格单调N/A。synthetic-edges/P QP100存在一个实测支配点：2031→1409字节，D9755492→8932284；它不能覆盖其它样例和质量点的退化。

因此，该候选完成了真实编码决策改变及多QP审计，但**尚未满足稳健率失真改进门槛**。建议保留实验默认off；若继续研究，另立更细的level/EOB操作候选，仍在同SB入状态比较完整重建及语法R，先跑相同冻结样本。不要通过按三片调lambda、删坏点或只报接受率来掩盖结果。是否保留实验代码由00裁决，本报告没有自行改生产策略。

## 证据

外置目录 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r4-20261018/b`：

- `result.json`：51配对、9兼容项、可见D/PSNR及完整trace，SHA256 `a6b1c345bc3639b2f572eeed8ca4ae5f96a5fea43f4bf943fcf9a6071fc10bd4`。
- `vmaf-result.json`：30评分点、3条VMAF曲线，SHA256 `c765824683bb9d73fa9c529c14df8513ff0807e8423f0e6ca0eb8c87f5e32b4a`，绑定上项结果和输入manifest/依赖。
- baseline第三轮最终程序SHA256 `4d878053eef9de20a5686d090b077d9328526f180ee9adbdeda614e276b36cb3`，candidate第四轮程序SHA256 `ed9731dbb5f147fd1770cc92d8522ffc170fdd07d02f3af8cd0e3a91f6c55ba0`；固定AVM SHA256 `110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac`。
- `v1-evidence/`：独立复制冻结上述manifest/result/VMAF结果及baseline/candidate/decoder实际程序；`verification.json`重核程序、102条保留流、9个源与FFmpeg/模型依赖指纹，并记录直接支配搜索的空结果。V2必须新目录保存，不覆盖本轮矩阵和程序。

新工具 `tools/m6_r4_validate.py`、`tools/m6_r4_vmaf.py` 均Python语法编译通过。长测、媒体和共享依赖只读；本轮证据不替代完整长测独立终验，不宣称M6 DONE。

## V2唯一同集复验

V1完成后00冻结V2合同：只对各叶/各平面normative scan最后一个非零AC降低一级幅值，不动DC，每个平面只试一次，最多12次完整path试探。B没有更改工具语义、源、QP、lambda或模型，只将原manifest逐字节复制到新 `b-v2` 目录；SHA仍为 `012fd7b4eaf6f86abb27836253374e9885e122a0a628a0559fe73c162a1b67eb`。

唯一复验51配对、9默认off抽查、30VMAF全部PASS，102条效果流均固定AVM逐样本一致，baseline/candidate各记录147帧RD统计。V2的全部51个baseline流/recon/原始trace与V1逐项完全相同。102次效果编码累计79.06秒，仅为同期负载下用时，不作隔离速度收益。

V2共15273次trial、11514次accept。51对中48条码量减小、2条相同、1条增大；43条weighted D增加、8条减小，7条在bytes与weighted D上同时不劣且至少一项更好（均独立合成源）。因此接受率仍不是收益结论。

| 真实small/P来源 | PSNR-Y BD-rate | VMAF BD-rate |
|---|---:|---:|
| 2D | −0.210% | −0.481% |
| 3D | −0.819% | −1.585% |
| Real | +0.655% | −1.988% |

冻结PCHIP得到三条VMAF曲线负值，但不能据此称三类都稳健改善。00使用相同30个VMAF点，以独立分段线性log-rate积分交叉核查，2D为+1.4002%、3D为−1.4007%、Real为−2.9710%；这不替换冻结PCHIP结果，而是揭示2D四点BD符号对插值敏感。3D/Real在两种积分方法下方向一致，仍限于这些短小样本。12条PSNR-Y曲线中8负、3正、1条非单调N/A；synthetic-edges/AI为+31.135%，Real/AI为+0.191%，均如实保留。四点覆盖宽质量区间，不把这些幅度外推到完整片、常用高质量区间或AV1对照。

同时存在无需插值的真实small/P同QP直接支配点：

| 来源/QP | baseline→V2字节 | baseline→V2 VMAF |
|---|---|---|
| 2D/100 | 8971→8876 | 91.946371→92.045179 |
| 2D/140 | 2818→2786 | 78.491046→78.928917 |
| 3D/80 | 16591→16569 | 95.482160→95.579533 |
| 3D/100 | 11414→11346 | 92.703582→92.758091 |
| Real/80 | 10292→10161 | 94.950708→95.024424 |

Real/220还以相同213字节将VMAF1.686362提高到3.535448，但该低分点不代表可用视觉质量。V2研究价值不只来自BD插值；这些点直接证明在特定冻结样例上的微小改进。2D两个直接实测支配点不受上述插值敏感性影响，也不因此证明整条2D曲线稳健获益。

| native首帧QP100 | baseline→V2字节 | baseline→V2 VMAF |
|---|---|---|
| 2D | 30497→29339 | 91.997487→91.934775 |
| 3D | 161473→158271 | 92.940361→92.917809 |
| Real | 76457→74038 | 89.627427→89.609819 |

原分辨率单点是少量质量损失换取码量减少，不叫严格等质收益，也不足以做native BD。V2可保留为默认off的隔离研究候选，进一步验证与最终选择由00决定；本轮不再增加样本、QP或第三个策略轮次。

V2证据：`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r4-20261018/b-v2/evidence/verification.json`，已独立复制程序/manifest/结果/工具并重核102条流、9源及VMAF依赖。`result.json` SHA256 `baaa723ff18d83297cab2666beaa00f1a0411307b98378ab5a5b0a713622e75c`；`vmaf-result.json` SHA256 `a2acd77c9a28adb8af393da936631975d7fbec23e8e2cd03041206f34fa64bb9`；V2程序SHA256 `1a24414435baa84a77d9620fad70bc269f4bb6f6215969d409f8ffb5f75da2d4`。V1全部目录与否决结果保持原状。

</details>


<details>
<summary>026 · M6-r5-B-validation.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r5-B-validation.md`  
原文件 SHA-256：`28618fb1e53c7386c5d3cda860a92c4329d18ca72eae440a65683d30c8723da5`

# 基础能力 R5：B 独立native质量与配对性能协议

2026-10-18，00+A/B，当前 `READY_FOR_INTEGRATION/质量与独立性能通过`。用户要求先补基础编码能力，再评估AVM的新技术；本轮首项是多个整数MV进入真实RD。三类固定1.001秒片段相对旧pruned RD基线取得有限改善，**不代表普遍优于M5的RD-off SAD路径**；跨profile对照显示2D/Real仍退步，RD基础效果问题尚未解决。当前AV1差距依然明显，不能外推完整片或编码标准总体优劣。正式仍 `0.0.5-m5`，隔离实验不代表M6发布。

## 样本与参数事前冻结

B在生成素材前向00声明，00批准统一取三类local原分辨率Y4M的0基索引48..71：最接近2秒的起始索引 `round(2*24000/1001)=48`，归一时间2.002秒；连续24帧、时长1.001秒，不看内容或编码结果挑段。三类均1920×1080、10bit420、24000/1001 fps，不缩放、不dup/drop，不扩大到48帧。

已生成 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r5-20261018/b/corpus/manifest.json`，SHA256 `3556e88fa1a2ddea6692fd35bbf8f5e2f4ad39ca4398fb1944cfa6e802f0512d`。每源记录原H264冻结路径/SHA、完整local Y4M实际SHA、源总帧数、24个精确索引/原始payload偏移/逐帧payload SHA及提取后SHA。完整local三源重新顺序哈希均匹配冻结provenance；提取后独立重开原源与新文件，72帧逐一字节比较全部PASS。新媒体约448MB，全部外置；原片和local媒体只读。

事前冻结QP80/100/120/140、low-delay-p、adaptive intra/partition、pruned、coeff-opt off。baseline为R4最终WindowsV2程序，SHA256 `1a24414435baa84a77d9620fad70bc269f4bb6f6215969d409f8ffb5f75da2d4`，默认off与R3兼容已由前轮验证。candidate开启独立 `--motion-opt rd`，与coeff优化互斥，算法范围以00的 `docs/basic-capability-contract.md`为准；不调lambda。

## 质量矩阵

三源×四QP，共12个baseline/candidate配对、24次编码，全部使用相同24帧序列。首对先顺序执行并报告baseline估时，再按00明确授权最多2个子进程并行。编码/AVM解码/VMAF同样受两个工作槽限制，不再叠加其它子任务。

`tools/m6_r5_quality.py`逐帧流式计算三平面SSE/PSNR及归一加权D，避免把24帧全部转为int64同时堆入内存；固定AVM全部可见样本须与内部重建一致。VMAF以raw recon显式声明源fps、同源帧序列评分，要求恰好24帧。完整流/质量/trace/指纹保留，成功后清理大raw/decoded。四点严格单调并有共同质量区间才报告PSNR/VMAF BD，其余N/A；配对完整bytes与画质同时报告，不以局部J下降替代等质收益。

每个子命令timeout1800秒、每次矩阵运行全局6小时预算；超时停止并报告，不静默减少帧数或删点。首对支持独立预检，其PASS可在程序/源/配置和保留流指纹均匹配时复用。质量阶段耗时包含recon导出及后台争用，不作为性能结论。

上一轮native首帧baseline约6秒、冻结完整2D pruned约12.1秒/帧，均非隔离。因此24帧可能每条约144～290秒甚至更长；24条编码串行约1～2小时，新候选开销另计。此为启动前预算，不是R5实际速度。48帧近似加倍，不扩展。

## 独立性能窗口（原ABBAAB方案，正式计时前已修订）

`tools/m6_r5_speed.py`采用相同24帧源、QP100，单进程严格顺序 `ABBAAB`，即AB/BA/AB三对、baseline和candidate各三次，每源6次、三源18次。顺序在结果产生前冻结；报告每对B/A比、三对中位数及最小/最大，不只挑最快一次。

速度仅CLI输入读取和码流输出，**不导出debug recon，不运行AVM或VMAF**；不能直接和旧含recon导出的时间表比较。用GetProcessTimes记录进程CPU秒、墙时/fps，用GetSystemTimes约0.5秒采样整机CPU及扣除本进程份额后的背景负载估计，记录logical CPU数与长测stage。变体三次码流须各自确定一致，保留程序/输入/命令/流指纹。

性能窗口开始前通知00/A暂停所有新增构建、质量和其它计算；原冻结长测继续正常跑。因此该窗口只隔离本轮新增工作，**不具备严格整机隔离条件**，后台负载及波动必须公开，不称纯内核单路性能。每次子命令1800秒、窗口全局6小时预算。

首次正式速度数据产生前，00已锁定共同Latin窗口替代R5/R6两套ABBAAB：每源ABC/BCA/CAB三轮，A=R4 motion-off、B=R5 rd、C=R6 wide，各三次鲜测，共27run，减少两套方案重复测B的9次编码；每源每版另两帧暖机不计入统计。每轮保留B/A、C/B、C/A的CPU/墙时全部值、中位数与spread。三版在同轮顺序测量，并非同时测量，原长测仍在后台。此修订事前完成，不依据任何正式速度结果改顺序；原corpus manifest和已冻结R5工具不修改，原ABBAAB工具仅保留历史，不启动。

实际待用工具为 `tools/m6_r6_joint_speed.py`，新协议 `optimization-r6-20261018/b/joint-protocol.json` SHA256 `d1399fdb3b5e71811d5ab4d529d5df67550434c562ed3952fc1febf1aa0494c0`，绑定原corpus、三个程序和合同副本。必须等R5/R6质量均完整PASS、00独立计算完成并明确通知后才开始共同速度窗口。

所有新增编码/解码/评分命令前及运行中定期查campaign stage；若进入原长测 `isolated-speed`，终止本工具拥有的子进程并停止后续任务，不碰原长测进程。

## 实际质量结果

完整12/12配对、24次编码全部PASS，固定AVM逐帧/逐样本与内部重建一致，24条VMAF评分均恰好24帧。逐项重新核验流字节/SHA、3个源SHA、程序/脚本/评分依赖与统计覆盖，通过。启用motion-rd时共441445次trial、58837次accept；每个key首帧motion trial/accept为0。接受率不作为压缩收益证据。

| 来源 | VMAF BD：冻结PCHIP | VMAF BD：00独立分段线性 | PSNR-Y BD：PCHIP | PSNR-Y BD：独立线性 |
|---|---:|---:|---:|---:|
| 2D | −6.892% | −6.330% | −15.511% | −14.979% |
| 3D | −2.018% | −1.798% | −1.635% | −1.744% |
| Real | −2.684% | −2.343% | −2.498% | −2.699% |

两种插值/积分方法同向，支持这三段固定原生短片上的有限率失真改善；不因R4曾有插值敏感性而隐藏本轮交叉检查，也不把幅度当完整原片收益。共同VMAF区间分别81.078498..95.137246、80.537059..95.859782、82.633241..95.862440。

存在直接实测支配点，例如2D/Q100从593811降到562387字节、VMAF92.893466升到92.948348；Q120从307667降到286892字节、88.849347升到88.962265；Real/Q100从1059846降到1051541字节、92.728763升到92.787776。并非每点都更优：2D/Q80和Q140及3D/Q100/Q140存在小量VMAF下降，所有固定点原样保留。

对Real/Q100和Q120曾观察到开放输出文件长度为0、CPU仍活跃；没有据此误判卡死或放宽超时，两条baseline分别300.17/294.6秒正常完成24帧。单次文件长度不能可靠定位全部时间是否发生在首帧，本报告不据此作热点归因。

00另对24条流/源/程序/脚本/评分、SSE重算PSNR及RD帧覆盖独立审计PASS，并对6条native流fresh AVM raw SHA抽查PASS，证据 `optimization-r5-20261018/root/native-quality-audit.json`。B证据 `b/evidence/verification.json`，绑定完整质量result SHA256 `c8ae51982ca717c4a3b1be60145634fecf047a6a5ca0272edd44b6b4e49ae220`，包含3源/24保留流与逐帧motion统计验证；baseline/candidate实际程序已独立复制冻结。

## 与RD-off SAD路径的中期对照

上述R5负BD是相对R4 **pruned RD、motion-off、固定CDF** 的改进，不能把实验基线简称M5然后声称普遍改进。00随后用同一native24/四QP的R7 baseline（RD-off、motion/coeff/entropy off，即旧SAD决策与固定概率）直接计算跨profile对照：

| 来源 | R4 pruned相对RD-off的VMAF BD | R5 motion-rd相对RD-off的VMAF BD |
|---|---:|---:|
| 2D | +10.284% | +2.440% |
| 3D | −4.173% | −5.961% |
| Real | +28.703% | +25.428% |

2D/Real的PSNR-Y亦较差。R5修正了旧RD的一部分损失，但没有使RD在三类片段上普遍优于SAD，不能称基础能力整体已解决。这是实测效果问题，不凭不同参考/邻居链下的单点就判定某段代码有bug。各跨profile积分共同区间不同，00直接重算而未将不同BD百分比相乘。中期证据 `optimization-r7-20261018/root/r5-r7-cross-profile-interim.json`；最终待R6齐后统一PCHIP/分段线性交叉核查。R7的同像素纯熵收益另有直接证明，不依赖本RD结论。

## 同一原生短片的当前AV1差距

00另外完成同一native24的fresh SVT-AV1 p6/CRF28/32/36/40/44/48、lp1/threads1共18点；B核对18项canonical SHA均与本corpus一致，全部24帧，无缩放/抽帧。不能把旧15秒8倍直接套到本1秒。

以下是QP100的直接实测支配比较，AV1质量更高，故无需假设QP等于CRF；它不是精确等质码率比：

| 来源 | R5 motion-rd字节/VMAF | 质量更高的fresh AV1 | AV1净字节/VMAF | R5体积倍数 |
|---|---|---|---|---:|
| 2D | 562387 / 92.948348 | CRF48 | 44321 / 93.399432 | 12.689 |
| 3D | 393152 / 93.705089 | CRF48 | 157276 / 94.982864 | 2.500 |
| Real | 1051541 / 92.787776 | CRF28 | 237116 / 93.428986 | 4.435 |

AV2为完整elementary stream，AV1为视频packet净量；AV1是RA，AV2为单参考低延迟P，不量化结构差异的贡献，也不借它淡化当前实现差距。00的QP100 OBU审计显示2D key仅占12.4%、Real占7.2%，不能把高码量主要归于1秒片段起始key开销。完整证据 `b/same-native-av1-gap.json` 与 `root/native-frame-byte-audit.json`。本项只比较字节/质量，不拿AV1单次编码时间与含recon的质量时间做公平速度比。

## 当前性能状态

素材提取与指纹验证PASS；三项新工具Python语法编译通过，Windows CPU计时/系统计时API可访问，并用空Python进程验证（未编码）。准备时B单次观察可用物理内存约15.22GiB、系统CPU20.67%，仅时点，不保证后续负载。

首对2D/Q80曾顺序预检，baseline编码218.87秒，candidate263.84秒，均含recon导出，非速度结论；随后复用PASS，以jobs2完成原12pair，没有改点。候选实际冻结到 `b/programs/candidate.exe`，SHA256 `d6e2c1c1f9ce20d057b177eb9f66b15f0d6561d5421410bb8db471678b6fd705`。共同速度尚未启动，不能把上述质量时间比当实测性能收益或代价。

## 独立速度完整结果

00明确GO后，joint窗口27次正式run+9次2帧暖机全部PASS，顺序为每源ABC/BCA/CAB，jobs1。A=R4 motion-off，B=R5 rd，C=R6 wide。测量仅输入读取和码流输出，不导出recon、不跑AVM/VMAF。00/A暂停新增计算，原冻结长测继续；所有采样stage为real-quality，故不是严格整机隔离。每条命令hash、流SHA、墙时、进程CPU、约0.5秒系统/本进程/背景估计负载全保留。

总result SHA256 `7e0854177a1c8c7794a37c1fa1dcff9f701d2f98c951563bcdd9be22d49271ea`，原始与冻结副本在 `optimization-r6-20261018/b/joint-speed/result.json` 和 `b/evidence/joint-speed-result.json`。下表为三轮比值中位数[最小,最大]，大于1表示耗时增加；完整每轮值见原始JSON，不复用质量任务时间。

| 源 | B/A墙时 | B/A CPU |
|---|---:|---:|
| 2D | 1.1733 [1.1682, 1.1990] | 1.1708 [1.1163, 1.2147] |
| 3D | 1.2998 [1.2953, 1.3033] | 1.2795 [1.2464, 1.2944] |
| Real | 1.1725 [1.1629, 1.1768] | 1.1904 [1.1728, 1.1964] |

R5在三源相对旧pruned基线的完整曲线改善需要墙时中位约17.3%/30.0%/17.2%的代价；这不改变其相对RD-off SAD在2D/Real仍落后的跨profile结论。背景CPU与CPU比值spread限制了细小速度差异的解释。

00独立速度审计PASS：`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r7-20261018/root/motion-speed-audit.json`。独立重算全部三轮配对/中位数/范围，27条正式流SHA/字节均匹配既有QP100质量流，程序/脚本/合同/协议指纹一致。最终完整性见同目录 `final-integrity.json`：192源及原长测76冻结文件/配置重核PASS。未新增计算。

</details>


<details>
<summary>027 · M6-r6-B-validation.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r6-B-validation.md`  
原文件 SHA-256：`2852840a1cccd1870548b196c2641702d097b0434940b7c493bd69b22a0ffc95`

# 基础能力 R6：B 有界wide运动搜索验证协议

2026-10-18，当前 `质量与独立速度PASS`。R6补充基础运动搜索能力，不增加AVM新工具族。算法合同与机制验证由00/A负责，本报告只记录独立效果/性能口径。默认off与正式 `0.0.5-m5` 不变。

## 事前固定的比较

输入完全复用R5已冻结三类原1080p、10bit420、索引48..71的24连续帧，manifest SHA256 `3556e88fa1a2ddea6692fd35bbf8f5e2f4ad39ca4398fb1944cfa6e802f0512d`。QP仍80/100/120/140、low-delay-P、adaptive intra/partition、pruned、coeff-opt off，不改源/帧/模型/lambda。

- A：R5已冻结程序，显式 `--motion-opt rd`。
- B：R6新程序，显式 `--motion-opt wide`。

R6程序已复制到 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r6-20261018/b/programs/candidate.exe`，SHA256 `c76e193b26be5a300684df3ff1e88862facbad85eb8be209c78d05b1c62bed2f`。R5完整12pair已PASS后启动R6；基线总result SHA256为 `c8ae51982ca717c4a3b1be60145634fecf047a6a5ca0272edd44b6b4e49ae220`，已作为稳定基线冻结。

## 质量

新工具 `tools/m6_r6_quality.py` 必须先读到R5质量总结果PASS、完整12pair覆盖、与原manifest hash一致，并逐条核验R5候选case result、保留流SHA和其质量/trace等于总结果。随后将R5候选质量和流作为R6 baseline重命名引用，冻结稳定R5总result与12项case hash；不重编码12条baseline，不把中间RUNNING结果当基线。

仅新增12条wide候选编码，最多2工作槽，编码/AVM/VMAF不叠加其它工作池。复用R5已冻结的逐帧可见D/PSNR与AVM比较函数，同一fps/模型的24帧VMAF，完整bytes与所有点如实保留。每条1800秒、全局6小时预算；四QP严格单调且共同质量区间存在才计算BD，否则N/A。局部J不劣与shell接收机制不代替多QP效果，也不承诺wide一定有效。

## 独立速度（首次计时前共同窗口修订）

原工具 `tools/m6_r6_speed.py` **显式给A传rd、给B传wide**，修正了不能直接复用R5默认off baseline的问题，但尚未执行。00在首次正式计时前为减少中间版本的重复鲜测，将两套ABBAAB修订为一个共同窗口；原工具保留历史，不改其语义或数据。

新 `tools/m6_r6_joint_speed.py` 采用A=R4 motion-off、B=R5 rd、C=R6 wide，每源ABC/BCA/CAB三轮，各版三次鲜测，三源共27run；每源每版先两帧暖机不计入统计。每轮记录B/A、C/B、C/A墙时及CPU比值，报告三轮中位数、全部值和min/max，保留逐次后台负载。共用的是同一窗口鲜测的B，不复用其它背景条件的旧时间；每轮顺序测试，不冒称同时测量。全流程只码流，不导出recon、不跑AVM/VMAF。

独立协议 `optimization-r6-20261018/b/joint-protocol.json` SHA256 `d1399fdb3b5e71811d5ab4d529d5df67550434c562ed3952fc1febf1aa0494c0`，绑定原corpus/三个程序/共同合同副本。工具强制完整R5/R6质量PASS，绑定计时measure脚本及协议hash；正式启动仍等待00独立验收完成后的明确通知。没有更改原质量样本或QP，也未产生正式速度数据后再改顺序。

开始速度前通知00/A暂停所有新增编译/质量/计算。原冻结长测保持正常运行，因此不具备严格整机隔离，必须如实报告后台负载/波动。与R5质量及R5速度窗口不重叠。

两项新工具Python语法编译通过；引用R5不变的底层函数且记录脚本依赖hash。没有修改已冻结R5工具或矩阵，也未新增源/QP。R6质量12/12已PASS；00最终审计与三源fresh AVM复核通过后，已明确GO，独立速度正在执行。

## 固定12点结果

新增12条wide编码、AVM逐样本重建和24帧VMAF全部PASS；基线复用完整R5候选，未删点或调整源/QP/lambda。总result SHA256 `00b78e58d0d4f23361529bf4e71d6d3ae1740b79d169b1811f479d8aebaf3134`，config SHA256 `a8e3a7bdbae239493a8d4918a451c361a44e120b916826ce16120653633c3a44`。已将result/config/manifest复制到 `optimization-r6-20261018/b/evidence`。

| 源 | VMAF BD-rate | PSNR-Y BD-rate |
|---|---:|---:|
| 2D | −0.079439% | −0.313748% |
| 3D | −0.072969% | −0.007229% |
| Real | −0.118122% | −1.118874% |

这里是固定四点共同质量区间的log-rate PCHIP积分。VMAF收益很小，不能称wide带来实质普遍压缩改善；收益/代价还需独立速度。Real QP140出现直接退步：177776→178091字节，VMAF82.481095→82.424364。每点如下（A=R5 rd，B=R6 wide）：

| 源/QP | A字节 | B字节 | A VMAF | B VMAF |
|---|---:|---:|---:|---:|
| 2D/80 | 1162634 | 1161620 | 95.137246 | 95.133441 |
| 2D/100 | 562387 | 562387 | 92.948348 | 92.951499 |
| 2D/120 | 286892 | 286791 | 88.962265 | 88.972907 |
| 2D/140 | 147469 | 147518 | 81.003168 | 80.982919 |
| 3D/80 | 788415 | 787866 | 95.874503 | 95.874056 |
| 3D/100 | 393152 | 392769 | 93.705089 | 93.712054 |
| 3D/120 | 211047 | 210831 | 89.349311 | 89.346977 |
| 3D/140 | 118432 | 118538 | 80.439541 | 80.453402 |
| Real/80 | 2486834 | 2487580 | 95.877166 | 95.883232 |
| Real/100 | 1051541 | 1051863 | 92.787776 | 92.799033 |
| Real/120 | 414877 | 414122 | 89.091111 | 89.098323 |
| Real/140 | 177776 | 178091 | 82.481095 | 82.424364 |
00已独立核验源/程序/脚本/流SHA、24帧指标、SSE重算PSNR全部PASS，三源QP100 fresh固定AVM raw SHA均匹配，见 `optimization-r6-20261018/root/native-quality-audit.json` 与 `native-avm-spotcheck`。R6改善仅相对R5 rd；R5/R6与RD-off策略的跨profile比较仍须单独报告，不能由同入口保M5候选的单SB机制推得多帧曲线不劣。

2026-10-18 11:46收到00明确GO后启动共同窗口，输出 `optimization-r6-20261018/b/joint-speed`。随后才运行 `optimization-r7-20261018/b/speed` 的RD-off熵配对。计时期间00/A暂停新增计算，原长测继续，故仍非严格整机隔离。
## 独立速度完整结果

00明确GO后，joint窗口27次正式run+9次2帧暖机全部PASS，顺序为每源ABC/BCA/CAB，jobs1。A=R4 motion-off，B=R5 rd，C=R6 wide。测量仅输入读取和码流输出，不导出recon、不跑AVM/VMAF。00/A暂停新增计算，原冻结长测继续；所有采样stage为real-quality，故不是严格整机隔离。每条命令hash、流SHA、墙时、进程CPU、约0.5秒系统/本进程/背景估计负载全保留。

总result SHA256 `7e0854177a1c8c7794a37c1fa1dcff9f701d2f98c951563bcdd9be22d49271ea`，原始与冻结副本在 `optimization-r6-20261018/b/joint-speed/result.json` 和 `b/evidence/joint-speed-result.json`。下表为三轮比值中位数[最小,最大]，大于1表示耗时增加；完整每轮值见原始JSON，不复用质量任务时间。

| 源 | C/B墙时 | C/B CPU | C/A墙时 | C/A CPU |
|---|---:|---:|---:|---:|
| 2D | 1.0531 [1.0337, 1.0710] | 1.0307 [0.9480, 1.0621] | 1.2395 [1.2356, 1.2511] | 1.1856 [1.1100, 1.2521] |
| 3D | 1.0790 [1.0764, 1.0894] | 1.0887 [1.0741, 1.1132] | 1.4062 [1.3991, 1.4111] | 1.3904 [1.3876, 1.3930] |
| Real | 1.0187 [1.0165, 1.0191] | 1.0133 [0.9045, 1.0213] | 1.1918 [1.1852, 1.1988] | 1.2122 [1.0608, 1.2158] |

三源全部三轮墙时C/B均大于1；约5.3%/7.9%/1.9%的额外墙时仅对应约0.08%/0.07%/0.12%的VMAF BD收益，当前证据不支持将wide当作默认压缩效率升级。2D/Real个别CPU配对波动较大，原始值与背景采样全部保留。

00独立速度审计PASS：`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r7-20261018/root/motion-speed-audit.json`。独立重算全部三轮配对/中位数/范围，27条正式流SHA/字节均匹配既有QP100质量流，程序/脚本/合同/协议指纹一致。最终完整性见同目录 `final-integrity.json`：192源及原长测76冻结文件/配置重核PASS。未新增计算。

</details>


<details>
<summary>028 · M6-r7-B-validation.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-r7-B-validation.md`  
原文件 SHA-256：`f506e80b3f07f158c15ddc0ceed445e379db5c1a2c31273ae6056a94db4272ad`

# 基础能力 R7：纯熵适应的独立验证协议

2026-10-18，当前 `READY_FOR_INTEGRATION/质量与独立性能通过`。12个固定native配对全部同重建，纯熵编码完整字节减少6.989%～20.029%。范围以 `docs/basic-entropy-contract.md`为准：frame内CDF适应，仅RD-off；固定模式/MV/qcoeff/recon，不组合R5/R6运动RD，也不调整训练offset或lambda。正式仍 `0.0.5-m5`，不称基础阶段全部完成。

使用同一事前冻结native corpus：三源1920×1080/10bit420、索引48..71连续24帧/1.001秒，原manifest SHA256 `3556e88fa1a2ddea6692fd35bbf8f5e2f4ad39ca4398fb1944cfa6e802f0512d`。QP80/100/120/140、P、adaptive intra/partition，RD/coeff/motion全off。baseline为冻结R6程序，candidate相同配置加 `--entropy-adapt on`，12配对/24编码；R6旧CLI没有entropy开关，baseline依赖其原始默认off，不传未知参数。

`tools/m6_r7_quality.py`复用不变的R5逐帧质量/AVM验证函数，先独立首对估时，再最多jobs2完整矩阵。每条baseline和candidate都要固定AVM逐样本一致；candidate recon SHA及三平面SSE/PSNR必须严格等于同QP baseline，任何变化直接失败。baseline每点评分一次VMAF，candidate只有确认像素完全相同后才复用该值，并标记 `quality_reused_from_baseline=true`，不冒称新评分或视觉质量改善。相同像素下直接报告完整字节差和百分比，不依赖BD插值。

原完整2D off约600秒/383帧仅提供预算量级，24帧可能约35～40秒/次，24次编码串行约14～16分钟、jobs2约7～10分钟；适应实现和背景负载另计。这是事前估算，不是R7实测速率。若R5完成时R7尚未稳定，则先执行R6质量，不等待本项阻塞。

`tools/m6_r7_speed.py`另做RD-off A/B单路鲜测，每源各两帧暖机后ABBAAB三对，三源18次实测。只码流输出，无debug recon/AVM/VMAF，不拿此时间与RD-on motion joint窗口直接相除。记录进程CPU/墙时/背景负载、三对全值/中位数/范围及程序/命令/输入指纹。开始前需00明确调度并暂停所有新增计算；旧长测仍运行，不称严格整机隔离。

每命令1800秒、质量及速度各自全局6小时预算；原campaign进入isolated-speed时停止自己的子进程，不操作原长测。R5/R6冻结工具、程序、配置和结果不改；R7所有输出在新 `optimization-r7-20261018/b`。两项新工具已通过Python语法编译。

00完成生产稳定验收后，B复制固定candidate程序到 `b/programs/candidate.exe`，SHA256 `3d9b45c047edd3682c158426827e1ed10840a4e72aa3cde6b9f52394550e8dd9`；合同副本 `b/basic-entropy-contract.md` SHA256 `c0a324b59ac16a8f181fe8cd418eb24bad1b1015df5217d0be2a5661d41038ff`。按00调度，R5完整PASS后完成R7首对及全质量，再启动R6质量，没有让新增编码超过2槽。速度窗口另等明确启动通知。

## 原生24帧实测

首对2D/QP80：baseline编码25.97秒、candidate29.65秒，含debug recon导出，**不作为独立性能结论**。确认同recon/AVM后复用首对PASS，jobs2完成全部固定12pair/24流；没有改变QP/源/帧或训练数据。

| 来源/QP | entropy-off完整字节 | entropy-on完整字节 | 同像素字节变化 |
|---|---:|---:|---:|
| 2D/80 | 1296423 | 1185325 | −8.570% |
| 2D/100 | 621002 | 561197 | −9.630% |
| 2D/120 | 328738 | 289053 | −12.072% |
| 2D/140 | 186423 | 153577 | −17.619% |
| 3D/80 | 955243 | 787938 | −17.514% |
| 3D/100 | 437136 | 349581 | −20.029% |
| 3D/120 | 242768 | 196622 | −19.008% |
| 3D/140 | 142545 | 114782 | −19.477% |
| Real/80 | 3508913 | 3225587 | −8.074% |
| Real/100 | 1539063 | 1431502 | −6.989% |
| Real/120 | 569019 | 521057 | −8.429% |
| Real/140 | 236188 | 209218 | −11.419% |

每对baseline/candidate的recon SHA和三平面SSE/PSNR严格同值；每条流的固定AVM解码逐样本等于内部重建。12个baseline VMAF各有恰好24帧；candidate没有新VMAF评分文件，仅在像素验证后复用baseline数值，明确标注复用。这是同质量的直接字节收益，无需依靠BD插值。B外部矩阵验证的是重建；模式/MV/qcoeff不变的内部机制证明由00/A测试另行覆盖。

不能把这些结果写成视觉质量提升，也不能和R5/R6不同RD配置直接做因果或速度相除。RD-on与CDF适应尚未在本首关组合，全部结果仍限于这三段固定1.001秒片段；对当前AV1差距的总贡献须用明确profile曲线比较，不声称解决。

00的中期跨profile审计还显示：以本节entropy-off/RD-off baseline作参照，R5 motion-rd的VMAF BD在2D/Real分别+2.440%/+25.428%，3D为−5.961%。因此R5虽改善旧pruned RD，也不能称比旧SAD路径普遍更好。这里的CDF收益只在同像素严格配对中归因于熵适应；不能用它掩盖RD基础效果问题。跨profile最终结果待R6完整后统一审计，未调lambda或将不同共同区间BD相乘。

## 独立审计与证据

B `b/evidence/verification.json` 重新核验3源、24条流字节/SHA、12对重建/全质量同值、12份baseline评分与候选无重复评分、程序/脚本/模型依赖，全部PASS，且已复制程序/合同/结果/配置。完整result SHA256 `45808f61898a2936db1e041fface175199b3c1369ede2b857f0f2cc0e7120357`，config文件SHA256 `00d1f0948bc6f317f449a8bca4701b45e8599230d8de1db79e39db5634087a66`。

00另有完整独立审计 `optimization-r7-20261018/root/native-quality-audit.json` PASS，并fresh解码三源QP100 entropy-on为raw，3/3 SHA独立等于两端encoder recon，见root/native-avm-spotcheck。独立性能结果见后节，不把本节质量耗时当作性能证据。

## 独立RD-off速度结果

共同motion窗口结束后才启动此独立窗口。三源ABBAAB各6次正式+各版2帧暖机，总18正式+6暖机全部PASS；每次正式QP100流SHA严格匹配对应quality场景/版本，而不只核对重复测量自身。无新增编译/质量/评分并发；原长测保留，仍非严格整机隔离，逐次背景负载全部记录，不与RD-on motion绝对时间相除。

result SHA256 `b85b1cb47af79885eaae2473a4d6a663c68bd413383c679ed36b050a93c146e8`，见 `optimization-r7-20261018/b/speed/result.json` 与 `b/evidence/speed-result.json`。下表是on/off三配对中位[最小,最大]，大于1表示更慢；全部3值/命令hash/流SHA/CPU/墙时/负载采样保留于JSON。

| 源 | on/off墙时 | on/off CPU |
|---|---:|---:|
| 2D | 1.1255 [1.1237, 1.1380] | 1.1208 [1.1095, 1.1389] |
| 3D | 1.1320 [1.1291, 1.1382] | 1.1397 [1.1068, 1.1756] |
| Real | 1.1694 [1.1284, 1.1850] | 1.1196 [1.0327, 1.1752] |

三源全部三配对墙时on更慢，中位增加约12.6%/13.2%/16.9%；CPU中位增加约12.1%/14.0%/12.0%。同像素的6.989%～20.029%码量收益成立，同时存在这些编码时间成本；只适用于已验证RD-off合同，不能隐含推广到RD-on。

00独立速度审计PASS：`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r7-20261018/root/entropy-speed-audit.json`。独立重算全部三配对/中位数/范围，18条正式流SHA/字节均匹配既有QP100质量流，程序/脚本/合同指纹一致。共同motion窗口审计见同目录 `motion-speed-audit.json`；最终 `final-integrity.json` 已重核192源与原长测76冻结文件/配置PASS。未新增计算。

</details>


<details>
<summary>029 · M6-recovery-20261018.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-recovery-20261018.md`  
原文件 SHA-256：`1ed476257322f128f3fcee9e91c138c6f77d43b194c7110c6ca7a707d3deac7d`

# M6 本地r2：系统意外重启后的恢复

> **后续状态覆盖**：本次直接启动的总控11508于08:12:56记录batch18600以状态码1退出，非再次重启；已有完整质量增至65项PASS。现改由Windows计划任务托管，08:24:20新总控17392启动，后续看[M6后台托管报告](M6-background-host-20261018.md)，不能继续按本文旧PID判断存活。

2026-10-18，用户授权检查中断原因、核验已有结果、恢复原测试，然后结束AI会话。本轮不开发编码器，不采用r2/r3优化后程序替换冻结程序，正式版本仍0.0.5-m5。

## 中断原因与边界

Windows记录上次意外关闭为北京时间04:50:53，当前启动时间04:52:25。04:52:28的Kernel-Power 41和04:52:43的EventLog 6008确认非正常关机；同一检查窗口未发现计划重启1074或蓝屏1001记录，事件41的BugcheckCode为0，未发现MEMORY.DMP/Minidump目录。

这些证据支持“系统非正常重启杀死测试进程”，不能进一步确定是断电、系统挂死还是强制重启。未发现编码器报错导致测试停止的证据，也不能凭重启认定编码器无任何潜在问题。原总控PID5284及batch PID5740不再运行；磁盘状态仍显示RUNNING，是进程突然消失后未执行失败落盘，不是真实存活状态。原stderr为空，stdout最后为real-quality START。

## 恢复前核验

恢复证据独立保存在 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/recovery-local-r2-20261018-0752`：原总控status、launch、质量summary、stdout/stderr/质量日志、系统事件JSON/XML，以及preflight.py/json。未删除旧attempt、未清理锁文件；OS文件锁已释放，独占锁实测可获取。

| 项目 | 中断前有效完成数 |
| --- | ---: |
| pilot | 6/6 |
| 小图穷举对照 | 1512/1512 |
| 1080p三帧筛查 | 576/576 |
| 完整质量主曲线 | 64/108 |
| 既有AV1点 | 3/3 |
| fresh AV1点 | 18/18 |

64条完整质量结果中2D为36/36、3D为28/36、Real为0/36，均为PASS，没有FAIL结果。每个已完成AV2任务已独立核验run_id、job、全部保留产物SHA、命令退出码及逐帧RD trace；AV1结果也核验保留产物SHA。前9个阶段原日志SHA与总控完成记录匹配。76项冻结文件和campaign-config SHA全部匹配；Python版本和host platform与已有manifest相同，磁盘剩余约113GiB，足够原临时空间预算。

有3个已建attempt但无result的中断任务：3D/pruned QP232、3D/pruned QP240、3D/M5 QP248。原attempt-0001保留，resume将另建attempt，不能把半成品当PASS。完整质量仍有44项未完成，随后还有real-ai、isolated-speed及审计/报告；未宣称整个M6完成。

## 恢复方式

北京时间07:53:30，以原 `tools/Start-M6Campaign.ps1` 和相同Python启动独立隐藏后台总控，**新PID11508**。配置、17阶段列表、冻结程序/依赖、QP、jobs和timeout均未改。脚本从阶段列表开头复核：素材准备复用已冻结有效转换，完成任务按原run_id与保留SHA复用，审计重新执行；恢复不会原样重复整批编码。输入Y4M及其他阶段依赖仍由原冻结脚本重新校验。

命令：

```powershell
& .\tools\Start-M6Campaign.ps1 -RunRoot <LOCAL_PRIVATE>\ffmpeg-native-m6-tests\campaign-local-r2-20261017 -Python 'C:\Program Files\Python311\python.exe'
```

恢复stdout/stderr分别为原批目录下 `20261018-075330-733.stdout.log` / `.stderr.log`，新launch.json已落盘，旧launch在恢复证据目录。后续以campaign-status.json、对应summary及逐项result为准，不按旧PID或RUNNING文本推断。

北京时间07:55:51确认：前9阶段重新复核全部PASS，总控RUNNING/real-quality；新batch PID18600、run_id仍为 `b34d5148550c72961acf598643d31dfc45438a06634f9ab46577670d83acea00`。3个编码子进程已实际启动，分别为上述3个中断任务的attempt-0002；attempt-0001均保留，恢复stderr为空。不是仅启动总控或停留在素材准备。此前64个PASS结果继续复用，不以恢复启动代替后续44项完成。

单路isolated-speed阶段不得有AI编译/计算负载；确认续跑后结束本会话。批次结束仍需独立审计，脚本PASS不自动发布M6。

</details>


<details>
<summary>030 · M6-review-20261017.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-review-20261017.md`  
原文件 SHA-256：`da0ffa62e29d04c8f33589771269c28f8dfcf11a8d2bf36ace8f23a8c91ed166`

# M6 集中批次验收分析

日期：2026-10-17。结论：**编码正确性、兼容性和RD机制可阶段性验收；真实视频与单路性能阶段未执行，M6总体未完成，正式仍0.0.5-m5。** 本次只核验和分析，未修改编码器、冻结批次或重启长测。

## 停止原因

总控2026-10-17 13:15:58启动，15:42:59在`prepare-real`以非零退出，共运行2小时27分。前19/28阶段通过，第20阶段失败，后8阶段未执行。

失败不是编码器崩溃或码流不合法。冻结ffprobe独立读取`boxing.mp4`为254帧、60fps、4.233333秒，而准备脚本固定要求125帧/25fps即5秒，并禁止循环补帧，故主动抛出`clip is too short for requested profile; no looping/padding frames`。这是选材时长与测试准备合同不匹配、未预检全部源造成的编排缺陷。r1保留FAIL及完整日志，不改写成成功。

Narrator、FoodMarket、Boxing三个源的下载尺寸和SHA全部匹配，排除下载不完整。前两片已各生成125帧720p、25帧pilot及三格式5帧小图；Boxing尚无派生，另六源未下载。real720/pilot/small/calibration四个正式corpus manifest均不存在；旧preflight结果不得替代正式视频验收。

## 已完成部分的独立审计

重新使用冻结工具校验423项批次依赖、配置SHA、19个已完成阶段的日志SHA，以及当前170项开发源码。六个Windows批次逐项复核任务覆盖、保留流/日志/指标hash、固定AVM重建、重复编码、RD统计和BD-rate重算；三个Linux重放逐项比较4320个实际码流、重建hash、日志hash、退出码和逐项JSON，并重新核对三程序SHA。全部通过。

| 项目 | 已完成/计划 | 复核 |
|---|---:|---|
| Windows短验CTest | 45/45 | 此前冻结的短验 |
| GCC / Clang / SAN完整CTest | 各44/44 | 日志核验通过 |
| 主边界矩阵 | 4320/4320 | PASS |
| 非整SB | 288/288 | PASS |
| 全255QP | 3060/3060 | PASS |
| GCC / Clang / SAN码流重放 | 各4320/4320 | 实际流/重建/日志指纹一致 |
| 720p / 1080p | 各54/54 | 固定解码及VMAF通过，素材为3帧合成流 |
| 1000帧长链 | 60/60，每项3次 | 固定解码、VMAF及重复确定性通过 |
| M5/off兼容 | 1080对 | 完整流及重建全部相同 |

共20796项已完成任务、20916次实际编码、242208帧编码（含重复）；固定AVM解码及质量对比覆盖83328帧。任务数量多不等于M6效果验收完整：尚缺的真实视频和速度阶段是独立硬门槛。

机器可读证据：`artifacts/m6-review/partial-verification.json`；复核命令为设置M6 pydeps后运行`python artifacts/m6-review/verify.py`。该摘要明确为PARTIAL VERIFIED，不覆盖总控FAIL。

## RD与剪枝机制结果

主矩阵1080组同输入/参数对照中，exhaustive相对off有480组流及重建发生变化，说明RD真实参与决策；其中182组体积下降、298组体积上升、600组相同。Y MSE为260组改善、18组变差、802组相同。三平面加权D加lambda乘rate的局部目标，不保证每项亮度质量或全序列体积同时改善。

| 指标 | exhaustive | pruned |
|---|---:|---:|
| 候选评估 | 298620 | 90720（减少69.62%） |
| forward调用 | 5634576 | 1006236（减少82.14%） |
| 总字节（该矩阵） | 22498362 | 22498398 |

1080对full/pruned重建全部相同，仅两组各增加18字节。例外位于8bit420/10bit420 chroma场景、QP255/P、两工具开启；143→161字节，涉及4帧winner差异。这些帧D/recon相同，但估算R/J较高，J相对增加24.14%–38.01%。这符合剪枝可能漏掉最低J的设计边界，不能宣称剪枝永远等同穷举或只给总体小字节差隐藏局部漏胜。

以上是合成短矩阵的机制证据。候选/变换计数下降不等于已实测相同幅度的速度提升；本批并发计时不能代替jobs1性能结果。具体记录见`artifacts/m6-review/quality-analysis.json`和`pruned-exceptions.json`。

## 已有画质点能说明什么

同为10bit420合成pan、P、QP100，每项3帧：

| 分辨率 | 模式 | 体积字节 | VMAF | PSNR-Y dB |
|---|---|---:|---:|---:|
| 720p | M5/off | 923297 | 92.046 | 18.697 |
| 720p | pruned | 1146807 | 98.437 | 37.399 |
| 1080p | M5/off | 2088752 | 92.036 | 18.825 |
| 1080p | pruned | 2575896 | 98.439 | 37.439 |

该例质量提高，但体积也分别增加24.21%和23.32%，只能证明当前同QP决策发生质量/码量取舍，不能当作同质量省码或普通视频效果结论。1000帧10bit420 cut/P的pruned在QP40/80/100分别为VMAF97.396/96.652/95.220，但此批仅pruned，不能单靠它宣称优于M5。

本次已生成的**828条BD-rate记录全部N/A**：边界/全QP短样例有非单调或无限PSNR，部分批次未启用VMAF；edge和720p/1080p只有3个QP，不满足至少4点。`missing/failed/infinite quality point`是通用N/A原因文字，此处不能据此认定编码失败；任务逐项均为PASS。long1000只有一个模式，没有BD对照。没有有效BD-rate证据时不发布统一压缩收益百分比。

## 剩余与最小恢复方向

尚未执行：真实pilot及其审计/耗时预算、小图full/pruned质量速度对照及审计、六片测试加两片holdout的真实视频曲线及审计、最终自动图表报告。

恢复应保留r1冻结数据和FAIL，建立r2补充配置，仅接续未完成部分。先预检全部九源时长和实际可解码帧数，再冻结真实素材帧数合同：优先每片在125帧上限内使用真实可用帧数，各编码模式严格使用同片同帧，不循环、不补帧；或者全部源确认足够后统一更短时长。不能只针对Boxing绕过检查后继续假设其余源都够5秒。恢复前核验缓存hash，可复用已通过矩阵和已完成下载；不需要把编码器矩阵全部重跑。

本次没有执行上述修复或恢复。完整M6验收与正式版本冻结仍PENDING，不能进入以M6已完成为前提的下一阶段。

</details>


<details>
<summary>031 · next-A-adaptive-rd-design.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\next-A-adaptive-rd-design.md`  
原文件 SHA-256：`6a40d3ff8c00aa4cfebf6d26e359e21816264bee13b7cb9ee28f28d6a70147ce`

# 自适应CDF进入RD事务：只读设计

2026-10-18。基于已冻结R7源码审计，不实施生产、不编译或编码。R7首关继续保持仅RD-off；本设计尚未实施或验证，不代表该组合已经可用。00最终核验96组流、Win49/GCC48及SAN6项通过；本报告不将其外推为自适应RD已经验证。

## 建议最小路径

沿用R7显式模型ID、初始化和更新内核，以及现有`tile_tree_write`共享语法。先独立实现规范位置调整概率的Q12估计，再让rate session拥有一份committed bank和一份scratch bank。每个SB候选都从同一个committed bank重新复制，完整候选内部按符号顺序逐步更新scratch；仅正式winner commit推进committed。首个公开闭环可先限制`entropy_adaptation+RD`时coeff/motion实验均off，基础exhaustive/pruned及partial全部按原候选规则工作。之后逐项验证coeff、motion-rd、motion-wide组合；不同时改候选、lambda或训练offset。

这不能实现成“候选使用静态R，最后writer才自适应”，也不能让上一个试探候选的尾CDF成为下一个候选的入CDF。每个SB仍是贪心局部RD；选择造成的后续模型变化不在当前SB的J内，因此不保证整帧/整段字节或质量更好。

## 率估计：固定AVM已经给出位置调整方法

当前`entropy_models.c:39`明确拒绝rate_only；`entropy_encoder.c:184-190`用raw forward相邻差并拒绝零差。R7直接更新oracle已观察到大量相等raw边界，不能直接解除这两个拒绝后沿用旧估计器。

新证据是固定`avm/av2/common/cost.c:30-41`：`av2_cost_tokens_from_cdf`逐边界调用`get_adjusted_prob`，再对调整后的inverse边界求差。`avm_dsp/prob.h:249-255`给出该函数；它与实际`od_ec_prob_scale`共享位置padding。在当前CDF_SHIFT=0、EC_PROB_SHIFT=7下，定义：

```
A(p, i, n) = 128 * floor(p / 128) + 8 * prob_inc[n - 2][i]
U = (symbol == 0) ? 32768 : A(inverse[symbol - 1], symbol - 1, n)
V = A(inverse[symbol], symbol, n)
p15 = U - V
R_q12 += existing_probability_cost_q12(p15)
```

inverse含terminal0，最后symbol的padding是该row的0项。R7 reachable N=2..8对应padding项严格下降，所以即使相邻raw相等，合法模型的p15仍正；所有symbol概率望远镜求和为32768。这是规范调整，不是给CDF塞epsilon，也不改实际更新状态。对越界/非单调/不合法模型仍报错，不能静默clamp来隐藏模型错误。

本项目可以保留原Q12整数log实现，不必复制AVM的Q9近似表；独立核验对象首先是每个p15和状态轨迹，不能宣称两种不同近似精度的整数cost逐值相等。literal仍每bit4096，CDF只在模型symbol成功后更新，Rice/quniform旁路不更新。所有冻结off/RD路径继续使用原raw估计器，禁止全局替换导致旧赢家变化。

该R仍是估计bit，不是最终OBU字节长度。实际range coder还有`rng>>8`和乘法取整、renormalization、carry与末尾flush，不能用“同一个padding”证明Q12总和等于实际bytes。首版验收应独立量化二者偏差，但不以固定阈值强行声称精确。符号成本来源正确与编码效果正收益是两道独立验收。

若后续确有必要，可研究携带normalized rng的无输出range状态，计算实际range收缩率；那还要把rng纳入SB快照/commit并处理literal规则，属于另一版本。更不能将每个SB独立finalize后的字节数相加，当作连续整tile真实R；终止开销与carry不能这样分解。首版不用该更复杂路径。

## 两份bank事务与恢复

当前`tile_writer.c:318-325`的session只有tree、plan、cursor和saved_motion。`tile_rate_trial:398-423`先`TileTree trial=s->tree`，但motion.cells是共享指针，已有SB矩形备份/恢复；不可误认为struct浅拷贝自动深拷贝全部状态。R7的models指针位于局部enc，正式writer使用自身fresh bank。

建议session在adaptive时一次分配连续两份`SvtAv2EntropyModels`，在free释放；off保持NULL，不做每候选分配。无需把bank嵌入每个TileTree，避免正式writer树再增加一个未使用bank、加重默认off的栈/清零成本。session中两个bank的身份固定，不用指针交换隐式改变commit意义。

1. create：按frame qctx初始化committed；scratch可等第一次trial复制。cursor=0，沿用空邻居/partition状态。此处只建立rate状态，不写实际header。
2. trial：校验SB索引和blocks/modes；保存plan与共享motion区域；复制完整committed到scratch（含count、terminal、3offset）；原tree浅拷贝后enc绑定scratch，rate计数清0。候选leaf之间持续使用同一scratch，不在每叶重置。
3. trial失败/非commit：恢复plan/motion，丢弃scratch内容，不碰committed/tree/cursor。scratch不属于已提交语义状态；下一trial无条件重新复制，不能靠失败分支“只恢复改过的row”猜测完整性。
4. commit：只有完整语法成功、blocks消费数正确，且winner重放检查通过后，复制scratch到committed并提交trial树、cursor。将enc/借用blocks/modes指针照旧置NULL；不得留下指向局部trial bank或局部enc的指针。
5. frame失败/新frame/新实例：释放整个rate session。packet容量重试只重写已选coeff，正式writer另起fresh默认bank，不能用RD帧尾bank写第一符号。

模型不会代替现有recon的region_copy，也不会代替motion/partition/邻居恢复；这四类状态都必须从同入口重建。R5/R6/coeff后置试探的rate_trial也必须回到同一SB committed bank，不能从刚接受的另一个试探的post-bank继续；它们的接受只更新当前最优policy，完整path依然从SB入口重放。

## 最小接口变化与重放证明

| 文件/接口 | 需要的变化 |
| --- | --- |
| entropy_models.c | 对adaptive+rate_only走新位置调整Q12估计，再更新同row；formal保持R7编码后更新；显式ID与数据不改 |
| entropy_encoder.c/h | 新增小型调整概率helper或复用现prob_inc定义，保持padding单一来源；旧raw rate函数不变。正式audit时可计算相同Q12，但不能把正常formal性能无条件加上log开销 |
| tile_writer.c | create/frame仅在已实现adaptive rate路径后解除拒绝；session两bank、trial绑定/复制/commit；full-frame rate replay用fresh单bank |
| tile_writer.h | 保留旧trial签名作为兼容wrapper；可加内部trial审计结果（R、post-bank digest、symbol序列digest/count），既有接口调用NULL即可。禁止借用局部bank指针返回调用者 |
| rd_frame.c | 现有best_d/best_r/path_hash之外保存best post-bank digest；winner非commit重放和commit重放都核对R与post-bank。candidate ID/原path hash/off winnerhash保持原格式 |
| Codec/API配置 | 只在所有机制通过后放开事先约定的adaptive+RD组合，早期仍拒绝未验证的coeff/motion组合；把同entropyflag传给RD frame_header及正式packet header |

`rd_frame.c:564-583`已有build_path重建、D/path hash/非commit R检查，再append_winner与commit R检查。新增模型摘要必须同时覆盖原始候选、非commit重放、commit重放；摘要不能只hash系数，CDF count/训练offset也要覆盖。定义固定顺序uint16值序列，避免平台字节序或struct padding影响。生产digest用于定位/重放防错；机制单测对bank完整memcmp，不能将有限hash无碰撞当数学证明。

不要只验证帧尾摘要：还要测试每个SB提交前/后模型状态。最终正式writer从fresh默认bank顺序写所有赢家；测试审计在相同SB边界输出R增量、symbol序列与bank状态，逐SB对上RD commit。全帧estimated R等于各SB winner R之和，formal audit估计也应一致；实际字节长度单独记录，不要求相等。formal与rate必须共同经过现tile_tree_write/motion_syntax/coeff_syntax，不复制第二套语法。

只需要内部/测试审计接口，不为产品CLI新增每symbol日志。可以只在审计启用时输出SB摘要；正常路径不遍历完整bank给每个symbol算hash。partial SB的原单候选和replay也不能跳过模型提交。

## 内存与成本上界（推导，不是测量）

当前bank为960×12×2=23,040字节=22.5KiB。两bank每adaptive rate session增46,080字节=45KiB；可只在create申请一次。full-frame独立rate replay另需22.5KiB，正常正式writer也保留R7的22.5KiB，但RD session在重建完成时已经释放，不必同时保留。只存每SB的64位audit摘要，1080p的510个SB每条摘要数组是4080字节；不要存510份完整bank（那会约11.75MB）。

每次trial至少复制23,040字节；成功commit再复制23,040字节。因此T次trial、S个提交SB的**逻辑复制量**是B×(T+S)，其CPU读写流量至少是该复制量的两倍，缓存效果另计。若每trial还完整hash post-bank，再增加约B×T读量；应把它纳入计时或把非必要完整hash限定到新best/重放审计。

现代码每完整SB最多2棵树、每树至多4叶、每叶4模式，保守候选上界512（合法性会减少；不是观测均值）。R6额外motion-rd≤16、wide≤8，与coeff互斥；再含winner非commit与commit两次trial，最宽组合T≤538S。对1920×1080用S≤510保守计，bank逻辑复制量≤约6.34GB/帧；这个极宽上界包含大量实际不发生的非法/partial路径，但足以说明不能承诺复制免费。首公开闭环禁motion/coeff时用T≤514S。pruned与实际合法候选应另记T、模型symbol数、copy/hash字节数，实测单路CPU/墙时。

每symbol额外工作是调整两边界、一次Q12 log和最多N−1≤7次CDF更新；不增加重建候选数量、不扩线程。offset/count等完整复制可先保持简单，只有真实性能证据再讨论按row journal/epoch等复杂优化。不能用COW或共享bank来换取错误的候选串用。

## 分期与退出门槛

1. **只测估计内核，生产组合仍拒绝。** 所有R7模型、N2..8、所有symbol、count15/16/31/32与长偏置相等边界，逐p15对照固定get_adjusted_prob；核总概率32768与finite Q12，证明model更新轨迹与R7 formal完全相同。旧off Q12回归必须逐值不变。actual arithmetic符号流另做固定AVM对照，报告估计与bytes差别。
2. **事务机制仍不公开放行。** 多SB/path用同entry full-bank基准，反序/重复候选、含大量相等边界、不同CDF初值alias、错误发生在coeff中途、容量/非法mode/partial、双实例与新frame，逐bank完整memcmp；逐SB独立最小J及平局ID，R/D/recon/path/post-bank重放一致。覆盖commit失败不推进cursor/plan/motion/recon。
3. **最小public闭环。** 首先adaptive+基础RD、coeff/motion off；默认off和R7 RD-off逐流兼容。三格式AI/P、四qctx与工具边界、多帧，用固定AVM逐样本校验，并对formal/RD逐SB symbol/R/bank轨迹。再依次验收coeff、motion-rd、wide组合；没有全部过关前继续拒绝相应组合，不产出半适应流。
4. **真实收益与性能分开。** 冻结同样本/同QP/候选/λ，记录是否实际bytes/质量受益，独立单路比较CPU与内存成本。adaptive RD会改变赢家和后续预测，不能复用R7同recon质量结论，也不能因首次曲线不佳调训练offset/λ或换片挑点。

所有阶段先锁合同与基线、遵守原campaign/共同速度窗口；本报告只提供可落地分期，不执行下一轮。

</details>


<details>
<summary>032 · next-A-cdf-design.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\next-A-cdf-design.md`  
原文件 SHA-256：`f55460d976354c222eb0e2a9d29e0fba0935b742a0b4189932c0d8ee9921a808`

# 下一基础能力 A 只读设计：帧内 CDF 自适应

2026-10-18。仅审查当前生产代码与本地固定AVM源码，未修改生产、编译或运行测试。R5/R6源码/程序与其效果证据保持；本设计不是已实现能力，也不跳过既定基础能力顺序。

## 最小完整范围

首步可以只做**每帧单tile内自适应**，继续PRIMARY_REF_NONE，不引入帧间CDF继承、次参考平均、多tile或并行。

固定AVM证据：`decoder/decodeframe.c:6858-6861`在PRIMARY_REF_NONE/禁跨帧上下文时调用`av2_setup_past_independence`；`common/entropymode.c:1261-1274`重置coeff/mode/MV默认；`common/entropy.c:33`按实际base_qindex选coeff类别。`decodeframe.c:4741`将tile的allow_update_cdf设为disable_cdf_update的反值。当前`frame_header.c:231`显式关闭更新，P帧又显式PRIMARY_REF_NONE。

因此适应期间每帧从当前实际QP类别的默认模型和count=0开始，帧内按decoder相同符号顺序更新；下一帧仍重新初始化。decoder帧尾会保存上下文及衰减计数（`decodeframe.c:10036-10043`、`entropy.c:79-84`），但下帧PRIMARY_REF_NONE丢弃它。首步不必新增编码器DPB概率继承；若比较最终状态hash，要区分tile结束更新状态与decoder帧尾计数衰减后的状态。

不能只翻disable_cdf_update：该位控制所有被decoder以`avm_read_symbol`读取的模型。只更新coeff、漏掉mode/MV/partition/Tx等模型，会在第二次相关符号开始解码失配。bypass literal不更新CDF。

## 固定AVM更新模型的完整数据

`../avm/avm_dsp/prob.h:35-49`：CDF_SIZE(n)=n+4，AVM_CDFn保存n−1个inverse边界、终端0、count；AVM_PARAn再补三个训练offset。n=2/3与n>=4的offset基值不同。

`prob.h:223`的update_cdf按更新前count选区间（count<=15、16..31、>=32），rate=2+对应已转换offset；count饱和32。不是通用AV1按alphabet推固定rate的公式。生产表当前只保留forward边界，丢掉count与训练offset。

必须从固定source逐模型恢复：

- `entropy_inits_coeffs.h`：全部四QP类别的coeff默认模型与各自AVM_PARA。
- `entropy_inits_modes.h`：partition、Y/UV模式、intra/inter、skip、DRL、palette及DCT_IDTX Tx模型。
- `entropy_inits_mv.h`：ONE_PEL shell set/class、offset、pair与col模型。

三个训练值是固定默认模型的一部分，不是这轮可调参数。例：y_mode_set为PARA4(1,1,1)，palette_y为PARA2(-2,-2,-2)，同样不能用统一rate代替。frame qp类别也不能只按边界相同合并，因为offset可能不同。

建议新可复现提取器保留**表族、完整row坐标、实际alphabet n、边界与配对PARA triplet**，记录固定AVM文件SHA。现有`cdf_tables_coeff.c`引用的tools/gen_coeff_cdfs.py当前路径不存在；现有边界表及历史extend脚本不足以直接提供完整模型元数据。不得用简单正则剥掉PARA后再猜offset。尤其inter_ext_tx数组声明CDF_SIZE(16)，部分实际row却是AVM_CDF2+PARA2，必须按实际row的alphabet而非声明的padding宽度取count/offset。

可存canonical inverse模型以直接对应AVM，或存forward边界并严格转换更新公式；两者都要逐符号与固定update_cdf独立对照，验证舍入方向、15/16与31/32区间切换及count饱和。

## 当前生产可达模型清单与身份风险

| 语法组 | 当前调用/索引 | 应恢复的模型族 |
| --- | --- | --- |
| 分区 | tile_writer.c:181/190，size/raw与rect_map/raw | do_split_cdf[structure=0][ctx]、rect_type_cdf[0][ctx]；当前64SB结构实际使用split前16row、rect前28row，不能把同值row合并 |
| Y模式 | tile_writer.c:135-143及237-241 | y_mode_set，y_mode_idx[3上下文]，y_mode_idx_offset[3]；alphabet仍为4/8/6，不能因只发DC/V/H就缩小 |
| UV模式/调色板关位 | tile_writer.c:147/242/247 | uv_mode[2]、palette_y_mode；palette固定0仍必须更新计数/边界 |
| intra/inter与运动模式 | motion_syntax.c:7-12/109-115 | intra_inter[4]、inter_single_mode[5]、DRL[type0][5]、skip_txfm[3]；永远写DRL0也要适应 |
| MV | motion_syntax.c:32-59 | joint_shell_set、ONE_PEL set0 class、low_class[2]、class2 unary首bin、offset bits[6]、pair greater[2]、col_index[4]；sign和quasi-uniform literals仍bypass |
| 系数是否为零 | coeff_syntax.c:683/706 | txb_skip[predmode][TX][ctx]与v_txb_skip[ctx]，含U/V耦合索引 |
| EOB | coeff_syntax.c:731-761 | eob16/32/64/128/256/512/1024[3个plane/pred上下文]，eob_extra；固定decoder确实共享单个eob_extra模型，不应擅自按平面复制 |
| 幅值/低频/BR | coeff_syntax.c:791-979 | base/base_lf、eob版本、UV版本、BR/HF/LF；当前TCQ=0但TX/ctx必须保持身份，不误步进n而非n−1 |
| DC sign | coeff_syntax.c:1007 | dc_sign[Y][hidden=0][3ctx]；固定decoder U/V sign是literal（decodetxb.c:873-886），不能把它们也适应 |
| inter DCT_IDTX | coeff_syntax.c:773局部16384 | decoder decodemv.c:1164的inter_ext_tx_cdf[eset][eob_tx_ctx][square_TX]；当前多个均匀初值模型必须分别建state |

有两个方向都要处理：**不同模型即使初值相同也不能合并，同一规范模型即使当前有多个转录对象也应合一**。例如global y_mode_index_0与m5_y_cdf[0]是同一y_mode_idx[0]；历史do_split_ctx12别名也映射同一partition row。motion的symbol(e,cdf_value,v)在栈上取临时&cdf，按指针登记将把不同模型混在同地址，按数值登记又会把均匀row等错误合并。

遗留`block_writer.c`的M1 minimal路径使用历史7-symbol近似，生产完整coeff路径已用8-symbol。首步自适应应明确只支持当前完整coeff生产路径，对blocks==NULL的遗留minimal writer拒绝开启，保留其默认off冻结行为，不借适应任务改写旧M1流。

## 可落地接口与事务方案

建议新增独立模型模块（名称仅建议）：

- `SvtAv2CdfModelId`：明确表族+row语义，禁止以CDF值或临时地址作身份。
- `SvtAv2EntropyModels`：当前frame/tile的固定布局可变bank，含每row边界/count/训练offset；按实际QP初始化，仅需当前类别的可变副本。
- `svt_av2_entropy_encode_model(enc, models, id, symbol)`：用**更新前**模型编码或累计Q12成本，成功后严格更新同一row。原const `encode_symbol`保留给默认off与低层冻结测试，不用const-cast修改静态表。
- 每个语法调用点显式传model ID；coeff可从已有family+row算ID，但不靠pointer散列猜alias。启用适应时未知/遗漏模型必须报错，不静默退回静态CDF。

模型bank建议首版直接嵌入TileTree，避免过早设计复杂undo journal：现有`TileTree trial=s->tree`便同时复制可变CDF和所有计数，enc在trial期间绑定trial bank。commit时把整个bank随tree提交；失败/rollback丢弃trial bank，原motion cells仍按已有SB备份恢复。不能只在tree里放共享bank指针后继续浅拷贝。

rate_only目前在entropy_encoder.c:184分支提前return，新增更新必须同时覆盖此分支及正式编码器。每SB候选从相同committed bank开始，候选内符号顺序逐步更新；winner重放必须得到相同R与末尾bank hash，再commit。最终正式帧writer从fresh默认bank顺序写赢家，其逐符号模型轨迹、累计R和末尾bank应与RD提交链一致；不能边写最终帧边重用已被RD推进到帧尾的bank。

模型状态仅为当前帧固定有限布局；首版完整bank复制有明确额外内存带宽成本，要实测，不承诺免费，也不先上指针缓存/写时复制。new frame、编码失败重试、packet扩容重试都要按原提交纪律恢复，不能在失败encode后复用半推进CDF。

## 最小开发/验证闭环

1. 先生成模型元数据及更新内核，逐row与固定AVM边界/PARA/count完全对照；同初值不同row独立更新、alias同row共享，覆盖计数区间。
2. 完整wire接入后先以**rd_mode off固定决策**比较adapt off/on：预测、qcoeff、recon必须相同，只变熵字节。这样直接观察同质的纯熵收益，避免同时引入RD选择变化；三格式AI/P、多SB、多帧、四工具与QP类别边界用固定AVM逐样本验证。
3. 再接RD适应事务：候选换序、逐SB最低J、trial前后bank hash、故障/容量重试、最终writer轨迹与R一致；覆盖R5/R6/coeff可用组合，但不同时变其策略或lambda。
4. 全部正确性通过才按原冻结native样本单独评质量/速度。新模型可改变RD决策，不保证更小局部估计J就有整段收益。

核心预计触及模型/生成元数据新文件、entropy_encoder.c/h、coeff_syntax.c、motion_syntax.c、tile_writer.c、frame_header.c/h，约10至14个内核文件；另有公共API/Codec/CLI/CMake与专属单测/生成器。实际模型清单需实现前机械核验闭合，不能把本表当已经计算出的最终row数量。不是翻一个开关的小补丁，也不需要先强行做frame上下文继承。

单tile保持时`decodeframe.c:4254-4257`不读取context_update_tile_id，新flag不应凭空增该字段；当前PRIMARY_REF_NONE与sequence的avg CDF设定可以保持。真正语法风险是某个reachable模型的身份、alphabet、初始化offset、更新顺序或一次literal/symbol分类错误，而不仅是帧头那一位。


## 首关收敛补充：仅rd_mode=off（只读设计）

建议首关明确拒绝`entropy_adaptation != 0 && rd_mode != off`，只接正式writer和帧头；不接rate session、不改候选、lambda或rd_frame。当前off的预测/变换/量化选择不依赖实际writer的熵长度，故同输入必须逐项保持模式/MV/qcoeff/recon，仅熵字节可变。motion/coeff实验开关原本要求RD，不能与本首关组合。

正式writer每次调用初始化fresh bank，按base_qindex选择默认系数表，帧头允许CDF更新，PRIMARY_REF_NONE保持；每个符号先编码后更新。容量不足重试也重新初始化，禁止复用失败writer的半推进状态。这个范围不需要候选bank复制、rollback、commit、RD重放模型hash或最终writer/RD轨迹同步，明显收敛动态状态与验收范围。模型身份与全部reachable语法接线仍必须一次做全，不能只打开coeff适应。

### 模型规模估计与自动提取

现有`cdf_tables_coeff.h`的物理行形状可直接核算：每个qctx共773行、2819个现有forward boundary words。恢复terminal/count/三个offset后是6684个uint16，即约13.1KiB系数可变状态。若保留全部这些物理行，加当前非系数语法约100行（分区44、Y/UV/palette10、模式/skip17、MV17、inter-tx最多12），bank设计容量约873行；采用每行最多8符号的固定24字节槽约20.5KiB，另有少量映射元数据。这里只是保守布局估计，**不是已经机械证明的精确reachable数量**。TCQ固定0、Y隐藏符号固定0等可把系数候选行上界缩至499，但还可有TX_CLASS/上下文不可达行，首版不建议为省数KiB提前压缩它们。四个qctx默认数据都应生成，运行时只复制当前一个，不需要四个可变bank。

边界和训练offset可以从固定AVM自动生成：提取`entropy_inits_coeffs.h`、`entropy_inits_modes.h`、`entropy_inits_mv.h`的表族/完整坐标/实际AVM_CDFn/配对AVM_PARAn，并展开prob.h的PARA偏移。独立校验建议另用C extractor直接include固定头，逐行导出真正宏展开后的边界、terminal、count、三个offset，与结构化解析生成结果逐word比较；记录输入头和prob.h的SHA。实际row alphabet必须随row保存，不能按声明最大CDF_SIZE或相同边界猜测。随后以显式语义ID清单核对每个formal emit调用及alias；单纯导出所有AVM表不能自动证明当前writer的可达集合闭合。

### 相等raw CDF边界与rate_only限制

已确认固定AVM允许非严格单调raw CDF。`update_cdf`的整数舍入可令相邻boundary相等；`prob.h:248`的`od_ec_prob_scale`还加入位置相关`av2_prob_inc_tbl[nsym-2][n]`，正式算术编码并非直接把raw差值当作range。固定`entenc.c`的q15路径允许`fh <= fl`。当前本项目正式符号路径也使用同样的scale/padding，而`entropy_encoder.c:184-190`的rate_only路径对`lower >= upper`返回ERROR_RANGE，零raw概率不能使用现有log-cost估计器。

因此首关禁止RD是有实质价值的隔离：不把长期适应状态带入当前rate_only。不能为绕过拒绝而给raw CDF夹epsilon或强制严格递增，那会偏离decoder状态。正式writer的独立验收须包含长偏置符号序列、出现相等raw边界后的所有符号，以及与固定AVM算术输出和逐样本解码对照。下一关RD若启用，应另行定义与规范scale/padding相容的有限R估计，而不是直接沿用raw差值或调lambda掩盖问题。

首关预计仍涉及模型/生成元数据、entropy_encoder、coeff_syntax、motion_syntax、tile_writer、frame_header及配置接线和专属测试，但可完全排除rd_frame/rate事务改造。验证目标由“适应改变编码决策后的曲线”收敛为同一完整recon的实际bytes差及固定AVM一致性；收益正负仍由冻结样本实测，不预先承诺。

</details>


<details>
<summary>033 · next-A-rd-baseline-audit.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\next-A-rd-baseline-audit.md`  
原文件 SHA-256：`ffff1dd79d7f64283ad43c2b11d8734d73f7e6462cc2303ec444af3fafb79bca`

# RD与RD-off基线：数学与源码只读审计

2026-10-18。仅审计冻结代码和00提供的原生固定点，不改码、不编译、不编码、不改lambda。当前事实不支持仅因同QP失真增大就认定实现bug，也不支持尚未完成的RD-off/RD整条曲线结论。

## 事实与比较边界

00给出的同源native2D、24帧、用户QP80：R7 RD-off为1,296,423B、VMAF96.186251、Y SSE1,450,105,727；R5 baseline RD-pruned为1,210,256B、VMAF95.192574、Y SSE2,741,937,769；R5 motion-RD为1,162,634B、VMAF95.137246。RD-pruned这一点以更低字节换取明显更高Y误差，是实测事实；它不等于VMAF、PSNR或固定J下整条曲线已经失败。

R7 entropy-on与其RD-off像素严格相同，QP80为1,185,325B，单独熵收益8.57%。这个新点确实比旧RD-pruned/QP80更小且VMAF更高，但同时改变了熵编码能力，不能把支配关系全归因于模式RD。QP100的R7 off为621,002B、VMAF94.154772、PSNR-Y43.946561；entropy-on为561,197B且同像素。R5 motion-RD/QP100为562,387B、VMAF92.948348；其QP80 PSNR-Y42.854611。PSNR与VMAF体现不同排序，仍须分别比较同工具下完整四QP曲线，不能混指标宣称同质增益。

仅这些整段Y SSE和完整bytes不能重算当前J：代码的D还含UV、按位深归一及逐SB舍入，R是共享语法的Q12估计而非最终bytes，J也按SB舍入相加。不同编码器提交的先前SB和P参考帧已经分歧，后续候选不是同入口条件；不能把整段旧off选择直接塞进新RD上下文后要求原D/R。

## 可证：lambda与D的固定点单位自洽

`rd_frame.c:17-28`从16×16量化参数取真实AC QTX，令b=bit_depth−8：

```
q = ac_QTX / (64 * 2^b)
lambda_Q8 = round(ac_QTX^2 / 2^(2 + 2*b))
lambda = lambda_Q8 / 256 ≈ 4*q^2
D = round((4*SSE_Y + w*(SSE_U + SSE_V)) / 2^(2*b))
J = D + round(lambda_Q8 * R_Q12 / 2^20)
w = 4 for 420, 1 for 444; R_Q12 = 4096 units/bit
```

`rd_frame.c:31-46`分拆乘法保留2^20的精确舍入并检查溢出，2^20正是Q8×Q12，未见多乘/漏乘一个256或4096。420三plane raw SSE都乘4，再除位深尺度；444的Y每sample权重4、UV各1，以抵消色度样本数差。这是合同指定的加权像素失真，**不是VMAF，也不是单Y PSNR目标**。

QTX包含QUANT_TABLE_BITS=3和transform的额外Q3；不能把ac直接当像素步长，也不能在lambda上再乘一次TX scale。量化`quantization.c:230-238`按真实dq、Q3 rounding与tx_scale得到decoder系数，`transform_path.c:57-97`按每个block尺寸重新初始化；16×16仅用来查询AC表，AC表本身不随TX尺寸变化。大TX的dq/inverse缩放已在实际重建里处理。

10bit public actual qindex=用户QP+48（Codec/encoder.c:824），qlookup每24项加倍，因此对用户QP1..255，ac10=4×ac8；除2^(bd−8)=4后q相同，lambda_Q8的上述舍入也相同。与“10bit未经归一导致lambda大16倍”的猜测不符。实际输入量化整数舍入/clip会使等幅图形D不完全相同，不能要求任意8/10bit内容逐像素相等。

既有独立R4 scale审计只给有限支持：固定level16、48项能量比0.952936..1.705956，最高来自8bit TX64低幅DC整数舍入；按side增幅的48项为0.959600..1.013430。不能把首组简写为全部接近1，也不能把两组低频基向量证明当作全部频率/全部量化正确性证明。

**仍未证明的是lambda系数4在现工具/率估计/真实内容下最优。** 单位正确不等于感知最优或曲线最优；这是待测模型适配问题，不能从一个native点推导“需要除8/除16”，也不按三片重定参数。当前代码确实允许较高D换较低R，不以SAD或VMAF为最终目标。

## 可证：同入口下RD-off策略的候选包含关系

这部分不能笼统写成“pruned可能没有baseline锚”。对当前工具，源码可作更强的条件证明：

1. **树。** off开启partition_selection时，`Codec/encoder.c:813-816`用source-only `partition_select_quads`产生每SB的0/1计划；`rd_frame.c:406-410`对完整SB考虑同两种tree（若工具关则都是唯一固定tree）。partial的off计划为0，RD也只有对应隐含树。因此off选定tree在RD树域内，前提是相同工具与几何。
2. **预测模式。** off AI先用Y-SAD在合法DC/V/H中取最小，tie按DC/V/H顺序；其UV都DC。off P在`inter_frame.c:40-59`仍先按Y-SAD选intra，然后`visit_luma:113-150`以三plane SAD比较该intra与单SAD ME赢家，tie取intra。由于各intra模式的UV预测完全相同，先取Y最优intra再比inter，等价于在所有合法mode的三plane SAD中取top1，tie按mode ID0/1/2/3。
3. **MV与重建。** baseline ME使用同一±8单SAD helper；R5 list slot0已有独立排序等价验收。RD的`predict/reconstruct_leaf`与off使用同样预测、transform/quant/inverse/clip。相同绝对位置、入recon/ref和当前叶prefix，选同mode/MV就得到同coeff和重建，后继叶输入随之相同。
4. **exhaustive。** 枚举每叶所有合法mode组合，自然含上述off逐叶选择路径。预检只去掉非法mode，不去合法off路径。
5. **pruned。** `rank_modes:201-236`按三plane SAD和mode ID排序；`build_path:344-356`在每个当前prefix上重排、保留前两名。off同入口路径就是每叶top1，所以该树的全0 ranked ordinal可按叶归纳重现off路径。另一树仍exhaustive；partial显式走M5选择，不能用完整SB的top2解释partial。

这给出的是**代码条件推理**，仍建议用下述独立受控机制补逐coeff证明；不将它冒称本轮已跑过的新测试。特别是整帧参考/邻居分歧后，旧off实际输出不再是“当前RD入口重新执行off策略”的同一个对象。

若上述包含关系成立且共享R和D计算正确，则每个SB所选J不高于该入口重新运行off策略的J；它不保证D不高，也不保证整个序列相对独立off运行J更低。改变当前winner会改变未来邻居、参考图、运动预测器及语法contexts；现算法没有序列lookahead最优保证。

## 可证：pruning仍能排除真正更低J的其它候选

保留off top1不等于保留exhaustive J赢家。rank只看未量化预测的L1 SAD，最终J看量化、逆变换、clip后的加权L2 SSE与语法R；二者没有单调等价关系，也不存在代码使用的可采纳J下界。SAD排第3/4的模式可能拥有更便宜的系数分布、更好的变换能量集中或更低重建SSE，因此被top2排除的路径可能J更小。

pruned只对decision!=0树启用（rd_frame.c:410），不能把所有tree的差异都归因于pruning。若先前叶已选择不同mode，后继叶的rank及预测也会变化；不能只对一张固定SAD表列top2就解释整条树。对同入口exhaustive与pruned保留域：后者是前者合法路径的子集，exhaustive最低J应不高于pruned；这是直接机制判据，整段两次独立编码的总J/PSNR排序不是等价判据。

## TX64高频截断：已存在的工具限制，不是本轮发现的非法丢系数

`transform_path.c:6-16, 86-96`明确TX64仅携带packed左上32×32，共1024系数；`transform.c:613-660`限制高频pass并重排，固定AVM的driver/max_eob也采用该限制。TX64不能表示其中高半频率，四个TX32则对各自局部块保留完整32×32域。两种树的可表示残差空间、量化误差和语法开销因而不同。

`rd_frame.c:287-299`的D逐个实际重建像素对source累积，覆盖整个leaf而非只看低频系数；正常情况下TX64截掉的能量已经计入D，不是遗漏高频D的评分捷径。RD若仍选择TX64，可能是固定lambda/估计R下用纹理损失换码率，需实际tree/TX直方图、同入口候选D/R/J和残差频谱证据才能归因这段native差异。

目前没有该native的winner TX/leaf模式统计，不能把Y SSE变大全部归因TX64；相同树内换预测或P参考传播也可造成差别。不能直接扩大TX64系数域：这涉及规范coded grid，不是本轮允许的质量修补。

## 其它待测假设

- **率代理偏差：** 现冻结entropy-off RD使用raw固定CDF差的Q12 log；实际算术编码还有位置padding和range取整/终止。J中的节省可能不等于完整流节省。R7当前仅off-adaptation，不应把其新熵模型当旧RD已经使用；自适应RD独立设计已有规范adjusted-prob方案，尚未实施。
- **目标不匹配：** D包含UV与权重，SAD预选、Y PSNR和VMAF观察不同侧面。既不能因为PSNR好就说VMAF一定好，也不能反向只拿VMAF否定整数目标的正确实现。
- **局部传播：** 贪心SB赢家会改变后续预测和P参考；同入口不劣不能跨帧累加成独立baseline的不劣保证。
- **lambda常数或量化模型适配：** 有可能影响曲线，但现有证据只支持研究，不支持指定一个修正倍数；不得从三片拟合lambda。

以上均不是已定位实现bug。目前可证的限制是启发式top2会丢域、TX64表示域有限、R为代理、目标不等同感知指标；这些限制对本native点的各自贡献尚无量化。

## 受控机制验证建议（事前锁定后再做，本轮未执行）

1. **同入口baseline锚重放。** 测试局部独立实现off选择，保留绝对坐标、完整参考帧、SB入口recon邻居和语法session状态；分别记录所选tree、mode/MV、qcoeff、重建、D/R/J。对RD exhaustive和pruned查到同路径并逐coeff/recon相等，winner J≤该锚。最先用首帧首SB消除历史分歧，再覆盖有邻居/P/partial；不能裁成孤立64×64后让frame边界/ME clamp改变入口。
2. **剪枝反例与域审计。** 用确定性纹理、梯度与稀疏残差机制集合，在完全相同SB入口枚举exhaustive，另枚举pruned，核保留路径相同D/R、minJ_exhaustive≤minJ_pruned；查第3/4 SAD模式是否实际成为J赢家，保存prefix而非只存叶独立rank。若无反例，只报告该集合未找到，不声称SAD是合法下界。
3. **尺度和截断分开。** 用频率覆盖的单basis/多basis、不同side/位深与足够幅值避免低幅整数舍入，独立检验dq→真实pixel SSE；对TX64高半频率单独记录loss，再对合法quad32路径计真实D/R/J。clip前能量、clip后D与舍弃频率误差分开，避免把边界clip解释成scale错。
4. **同语法估计与实际bytes。** 对同entry保存完整候选符号/系数，使用冻结formal writer核actual流及固定AVM样本；对完整tile比较estimated R与实际payload，记录终止/OBU固定开销。不要把每SB独立finalize字节相加冒充连续tile真实R。
5. **最后才做曲线与归因。** 完成同工具RD-off、pruned、exhaustive的冻结四QP native曲线，分别看VMAF与PSNR的公共区间和实际点；再用tree/mode/TX占比解释，而非先按观察调λ。R7 entropy-on/RD-off可单独作为已证同像素熵收益，但不混入旧RD对照声称算法bug。

全程保持现生产不动；若后续授权机制实验，源码/程序/配置均新隔离，继续遵守共同速度窗口禁算。

</details>


<details>
<summary>034 · next-A-subpel-design.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\next-A-subpel-design.md`  
原文件 SHA-256：`28df70f4ef64d669ddd9237bd3f32ac50c0d90b57e8c84923c25adcb0439a1d1`

# 基础亚像素最小闭环：只读准备

2026-10-18。R6在2D小幅或近零收益不能证明亚像素一定获益，也不能代替其它内容/速度事实。以下不实施、不编译、不编码；现生产与R7冻结交付不变。

## 建议首步：固定帧1/2 luma精度，保留±16物理范围

先做单参考、固定BILINEAR、固定帧半像素精度；继续关闭flex_mvres、AMVD、sign derive、OPFL/refinemv、复合/全局运动。1/4并非不合法，但多出luma quarter/chroma eighth相位与更大的scaled shell范围，首步半像素更容易精确归因。精度信令与模型必须完整实现，不能只在预测器对整数MV加插值。

现`inter_prediction.c:3-51`的420奇数luma整数MV已产生chroma半像素，复用了floor负数处理、逐tap边缘复制、两轴最后一次舍入。它只实现0和1/2色度相位及等权平均，**不能直接承担半像素luma**：半像素luma使420色度达到1/4、3/4相位；若未来quarter luma，色度还需1/8等相位。

建议保留旧整数接口与单位不动（motion_estimation.h:4-6），新增显式Q3 MV内部类型/入口，对齐decoder的1/8 luma单位。整数结果在新路径显式乘8，半像素候选分量必须为4的倍数；旧路径不默默重解释现有int32为另一单位。motion cells、选定MV、NEARMV比较、delta、replay与输出hook在新路径统一Q3，不能一处仍以integer pel记MVP。

## 必须同时闭合的wire与预测链

| 部分 | 当前锚与最小变化 |
| --- | --- |
| sequence | sequence_header.c:369-390已写flex_mvres=0、seq_choose_integer_mv=1，因此可保持序列信令与高级开关不变 |
| frame | frame_header.c:216-222现screen content=1、force integer=1、intrabc=0后直接fixed filter。改半像素时force integer=0，intrabc仍0；在filter之前补帧精度bits **0,0**。固定decodeframe.c:8908-8928：首bit1表示quarter；首bit0后二bit0=half、1=eighth。不能套旧AV1 allow_high_precision_mv单bit布局 |
| MV单位 | 固定mv.h:75-81中ONE_PEL=3、HALF=4、QUARTER=5；decodemv.c:1972-1974把scaled shell分量左移(6−precision)恢复Q3。half差值应先以Q3相减、检查可除4，再编码除4后的整数，sign仍literal |
| 预测器 | reconinter.c:2512-2528/reconinter.h:966-1005将Q3换为plane Q4、做UMV border clamp再提phase。无缩放参考时luma Q4=Q3×2，420 chroma Q4=Q3；负MV用数学floor，不能C向零除法 |
| filter/round | 固定filter.h:75-85给16相位BILINEAR权重(128−8phase,8phase)。convolve.h:146-153在8/10bit单参考用round0=3、round1=11；convolve.c:104-154含偏置及两级舍入。可提取两非零tap作等价标量实现，但要证明偏置消去和第一阶段可整除，不能把两轴分别round成像素再平均 |
| border | 可复用逐tap reference_sample的复制边界思路；需对照固定UMV clamp与extend_mc_border（reconinter.c:2357-2430），特别是小块和全块越界。不能把“clamp MV中心”当成“每个tap复制”的无条件等价证明 |

half时luma phase为0/8，420 chroma phase为0/4/8/12，444与luma相同。现半相位64/64只是这些表的一行；加权quarter相位必须正确保留中间精度。BILINEAR只有两个非零tap，不需首步引入8tap滤波搜索、可切换滤波或新高级工具。

## 不能遗漏的shell范围与CDF

当前motion_syntax.c:26-48只支持ONE_PEL/set0，class alphabet7、offset bits0..5，绝对MV±16、差值±32。**同物理±16一旦改half，旧guard按数值照抄就错：**Q3绝对分量≤128、delta≤256；除4后的half单位差值分量≤64，最大shell=128、class7，需要set1。

固定mv.h:296-306：half共有15个class，分set0=7、set1=8。默认模型必须换成entropy_inits_mv.h的joint_shell_class_cdf_0[HALF=4]（7-symbol），并补joint_shell_class_cdf_1[4]（8-symbol）；class7在set1写相对class0。class7仍读7个offset bits，所以即使最坏shell128的offset恰0，也必须写offset bit6并使用其规范CDF。现第6项之后的新增bit6默认forward为20426，PARA2(1,1,1)。shell_set仍为原同一二元模型，但从“总发0”变为可能发1。

若退回±8物理范围，half最大shell64可暂不需要set1；这会删掉R6可用范围，不能宣称完整保留R6基础能力。因此建议首关直接闭合上述set1和bit6，而非靠收紧guard掩盖遗漏。quarter保持±16时最大scaled shell256/class8，set0改8-symbol，还需更多offset位；这也是先half的额外理由。

low-class、class2首TU、offset低位、greater/col-index等是nmv_context中的共享模型，不能按精度全部复制；class CDF却按precision分row，不能因为alphabet相同就复用ONE_PEL row。R7 bank若保持现有语义ID不动，可追加HALF/set0、HALF/set1和offset bit6三行及完整count+3offset。用原独立C extractor逐word校验新增数据，并在formal适应测试中覆盖shell_set=1及这三行，不能仅更新静态表而遗漏适应身份。

## 搜索与决策最小合同

新开关默认off，旧整数函数、帧头bit布局、R5/R6候选/ID/hash与R7默认流保持逐字节兼容。开启后固定half frame precision；继续原整数候选与±16物理guard，先产生原整数winner，在其每叶实际MV附近固定8个half邻点（Q3的±4轴/对角），去重/裁范围，按预先固定SAD次序保留最多2个。固定tree/mode的raster坐标试探，≤8个额外完整path/SB；每次同SB入口重建后继叶，真实三plane D+同half语法R、原λ评分，最后精确MV/coeff/recon/R重放。首步partial跳过refinement，AI不变；不增加参考数或线程。

候选ID必须分配无碰撞的新阶段并保存实际Q3 MV，不能直接覆盖R6既有bit29阶段/slot含义。整数候选保留保证的是**同half帧语法和同入口下**的incumbent不劣；改变frame precision后整数MV的CDF/差值编码也变了，旧ONE_PEL模式的R/J数字不能原样当half域的下界。若未来要在integer/half两种帧头之间择优，需要整帧比较，不能SB级混用而又保持flex_mvres=0。

当前R7公共adaptive+RD仍拒绝，首个half-RD闭环应维持entropy_adaptation=off；不要借亚像素任务提前放开未验证的adaptive RD。新增half模型可先用固定coeff/MV的formal probes分别验证adapt off/on，这不等于公共RD组合已支持。待自适应RD事务另行过关再组合；subpel off时已有R7 adapt-on完整流也必须不变。

## 受控验证门槛

1. 预测primitive独立对固定AVM：三格式、两轴所有half/chroma-quarter组合、正负位移、非平坦参考、最小/大块与四角外延；逐sample比较，不以平坦图或只比SAD代替。覆盖负半/负quarter的floor、两轴奇数加权和与8/10bit极值。
2. 完整wire极值fixture：固定真实key重建作纹理参考，强制(+16,+16)/(-16,-16)相邻MV产生scaled shell128/set1，以及±0.5等非零相位；保存actual Q3 MV、predictor/delta/precision trace，固定AVM逐sample核expected P。只调用motion_write_difference返回成功不算证明。
3. 新CDF默认值/训练offset与长符号更新、adapt两态formal流、双实例/失败fresh；旧subpel-off所有profile流/recon/统计exact。内部率试探必须用同precision相同model family，不能静态ONE_PEL成本评half流。
4. 单SB同入口实际MV/coeff/D/R/J、反序/故障/重放/partial，固定候选预算；然后才冻结同原片的多QP native质量与独立单路成本。没有事先保证收益，也不按三段数据调整λ、滤波或搜索半径。

以上为后续可锁合同的最小范围，未运行任何新增验证或计算。

</details>


<details>
<summary>035 · R5-A-motion-rd.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\R5-A-motion-rd.md`  
原文件 SHA-256：`b1fa483bee1255ef085852dc3c313c2e75d15c000b175a6aebbac56a26319d08`

# 基础能力 R5 A：整数运动候选的真实 RD 比较

2026-10-18，状态 **READY_FOR_INTEGRATION**。遵循`docs/basic-capability-contract.md`的R5单因素冻结算法。本次补齐运动候选选择，不代表整个基础能力阶段完成；正式仍0.0.5-m5。默认motion优化关闭，压缩效果和单路性能由B分开实测后交00独立判断。

## 实现

仅改A属主`motion_estimation.c/h`、`rd_frame.c/h`，新增`tests/unit/test_m6_motion.c`。公共config/API/Codec/CLI、CMake和状态校验由00接线。

新增`svt_av2_motion_estimate_list`，在原±8范围的289个整数MV中按(SAD,|x|+|y|,y,x)精确保留前三名；零MV不在前三时附加为slot3，最多四个、无重复。slot0的MV、SAD和实际搜索位置数须与旧入口完全相同。新helper用当前第三名SAD作为提前终止阈值：只有已累计SAD严格超过第三名时停止该位置扫描，不把用第一名截断的部分SAD误作精确第二/三名。旧`svt_av2_motion_estimate`函数体未改。

full P的正常RD仍用slot0，先运行全部原树/模式候选，保留同入口旧赢家。最终选择赢家树后，单raster叶坐标精化：每叶试至多四个候选MV并把该叶设inter，允许原intra叶转inter；其他叶保持当前已接受的模式/MV。完全相同的当前解按该叶入口状态跳过，反向枚举时跳过集合相同。

每次从SB入口备份重建整path，后继叶预测/变换/量化全部随prefix重算，D来自可见重建，R来自真实共享语法事务，原lambda与整数J不变。按更小J、等J更小稳定ID接受。ID低9bit保留树/模式，MV slot每叶2bit位于bit21..28，原coeff bit9..20保留但本轮两开关互斥。最终保存模式IDs与MV slot向量，按该向量重放并核hash/D/R后只提交一次；实际MV和非零motion policy参与hash。policy0不追加零值，flag0原hash保持。

每完整SB最多四叶×四MV=16额外完整路径；不声称扩展运动域穷举。新helper没有扩大搜索位置，但较宽top3阈值会多扫描一些SAD行，额外RD还会增加正逆变换与rate计算。AI/partial无额外MV试探；原source与committed reference只读，内部也拒绝motion与coeff同时开启。

## 严格机制验收

独立全新外置构建，j2：

| 项目 | MSVC /W4 /WX | GCC严格Werror |
| --- | ---: | ---: |
| 新test_m6_motion | 8658/0 | 8658/0 |
| 原test_m6_rd | 8813/0 | 8813/0 |
| 已有V2 test_m6_coeff | 231084/0 | 231084/0 |

GCC告警为`-Wall -Wextra -Wpedantic -Wshadow -Werror`。没有改旧测试期望来迎合新行为。

新增验证包括：

- 三格式、平坦同分/独立纹理/受控位移、左上/内部/右下块，独立逐像素完整计算全部289个MV的SAD，独立排序，逐项核新helper前三名、精确SAD、slot0旧结果、289搜索计数及补零去重。oracle没有使用生产早停或preferred函数。
- 三格式AI/P、64/128完整SB、136x72/120x120混合边界及56x56全partial。
- 受控Y平坦、UV按(-1,0)运动的fixture使亮度SAD旧赢家为零MV、候选中另有改善三平面D/R的MV；真实生产变换下`motion_accepts>0`。这是机制覆盖，不是实际片收益声明。
- hook检查每条试探的独立整数J与最终逐SB最小J/最小ID；单64SB直接对相同入状态的flag0成本不劣。提交ID逐叶解码mode和MV slot，并与输出真实MV及候选列表核对。
- trial≤fullSB×16、候选增量等于trial、accept≤trial；AI与全partial零trial，winner hash与flag0相同。
- 从最终重建独立逐像素求D、正式整帧writer重放R；原模式及新增MV反向枚举后coeff/recon/hash/J/R保持一致。
- 源与committed reference含padding指纹保持；容量失败后可重试；单64 P故障注入定位到首个MV精化trial的forward，随后干净重试恢复；内部互斥拒绝且输出计数清零。

逐SB不劣只约束相同入口，不保证整个序列的质量或码率；本轮未增加运动范围、subpel、参考帧、lambda自由度或内部线程。

## 证据与剩余验收

目录`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r5-20261018/a/`，入口`delivery-hashes.json`、相对00冻结`root/baseline-source`的四个生产文件diff，以及Windows/GCC构建与三个机制测试日志。R4源码、程序、证据只读保留。

A没有自行运行真实效果矩阵。B按事前冻结三类1080p、帧48..71、四QP进行真实字节/PSNR/VMAF/AVM验证，再单独jobs1暖机与三轮交替配对测速度。速度窗口中A停止所有新增计算；原长测仍继续，因此不能称全机空闲隔离。所有本次计算开始前读原campaign均为real-quality，未触碰冻结素材、模型、依赖、配置或原调度任务。

最终默认off兼容、严格集成/检测器和效果判定由00独立执行。局部单测PASS不代表压缩价值或整个基础阶段DONE。

</details>


<details>
<summary>036 · R6-A-motion-range-design.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\R6-A-motion-range-design.md`  
原文件 SHA-256：`29d36f772619b8b92f8e6398572e47068dbe1d087b74ec09280c71df45e0b225`

# 下一基础能力 A 只读设计：整数搜索范围与亚像素边界

2026-10-18。R5生产、程序、测试和效果数据保持不变；本报告仅阅读本项目及本地固定AVM源码，没有编译、运行测试或实施下一能力。当前R5交付见`R5-A-motion-rd.md`。下一步仍须00锁单因素合同，不把本设计当已实现能力。

## 现有±8不是整数MV规范极限

本项目`Source/Lib/Encoder/motion_estimation.h:4`定义±8搜索；`Source/Lib/EntropyCoding/motion_syntax.c:27`把每轴MV差限制±16，`:79`起把绝对MV限制±8。这是编码器实现guard。

本地固定AVM证据：

- `../avm/av2/common/mv.h:78-81`：ONE_PEL=3、HALF_PEL=4、ONE_EIGHTH=6；`:128` MAX_NUM_SHELL_CLASS=17；`:296-304`按精度计算类别数并等分前后两组。因此ONE_PEL为14类，set0/set1分别7类。
- `../avm/av2/encoder/encodemv.c:172-207`与`../avm/av2/decoder/decodemv.c:1872-1904`：先编码shell_set，再选该组class；ONE_PEL的class0..6在set0，class7..13在set1。
- `../avm/av2/common/entropy_inits_mv.h:29`为当前ONE_PEL set0 CDF7；`:59-64`对应已有offset bits；本项目相同常数在`motion_syntax.c:32-34`。
- 本项目`motion_syntax.c:35-48`固定写set0，随后Cdf7；class>=3编码class个offset位，现有6个CDF支持class6。shell=|dx|+|dy|，class6覆盖64..127，所以本实现已有语法分支可表示shell≤127，实际guard比它窄。

据此，**首步扩绝对MV至±16**，若当前/预测MV均在该范围，则每轴delta≤32、shell≤64，仅触及class6；现有set0/Cdf7、6个offset CDF及pair quasi-uniform编码已足够。只需有界放宽绝对MV/delta guard并通过独立AVM验证，不必开启新wire工具或改变帧头。

不能把同一结论直接推至±32：极端当前(32,32)、预测(-32,-32)产生shell128/class7，本项目尚未写set1分支及其CDF、更多offset位；这时必须补全已有规范语法子集，不能只改整数guard。它仍是现有规范工具的实现补全，未必是新序列工具，但需要单独语法验证。也不能把MV分量上限、shell alphabet上限和参考边界clamp混作一个数值。

固定AVM `mvref_common.h:155-164`另有16像素MV_BORDER以及block尺寸/帧边界共同的预测MV clamp。本项目writer用首个可用空间候选，没有同名clamp。对本轮fullSB的32/64叶和±16，边界允许量覆盖该范围；这是源码推断，仍需边角块/邻居正负极值的AVM反事实实测验证，不能免验。

## 最小可归因整数扩域方案建议

保留R5完整算法和最终赢家作incumbent，新增后置宽域候选，不替换旧top3而丢失R5候选。不改lambda、参考、精度或wire工具。

候选生成只在新外域：

1. [-16,16]按步4做全分辨率SAD粗位置扫描，去掉已覆盖[-8,8]的25点：81−25=56个新位置。
2. 固定选最佳粗点，在其周围±2的5×5整数窗口细化，去重、裁到±16并跳过旧域/已测点，至多24个新位置。
3. 从该已访问集合稳定保留至多两个不同的宽域候选。每叶新增SAD位置≤80；原289仍完整保留，不把全搜直接扩大至1089。这里“粗”指MV位置网格，SAD仍读取完整原分辨率块，无新图像金字塔或下采样误差。
4. 在相同SB入口、固定R5赢家树的后置单raster坐标阶段，仅试这两个新MV，允许intra→inter，每leaf保留现解，至多8个额外完整path。完整D/同语法R/J、重放/回滚和默认off纪律沿用。

这不是±16全局搜索，可能错过粗点不佳而细点很好的局部极值；不应冒充1089点最优。步4/±2是事前固定有限预算，不按三片结果改。若速度成本过高或真实曲线无收益，留否决证据。

ID需要事先明确：R5已占低9bit模式/树及bit21..28的4×2bit MV slot，coeff还占bit9..20。直接把每叶slot扩到3bit会越过uint32。建议用尚空bit29区分新宽域阶段，该阶段复用bit21..28，slot0代表R5保存的实际incumbent MV、slot1/2代表两个宽域MV。原R5候选/赢家ID完整保留；只有新宽域trial设置stage bit。stage/list映射、实际MV和hash必须一起审计。若未接受新候选，应恢复原R5模式/MV/hash/ID，不把旧slot含义静默改掉。此建议比扩大候选ID公共类型影响小，但要新增阶段状态测试。

测量口径应保持R5的native24帧/四QP集合，默认兼容与AVM验证先通过，再做质量曲线和单路速度，不能把新增搜索位置数当运行时间百分比。除更多SAD外，后置最多8path会增加变换/语法评分；目前没有实际成本数据。

最小测试需覆盖独立穷举“实际访问集合”的SAD排序和去重、粗/细边界访问上限、位移±12/±15/±16、保留R5 incumbent、stage ID映射、正负极值相邻块delta(32,32)/shell64、class5→6边界、quasi-uniform pair极值、单/多SB及帧边角固定AVM逐样本、错误重试/反序/只读源参考。不能要求层次搜索等于完整1089点oracle。

## 后续亚像素不是加一个0.5步长

建议整数扩域完成并独立归因后，再单独做固定BILINEAR半像素候选，不同时扩多参考/更换滤波器。

- 帧语法：当前`frame_header.c:216`强制整数；固定AVM `decodeframe.c:8908-8927`表明取消force_integer_mv后必须写frame precision选择（先0再0表示HALF_PEL），且精度决定是否读额外字段。`sequence_header.c:372`现有flex_mvres关闭可保持，先不用每块可变精度。
- MV单位：固定AVM内部MV为1/8像素（`mv.h:53`），本项目结构现为整数亮度像素。需要明确一种新固定单位或独立预测入口，统一ME候选、mode/cell、预测MV差、rate与最终writer；不能混用整数和1/8单位。
- 概率：HALF_PEL的set0仍7类，但初始CDF已不同（`entropy_inits_mv.h:31`），更大shell还可能进入set1。不能复用ONE_PEL CDF只把差值乘2。实际precision、shell set、offset与sign必须同decoder。
- 预测：当前`inter_prediction.c:31-48`只支持整数Y与420因奇数亮度MV产生的半相位UV。半像素Y会让420 UV出现四分之一相位，必须扩相位、负MV floor、两轴组合与准确舍入。
- 固定AVM `filter.h:75-85`有16相位BILINEAR表；`convolve.h:116/146-149`规定round0/round1。即便BILINEAR可简化，也须以固定AVM证明8/10bit、420/444、边界延拓、正负/双轴相位和clip的逐样本结果，不能把两个轴各自round后直接相加。

首个半像素实验可围绕整数incumbent保留原点并试至多8个相邻半像素MV，用完整D/R评分；仍不称全亚像素最优。该项涉及wire精度和预测链，是下一份合同，当前不实施。

</details>


<details>
<summary>037 · R6-A-wide-motion.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\R6-A-wide-motion.md`  
原文件 SHA-256：`414f27f8d1ccc0c9a28960cadc3a288cbc9e1249c121bba67ce0c47a16ab08cd`

# 基础能力 R6 A：保留 R5 的有界 ±16 整数扩域

2026-10-18，状态 **READY_FOR_INTEGRATION**。算法在查看R5首对候选质量之前事前冻结于`docs/basic-capability-contract.md`；R5源码/程序/证据保持外置只读。本次不扩亚像素、参考数或wire工具，正式仍0.0.5-m5。

## 实现与预算

A修改`motion_estimation.c/h`、`rd_frame.c/h`、`motion_syntax.c`，新增`tests/unit/test_m6_wide.c`。公共接口/CLI/CMake由00接线。`motion_optimization=2`先完整执行R5，保留其最终树、模式、实际MV、成本、ID和hash作incumbent；不是用新搜索替换R5而丢掉原解。

新`svt_av2_motion_estimate_wide`只搜索原±8之外的新外域：步4粗网格56点，固定最佳粗点周围±2一次细化；裁到±16、去重并跳过旧域/已测粗点，最多24新点，新增SAD位置最多80/叶。始终全分辨率SAD，按原(SAD,Manhattan,y,x)稳定排序保留访问集合的两个最佳MV。早停使用当前第二名SAD，最优粗点在细化前固定，不移动中心或追加迭代。不声称完整1089点±16最优。

完整P SB在R5赢家树上单raster逐叶试两个wide MV并设inter，允许intra→inter，最多8个额外完整path。每trial同SB入口恢复、整path重新预测/transform/quant/inverse/clip、原D/共享语法R/整数lambda/J。其它叶保留当前已接受模式/MV，未接受宽域时完整恢复R5的ID、hash与真实MV。AI/partial不扩。

ID bit29标记wide阶段；bit21..28在该阶段逐叶编码0=保存的R5 incumbent实际MV，1/2=两个wide MV；R5未带stage位的ID继续使用原top3+zero映射。低9bit仍为模式/树。hash包含真实MV、policy和wide阶段；不能只重用slot字段而静默改变旧ID解释。最终同阶段/slot/模式整path重放核对hash、D、R后只commit一次。增加实际wide_trials/wide_accepts；仍与coeff优化互斥。

motion_syntax只放宽绝对MV guard至±16、差值guard至±32。最坏shell64/class6在既有ONE_PEL set0/CDF7与6个offset位以内，未更换概率、帧精度、预测滤波或新规范工具。旧MV±8/delta±16以内的编码表达式和旧ME入口未改。

## 严格机制检查

独立全新外置Windows/GCC构建，j2：

| 检查 | MSVC /W4 /WX | GCC严格Werror |
| --- | ---: | ---: |
| 新test_m6_wide | 10295/0 | 10295/0 |
| R5 test_m6_motion | 8658/0 | 8658/0 |
| 原test_m6_rd | 8813/0 | 8813/0 |
| V2 test_m6_coeff | 231084/0 | 231084/0 |

GCC使用`-Wall -Wextra -Wpedantic -Wshadow -Werror`。未修改旧机制测试期望。

新测试独立完整计算粗点56位置的SAD、独立排序锁最佳粗点，再独立构造去重后的固定细化集合、完整SAD排序，与生产返回的两个向量/精确SAD/访问次数逐项一致。oracle不调用生产早停、排序或keep函数；另保留R5全部289位置独立排序/slot0兼容检查。

机制还覆盖：实际wide接受（受控(-16,16)预测源）、同入口保留R5成本不劣、原域与wide全部已访问候选的逐SB最小J/最小ID、stage29下的R5 incumbent与wide slot映射/输出真实MV核对、反向原候选/MV枚举的最终coeff/recon/hash/J/R一致、最多8trial、AI/partial跳过、逐像素独立D、正式整帧R重放、源/参考padding只读、容量失败及首wide-trial故障后的重试。

## 真实极值语法与固定 AVM 逐样本证明

不是只检查`motion_write_difference`返回成功。外置C generator使用当前真实tile writer生成128×64的两个64SB零残差P帧，前接冻结R5程序从非平坦纹理源编码的真实keyframe。独立Python从该keyframe内部重建逐像素按指定整数MV与边缘clamp计算expected P，保存两帧source、expected和完整流，再用固定AVM解码逐字节比较两帧可见样本。

| 目录 | 实际两SB MV | 第二SB predictor | delta | shell | 结果 |
| --- | --- | --- | --- | --- | --- |
| a/extreme-proof | (-16,-16)、(16,16) | (-16,-16) | (32,32) | 64 | 三格式全部PASS |
| a/extreme-proof-negative | (16,16)、(-16,-16) | (16,16) | (-32,-32) | 64 | 三格式全部PASS |

共6条真实流，8bit420、10bit420/444各两个方向。writer状态trace记录第一SB落入空间候选cell后的实际predictor，第二SB因而确实经过class6；两方向还验证sign与非零位移的像素对应。参考每平面均检查非平坦，expected P与零位移参考不同，避免用平坦图掩盖错误MV。

每个case保留`warm.y4m`、`source.y4m`、`warm.yuv`、`expected.yuv`、`stream.av2`、`decoded.y4m`、`actual-mv.log`与命令输出；result.json冻结全部SHA，以及C generator/EXE/Python、warm app和decoder的SHA。generator以`/utf-8 /MD /W4 /WX`及link `/WX`编译，未放宽项目告警。00可独立重跑AVM并与expected比对。

## 交付与限制

根目录`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r6-20261018/a/`；入口`delivery-hashes.json`、相对`root/baseline-source`的五个生产diff、Windows/GCC构建和四个机制测试日志、两组extreme-proof/result.json。R5程序/构建目录未覆盖。

A没有另跑真实效果矩阵。B沿用事前同native24帧样本和四QP，用R5质量候选可审计复用作基线，并做鲜配对单路速度。局部J不劣不保证后续参考链或VMAF收益，≤80位置不等于运行时间百分比。所有新增计算前检查原campaign为real-quality；速度窗口中A停止新增计算，原冻结长测继续。默认off兼容、检测器、跨平台流以及最终效果判定由00独立验收。

</details>


<details>
<summary>038 · R7-A-entropy-adaptation.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\R7-A-entropy-adaptation.md`  
原文件 SHA-256：`124645784ddfbaf83fa7e392d060b9d863cf4d978a5f620675ef6045eafee1e1`

# R7 A：帧内概率自适应首关

2026-10-18。遵循`docs/basic-entropy-contract.md`；R5/R6源码、程序、原质量任务均不覆盖。正式版本保持0.0.5-m5。新增构建和证据在`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/optimization-r7-20261018/a`。本报告不宣称真实1080p压缩/速度结果。

## 实现范围

`entropy_models.c/h`使用显式语义ID和固定布局bank；`entropy_model_data.c/h`由`tools/gen_entropy_models.py`从只读固定AVM三组默认表生成，保存inverse boundaries、terminal、count、三个训练offset。每个正式tile写入fresh bank，成功符号先按旧模型编码再更新；符号失败不更新。bank为23,040字节，存于当前writer栈，无全局可变模型。默认off绑定NULL，走原冻结CDF编码函数。

`frame_header`的`entropy_adaptation`默认0，开启时仅把disable_cdf_update写0；继续PRIMARY_REF_NONE。`tile_rate_create`和`tile_rate_frame`对开启明确拒绝；遗留blocks==NULL minimal writer也拒绝。新`svt_av2_encode_packet_tools_entropy`在旧tools参数的decision_count后追加entropy_adaptation；旧tools和所有旧wrapper传0。公共RD组合限制由00的配置层执行。

没有接ratebank、改lambda、预测/变换/量化选择或帧间CDF继承。`Codec/encoder.c:658`先完成encode_reconstruction，之后在673行读取熵开关路由正式packet writer。Encoder/Prediction/Transform/Quantization/Reconstruction源中没有entropy_adaptation读取；传入writer的coded_blocks/coded_modes及其qcoeff均为const借用，tile tree只修改自身邻居状态。开启后的完整重建一致还由00的固定流矩阵独立验证，不能仅靠源码审计宣称wire正确。

## 模型闭合清单

物理bank是960行/当前qctx，四组默认状态共3840行；**960不是精确可达模型数**。保留全部773个系数物理行，以及187个非系数物理行，不按相同CDF数值合并。各行完整AVM源坐标、alphabet和规范状态见`a/data/manifest.json`。

| 正式symbol-site | 语义family及flattened row | 物理行数/当前限制 |
| --- | --- | --- |
| tile_writer.c:182/191 | SPLIT=size_map×4+raw；RECT=rect_map×4+raw，均structure0 | 各64行；当前64SB仅split前16、rect前28 |
| tile_writer.c:136-149、238-245 | Y_SET；Y_IDX[context0..2]；Y_OFFSET[context0..2]；UV[mode!=0] | 1+3+3+2；旧global Y_IDX0与m5 row0共享同ID |
| tile_writer.c:248 | PALETTE，固定0仍更新 | 1 |
| motion_syntax.c:90、111-113 | INTRA_INTER[ictx]、SKIP[skip_ctx]、INTER_MODE[ctx]、DRL[type0][ctx] | 4+6+5+5；SKIP仅前3可达 |
| motion_syntax.c:36-58 | MV_SET；ONE_PEL/set0 MV_CLASS；MV_LOW[class0..1]；MV_CLASS2首TU bin；MV_BITS[offsetbit0..5]；MV_GREATER[0..1]；MV_COL[min(class,3)] | 1+1+2+1+6+2+4；其余TU/quniform/sign为literal |
| coeff_syntax.c:684/707 | V_SKIP[ctx]；TXB_SKIP[(is_inter×5+txs_ctx)×10+ctx] | 12+100 |
| coeff_syntax.c:735/739/754 | EOB16..1024[plane/pred eob_ctx]；EOB_EXTRA单共享模型 | 7×3+1；当前square只选16/64/256/1024 |
| coeff_syntax.c:777 | INTER_TX[eset3][eobctx][square_TX] | 12；即使各row初值均匀也独立。eobctx严格对应get_lp2tx_ctx，TX64不发 |
| coeff_syntax.c:804-864、925-983 | BASE_EOB[TX][ctx]、UV版；LF_EOB及UV版；BASE[TX][20ctx][TCQ]、LF[TX][33ctx][TCQ]、两者UV版 | 20+4+20+4+200+330+12+12；TCQ固定0，保留另一半物理行 |
| coeff_syntax.c:882-892、997-1007 | BR[ctx]、BR_UV[ctx]、BR_LF[ctx]，每个低幅扩展符号都更新同模型 | 7+4+14 |
| coeff_syntax.c:1039 | DC_SIGN[Y][hidden0][sctx] | 物理12、实际前3；UV/DC与AC sign仍literal |

正式tile_writer/coeff_syntax/motion_syntax没有残留直接encode_symbol调用，分别通过显式模型wrapper。wrapper只用model ID访问bank，不用临时指针、地址散列或CDF数值识别模型；nsyms或ID不合法时拒绝。旧block_writer仍直接使用固定CDF，但adaptive入口明确拒绝该路径。literal/Rice/quniform语法没有更新模型。

## 独立默认数据与更新验证

生成器结构化解析真实CDFn/PARAn；实际n随row保存，不用数组CDF_SIZE最大padding猜测。独立C extractor直接include固定AVM原始coeffs/modes/mv头及prob.h，对3840组、31,440个有效canonical words逐word核对全部边界、terminal、count和3offset：PASS。证据`a/data/validation.json`保存输入、生成C、编译器输出、extractor和展开文本指纹。AVM源码只读；为Windows extractor避免pthread，独立外置config从既有诊断build复制，仅CONFIG_MULTITHREAD设0，未改概率宏。

第二个独立C oracle直接调用固定`prob.h:update_cdf`，对全部3840组各1024符号执行长偏置后全alphabet序列：3,932,160步，0差异；观察到相等raw相邻boundary共810,202次。计数15/16/31/32分界及饱和自然覆盖。证据`a/data/update-oracle.c`、`update-result.txt`、`update-oracle.exe`，生产无AVM调用依赖。

raw边界相等不夹epsilon：正式range coder沿用规范位置prob_inc缩放；当前rate_only对raw零概率拒绝，因此首关明确禁RD。该oracle的相等次数是全部行/多步的观察计数，不是不同模型数量。

## A短验

Windows Release严格告警/WX、j2：`test_m7_entropy` 3,934,722检查/0失败；既有`test_m6_rate` 98,534/0、`test_m6_rd` 8,813/0。新测试包含N2..8/所有qctx/所有物理row更新参考对照、长偏置产生相等boundary、双bank独立/重复同符号流一致、非法ID/alphabet和失败符号状态不推进、rate_only拒绝、重新初始化、三格式AI/P实际tile写入、容量失败输出不变及fresh重试与干净输出一致、输入coeff不变。

初次新测试的P fixture忘记multi_frame=1而被帧头合法拒绝；补齐合法fixture后9项失败全部消失，没有放宽生产校验。A首次GCC入口因WSL启动错误未进入编译；按00安排，GCC/SAN和正式多流验收由根统一，A未重复大编译。A没有运行96流或native矩阵。

编译/测试均先读原campaign-status：RUNNING/real-quality。上述小测试与原长质量、B的R5/R6质量任务并发，CPU消耗并非零；仅使用j2，没有性能隔离结论。共同速度窗口通知后将暂停新增计算。

真实同recon字节收益、96组跨平台/固定AVM结果与独立单路计时由00/B后续报告；本交付只证明当前机制短验及默认数据/更新一致性，不把工具接线成功等同于已经解决压缩效率差距。

</details>


<details>
<summary>039 · 00-INTEGRATION.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\00-INTEGRATION.md`  
原文件 SHA-256：`df96cceb574b180f1910c29fae6086c3e0d7d183f53454eb16ca99ab34ba6e3b`

# 00：M6 集成、合同与独立终验

## 授权边界

本文件是下一轮开发指令。本轮只写指南，不实现 M6。沿用 Codex 主会话加 A/B 两个子智能体；用户不需要在线回答实现细节，按本合同和实测证据解决常规选择。需要超出本阶段范围时留待后续，不把边界扩展伪装成依赖。

## M6.0：先冻结，再并行

1. 读取 M5 正式收尾报告，以及 `svt-av2/artifacts/m53-release/source-manifest.json`、`svt-av2/artifacts/m53-release/final-verification.json`，重新核对当前140源与5程序 SHA；冻结 `artifacts/m6-root/baseline/`。M5.3 旧候选清单不能自动代表版本收尾后的最终树。
2. 写 `docs/m6-rd-contracts.md`：候选域、遍历顺序、整数成本和 λ 单位、三平面失真权重、rate 接口、事务状态表、提交/回滚、固定 tie-break、配置变化时机。
3. 向 A/B 下发读写边界。A 拥有 RD 核心及 frame-core 决策；B 拥有熵计量和独立评测；00 拥有公共 API/CLI、配置校验、构建文件、README/project/版本和最终接线。`keyframe.c` 连接候选与写码流处要单独指定写者。
4. 明确新开关 `off|exhaustive|pruned` 与旧工具候选开关的组合含义，记录 API/ABI 和 CLI 帮助。`off` 原路径不经新评分器。
5. 让 B 尽早建立真实视频源清单、转换合同与测量脚本；A 可用 B 的固定测试 rate stub 开发接口，但最终验收不允许 stub/零 rate。

## 接口依赖与串行合入

先接 B 的只读 symbol-rate/熵 checkpoint 最小接口，再接 A 的事务和候选评价，依次接 AI、P、分区，最后接剪枝。每一步都运行该步固定样例的 AVM 解码和重建对比，不等最后一次大合入才发现上下文分叉。

交易契约至少返回候选标识、三个平面 D、固定精度 R、λ/J、合法性、所需 scratch 和有效生命周期；赢家提供足够的 mode/MV/coeff/partition 信息供正式写入。禁止“评分时一套残差、提交时重新选择另一套模式”。允许确定性重放赢家，但必须核验重放的 coeff/recon/rate 与试探一致。

当前帧 core 通过 hook 将结果交给 tile/coeff 写入，M6 必须让评分看到与正式 writer 相同的上下文，或先产出可重放决策记录再串行写入。不能继续按 source-only 预计算整帧 partition 后称其为完整 RD。

## 终验矩阵

先用小矩阵逐步放行，再跑冻结组合；不把所有工具/QP/视频/平台/穷举预算作笛卡尔积。

- **兼容**：RD `off` 的 M5 默认和旧工具四组合，QP100 冻结指纹；新增选取 QP1/40/80/100/160/220/255 做三格式 AI/P 兼容；历史输入离线的按清单定向恢复。
- **正确性**：三格式 × AI/P × 三种 RD 模式 × 旧工具四组合 × 七个 QP；小分辨率为主，包含边界尺寸、首行首列、非整 SB、零残差、高幅系数。全部固定 AVM 解码与内部重建逐样本一致。
- **QP 完整范围**：每个用户 QP1..255 至少覆盖三格式 AI/P 的 RD 开启小样例；8/10-bit qindex/CDF 分组边界专项。穷举分区大组合只在小图运行，不强制全 QP 大视频穷举。
- **决策机制**：小图 exhaustive 全候选的最低 J 核验；候选换序后固定 tie-break 的赢家和最终流一致；回滚/容量重试/错误重试结果等于从干净状态编码。
- **确定性**：Windows/MSVC、WSL GCC/Clang 完整码流和 recon 指纹一致；ASan/UBSan/LSan、泄漏、失败路径。整数实现避免 libm 舍入造成跨平台不同。
- **工程**：现有严格 CTest、API、量化/系数/边界专项保持；新模块有必要单测。候选池不能每个候选 malloc/free；记录峰值内存、候选数、重建/变换调用数和分配次数。
- **效果**：按 B 的真实视频合同，比较冻结 M5、M6 exhaustive、M6 pruned；给出每片多 QP 的体积/码率、PSNR Y/U/V、VMAF、单独编码时间和内存。exhaustive 小图数据与真实视频 pruned 结果分表，不能混为同一速度点。

## 开发完成与放行

`READY_FOR_INTEGRATION` 只表示模块交付，00 完成独立验收后才写 `DONE`。最终写 `docs/task-reports/M6-integration.md`、机器可读验证摘要及源/工具/素材清单。若剪枝样本上损失明显、速度没有收益，报告失败的策略并保留可核验 exhaustive 路径；不为了过门槛改视频或挑 QP。

质量优化不承诺无条件支配 M5：即使局部 J 较小，后续邻居/参考会变化，整段 BD-rate 仍需实测。最低交付要求是 RD 正确、剪枝可量化、真实视频结果可复现、明显回归已解释并处理；若结果不能支持生产启用，保持 `off` 默认，明确标注实验状态。00 在 M6.0 根据基线实测冻结可接受的时间/内存与剪枝损失预算，不能测完后倒改阈值。

完成本阶段后才讨论 M7 preset。CRF 与编码器内部并行按路线分别留在 M9/M8，不顺手提前实现。

</details>


<details>
<summary>040 · 01-RD-CORE.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\01-RD-CORE.md`  
原文件 SHA-256：`787a5d7140eda22dbe7e13f72c68427bc13222f877a4e62ef0f5e050a6fe9441`

# A：有界 RD 候选与重建事务

## 起点与归属

先读 README、00 任务及 `Source/Lib/Encoder/{intra_frame,inter_frame,transform_path}.c`、`Source/Lib/Common/block_geometry.c`。M5 的 `m5_select_intra` 为预测 SAD，P 的 intra/inter 比较也是 SAD；partition 选择不是完整重建成本。不要只给 SAD 加一个常数就宣布完成 M6。

A 负责新 RD 核心、候选 scratch、λ/D、AI/P 和分区决策。公共头/CLI/构建交 00，熵符号 rate 与 checkpoint 交 B。需要熵/系数上下文接口时直接联系 B，并抄 00；不可另写一份与真实 writer 分叉的系数估算器。

## 成本合同

采用 `J = D + λR`，生产评分使用固定精度整数，写明每一项单位和舍入。D 从**量化、逆量化、逆变换并裁剪后的重建**计算可见样本 SSE，禁止用原始 transform coeff 能量或预测 SAD代替最终 D。

三平面权重须明确、固定并覆盖 420/444，不能让 444 因样本数量变化悄悄切换优化目标。可采用归一到 8-bit 量纲的加权 SSE，但 10-bit 归一化与 λ 只能选一套一致方案，禁止重复乘/除位深比例；权重及归一化由 00 在 M6.0 冻结。对本帧不同候选比较同一可见区域，不计 padding。

λ 从本编码器**实际量化步长/表**导出，不能把用户 QP 当成 AV1/SVT-AV1 的 QP直接套经验指数式。10-bit实际 qindex=用户值+48；记录 qindex、量化步长、λ。首版允许有限常数标定，但只用独立标定集，测试前冻结；不按每个视频调整 λ 伪造收益。不要求 8/10-bit同一数字 QP 得到相同质量。

R 使用 B 提供的同上下文语法成本，含模式、分区、MV、skip/EOB、系数幅值、额外位和符号，不能只数 nonzero。统一固定精度 `D/λ/R/J`，证明 QP1/255、10-bit、64×64三平面与分区求和的最坏值不溢出；乘法中间精度和舍入有专项测试。定点 log/LUT 放在 B 的 rate 模块，不在 A 另做浮点路径。

## 候选集合与顺序

1. AI：合法 DC，旧 adaptive 开放 V/H；UV DC。边界可用性与现有模式信令条件一致。
2. P：保留上一张已提交重建参考；候选为 DC/V/H intra 和现有整数 ME 产出的合法 single-inter。当前 ME 搜索半径8、至多17×17=289整数位置，M6首版只把旧 SAD 预筛获胜的一个 MV 送完整 RD，不把289个都送入 RD；因此只称 ME 预筛后的有界 RD，不能称 motion exhaustive。若后续保留 top-K，K 和稳定排序须重新冻结，报告候选数量。
3. 完整 SB：原64树与现有四32树；两者需含各叶 mode 和真实 partition syntax rate。partial SB沿用固定合法边界树。
4. tie-break固定为合同中的候选ID顺序，与遍历、分配地址、线程完成时间无关。exhaustive与pruned使用同一ID和成本实现。

廉价阶段用 source 特征/SAD/SATD 排序与剪枝即可，不强行为五个阶段各造一套计算。进入最终评分的候选必须完整 transform/quant/recon。小图 exhaustive 不剪枝；分裂树的兄弟叶相互影响预测和熵上下文，必须按合法编码顺序重建。小图超块穷举须枚举冻结候选的完整叶组合：AI至多3^4=81、P至多4^4=256条模式路径，再与未分裂树比较；各路径中的ME仍为一个预筛MV，不是所有MV组合。若提供逐叶局部oracle用于调试，必须另命名并禁止冒充上述超块穷举。

遵守现有变换尺寸和系数布局，特别是 TX64 的有效packed系数为top-left32×32，不可按4096个可编码系数复制、计rate或重建。

## 试探事务与候选寿命

每个候选从同一入状态开始。至少列出并隔离：

- 当前候选覆盖区的 Y/U/V 重建、邻居可见状态及 padding 写入。
- `TileTree` 的 above/left mode与partition、三路CoeffNeighbours、motion cells（含MV/valid/inter/new/skip）、next/count/mode_count等游标及所有hook写入的状态；含指针的浅拷贝不隔离这些数组。
- 熵编码器 low/rng/cnt/offs/error、输出/precarry缓冲有效区；结构体浅拷贝不等于隔离底层缓冲。
- 决策记录、统计计数、packet 长度、错误码；失败候选不污染最终统计。

源图和上一张 committed reference 只读，只有整帧成功后才能更新参考。赢家提交一次；其他候选不留下任何字节、重建或计数。容量不足重试和候选错误不得重复提交参考或使用半完成 recon。

候选元数据和 scratch 以帧/上下文或最大 SB 一次分配/复用，标记候选临时 coeff/recon 的寿命，避免下一候选覆盖赢家所需内容。小图 oracle 可以更慢，但要设预算且不能在生产路径每叶深层反复 malloc/free。优先保存小型事务域，不为每个候选复制整段视频。

## 分步交付

- A1：成本、λ、候选ID、scratch/事务接口和单测；用 B 的固定 rate接口接线。
- A2：AI 完整 RD，固定小样例解释为什么非 SAD 最小者可胜出；AVM/recon一致。
- A3：P 完整 RD，包含静止、平移、切场和新纹理；验证真实 intra/inter/MV 语法成本。
- A4：两种合法 partition tree 完整试探、提交；分裂赢家只更新一次邻居。
- A5：固定 exhaustive 基准后实现最小 pruned 策略，报告候选保留数、最低 J 差值及时间，不擅自增加 preset。

必要单测包括候选换序、先试失败候选再试赢家、跨 SB 邻居依赖、四32中不同叶选不同模式、全零/高幅 coeff、边界 V/H 不可用、三平面位深/布局、相同 J、API失败/容量重试。每个交付附源码与产物 SHA、命令和结果路径，向 00 报 `READY_FOR_INTEGRATION`，不自行标 M6 DONE。

</details>


<details>
<summary>041 · 02-RATE-VALIDATION.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\02-RATE-VALIDATION.md`  
原文件 SHA-256：`770d498bdde6e6c0aa1318508b59ac3ec97ae9161217c270a28ee60a7ff17006`

# B：真实语法 Rate、独立核验与效果评测

## 归属与第一步

B 负责熵/符号成本接口和事务支撑、独立决策验证、评测脚本及素材清单；A 负责候选与重建，00 负责公共接口/串行接线。先读 `EntropyCoding/{entropy_encoder,coeff_syntax,motion_syntax}.c`、`Bitstream/keyframe.c` 和 M5.3 批测工具；确定真实写入路径后与 A/00冻结接口。

当前 CDF 更新禁用，概率表本身只读，不代表没有上下文状态：系数、模式、邻居、MV、range coder 与其输出缓冲仍会影响候选。不要为本阶段顺带开启 CDF adaptation。

## R 的含义必须准确

提供复用真实 symbol/literal 写入规则的成本通道：同一候选使用同一 CDF、上下文、符号与 literal 位数。可以累计固定精度 `-log2(p)` 为估算 bit，或在独立副本试写 range coder 并给出规范化 fractional tell 差值；实现方案在 M6.0 冻结，禁止在两种量纲间混加。

覆盖 partition、intra/inter/mode、MV、skip/EOB、系数头尾、level扩展位、符号与UV，不允许只用非零系数数量或只算 coeff。固定常数的帧头/OBU 开销可以在局部等长候选比较中剔除，但最终效果表必须统计完整实际码流字节。

**禁止把局部 `offs_after-offs_before` 或每候选各自 finalize 后的字节数当精确增量 rate。** range coder 有未输出比特、carry和终止开销。`-log2(p)` 是真实语法驱动的估计，不等于最终字节完全相等；报告使用的名称和误差。需要实际字节 oracle 时，只在隔离副本上重放相同上下文/共同后缀并 finalize，或比较完整小帧实际输出，不改变正式编码状态。

固定精度表/算法须跨编译器确定，CDF累计值到概率的解释须与当前 entropy API一致。零概率/非法符号拒绝，不悄悄给零成本。literal成本按真实 bit数计入；记录每类符号 cost，方便核查“少量非零但大level”不会被低估。

熵 checkpoint 若浅拷贝结构体，会共享 `buf/precarry`，必须单独管理缓冲有效区与指针，含 carry可能修改的先前位置。计量模式和正式写码流不得各自维护两份易漂移的语法分支。独立trace审计可单独实现，但生产rate须与真实writer共享语义。

## 独立决策验证

向 A 提供单元样例，测试 symbol/CDF上下文、literal、高幅coeff、skip/EOB、partition、MV与重放的对应关系。验证评分不改正式流；候选 A→B→A与干净A输出一致；rollback前后状态/有效缓冲hash相同；winner commit的coef/recon与评分一致。

小图 exhaust oracle从同一入状态列出所有合法候选组合、D/R/λ/J和胜者，独立核对最低J。pruned 必须是该全集的子集，记录漏掉最低J的次数和绝对/相对J差值，不能以“通常相同”代替统计。局部最低J不保证整个序列最终字节或VMAF最优，二者分开验收。

## 真实视频评测合同

现有128×128合成 cut/noise用于正确性和压力；保留它们，但新增普通视频压缩测试。至少选六段有合法使用来源的真实片段，覆盖静态访谈/人物、自然细纹理、相机移动、快速运动、低照噪声、动画/屏幕文字；至少三段以10-bit420为主要测量。可用已存在本地素材，缺失时补公开授权素材和依赖，记录来源/授权与SHA；不要用重新合成的噪声冒称真实视频。

每段建议5–10秒、统一逐帧数量与时间基、代表性720p/1080p；不为凑分把难样本删掉。标定片与最终六片测试集不得重合；QP集合、λ和剪枝阈值在正式测试开始前冻结。另设至少两段未用于λ/剪枝调参的holdout，最终单独列结果。保存原始素材hash、截取位置、完整转换命令、范围/色彩/像素格式、帧数与派生Y4M/RAW的hash。10-bit444若由420升采样，只证明该输入格式路径，必须标注，不声称新增真实色度信息；另保留原生444/图形专项。

编码比较固定相同输入、QP集合、结构、旧工具候选开关和无关参数。主要曲线为冻结M5和M6 `pruned`；小段/缩小分辨率另跑`exhaustive`对照并明确分辨率。首轮QP建议40/80/100/160/220，加1/255作为正确性端点；若质量区间不重叠，在主测试开始前对独立标定片选至少四个覆盖共同质量区间的QP并冻结。每条有效BD-rate曲线须至少四个有效点和合法共同质量区间；没有合法重叠或非单调时记N/A，照样展示原始点和机器可读原因，不插补伪造结论。编码/解码/指标失败必须另标FAIL，不能用N/A掩盖失败。

至少包括以下输出：

| 输出 | 内容 |
|---|---|
| 每点表 | 素材、版本/模式、格式/结构/QP、实际qindex、字节/码率、PSNR Y/U/V、VMAF、编码秒数/fps、峰值内存 |
| 曲线 | 横轴码率或体积，纵轴VMAF/PSNR；同片同设置连接QP点；另给编码时间—质量或体积—时间 |
| 多点对照 | 每片BD-rate/共同区间及N/A原因；汇总保留各片结果，不能只报最好片 |
| 剪枝报告 | exhaustive/pruned候选数、变换次数、漏胜次数/J损失、码率质量差和时间 |

VMAF固定模型和工具hash，参考/失真帧PTS与帧数一致，无静默缩放。依照M5.3既有合同明确转换到模型接受的像素格式；444额外保留原格式U/V PSNR，不用亮度VMAF代表色度质量。报告合成小图和真实片的适用范围，不承诺VMAF≥某绝对值或BD-rate改善固定百分比。

## 时间、资源与长测

质量批测可并发；速度测量必须单任务、固定环境，编码器内部threads1，测量不混入AVM解码/VMAF。至少三次，报告中位数与波动，区分API核心时间、CLI总时间（含读写/recon导出）与整条评测流水线耗时；不能将四任务并发的墙钟当单路速度。

复用M5.3断点/锁/manifest思路，新增M6配置、候选合同、程序、脚本、模型、素材hash；任何变化拒绝复用旧PASS。先跑一段1秒样片估算解码/VMAF和两种RD成本，记录预算后再冻结长测帧数；exhaustive限小分辨率/短片，不能直接按完整1080p全矩阵启动。清理策略先冻结：保留完整码流和JSON/log，recon逐帧校验后可按策略删除；磁盘预算包含素材、三次重复、平台重放和诊断副本，不恢复所有历史媒体。停止/异常/重启后必须可审计完成数，成功摘要最后写入。

B提交可复现命令、机器可读结果、模型/素材清单、测试限制及报告给00；不自行升级正式版本，不宣称M6 DONE。

</details>


<details>
<summary>042 · 03-M6-single-thread-compression-roadmap.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\03-M6-single-thread-compression-roadmap.md`  
原文件 SHA-256：`1fe4440a1ab2e3b028feae612af37f6dfd2f344d930e8c7fda700689e57b01b3`

# SVT-AV2 M6 后续技术开发总纲：先压缩竞争力，再多线程、preset、CRF

> **执行用文档** · 2026-10-18 · NuxBox1  
> **项目**：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2`  
> **文档**：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\M5-M6-AI-TASKS\03-M6-single-thread-compression-roadmap.md`  
> **主目标**：先开发真正有竞争力的**单线程 AV2 编码器**；以可复现的等质量 BD-rate 为首要退出条件。其后开发内部多线程；完成并行化之后，才制作有实际速度—质量差异的 preset；最后建立 CRF。  
> **属性**：新的技术规划与本地 AI 施工指令；不代表下列技术已经实现。不得覆盖现有冻结实验、程序、输出或正在运行的测试。

## 0. 给本地 AI 的首要执行协议

### 0.1 新的优先级覆盖旧里程碑排序

既有 `M5-M6-AI-TASKS/README.md` 将 M7 标为 preset、M8 标为内部并行、M9 标为 CRF；该排列与用户最新目标冲突。**本文件明确优先顺序：M6 单线程压缩能力扩展 → 竞争力验收 → 内部多线程 → preset → CRF。** 旧编号作为历史计划保留，新的正式版本号在 00 冻结新阶段合同后统一决定，不篡改旧报告。CRF 以前必须继续保证 CQP 1..255 功能稳定；不扩展 CBR、VBV、2-pass 或独立 VBR 产品路线。

**禁止**为了先做速度而重排上述主目标。此阶段所有算法基础以 `threads=1` 实现，允许独立的批量评测多进程，但不能冒充编码器自身多线程。

### 0.2 开发前必须重读的文件

从 `<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2` 阅读：

1. `AGENTS.MD`、`SVT-AV2-PLAN-AI.md`、`svt-av2/project.md` 最后章节。
2. `M5-M6-AI-TASKS/README.md`、`00-INTEGRATION.md`、`01-RD-CORE.md`、`02-RATE-VALIDATION.md`。
3. `svt-av2/docs/m6-rd-contracts.md`、`svt-av2/docs/basic-capability-contract.md`、`svt-av2/docs/basic-entropy-contract.md`。
4. `svt-av2/docs/task-reports/basic-capability-progress.md`、`M6-basic-capability-assessment.md`、`next-A-adaptive-rd-design.md`、`next-A-subpel-design.md`、`next-A-rd-baseline-audit.md`。
5. **唯一规范版本**：`docs/spec/AV2_Spec_v1.0.0_20260528_38f28e7.pdf`，对应固定 AVM v1.0.0 源与 decoder。正式规范定义解码/语法，AVM encoder 的算法启发式不能被当成规范强制条件。

旧 OpenCode 队列规则不能替代当前 00 主会话 + A/B 两个子智能体的开发组织。00 是唯一的最终验收与公共 API/CMake/版本文件写者；A/B 必须先冻结交叉接口与文件归属。**不允许两个 AI 同时写 `tile_writer.c`、`rd_frame.c`、公共 header 或其他共享文件。**

### 0.3 当前事实（2026-10-18 快照，开始开发前复核）

正式版本 `0.0.5-m5`；M6 的 RD 与 R5/R6/R7 是未全部正式发布的实验分支。当前支持：8-bit420、10-bit420/444，CQP 1..255（10-bit qindex = 用户 QP +48），AI 和单逻辑上一帧低延迟 P，Y DC/V/H、UV DC、64×64 与 4×32×32 的有限分区，部分实际 RD 和量化链。

- R5：±8 整数搜索保留多个 MV 进入真实 RD；已验证收益，但不足以解决整体压缩差距。
- R6：保留 R5 赢家，增加 ±16 有界外域，原生样本改善接近零；不是完整半像素或全域搜索。
- R7：帧内 CDF 自适应，目前首关只在 RD-off，**未**完整接入 RD 事务。相同重建下原生 2D/3D/Real 码流减少 6.99%～20.03% 的场景/点位变化，真实有价值但不能直接乘进旧 RD 收益。
- 半像素、多参考、递归分区、丰富 intra、成熟 RDOQ、环路滤波、B/层级 GOP、AV2 部分新工具尚未形成生产闭环。
- `Source/Lib/Codec/encoder.c` 当前 `threads>1` 显式拒绝；不是有隐藏的多线程优化。`sequence_header.c` 目前主要滤波能力（CDEF/GDF/restoration/CCSO）仍关闭。
- RD-on 在 2D 和 Real 的多 QP 曲线上甚至可显著劣于 RD-off，不能把“局部选出最小 J”当成全片压缩成功。
- 真实 24 帧 1080p 2D 单点：R5 QP100 为 562387 B、VMAF 92.948348；SVT-AV1 p6 CRF48 为 44321 B、VMAF 93.399432（约 12.7 倍、更差画质）。这不是全部场景的等质 BD-rate。另有完整 15 秒视频中超过八倍差距的直接支配点。

必须先检查外置 `<LOCAL_PRIVATE>\ffmpeg-native-m6-tests\campaign-local-r2-20261017\campaign-status.json`、`optimization-r6-20261018\b\joint-speed\result.json`、计划任务与真实 PID。旧速度窗口 `isolated-speed` 活跃时**禁止**新增编译、编码、解码、VMAF 工作；不得杀掉或重启原正在运行的批测。

## 0.4 2026-10-20 执行顺序变更：先完成单帧，再进入帧间

本轮最新有效合同见同目录的 [04-M6-intra-only-first.md](04-M6-intra-only-first.md)。它覆盖本总纲旧的“C1 率事务→C2 亚像素→C3 帧内”执行顺序；本文件后续仍保留原技术设计作为参考，但执行上先以 I-GATE 为硬门槛。

本机源码已经有 AV2 single-picture-header 码流生成路径，但当前帧内闭环主要是 Y 的 DC/V/H，UV 仍 DC。经检查，single-picture 及 multi-frame sequence header 两分支中的 DIP、intra edge filter、MRLS、CfL、MHCCP、IBP 开关当前都写 0；FSC、帧内 IDTX、帧内 IST、CCTX、TCQ 也未启用，生产变换路径仍标识为 DCT_DCT。它们构成了先提升单帧压缩率的直接技术空间。

AV2 v1.0.0 还定义了丰富的 directional/angle modes、DIP、MRLS、IBP、CfL、MHCCP、FSC/IDTX-intra、intra IST、CCTX、TCQ 和 parity hiding。single picture header 语法中不含 multi-frame 的 `enable_inter_ddt` 字段，因此 inter DDT 不能拿来做单帧收益证明。官方规范与 fixed AVM v1.0.0 见 https://av2.aomedia.org/v1.0.0/index.html 与 https://github.com/AOMediaCodec/avm/tree/v1.0.0。

**新的放行条件**：主基准改为 libaom 原生 `aomenc --ai --threads=1 --cpu-used=0 --end-usage=q --cq-level=<Q> --limit=1 --passes=1 --obu`，不经过 `avifenc` 或 AVIF 容器。相同画质下，AV2 raw OBU 的 holdout 几何均值须不超过 libaom raw OBU 的 85%（至少节省 15%），SVT-AV2 单线程吞吐须达到 libaom `cpu-used=0` 的 50% 以上（`T_AV2/T_AOM ≤ 2.0`）。另加入一组 SVT-AV1 still-image 曲线：固定单一偏压缩 preset，使用 all-intra + still-image IQ 参数，只跑一轮质量点，不做多 preset 网格或重复测速；它是辅助压缩参照，不取代 libaom 主门槛。测试集至少 50 张图/固定帧、至少 20 张独立 holdout、每条曲线至少 4 个共同质量点，冻结像素输入/色彩语义/程序版本及所有 hash。相同 Q 与 QP 数字不能当作同质量。

I-GATE 通过以前，禁止把亚像素、运动搜索、多参考、B 帧/GOP、时域前瞻列为 active implementation。失败就继续补帧内工具并按实际收益排序；不承诺 AV2 单图必然能省 15%，但也不能因为当前简单路径没竞争力就跳过 AV2 帧内技术。I-GATE 通过后应冻结单线程单帧基线，再进入帧间编码。

## 1. 技术目标和退出门槛

### 1.1 不能把 AV2 编码器的“合规”混同于“竞争力”

用五类诊断区分缺口：

| 根因 | 当前薄弱点 | 直接代价 |
|---|---|---|
| 时域预测 | 单参考 P、整数 ME、缺亚像素及 B/层级 GOP | 残差大、P 帧膨胀 |
| 空间预测 | Y 少数模式、UV DC、简单分区 | 复杂斜边/纹理难表示 |
| 变换与量化 | 变换候选少、缺完整 RDOQ | EOB、levels、符号昂贵 |
| 概率与 RD | CDF/真实 rate 与 trial 状态未完整统一 | 低 J 不等于最终小码流 |
| 重建参考 | 关键 loop 工具关闭 | 后续参考误差持续传播 |

旧 M6-R3 的三帧诊断里，估算系数相关语法占 R 的 96.56%/95.83%/77.47%；这一估计包含 skip/EOB/level 等，**不是**实际可直接节省的字节比例。完整 383 帧码流为 1 key + 382 P、流头很小，体积问题主要不在重复关键帧或容器。下一轮要分别测预测残差、已重建误差、系数成本与时间参考误差。

### 1.2 正确性门槛 C（每项必须满足）

- fixed AVM v1.0.0 独立解码完整码流并逐可见 sample 与编码器规范重建匹配。引入滤波后要明确比较滤波前/后，参考帧必须使用规范要求的重建版本。
- 8bit420、10bit420/444，各 QP、AI/P、后续 B，边界块/partial、变更模型、新旧开关、错误返回均可测试；未经批准不得把 444 转 420 冒充同格式。
- MSVC、GCC、Clang strict，ASan/UBSan/LSan、API 容量重试、EOS、双实例、1000 帧长链通过；不能用 Windows 全绿代替 Linux 全绿。
- 新功能默认 off 路径保持各自冻结祖先码流、重建、统计、错误语义指纹；输出改变时保留实验开关和旧版本，不能覆盖 frozen artifacts。
- 一个 RD 候选从相同入口反复顺序 A→B→A 时，完整 D/R/J、mode、MV、coeff、recon、邻居、CDF bank 不得串扰。

### 1.3 竞争力门槛 Q（建议工程指标，不是规范保证）

建立同源完整曲线：真实 1080p 原生 2D、3D、Real 的预先冻结 24 帧与完整约 15 秒版本，另至少六类真实题材（静态访谈、自然纹理、相机运动、快速运动、低照噪声、动画/文字），以及至少两个**不参加调参的 holdout**。每条有效曲线至少四个质量有交集的点，分别计算 VMAF BD-rate、PSNR-Y BD-rate，保留 U/V PSNR；没有公共区间为 N/A，失败为 FAIL。

先建立三道退出门槛：

- **Q1 消除数量级缺口**：同画质范围内，不再普遍出现前述八至十余倍的明显被支配案例；不拿单点体积比冒充 BD-rate。
- **Q2 接近成熟 AV1**：建议多数事前冻结真实视频的有效 VMAF BD-rate 相比**同结构 SVT-AV1** 不高于 **+25%**，无重要题材持续高于 **+100%**；这仅是开发退出阈值提案，远不是当前成绩。达不到就继续 M6-C，而不是宣布压缩能力完成。
- **Q3 真正追上/争取超越**：在广泛 holdout 上接近 BD-rate 持平，再争取负 BD-rate。SVT-AV1 RA 与当前低延迟 P 可作产品参照，但**LOW_DELAY_P 与 LOW_DELAY_P、RA 与 RA**必须分别评价技术贡献。

单线程慢可以接受，但完整 1080p 必须有界完成、不爆内存/磁盘。性能以真实吞吐、CPU 时间、峰值 RAM 记录，不能通过无限制搜索换来不可复现的压缩“最优”。

## 2. 分阶段依赖与必要技术总览

```text
I0   冻结 AV2 single-picture / libaom aomenc --ai 主基线、SVT-AV1 单 preset 辅助曲线、decoder、数据集与计时
 ↓
I1   扩展规范帧内模式 + UV 预测 + mode syntax
 ↓
I2   合法 intra partition/TX/coeff 路径
 ↓
I3   单帧 CDF/rate 事务 + intra RD/RDOQ
 ↓
I4   DIP / MRLS / IBP / CfL / MHCCP 等 AV2 帧内工具逐项验证
 ↓
I5   FSC/IDTX-intra / intra IST / CCTX / TCQ / parity hiding
 ↓
I6   帧内 loop filters 与单因素效果测量
 ↓
I-Q  单线程 still-image 竞争力门槛：
     相同质量 payload ratio ≤0.85，吞吐 ≥ AOM 单线程的 50%
 ↓ PASS
M6-INTER  亚像素、帧间模式、多参考、B 帧/GOP、时域 RD
 ↓
MT   内部多线程/任务图
 ↓
PRESET  真正有意义的速度—质量等级
 ↓
CRF  基于成熟 CQP/时域模型的一致质量模式
```

这里是新阶段的正式执行顺序，而不是“所有 C 项都完成再测”。**I-GATE 单帧门槛是开工帧间技术的硬条件**；先以单线程建立可竞争的 still-image/单帧编码，再扩展 motion、subpel、references 与 GOP。每阶段只改变一个主要候选域/变量并保存完整原始结果。若某个帧内工具质量收益不足或速度成本过高，应保留 off 并尝试下一个，不要机械完成清单；但不得在 I-GATE 前以帧间技术绕过单帧问题。

## 3. M6-C0：单线程诊断基础与真实比特成本基线

先对**已经提交的真实 winner**按帧/超块分组，仅在 profiling 模式输出：

- 帧：I/P/B 类型、真实 packet bytes、QP/qindex、VMAF、PSNR Y/U/V、参考刷新、有效 bitstream payload。
- 预测：intra/inter/mode/参考索引、MV 范围与搜索边界命中、预测残差 SAD/SATD/SSE、重建 SSE、运动补偿错误。
- 结构：partition 形状/深度、TX size/type、非零/零块比例、EOB、系数 level/sign 分布、裁剪候选数与最优 J 损失。
- R：partition/mode/MV/ref/skip/EOB/coeff/literal 的 Q12 成本、CDF 模型 ID 和状态；另外统计**整帧实际字节**，不把每个 SB 的局部 range coder 输出长度当精确可加字节。
- CPU：ME、predict、transform、quant、RD 试探、CDF bank copy、entropy formal writer、I/O 和峰值内存；分析三帧第一 P/长 P 的行为，不能从三帧外推全片。

把 `source-pred`、`source-recon_before_loop`、`source-recon_after_loop` 的差异严格分开。对“预测不好”和“预测已好但相同残差的码量昂贵”给出不同优先级建议。主要模块：`Encoder/{rd_frame,inter_frame,motion_estimation,transform_path}.c`、`Bitstream/tile_writer.c`、`EntropyCoding/{coeff_syntax,entropy_models}.c`。

## 4. M6-C1：概率自适应和 RD 状态事务（第一开发任务）

现状：R7 有规范 CDF update 且保持重建不变，但 RD trial 仍不能安全直接共享自适应率模型。先关闭所有其它新技术，保证这个最基础的一致性。

### 4.1 RD 数学与 rate 路径

遵守现有固定量纲：

- `R_Q12`：4096 单位/bit；`λ_Q8`：256 单位/λ。
- `D = round((4*SSE_Y + w*(SSE_U+SSE_V))/2^(2*(bit_depth-8)))`，420 时 `w=4`、444 时 `w=1`。
- `J = D + round(λ_Q8 * R_Q12 / 2^20)`，使用确定性无溢出整数运算。先核算真实变换量化表与 qindex，不按三段样片擅自调整 λ×8、λ÷16。
- adaptive CDF 的 raw 邻接边界可能相等，**不能**直接 raw 差值求 p；需要按固定 AVM 的 `get_adjusted_prob`/position padding 求真正概率，随后用现有 Q12 log 计价。`next-A-adaptive-rd-design.md` 已有 N=2..8 的模型公式、prob_inc 和回归边界。
- 正式 writer 与 rate-only 必须复用同一真实 symbol/coeff/MV/partition 语义，不得维护两份会漂移的语法。R 是估计 bit，不是 range coder 精确 flush 后字节数；对完整实际码流单独测量偏差。

### 4.2 恢复、隔离与赢家提交

```text
for each SB in raster order:
    entry = snapshot(recon_region, partition, neighbour_cells, motion_cells, entropy_bank)
    best = none
    for legal_candidate in frozen_deterministic_order:
        restore(entry)
        recon = predict_transform_quant_inverse_clip(candidate)
        R, post_bank = rate_replay_real_syntax(candidate, scratch_copy(entry.bank))
        D = visible_pixel_SSE(recon)
        J = D + lambda * R
        best = min_by(J, stable_candidate_id)
    restore(entry)
    replay_winner_and_compare(D, R, coeff, MV, path, recon, post_bank)
    commit_once(best)
formal_write_from_frozen_winner_decisions()
```

这是**语义约束伪代码，不是允许复制一份生产语法实现**。两个 bank：`committed` 保存已确认 SB，`scratch` 只用于当前 trial。恢复必须包括重建/邻居/MV/partition 游标、可变缓冲与 CDF 的 count/offset；`TileTree` 或 motion.cells 的浅拷贝不等于完整回滚。被淘汰候选不准改变正式 bank、参考画面或 range 输出缓冲。packet 容量失败/重试从明确状态重新生成，不重复决策 commit。

建议先只放开 `entropy-adapt=on` + 基础 `rd-mode`，并明确拒绝未验证的 coeff/motion 实验组合；再依次验收与 R5/R6/RDOQ 的组合。旧 off 冻结指纹完全保留。关键源码：`Source/Lib/EntropyCoding/{entropy_models,entropy_encoder,coeff_syntax,motion_syntax}.c`、`Bitstream/{tile_writer,keyframe,frame_header}.c`、`Encoder/rd_frame.c`、`Codec/encoder.c`。

验收：所有语义 CDF 行/符号、概率边界及 count15/16/31/32、失败中途恢复、双实例、A→B→A、逐 SB bank 完整比较、fixed AVM 独立重建；最终多 QP **RD-on vs RD-off** 实测曲线。旧 RD-on 可能更差，必须保留数据，不得把局部最低 J 当全片最低码率。

## 5. M6-C2：亚像素补偿、运动信令与高精度 MV 搜索

目前 `motion_estimation.c` 的整数搜索不足以处理真实视频中普遍存在的亚像素运动。不能“直接增加搜索步数”或将现有整数 MV 用小数解释。

### 5.1 固定 half-pel 最小闭环

参照 `docs/task-reports/next-A-subpel-design.md`，先维持**单参考、帧固定 half-pel 精度、固定 BILINEAR、±16 物理坐标范围、保留旧整数路径**。

- 新代码用显式 Q3 MV（单位 1/8 像素）类型，整数坐标转换为 Q3 时乘 8，half-pel 的相位为 4 Q3；所有 predictor/delta、motion cells、候选 ID、hash、replay 必须同单位。旧整数函数签名不默默改变。
- 420 色度的 quarter/three-quarter 相位必须完整支持：半亮度像素位移对应色度 1/4，相位 0/4/8/12（plane Q4）。444 采用亮度对应 phase。所有负值使用数学 floor，不能把 C 的向零取整用于负坐标。
- 输出序列/帧头的精度信令必须在 fixed AVM v1.0.0 对应语法下闭合。原 ONE_PEL/set0 模型不能直接用于 HALF；shell set1、对应 class CDF、offset bit6、训练 rate 及 entropy model family 同时接入。
- 固定 BILINEAR 首关就要逐采样与 AVM 的插值、两级 round0/round1、UMV border / edge extend 一致，不得用“先横向求整数均值再纵向求整数均值”代替规范二维卷积。
- 先给整数赢家附近 8 个半像素邻居进行便宜预选，再让最多 2 个进入固定树/模式的完整 D/R/J；所有 half 候选要从同一入口重新推导残差、qcoeff、重建。输出真 MV 与真 coefficients，不能只把它写入统计。
- 新路径关闭时原 R5/R6、R7 各自冻结码流应完全不变；启用 half 后不得将旧 integer 模式的 R/J 直接作 half 域下界，因为 frame precision 及 CDF 成本已改变。

### 5.2 先功能正确，随后搜索质量优化

half-pel 通过后再做更丰富的 subpel（例如 quarter / eighth）、EIGHTTAP、EIGHTTAP_SMOOTH、EIGHTTAP_SHARP、switchable interpolation、滤波器 RD。必须逐个检查合法精度与 filter syntax，不直接照抄 AV1 的 allow_high_precision 规则。

ME 预算可按“低分辨率金字塔 → coarse SAD/SATD → diamond/hex 局部细化 → 最终像素域 D+真实 MV/coeff rate”实现。把零 MV、当前块空间邻居和历史 predictor 合并、稳定排序去重，比较固定搜索预算与 brute-force 小图 oracle 的漏胜率。扩大运动范围是否划算必须看完整多 QP 曲线，R6 的近零收益已经说明“范围更广”本身不等于“压缩更好”。

**改动锚点**：`Source/Lib/Prediction/inter_prediction.c`、`Encoder/motion_estimation.c`、`Encoder/inter_frame.c`、`EntropyCoding/motion_syntax.c`、`Bitstream/{sequence_header,frame_header}.c`、`Encoder/rd_frame.c`。

**验收**：8/10-bit 420/444、水平/垂直/双轴和正负 half-phase、UMV 边界、极端 qindex、MV shell set1、固定 AVM 逐 sample；旧 off 完整 hash、new half 实际码流与 CDF 更新、真实多 QP 质量和单线程成本。不得用平坦帧的 SAD 一致代替插值正确性。

## 6. M6-C3：扩展帧内预测，尤其是 UV 预测

当前 Y=DC/V/H、UV=DC 对斜边、光滑渐变、复杂动画/文本的局部预测不足。建议**分批逐个合法模式**增加，不一次打开所有 mode：

1. 首批：PAETH、SMOOTH、SMOOTH_V、SMOOTH_H 等 v1.0.0 中已定义的基础模式。读取 decoder 的邻居可用性和整数舍入，覆盖左/上不可用、边缘扩展与尺寸限制。
2. 第二批：其它 directional intra angle、角度偏移及其符号上下文。先对固定角度逐像素差分，再打通真实 bitstream mode set/index、上下文 CDF、size restrictions 与 RD。合法模式的集合由实际规范决定，不根据枚举数字猜测。
3. UV：从 DC 扩展到合法方向/平滑模式，再实现 CfL。CfL 要使用已重建的 Y、正确 subsampling/downsample 和 alpha 参数；在 420/444 的适用条件、语法成本与重建精度都要校验，不能套用在规范禁止的模式上。
4. 后续可试 AV2 的 multiple reference lines（MRL/MRLS）、data-driven intra prediction 等，前提是完整规范、概率模型和 fixed AVM 可独立验收。

**候选决策**必须完整运行 predict→residual→transform→quant→inverse→clip→真实 mode/coeff rate→`J`；不以最小 Y-SAD 直接宣称全片最优。分区内后续叶的邻居取决于先前叶的重建，不能把预测结果按 (leaf,mode) 简单缓存而无视上下文。

**改动锚点**：`Prediction/intra_prediction.c`、`Encoder/intra_frame.c`、`Encoder/rd_frame.c`、`EntropyCoding/block_writer.c`、`Bitstream/tile_writer.c`。

**验收**：对水平、垂直、斜线、平坦、渐变、屏幕文字、复杂纹理、噪声及 HDR/SDR 保持完整 YUV 输入，用 fixed AVM 确认 primitive 与整帧 sample；输出模式命中率/残差 EOB/模式总 R 与多 QP BD-rate。不能用某模式“经常被选中”冒充节省了多少码率。

## 7. M6-C4：规范合法的递归分区和树级 RD

现有完整 64×64 SB 只在原树与 4×32×32 间选。下一步要支持 AV2 **规范定义**的可用 split 类型和叶尺寸，不是任意四分递归：

- 首先独立实现合法几何枚举器：画面边缘/partial、split 方向、块尺寸上下限、Y/UV subsampling、合法 transform/block/partition 组合。固定 AVM 解析语法证明每一类新增 split 有正确编码与重建。
- 先固定一个新增合法 split 类型，完成写码流→独立解码→逐叶系数与画面匹配，再扩大候选集合；不可同时改 motion/intra/transform 而不单因素对照。
- 真正的树级 RD 必须考虑 split 符号、每叶 mode/coeff/ref/MV 以及 **后续兄弟叶能看到的 reconstruction 和 entropy context**。一般情况下不能把没有状态的逐子树 DP 和相加 J 直接冒充全局树最优。
- 允许使用受限 beam search、prefix-state 事务与剪枝，但 beam 候选必须从相同入状态恢复和提交。保留小图 exhaustive 真值集；记录剪掉更低 J 的次数、相对损失、候选总数、内存与 BD-rate。
- 原 M6 合同中 partial SB 目前只走固定合法单路径。扩展时须单独证明 partial 语法，不能把完整 SB 的决策/缓冲条件照搬到末行和最右列。

**锚点**：`Common/block_geometry.c`、`Bitstream/tile_writer.c`、`EntropyCoding/block_writer.c`、`Encoder/{rd_frame,intra_frame,inter_frame}.c`。注意 tile writer 的 motion/neighbour bank 深拷贝以及 writer 游标回滚；default-off byte/recon 不变。

## 8. M6-C5：变换选择与 AV2 变换工具

当前的有限 DCT 组合不足以集中不同方向和颜色关联的 residual。建议先建立**可合法选择 TX 类型与尺寸**的基础层，然后再接 AV2 高级变换：

1. 核对 v1.0.0 中当前预测模式、块大小、reduced_tx_set、lossless/QP、frame/sequence flags 下真正合法的 DCT/DST 变换及矩阵尺寸；先在固定 mode/tree 上测试小集合 TX-RD。
2. 对每个 TX 候选计算实际 forward、quant、dequant、inverse、clip，像素域 SSE 与真实 coeff syntax R。变换域误差可以用于预筛，但在交叉项、整数舍入、clip 下不得直接替代最后重建 D。
3. 正确处理 TX64 的规范 coded grid 高频截断：packed 系数 1024 不能被错误解释成 4096 个可写元素。四 TX32 的可表示残差域和语法不一样，要做合法对照，不要非法“补上”被标准省去的位置。
4. 基础正确后探索 AV2 的 DDT、IST（intra/inter secondary transforms）、MDTX、改进 DCT/DST、CCTX、大色度 transform。每个工具单独实现“启用语法→forward/inverse→EOB/scan→正式系数语法→RD→独立 decoder 验收”；CCTX 要关注 U/V 依赖和逆变换顺序，不能当单平面的额外 enum。
5. 搜索预算先“固定已选 tree/mode → 少量 TX 候选 → 把赢家回送 mode/tree RD”，受控小块 exhaustive 用于查剪枝损失，不能把全尺寸 TX×split×MV×mode 全穷举。

**锚点**：`Transform/transform.c`、`Encoder/transform_path.c`、`Quantization/quantization.c`、`Reconstruction/reconstruction.c`、`EntropyCoding/coeff_syntax.c`、`Bitstream/{frame_header,tile_writer}.c`。必须保留完整位深、量化尺度、scan、padding/overflow 错误回归。

## 9. M6-C6：真正的 RDOQ / Trellis 与系数概率模型

前述 R4 的尾部 AC 减一级实验只能叫最小系数优化，**不是**完整 RDOQ；粗暴平面全零曾产生巨大的 BD-rate 退步。下一阶段：

- 基于已经冻结的 mode、tree、TX 产生初始 qcoeff/scan/EOB，按合法范围尝试 level 0 / q±1 / EOB 缩短/尾部稀疏化；不越过有符号 level、bitdepth、TX64 packing 和 decoder 约束。
- 单候选 R 要包含 skip/EOB、level/sign、上下文依赖、扩展符号、系数符号等全部真实语法；不能仅按非零数或绝对值大小估算 bits。
- Trellis 的状态需体现 EOB、已走过 scan 的上下文/邻居/可用 CDF 等真正依赖。只有状态马尔可夫性被证明才可用精确 DP；否则用有限 beam /局部扰动 + 完整重放和比较，不能把互相关联的系数优化成独立可加问题。
- 真正 D 为 quant→inverse transform→重建 clip 后的可见像素 SSE，不能假定 `sum((c-c')²)` 无条件等价；额外 chroma 变换会放大这种差异。
- 记录扫描末尾位置/level 直方图、EOB 变化、符号成本误差、候选/变换次数、相同入口最小 J、完整流实际字节与真实 BD-rate。不同 QP/8/10bit、强纹理、稀疏噪声/黑白极值、全零与单非零必须覆盖。

**锚点**：`Quantization/quantization.c`、`Transform/transform.c`、`EntropyCoding/coeff_syntax.c`、`Encoder/rd_frame.c`。对 RDOQ-on/off 必须多点真实等质对照；只在极低质量下获益的方案先默认 off，不能据某个 small/P 单点直接正式发布。

## 10. M6-C7：多参考 P、运动矢量预测与参考生命周期

半像素和基础 intra/partition/RD 可靠之后再扩参考系统：

1. **先两个短期参考的 P**：正确 DPB/frame ID/order hint/refresh/lifetime，避免引用未完整重建或已失效帧。ref-index 的规范语法成本进入 mode-RD；不能用视觉最低 SAD 的参考直接当赢家。
2. **建立空间与时间 MV predictors**，NEAR/NEWMV 候选、邻居 motion map、参考缩放/插值相位和对应 CDF；同一候选的实际 MV difference 与 coefficient bits 都要进入 J。临时候选不得污染下一个块的 MV bank。
3. **扩展到更多合法参考**，每步测试固定引用顺序、参考寿命、帧尾 flush 与 1000 帧资源上界。数据结构保证正式编码和 fixed AVM decoder 使用相同 reference slot 语义。
4. **SCD 源图检测**：低分辨率差异 + 分块亮度/颜色直方图 + 运动补偿误差比例 +最小间隔，防止闪光/字幕导致反复 key。插 key 与保留 P 的总体成本应结合多帧，而不是看到转场就强制插帧。
5. **时间依赖成本**：逐 SB 的局部最低 J 不保证最小序列码率；分析参考质量传播，逐步研究有限 lookahead / dependency weights /跨帧代价，而不是把相同帧 J 简单求和。

**锚点**：`Encoder/{reference_frame,inter_frame,motion_estimation,rd_frame}.c`、`Prediction/inter_prediction.c`、`Bitstream/{frame_header,sequence_header}.c`、`EntropyCoding/motion_syntax.c`。新增工具关闭要保持原 P bitstream SHA；明确多参考路径不支持时拒绝，不能 silent fallback。

## 11. M6-C8：环路滤波、像素一致性和参考质量

当前帧头路径将 deblocking 置为关闭，sequence header 也禁用了 CDEF、GDF、restoration、CCSO 等滤波能力；应逐个建立**编码器重建参考 = 固定 AVM 规范解码重建**的闭环，而不仅是播放器画面美化。

按固定规范合法 pipeline 顺序单独接：

1. **Deblocking**：帧头参数/边缘强度、块/Tile 边界、不同位深的阈值/舍入，映射到正式重建 buffer；严格按规范确定启用条件。
2. **CDEF / GDF**：分别解除对应 sequence/frame flags，先强制一个合法固定模式、完整逐 sample decoder 对照，再以有界 RD 选择强度/状态；不得假设 AV1 和 AV2 的同名函数精确等价。
3. **Restoration / CCSO**：region/plane 的实际语法、参数、滤波前输入/滤波后输出、色度/subsampling 条件；连同滤波 flag 和 payload 的 bit cost 一起算，不把参考失真下降误当“完全免费”。
4. **Reference 净化**：编码器用于下一帧的参考画面必须是 decoder 将使用的相同规范滤波结果。`--export-recon` 若只导出 filter 前重建，独立比对需明确改用正确阶段，禁止把不同阶段 hash 作错误报警。

先用可复现标量参考实现，边缘、10bit 极值、非平坦画面、文字/高频/噪声逐 sample 核验，再比较多帧 residual 的降低与完整质量曲线。若 PSNR-Y 与 VMAF 方向不一致，全部报告，不选最有利指标宣称通过。

## 12. M6-C9：B 帧、层级 GOP、前瞻与真正的时间 RD

**没有 B 帧不等于没有 AV2，然而若长期只有单参考低延迟 P，就不能指望在常规 RA 压缩任务里直接达到成熟 SVT-AV1 的效率。** B 帧和时域 GOP 完全可以先用一个线程按依赖顺序实现，不能把它等同于多线程。

分五关开发：

- **GOP-A 多参考前置**：先通过 C7，明确合法参考帧列表、帧 ID、时间位置、可用性和重建刷新。
- **GOP-B 最简单双向 B**：一层非参考 B、双向预测与允许的 compound syntax；严格区别 display order 与 decode order，构建 packet PTS/DTS、输入缓存、EOS flush 和参考生存期。首版只允许固定 GOP 模板。
- **GOP-C 分层可参考 B**：由固定深度2→3，按参考可用性决定编码顺序与输出顺序。测试异步读取源帧/重建和尾部不完整 GOP 的正确性，避免循环引用与帧泄漏。
- **GOP-D 前瞻与关键帧/转场**：短窗口下采样、运动量、噪声、局部差异/直方图判断 scene cut；实际比较 key 与 inter 的语法成本和后续参考收益。对 flash、字幕、推镜、长静态段设置可解释条件与最小/最大间距，不一律大差异就插 key。
- **GOP-E 传播代价**：单帧 `J` 最小不能保证序列最优；先记录被参考概率与误差传播的受控反事实，再研究小窗 TPL/temporal dependency、frame/temporal-layer 权重和帧级 QP/λ 配置。禁止用“各帧局部 J 求和”声称全局最优。

对于帧间高级工具，如 multi-hypothesis、masked compound、interintra、warped motion、global motion、TIP 等，**只按 AV2 v1.0.0 合法性和已实现依赖**选择，逐项验收。不是任何 AV1 同名工具都可直接复用代码、信令或默认值。

**验收**：小流 I/P/B 与参考 B 的固定解码时序、PTS/DTS、引用更新、order hint、1000 帧、首尾关键帧、场景切换、大小缓冲重试和显示帧哈希；对比 LOW_DELAY_P 对 LOW_DELAY_P 及 RA 对 RA 的多 QP 曲线。不能在只有单帧先验时就得出“B 帧普遍省 N%”的结论。

## 13. M6-C10：AV2 专有编码工具的选择顺序

基础 C1–C9 与新的可复现压缩曲线建立之后，优先考虑**标准 v1.0.0 确实存在、可达、可被固定 AVM 验证且有预期收益**的特性，而不是以加入的名称数量衡量进展。

| 工具候选 | 依赖 | 最小落地闭环 | 重点风险 |
|---|---|---|---|
| 扩展 DCT/DST、DDT、IST、MDTX | C5/C6 | 合法 TX set → forward/quant/inverse → coeff syntax → RD | 整数核、size/scan/位深 |
| CCTX、大色度 TX | C5/C6，色度数据正确 | 跨 U/V 残差建模与逆变换、逐像素 AVM | chroma dependency、420/444 分歧 |
| CfL、MRLS、数据驱动 intra | C3/C4 | mode/参数语法 → 预测 → 残差 → rate → decode | 模式准入、邻居取样 |
| 更先进的 MV/滤波与运动模型 | C2/C7 | frame flag+motion CDF+subpel/filter → ref pixels | 单位、上下文、参考坐标 |
| 帧间复合预测/时间模型 | C7/C9 | reference list → prediction → compound coeff → syntax | 生命周期、重新排序与依赖 |
| CDEF/GDF/restoration/CCSO | C8 | 规范 filter 与参数语法 → reconstructed reference | 输出/参考顺序、bit cost |
| 屏幕内容（IBC、palette 等） | C3/C4/C6 | 严格合法的 block reference 或调色板模型 | 当前帧引用、symbol context |

AV2 变换领域可参考 2026 年论文对 DDT/IST/CCTX/MDTX 的概述，但论文不能替代规范。每个新工具必须交付“enable flags → 合法语法 → fixed AVM decoder → 编码器 forward/inverse → rate → RD → 基线比较”的证据链；不照旧 AV2 working draft/v13 猜 v1.0.0 位布局。

## 14. M6-Q 前只做服务质量的单线程性能优化

编码器没有线程架构时，速度可能不如技术更多但经过充分 SIMD、缓存和调度优化的 AV1/H.264；这不意味着应先追求多线程。

允许小规模**等码流**优化：单次完整 winner 编码的阶段 profiler，避免无谓的重算和 heap allocation，预分配 scratch/变换 buffer；只缓存不依赖已重建前缀、模型和参考状态的中间值；单独优化 SAD、SATD、DCT/IDCT、量化和 residual 计算。

先保留可审计的 scalar reference，然后对实际 profiler 显示高比例计算热点加入 SSE/AVX2；SIMD 必须在所有合法输入下与标量实际编码**逐字节和逐 sample 一致**，包括整数溢出、舍入、MV 平局和跨平台确定性。严禁为了缓存省略 CDF bank/neighbor/motion 的事务恢复；完整 bank copy 有真实性能成本，但首先是正确性要求。

保留小分辨率 exhaustive/oracle 用于排除剪枝失效；真实 1080p 的候选预算必须有上界，支持“少计算换相近 BD-rate”的科学 Pareto 评估，不允许把无界穷举叫“慢 preset”或“能力完整”。

## 15. 单线程压缩竞争力最终冻结门槛（M6-Q）

在进入线程架构之前，00 必须正式冻结：

1. 当前支持格式与低延迟/RA 路径的规范合法性、独立解码全重建、严格跨平台/sanitizer、长链/1000 帧/EOS/异常测试，全部 PASS。
2. 与 SVT-AV1 同一帧序列、同等质量区间的 VMAF/PSNR 曲线、总 bytes 和各片负面结果；至少 Q2 满足时方可标注“有竞争力的单线程版本”。未达到则继续缺口最大技术，不因已有十个功能就进入多线程。
3. 所有默认启用的工具均有单因素和组合的质量贡献与耗时报告；尚不稳定的分支默认 off。Q3（企图 AV1 持平/优于）作为进一步的目标，而非伪造已成功门槛。
4. 单线程速度与峰值 RAM 的可复现报告、完整场景/holdout、冻结源码/程序/模型 hash。至少可让本地 AI 从当前稳定版本重新跑出同结果。
5. 不存在公开 no-op 参数。`threads=1` 的 golden path 被保留，未来所有并行设计以它为 correctness、BD-rate、性能基线。

## 16. MT：完成压缩能力后才构建内部多线程

### 16.1 架构先于线程数量

- 将 frame 显示/解码顺序、DPB/reference lifetime、输入帧所有权、SB/recon 依赖、上/左/右上邻居、熵上下文、滤波依赖和 packet 序列化画成任务 DAG。
- 保持一个完整 `threads=1` 的 golden 途径；`threads=2/4/8/16` 的主要任务要么以相同决策路径得到相同码流，要么将不同 tile/context 导致的真实质量差量化公开。
- 按可独立性渐进：先并行纯候选 ME/预测 → 前瞻/预分析与最终写入流水线 → 可用的帧间任务 → 规范允许独立的 tile → 在实际依赖满足后研究 row/wavefront/CTU 并行。
- 不能多个线程同时无序操作一个共享有状态 range coder，也不能越过尚未重建/滤波完成的参考。候选并行可以在局部私有 scratch 评估，再有序 winner commit。
- ThreadPool 有有界队列、stop/cancel、失败清理、双实例独立性、EOS/短流长链、内存上限；对不同核心数自动调度优于硬编码固定 CPU 亲和性。Windows 大小核/P核/E核和双 CCD 策略**只有在具体机器有此硬件并有 profiling 证据时**才专项配置。

### 16.2 不承诺 16 倍加速

NuxBox1 8C16T，SMT 逻辑线程不等于 16 个物理核心；预测/熵提交的串行部分、内存带宽和同步使加速受限。至少各运行三次独立单进程速度样本、含进程 CPU /墙时 /吞吐 /内存，并记录：

`speedup(N) = T(1)/T(N)`；`efficiency(N) = speedup(N)/N`。

同时输出每版是否 bit-exact、VMAF/PSNR BD-rate 的变化、每线程空转和串行瓶颈。线程分配不能以更高吞吐为由掩盖压缩率下降。

## 17. PRESET：真实算法预算驱动的性能等级

只在已经具备高质量编码器及多线程后建立 preset，且每一级必须真实改变至少部分：

- ME 层次/搜索点/亚像素 refine 次数、参考数与候选 MV、完整 RD 候选预算；
- directional intra/CfL/partition 树深、beam 宽度；
- TX 类型与 RDOQ 搜索次数、entropy/RD 高精度估计；
- GOP lookahead 深度、参考与 temporal 分析；
- 是否启用已验证的高级工具，以及线程/任务调度开销。

可以先有 fast/balanced/slow/exhaustive-debug 四档，通过实际三次速度与全片 BD-rate 证明速度—质量有意义；未来再映射 `p0/p4/p8/p12` 等产品参数，但**不能为对齐旧 AV1 编号而假定用户数字与旧编码器有相同计算含义**。

慢档扩大搜索候选，不保证整段 VMAF BD-rate 数学上严格单调，因为决策还会改变后续参考/模型。反向结果需要记录原因、修复策略或诚实标注；绝不把 no-op preset 暴露为有意义的编码设置。

## 18. CRF：完成 CQP/时域率失真模型后建立一致质量模式

CRF 不是改写 QP 名称。前置条件是 8/10bit、420/444 的 QP→量化步长→质量响应已验证，参考链、GOP、BD-rate、真实 RD 和每帧统计已可信。

1. 从目标 CRF 映射基准 QP/qindex，再根据空间复杂度、运动、reference importance、时间层级和帧型调整 frame QP；不能让“同 CRF 等于每帧恒定 QP”成为最终实现。
2. 每帧 QP 改变必须正确更新 quant lookup、λ、RD 与 entropy rate、色度 offsets；QP 稳定不代表感知质量恒定，不能宣称不同题材有完全相同 VMAF。
3. 用场景切换/闪光/暗噪声/HDR/动画/文字及 holdout 测量质量稳定性、异常帧、平均/分位 VMAF、PSNR-Y/U/V、码率分布；固定多个 CRF 点验证整体质量/码率方向与可复现性。
4. 单一平均 VMAF 可能掩盖非常差的几帧，应额外检查尾部百分位和时域波动。CRF 优化不等同于 VBV/CBR/2-pass，不自行加入用户未要求的码率约束模式。

## 19. 00 主会话 + 两个子智能体的执行组织

每个子阶段只冻结一次输入/规范/开关/预算，避免多个模型同时修改相同编码器上下文。默认由 00 管项目状态和公共接线，A 写算法核心，B 写实际语法与独立验证；**这是职责划分模板，具体文件 owner 每阶段单独冻结**。

| 角色 | 主要工作 | 不可越权 |
|---|---|---|
| 00 / Integrator | 冻结源/AVM/数据 SHA、API/CLI、CMake、模块接线、最终构建与独立验收、版本和对外文档 | 不覆盖已有正式/外置冻结程序或运行中的原批次 |
| A / Encoder core | Prediction、motion、RD、Transform、Quantization、Reference、Loop 核心之一的本期唯一属主，独立算法/错误机制测试 | 不同时修改 B 正在写的 entropy/bitstream 和公共头 |
| B / Syntax+Rate+Validation | EntropyCoding、Bitstream 唯一属主、规范语法/独立 AVM trace、完整 2D/3D/Real/holdout 评测、speed 协议 | 不同时修改 A 正在写的核心路径和公共头 |

跨模块如 `inter_prediction.c` 与 `motion_syntax.c` 需要接口时，00 先冻结 MV Q3 精度、类型、合法 range、序列/帧信令、entropy family、差分验收、唯一文件写者后再派工。如果无法无重叠地拆分，改为 A/B 串行编写，另一人只读验证。

本地 AI 在每个阶段新建独立快照目录（不得覆盖既有优化目录），保存：

- `SPEC.md`：v1.0.0 规范精确条款、fixed AVM 对应入口、可达条件、必要 header、错误回退；
- `IMPLEMENTATION.md`：变更文件/函数、精简 diff、状态生命周期、真实开关（默认 off）、可复用 baseline；
- `TESTS.json`：独立 primitive oracle、真实完整码流、固定 AVM 逐样本、平台与 sanitizer/错误重试结果及指纹；
- `RD-EFFECT.json`：全部 QP/素材/模式与字节、PSNR、VMAF、BD-rate、耗时、峰值 RAM，含不好看的失败/负收益；
- `INTEGRATION.md`：00 逐项签收、保留风险、受控开启或继续 off 的决定、下一阶段入口和冻结 SHA。

**只有 00 在当前唯一数据/代码版本下完成独立复验，才准写 DONE**。编译通过只是代码构件状态；单测通过只是机制状态；固定 AVM 正确性通过也不等于真实压缩效果通过。

## 20. 统一测试矩阵、性能协议与失败处理

### 20.1 正确性矩阵

覆盖 8bit420、10bit420、10bit444；64×64、128×128、176×144（非整 SB）、720p、1080p；QP 1、40、80、100、120、140、220、255，以及具体变换/符号的合法边界。1/2/3/17/1000 连续帧，静止/随机/棋盘/梯度/平移/斜边/强噪声、场景切换和各种合法 chroma phase。

- 包含部分 SB、上/左/右上邻居不可用、负 MV、末行/末列、最大/最小位深取值、coeff sign 与 EOB 异常、模型 count 临界、全零、满量化；
- 所有开启路径由固定 AVM v1.0.0 独立完整解码，比对编码器按**相同规范 stage**的真实重建；出现新 Loop 或 B 帧时重新设计 oracle frame order，不硬套旧 `export-recon` 位置；
- 对每个 trial 添加 A→B→A/顺序变换/故障中断、state hash、浅拷贝共享 buffer 污染与容量不足重试；验证 winner 不会因另一个候选被试过而变化；
- Windows MSVC `/W4 /WX`、Linux GCC/Clang `-Wall -Wextra -Wpedantic -Wshadow -Werror` 与 ASan/UBSan/LSan，确认最终实际 executable 晚于源文件，不受陈旧 obj/target 误判影响；
- 长测启动前先 1–2 帧实验、小尺寸断言和 bounded timeout。旧 M5/R4/R5/R6/R7 的 frozen 指纹对照必须只读运行。

### 20.2 效果和外部对照

对每种模式冻结真实视频及完整派生链：原视频的 hash、剪辑位置/帧索引、ffmpeg 版本、Y4M/RAW hash、fps/位深/色度、显示及编码顺序、原生宽高、全部参数。24 帧用于快速验证，15 秒/1000 帧用于参考传播/GOP 的有效评价。

至少四个真实质量重叠点，BD-rate 计算用事前固定方法并提供线性 log-rate 敏感性检查；无合法共同区间则 N/A 与机器可读原因，失败记 FAIL。含 PSNR-Y/U/V 和固定模型的 VMAF；若二者方向不同不得只引用更好的一列。需要时对 444 另做原生 444 而非 420 升采样替代。

- 对旧 RD-off/最佳已有 RD 比较**新增工具的单因素影响**，新增两个工具组合要单独测，不把 A 节省 10% 与 B 节省 10% 相乘冒充 19%；
- AV1 基线为相同逐帧输入和同结构、在相近质量区间的多 CRF 点；固定 SVT-AV1 参数、lp1、真实 bitstream packet bytes。可额外用 x264 slow/veryslow 作 H.264 对照，不能未经本机真实编码就说“打败 H.264”；
- 不得用原始 H.264 素材本身的压缩大小与当前重新压制 AV2 文件比；比较对象必须同源重新编码。
- 真实 1080p 字节比、同画质支配案例、合法 BD-rate 是三个不同指标，报告必须分别命名。
- 计时隔离：单编码进程，不导出 recon、不包含 AVM decode/VMAF；每个程序每源暖机、重复至少三次、交叉顺序，统计中位数与全部样本/范围、CPU time、wall time、RSS、背景负载。原冻结长测继续时可声明是**有背景负载的配对计时**，绝不能叫独占整机环境。

### 20.3 输出数据格式与恢复

每个可恢复测试 case 至少记录机器字段：

```json
{
  "stage": "M6-Cx",
  "status": "PENDING|RUNNING|PASS|FAIL",
  "source_manifest_sha256": "sha256-placeholder",
  "encoder_sha256": "sha256-placeholder",
  "oracle_spec": "AV2 v1.0.0",
  "oracle_decoder_sha256": "sha256-placeholder",
  "input_sha256": "sha256-placeholder",
  "derived_y4m_sha256": "sha256-placeholder",
  "pixel_format": "yuv420p10le",
  "frames_start": 48,
  "frames_count": 24,
  "fps": "24000/1001",
  "structure": "low-delay-p",
  "qp_requested": 100,
  "qindex_actual": 148,
  "tool_flags": {},
  "stream_bytes": null,
  "stream_sha256": null,
  "reconstruction_sha256": null,
  "independent_decoder_match": null,
  "psnr_yuv": null,
  "vmaf": null,
  "encode_wall_seconds": null,
  "process_cpu_seconds": null,
  "peak_rss_bytes": null,
  "reason": "未执行时字段保持null，不能填伪造PASS"
}
```

以上仅是格式示意而非真实结果。批测每 case **原子写完成状态**，controller 有 stage index、完成数、失败数、PID、锁、run ID、resume 清单。结果被复用时验证媒体/程序/参数/model/脚本全部 SHA；前一轮 PASS 不能在参数改变后继续记 PASS。

### 20.4 磁盘、长测与正在运行任务保护

- 当前开发树是 `<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2`，新大型试验放 `<LOCAL_PRIVATE>\ffmpeg-native-m6-tests\optimization-Cx-YYYYMMDD`，**不要覆盖** `optimization-r5/r6/r7-20261018`、`campaign-local-r2-20261017`。
- 仓库本身当前**没有 Git 初始化**，恢复靠完整源码/程序快照、差异补丁、SHA manifest 和目标文件时间。不得声称已有 Git 分支/PR。
- 稳定复用全部原媒体；旧媒体离线归档按 `<LOCAL_PRIVATE>\ffmpeg-native\WORKSPACE-CLEANUP.md` 查找，不将已归档媒体误判为缺失回归。大体积 recon 校验后可按既定策略清理，但保留完整码流、指标、hash、日志与恢复办法。
- 先小图/1 秒估算预算，再 24 帧/4 QP、真实 15 秒，最后 1000 帧/多平台；不在 1080p 上无界 exhaustive。原工作区约 30GB 的管理要求继续有效，大媒体外置保存。
- 任何 `isolated-speed` 期间，不编译或抢跑质量/解码工作；不能杀掉既有进程，不能未经状态判断重复启动计划任务。新阶段需要计算时先由 00 读取状态并选择不冲突窗口。

## 21. 本地 AI 下一轮第一批工单（可直接执行的任务拆解）

**此次写文档没有执行下面的编码任务。下一次本地 AI 应直接从这些工单开始，而不是先制作 preset/CRF。**

| 工单 | Owner | 核心产物 | 最小通过条件 |
|---|---|---|---|
| C0-00 | 00 | 当日进程/运行阶段/源版本/基线 hash 审计，冻结新 C0 合同及文件 owner | 不影响旧长测，报告确切 SHA 与当前 RUNNING/PASS |
| C0-B | B | 同源视频诊断 schema、AV1 参照、R5/R6/R7 与旧 RD-off 多 QP 曲线 | 保存全部坏点、完整 payload /PSNR/VMAF |
| C1-A1 | A | 按现有 `next-A-adaptive-rd-design.md` 实现 adjusted probability Q12 rate 与规范模型差分测试 | 形式概率一致，off 逐值不变，异常模型拒绝 |
| C1-B1 | B | fixed AVM 独立 symbol-site、CDF update 与边界 count trace | 路径不共享同一生产实现自证，错误/重试有覆盖 |
| C1-A2 | A | 两 bank 的 committed/scratch 事务及重放核验，补 all-intra/P/partial | 反序、failure、recon、neighbours 与完整 bank 不污染 |
| C1-00 | 00 | 限定熵自适应+基础 RD 的 API/CLI 接线、Windows/Linux strict、整流 AVM 解码 | 不支持组合明确报错，旧 off 指纹保持 |
| C1-B2 | B | 真正的 2D/3D/Real 多 QP 效果与耗时 | 真实压缩增益/损失明确，独立 speed |
| C2-SPEC | 00+A+B | 半像素 Q3、frame/sequence signal、HALF 模型及 oracle 合同 | fixed AVM 同单位、半像素相位/模型 trace 冻结 |
| C2-A/B | A/B | 亚像素插值与语法、完整 MV/recon 试验、验证 | fixed AVM 逐样本与小图机制通过后才跑原生曲线 |

随后按 C3→C10 独立启动，上一阶段留下完整 PENDING/DONE/FAIL 的测试与指纹。若 C1 真实曲线仍比 RD-off 差，先用同入口候选审计、剪枝漏胜和跨帧参考分歧定位原因，**不直接把 lambda 除 8 或无界加候选**。

## 22. 原则、资料和完工定义

### 22.1 资料

- [AV2 v1.0.0 规范（AOMedia）](https://av2.aomedia.org/v1.0.0/index.html)；2026-05-28 正式版，对应仓库固定 `docs/spec`。
- [固定 AVM v1.0.0](https://github.com/AOMediaCodec/avm/tree/v1.0.0)；不以最新主分支随意替换 oracle。
- [AV2 变换技术概述（Google Research，2026）](https://research.google/pubs/overview-of-transform-coding-in-av2/)；研究思路而非强制 bitstream 定义。
- `svt-av2/docs/task-reports/M6-r3-assessment.md`：完整 15 秒 AV1 差距、系数诊断。
- `svt-av2/docs/task-reports/M6-basic-capability-assessment.md` 与 `basic-capability-progress.md`：R5/R6/R7 当日状态与真实测试。
- `svt-av2/docs/task-reports/next-A-adaptive-rd-design.md`、`next-A-subpel-design.md`、`next-A-rd-baseline-audit.md`：实际源码锚定的规范与算法设计。
- `svt-av2/docs/m6-rd-contracts.md`、`basic-capability-contract.md`、`basic-entropy-contract.md`：以前阶段的冻结范围，历史数据必须保留。

### 22.2 完工定义

本阶段最终只能在以下四个条件同时成立时宣称完成：

1. **规范正确**：固定 AVM 独立解码、所有重建/参考/熵模型、跨平台与异常测试通过。
2. **真实有效**：同条件多 QP、真实长片与 holdout 的压缩曲线达到预先声明的竞争力门槛，不删负样本、不改测试定义。
3. **单线程基准可靠**：可在固定程序/源/输入条件下复现真实单线程成本，搜索和内存有界，所有开启的技术有独立贡献证据。
4. **后续可工程化**：对并行任务图有明确数据依赖与单线程黄金值，随后做多线程；再做真实 preset；最后做 CRF。上述先后顺序不可反转。

**禁止伪进展**：仅加 feature enum / no-op 开关、用非法转换降码率、拿 AV1 与 AV2 相同数字 QP/CRF 直接比较、只保留有利样本、在新工具试探中泄漏 entropy/recon state、用更多线程掩盖落后的 BD-rate、拿合成内容测试冒充真实世界压缩竞争力。

本文件是后续本地 AI 的施工总纲。单一模块通过只能称“模块机制通过”；只有合法性、真实压缩效果和冻结证据全部通过，才有资格进入下一阶段。

</details>


<details>
<summary>043 · 06-M6-frozen-requirements-20261025.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\06-M6-frozen-requirements-20261025.md`  
原文件 SHA-256：`0cd6c9b5e6aa56563ac42edb0ec7c207d7f34d54db5825537beec3c59e7b8fd7`

# M6单帧要求冻结与第三轮入口

2026-10-25，用户明确“继续下一轮”“这一版要求可以冻结”。以下沿用05指南的既有约定，不因本轮实验成败调整目标。

## 冻结目标

- 相同画质下，AV2单帧体积目标为冻结AOM AV1 cpu0、ALL-INTRA、threads1的**0.85×**，编码用时目标为**≤2×**。
- 主指标固定`vmaf_v1.0.16_3d0h`，副指标固定`vmaf_v0.6.1neg`；普通0.6.1仅作历史记录，不重新用于选优。
- v1的**93–95**区间单列，固定匹配质量93、93.5、94、94.5、95，同时保留宽域和跨内容检查，不以局部均值替代其它原有要求。
- 质量约束参考仍为原冻结r4c；对其相同像素使用新模型评分后，最多允许**1%同画质体积回退**。不把当前fast或新候选换作参考。
- PCHIP对数体积与线性插值同时报告。不外推、不丢弃不利点；影响验收的插值分歧须补真实样本，不能改用有利的插值方法。
- 高区间每曲线至少四个直接点且双端有包围；评分时间不计入编码用时。正式速度对照同机、同电源、同亲和，顺序独占；每实际点独立暖机后，各至少五个新进程，CPU和墙钟median/min/max分别保存。
- 候选冻结后才进入既有holdout20/I-GATE。当前先用开发集，不推进帧间/GOP/多线程，不改变旧默认模式。

模型、评分器、原r4c与合同来源的完整SHA见[不可静默修改的机器合同](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/requirements-freeze-r1.json)。原约定详见[05指南](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/M5-M6-AI-TASKS/05-M6-intra-next-round-guide.md)。今后若用户明确变更要求，另建新版本并记录变更；本版证据保留。

## 第三轮执行边界

实验目录为`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025`。第一批只补原r4c Q100一个参考点，原Q110模板仅改QP、stream和recon目的路径，原439文件和程序逐SHA核对。该点已获本轮继续指令授权；上轮NOTRUN计划保持历史原样。编码超时7200秒是本批执行边界，不是质量或速度验收门槛。

同时最多准备两个单因素压缩候选：两个finalist完成原RDOQ后排序；fast原有RDOQ提案档位/上下文的最小实验。先验证有限正确性与三个原生点的趋势，有足够信号才扩曲线。不会把两项一开始混合，不复活上轮没有稳定收益的内核方向。约两小时形成检查点，避免无边界串联新阶段。

上轮结果与尚未解决的问题见[第二轮报告](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/docs/task-reports/M6-intra-v1-r2-20261025.md)。第三轮进展见[本轮报告](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/docs/task-reports/M6-intra-v1-r3-20261025.md)。

## 后续执行授权（追加，不变更验收要求）

2026-10-25，用户明确本轮可以持续到次日，必要测试全部继续，也可追加有证据支持的实验。因此上面的约两小时检查点不构成停止期限，首批一个昂贵参考点和两个候选的数量也不构成后续实验上限。模型、像素、质量与速度目标、独立验证和候选准入要求保持冻结；原机器合同保留，不覆盖历史。

所有本轮实验及正式性能计时完成后，再用computer-use找到3FUI（窗口标题FFmpegFreeUI），点击一次“加入编码队列”并验证结果，随后汇报并停止会话。用户说明该动作将启动编码并占用CPU，因此不得在仍有测试或计时需要CPU时提前点击。

</details>


<details>
<summary>044 · 07b-M6-AVM-gap-analysis-20261026.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\07b-M6-AVM-gap-analysis-20261026.md`  
原文件 SHA-256：`51f3241fff3134e4937709216977705d4febf0e5b2ddcc8b33e1f0e043569a5e`

# M6 单帧压缩差距审查（2026-10-26，供 Codex 实验设计）
> 状态：分析与候选排序；**不是**新增编码验证报告。主参考和测试量以 06 冻结合同及 r3-final 原始证据为准。

## 1. 中途目标不改变最终合同
r3 最新隔离候选 Y4/wide+BO-only CCSO，1920x1080 10bit420 2D 同 VMAF v1 93–95 段 PCHIP AV2/AOM=1.185136518，线性=1.186641671。95分附近 PCHIP=1.261299。当前要压到1.00，平均需减少约15.62%（1-1/1.185136518），95附近需减少约20.72%（1-1/1.261299）。最终仍要0.85，即相对当前高带还需减28.28%。这些是候选体积降低需求，不是凭空承诺单工具收益。
AOM固定3.15.1 cpu0 ALL-INTRA threads1；同画质 VMAF v1.0.16_3d0h，副 v0.6.1neg；参考r4c允许同画质体积退步<=1%，不触holdout20，PCHIP与线性一起报告。

## 2. 官方 AVM 的 0.85~0.90× 代表什么
AV2规范与AVM模型分离：语法工具有潜力，真正实现高度依赖编码器如何选择合法组合，工具开关、内容、编码级别、视觉评分模型和码率匹配。AOMedia AV2 v1.0.0 规范(2026-05-28)及参考软件 https://av2.aomedia.org/ ；AVM编码器配置源码 https://aomedia.googlesource.com/avm/+/69d1b74ac2ced3b28564b99aef8c225026ce4f8a/av2/encoder/encoder.h 。无法仅凭某工具存在就给它分配BD收益；官方参考版本必须以本机冻结二进制SHA为准，不可偷偷替换到较新AVM。
本机早期官方 AVM v1 93–95 对 AOM约0.86x (具体P/L看05及r2报告)，不是普遍所有内容都达到0.85。官方代码支持几何分区、MRL/角度、DIP/CfL/MHCCP、变换分区、RDOQ/概率自适应及CCSO等；性能归因必须按实际启用/命中/消融/同分积分分开。
本机已知诊断反例：DIP/Palette/IST一些2D测试点未真正使用，CCTX=NONE，restoration=NONE，FSC低面积；盲目新增这些工具缺乏减码率证据。新版v1下官方滤波全关在93–95相对原官方+4.19% BD体积、单关CCSO+2.86%，但整个编码树重选，不能将各项简单相加、也不能推断本项目开启某滤波直接获取该百分比。
此前冻结 r4x 使用多个分区搜索、角度/CfL/RDOQ/MRL 及 AI λ 后有实质压缩改善；也证明粗略移植工具未必收益，如额外CfL和YALL在v1下可退步。更大的官方AVM搜索预算也可能影响同画质体积，速度差和码率收益要分开观察。这里官方CPU9档与AOMCPU0档并非等复杂度搜索，质量比较可以用，但不意味着标准天然拥有固定的优势值。

## 3. 本轮最值得做的代码实验（按证据强度）
### P0：可识别的模式遗漏
当前Y4/wide先 SAD 选基本Y，再DCT粗J，再完整角度/MRL/TX/RDOQ；不能靠最后阶段调λ修复此前漏选。做 observer + 离线oracle：按 SB 同一个规范入口快照，只测有限被淘汰的 mode seed（例如SMOOTH/angle候选、TX互补），记录SAD门是否遗漏、完整真实SB/J/码流bytes，跨重建边界时单独回放而不是加总每SB假想收益。再根据实际 winner 分布决定增加2~4个seed，保留原seed以阻止新路径完全排除旧优解。严禁针对坐标、图片名、固定QP硬编码。
### P1：分区搜索表达差距
将官方AVM Q96/128以及本项目Q96/110等相邻主v1画质点比对同空间Y/UV分区几何（须标明不完全同分），包含8×16/16×8、细长矩形、四象限混合、剪枝前后J。找“官方高频且当前plan无法表达/先被剔除”的具体一种混合子树，才以有界beam/增量搜索加入。避免盲扫全组合与破坏共享CDF因果性。
### P2：完整RDOQ与树winner顺序
只对在完整前缀下粗J接近的winner/finalist进行两条完整优化链；重新读取r3对双finalist RDOQ和phase1的负证据，不能重新无界开回。记录真实bit/J与拒绝理由，评估有界额外FWD是否买到压缩增益。
### P3：UV联合决策与视觉指标
只有在数据提示色度结构误差/模型负贡献时，才扩UV派生TX、CfL/MHCCP、CCSO邻域/颜色模型。SSE下降不意味着v1上升，v1/NEG和PSNR-U/V必须并报；UV TX需从Y类型和模式派生，不能任意自由信令。
### P4：滤波有界实验
先复用官方已评分滤波消融，观察视觉特征CAMBI/chroma/边缘，再决定是否做兼容的CCSO复杂化或其他环路滤波；在大负载FFmpeg期间仅做小图正确性测试。

## 4. 本轮资源与验收规则
FFmpeg/FFmpegFreeUI 不停止、不调优、不点击新队列。编码质量数据可串行低优先级；绝不跑正式或替代性的速度bench，不用当前CPU/墙钟对比作算法改善宣称。开发片公开可重复，采用隔离源副本与全量命令/程序/输入/SHA，真实AVM独立解码逐像素，先少量QP试验，强正信号才铺满有包围的4+实点并同时算PCHIP和线性。候选必须相对冻结r4c满足<=1%损失，新版本资料不覆盖原r3，未过门不合MAIN。每一项结果必须标清: 假设、改动路径、正确性门、编码点、同画质体积与未验收项。

</details>


<details>
<summary>045 · 07c-M6-codex-execution-audit-20261026.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\07c-M6-codex-execution-audit-20261026.md`  
原文件 SHA-256：`eca0e4f7e29dd66e3503ecdb80a333a10dff76df820d3a5ebd1ef59f72d9103f`

# M6 r4 Codex 执行交接与事实状态（2026-10-26）

## 目标与原冻结合同
用户要求通过 NuxBox1 本机 AgentDock 调用本机 Codex，继续自研 SVT-AV2 M6单帧压缩优化；本轮先争取v1同分93–95 AV2/AOM≤1.0x，后续几轮继续到≤0.85x；最终单线程编码时间≤2x保留但**本轮完全不测速度**，因为 FFmpegFreeUI和 ffmpeg 正在占用CPU。不得中断压制任务、误触3FUI新增队列。r3冻结基线：1.185136518x (PCHIP)/1.186641671x(线性)，95点1.261299x；1.0目标未达标。

## 已进行的真实主机检查与落盘
- 本机 Codex Windows二进制位于 <LOCAL_USER>/AppData/Local/OpenAI/Codex/bin/37f54d68e5533a51/codex.exe，原桌面app-server一直在运行。本轮新命令没有重启桌面端。
- FFmpegFreeUI PID 21860 和FFmpeg PID32444启动时仍在处理视频；未停止、降低优先级或更改它们。
- 指令文件：M5-M6-AI-TASKS/07-M6-intra-r4-20261026-codex-brief.md。
- 独立分析文件：M5-M6-AI-TASKS/07b-M6-AVM-gap-analysis-20261026.md，说明AVM压缩效率的机制、已经排除的低价值方向和候选优先级；它是分析文档，**不是实际实现及实验结果**。
- 旧r3质量和速度合同不变，未动MAIN或holdout20。

## 本机Codex调用故障（不得伪称实现）
1. 首次 CLI exec 将互斥的 --approve-for-me 与 -s workspace-write 合用，CLI 立即拒绝（没有执行）。
2. 修正参数重新启动后，Codex会话在首批文件读取命令的 code-mode cell 长时间不返回。仅结束这次新建的CLI PID5700，不影响桌面Codex和FFmpeg。
3. 尝试设置 features.code_mode_host=false；该CLI会话可以回复，但文件工具返回 `code-mode host is disabled`，最后明确说没有源码读取/修改/测试。记录保存在 <LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-r4-20261026/codex-r4-retry-last-message.md。
4. 进一步走本机 Codex app-server API，已收到 thread/start、turn/start 的inProgress确认，thread ID=01a175da-ccd5-7783-9f2d-af24bfb38201，但在本报告写入时还未见源码读取或压缩结果；**不要据此认定已实现代码或已开始真正的长测**。

## 下一步恢复操作
- 首先修好Code Mode工具实际exec的阻塞，确认一个只读Get-Content和一个不会遍历git目录的大文件读取返回；若Codex CLI工具失败可沿用App Server，但仍必须有真实文件事件与结果。
- 工作起点为07/07b和原r3 final，优先候选遗漏（SAD→DCT-J→联合TX/MRL/RDOQ），其次分区语法搜索差距，禁止无根据叠加AVM全套工具。
- 存在2026-10-26视频压制时，质量实验只允许有边界、低并发小样本；正式速度测试全部跳过。任何新体积比须用正确的v1/NEG、真实解码流和完整质量曲线计算后才能宣布。

</details>


<details>
<summary>046 · 09-M6-longrun-20261026.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\09-M6-longrun-20261026.md`  
原文件 SHA-256：`3dc8ff6d2bcc607df58866e59d5addc24f5193a2786ab358768cb0d24d78f412`

# M6 长周期自主开发合同（2026-10-26）

## 目标、原则和停止条件

本地 NuxBox Codex 多轮持续执行；优先在 **AV2 单帧 1920×1080 10-bit 420、VMAF v1.0.16_3d0h 主指标**下，把相对冻结 AOM AV1 3.15.1 cpu0 / ALL-INTRA / threads1 的同画质体积由约 **1.185136518×（PCHIP）、1.186641671×（对数线性）**降低到 **≤1.000×**。不能用旧 VMAF v0.6.1、只看单QP字节或牺牲质量来达标。95 分附近当前约 1.261×，必须单列。最终目标仍 **≤0.85×体积、≤2×编码耗时**，后者不是本轮任务。

候选相对原冻结 **r4c** 的同画质体积连续最坏回退不超过1%；副模型 VMAF v0.6.1neg 单独报告；固定评分点 93、93.5、94、94.5、95，PCHIP 对数体积与分段线性两种插值都报告，不外推、留存负样本；高分段每条曲线至少4个真实点并双侧有包围，保留宽质量区间、2D/3D/Real交叉测试。不触及 holdout20/I-GATE，直至候选冻结并完成开发集验收。到达1.00×需要带内BD和五个匹配质量点在两种插值下都≤1.00，质量/解码门通过；符合条件后写入`svt-av2/artifacts/m6-longrun/milestone-1x.json`及独立验证资料，让本地任务自动停止等待用户确认。未经独立验证不写PASS。

**安全与资源：** NuxBox 3FUI正在进行 FFmpeg 压制，不停止、不更改 FFmpeg/3FUI 进程、参数、队列，不重启电脑。用户已授权多轮测试；FFmpeg工作时使用低优先级、单个编码或测试进程、低并发小样本，在资源允许时以串行原生1080p质量试验为主，绝不进行速度benchmark/CPU性能对照。盘空间低于20GiB时暂停大测试；不删除冻结证据，不清理非本任务文件。长周期14天，自动对 Codex 每周额度设置保留阈值；额度不足则等待重置，避免接口反复失败。任务可由 stop.flag / pause.flag 控制。

## 每轮开发与验收闭环

1. **先读资料再决策。** 读`AGENTS.MD`、`06-M6-frozen-requirements-20261025.md`、`07b-M6-AVM-gap-analysis-20261026.md`、`08-M6-intra-r5-plan-20261026.md`、`svt-av2/docs/task-reports/M6-intra-r4-20261026.md`与`svt-av2/artifacts/intra-r4-20261026`真实代码、`svt-av2/artifacts/m6-longrun/progress.md`和本轮历史。当前MAIN、冻结r4c及所有历史目录只读，不因之前某个候选局部 J 为正就升为生产。
2. **选择一个可证伪的主假设，避免重复试错。** 重审官方 AVM 中比当前精简编码器更完整的分区、候选预筛/枝剪、Y预测×TX×MRL 联合、UV派生 TX、熵CDF/RDOQ和滤波决策。分析 *技术合法且在AVM源码中出现*、*在该素材上实际使用*、*同分真实收益* 三者区别。优先修复 SAD/DCT 候选过早淘汰（包括SMOOTH）；比较有限24树和AVM实际几何，勿无界扩大搜索，勿直接照搬官方 cpu9 搜索深度。当前筛选/剪枝没有AOM的速度，说明不能只粗放加更多候选，需有成本/收益因果证据，但本阶段不进行速度比拼。
3. **做最小代码和受控测试。** 独立候选源、副本路径及全部SHA；MSVC/GCC/SAN按影响范围，固定AVM解码逐像素、真实错误/候选反序、原OFF码流与重建精确回归。记录失败原貌，不改旧日志。确认后串行测试至少三真实Q，若高质量点稳定收益才补至少4个直接实测点及两侧包围并计算BD和五个匹配质量位置。VMAF v1/NEG及PSNR Y/U/V一并报告。**局部J改善不代表整帧压缩率**。
4. **每轮评估、淘汰、推进。** 若无益或跨内容回退超门则封存负实验，分析下一个更有证据的方向。强正候选可逐步集成到单独冻结副本并重新做独立审计，未过门不碰MAIN。不要在单张图、固定坐标、QP设置硬编码。多轮直到 1.00× 通过，或14天/额度/磁盘/工具故障强制停止。
5. **必须保存可继续的检查点**至`svt-av2/artifacts/m6-longrun/progress.md`，记录轮次时间、当前假设、改动/哈希、编译及样本结果、最近实际体积比（未测写 NOT_RUN）、待做一步；在`svt-av2/docs/task-reports`中保存每轮独立报告。下一轮启动先读检查点，**不要只写建议和方案，要实际实现+测试+审核**。工具不可用时做小范围探测，切换已证实可用的执行工具，不要持续空等补丁工具。

## 设备内执行器

控制器文件位于`scripts/m6-longrun-20261026`。Windows Task Scheduler 主任务 `SVT-AV2-M6-Autopilot-20261026`（每天09:00及手动启动），小时级恢复触发器 `SVT-AV2-M6-RestartWatch-20261026`（每小时，单实例锁防止重复启动）。任务在已登录用户 ARXChem 的交互会话中运行，设备重启后需要该用户登录；执行器按持久化状态恢复。Windows主任务默认72小时运行期限届满时，会由恢复触发器再次启动，14天总期限仍由执行器判断。本机`state.json`、`events.jsonl`持久化；14天活动期内连续轮次，每轮最多100分钟，中间短暂休息/健康检查；每周 Codex 已用额度≥92%时暂停至额度重置后再查询。停止文件`stop.flag`，暂停文件`pause.flag`；永久禁用须同时禁用上述两项Windows任务。此前默认 elevated Windows 沙箱在项目 `.codex` 目录 ACL 更新时返回错误5；只为此执行器传入`windows.sandbox="unelevated"`，不动全局Codex配置与ACL；该fallback比elevated网络隔离弱，待系统修复后可恢复。自动化目标是持续推进，有无成果以真实曲线和独立审计为准，不承诺必定14天内达到1.00×。

</details>


<details>
<summary>047 · README.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\README.md`  
原文件 SHA-256：`51452cc0f6b4f086351f4c8f4df269601f817016d3cf23dd0ea93b52a7f76fdd`

> **2026-10-18 当前开发方向：压缩竞争力优先。** 用户指定 `M5-M6-AI-TASKS/03-M6-single-thread-compression-roadmap.md` 为执行总纲：单线程压缩能力与同结构真实 BD-rate 验收 → 内部多线程 → 真实 preset → CRF。旧 M7/M8 排序仅保留历史含义。当前执行合同 `svt-av2/docs/m6-compression-contract.md`；C1 首轮 adjusted probability rate 内核已通过机制验证，双 bank/RD 公共组合和真实收益尚未验收，默认关闭、正式仍 0.0.5-m5。用户因额度即将耗尽，要求续费后于 2–4 天接续，本轮不新增原生大矩阵。



# M5 → M6：有界 Mode Decision 与 RD 框架开发指南



> **2026-10-17执行续行：用户已授权启动，M6 RUNNING。** 已有RD代码与必要短验，进度、实际边界（含partial固定单路径）及集中长测以svt-av2/docs/m6-rd-contracts.md、docs/task-reports/M6-integration.md和外部campaign-status.json为准。以下“本轮仅制定任务”是指南编写时点，不表示撤回当前开发授权。



> 2026-10-17 编写。本轮仅制定任务，**M6 尚未启动、未实现**。下一轮由当前 Codex 主会话 00 加两个子智能体 A/B 执行；不用 OpenCode 会话或其 HTTP 消息脚本，不操作 FFmpegFreeUI。



## 起点与目标



M5.3 已有非无损 CQP 1..255、三格式 AI/单参考 LOW_DELAY_P、DC/V/H 预测、完整 64×64 超块的原树/四个 32×32 叶选择，以及多 QP、PSNR/VMAF、BD-rate、断点批测。最终基线必须引用 00 收尾冻结的 `../svt-av2/artifacts/m53-release/source-manifest.json` 和 `final-verification.json`，核对140源与5程序 SHA，不用版本字符串代替指纹；启动时若正式收尾尚未 PASS，先完成这个前置门禁。旧候选长测证据为 `../svt-av2/artifacts/m53-root/long-campaign-verification.json`，不能把旧长测程序 hash 当新版发行程序 hash。



现有模式主要以预测 SAD 选择，分区主要以源图均值 SSE 代理选择；M6 将其升级为比较实际量化重建失真与真实编码语法成本的统一 `J = D + λR`。优先把已有工具选对，不在这一阶段增加所有 AV2 工具。VMAF 偏低可能受合成内容、分辨率、模型及现有编码工具共同影响，当前未确认各因素贡献，也不承诺 M6 达到某绝对分数。



## 固定范围



- 格式保持 8-bit 420、10-bit 420、10-bit 444；不做 8-bit 444、422。

- AI 和上一张内部重建为唯一参考的 P；保持当前整数运动搜索，允许将其合法结果作为 RD 候选，不扩多参考、B/RA、亮度 subpel。

- Y 候选 DC/V/H，UV 保持 DC；变换/量化/系数语法用已支持路径。

- 完整 64×64 超块比较原树与四个 32×32 叶；部分超块继续遵循已有合法边界树，不引入任意分区递归。

- CQP 保持 1..255；10-bit 实际 qindex=用户 QP+48。CRF 属于 M9；不实现 CBR、2-pass、VBR、VBV 独立码率模式。

- 内部仍单线程、现有尺寸约束不变。M7 负责 preset，M8 负责编码器内部并行；本阶段允许离线批测多进程。



## 1+2 属主



| 会话 | 任务书 | 唯一职责 |

|---|---|---|

| 00 主会话 | [00-INTEGRATION.md](00-INTEGRATION.md) | 冻结合同、API/CLI/构建与公共文件、串行接线、独立终验 |

| A 子智能体 | [01-RD-CORE.md](01-RD-CORE.md) | 候选、失真/λ、重建事务、AI/P 和分区 RD |

| B 子智能体 | [02-RATE-VALIDATION.md](02-RATE-VALIDATION.md) | 语法 rate 计量/熵事务、独立核验、真实视频效果评测 |



A/B 直接交流接口与发现，并抄 00。文件归属在实际启动时由 00 落盘；共享头、`keyframe.c`、`encoder.c` 等交叉接线只能指定一个写者，不能两人同时改。各自用独立 build/artifacts 目录；只有 00 能验收 `DONE`。



## 分步执行与通过条件



| 步骤 | 交付 | 通过条件 |

|---|---|---|

| M6.0 | `docs/m6-rd-contracts.md` 与冻结 M5 基线 | 候选集合、成本单位、事务状态、开关优先级、测试矩阵均明确 |

| M6.1 | 失真、λ、候选池和 rate 基础 | 量纲/溢出边界、符号级 rate、回滚与提交单测通过 |

| M6.2 | AI 与 P 块候选完整 RD | 实际 transform/quant/recon，三格式独立解码与内部重建一致 |

| M6.3 | 分区候选及有界 exhaustive | 相同入状态下比较完整候选树，兄弟叶重建/熵上下文顺序正确 |

| M6.4 | pruned 与 exhaustive 对照 | 固定同一候选全集，记录剪枝损失、候选数、耗时及多点质量结果 |

| M6.5 | 真实视频、多平台/检测器和无人值守验收 | 完成 00 规定矩阵，报告体积—质量—时间，不预设收益百分比 |



“exhaustive”只表示**在合同冻结的单个决策域和候选集合内不剪枝**，不是 AV2 全工具、全帧或全序列全局最优。对子树，不能把逐叶贪心冒称整个超块穷举；可以在小图上枚举全部合法叶模式组合，生产路径另做剪枝。



## 新开关与兼容约定



拟定 CLI `--rd-mode off|exhaustive|pruned`，默认 `off`；由 00 在 M6.0 冻结 API 字段/枚举与 CLI 名称，本文件不是已实现用法。`off` 必须走原 M5 决策路径，保留默认及两旧工具四组合的码流/重建指纹。



RD 开启时，旧 `intra-select`/`partition-select` 只定义候选是否开放：`dc` 仅 DC，`adaptive` 开放合法 DC/V/H；`fixed` 固定原树，`adaptive` 开放现有两种树。新 RD 不应暗中打开用户关闭的工具。非法 RD 参数、运行中不支持的切换明确拒绝，不静默降级。新配置追加涉及 ABI，所有调用方按新头重编；不在本轮顺带做 FFmpeg wrapper 或 `svtav2-params` 通用解析。



## 资料与数据纪律



先读 `../AGENTS.MD`、`../SVT-AV2-PLAN-AI.md` 第 10 节、`../svt-av2/docs/m53-validation-contracts.md` 与本任务包。旧 OpenCode 通信规则按上方 Codex 1+2 约定替代。历史测试媒体已有离线归档，缺文件先查 `<LOCAL_PRIVATE>/ffmpeg-native/WORKSPACE-CLEANUP.md`，不把离线文件误判为回归失败。



真实视频与产物按新合同设定保留策略，成功任务默认留码流、指标、源/工具 hash、日志和摘要；大体积 recon 可在校验后按明确策略清除。保留冻结 M5 程序与固定 AVM oracle，不改旧结果，不重新展开全部历史媒体。新增长测启动前报告预计磁盘预算并保证工作区不再次无界膨胀。



最终报告必须分开写“合法性/确定性通过”“RD 机制通过”“样本上的压缩效果”，不能用某一条高 VMAF 或某一个 QP 代表总体压缩能力。

</details>


<details>
<summary>048 · READ-ME.txt</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\inter-20261028\READ-ME.txt`  
原文件 SHA-256：`bfd2da6a3cb3a2d8738eb404906751b12a782d7f26a17709414f3b06af34add3`

SVT-AV2 M6 帧间优化首轮交付

先阅读 REPORT.html；数值索引为 REPORT-DATA.json；完整同质曲线审计为 comparison-v3.json。

目标 0.95× AOM p0 尚未达到。本轮没有给出归一化的纯帧间因子。



source/ 是本轮已提交的完整跟踪源码，版本仍为 0.0.5-m5。

code-delta-from-frozen-r6.patch 与 commits-from-frozen-r6.mbox 从冻结 R6 基线导出。

完整源码与补丁是两个使用入口；不要将补丁再次应用到包内已更新的 source/。

合入另一工作树前先 git apply --check，并按并行帧内改动解决冲突；本会话没有修改原帧内工作树。



包含：构建/验证/评分脚本、所列冻结研究EXE、已完成整段与前缀码流、逐帧metrics、门禁日志、模型副本与哈希。

不包含：原始383帧Y4M、GB级raw参考/recon/decoded、外部AOM/AVM/VMAF工具和build目录。

这些文件仍在设备工作目录中；result.json中的绝对路径和SHA指向当时的输入与工具。

已有SVT p6文件以benchmarks/svt-p6-crf36/existing-encode.mkv附入，结果按payload计体积。



脚本保留设备原有工具/素材路径；完整质量runner可用新label复跑，防止覆盖冻结测点。

部分外部验证脚本依赖先前的本机验证产物，不能当作无依赖的一键安装脚本。

研究开关默认关闭；M6_INTER_RDOQ_POST、DRL1和forced residual skip均未进入推荐配置。

所有walltime来自争用环境，不能据此给出速度达标结论。

ARCHIVE-CONTENTS.json记录包内文件SHA256；整体ZIP SHA另记于设备ARTIFACT.json。

</details>


<details>
<summary>049 · MODE-MIGRATION-MATRIX.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\inter-r12-20261030\MODE-MIGRATION-MATRIX.md`  
原文件 SHA-256：`4ca65831ce6227d3091a0b5f79594e764a5d65f818d03123629f46fe53fc3462`

# SVT-AV2 M6 模式移植矩阵



生成：2026-10-30T08:53:55.823555+08:00



| 模块 | AOM AV1 | AVM AV2 | SVT-AV1 | 当前 M6 状态 | 优先级 |

|---|---|---|---|---|---|

| Intra DC/V/H + smooth/Paeth + 56 directional | av1/encoder/rdopt.c; av1/encoder/encodeframe.c | av2/encoder/rdopt.c; av2/encoder/encodeframe.c | Source/Lib/Codec/encode_loop.c; intra prediction modules | syntax exists for M5 modes 0..61; RD candidate entry currently rank_modes only tests 0..3; P-frame validation blocks m5>=2 | P1 |

| UV mode selection / CfL / MHCCP | av1/encoder/rdopt.c; av1/encoder/reconinter_enc.c | av2/encoder/rdopt.c; av2/encoder/reconinter_enc.c | intra prediction + chroma prediction paths | UV syntax partially present; sequence flags exist in sequence_header.c; packet-tools API does not expose MRL/MHCCP/transform on multi-frame path; RD rejects these when reference/multi_frame | P2 |

| MRL / MRL averaging | av1/encoder/rdopt.c | av2/encoder/rdopt.c; AV2 MRL syntax | intra prediction and MRL mode search | syntax and predictor exist; multi-frame validation and packet sequence signaling not wired | P2 |

| Full transform set / transform-type search | av1/encoder/tx_search.c; txb_rdopt.c | av2/encoder/tx_search.c; txb_rdopt.c | transform_search.c / coding_loop.c | RD path supports selection, but packet-tools entropy API passes transform=0; P frame validation forbids transform selection | P1 |

| RDOQ / coefficient trellis | av1/encoder/txb_rdopt.c | av2/encoder/txb_rdopt.c | quantization.c / rdoq.c / coding_loop.c | RDOQ path exists but previous P postpass was quality-negative and default OFF; do not blindly enable. TCQ multi-frame explicitly disallowed | P3 |

| Block partition search | av1/encoder/partition_search.c | av2/encoder/partition_search.c | pd_process.c + enc_dec_segments.c | coarse SB partition exists; P16 candidate code not active in current accepted multi-frame path | P2 |

| MV prediction / NEWMV / NEARMV / DRL | av1/encoder/mcomp.c; rdopt.c | av2/encoder/mcomp.c; rdopt.c | motion_estimation.c; me_context.c | single integer MV; syntax collapses to NEW vs near-zero; one spatial predictor; no DRL candidate list | P1 |

| Sub-pixel motion search and interpolation | av1/encoder/mcomp.c; interp_search.c | av2/encoder/mcomp.c; interp_search.c; reconinter_enc.c | motion_estimation.c subpel stages | integer-only ME and integer MC; no fractional syntax or interpolation filter search | P1 |

| Reference frame selection / temporal MV / compound prediction | av1/encoder/rdopt.c; global_motion.c | av2/encoder/rdopt.c; global_motion.c; reconinter_enc.c | reference_object.c; pd_process.c; me_context.c | one-frame low-delay reference only; no multi-ref, compound, temporal MV, or refresh policy | P1 |

| Skip / skip-mode / global motion | av1/encoder/rdopt.c; global_motion.c | av2/encoder/rdopt.c; global_motion.c | mode decision and global motion modules | residual skip inferred from all-zero coefficients; no distinct AV2 skip_mode candidate; global motion only identity trial in prior branch | P2 |

| Multi-thread scheduling / lookahead / EncDec wavefront | encodeframe.c tile/row MT | encodeframe.c tile/row MT | pd_process.c; me_context.c; enc_dec_tasks.c; enc_dec_segments.c | no persistent worker pool, lookahead or EncDec wavefront; preserve causal tile/CDF/recon commits | P3 |



## 迁移约束

- AVM AV2 是码流语法和重建的最终 oracle；不把 AV1 语法直接搬入 AV2。

- AOM/AV1 主要借鉴搜索、候选剪枝、分区和多线程调度思想。

- SVT-AV1 主要借鉴 ME、PD、参考生命周期与 EncDec task/segment 的流水线拆分。

- 先让当前最佳帧内模式在 P 帧回退中进入 RD、真实 tile writer 和固定 AVM 解码闭环；随后再扩展 MV/分区/参考模式。

- 所有模式都必须具备语法、预测重建、率失真成本和独立解码证据；仅有搜索候选不算实现。

</details>


<details>
<summary>050 · REPORT-DATA.json</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\inter-20261028\REPORT-DATA.json`  
原文件 SHA-256：`8588cf35388f9de717814114e547c8cef2254cd9cb3bca9dd0404f81acbb76a3`

{

  "generated_at": "2026-10-28T17:41:23.899146+08:00",

  "source_commit": "d244374b79417fa8a624b1f35aaaaf0afa8b562b",

  "source_clean": true,

  "status": "FIRST_INTER_OPTIMIZATION_MILESTONE_TARGET_NOT_MET",

  "full_comparison_status": "PASS_ALL_COMPARISONS_READY",

  "full_points": [

    {

      "title": "AOM AV1 p0 Q30",

      "path": "benchmarks/aom-p0-q30-r1/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 767974,

      "v1": 96.97104553263704,

      "neg": 94.59983003394255,

      "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

      "stream_sha256": "830bb8cbecc507039eeb3f5ad7875c89b6ef8f7885da4cd1a5c395bfe781c248"

    },

    {

      "title": "AOM AV1 p0 Q36",

      "path": "benchmarks/aom-p0-q36/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 540727,

      "v1": 96.1534688981723,

      "neg": 93.83849812271544,

      "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

      "stream_sha256": "d5011fdad0e21eede8136efb635b6a0067dbfa2ef9ce7539d69fdb1ee7b726fe"

    },

    {

      "title": "AOM AV1 p0 Q42",

      "path": "benchmarks/aom-p0-q42/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 401681,

      "v1": 95.09700267101825,

      "neg": 92.80113969451706,

      "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

      "stream_sha256": "273783cd196823c218ae46b442c11f0af9540aa1fdc3be504a3eb40cb0d55a56"

    },

    {

      "title": "AOM AV1 p0 Q44",

      "path": "benchmarks/aom-p0-q44/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 367342,

      "v1": 94.69445877545701,

      "neg": 92.39896300783296,

      "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

      "stream_sha256": "dd4645a955589d70ca0cdda9959e3d496deae4a0c31e8fe4eac46aed4542f672"

    },

    {

      "title": "既有 SVT-AV1 p6 CRF36",

      "path": "benchmarks/svt-p6-crf36/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 801583,

      "v1": 95.92044236553518,

      "neg": 93.28711720104434,

      "encoder_sha256": null,

      "stream_sha256": null

    },

    {

      "title": "冻结基础 P Q100",

      "path": "quality/baseline-q100/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 4344216,

      "v1": 93.01738566057443,

      "neg": 91.59372942036548,

      "encoder_sha256": "b431f82dd351a15012c8abd7a969bcac2b59765718e4e6d79edcf98220176895",

      "stream_sha256": "7529127760a2b4b5e057bb15eb7b43645a83f8260240fa17b1ef793a8b285b5e"

    },

    {

      "title": "Adaptive CDF P Q100",

      "path": "quality/adaptive-q100/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 3768812,

      "v1": 93.07824779634466,

      "neg": 91.61752094778075,

      "encoder_sha256": "42257c53c3392efa89d6f929583041c10728bb2aa08169bd5c707a54b874e996",

      "stream_sha256": "4e1b09ad5d65a3a2f75362d7cde29708aa22ea0167942f0951902135e990e6c2"

    },

    {

      "title": "GLOBALMV 路线 Q92",

      "path": "quality/global-q92/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 4978711,

      "v1": 94.42871815926893,

      "neg": 92.7542595091383,

      "encoder_sha256": "eee910a6adcd7756bdbf14d581b7b439a36704f705adb969e39bb1dc8789d342",

      "stream_sha256": "1cd50060e838ad8e2739b5838235710af4309397e3c599cf7faf8497ab5452ff"

    },

    {

      "title": "Basic P Q88",

      "path": "quality/basic-q88/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 5725392,

      "v1": 95.06545901305485,

      "neg": 93.27301328198432,

      "encoder_sha256": "8fcbd8e83f74a1f154ceb9cfaa03808124c5745032b3c5fa0c63d5057ee4181b",

      "stream_sha256": "e1a0b543adf243a841a8eede544edc93225d4098d58911820a9b8bb933e5db60"

    },

    {

      "title": "Basic P Q100",

      "path": "quality/basic-q100/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 3646077,

      "v1": 93.21044609921668,

      "neg": 91.60939480678854,

      "encoder_sha256": "8fcbd8e83f74a1f154ceb9cfaa03808124c5745032b3c5fa0c63d5057ee4181b",

      "stream_sha256": "d687cc8baa16cd94447cce938834014ab1fdc1fd7b7876937575e13f6335fc88"

    },

    {

      "title": "P16 v2 Q88",

      "path": "quality/fine16-v2-q88/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 5480357,

      "v1": 95.2794215979113,

      "neg": 93.45381638903392,

      "encoder_sha256": "e13591438eb6cacb354676b55e090433c6a05a0419030b3871c77f8c5227e756",

      "stream_sha256": "5e15e3a13e7d278b0f62762eef35bd6f1884cb3aaeb637fae5611a6617b107a6"

    },

    {

      "title": "P16 v2 Q100",

      "path": "quality/fine16-v2-q100/result.json",

      "status": "PASS_FULL_SEQUENCE",

      "frames": 383,

      "bytes": 3462815,

      "v1": 93.4774089895561,

      "neg": 91.81173531592688,

      "encoder_sha256": "e13591438eb6cacb354676b55e090433c6a05a0419030b3871c77f8c5227e756",

      "stream_sha256": "7f4acaaa17910befe5db593529e5d5466ee38a8fa9450860ae33193e686eafaa"

    }

  ],

  "curves": {

    "aom_p0": {

      "status": "PASS_BRACKETED_INTERPOLATION",

      "bytes_log": 393123.4165415916,

      "bytes_linear": 393406.1888382225,

      "neg_at_v1_95": 92.70422551134399,

      "v1_weight": 0.7590258550983575,

      "sources": [

        "benchmarks/aom-p0-q44",

        "benchmarks/aom-p0-q42"

      ],

      "note": "V1 matched; NEG uses the same V1 weight and is not matched separately."

    },

    "av2_basic": {

      "status": "PASS_BRACKETED_INTERPOLATION",

      "bytes_log": 5634943.66042809,

      "bytes_linear": 5652017.897472315,

      "neg_at_v1_95": 93.21430812544672,

      "v1_weight": 0.9647123680021135,

      "sources": [

        "quality/basic-q100",

        "quality/basic-q88"

      ],

      "note": "V1 matched; NEG uses the same V1 weight and is not matched separately."

    },

    "av2_fine16v2": {

      "status": "PASS_BRACKETED_INTERPOLATION",

      "bytes_log": 5103791.062263227,

      "bytes_linear": 5167515.232478899,

      "neg_at_v1_95": 93.19919391013447,

      "v1_weight": 0.8449391549117189,

      "sources": [

        "quality/fine16-v2-q100",

        "quality/fine16-v2-q88"

      ],

      "note": "V1 matched; NEG uses the same V1 weight and is not matched separately."

    }

  },

  "same_qp_cdf_reduction_pct": 13.245289829050854,

  "baseline_to_fine16_same_qp100_reduction_pct": 20.28906942012092,

  "baseline_to_fine16_same_qp100_v1_delta": 0.46002332898166287,

  "baseline_to_fine16_same_qp100_neg_delta": 0.21800589556140437,

  "fine16_vs_basic_at_v1_95_reduction_pct": 9.426049844915596,

  "current_pipeline_vs_aom_at_v1_95_ratio": 12.98266866716462,

  "conditional_0p95_aom_bytes_at_v1_95": 373467.24571451196,

  "normalized_inter_factor": null,

  "normalization_reason": "No matched full383 best all-intra curves for both codecs; current P intra fallback still basic, not full best R6.",

  "rdoq_decision": "OFF; Q88 prefix point is directly dominated by unmodified P16 Q100 in bytes, V1 and NEG.",

  "performance_scope": "All walltimes contended diagnostics, no speed claim.",

  "main_intra_work": "All edits isolated in inter-20261028/source; R6 best still3formats bitstream/recon preserved."

}

</details>


<details>
<summary>051 · comparison-v3.json</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\inter-20261028\comparison-v3.json`  
原文件 SHA-256：`4e795893d96995b7869d3adfe5b317b5de8616ab1cde081a729fe64995c75456`

{

  "updated_at": "2026-10-28T17:41:23.306856+08:00",

  "target_v1": 95,

  "frames": 383,

  "profiles": {

    "aom_p0": {

      "points": [

        {

          "path": "benchmarks/aom-p0-q30-r1",

          "status": "PASS_FULL_SEQUENCE",

          "q": 30,

          "bytes": 767974,

          "v1": 96.97104553263704,

          "neg": 94.59983003394255,

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

          "decoder_sha256": "feed8b911da9af85d21adfcdaa3b9ff4016dee8be7c38c18fbee05579bfdd4c1",

          "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

          "stream_sha256": "830bb8cbecc507039eeb3f5ad7875c89b6ef8f7885da4cd1a5c395bfe781c248",

          "profile": {

            "input_sha256": "70eec42e92f5e67c1f6055aa6380122c3cdf50ca3da2bfb72b8be56d50f31a22",

            "reference_sha256": "8c6899582907738163e9fcbee8b04a6963599edd71d875cad3971cae0758a701",

            "frames": 383,

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

            "decoder_sha256": "feed8b911da9af85d21adfcdaa3b9ff4016dee8be7c38c18fbee05579bfdd4c1",

            "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

            "schema": 1,

            "fps_num": 24000,

            "fps_den": 1001,

            "models": {

              "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

              "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

            },

            "encode_argv_normalized": [

              "--cpu-used=0",

              "--end-usage=q",

              "--passes=1",

              "--threads=4",

              "--row-mt=1",

              "--tile-columns=0",

              "--tile-rows=0",

              "--obu",

              "--bit-depth=10",

              "--input-bit-depth=10",

              "--limit=383",

              "--cq-level=<Q>",

              "-o",

              "<STREAM>",

              "<INPUT>"

            ],

            "decode_argv_normalized": [

              "--rawvideo",

              "--output-bit-depth=10",

              "-o",

              "<DECODED>",

              "<STREAM>"

            ],

            "score_argv_normalized": [

              "--reference",

              "<REFERENCE>",

              "--distorted",

              "<DECODED>",

              "--width",

              "1920",

              "--height",

              "1080",

              "--pixel_format",

              "420",

              "--bitdepth",

              "10",

              "--model",

              "path=v1.json:name=v1",

              "--model",

              "path=neg.json:name=neg",

              "--threads",

              "2",

              "--json",

              "--output",

              "metrics.json"

            ],

            "pooling": "arithmetic_mean_all_displayed_frames",

            "primary_metric": "v1",

            "aux_metric": "neg"

          },

          "profile_fingerprint": "9852960a430765009667cfd846cd7d356c395a9b239a6d4f4c4ed339d75d7321",

          "metric_scope": "383 displayed frames, arithmetic means",

          "decode_log_error_markers_absent": true

        },

        {

          "path": "benchmarks/aom-p0-q36",

          "status": "PASS_FULL_SEQUENCE",

          "q": 36,

          "bytes": 540727,

          "v1": 96.1534688981723,

          "neg": 93.83849812271544,

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

          "decoder_sha256": "feed8b911da9af85d21adfcdaa3b9ff4016dee8be7c38c18fbee05579bfdd4c1",

          "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

          "stream_sha256": "d5011fdad0e21eede8136efb635b6a0067dbfa2ef9ce7539d69fdb1ee7b726fe",

          "profile": {

            "input_sha256": "70eec42e92f5e67c1f6055aa6380122c3cdf50ca3da2bfb72b8be56d50f31a22",

            "reference_sha256": "8c6899582907738163e9fcbee8b04a6963599edd71d875cad3971cae0758a701",

            "frames": 383,

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

            "decoder_sha256": "feed8b911da9af85d21adfcdaa3b9ff4016dee8be7c38c18fbee05579bfdd4c1",

            "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

            "schema": 1,

            "fps_num": 24000,

            "fps_den": 1001,

            "models": {

              "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

              "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

            },

            "encode_argv_normalized": [

              "--cpu-used=0",

              "--end-usage=q",

              "--passes=1",

              "--threads=4",

              "--row-mt=1",

              "--tile-columns=0",

              "--tile-rows=0",

              "--obu",

              "--bit-depth=10",

              "--input-bit-depth=10",

              "--limit=383",

              "--cq-level=<Q>",

              "-o",

              "<STREAM>",

              "<INPUT>"

            ],

            "decode_argv_normalized": [

              "--rawvideo",

              "--output-bit-depth=10",

              "-o",

              "<DECODED>",

              "<STREAM>"

            ],

            "score_argv_normalized": [

              "--reference",

              "<REFERENCE>",

              "--distorted",

              "<DECODED>",

              "--width",

              "1920",

              "--height",

              "1080",

              "--pixel_format",

              "420",

              "--bitdepth",

              "10",

              "--model",

              "path=v1.json:name=v1",

              "--model",

              "path=neg.json:name=neg",

              "--threads",

              "2",

              "--json",

              "--output",

              "metrics.json"

            ],

            "pooling": "arithmetic_mean_all_displayed_frames",

            "primary_metric": "v1",

            "aux_metric": "neg"

          },

          "profile_fingerprint": "9852960a430765009667cfd846cd7d356c395a9b239a6d4f4c4ed339d75d7321",

          "metric_scope": "383 displayed frames, arithmetic means",

          "decode_log_error_markers_absent": true

        },

        {

          "path": "benchmarks/aom-p0-q42",

          "status": "PASS_FULL_SEQUENCE",

          "q": 42,

          "bytes": 401681,

          "v1": 95.09700267101825,

          "neg": 92.80113969451706,

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

          "decoder_sha256": "feed8b911da9af85d21adfcdaa3b9ff4016dee8be7c38c18fbee05579bfdd4c1",

          "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

          "stream_sha256": "273783cd196823c218ae46b442c11f0af9540aa1fdc3be504a3eb40cb0d55a56",

          "profile": {

            "input_sha256": "70eec42e92f5e67c1f6055aa6380122c3cdf50ca3da2bfb72b8be56d50f31a22",

            "reference_sha256": "8c6899582907738163e9fcbee8b04a6963599edd71d875cad3971cae0758a701",

            "frames": 383,

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

            "decoder_sha256": "feed8b911da9af85d21adfcdaa3b9ff4016dee8be7c38c18fbee05579bfdd4c1",

            "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

            "schema": 1,

            "fps_num": 24000,

            "fps_den": 1001,

            "models": {

              "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

              "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

            },

            "encode_argv_normalized": [

              "--cpu-used=0",

              "--end-usage=q",

              "--passes=1",

              "--threads=4",

              "--row-mt=1",

              "--tile-columns=0",

              "--tile-rows=0",

              "--obu",

              "--bit-depth=10",

              "--input-bit-depth=10",

              "--limit=383",

              "--cq-level=<Q>",

              "-o",

              "<STREAM>",

              "<INPUT>"

            ],

            "decode_argv_normalized": [

              "--rawvideo",

              "--output-bit-depth=10",

              "-o",

              "<DECODED>",

              "<STREAM>"

            ],

            "score_argv_normalized": [

              "--reference",

              "<REFERENCE>",

              "--distorted",

              "<DECODED>",

              "--width",

              "1920",

              "--height",

              "1080",

              "--pixel_format",

              "420",

              "--bitdepth",

              "10",

              "--model",

              "path=v1.json:name=v1",

              "--model",

              "path=neg.json:name=neg",

              "--threads",

              "2",

              "--json",

              "--output",

              "metrics.json"

            ],

            "pooling": "arithmetic_mean_all_displayed_frames",

            "primary_metric": "v1",

            "aux_metric": "neg"

          },

          "profile_fingerprint": "9852960a430765009667cfd846cd7d356c395a9b239a6d4f4c4ed339d75d7321",

          "metric_scope": "383 displayed frames, arithmetic means",

          "decode_log_error_markers_absent": true

        },

        {

          "path": "benchmarks/aom-p0-q44",

          "status": "PASS_FULL_SEQUENCE",

          "q": 44,

          "bytes": 367342,

          "v1": 94.69445877545701,

          "neg": 92.39896300783296,

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

          "decoder_sha256": "feed8b911da9af85d21adfcdaa3b9ff4016dee8be7c38c18fbee05579bfdd4c1",

          "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

          "stream_sha256": "dd4645a955589d70ca0cdda9959e3d496deae4a0c31e8fe4eac46aed4542f672",

          "profile": {

            "input_sha256": "70eec42e92f5e67c1f6055aa6380122c3cdf50ca3da2bfb72b8be56d50f31a22",

            "reference_sha256": "8c6899582907738163e9fcbee8b04a6963599edd71d875cad3971cae0758a701",

            "frames": 383,

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13",

            "decoder_sha256": "feed8b911da9af85d21adfcdaa3b9ff4016dee8be7c38c18fbee05579bfdd4c1",

            "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

            "schema": 1,

            "fps_num": 24000,

            "fps_den": 1001,

            "models": {

              "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

              "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

            },

            "encode_argv_normalized": [

              "--cpu-used=0",

              "--end-usage=q",

              "--passes=1",

              "--threads=4",

              "--row-mt=1",

              "--tile-columns=0",

              "--tile-rows=0",

              "--obu",

              "--bit-depth=10",

              "--input-bit-depth=10",

              "--limit=383",

              "--cq-level=<Q>",

              "-o",

              "<STREAM>",

              "<INPUT>"

            ],

            "decode_argv_normalized": [

              "--rawvideo",

              "--output-bit-depth=10",

              "-o",

              "<DECODED>",

              "<STREAM>"

            ],

            "score_argv_normalized": [

              "--reference",

              "<REFERENCE>",

              "--distorted",

              "<DECODED>",

              "--width",

              "1920",

              "--height",

              "1080",

              "--pixel_format",

              "420",

              "--bitdepth",

              "10",

              "--model",

              "path=v1.json:name=v1",

              "--model",

              "path=neg.json:name=neg",

              "--threads",

              "2",

              "--json",

              "--output",

              "metrics.json"

            ],

            "pooling": "arithmetic_mean_all_displayed_frames",

            "primary_metric": "v1",

            "aux_metric": "neg"

          },

          "profile_fingerprint": "9852960a430765009667cfd846cd7d356c395a9b239a6d4f4c4ed339d75d7321",

          "metric_scope": "383 displayed frames, arithmetic means",

          "decode_log_error_markers_absent": true

        }

      ],

      "curve": {

        "status": "PASS_BRACKETED_INTERPOLATION",

        "bytes_log": 393123.4165415916,

        "bytes_linear": 393406.1888382225,

        "neg_at_v1_95": 92.70422551134399,

        "v1_weight": 0.7590258550983575,

        "sources": [

          "benchmarks/aom-p0-q44",

          "benchmarks/aom-p0-q42"

        ],

        "note": "V1 matched; NEG uses the same V1 weight and is not matched separately."

      }

    },

    "av2_basic": {

      "points": [

        {

          "path": "quality/basic-q88",

          "status": "PASS_FULL_SEQUENCE",

          "q": 88,

          "bytes": 5725392,

          "v1": 95.06545901305485,

          "neg": 93.27301328198432,

          "encoder_sha256": "8fcbd8e83f74a1f154ceb9cfaa03808124c5745032b3c5fa0c63d5057ee4181b",

          "decoder_sha256": "110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac",

          "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

          "stream_sha256": "e1a0b543adf243a841a8eede544edc93225d4098d58911820a9b8bb933e5db60",

          "profile": {

            "input_sha256": "70eec42e92f5e67c1f6055aa6380122c3cdf50ca3da2bfb72b8be56d50f31a22",

            "reference_sha256": "8c6899582907738163e9fcbee8b04a6963599edd71d875cad3971cae0758a701",

            "frames": 383,

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "encoder_sha256": "8fcbd8e83f74a1f154ceb9cfaa03808124c5745032b3c5fa0c63d5057ee4181b",

            "decoder_sha256": "110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac",

            "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

            "schema": 1,

            "fps_num": 24000,

            "fps_den": 1001,

            "models": {

              "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

              "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

            },

            "encode_argv_normalized": [

              "--input",

              "<INPUT>",

              "--output",

              "<STREAM>",

              "--export-recon",

              "<ENCODER_RECON>",

              "--frames",

              "383",

              "--qp",

              "<Q>",

              "--pred-structure",

              "low-delay-p",

              "--intra-select",

              "basic",

              "--partition-select",

              "adaptive",

              "--rd-mode",

              "pruned",

              "--motion-opt",

              "wide",

              "--entropy-adapt",

              "on",

              "--threads",

              "4"

            ],

            "decode_argv_normalized": [

              "--rawvideo",

              "--output-bit-depth=10",

              "-o",

              "<DECODED>",

              "<STREAM>"

            ],

            "score_argv_normalized": [

              "--reference",

              "<REFERENCE>",

              "--distorted",

              "<DECODED>",

              "--width",

              "1920",

              "--height",

              "1080",

              "--pixel_format",

              "420",

              "--bitdepth",

              "10",

              "--model",

              "path=v1.json:name=v1",

              "--model",

              "path=neg.json:name=neg",

              "--threads",

              "2",

              "--json",

              "--output",

              "metrics.json"

            ],

            "pooling": "arithmetic_mean_all_displayed_frames",

            "primary_metric": "v1",

            "aux_metric": "neg"

          },

          "profile_fingerprint": "471f7fa044fb2c072f9b18ece2d898d6f2b403d29a5a06172f98190bd17cb680",

          "metric_scope": "383 displayed frames, arithmetic means",

          "decode_log_error_markers_absent": true

        },

        {

          "path": "quality/basic-q100",

          "status": "PASS_FULL_SEQUENCE",

          "q": 100,

          "bytes": 3646077,

          "v1": 93.21044609921668,

          "neg": 91.60939480678854,

          "encoder_sha256": "8fcbd8e83f74a1f154ceb9cfaa03808124c5745032b3c5fa0c63d5057ee4181b",

          "decoder_sha256": "110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac",

          "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

          "stream_sha256": "d687cc8baa16cd94447cce938834014ab1fdc1fd7b7876937575e13f6335fc88",

          "profile": {

            "input_sha256": "70eec42e92f5e67c1f6055aa6380122c3cdf50ca3da2bfb72b8be56d50f31a22",

            "reference_sha256": "8c6899582907738163e9fcbee8b04a6963599edd71d875cad3971cae0758a701",

            "frames": 383,

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "encoder_sha256": "8fcbd8e83f74a1f154ceb9cfaa03808124c5745032b3c5fa0c63d5057ee4181b",

            "decoder_sha256": "110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac",

            "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

            "schema": 1,

            "fps_num": 24000,

            "fps_den": 1001,

            "models": {

              "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

              "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

            },

            "encode_argv_normalized": [

              "--input",

              "<INPUT>",

              "--output",

              "<STREAM>",

              "--export-recon",

              "<ENCODER_RECON>",

              "--frames",

              "383",

              "--qp",

              "<Q>",

              "--pred-structure",

              "low-delay-p",

              "--intra-select",

              "basic",

              "--partition-select",

              "adaptive",

              "--rd-mode",

              "pruned",

              "--motion-opt",

              "wide",

              "--entropy-adapt",

              "on",

              "--threads",

              "4"

            ],

            "decode_argv_normalized": [

              "--rawvideo",

              "--output-bit-depth=10",

              "-o",

              "<DECODED>",

              "<STREAM>"

            ],

            "score_argv_normalized": [

              "--reference",

              "<REFERENCE>",

              "--distorted",

              "<DECODED>",

              "--width",

              "1920",

              "--height",

              "1080",

              "--pixel_format",

              "420",

              "--bitdepth",

              "10",

              "--model",

              "path=v1.json:name=v1",

              "--model",

              "path=neg.json:name=neg",

              "--threads",

              "2",

              "--json",

              "--output",

              "metrics.json"

            ],

            "pooling": "arithmetic_mean_all_displayed_frames",

            "primary_metric": "v1",

            "aux_metric": "neg"

          },

          "profile_fingerprint": "471f7fa044fb2c072f9b18ece2d898d6f2b403d29a5a06172f98190bd17cb680",

          "metric_scope": "383 displayed frames, arithmetic means",

          "decode_log_error_markers_absent": true

        }

      ],

      "curve": {

        "status": "PASS_BRACKETED_INTERPOLATION",

        "bytes_log": 5634943.66042809,

        "bytes_linear": 5652017.897472315,

        "neg_at_v1_95": 93.21430812544672,

        "v1_weight": 0.9647123680021135,

        "sources": [

          "quality/basic-q100",

          "quality/basic-q88"

        ],

        "note": "V1 matched; NEG uses the same V1 weight and is not matched separately."

      }

    },

    "av2_fine16v2": {

      "points": [

        {

          "path": "quality/fine16-v2-q88",

          "status": "PASS_FULL_SEQUENCE",

          "q": 88,

          "bytes": 5480357,

          "v1": 95.2794215979113,

          "neg": 93.45381638903392,

          "encoder_sha256": "e13591438eb6cacb354676b55e090433c6a05a0419030b3871c77f8c5227e756",

          "decoder_sha256": "110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac",

          "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

          "stream_sha256": "5e15e3a13e7d278b0f62762eef35bd6f1884cb3aaeb637fae5611a6617b107a6",

          "profile": {

            "input_sha256": "70eec42e92f5e67c1f6055aa6380122c3cdf50ca3da2bfb72b8be56d50f31a22",

            "reference_sha256": "8c6899582907738163e9fcbee8b04a6963599edd71d875cad3971cae0758a701",

            "frames": 383,

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "encoder_sha256": "e13591438eb6cacb354676b55e090433c6a05a0419030b3871c77f8c5227e756",

            "decoder_sha256": "110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac",

            "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

            "schema": 1,

            "fps_num": 24000,

            "fps_den": 1001,

            "models": {

              "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

              "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

            },

            "encode_argv_normalized": [

              "--input",

              "<INPUT>",

              "--output",

              "<STREAM>",

              "--export-recon",

              "<ENCODER_RECON>",

              "--frames",

              "383",

              "--qp",

              "<Q>",

              "--pred-structure",

              "low-delay-p",

              "--intra-select",

              "basic",

              "--partition-select",

              "adaptive",

              "--rd-mode",

              "pruned",

              "--motion-opt",

              "wide",

              "--entropy-adapt",

              "on",

              "--threads",

              "4"

            ],

            "decode_argv_normalized": [

              "--rawvideo",

              "--output-bit-depth=10",

              "-o",

              "<DECODED>",

              "<STREAM>"

            ],

            "score_argv_normalized": [

              "--reference",

              "<REFERENCE>",

              "--distorted",

              "<DECODED>",

              "--width",

              "1920",

              "--height",

              "1080",

              "--pixel_format",

              "420",

              "--bitdepth",

              "10",

              "--model",

              "path=v1.json:name=v1",

              "--model",

              "path=neg.json:name=neg",

              "--threads",

              "2",

              "--json",

              "--output",

              "metrics.json"

            ],

            "pooling": "arithmetic_mean_all_displayed_frames",

            "primary_metric": "v1",

            "aux_metric": "neg"

          },

          "profile_fingerprint": "8f6f147759dab7a30c29f8e2c8865d4c1de12f82d690c702cd516d533216e9b4",

          "metric_scope": "383 displayed frames, arithmetic means",

          "decode_log_error_markers_absent": true

        },

        {

          "path": "quality/fine16-v2-q100",

          "status": "PASS_FULL_SEQUENCE",

          "q": 100,

          "bytes": 3462815,

          "v1": 93.4774089895561,

          "neg": 91.81173531592688,

          "encoder_sha256": "e13591438eb6cacb354676b55e090433c6a05a0419030b3871c77f8c5227e756",

          "decoder_sha256": "110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac",

          "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

          "stream_sha256": "7f4acaaa17910befe5db593529e5d5466ee38a8fa9450860ae33193e686eafaa",

          "profile": {

            "input_sha256": "70eec42e92f5e67c1f6055aa6380122c3cdf50ca3da2bfb72b8be56d50f31a22",

            "reference_sha256": "8c6899582907738163e9fcbee8b04a6963599edd71d875cad3971cae0758a701",

            "frames": 383,

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "encoder_sha256": "e13591438eb6cacb354676b55e090433c6a05a0419030b3871c77f8c5227e756",

            "decoder_sha256": "110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac",

            "scorer_sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5",

            "schema": 1,

            "fps_num": 24000,

            "fps_den": 1001,

            "models": {

              "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

              "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

            },

            "encode_argv_normalized": [

              "--input",

              "<INPUT>",

              "--output",

              "<STREAM>",

              "--export-recon",

              "<ENCODER_RECON>",

              "--frames",

              "383",

              "--qp",

              "<Q>",

              "--pred-structure",

              "low-delay-p",

              "--intra-select",

              "basic",

              "--partition-select",

              "adaptive",

              "--rd-mode",

              "pruned",

              "--motion-opt",

              "wide",

              "--entropy-adapt",

              "on",

              "--threads",

              "4"

            ],

            "decode_argv_normalized": [

              "--rawvideo",

              "--output-bit-depth=10",

              "-o",

              "<DECODED>",

              "<STREAM>"

            ],

            "score_argv_normalized": [

              "--reference",

              "<REFERENCE>",

              "--distorted",

              "<DECODED>",

              "--width",

              "1920",

              "--height",

              "1080",

              "--pixel_format",

              "420",

              "--bitdepth",

              "10",

              "--model",

              "path=v1.json:name=v1",

              "--model",

              "path=neg.json:name=neg",

              "--threads",

              "2",

              "--json",

              "--output",

              "metrics.json"

            ],

            "pooling": "arithmetic_mean_all_displayed_frames",

            "primary_metric": "v1",

            "aux_metric": "neg"

          },

          "profile_fingerprint": "8f6f147759dab7a30c29f8e2c8865d4c1de12f82d690c702cd516d533216e9b4",

          "metric_scope": "383 displayed frames, arithmetic means",

          "decode_log_error_markers_absent": true

        }

      ],

      "curve": {

        "status": "PASS_BRACKETED_INTERPOLATION",

        "bytes_log": 5103791.062263227,

        "bytes_linear": 5167515.232478899,

        "neg_at_v1_95": 93.19919391013447,

        "v1_weight": 0.8449391549117189,

        "sources": [

          "quality/fine16-v2-q100",

          "quality/fine16-v2-q88"

        ],

        "note": "V1 matched; NEG uses the same V1 weight and is not matched separately."

      }

    }

  },

  "latency_note": "AOM default lookahead/altref vs AV2 low-delay-P: compression anchor, not matched latency.",

  "user_normalized_inter_target": 0.95,

  "comparisons": {

    "av2_basic": {

      "status": "PASS_BRACKETED_COMPARISON",

      "current_pipeline_bytes_ratio": 14.333777697599771,

      "bytes_linear_sensitivity_ratio": 14.36687591052756,

      "normalized_inter_factor": null,

      "normalization_note": "Matched full-sequence best all-intra baselines are absent; no intra/inter product decomposition is claimed."

    },

    "av2_fine16v2": {

      "status": "PASS_BRACKETED_COMPARISON",

      "current_pipeline_bytes_ratio": 12.98266866716462,

      "bytes_linear_sensitivity_ratio": 13.135317590552441,

      "normalized_inter_factor": null,

      "normalization_note": "Matched full-sequence best all-intra baselines are absent; no intra/inter product decomposition is claimed."

    }

  },

  "fine16_vs_basic": {

    "status": "PASS_BRACKETED_COMPARISON",

    "current_pipeline_bytes_ratio": 0.905739501550844,

    "bytes_linear_sensitivity_ratio": 0.9142779315666898,

    "normalized_inter_factor": null,

    "normalization_note": "Matched full-sequence best all-intra baselines are absent; no intra/inter product decomposition is claimed."

  },

  "status": "PASS_ALL_COMPARISONS_READY"

}

</details>


<details>
<summary>052 · REPORT.html</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\inter-20261028\REPORT.html`  
原文件 SHA-256：`ab4295e990e40623649fa109e4ada443ffac6dd59ace5cb3148ddd640aa4075f`

SVT-AV2 M6 帧间优化首轮报告

body{font:16px/1.7 system-ui,"Microsoft YaHei",sans-serif;color:#172337;background:#f5f7fb;margin:0}main{max-width:1160px;margin:32px auto;background:white;padding:40px 48px;border-radius:12px}h1{font-size:30px;line-height:1.25}h2{margin-top:36px;border-top:1px solid #dbe3ec;padding-top:24px}h3{margin-top:24px}p{max-width:100ch}table{width:100%;border-collapse:collapse;font-size:14px;margin:16px 0;display:block;overflow:auto}th,td{text-align:left;border-bottom:1px solid #dde3ec;padding:9px 12px;vertical-align:top}th{background:#edf3f9;white-space:nowrap}td:nth-child(n+2){font-variant-numeric:tabular-nums}code,pre{font-family:ui-monospace,Consolas,monospace}code{overflow-wrap:anywhere}pre{padding:16px;background:#f0f4f8;white-space:pre-wrap;overflow-wrap:anywhere;font-size:13px}a{color:#1358a0;overflow-wrap:anywhere}.lead{padding:20px 24px;background:#edf4ff;border-left:5px solid #2c69aa}.caution{padding:16px 20px;background:#fff6e5;border-left:5px solid #ad750a}.muted{color:#55657a;font-size:14px}img{max-width:100%;height:auto;border:1px solid #cbd5e1}li{margin:7px 0}@media print{body{background:white}main{margin:0;padding:0}h2{break-before:auto}table{font-size:11px}}

SVT-AV2 M6 帧间优化：代码、实测与下一步

生成时间 2026-10-28T17:41:23.899146+08:00 · 独立分支 inter/m6-20261028 · 源码提交

d244374b79417fa8a624b1f35aaaaf0afa8b562b

· 产品版本保持 0.0.5-m5

本轮完成了真实的帧间代码改动和验证，但没有达到 0.95× AOM p0 的目标。

Adaptive CDF 在完整 383 帧、同 QP100 下减少 13.2453% 体积，V1 与 NEG 略升。P16 v2 相对 Basic P 在 V1=95 的体积改善为 9.4260%。当前 P16 整条编码路径与 AOM p0 在 V1=95 的体积比为 12.982669×。它不是已经归一化的“纯帧间因子”。

帧间代码仍使用单个已重建参考、整数运动搜索和 basic intra 回退；完整最佳 R6 帧内工具尚未全部接入 P，也没有移植完整的 SVT-AV1 lookahead、帧调度和 EncDec 波前系统。本轮的多线程落地是确定性的 ME 预分析阶段。

1. 目标与比较口径

使用用户指定的 1080p 2D 素材。目录名标称 15s，实测为

383 帧、1920×1080、10-bit 4:2:0、24000/1001 fps，时长 15.9742917s

。所有整段对照保留全部 383 帧，没有截成 360 帧。

主指标为固定的 VMAF v1 模型

vmaf_v1.0.16_3d0h.json

；NEG 为

vmaf_v0.6.1neg.json

。每帧序号必须严格为 0…382，所有值有限，以完整显示帧的算术平均计算。AV2 的质量来自固定官方 AVM 解码输出，并要求与编码器导出重建逐字节相同。模型、工具、输入、码流和重建 SHA 保存在各 result.json。

AOM 使用 p0、单遍恒定质量、4 线程、row-mt、单 tile、默认 lookahead/altref。当前 AV2 使用 low-delay-P，因此 AOM 是压缩率锚点；本轮没有建立相同延迟约束下的公平速度比较。既有 SVT p6 文件重新按同一模型评分，体积取 packet payload 合计 801,583 B，MKV 容器总计 805,088 B。原记录的另一个 VMAF 模型不能混入本轮曲线。

每个配置组必须使用同一输入、模型、编码器与完整参数指纹。只有真实测点跨过 V1=95 时，才在 V1–ln(bytes) 平面相邻插值；另列线性 bytes 插值作为敏感性参考。NEG 沿相同 V1 权重计算，不再单独匹配 NEG。完整规则见

benchmarks/PROTOCOL.md

，审计实现见

collect_comparison_v3.py

。

2. 完整 383 帧结果

配置

体积 / B

VMAF v1

NEG

状态

AOM AV1 p0 Q30

767,974

96.971046

94.599830

PASS_FULL_SEQUENCE

AOM AV1 p0 Q36

540,727

96.153469

93.838498

PASS_FULL_SEQUENCE

AOM AV1 p0 Q42

401,681

95.097003

92.801140

PASS_FULL_SEQUENCE

AOM AV1 p0 Q44

367,342

94.694459

92.398963

PASS_FULL_SEQUENCE

既有 SVT-AV1 p6 CRF36

801,583

95.920442

93.287117

PASS_FULL_SEQUENCE

冻结基础 P Q100

4,344,216

93.017386

91.593729

PASS_FULL_SEQUENCE

Adaptive CDF P Q100

3,768,812

93.078248

91.617521

PASS_FULL_SEQUENCE

GLOBALMV 路线 Q92

4,978,711

94.428718

92.754260

PASS_FULL_SEQUENCE

Basic P Q88

5,725,392

95.065459

93.273013

PASS_FULL_SEQUENCE

Basic P Q100

3,646,077

93.210446

91.609395

PASS_FULL_SEQUENCE

P16 v2 Q88

5,480,357

95.279422

93.453816

PASS_FULL_SEQUENCE

P16 v2 Q100

3,462,815

93.477409

91.811735

PASS_FULL_SEQUENCE

从冻结基础P到当前P16 v2，完整Q100端到端体积下降

20.2891%

，V1提高0.460023，NEG提高0.218006。这是同QP实测差异，仍位于V1约93的区间。

GLOBAL、Basic 与 P16 为不同配置，不能交叉拼接曲线。Baseline→Adaptive 的 13.2453% 是同 QP 的直接结果，不能写成完整区间 BD-rate，也不能与后续百分数无条件相乘。

VMAF v1=95 的体积

配置组

log-bytes 插值 / B

linear-bytes 参考 / B

沿 V1 权重的 NEG

状态

AOM p0

393,123.42

393,406.19

92.704226

PASS_BRACKETED_INTERPOLATION

Basic P

5,634,943.66

5,652,017.90

93.214308

PASS_BRACKETED_INTERPOLATION

P16 v2

5,103,791.06

5,167,515.23

93.199194

PASS_BRACKETED_INTERPOLATION

完整计算和参数指纹见

comparison-v3.json

。本轮 P16 对 Basic 的改善为

9.4260%

；P16 对 AOM p0 的整条路径体积比为

12.982669×

。若帧内因子确为 1×，0.95× AOM 的条件目标体积约为

373,467 B

，当前结果明显未到达。

当前完整 P 曲线仍不是“完整最佳帧内能力已接入所有帧间回退”的最终底座。

不把这条曲线强行命名为用户要求的归一化帧间系数，也不拿此前单帧 R6 数据代替完整 383 帧 all-intra 曲线。

3. 已落地的代码

改动

实现与作用

验证与使用边界

Adaptive P CDF

每个 RD trial 使用私有 CDF 副本，只有最终 winner 提交；P 仍 PRIMARY_REF_NONE，逐帧初始化。

完整 QP100 码量下降、两项质量略升；27 次小素材官方解码一致，关闭适配时保留冻结流。

ME 阶段拆分

不可变 source / committed reference → 分段 worker → join barrier → 顺序 RD/recon/CDF。Windows CreateThread、Linux pthread；线程创建失败的 lane 同步完成。

1、4、自动线程确定性。没有把共享 CDF 或重建邻居直接并行写入。

Identity GLOBALMV

零 MV 时用完整真实语法成本比较 GLOBAL、NEAR、NEW。

这是 identity global 模式，不是仿射运动估计；语法和官方解码通过。

P 内 basic intra

允许七种基础帧内模式作为 P 的回退，实际模式传入完整 rate。

仅受限 basic/DCT 域；尚未等价于完整最佳 R6。

P16 v2

在完整 64×64 SB 上增加 16×16 方案，比较全 INTER fine ME、继承 coarse 模式及实际 wide MV、继承模式但使用 fine ME 三种种子。

固定 tie 顺序，快照与回滚，最终完整重放核对 D/rate/hash；完整整段曲线见上。

P16 tuple refine

固定 raster 的一次坐标改进，在已有三种 mode/UV/MV tuple 中选择；每次重建受影响后缀并重新计算整 SB rate。

18 次真实 AVM 一致；前32帧有收益。还没有这项追加改动的完整383帧曲线。

R6-subset 首 key

显式

--key-intra-profile r6-subset

，仅首 key 使用 directional / mixed8 / full Y TX / RDOQ / R6 AI lambda 子集，P 保留原配置。

6 组 key→P→P 与6组冻结 subset 检查；CfL/MRL/MHCCP 和滤波未加入该 key 子集。

Fine16 ME prepass

追加16×16 narrow jobs，1080p每个P为10,080个 narrow job；开启wide prepass时仍仅2,400个 coarse wide job。

36 次小素材身份检查、9次 AVM 解码；前32帧与冻结P16 v2的流及recon SHA完全相同。

最终 packet 诊断

只读最终实际 modes / qcoeffs，统计可见像素、分区、零 MV、自然 skip 和非零系数。

不把搜索候选当作最终模式分布；不把系数个数当作bit占比。

新研究宏默认关闭。完整质量结果来自各 manifest 中冻结、哈希固定的构建；不是仓库默认无参数构建。编译配方、运行命令与冻结二进制共同保留。

4. 前 32 帧筛选：与整段结果分开

配置

体积 / B

VMAF v1

NEG

Basic Q100

347,420

94.178559

92.691024

P16 v2 Q100

327,562

94.493926

92.902792

P16 tuple refine Q100

323,659

94.516677

92.962928

R6-subset 首 key Q100

317,437

94.593641

92.978445

Fine16 并行 prepass Q100

327,562

94.493926

92.902792

INTER-Y RDOQ Q100

282,783

91.837124

89.365068

INTER-Y RDOQ Q88

383,481

94.067592

91.498262

强制 residual skip Q100

301,579

89.316342

87.279695

Tuple refine 对 P16 v2 在这个同 QP 前缀上再减 1.1915% 字节，V1/NEG 略升；R6 key-subset 是另一条独立实验支路，不能将两个百分数直接当作已实测的组合收益。Fine16 prepass 与 P16 v2 的数值完全相同，因为它只改变调度；本轮负载混杂，不以观测到的 walltime 差异报告加速比。

保留关闭的试验

INTER-Y RDOQ：

事务、语法和官方解码门禁通过，但前32帧 Q100 省13.6704%字节时 V1下降2.6568；补测Q88仍被原P16 Q100直接支配：原版更小，V1与NEG都更高。因此保持

M6_INTER_RDOQ_POST=0

，不启动这个方案的完整383帧曲线。详见

inter-rdoq-quality-screen.json

。

强制 residual skip：

语法正确，但已测前缀质量下降过大，保持关闭；没有把单点结果写成负 BD-rate。

DRL1：

第二空间 MV 候选语法与解码通过，但1080p三帧筛选不优，未纳入推荐质量配置。相关试验结果与默认关闭开关均保留，便于继续研究。

5. 根因诊断与帧内/帧间边界

对前32帧的最终packet记录，排除K0，P1–31的INTER像素占比为

83.4129%

，自然skip占INTER像素

46.9876%

，零MV占INTER像素

85.3576%

。帧间已经大面积工作，不能把高体积概括成“没有用上inter”。

剩余16.5871%的帧内回退像素包含76.4405%的非零系数。这是系数数量分布，尚无逐语法bit分解。11个高码量P帧合计占这31个P帧码量的87.7674%；frame31单帧61,221 B，占20.3200%，98.7654%像素回退到intra。查看原始source frame29/30/31/32后，确认31为切镜：由手放在头发上的特写切到两个人物的中景。

行序：29、30 / 31、32。仅用于定位切镜的亮度缩略图，指标计算始终使用完整10-bit YUV。

统计方法使用整数总数求和后再算比例，不平均每帧ppm。所有32条记录唯一、连续、覆盖完整画面，packet字节总和与码流总长相等。原始聚合和完整逐帧记录见

analysis/prefix32-fine16-prepass-q100.json

；分析脚本见

analyze_diagnostics.py

。

6. 验证证据与限制

新代码最后一轮：MSVC POST ON/OFF 各11项、RETAIN反序/回放测试、GCC key+refine+prepass+POST组合11项通过；包括旧R6 RDOQ路径回归。

P RDOQ 外部验证：9个RETAIN完整流身份，27个ON编码覆盖线程1/4/auto，9个entropy OFF身份，9次固定AVM解码；首key在45个编码中保持原流/recon。其中6组4:2:0素材覆盖POST实际运行，3组10-bit4:4:4素材验证格式守卫回退；27个ON编码和9次AVM解码包含这两类验证。

完整最佳R6（CfL/MRL/MHCCP/fullTX/RDOQ/mixed8等既有配置）在8-bit420、10-bit420、10-bit444的最终回归中，码流与重建仍与冻结combo.exe相同，并再次通过固定AVM解码。证据见

intra-preservation-final/results.json

。

逆序枚举比较每个POST strength的ID、distortion、完整SB rate和cost，以及最终winner/coeff/recon；coeff_accepts是临时改善次数，不当作最终写出的RDOQ块数。

历史宽MV回滚fixture的10条流均通过解码一致性，但未覆盖预期的“fine试探均败、保留粗层wide winner”场景：继承wide MV的fine种子实际获胜，导致coverage断言失败。原记录保留；后续RETAIN控制在执行全部fine试探后强制保留旧结果，验证码流与重建身份，并覆盖实际wide MV。历史coverage失败不改写为一次全绿。

所有计时均在并行帧内会话和多个编码任务共存时采集，仅作诊断；没有达到预设速度门槛或多线程线性扩展的结论。

当前源干净且已本地提交，工作位于

<LOCAL_PRIVATE>\ffmpeg-native-m6-tests\inter-20261028\source

。修改仅发生在这个独立副本；并行帧内会话的工作树没有被本会话写入。

7. “帧内 × 帧间”的归一化

在相同素材、质量和比较协议下，定义：

R_I = B_AV2_AI / B_AOM_AI

R_T = (B_AV2_INTER / B_AV2_AI) / (B_AOM_INTER / B_AOM_AI)

R_total = R_I × R_T = B_AV2_INTER / B_AOM_INTER

用户的1×指帧内体积基线，不是编码耗时。若将来实测帧内R_I=0.85，且归一化帧间R_T=0.95，则总体目标为0.8075。这是条件关系，并非本轮已测结果。

本轮缺少双方最佳all-intra在完整383帧上、V1=95附近的曲线；当前P回退也未全部使用完整最佳R6。因此

normalized_inter_factor=null

。此前单帧或48-band R6指标不能代入完整视频公式。首key变强、P预测改进与后续参考误差会相互作用，独立试验百分数也必须通过组合实测确认。

8. 下一阶段的具体入口

分开“实际帧序号”和“本帧KEY/P决策”。

现有代码把非零帧号视为P。首先接入显式流内KEY请求，再由场景分析产生同一决策。实际frame n和reference index连续，sequence仍只在0写入，KEY的order_hint为n&255；不能把中途KEY伪装成frame0。

将R6能力接入真正需要帧内回退的位置。

现有first-key subset可先用于切镜的闭环KEY；完整CfL/MRL/MHCCP、变换和滤波需把sequence能力与逐帧搜索域分开，并保证正式writer、RD语法和参考像素一致。不要直接删除旧guard。

扩展预测能力与参考组织。

当前仍是整数、单参考、低延迟P。亚像素MC、多参考/compound和分层帧结构需要相应的真实AV2语法与固定AVM验证。TIP、光流/细化等官方新工具应在正确参考/帧调度基础上逐项进入，不宣称本轮已实现。

继续按SVT分阶段的依赖关系拆分。

ME预分析已经落地；下一步是持久worker池、明确的帧/参考生命周期和有界队列，再处理PD/lookahead与EncDec行/带依赖。只在不可变阶段并行，依赖屏障必须先明确。

维持同一V1=95协议。

每次候选先解码一致性、局部筛选，再跑同配置多点完整曲线；NEG继续辅助观察。帧内0.85的新结果可通过独立模块或后续合并进入，不应被当前帧间实验覆盖。

中途KEY的规范路径已只读核对固定AVM：CLOSED_LOOP_KEY可复用已保存sequence、重置decoder参考map、隐式刷新唯一参考槽，并开启新的display-order epoch。编码器应保留reference对象及实际index，只在KEY重建时不读取旧reference，成功后以n正常prepare/commit。该设计尚未实现，不能计作本轮能力。

9. 源码、复现与审计入口

包内包含当前完整跟踪源码、从冻结R6基线导出的patch/提交序列、构建与评分脚本、冻结研究二进制、结果JSON、metrics和相关门禁日志。大体积原片及raw重建留在设备工作目录，不包含在包内。脚本保留当前设备上的素材和工具路径；在原工作目录重跑时请使用新label，避免覆盖冻结结果。

python run_inter_quality.py --encoder fine16-v2.exe --label repeat-fine16-q88 --qp 88 --adapt on --threads 4 --frames 383 --intra basic



python run_inter_quality.py --encoder key-subset.exe --label repeat-key-subset-prefix32 --qp 100 --adapt on --threads 4 --frames 32 --intra basic --key-intra-profile r6-subset



python collect_comparison_v3.py

第二条是有意受限的首key子集试验；不是完整最佳R6已移植到所有P。RDOQ和DRL1、forced skip不要混入这两条已列出的配置。构建脚本以当前源为输入，但历史质量点仍以其记录的冻结binary SHA为准。

提交记录

07ca26f Enable transactional adaptive entropy RD for inter frames

23b0eb0 Evaluate inter residual skip with complete causal SB rate-distortion replay

02cd509 Precompute frame-local motion lists with deterministic parallel workers

79a4088 Choose identity GLOBALMV by transactional syntax rate cost

fc119dc inter: allow guarded seven-mode intra candidates in low-delay P RDO

a06c104 inter: precompute wide ME candidates in deterministic worker lanes

53d5b7d inter: add guarded second spatial MV predictor syntax experiment

861546a inter: add retained-winner P16 partition seed experiment

af34659 Refine P16 seeds with coarse mode and wide-MV inheritance

e231acb Add guarded P16 tuple refinement and first-key R6 intra subset

d244374 Add deterministic P16 preanalysis and guarded inter RDOQ research

主要证据

baseline-manifest.json

：冻结R6的文件身份；

prepass-rdoq-source.json

：最新源与二进制关系。

comparison-v3.json

与

REPORT-DATA.json

：整段曲线、参数指纹与结论数值。

fine16-v2-validation/results.json

、

fine16-controls/results.json

、

fine16-refine-validation/results.json

：P16及回滚。

key-subset-validation/results.json

、

fine16-prepass-validation/results.json

、

fine16-prepass-prefix32-identity.json

：首key子集、确定性prepass。

inter-rdoq-validation/results.json

、

inter-rdoq-quality-screen.json

：语法通过但质量筛选未晋升。

intra-preservation-final/results.json

、

linux-inter-rdoq-results.json

与各final-gate日志：最终回归。

规范与实现依据

AOMedia AV2 Bitstream & Decoding Process Specification v1.0.0

；固定本地AVM提交

01803db6afa5a0616702a3814fc5934ab9e99adf

。SVT架构审计针对本机

<LOCAL_PRIVATE>\ffmpeg-compare\source\svt-simple

，包含本地CONFIG_SINGLE_THREAD_KERNEL修改，不能把该副本所有行为归为未经改动的上游SVT。

本报告区分规范依据、代码正确性、实际压缩结果与未来设计。目标0.95×尚未达到；首轮可复核成果是已提交的实现、冻结数据和明确的后续瓶颈。

</details>
