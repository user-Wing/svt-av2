# 基础能力 R5：B 独立native质量与配对性能协议

2026-10-18，00+A/B，当前 `READY_FOR_INTEGRATION/质量与独立性能通过`。用户要求先补基础编码能力，再评估AVM的新技术；本轮首项是多个整数MV进入真实RD。三类固定1.001秒片段相对旧pruned RD基线取得有限改善，**不代表普遍优于M5的RD-off SAD路径**；跨profile对照显示2D/Real仍退步，RD基础效果问题尚未解决。当前AV1差距依然明显，不能外推完整片或编码标准总体优劣。正式仍 `0.0.5-m5`，隔离实验不代表M6发布。

## 样本与参数事前冻结

B在生成素材前向00声明，00批准统一取三类local原分辨率Y4M的0基索引48..71：最接近2秒的起始索引 `round(2*24000/1001)=48`，归一时间2.002秒；连续24帧、时长1.001秒，不看内容或编码结果挑段。三类均1920×1080、10bit420、24000/1001 fps，不缩放、不dup/drop，不扩大到48帧。

已生成 `C:/Private/ffmpeg-native-m6-tests/optimization-r5-20261018/b/corpus/manifest.json`，SHA256 `3556e88fa1a2ddea6692fd35bbf8f5e2f4ad39ca4398fb1944cfa6e802f0512d`。每源记录原H264冻结路径/SHA、完整local Y4M实际SHA、源总帧数、24个精确索引/原始payload偏移/逐帧payload SHA及提取后SHA。完整local三源重新顺序哈希均匹配冻结provenance；提取后独立重开原源与新文件，72帧逐一字节比较全部PASS。新媒体约448MB，全部外置；原片和local媒体只读。

事前冻结QP80/100/120/140、low-delay-p、adaptive intra/partition、pruned、coeff-opt off。baseline为R4最终WindowsV2程序，SHA256 `1a24414435baa84a77d9620fad70bc269f4bb6f6215969d409f8ffb5f75da2d4`，默认off与R3兼容已由前轮验证。candidate开启独立 `--motion-opt rd`，与coeff优化互斥，算法范围以00的 `docs/basic-capability-contract.md`为准；不调lambda。

## 质量矩阵

三源×四QP，共12个baseline/candidate配对、24次编码，全部使用相同24帧序列。首对先顺序执行并报告baseline估时，再按00明确授权最多2个子进程并行。编码/AVM解码/VMAF同样受两个工作槽限制，不再叠加其它子任务。

`tools/m6_r5_quality.py`逐帧流式计算三平面SSE/PSNR及归一加权D，避免把24帧全部转为int64同时堆入内存；固定AVM全部可见样本须与内部重建一致。VMAF以raw recon显式声明源fps、同源帧序列评分，要求恰好24帧。完整流/质量/trace/指纹保留，成功后清理大raw/decoded。四点严格单调并有共同质量区间才报告PSNR/VMAF BD，其余N/A；配对完整bytes与画质同时报告，不以局部J下降替代等质收益。

每个子命令timeout1800秒、每次矩阵运行全局6小时预算；超时停止并报告，不静默减少帧数或删点。首对支持独立预检，其PASS可在程序/源/配置和保留流指纹均匹配时复用。质量阶段耗时包含recon导出及后台争用，不作为性能结论。

上一轮native首帧baseline约6秒、冻结完整2D pruned约12.1秒/帧，均非隔离。因此24帧可能每条约144～290秒甚至更长；24条编码串行约1～2小时，新候选开销另计。此为启动前预算，不是R5实际速度。48帧近似加倍，不扩展。

## 独立性能窗口（原ABBAAB方案，正式计时前已修订）

`tools/m6_r5_speed.py`采用相同24帧源、QP100，单进程严格顺序 `ABBAAB`，即AB/BA/AB三对、baseline和candidate各三次，每源6次、三源18次。顺序在结果产生前冻结；报告每对B/A比、三对中位数及最小/最大，不只挑最快一次。

速度仅CLI输入读取和码流输出，**不导出debug recon，不运行AVM或VMAF**；不能直接和旧含recon导出的时间表比较。用GetProcessTimes记录进程CPU秒、墙时/fps，用GetSystemTimes约0.5秒采样整机CPU及扣除本进程份额后的背景负载估计，记录logical CPU数与长测stage。变体三次码流须各自确定一致，保留程序/输入/命令/流指纹。

性能窗口开始前通知00/A暂停所有新增构建、质量和其它计算；原冻结长测继续正常跑。因此该窗口只隔离本轮新增工作，**不具备严格整机隔离条件**，后台负载及波动必须公开，不称纯内核单路性能。每次子命令1800秒、窗口全局6小时预算。

首次正式速度数据产生前，00已锁定共同Latin窗口替代R5/R6两套ABBAAB：每源ABC/BCA/CAB三轮，A=R4 motion-off、B=R5 rd、C=R6 wide，各三次鲜测，共27run，减少两套方案重复测B的9次编码；每源每版另两帧暖机不计入统计。每轮保留B/A、C/B、C/A的CPU/墙时全部值、中位数与spread。三版在同轮顺序测量，并非同时测量，原长测仍在后台。此修订事前完成，不依据任何正式速度结果改顺序；原corpus manifest和已冻结R5工具不修改，原ABBAAB工具仅保留历史，不启动。

实际待用工具为 `tools/m6_r6_joint_speed.py`，新协议 `optimization-r6-20261018/b/joint-protocol.json` SHA256 `d1399fdb3b5e71811d5ab4d529d5df67550434c562ed3952fc1febf1aa0494c0`，绑定原corpus、三个程序和合同副本。必须等R5/R6质量均完整PASS、00独立计算完成并明确通知后才开始共同速度窗口。

所有新增编码/解码/评分命令前及运行中定期查campaign stage；若进入原长测 `isolated-speed`，终止本工具拥有的子进程并停止后续任务，不碰原长测进程。

## 实际质量结果

完整12/12配对、24次编码全部PASS，固定AVM逐帧/逐样本与内部重建一致，24条VMAF评分均恰好24帧。逐项重新核验流字节/SHA、3个源SHA、程序/脚本/评分依赖与统计覆盖，通过。启用motion-rd时共441445次trial、58837次accept；每个key首帧motion trial/accept为0。接受率不作为压缩收益证据。

| 来源 | VMAF BD：冻结PCHIP | VMAF BD：00独立分段线性 | PSNR-Y BD：PCHIP | PSNR-Y BD：独立线性 |
|---|---:|---:|---:|---:|
| 2D | −6.892% | −6.330% | −15.511% | −14.979% |
| 3D | −2.018% | −1.798% | −1.635% | −1.744% |
| Real | −2.684% | −2.343% | −2.498% | −2.699% |

两种插值/积分方法同向，支持这三段固定原生短片上的有限率失真改善；不因R4曾有插值敏感性而隐藏本轮交叉检查，也不把幅度当完整原片收益。共同VMAF区间分别81.078498..95.137246、80.537059..95.859782、82.633241..95.862440。

存在直接实测支配点，例如2D/Q100从593811降到562387字节、VMAF92.893466升到92.948348；Q120从307667降到286892字节、88.849347升到88.962265；Real/Q100从1059846降到1051541字节、92.728763升到92.787776。并非每点都更优：2D/Q80和Q140及3D/Q100/Q140存在小量VMAF下降，所有固定点原样保留。

对Real/Q100和Q120曾观察到开放输出文件长度为0、CPU仍活跃；没有据此误判卡死或放宽超时，两条baseline分别300.17/294.6秒正常完成24帧。单次文件长度不能可靠定位全部时间是否发生在首帧，本报告不据此作热点归因。

00另对24条流/源/程序/脚本/评分、SSE重算PSNR及RD帧覆盖独立审计PASS，并对6条native流fresh AVM raw SHA抽查PASS，证据 `optimization-r5-20261018/root/native-quality-audit.json`。B证据 `b/evidence/verification.json`，绑定完整质量result SHA256 `c8ae51982ca717c4a3b1be60145634fecf047a6a5ca0272edd44b6b4e49ae220`，包含3源/24保留流与逐帧motion统计验证；baseline/candidate实际程序已独立复制冻结。

## 与RD-off SAD路径的中期对照

上述R5负BD是相对R4 **pruned RD、motion-off、固定CDF** 的改进，不能把实验基线简称M5然后声称普遍改进。00随后用同一native24/四QP的R7 baseline（RD-off、motion/coeff/entropy off，即旧SAD决策与固定概率）直接计算跨profile对照：

| 来源 | R4 pruned相对RD-off的VMAF BD | R5 motion-rd相对RD-off的VMAF BD |
|---|---:|---:|
| 2D | +10.284% | +2.440% |
| 3D | −4.173% | −5.961% |
| Real | +28.703% | +25.428% |

2D/Real的PSNR-Y亦较差。R5修正了旧RD的一部分损失，但没有使RD在三类片段上普遍优于SAD，不能称基础能力整体已解决。这是实测效果问题，不凭不同参考/邻居链下的单点就判定某段代码有bug。各跨profile积分共同区间不同，00直接重算而未将不同BD百分比相乘。中期证据 `optimization-r7-20261018/root/r5-r7-cross-profile-interim.json`；最终待R6齐后统一PCHIP/分段线性交叉核查。R7的同像素纯熵收益另有直接证明，不依赖本RD结论。

## 同一原生短片的当前AV1差距

00另外完成同一native24的fresh SVT-AV1 p6/CRF28/32/36/40/44/48、lp1/threads1共18点；B核对18项canonical SHA均与本corpus一致，全部24帧，无缩放/抽帧。不能把旧15秒8倍直接套到本1秒。

以下是QP100的直接实测支配比较，AV1质量更高，故无需假设QP等于CRF；它不是精确等质码率比：

| 来源 | R5 motion-rd字节/VMAF | 质量更高的fresh AV1 | AV1净字节/VMAF | R5体积倍数 |
|---|---|---|---|---:|
| 2D | 562387 / 92.948348 | CRF48 | 44321 / 93.399432 | 12.689 |
| 3D | 393152 / 93.705089 | CRF48 | 157276 / 94.982864 | 2.500 |
| Real | 1051541 / 92.787776 | CRF28 | 237116 / 93.428986 | 4.435 |

AV2为完整elementary stream，AV1为视频packet净量；AV1是RA，AV2为单参考低延迟P，不量化结构差异的贡献，也不借它淡化当前实现差距。00的QP100 OBU审计显示2D key仅占12.4%、Real占7.2%，不能把高码量主要归于1秒片段起始key开销。完整证据 `b/same-native-av1-gap.json` 与 `root/native-frame-byte-audit.json`。本项只比较字节/质量，不拿AV1单次编码时间与含recon的质量时间做公平速度比。

## 当前性能状态

素材提取与指纹验证PASS；三项新工具Python语法编译通过，Windows CPU计时/系统计时API可访问，并用空Python进程验证（未编码）。准备时B单次观察可用物理内存约15.22GiB、系统CPU20.67%，仅时点，不保证后续负载。

首对2D/Q80曾顺序预检，baseline编码218.87秒，candidate263.84秒，均含recon导出，非速度结论；随后复用PASS，以jobs2完成原12pair，没有改点。候选实际冻结到 `b/programs/candidate.exe`，SHA256 `d6e2c1c1f9ce20d057b177eb9f66b15f0d6561d5421410bb8db471678b6fd705`。共同速度尚未启动，不能把上述质量时间比当实测性能收益或代价。

## 独立速度完整结果

00明确GO后，joint窗口27次正式run+9次2帧暖机全部PASS，顺序为每源ABC/BCA/CAB，jobs1。A=R4 motion-off，B=R5 rd，C=R6 wide。测量仅输入读取和码流输出，不导出recon、不跑AVM/VMAF。00/A暂停新增计算，原冻结长测继续；所有采样stage为real-quality，故不是严格整机隔离。每条命令hash、流SHA、墙时、进程CPU、约0.5秒系统/本进程/背景估计负载全保留。

总result SHA256 `7e0854177a1c8c7794a37c1fa1dcff9f701d2f98c951563bcdd9be22d49271ea`，原始与冻结副本在 `optimization-r6-20261018/b/joint-speed/result.json` 和 `b/evidence/joint-speed-result.json`。下表为三轮比值中位数[最小,最大]，大于1表示耗时增加；完整每轮值见原始JSON，不复用质量任务时间。

| 源 | B/A墙时 | B/A CPU |
|---|---:|---:|
| 2D | 1.1733 [1.1682, 1.1990] | 1.1708 [1.1163, 1.2147] |
| 3D | 1.2998 [1.2953, 1.3033] | 1.2795 [1.2464, 1.2944] |
| Real | 1.1725 [1.1629, 1.1768] | 1.1904 [1.1728, 1.1964] |

R5在三源相对旧pruned基线的完整曲线改善需要墙时中位约17.3%/30.0%/17.2%的代价；这不改变其相对RD-off SAD在2D/Real仍落后的跨profile结论。背景CPU与CPU比值spread限制了细小速度差异的解释。

00独立速度审计PASS：`C:/Private/ffmpeg-native-m6-tests/optimization-r7-20261018/root/motion-speed-audit.json`。独立重算全部三轮配对/中位数/范围，27条正式流SHA/字节均匹配既有QP100质量流，程序/脚本/合同/协议指纹一致。最终完整性见同目录 `final-integrity.json`：192源及原长测76冻结文件/配置重核PASS。未新增计算。
