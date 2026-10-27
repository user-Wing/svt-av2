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
