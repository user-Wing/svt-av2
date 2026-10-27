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

根目录`C:/Private/ffmpeg-native-m6-tests/optimization-r6-20261018/a/`；入口`delivery-hashes.json`、相对`root/baseline-source`的五个生产diff、Windows/GCC构建和四个机制测试日志、两组extreme-proof/result.json。R5程序/构建目录未覆盖。

A没有另跑真实效果矩阵。B沿用事前同native24帧样本和四QP，用R5质量候选可审计复用作基线，并做鲜配对单路速度。局部J不劣不保证后续参考链或VMAF收益，≤80位置不等于运行时间百分比。所有新增计算前检查原campaign为real-quality；速度窗口中A停止新增计算，原冻结长测继续。默认off兼容、检测器、跨平台流以及最终效果判定由00独立验收。
