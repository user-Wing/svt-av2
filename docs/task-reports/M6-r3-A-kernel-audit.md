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

外置证据`C:/Private/ffmpeg-native-m6-tests/optimization-r3-20261017/a/`：`quantization.before.c`、`probe/bench.c`、`probe-timing.json`、Windows/GCC构建与测试log及delivery-hashes.json。本轮不启新长测，不改冻结campaign、媒体、模型、依赖或lambda。正式版不变。
