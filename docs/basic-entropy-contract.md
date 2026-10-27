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
