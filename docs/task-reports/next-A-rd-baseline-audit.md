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
