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
