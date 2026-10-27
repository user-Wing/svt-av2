# M6 第二轮 B：独立有界回归与已有小图数据分析

2026-10-17。当前交付为 `READY_FOR_INTEGRATION`，正式版本仍为 `0.0.5-m5`。本轮不发布 M6，不修改冻结长测、媒体、模型或依赖。原 M6.2 AI/P 完整块 RD 已实现，此处是其第二轮性能优化验证。

## 独立回归

新增 `tools/m6_r2_regress.py`，固定比较冻结 M6 基线与候选的完整码流、内部重建和逐帧 RD 决策统计；唯一允许变化的是 `forward` 实际变换次数，并要求其不增加。`candidates/replay/winner/D/Rq12/J/lambdaq8/full/partial` 必须相同。流一致同时覆盖最终系数语法，不把没有单独导出的内部 coefficient 数组冒称已逐元素比较。

54 个配置覆盖三格式（8bit420、10bit420/444）、AI/P、off/exhaustive/pruned、64×64/128×64/72×72、QP1/100/255和四种工具组合；三帧确定性纹理带位移及第三帧内容变化。尺寸/QP/工具采用分配覆盖，不是全部笛卡尔积。18 个 off 配置额外与冻结 M5 流及重建比较。每条候选流调用固定 AVM 解码器，比较全部可见重建字节。计算前逐次读取总控状态，遇到 `isolated-speed` 停止后续计算。

首轮 54/54 PASS，54 条固定 AVM 解码逐字节一致，18 条 M5/off 流及重建一致。证据：`C:/Private/ffmpeg-native-m6-tests/optimization-r2-20261017/b/regression-initial/result.json`。基线为长测只读 `snapshot/current/encoder.exe`，SHA256 `16a9698dc53aa51d74221b84b387660acb9ce5763713c5650aa0c419cfa260da`；候选为 A 的 `a/Windows/bin/SvtAv2EncApp.exe`，SHA256 `2548e1020d34d05f3debafb4932fe85f085452ac43d9c4166d3fd78935480f22`。

首轮执行后工具增加邻接 DLL 指纹和 forward 不增加的显式断言。按 00 要求不重复相同源码的54条计算，离线重审原始结果及实际程序指纹：108条 RD 帧记录全部 forward 不增加，78条不变、30条下降，总计35622→23445；补记冻结 M6/M5 邻接 DLL 指纹。独立证据 `b/regression-offline-audit.json` 为 PASS，并绑定原 result SHA256 `a1f75d4edcfbe565f21f521bb9aff857c93bacfdacca06578169bfa648d4144a`。独立 Linux 编译及单测由 A/00 提供，本报告不将 Windows 短流结果外推为 Linux 验收。

## 源码交叉审查

对照 00 的 `root/baseline-source/Source/Lib/Encoder/rd_frame.c` 审查 A 的三处修改，未发现阻塞问题：

- 区域备份改为逐行 memcpy：原/目标偏移均按字节计算，行宽为 `block->w * step`，输入仍按自身 byte stride 逐行访问；memcpy 不要求样本或行指针对齐，可支持10bit奇数stride与非对齐地址。每平面 backup 原有8192字节容量覆盖最大64×64×2字节，backup仅由此函数保存/恢复，没有其他代码按旧 uint16 索引读取。只复制可见区，不写padding。
- exhaustive 无效路径预检仅调用纯读取的 `legal_mode`，判断依赖固定的叶坐标、工具开关与 reference 是否存在，不读取重建邻居。候选循环每次先恢复 SB 入状态，非法候选不进入 rate trial；合法路径顺序、ID与所有重建仍保持。因此跳过旧无效路径在后叶拒绝前做过的变换，不改变后续候选邻居或熵状态。partial/pruned/赢家重放维持原路径。
- reconstruct 传 NULL SAD 仅跳过预测后的源像素差扫描，所有预测输出和错误返回仍完成；rank_modes 仍传实际 SAD 数组。未改变候选排名。

以上 stride/unaligned 结论来自代码语义审查，54条 CLI 回归使用内部正常分配帧；不冒称另做了奇数stride API实验。00另行比较全部合法候选 observe 输出，补强局部候选级不变量。

## 已有 1512 项小图数据

`tools/m6_r2_analyze.py` 只读 `campaign-local-r2-20261017/small-exhaustive/cases/*/result.json`，确认1512项均 PASS；三片×三格式×7QP×AI/P×四工具×三RD，256×144、各5帧。分析结果在 `C:/Private/ffmpeg-native-m6-tests/optimization-r2-20261017/b/small-analysis.json`。

504 组 exhaustive/pruned 配对中，438 组流与重建完全相同，66 组不同。全部差异集中于同时开放 adaptive partition 和 adaptive intra 的 `11` 组合。其余工具组合的378组全部相同。

| 结构/工具 | exhaustive 候选/帧均值 | pruned 候选/帧均值 | exhaustive forward/帧均值 | pruned forward/帧均值 | pruned/full 时间比中位数 |
|---|---:|---:|---:|---:|---:|
| AI/10 | 20 | 20 | 2216.71 | 536.71 | 0.258 |
| AI/11 | 421 | 142 | 8711.43 | 1646.43 | 0.225 |
| P/10 | 122.4 | 122.4 | 5196.60 | 1481.40 | 0.415 |
| P/11 | 1241 | 154.8 | 16273.43 | 1727.11 | 0.162 |

工具位顺序是 partition/intra。`10` 组合两模式合法候选计数完全相同，forward 却明显不同，是候选计数之外存在重复计算的真实证据。它支持 A 对无效路径预检及重复重建的进一步画像，不能单凭此表断言所有耗时都来自剪枝。

时间来自原批次 jobs4，包含 CLI 读写和导出重建，仅描述该批次记录，不能称单路速度收益。`00/01` 的 pruned/full 时间比中位数在0.998至1.007附近，说明没有可辨识收益。

pruned-full 的完整流字节差范围 −337..+273，均值 +3.752；VMAF 差范围 −1.152581..+0.301030，均值 −0.004361、中位数0。AI/11的34/63条不同、VMAF差 −0.054203..+0.123376；P/11的32/63条不同、差 −1.152581..+0.301030。这些是缩小图片段的实际效果差，不能扩写为1080p收益或标准总体优势。

现有 trace 只记录整帧总 D/R/J 和赢家摘要。先前 SB 或帧的赢家分歧会改变后续预测邻居、熵上下文和参考图，因此不能把两个整帧 J 相减称为“同一入状态的局部剪枝损失”。本轮如需精确局部 regret，应另作隔离插桩实验，不改冻结 baseline 或按这三片调 lambda。

## 边界

以上验证针对保持决策/流/重建一致的性能优化。没有新增大矩阵，没有复跑旧 r1 或在线九源；冻结本地 r2 长测继续。长测最终结果仍需 00 独立审计；不以进程退出或脚本 PASS 自动声明 M6 DONE。
