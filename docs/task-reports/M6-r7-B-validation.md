# 基础能力 R7：纯熵适应的独立验证协议

2026-10-18，当前 `READY_FOR_INTEGRATION/质量与独立性能通过`。12个固定native配对全部同重建，纯熵编码完整字节减少6.989%～20.029%。范围以 `docs/basic-entropy-contract.md`为准：frame内CDF适应，仅RD-off；固定模式/MV/qcoeff/recon，不组合R5/R6运动RD，也不调整训练offset或lambda。正式仍 `0.0.5-m5`，不称基础阶段全部完成。

使用同一事前冻结native corpus：三源1920×1080/10bit420、索引48..71连续24帧/1.001秒，原manifest SHA256 `3556e88fa1a2ddea6692fd35bbf8f5e2f4ad39ca4398fb1944cfa6e802f0512d`。QP80/100/120/140、P、adaptive intra/partition，RD/coeff/motion全off。baseline为冻结R6程序，candidate相同配置加 `--entropy-adapt on`，12配对/24编码；R6旧CLI没有entropy开关，baseline依赖其原始默认off，不传未知参数。

`tools/m6_r7_quality.py`复用不变的R5逐帧质量/AVM验证函数，先独立首对估时，再最多jobs2完整矩阵。每条baseline和candidate都要固定AVM逐样本一致；candidate recon SHA及三平面SSE/PSNR必须严格等于同QP baseline，任何变化直接失败。baseline每点评分一次VMAF，candidate只有确认像素完全相同后才复用该值，并标记 `quality_reused_from_baseline=true`，不冒称新评分或视觉质量改善。相同像素下直接报告完整字节差和百分比，不依赖BD插值。

原完整2D off约600秒/383帧仅提供预算量级，24帧可能约35～40秒/次，24次编码串行约14～16分钟、jobs2约7～10分钟；适应实现和背景负载另计。这是事前估算，不是R7实测速率。若R5完成时R7尚未稳定，则先执行R6质量，不等待本项阻塞。

`tools/m6_r7_speed.py`另做RD-off A/B单路鲜测，每源各两帧暖机后ABBAAB三对，三源18次实测。只码流输出，无debug recon/AVM/VMAF，不拿此时间与RD-on motion joint窗口直接相除。记录进程CPU/墙时/背景负载、三对全值/中位数/范围及程序/命令/输入指纹。开始前需00明确调度并暂停所有新增计算；旧长测仍运行，不称严格整机隔离。

每命令1800秒、质量及速度各自全局6小时预算；原campaign进入isolated-speed时停止自己的子进程，不操作原长测。R5/R6冻结工具、程序、配置和结果不改；R7所有输出在新 `optimization-r7-20261018/b`。两项新工具已通过Python语法编译。

00完成生产稳定验收后，B复制固定candidate程序到 `b/programs/candidate.exe`，SHA256 `3d9b45c047edd3682c158426827e1ed10840a4e72aa3cde6b9f52394550e8dd9`；合同副本 `b/basic-entropy-contract.md` SHA256 `c0a324b59ac16a8f181fe8cd418eb24bad1b1015df5217d0be2a5661d41038ff`。按00调度，R5完整PASS后完成R7首对及全质量，再启动R6质量，没有让新增编码超过2槽。速度窗口另等明确启动通知。

## 原生24帧实测

首对2D/QP80：baseline编码25.97秒、candidate29.65秒，含debug recon导出，**不作为独立性能结论**。确认同recon/AVM后复用首对PASS，jobs2完成全部固定12pair/24流；没有改变QP/源/帧或训练数据。

| 来源/QP | entropy-off完整字节 | entropy-on完整字节 | 同像素字节变化 |
|---|---:|---:|---:|
| 2D/80 | 1296423 | 1185325 | −8.570% |
| 2D/100 | 621002 | 561197 | −9.630% |
| 2D/120 | 328738 | 289053 | −12.072% |
| 2D/140 | 186423 | 153577 | −17.619% |
| 3D/80 | 955243 | 787938 | −17.514% |
| 3D/100 | 437136 | 349581 | −20.029% |
| 3D/120 | 242768 | 196622 | −19.008% |
| 3D/140 | 142545 | 114782 | −19.477% |
| Real/80 | 3508913 | 3225587 | −8.074% |
| Real/100 | 1539063 | 1431502 | −6.989% |
| Real/120 | 569019 | 521057 | −8.429% |
| Real/140 | 236188 | 209218 | −11.419% |

每对baseline/candidate的recon SHA和三平面SSE/PSNR严格同值；每条流的固定AVM解码逐样本等于内部重建。12个baseline VMAF各有恰好24帧；candidate没有新VMAF评分文件，仅在像素验证后复用baseline数值，明确标注复用。这是同质量的直接字节收益，无需依靠BD插值。B外部矩阵验证的是重建；模式/MV/qcoeff不变的内部机制证明由00/A测试另行覆盖。

不能把这些结果写成视觉质量提升，也不能和R5/R6不同RD配置直接做因果或速度相除。RD-on与CDF适应尚未在本首关组合，全部结果仍限于这三段固定1.001秒片段；对当前AV1差距的总贡献须用明确profile曲线比较，不声称解决。

00的中期跨profile审计还显示：以本节entropy-off/RD-off baseline作参照，R5 motion-rd的VMAF BD在2D/Real分别+2.440%/+25.428%，3D为−5.961%。因此R5虽改善旧pruned RD，也不能称比旧SAD路径普遍更好。这里的CDF收益只在同像素严格配对中归因于熵适应；不能用它掩盖RD基础效果问题。跨profile最终结果待R6完整后统一审计，未调lambda或将不同共同区间BD相乘。

## 独立审计与证据

B `b/evidence/verification.json` 重新核验3源、24条流字节/SHA、12对重建/全质量同值、12份baseline评分与候选无重复评分、程序/脚本/模型依赖，全部PASS，且已复制程序/合同/结果/配置。完整result SHA256 `45808f61898a2936db1e041fface175199b3c1369ede2b857f0f2cc0e7120357`，config文件SHA256 `00d1f0948bc6f317f449a8bca4701b45e8599230d8de1db79e39db5634087a66`。

00另有完整独立审计 `optimization-r7-20261018/root/native-quality-audit.json` PASS，并fresh解码三源QP100 entropy-on为raw，3/3 SHA独立等于两端encoder recon，见root/native-avm-spotcheck。独立性能结果见后节，不把本节质量耗时当作性能证据。

## 独立RD-off速度结果

共同motion窗口结束后才启动此独立窗口。三源ABBAAB各6次正式+各版2帧暖机，总18正式+6暖机全部PASS；每次正式QP100流SHA严格匹配对应quality场景/版本，而不只核对重复测量自身。无新增编译/质量/评分并发；原长测保留，仍非严格整机隔离，逐次背景负载全部记录，不与RD-on motion绝对时间相除。

result SHA256 `b85b1cb47af79885eaae2473a4d6a663c68bd413383c679ed36b050a93c146e8`，见 `optimization-r7-20261018/b/speed/result.json` 与 `b/evidence/speed-result.json`。下表是on/off三配对中位[最小,最大]，大于1表示更慢；全部3值/命令hash/流SHA/CPU/墙时/负载采样保留于JSON。

| 源 | on/off墙时 | on/off CPU |
|---|---:|---:|
| 2D | 1.1255 [1.1237, 1.1380] | 1.1208 [1.1095, 1.1389] |
| 3D | 1.1320 [1.1291, 1.1382] | 1.1397 [1.1068, 1.1756] |
| Real | 1.1694 [1.1284, 1.1850] | 1.1196 [1.0327, 1.1752] |

三源全部三配对墙时on更慢，中位增加约12.6%/13.2%/16.9%；CPU中位增加约12.1%/14.0%/12.0%。同像素的6.989%～20.029%码量收益成立，同时存在这些编码时间成本；只适用于已验证RD-off合同，不能隐含推广到RD-on。

00独立速度审计PASS：`C:/Private/ffmpeg-native-m6-tests/optimization-r7-20261018/root/entropy-speed-audit.json`。独立重算全部三配对/中位数/范围，18条正式流SHA/字节均匹配既有QP100质量流，程序/脚本/合同指纹一致。共同motion窗口审计见同目录 `motion-speed-audit.json`；最终 `final-integrity.json` 已重核192源与原长测76冻结文件/配置PASS。未新增计算。
