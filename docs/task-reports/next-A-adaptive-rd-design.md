# 自适应CDF进入RD事务：只读设计

2026-10-18。基于已冻结R7源码审计，不实施生产、不编译或编码。R7首关继续保持仅RD-off；本设计尚未实施或验证，不代表该组合已经可用。00最终核验96组流、Win49/GCC48及SAN6项通过；本报告不将其外推为自适应RD已经验证。

## 建议最小路径

沿用R7显式模型ID、初始化和更新内核，以及现有`tile_tree_write`共享语法。先独立实现规范位置调整概率的Q12估计，再让rate session拥有一份committed bank和一份scratch bank。每个SB候选都从同一个committed bank重新复制，完整候选内部按符号顺序逐步更新scratch；仅正式winner commit推进committed。首个公开闭环可先限制`entropy_adaptation+RD`时coeff/motion实验均off，基础exhaustive/pruned及partial全部按原候选规则工作。之后逐项验证coeff、motion-rd、motion-wide组合；不同时改候选、lambda或训练offset。

这不能实现成“候选使用静态R，最后writer才自适应”，也不能让上一个试探候选的尾CDF成为下一个候选的入CDF。每个SB仍是贪心局部RD；选择造成的后续模型变化不在当前SB的J内，因此不保证整帧/整段字节或质量更好。

## 率估计：固定AVM已经给出位置调整方法

当前`entropy_models.c:39`明确拒绝rate_only；`entropy_encoder.c:184-190`用raw forward相邻差并拒绝零差。R7直接更新oracle已观察到大量相等raw边界，不能直接解除这两个拒绝后沿用旧估计器。

新证据是固定`avm/av2/common/cost.c:30-41`：`av2_cost_tokens_from_cdf`逐边界调用`get_adjusted_prob`，再对调整后的inverse边界求差。`avm_dsp/prob.h:249-255`给出该函数；它与实际`od_ec_prob_scale`共享位置padding。在当前CDF_SHIFT=0、EC_PROB_SHIFT=7下，定义：

```
A(p, i, n) = 128 * floor(p / 128) + 8 * prob_inc[n - 2][i]
U = (symbol == 0) ? 32768 : A(inverse[symbol - 1], symbol - 1, n)
V = A(inverse[symbol], symbol, n)
p15 = U - V
R_q12 += existing_probability_cost_q12(p15)
```

inverse含terminal0，最后symbol的padding是该row的0项。R7 reachable N=2..8对应padding项严格下降，所以即使相邻raw相等，合法模型的p15仍正；所有symbol概率望远镜求和为32768。这是规范调整，不是给CDF塞epsilon，也不改实际更新状态。对越界/非单调/不合法模型仍报错，不能静默clamp来隐藏模型错误。

本项目可以保留原Q12整数log实现，不必复制AVM的Q9近似表；独立核验对象首先是每个p15和状态轨迹，不能宣称两种不同近似精度的整数cost逐值相等。literal仍每bit4096，CDF只在模型symbol成功后更新，Rice/quniform旁路不更新。所有冻结off/RD路径继续使用原raw估计器，禁止全局替换导致旧赢家变化。

该R仍是估计bit，不是最终OBU字节长度。实际range coder还有`rng>>8`和乘法取整、renormalization、carry与末尾flush，不能用“同一个padding”证明Q12总和等于实际bytes。首版验收应独立量化二者偏差，但不以固定阈值强行声称精确。符号成本来源正确与编码效果正收益是两道独立验收。

若后续确有必要，可研究携带normalized rng的无输出range状态，计算实际range收缩率；那还要把rng纳入SB快照/commit并处理literal规则，属于另一版本。更不能将每个SB独立finalize后的字节数相加，当作连续整tile真实R；终止开销与carry不能这样分解。首版不用该更复杂路径。

## 两份bank事务与恢复

当前`tile_writer.c:318-325`的session只有tree、plan、cursor和saved_motion。`tile_rate_trial:398-423`先`TileTree trial=s->tree`，但motion.cells是共享指针，已有SB矩形备份/恢复；不可误认为struct浅拷贝自动深拷贝全部状态。R7的models指针位于局部enc，正式writer使用自身fresh bank。

建议session在adaptive时一次分配连续两份`SvtAv2EntropyModels`，在free释放；off保持NULL，不做每候选分配。无需把bank嵌入每个TileTree，避免正式writer树再增加一个未使用bank、加重默认off的栈/清零成本。session中两个bank的身份固定，不用指针交换隐式改变commit意义。

1. create：按frame qctx初始化committed；scratch可等第一次trial复制。cursor=0，沿用空邻居/partition状态。此处只建立rate状态，不写实际header。
2. trial：校验SB索引和blocks/modes；保存plan与共享motion区域；复制完整committed到scratch（含count、terminal、3offset）；原tree浅拷贝后enc绑定scratch，rate计数清0。候选leaf之间持续使用同一scratch，不在每叶重置。
3. trial失败/非commit：恢复plan/motion，丢弃scratch内容，不碰committed/tree/cursor。scratch不属于已提交语义状态；下一trial无条件重新复制，不能靠失败分支“只恢复改过的row”猜测完整性。
4. commit：只有完整语法成功、blocks消费数正确，且winner重放检查通过后，复制scratch到committed并提交trial树、cursor。将enc/借用blocks/modes指针照旧置NULL；不得留下指向局部trial bank或局部enc的指针。
5. frame失败/新frame/新实例：释放整个rate session。packet容量重试只重写已选coeff，正式writer另起fresh默认bank，不能用RD帧尾bank写第一符号。

模型不会代替现有recon的region_copy，也不会代替motion/partition/邻居恢复；这四类状态都必须从同入口重建。R5/R6/coeff后置试探的rate_trial也必须回到同一SB committed bank，不能从刚接受的另一个试探的post-bank继续；它们的接受只更新当前最优policy，完整path依然从SB入口重放。

## 最小接口变化与重放证明

| 文件/接口 | 需要的变化 |
| --- | --- |
| entropy_models.c | 对adaptive+rate_only走新位置调整Q12估计，再更新同row；formal保持R7编码后更新；显式ID与数据不改 |
| entropy_encoder.c/h | 新增小型调整概率helper或复用现prob_inc定义，保持padding单一来源；旧raw rate函数不变。正式audit时可计算相同Q12，但不能把正常formal性能无条件加上log开销 |
| tile_writer.c | create/frame仅在已实现adaptive rate路径后解除拒绝；session两bank、trial绑定/复制/commit；full-frame rate replay用fresh单bank |
| tile_writer.h | 保留旧trial签名作为兼容wrapper；可加内部trial审计结果（R、post-bank digest、symbol序列digest/count），既有接口调用NULL即可。禁止借用局部bank指针返回调用者 |
| rd_frame.c | 现有best_d/best_r/path_hash之外保存best post-bank digest；winner非commit重放和commit重放都核对R与post-bank。candidate ID/原path hash/off winnerhash保持原格式 |
| Codec/API配置 | 只在所有机制通过后放开事先约定的adaptive+RD组合，早期仍拒绝未验证的coeff/motion组合；把同entropyflag传给RD frame_header及正式packet header |

`rd_frame.c:564-583`已有build_path重建、D/path hash/非commit R检查，再append_winner与commit R检查。新增模型摘要必须同时覆盖原始候选、非commit重放、commit重放；摘要不能只hash系数，CDF count/训练offset也要覆盖。定义固定顺序uint16值序列，避免平台字节序或struct padding影响。生产digest用于定位/重放防错；机制单测对bank完整memcmp，不能将有限hash无碰撞当数学证明。

不要只验证帧尾摘要：还要测试每个SB提交前/后模型状态。最终正式writer从fresh默认bank顺序写所有赢家；测试审计在相同SB边界输出R增量、symbol序列与bank状态，逐SB对上RD commit。全帧estimated R等于各SB winner R之和，formal audit估计也应一致；实际字节长度单独记录，不要求相等。formal与rate必须共同经过现tile_tree_write/motion_syntax/coeff_syntax，不复制第二套语法。

只需要内部/测试审计接口，不为产品CLI新增每symbol日志。可以只在审计启用时输出SB摘要；正常路径不遍历完整bank给每个symbol算hash。partial SB的原单候选和replay也不能跳过模型提交。

## 内存与成本上界（推导，不是测量）

当前bank为960×12×2=23,040字节=22.5KiB。两bank每adaptive rate session增46,080字节=45KiB；可只在create申请一次。full-frame独立rate replay另需22.5KiB，正常正式writer也保留R7的22.5KiB，但RD session在重建完成时已经释放，不必同时保留。只存每SB的64位audit摘要，1080p的510个SB每条摘要数组是4080字节；不要存510份完整bank（那会约11.75MB）。

每次trial至少复制23,040字节；成功commit再复制23,040字节。因此T次trial、S个提交SB的**逻辑复制量**是B×(T+S)，其CPU读写流量至少是该复制量的两倍，缓存效果另计。若每trial还完整hash post-bank，再增加约B×T读量；应把它纳入计时或把非必要完整hash限定到新best/重放审计。

现代码每完整SB最多2棵树、每树至多4叶、每叶4模式，保守候选上界512（合法性会减少；不是观测均值）。R6额外motion-rd≤16、wide≤8，与coeff互斥；再含winner非commit与commit两次trial，最宽组合T≤538S。对1920×1080用S≤510保守计，bank逻辑复制量≤约6.34GB/帧；这个极宽上界包含大量实际不发生的非法/partial路径，但足以说明不能承诺复制免费。首公开闭环禁motion/coeff时用T≤514S。pruned与实际合法候选应另记T、模型symbol数、copy/hash字节数，实测单路CPU/墙时。

每symbol额外工作是调整两边界、一次Q12 log和最多N−1≤7次CDF更新；不增加重建候选数量、不扩线程。offset/count等完整复制可先保持简单，只有真实性能证据再讨论按row journal/epoch等复杂优化。不能用COW或共享bank来换取错误的候选串用。

## 分期与退出门槛

1. **只测估计内核，生产组合仍拒绝。** 所有R7模型、N2..8、所有symbol、count15/16/31/32与长偏置相等边界，逐p15对照固定get_adjusted_prob；核总概率32768与finite Q12，证明model更新轨迹与R7 formal完全相同。旧off Q12回归必须逐值不变。actual arithmetic符号流另做固定AVM对照，报告估计与bytes差别。
2. **事务机制仍不公开放行。** 多SB/path用同entry full-bank基准，反序/重复候选、含大量相等边界、不同CDF初值alias、错误发生在coeff中途、容量/非法mode/partial、双实例与新frame，逐bank完整memcmp；逐SB独立最小J及平局ID，R/D/recon/path/post-bank重放一致。覆盖commit失败不推进cursor/plan/motion/recon。
3. **最小public闭环。** 首先adaptive+基础RD、coeff/motion off；默认off和R7 RD-off逐流兼容。三格式AI/P、四qctx与工具边界、多帧，用固定AVM逐样本校验，并对formal/RD逐SB symbol/R/bank轨迹。再依次验收coeff、motion-rd、wide组合；没有全部过关前继续拒绝相应组合，不产出半适应流。
4. **真实收益与性能分开。** 冻结同样本/同QP/候选/λ，记录是否实际bytes/质量受益，独立单路比较CPU与内存成本。adaptive RD会改变赢家和后续预测，不能复用R7同recon质量结论，也不能因首次曲线不佳调训练offset/λ或换片挑点。

所有阶段先锁合同与基线、遵守原campaign/共同速度窗口；本报告只提供可落地分期，不执行下一轮。
