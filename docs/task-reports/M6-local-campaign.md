# M6 本地H264原片集中测试

> 已于北京时间2026-10-17 17:15:34独立启动，PID5284，17阶段。启动后总控RUNNING/prepare-local，2D完整383帧Y4M已成功生成，stderr为空；76项冻结依赖校验通过。此为运行交接，不是完整测试通过声明。

2026-10-17，用户提供AV2/Test-Video/1080p-15s的三组H264原片与SVT-AV1 p6 CRF36成片，授权替换在线素材继续测试，正常开始后台长跑后结束AI会话。沿用00+A/B：A准备本地素材，B实现AV1对照，00负责独立链路检查、总控和最终报告。

## 参考与依赖

全部1920×1080，24000/1001 fps；2D/3D/Real分别383/378/390帧，H264原片8bit420，AV1成片10bit420。三对按序号帧数一致，PTS仅存在最多1ms的容器舍入差；2D等尾部存在时间戳间隙。统一解码帧序列为参考，保留全部帧，不循环、不dup/drop，Y4M按标称fps归一；帧序列时长用于码率，原容器时长/PTS另行保留。

完整与pilot/speed均不缩放，统一420p10；8bit升10bit不增加原始精度。2D缺少部分色彩元数据，冻结原probe并保留YUV码值，不冒充已确认BT.709。小图专项使用256×144及三格式，明确缩放与padding，不把小图得分当1080p得分。

现有完整版FFmpeg已实测支持H264软件解码、AV1软件解码、SVT-AV1编码与libvmaf，无须新增编译依赖。FFmpeg/ffprobe、M6 encoder及M5基线、固定AVM decoder、VMAF模型与脚本均复制到独立快照并绑定SHA；原六文件只读，派生媒体在C:\Private\ffmpeg-native-m6-local-media。

已完成三片短解码预检、4项AV1工具自测，以及同一小图的H264参考→既有AV1和fresh六CRF→AV2三模式→AVM重建→审计→SVG/JSON/Markdown完整链路，全部PASS。小图证明在C:\Private\ffmpeg-native-m6-tests\local-report-proof-20261017，仅用于工具链验收，不是完整三片效果结论。M6编码器算法未修改，沿用r1已通过的170源/程序与20796任务证据。

## 集中配置

目录：C:\Private\ffmpeg-native-m6-tests\campaign-local-r2-20261017。

| 阶段 | 配置 | 任务数 |
|---|---|---:|
| 原片pilot | 三片各3帧、QP100/P、M5/pruned | 6 |
| 用户AV1点 | 三段现成p6 CRF36，对完整原片测VMAF/PSNR和视频packet净码量 | 3 |
| fresh AV1曲线 | 同一完整参考，p6、CRF28/32/36/40/44/48，lp1、单次 | 18 |
| 小图穷举对照 | 三片×三格式×7QP×AI/P×四工具×off/full/pruned | 1512 |
| 1080p参数筛查 | 三片各3帧×12QP×AI/P×四工具×off/pruned | 576 |
| 完整1080p主曲线 | 三片全帧×12QP×P×M5/off/pruned，jobs3单次 | 108 |
| 完整AI补充 | 三片全帧×4QP×AI×M5/pruned，jobs3单次 | 24 |
| 独立速度样例 | 每片前24帧×QP80/160/240×P×M5/pruned，jobs1重复3次 | 18 |

12个QP：40/80/100/140/180/200/216/224/232/240/248/255。四工具是DC/adaptive intra与fixed/adaptive partition的组合；不用尚未实现实际策略的preset枚举凑数。小图包含exhaustive及8bit420/10bit420/444，完整曲线主测10bit420。共2244个AV2配置任务，加AV1度量/曲线共2265项；含重复总计2298次编码，其中现成AV1只解码评分、不算新编码。

完整片质量曲线并发运行，其时间不能当单路性能；24帧速度样例只对该段报告三次中位数及波动，不伪装成完整15–16秒耗时。AV2时间含导出重建，fresh AV1不含，速度表注明差异；用户成片原编码时间未知，保留null。

## AV1/AV2结论边界

参考为H264原片解码画面，避免以已经AV1有损压缩的成片当真值。现成AV1只提供每片一个点；另测fresh AV1六点后，才在严格单调且有共同质量区间时计算BD-rate，其他情况写N/A。完整片报告叠加AV1点和曲线，检查是否存在AV2体积更小且VMAF不低于用户成片的已测点；未测中间QP不捏造为实测。

fresh AV1默认RA，AV2当前只有AI/单参考低延迟P，参考结构不同。这是当前实现与配置在三片上的比较，不推断编码标准总体优势。用户三类片段替代此次在线素材计划，未添加独立holdout，不宣称原六类加holdout门槛已满足。

## 状态与恢复

总控campaign-status.json为真实状态，逐项结果cases/*/result.json支持有效指纹复用。任何配置或保留文件变化拒绝误用旧PASS；成功后清除大raw与重复流。r1 FAIL与冻结源码/程序和全部已验收测试保持原状，不重跑其大矩阵。

```powershell
Get-Content C:\Private\ffmpeg-native-m6-tests\campaign-local-r2-20261017\campaign-status.json
& C:\Private\ffmpeg-native\deps-full\AV2\svt-av2\tools\Start-M6Campaign.ps1 -RunRoot C:\Private\ffmpeg-native-m6-tests\campaign-local-r2-20261017
```

恢复仍使用相同冻结配置。完整输出在reports/README.md、av2-points.json、av1-points.json、cross-codec-comparison.json与SVG。总控PASS后仍需下一次AI独立验收，正式M6版本不自动发布。
