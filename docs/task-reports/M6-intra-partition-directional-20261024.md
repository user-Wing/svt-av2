# M6-I 第二轮：16×16 分区与斜向亮度预测（2026-10-24）

## 状态与目标

目标仍为同质量 raw OBU 体积 ≤ AOM-AV1 的85%，随后将单线程耗时比压到2.0。本轮补齐两项实际缺口；不以当前有限实现的结果判断 AV2 标准的上限。**I-GATE 为 NOT_RUN，正式版本仍0.0.5-m5，新入口默认关闭。**

本轮证据：`C:/Private/ffmpeg-native-m6-tests/intra-r2-20261024`。原始任务书 `../../../M5-M6-AI-TASKS/04-M6-intra-only-first.md`，现行合同 `../m6-intra-contract.md`，上一轮 `M6-intra-first-20261024.md`。

## 实现范围

- `--partition-select fine`：完整64×64超块可选择64、四个32、十六个16的合法二叉树。每棵树完成模式优化后再比较，保留原有粗分区候选；partial超块使用既有合法边界树。分区标识0/1/2对应64/32/16，重建、tile writer、rate trial和最终码流使用相同叶序。
- `--intra-select directional`：在原七种亮度模式外增加 D45/D67/D113/D135/D157/D203，色度保留原七种共享 U/V 模式。使用固定 AVM 的方向导数和四抽头 IDIF，并处理符号舍入、clip、TR/BL连续已解码样本及边界扩展。没有套用 AV1 的亮度插值。
- 每个64/32树保留原有 DC/V/H 有界种子；16树由一条因果 SAD 路径开始，再逐叶做一轮 Y/UV 坐标优化。每次候选从相同超块入口重建完整叶序列，比较真实 tile 符号路径的概率成本 R 与量化/逆变换/clip后的 D；自适应CDF在成功重放后才提交。不是全组合或全局最优搜索。
- 修正候选标识容量：十三模式使用每叶四位；16叶及partial树不再强塞入四叶路径字段，另用完整模式/UV/系数/重建hash校验重放。保留候选反序、失败恢复和整帧/逐超块rate一致检查。
- `fine` 限定 single-picture + basic/directional + RD；basic/directional继续与现有系数、运动优化互斥。旧默认、旧多帧及旧单图入口保持行为。

可运行的冻结开发程序：

```powershell
& C:/Private/ffmpeg-native-m6-tests/intra-r2-20261024/bin/SvtAv2EncApp.exe `
  --input input.y4m --output image.obu --still-picture --threads 1 `
  --intra-select directional --partition-select fine --rd-mode pruned `
  --entropy-adapt on --qp 100
```

生产库没有链接 AVM。外置 oracle 只用于差分与码流验证。

## 压缩测量

本轮新增132质量点全部通过：fine与directional在三张原生图各18点，四张额外图片的AOM/adaptive/fine/directional共96点。以下按各对曲线的共同区间计算；`development-summary.json`保留全部45对比较与三个平面PSNR结果。

三张原生1080p开发帧，主指标为VMAF BD-rate：

| 图片 | fine较上一轮 | 斜向较fine | 本轮组合较上一轮 | 本轮组合较AOM |
|---|---:|---:|---:|---:|
| 2D | −5.41% | −9.83% | −14.73% | +70.86% |
| 3D | −4.92% | −1.83% | −6.70% | +55.94% |
| Real | −9.49% | −0.82% | −10.23% | +37.73% |

四张额外calibration图片，主指标为VMAF BD-rate：

| 图片 | fine较上一轮 | 斜向较fine | 本轮组合较上一轮 | 本轮组合较AOM |
|---|---:|---:|---:|---:|
| kodak-02 | −12.38% | −5.60% | −17.32% | +113.39% |
| kodak-03 | −13.30% | −5.68% | −18.28% | +69.91% |
| urban-002 | +0.34% | −0.92% | −0.57% | +68.90% |
| urban-003 | −1.30% | +0.28% | −1.02% | +78.98% |

Urban-003 的斜向单因素约+0.28%，保留该负收益；Urban-002 的fine也有轻微负收益。摄影、动画和建筑细线的收益并不相同，不能把这些有限候选称为基础帧内能力已全部补齐。每个表格单元使用各自共同区间，不能将百分比直接相乘。三平面PSNR BD-rate及区间完整保存在 `development-summary.json`，原始SSE/PSNR保存在每点结果中。

七张图片相对AOM均未达到−15%的体积门槛。20张holdout没有编码评分，正式2×计时没有运行，M6-I仍在进行。

三张既有 2D/3D/Real 使用未缩放的1920×1080、10bit420第48帧。另从已冻结 calibration 按身份字典序选每个来源家族前两张：kodak-02、kodak-03、urban-002、urban-003；读取原准备尺寸768×512、768×512、512×384、512×336的8bit420，不再缩放。选择规则先保存于 `calibration-selection.json`；没有读取holdout进行评分。

沿用固定 libaom v3.15.1、allintra、cpu-used0、threads1、单tile、一次编码、raw OBU。AOM Q=16/24/32/40/48/56，AV2 QP=40/70/100/130/160/190。输入、模型、全部编码/解码/评分命令及程序SHA保存于各实验 `protocol.json`；每点保留码流、内部与固定AVM重建、VMAF JSON、三平面SSE/PSNR及进程日志。旧AOM与官方SVT-AV1曲线只读复用，旧AOM构建的18流已在上一轮由隔离正式构建逐字节复现。

`adaptive` 为上一轮七模式＋64/32＋CDF；`fine`只换为本轮三树比较；`directional`再增加六个亮度方向。所有比较均在各自严格单调的共同质量区间做 PCHIP 对数码率积分，不按同QP大小比较，不将分阶段百分比相乘；非单调结果保留并报告N/A。

质量运行含重建导出，与构建、检测器及其它质量实验存在资源争用；这些墙时不能用于2×速度验收。正式同质量、五次新进程独立测速仍未运行。

## 正确性与兼容性

- 固定 AVM 斜向原语差分：43,200案例、47,139,840样本零差异；覆盖五种方形尺寸、8/10bit、边界、无/半/完整TR与BL、随机及高对比参考。重新编译只读固定 `reconintra.c` 的标量实现，结果和SHA在 `primitive-directional.log`，脚本 `check-directional-avm.sh`。
- 先验证16分区：2,352强制流PASS。扩展六个斜向后：13亮度×7色度×三格式×四几何×固定/混合邻居×固定/自适应CDF，共4,368流全部固定AVM可见像素一致；`forced-16`、`forced-directional`。
- 最终API/CLI 306组：288单图流全部固定AVM一致，另18组旧多帧与冻结程序指纹一致；`api-directional/result.json`。新程序对先前162组流/重建完全不变，先前162又包含90组上一轮指纹，见两份 compatibility JSON。
- MSVC/GCC/Clang各288组，共864完整流及重建一致，见 `cross-platform-directional/result.json`。最终GCC/Clang程序与该矩阵程序SHA相同，MSVC程序SHA也与质量测量版本一致。
- 最终严格CTest：MSVC52/52、GCC51/51、Clang51/51。包含候选反序、真实概率R/D/J、完整和partial树、最终赢家重放、失败恢复、包缓冲重试及1000个独立handle。GCC ASan/UBSan/LSan最终51/51通过（675.92秒），见 `final-san/SAN-ctest.log`。

初次普通CTest暴露旧fixture仍把depth2当非法值，以及几何记录器4096叶不足以容纳1080p的8100叶；现按合法新域把非法值改3、记录容量改8192。初次检测器为50/51，扩大的RD矩阵触发旧180秒时限，未出现检测器错误；已仅对检测器的该项设置1200秒，完整51项重跑通过。原失败/超时日志保留，没有将它们改为PASS。

诊断解码器额外核对2D/QP40：最终4593叶，其中4024个16×16、225个64×64、104个32×32、240个边界8×8；六种新斜向共537块、153216亮度像素，约占图像7.39%。该诊断解码像素与固定主AVM完全相同，见 `diagnostic/result.json`。这证明新工具实际被选择，不是只添加开关；它只说明该开发点。

## 仍需推进

当前仍是受限帧内实现：缺少angle delta、wide-angle、单超块内混合32/16、完整超块8×8候选、独立TX划分/类型、RDOQ、DIP/CfL/MRLS/IBP及滤波闭环。现有量化主要是标量量化，不能把本轮的模式/均匀分区扩展当作这些能力已经完成。

下一优先单元应为系数率失真量化和变换选择，与现有CDF和模式搜索闭环；继续先强制验证每种合法语法及重建，再测单因素多质量点收益。后续也需补足屏幕文字、高ISO、原生HDR等题材。20张holdout继续保持未评分，在体积明显接近目标后再做冻结验收；I-GATE前不恢复帧间、多线程或preset路线。

## 冻结与复核

`final-integrity.json`逐点复核全部132个新质量点的程序/输入/码流/解码/重建指纹，并从像素独立重算SSE/PSNR、核对VMAF JSON；另有两点独立重跑VMAF完全复现。旧282源快照及旧开发程序未变。最终源清单和差分在 `final-source-manifest.json`、`changes.patch`；生产代码与严格验证快照在换行规范化后相同，原编译快照保留在 `validation-source`。冻结开发程序SHA256：`8c7906e08244e61b32644b93d62c4c98b725097e546f9ee1468b270c996e1c89`。
