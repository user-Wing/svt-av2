# M6 后续实验合同：有界系数 RD 精化

2026-10-18，用户要求在冻结长测并行期间继续实际压缩效率优化。本合同只约束新隔离候选；原 `m6-rd-contracts.md`、长测快照/程序/配置及其结果不变。正式仍0.0.5-m5，不预设每轮减半或总体收益。

## 接口与属主

- 00：公共API/Codec/CLI、CMake、API测试、合同和独立集成验收。
- A：rd_frame.c/h与新test_m6_coeff.c；B：新m6_r4评测工具、预冻结样本、独立AVM/效果验证。唯一文件写者不重叠。
- 公共config尾追加 `coeff_optimization`，参数 `SVT_AV2_PARAM_COEFF_OPTIMIZATION`，CLI `--coeff-opt off|rd`，默认off；值1要求rd_mode为exhaustive或pruned，rd-mode off下明确拒绝。不允许运行中切换。public/internal RD stats尾追加coeff_trials/coeff_accepts，CLI仅开启时新增独立coeff统计行。全部调用者重新编译，不混用旧DLL。
- build/artifacts外置于 `C:/Private/ffmpeg-native-m6-tests/optimization-r4-20261018/{root,a,b}`；原173源及完整源码副本在root/baseline-source冻结。

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
