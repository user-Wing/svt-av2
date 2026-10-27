# M6 有界 RD 集成与集中批测交接

> **2026-10-17压缩效率主线与后续短验**：同源2D直接支配点显示pruned100比质量更高的fresh AV1 CRF44大8.75倍；原18.8%合成耗时下降不能解决体积。已做外置三帧分类诊断（流/recon与冻结pilot一致）并合入量化峰值溢出预检等价优化，00 Windows45/GCC44/SAN3、B54流/AVM及18off/M5通过。详见[M6后续评估](M6-r3-assessment.md)。22:50原real-quality26/108 PASS，9/17阶段；76冻结文件及配置SHA匹配，未最终验收或发布。

> **2026-10-17 第二轮性能优化已独立短验通过**：仅RD核心提前拒绝非法路径、删除无用SAD、按行复制备份；合法候选/评分/码流/重建保持。00严格Windows45/GCC44、sanitizer专项3、2319条候选事件前后一致，B54流及AVM重建通过。合成核心三轮中位耗时约减少18.8%，不是隔离真实视频速度结论。详见[M6第二轮性能报告](M6-r2-performance.md)。外部原长测继续使用旧冻结程序；22:32左右real-quality为24/108 PASS，9/17阶段完成，未最终验收，正式仍0.0.5-m5。

> **本地原片r2续行优先**：用户已提供三H264原片及对应AV1成片，授权替换在线素材续跑。C:\Private\ffmpeg-native-m6-tests\campaign-local-r2-20261017于2026-10-17 17:15:34（北京时间）以PID5284独立启动，17阶段。详见[M6本地原片测试](M6-local-campaign.md)。保留r1 FAIL及已通过矩阵，新批仅补真实效果/参数/速度部分；当前M6未最终验收，不以启动代替PASS。

> **2026-10-17验收续行优先：后台已在15:42:59于prepare-real停止，状态FAIL。** Boxing仅4.233333秒，固定5秒素材准备检查失败；编码测试前19/28阶段通过，独立复核20796任务/20916次编码及指纹全部通过。真实视频、单路性能与最终报告未运行，M6只阶段性验收通过，正式仍0.0.5-m5。详情[M6验收分析](M6-review-20261017.md)，机器证据artifacts/m6-review/partial-verification.json。本轮未改编码器或重启长测，后续应新r2接续真实素材阶段，勿原样重复r1或改写冻结证据。下方已启动/RUNNING为历史时点。

> **集中批次已于2026-10-17 13:15:58（北京时间）独立启动，PID9936。** 启动后已核实总控RUNNING/build-GCC，冻结源构建103个目标完成，stderr为空；其余28阶段按配置依次执行，AI按用户要求在此暂停。总控状态只读C:\Private\ffmpeg-native-m6-tests\campaign-r1-20261017\campaign-status.json。170项当前开发源与快照逐项MATCH，423项快照/依赖已校验；短验源清单SHA为08211aabbd1bac49a2a5153995096adbd241fb792c67e06579bcdec0caf1b286。启动不代表长测已通过。

2026-10-17，00主窗口+A=m6_core+B=m6_rate。**M6 RUNNING，开发接线和必要短验通过，完整长测及效果验收PENDING。正式版本仍0.0.5-m5，不发布M6。** 用户要求遇到长时间计算集中启动后暂停AI会话，全部任务完成后再集中审计。

## 已实现与短验

- 新增独立RD frame路径；off仍走原M5核心。公开参数SVT_AV2_PARAM_RD_MODE/配置rd_mode，CLI `--rd-mode off|exhaustive|pruned`，默认off；运行后修改拒绝。API增加最后完成帧RD统计，只用于可审计测量。
- 完整SB原树与四32树的真实transform/quant/inverse/clip重建D，实际量化表推导固定点lambda，同writer语法概率rate；完整候选组合与每叶前2候选剪枝。M5工具开关限定候选。Partial SB只保留M5预筛单路径，未冒充边缘全模式RD。
- 每候选回滚SB重建及语法上下文；赢家重放核对coeff/recon/D/R后提交。参考帧仍在正式码流成功后提交，packet扩容不重复RD。使用整数成本和溢出安全乘积。
- A的MSVC/GCC独立核心测试各8813 checks、0 failures；B rate/session各98534 checks、0 failures，包含候选换序、完整81/256域、最低J、剪枝子集、故障回滚、完整帧R与SB之和。
- 00独立Windows严格CTest **45/45**、GCC **44/44**。三格式AI/P×exhaustive/pruned **12条短流**固定AVM逐样本重建全部一致，证据artifacts/m6-root/smoke。
- 四模式小批次、三次编码及VMAF与审计、真实断点复用/单条损坏重跑/参数改变拒绝已通过。00另验证GCC四模式流/recon一致，跨平台缓存3条复用、1条破坏后重跑通过；自动Markdown/JSON/SVG报告实际生成通过。证据artifacts/m6-root/final-tool-proof、cross-proof和artifacts/m6-rate。
- 真实输入Narrator官方QP15完整源及720p/小图转换已短验。其余源下载与转换留后台；部分题材标签仍待缩略图复核，尚未宣称六类内容已验证。

合同：[m6-rd-contracts.md](../m6-rd-contracts.md)。模块报告：[A](M6-A-rd-core.md)、[B](M6-B-rate-validation.md)。短验最终指纹入口artifacts/m6-root/short-verification.json；不以这里的短验代替完整M6验收。

## 集中批次

批次目录：`C:\Private\ffmpeg-native-m6-tests\campaign-r1-20261017`。
真实素材与转换目录：`C:\Private\ffmpeg-native-m6-media`。当前工作区约29.27GB，大批结果和新增大素材均放工作区外。

总控先冻结source/工具/Windows程序/M5基线/oracle/模型和配置SHA；Linux三个配置从冻结源构建。自动依次执行：

| 项目 | 独立任务数 | 说明 |
|---|---:|---|
| 主边界矩阵 | 4320 | 三格式×三场景×15QP×AI/P×四工具×M5/off/full/pruned |
| 非整SB | 288 | 136×72、三格式、QP1/100/255、两结构、四工具、四模式 |
| 全QP | 3060 | 255QP、三格式、AI/P、full/pruned、两工具开启 |
| Linux完整流重放 | 12960 | GCC/Clang/SAN各重放主矩阵4320项 |
| 720p/1080p | 各54 | 三格式、三QP、AI/P、M5/off/pruned，含VMAF |
| 1000帧长链 | 60 | 三格式cut/noise、五QP、AI/P、pruned，每项3次 |
| 真实片pilot | 4 | 两片各1秒、M5/pruned、P；写入粗略耗时预算 |
| 真实片小图full对照 | 180 | 两片×三格式、五QP、AI/P、M5/full/pruned，每项3次 |
| 真实片主要曲线 | 160 | 六片测试+两片holdout、五QP、AI/P、M5/pruned，每项3次，jobs1 |

合计约**21140个任务、21940次编码**（含重复）；不把同源不同平台重复计作独立素材。下载约827MB，派生Y4M和临时raw另有预算。真实片使用固定高质量已压缩参考，不是摄影机无损母版；各种位深/色度转换明确标注。速度三次中位数及波动，包含CLI读写/重建导出，排除decoder/VMAF，真实速度阶段无其他批次并发。

每个batch完成后独立审计；错误停止后续阶段，不伪装PASS。仅复用输入、程序、合同、配置和保留产物指纹匹配的结果。每项保留首流/指标/JSON/log，成功验证后删除raw和重复流。最终reports/README.md、points.json、bd-rate.json及SVG提供每片体积—VMAF—时间曲线；非单调/无共同质量区间写N/A，实际失败另写FAIL。批次PASS只表示脚本运行完成，M6发布仍待AI独立验收。

## 查看和恢复

```powershell
Get-Content C:\Private\ffmpeg-native-m6-tests\campaign-r1-20261017\campaign-status.json
# 读取总控progress字段对应的summary.json；逐项完成数可查看该batch/cases/*/result.json。
# 需要恢复时使用原配置与原快照，勿重新prepare或更改QP/模式：
& C:\Private\ffmpeg-native\deps-full\AV2\svt-av2\tools\Start-M6Campaign.ps1 -RunRoot C:\Private\ffmpeg-native-m6-tests\campaign-r1-20261017
```

恢复会重新验证所有冻结依赖；批测自行复用有效PASS，损坏或未完成项重跑。运行中不得删除本批快照、素材或build/m6-root/pydeps运行依赖；源码后续改动不会改变外部快照。无需AI持续在线或轮询，不启动自动消息通知。用户下轮要求汇报时先查总控和逐项结果，再做最终收集；不要看到进程消失就推定成功。
