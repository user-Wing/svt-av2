# M6 隔离候选 A：有界系数 RD 精化

2026-10-18，状态 **READY_FOR_INTEGRATION**。遵循`docs/m6-r4-coeff-contract.md`；这次允许实验候选改变码流以检验压缩价值，默认关闭，正式仍0.0.5-m5。效果是否值得保留，由B预冻结样本和00独立验收决定，不能由局部J或机制单测放行。

## 实现与真实评分

A仅修改`Source/Lib/Encoder/rd_frame.c/h`，新增`tests/unit/test_m6_coeff.c`；公共API/Codec/CLI/CMake由00接线。内部`coeff_optimization=0/1`，默认0；新增实际统计`coeff_trials/coeff_accepts`。

先按原有RD合同选出完整SB的原始树/模式赢家，固定该树与模式，按raster叶序做一轮坐标精化。每叶分别试：policy1各平面只保留DC，policy2各平面全零；已有policy0原量化保留，其他叶使用目前已接受的policy。最多4叶、8条附加完整路径。partial跳过，仍走原边缘合同。不是完整RDOQ、完整系数穷举或全帧最优。

每条trial先从同一SB入口恢复三平面重建，再按固定模式重新预测、transform/quant、对本叶三平面的packed qcoeff/dqcoeff作相同置零、inverse/clip。后继叶全部随新邻居重建重新预测与量化，没有沿用旧SSE、旧预测或旧系数。DC-only实际保留每个平面的index0，TX64仍按1024 packed系数。

完整SB可见重建SSE经原D公式加权，R来自原tile_rate_trial同语法事务，lambda和整数J完全不变。仅更小J或等J更小稳定ID接受。低9bit保留原树/模式ID；bit9起每叶2bit表示policy。policy非0追加进path hash；policy0不追加零值，因此默认off的winner hash仍与旧实现相同。

试探均不提交writer状态。最后从入口按最终policy重放整path，核对D、真实语法R和包含模式/MV/coeff/recon/policy的hash后才append赢家并commit一次。失败恢复当前SB，帧输出计数清零，重试重新初始化。每次接受只证明同一入状态的当前SB J不增，不外推整帧/后续参考链J、PSNR或VMAF。

## 机制单测

MSVC `/W4 /WX`与GCC `-Wall -Wextra -Wpedantic -Wshadow -Werror`均全新外置构建（j2）：

| 检查 | Windows | GCC |
| --- | ---: | ---: |
| 原test_m6_rd | 8813/0 | 8813/0 |
| 新test_m6_coeff | 13041/0 | 13041/0 |

新增测试覆盖：

- 8bit420、10bit420/444，AI/P，64/128完整SB、136x72与120x120混合边界、56x56全partial。
- 原有QP100高纹理fixture，加低幅独立合成纹理QP80/140/220；后者确保真实生产transform下确有`coeff_accepts>0`，防止只跑未接受分支。前者无accept是实际结果，不隐藏。
- hook独立检查每个候选的整数J，并确认提交的是本SB所有已观察原候选及精化候选的最小J/最小ID；单64SB直接与相同入口的flag0成本比较。
- 额外trial不超过fullSB×8、accept不超过trial，候选增量等于trial；partial没有trial且hash与flag0相同。
- 解码提交候选ID的逐叶policy，直接检查最终输出packed qcoeff的对应AC或全系数为0；覆盖四叶ID位域合法性。
- 从输出重建独立逐像素计算D；正式整帧tile rate重放R等于累计SB R。
- 原模式候选反向枚举后，最终coeff/recon/hash/J/R保持一致；pruned仍是原模式域子集，系数精化不冒充扩展域穷举。
- 输出容量失败后可重试；单64SB故障注入定位在首个系数trial的forward调用，错误后干净重试完全恢复；源与committed reference含padding指纹保持。

首次严格MSVC发现trial_cost控制流赋值无法被编译器证明，已显式零初始化后两平台通过，未放宽告警。未改原test_m6_rd。

## 效果证据边界与产物

A没有另开效果矩阵或长测。B负责预先冻结的6小图×4QP×AI/P、固定AVM逐样本及真实bytes/D/PSNR比较，00负责默认off指纹、公共API/CLI、sanitizer和最终集成验收。只有这些结果能决定是否产生真实等质字节收益；同QP少字节不等于等质改善。

独立构建与日志位于`C:/Private/ffmpeg-native-m6-tests/optimization-r4-20261018/a/`，入口`delivery-hashes.json`、`rd_frame.diff`、`rd_frame.h.diff`、Windows/GCC编译和两项test日志。基线来自00冻结的173源`root/baseline-source`。

所有编译/测试开始前只读确认原长测处于real-quality；新增计算有实际CPU/内存占用，不宣称对后台零影响。没有触碰原campaign快照、媒体、模型、运行依赖和配置，未进入isolated-speed时继续计算；默认off与正式包不变。
