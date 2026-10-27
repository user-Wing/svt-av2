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
