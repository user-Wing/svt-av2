# R7 A：帧内概率自适应首关

2026-10-18。遵循`docs/basic-entropy-contract.md`；R5/R6源码、程序、原质量任务均不覆盖。正式版本保持0.0.5-m5。新增构建和证据在`C:/Private/ffmpeg-native-m6-tests/optimization-r7-20261018/a`。本报告不宣称真实1080p压缩/速度结果。

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
