# 基础能力补齐：集成进展

2026-10-18，00+A/B，本轮R5/R6/R7代码、质量与独立性能验收完成，M6总体仍RUNNING，正式0.0.5-m5不变。用户授权先补基础能力，再引入AVM新技术，使用其1080p原片；长测结束不作为开发前置。执行合同为`docs/basic-capability-contract.md`，最终结论见`M6-basic-capability-assessment.md`。本文不把本轮三项候选称为整个基础阶段完成。

## 阶段R5：整数运动候选RD

代码已完成，默认关闭的`--motion-opt rd`将原±8整数搜索的精确SAD前三名和零MV去重后交给真实重建D/共享语法R比较。先保留原RD赢家，随后固定树逐叶尝试inter，允许原intra叶改inter，最多16额外完整路径；每次重建全部叶并正确回滚、重放。系数优化互斥，原lambda不改。

00独立严格Windows47/47、GCC46/46、ASan/UBSan/LSan四项通过；最终新增机制测试8658/0，原RD8813/0、coeff231084/0。6组默认off与R4基线的流/recon/原RD trace一致；三格式AI/P的6组motion实际编码MSVC/GCC完整流/recon/trace一致，固定AVM逐可见样本一致。A报告`R5-A-motion-rd.md`。

180源及完整候选源码冻结在`optimization-r5-20261018/root/source-manifest.json`与`candidate-source`，原176源中9项改变；Windows程序SHA256 `d6e2c1c1f9ce20d057b177eb9f66b15f0d6561d5421410bb8db471678b6fd705`已由B独立复制固定。后续源码开发不覆盖它。

原生质量已12/12配对、24条完整AVM/PSNR/VMAF通过，独立共同性能亦已完成。B事前统一选三类原生1920×1080、10bit420、24000/1001的帧48..71，24帧约1.001秒；逐帧payload与原完整解码文件相同，原H264和local Y4M只读。三源、QP80/100/120/140、P/pruned/adaptive、coeff-off全部保留。2D/3D/Real的PCHIP VMAF BD为−6.892/−2.018/−2.684%，PSNR-Y BD为−15.511/−1.635/−2.498%；00独立分段线性VMAF为−6.330/−1.798/−2.343%，PSNR-Y为−14.979/−1.744/−2.699%，方向一致但不外推其它片段。少数单点VMAF小降仍如实保存。

00核验全部24保留流的SHA/字节、源/程序/脚本/评分依赖、24帧指标及SSE重算PSNR通过，另三源各基线/候选六条重新固定AVM解码的raw SHA与已保存内部重建指纹一致。证据`optimization-r5-20261018/root/native-quality-audit.json`及`native-avm-spotcheck/verification.json`。首条2D/QP80基线编码218.87秒属于含recon导出的质量耗时，不作速度结论。

质量首次首对顺序预检后最多两路，其余所有源/QP保持冻结。正式速度在首次数据产生前已修订为R4/R5/R6共同窗口：QP100，单路，每源三版各两帧暖机后ABC/BCA/CAB三轮，记录进程CPU/墙时/后台负载，不导出recon或运行解码/VMAF。该窗口暂停全部新增编译/评测，原冻结长测继续，所以不声称整机隔离。事前冻结joint-protocol另绑三程序与参数，旧ABBAAB工具仅保留复现。报告`M6-r5-B-validation.md`及外置b目录保存独立原始数据。

## 阶段R6：有界±16整数搜索

在R5候选效果尚未产生之前，依固定AVM整数语法边界和成本预算确定下一项：保留完整R5赢家，增加外圈步4粗网格56点、固定最佳粗点±2局部细化最多24点，每叶最多80新增SAD、两条外域MV候选，后置最多8完整RD路径。不是完整±16全搜，也不引入亚像素或新的wire工具。

本实现MV±8/delta±16是人工guard。固定AVM的onepel shell set0覆盖class0..6；MV±16和同范围邻居产生最大每轴delta32、shell64，现有编码支路可覆盖。±32最坏shell128需要当前未实现的set1分支，禁止只放宽guard；具体行证据见`R6-A-motion-range-design.md`。R6需要额外极限码流/固定AVM逐样本验证，而不只是范围函数返回成功。

R6使用`--motion-opt wide`、独立源码/程序/构建与结果目录`optimization-r6-20261018`，不覆盖R5正在测试的程序或数据。184源/完整源码及程序已冻结，Windows程序SHA256 `c76e193b26be5a300684df3ff1e88862facbad85eb8be209c78d05b1c62bed2f`。00独立严格Windows48/48、GCC47/47、ASan/UBSan/LSan五项通过；新增wide机制10295/0，六组off对R4和六组rd对R5流/recon/trace不变。三格式AI/P跨平台及固定AVM验证通过，另三格式受控+12位移均实际接受32次wide候选；正负极限MV/delta/shell六条非平坦流由00再次AVM逐样本确认。详见`R6-A-wide-motion.md`。

R6原生质量12/12通过，复用经指纹核验的R5候选质量点作基线，仅编码12个新候选。2D/3D/Real的PCHIP VMAF BD为−0.079439/−0.072969/−0.118122%，独立分段线性为−0.020987/−0.089294/+0.019399%；PSNR-Y PCHIP为−0.313748/−0.007229/−1.118874%，线性为−0.271984/+0.004310/−0.828004%。VMAF整体接近零，Real对积分方法变号，不能宣称稳健原生收益。全部点包括微小退步均保留。

00全部源/程序/依赖/流/指标审计、SSE重算PSNR及三源QP100 fresh AVM raw SHA复核通过，证据`optimization-r6-20261018/root/native-quality-audit.json`与`native-avm-spotcheck/verification.json`。最终quality result SHA256为`00b78e58d0d4f23361529bf4e71d6d3ae1740b79d169b1811f479d8aebaf3134`。独立共同速度窗口已完成，不能复用质量耗时。

## 阶段R7：基础概率自适应首关

依`basic-entropy-contract.md`，在R5/R6已冻结、质量长窗口继续时补帧内CDF更新，先限RD-off，保留PRIMARY_REF_NONE和逐帧默认模型，不混入新预测/量化工具。要求所有可达语义模型完整接线，先编码后规范更新，容量重试/帧/实例均fresh，未知模型拒绝；不能靠翻header位或相同CDF合并模型。独立宏展开核对默认边界、count与训练rate offset，单独验证规范更新，再通过固定AVM逐样本验收。

代码及接线已完成，默认关闭`--entropy-adapt off|on`。独立AVM宏展开3840组/31,440有效word一致；直接固定AVM update_cdf差分3,932,160步0差异。00严格Windows49/49、GCC48/48、ASan/UBSan/LSan六项通过。事前冻结96组小片矩阵覆盖三格式AI/P、每格式四qctx、四工具组合、完整与边界尺寸：off对R6完整流/重建一致；on的Windows/GCC完整流/重建一致，固定AVM逐样本一致。新机制检查3,934,722/0。报告`R7-A-entropy-adaptation.md`区分960物理bank行与实际可达上下文，并列出全部正式symbol-site闭合表。

192源/完整源码已冻结在`optimization-r7-20261018/root/source-manifest.json`和`candidate-source`；Windows程序SHA256 `3d9b45c047edd3682c158426827e1ed10840a4e72aa3cde6b9f52394550e8dd9`，B已复制独立固定。原生12/12配对、24流固定AVM通过，12点两端完整recon SHA、SSE和PSNR完全相同；候选VMAF仅复用相同像素的baseline评分。按QP80/100/120/140，2D字节减少8.570/9.630/12.072/17.619%，3D减少17.514/20.029/19.008/19.477%，Real减少8.074/6.989/8.429/11.419%。这属于RD-off纯熵收益，不是新视觉提升或与RD-on共同开启的结论。

00完整指纹/指标审计与三源QP100 fresh固定AVM raw SHA再次校验通过，见`optimization-r7-20261018/root/native-quality-audit.json`和`native-avm-spotcheck/verification.json`；B冻结报告`M6-r7-B-validation.md`。R6完整质量已通过。共同运动速度及R7的RD-off新鲜配对均已完成；不与RD-on运动窗口直接相除。

同QP的RD-off与RD-on质量/字节存在显著差异，不能把“新增RD”本身当作必然改善。完整四点曲线以M5 static RD-off为参照：2D/3D/Real的原pruned VMAF BD为+10.284/−4.173/+28.703%，R5运动RD为+2.440/−5.961/+25.428%；独立分段线性分别为+9.758/−4.256/+24.956%与+2.564/−5.867/+22.052%，方向一致。这说明R5改进了既有RD路径，但该路径在2D和Real仍落后于RD-off。各比较仅用各自公共质量区间，不将不同区间BD相乘。

A只读单位/候选审计`next-A-rd-baseline-audit.md`未发现固定点单位错误，不据三片调整lambda。外置54个单SB同入口锚用例覆盖三格式、AI/P、三QP和三种预定义纹理：原M5策略在exhaustive和pruned中均实际找到，几何/模式/MV/系数/重建以及D/R/J逐项一致，赢家J不高于锚；另有3个P/平坦加稀疏噪声用例中exhaustive优于pruned，证明局部包含原策略与裁剪损失可以同时存在。严格Windows/GCC及ASan/UBSan/LSan三方记录一致；证据`optimization-r7-20261018/a/anchor-proof`与`root/anchor-check/verification.json`。单SB证明不外推整帧参考与上下文分岔后的曲线。完整42项profile曲线比较已落`optimization-r7-20261018/root/profile-comparison.json`；独立性能全部完成并经00复核。

## 独立性能最终结果

共同窗口27正式/9暖机和R7窗口18正式/6暖机全部PASS。QP100三配对墙时中位增量按2D/3D/Real：R5对原pruned为+17.33/+29.98/+17.25%，R6对R5为+5.31/+7.90/+1.87%，R7 on对RD-off静态CDF为+12.55/+13.20/+16.94%。进程CPU配对中位增量分别为+17.08/+27.95/+19.04%、+3.07/+8.87/+1.33%、+12.08/+13.97/+11.96%。全部范围和45条原始正式记录保留；R6的2D/Real单轮CPU范围跨1，墙时则三轮均增加，不能声称每次CPU都变慢。

00独立核验全部45条正式流SHA/字节均匹配已验收的QP100质量流、程序/脚本/合同/协议一致，并复算配对、median/min/max全部通过。证据`optimization-r7-20261018/root/motion-speed-audit.json`和`entropy-speed-audit.json`。后台整机CPU估计中位约18.2%至19.3%，原大测持续，所以非整机隔离。R6近零质量收益但明确墙时成本，不支持默认升级；R7真实同像素压缩收益需权衡约13%至17%墙时成本。

## 同片段SVT-AV1参照

00另冻结同native24、既有协议六CRF28/32/36/40/44/48、preset6、lp1/threads1的18点新鲜参照，编码/24帧解码/PSNR/VMAF全部PASS；证据`optimization-r6-20261018/root/native-av1`。使用视频packet payload，不将旧15秒8倍比值直接套到新1秒，也不将此质量工作流耗时当独立速度。SVT-AV1默认RA与本实现低延迟P的结构差异保留说明，不能据此归因所有差距。

固定2D/QP100 R5点562387字节/VMAF92.948348，SVT-AV1 CRF48为44321字节/93.399432，直接实测后者画质更高且R5体积约为其12.7倍；这只适用于该片该点，不冒称所有场景统一比值。00解析R5三源QP100真实OBU：2D key占12.4%、Real占7.2%、3D占43.9%，因此2D/Real大部分码量在P帧，不能仅以1秒key开销解释差距。运动小改尚不足弥合根本差距，后续基础项继续保留。

## 后续基础与测试保护

亚像素、实用帧内预测/递归分区、CDF自适应与RD组合、完整系数优化、环路和参考/时域结构仍属后续基础项，未完成。亚像素需要精度信令、对应CDF、MV单位及插值链一起接入；CDF自适应与RD组合需要模型状态事务回滚。完成基础后再评估AVM高级工具，不用新工具掩盖基础缺失。

原`campaign-local-r2-20261017`由既有计划任务和冻结程序继续运行；新构建j2、质量最多2路、速度单路，进入其isolated-speed即暂停新增计算，本轮无冲突。额外负载可能延长质量测试，不承诺零资源影响。00最终核验192源、三阶段冻结程序和原长测76冻结文件/配置一致，见`optimization-r7-20261018/root/final-integrity.json`；14:00真实host/总控/batch及计划任务存活，唯一94/108质量任务PASS、无FAIL。新测量进程已结束。正式发布与M6整体最终验收仍待原长测和后续基础工作。
