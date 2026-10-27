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

外置目录`C:/Private/ffmpeg-native-m6-tests/optimization-r4-20261018/a/V2/`，含Windows/GCC构建与测试日志、`delivery-hashes.json`、相对冻结V1的`rd_frame.diff`和`test_m6_coeff.diff`。V1的A构建在其父目录，保持原程序。当前V2源码稳定，等待B效果与00集成验收；不以机制PASS替代压缩价值。
