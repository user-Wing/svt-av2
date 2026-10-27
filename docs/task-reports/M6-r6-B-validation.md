# 基础能力 R6：B 有界wide运动搜索验证协议

2026-10-18，当前 `质量与独立速度PASS`。R6补充基础运动搜索能力，不增加AVM新工具族。算法合同与机制验证由00/A负责，本报告只记录独立效果/性能口径。默认off与正式 `0.0.5-m5` 不变。

## 事前固定的比较

输入完全复用R5已冻结三类原1080p、10bit420、索引48..71的24连续帧，manifest SHA256 `3556e88fa1a2ddea6692fd35bbf8f5e2f4ad39ca4398fb1944cfa6e802f0512d`。QP仍80/100/120/140、low-delay-P、adaptive intra/partition、pruned、coeff-opt off，不改源/帧/模型/lambda。

- A：R5已冻结程序，显式 `--motion-opt rd`。
- B：R6新程序，显式 `--motion-opt wide`。

R6程序已复制到 `C:/Private/ffmpeg-native-m6-tests/optimization-r6-20261018/b/programs/candidate.exe`，SHA256 `c76e193b26be5a300684df3ff1e88862facbad85eb8be209c78d05b1c62bed2f`。R5完整12pair已PASS后启动R6；基线总result SHA256为 `c8ae51982ca717c4a3b1be60145634fecf047a6a5ca0272edd44b6b4e49ae220`，已作为稳定基线冻结。

## 质量

新工具 `tools/m6_r6_quality.py` 必须先读到R5质量总结果PASS、完整12pair覆盖、与原manifest hash一致，并逐条核验R5候选case result、保留流SHA和其质量/trace等于总结果。随后将R5候选质量和流作为R6 baseline重命名引用，冻结稳定R5总result与12项case hash；不重编码12条baseline，不把中间RUNNING结果当基线。

仅新增12条wide候选编码，最多2工作槽，编码/AVM/VMAF不叠加其它工作池。复用R5已冻结的逐帧可见D/PSNR与AVM比较函数，同一fps/模型的24帧VMAF，完整bytes与所有点如实保留。每条1800秒、全局6小时预算；四QP严格单调且共同质量区间存在才计算BD，否则N/A。局部J不劣与shell接收机制不代替多QP效果，也不承诺wide一定有效。

## 独立速度（首次计时前共同窗口修订）

原工具 `tools/m6_r6_speed.py` **显式给A传rd、给B传wide**，修正了不能直接复用R5默认off baseline的问题，但尚未执行。00在首次正式计时前为减少中间版本的重复鲜测，将两套ABBAAB修订为一个共同窗口；原工具保留历史，不改其语义或数据。

新 `tools/m6_r6_joint_speed.py` 采用A=R4 motion-off、B=R5 rd、C=R6 wide，每源ABC/BCA/CAB三轮，各版三次鲜测，三源共27run；每源每版先两帧暖机不计入统计。每轮记录B/A、C/B、C/A墙时及CPU比值，报告三轮中位数、全部值和min/max，保留逐次后台负载。共用的是同一窗口鲜测的B，不复用其它背景条件的旧时间；每轮顺序测试，不冒称同时测量。全流程只码流，不导出recon、不跑AVM/VMAF。

独立协议 `optimization-r6-20261018/b/joint-protocol.json` SHA256 `d1399fdb3b5e71811d5ab4d529d5df67550434c562ed3952fc1febf1aa0494c0`，绑定原corpus/三个程序/共同合同副本。工具强制完整R5/R6质量PASS，绑定计时measure脚本及协议hash；正式启动仍等待00独立验收完成后的明确通知。没有更改原质量样本或QP，也未产生正式速度数据后再改顺序。

开始速度前通知00/A暂停所有新增编译/质量/计算。原冻结长测保持正常运行，因此不具备严格整机隔离，必须如实报告后台负载/波动。与R5质量及R5速度窗口不重叠。

两项新工具Python语法编译通过；引用R5不变的底层函数且记录脚本依赖hash。没有修改已冻结R5工具或矩阵，也未新增源/QP。R6质量12/12已PASS；00最终审计与三源fresh AVM复核通过后，已明确GO，独立速度正在执行。

## 固定12点结果

新增12条wide编码、AVM逐样本重建和24帧VMAF全部PASS；基线复用完整R5候选，未删点或调整源/QP/lambda。总result SHA256 `00b78e58d0d4f23361529bf4e71d6d3ae1740b79d169b1811f479d8aebaf3134`，config SHA256 `a8e3a7bdbae239493a8d4918a451c361a44e120b916826ce16120653633c3a44`。已将result/config/manifest复制到 `optimization-r6-20261018/b/evidence`。

| 源 | VMAF BD-rate | PSNR-Y BD-rate |
|---|---:|---:|
| 2D | −0.079439% | −0.313748% |
| 3D | −0.072969% | −0.007229% |
| Real | −0.118122% | −1.118874% |

这里是固定四点共同质量区间的log-rate PCHIP积分。VMAF收益很小，不能称wide带来实质普遍压缩改善；收益/代价还需独立速度。Real QP140出现直接退步：177776→178091字节，VMAF82.481095→82.424364。每点如下（A=R5 rd，B=R6 wide）：

| 源/QP | A字节 | B字节 | A VMAF | B VMAF |
|---|---:|---:|---:|---:|
| 2D/80 | 1162634 | 1161620 | 95.137246 | 95.133441 |
| 2D/100 | 562387 | 562387 | 92.948348 | 92.951499 |
| 2D/120 | 286892 | 286791 | 88.962265 | 88.972907 |
| 2D/140 | 147469 | 147518 | 81.003168 | 80.982919 |
| 3D/80 | 788415 | 787866 | 95.874503 | 95.874056 |
| 3D/100 | 393152 | 392769 | 93.705089 | 93.712054 |
| 3D/120 | 211047 | 210831 | 89.349311 | 89.346977 |
| 3D/140 | 118432 | 118538 | 80.439541 | 80.453402 |
| Real/80 | 2486834 | 2487580 | 95.877166 | 95.883232 |
| Real/100 | 1051541 | 1051863 | 92.787776 | 92.799033 |
| Real/120 | 414877 | 414122 | 89.091111 | 89.098323 |
| Real/140 | 177776 | 178091 | 82.481095 | 82.424364 |
00已独立核验源/程序/脚本/流SHA、24帧指标、SSE重算PSNR全部PASS，三源QP100 fresh固定AVM raw SHA均匹配，见 `optimization-r6-20261018/root/native-quality-audit.json` 与 `native-avm-spotcheck`。R6改善仅相对R5 rd；R5/R6与RD-off策略的跨profile比较仍须单独报告，不能由同入口保M5候选的单SB机制推得多帧曲线不劣。

2026-10-18 11:46收到00明确GO后启动共同窗口，输出 `optimization-r6-20261018/b/joint-speed`。随后才运行 `optimization-r7-20261018/b/speed` 的RD-off熵配对。计时期间00/A暂停新增计算，原长测继续，故仍非严格整机隔离。
## 独立速度完整结果

00明确GO后，joint窗口27次正式run+9次2帧暖机全部PASS，顺序为每源ABC/BCA/CAB，jobs1。A=R4 motion-off，B=R5 rd，C=R6 wide。测量仅输入读取和码流输出，不导出recon、不跑AVM/VMAF。00/A暂停新增计算，原冻结长测继续；所有采样stage为real-quality，故不是严格整机隔离。每条命令hash、流SHA、墙时、进程CPU、约0.5秒系统/本进程/背景估计负载全保留。

总result SHA256 `7e0854177a1c8c7794a37c1fa1dcff9f701d2f98c951563bcdd9be22d49271ea`，原始与冻结副本在 `optimization-r6-20261018/b/joint-speed/result.json` 和 `b/evidence/joint-speed-result.json`。下表为三轮比值中位数[最小,最大]，大于1表示耗时增加；完整每轮值见原始JSON，不复用质量任务时间。

| 源 | C/B墙时 | C/B CPU | C/A墙时 | C/A CPU |
|---|---:|---:|---:|---:|
| 2D | 1.0531 [1.0337, 1.0710] | 1.0307 [0.9480, 1.0621] | 1.2395 [1.2356, 1.2511] | 1.1856 [1.1100, 1.2521] |
| 3D | 1.0790 [1.0764, 1.0894] | 1.0887 [1.0741, 1.1132] | 1.4062 [1.3991, 1.4111] | 1.3904 [1.3876, 1.3930] |
| Real | 1.0187 [1.0165, 1.0191] | 1.0133 [0.9045, 1.0213] | 1.1918 [1.1852, 1.1988] | 1.2122 [1.0608, 1.2158] |

三源全部三轮墙时C/B均大于1；约5.3%/7.9%/1.9%的额外墙时仅对应约0.08%/0.07%/0.12%的VMAF BD收益，当前证据不支持将wide当作默认压缩效率升级。2D/Real个别CPU配对波动较大，原始值与背景采样全部保留。

00独立速度审计PASS：`C:/Private/ffmpeg-native-m6-tests/optimization-r7-20261018/root/motion-speed-audit.json`。独立重算全部三轮配对/中位数/范围，27条正式流SHA/字节均匹配既有QP100质量流，程序/脚本/合同/协议指纹一致。最终完整性见同目录 `final-integrity.json`：192源及原长测76冻结文件/配置重核PASS。未新增计算。
