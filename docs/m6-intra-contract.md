> 2026-10-25 速度优化已恢复：r4c 同质量体积允许最多回退 1%，优先 AOM 2×耗时。真实熵前缀候选已通过本开发帧原生六点质量审计（体积相对 r4c −3.537%），更快候选仍在测；正式速度门槛未过，见[进展](task-reports/M6-intra-speed-20261025.md)。旧暂停状态保留历史。

# M6-I：单图压缩优先执行合同

> **第三轮执行更新（2026-10-24）**：用户授权4路队列、可多轮测试并使用全部CPU。先以同质量AOM体积1.0为本阶段门槛，再继续原0.85目标；压缩优先，内部仍threads=1，测试可多进程并发。主调度与3子智能体隔离源码，完成后排队接续。旁边官方AVM9档对AOM0档的2D曲线省22.37%，VMAF90～93约省16%～17%，仅为三张开发帧中的2D证据。原最终I-GATE不降低，也不将阶段工具测试称为达标。

2026-10-24。执行用户指定的 `../../M5-M6-AI-TASKS/04-M6-intra-only-first.md`，覆盖旧 M6-C 的亚像素优先顺序。内部固定单线程，先达到同质量 AV2/AOM raw OBU 体积约 0.85，再将同质量编码耗时比压到 2.0；早期允许较高搜索成本。I-GATE 所有条件通过前不进入帧间开发。

## 验收口径

- 主基准为固定 libaom v3.15.1 的 `aomenc --allintra --threads=1 --cpu-used=0 --end-usage=q --cq-level=Q --limit=1 --passes=1 --tile-columns=0 --tile-rows=0 --obu`。`--allintra` 是实际 CLI 名称，映射 `AOM_USAGE_ALL_INTRA`；任务书中的 `--ai` 是参数拼写勘误。不能回退为 AVIF 或较快的 AOM 主档。
- 主指标冻结 VMAF v0.6.1 模型 SHA，另保存三平面 PSNR/SSE；同像素、同尺寸、同 bit depth/chroma/range，至少六个独立 Q 点，保留全部点。主质量匹配容差 0.1。PCHIP 对数码率积分只在合法共同区间计算；非单调曲线报告 N/A。
- raw OBU 统计包含完整合法序列头和帧头。新增 `--still-picture` 必须真正写 `single_picture_header_flag=1`，只接受一张图；多帧输入 CLI 最多读取第一帧，API 拒绝第二张。
- 目前的 `intra_curve.py` 仅用于标定筛查，拒绝读取 holdout。质量运行含重建导出，不能用其耗时判断 2× 门槛。正式速度另需暖机和每点至少五个新进程，编码与解码/评分分开。
- 最终仍需至少 20 张未调参 holdout、分类统计、固定 AVM 逐样本一致、多平台/检测器和源/程序/输入/命令完整指纹。阶段测试通过不能代替 I-GATE。

## 当前实验入口

```text
SvtAv2EncApp --input input.y4m --output image.obu --still-picture --threads 1
  --intra-select cfl --partition-select mixed8 --rd-mode pruned --entropy-adapt on
  --transform-select full --coeff-opt rdoq --angle-search rd --mrl-search rd
  --cfl-search rd --mhccp-search rd --tree-refine wide --rd-lambda avm-ai --qp 100
```

所有新增功能默认关闭，仅用于单图 + RD。目前主线子集如下。

- `basic` 为七种 Y/UV 基本模式；`directional` 加六种零偏移斜向 Y；`cfl` 再允许 UV 隐式单 alpha CfL。CfL 在本候选 Y 逆变换及 clip 后读取重建，支持方形、小矩形及六种长条树所需的17类色度TX形状。宽、高独立处理64维邻居均值采样，4:1矩形仅沿长边取8个alpha拟合样本。`--cfl-search rd` 在此基础上追加经典显式alpha：每色度平面的17个signed q3（−8..8）按真实预测SSE取前二，最多四对接受完整SB/CDF真实J检验；原普通UV/隐式CfL保留。显式参数计入hash、前缀、回滚和重放，语法使用classic index0及七个尾附CDF。此项位于TX后/RDOQ前；还未有其原生六点收益。`--mhccp-search rd`再加入三方向因果MHCCP，U/V各自三个Q16回归参数，只读当前重建Y与可用UV邻居。新增五个CDF保留原986模型；显式CfL与MHCCP互斥，预测参数随Y/参考线/变换/系数变化重新拟合，同样参与hash、回滚及重放。联合原生收益待测。
- `fine` 比较完整 SB 的统一64/32/16叶；`mixed` 增加8叶及四个32象限各选32/16/8的有界坐标搜索；`rect` 保留 mixed 并追加完整 SB 统一16×8、8×16两棵树。`longrect` 再追加32×64、64×32、16×32、32×16、16×64、64×16六类统一树，保留原候选及plan编号。长条关闭angle时用七基本Y模式，开启后增加零偏移斜向及角度搜索；UV为七基本模式或隐式CfL，变换仍DCT，长条MRL与RDOQ已接、primary仍未接（冻结r4c及更早版本仍跳过长条RDOQ）。对应UV尺寸、扫描、4:1 split上下文及长边TX符号同步处理。partial SB 保留旧合法树，尚非任意混合二叉树或独立 TX 划分。
- `mixedrect` 保留longrect的24次树搜索，另对两个32象限矩形族分别作8次坐标试验，总上限40次。plan93..173每象限取32²/两个16×32/两个32×16，174..254将第一项换成四个16²；全零别名93/174不加入搜索。内部计划已扩为uint16，原0..254编号不变，现有partition/CDF语法不变，partial SB不变。新增几何能力已通过强制完整帧，公开入口与原生体积另行核验，不能将synthetic获选视为BD收益。
- `mixed8` 保留mixedrect，追加四个32象限各选32²/16²/8²/16×8/8×16的有界搜索。新计划255..879为base5四象限编码，统一/旧族别名回旧计划；搜索追加16个坐标候选，总上限56，不穷举625。新计划候选ID使用uint64高32位，旧ID不变。独立矩阵、旧流及故障恢复通过，合入后的三格式公开入口桥接通过；与MHCCP的联合验证12/12及分配失败恢复3/3已过，冻结r4c六点正在运行。
- `thinrect` 在mixed8后追加统一Y8×32/32×8两棵树（plan880/881）；`mixedthin` 再作四象限各两种thin形状的8次坐标试验，总上限66。共享base7 helper把旧几何映回旧ID；新增1774个稀疏canonical计划（首887、末3281），其余raw别名拒绝。UV420对应4×16/16×4，方向/CfL/RDOQ可用，新thin保持DCT/MRL0。单项fixed/SAN通过，组合与原生收益待验。
- `--angle-search rd` 增加八个方向的 ±3/6/9 度。当前主线覆盖方形、8×16/16×8及六种长矩形Y，48个额外方向经因果 SAD 筛选后进行完整 SB 真实 R+D 验收；UV V/H 与 Y 同基方向时按规范继承偏移。Y 为4tap IDIF，UV 为2tap。UV宽>32禁TR、高>32禁BL，两边独立；矩形包含2:1及4:1隐式广角映射以及TR按宽/BL按高的参考上限。长矩形方向已通过官方完整参考构造及26352完整帧、同组fresh SAN；主线专项4/4通过。
- `--transform-select primary` 在方形 Y4/8/16 上比较 DCT_DCT 与 fixed v1 tuned ADST_ADST，使用 reduced_tx_set=1。EOB≤1 时隐式 DCT，必须重新按 DCT 量化；其它尺寸、矩形及 UV 保留 DCT。另可选 `full`：使用reduced_tx_set=0及七符号CDF，在上述方形及8×16/16×8亮度矩形搜索DCT_DCT/ADST_DCT/DCT_ADST/ADST_ADST，保留4×4 Paeth特例及EOB约束；矩形有独立前/逆变换与归一化。UV继续显式chroma_dctonly。旧方形full的原生2D边际约0.18%，新增矩形尚无完整曲线，不作主力压缩结论。
- `--mrl-search rd` 在方形Y8..64、8×16/16×8及六种长条形状上搜索三个非零参考线及单/多线六候选，位于角度后、TX/RDOQ前。包含MRL角度±1修正、2:1/4:1矩形广角、SB顶边参考限制和bottom-left/above-right邻居CDF，TR按宽、BL按高独立封顶。UV仍为0，8×32/32×8/8×64/64×8不放开。90°/180°方向的无用导数表越界读取已修正，单独保留检测器反例和axis回归。隔离增量已通过62208固定AVM完整流；当前组合另行验证。
- `thin64` 在mixedthin的66次树试验后追加统一Y8×64/64×8（plan3283/3284），UV420为4×32/32×4。新wide保留旧最多八个完整候选并另加这两棵树，最多十个；不改变base7或partial plan0。新shape方向需angle-search，ADST/MRL仍跳过；强制几何7656条fixed验证已过，公开RD组合与原生收益待验。
- `--tree-refine rd` 保留基本预测排序的两个不同树候选；`wide` 再合入plan87之前旧域的两个最佳候选，按decision/hash去重后最多四个。新thinrect/mixedthin的wide为global4+pre-long top2+pre-thin top2最多八个，pre-thin位于旧mixed8搜索完成后；旧selectors0..7池不变。各自在相同SB入口及committed CDF上执行完整angle/MRL/TX/RDOQ链，按最终真实J选优、重放后只提交一次。完整快照包含重建、系数、预测/参考线及统计，失败恢复当前SB入口。三格式专项实际触发四个候选并与冻结公开CLI流/重建逐字节一致；尚未穷举所有树和预测组合。
- `--intra-joint rd` 为默认off的有限Y模式与primary TX联合搜索，要求still、directional/CfL、RD和full TX。在原primary后，从13个basic Y的真实DCT-J前二枚举四TX，保留原mode/MRL/TX tuple；新模式MRL置0，后缀重建和整SB真实CDF率验收。与MH组合时从同basic seed分别完成joint/no-joint及MH/no-MH最多四路线，同J保旧joint0组。单次分配两个完整工作快照，失败恢复入口；group观察事件表示各joint组跨MH路线的最佳完整J，不是每个原子分支。局部六组与独立gate验证已过，整合四路及原生收益待验。
- `--coeff-opt rdoq` 为真实 level 邻点/EOB 的有界搜索。主线局部proposal比较全局lambda/16、/4及1倍三档，都从相同scalar系数提出，最终完整 SB 逆变换/clip、committed CDF 真实率及全局 lambda 的 J 严格不增验收。主线已在固定AVM变换/归一化、完整RD与检测器验证后移除六类大矩形及其UV的跳过限制；冻结r4c仍保留旧限制，后续组合的原生收益另验。保存实际 qcoeff/TX type 重放，不是完整 trellis/TCQ。MH与RDOQ共开时保留同启用语法下提出/不提出MH的两条完整链，按最终J选择；计数为全部搜索工作。
- `--rd-lambda avm-ai` 为显式单图RD实验项，默认`original`。按fixed AVM AI基础rdmult的整数顺序换算当前D/R单位，lambda_q8=(rdmult+1)/2；量化、RDOQ局部比例及Y/U/V权重不变。510个8/10bit全CQP数值与独立官方C探针逐项一致，444色度权重仍不同于官方，尚不宣称完整官方运行态或BD收益。关闭选项仍走原函数，公共结构新增尾字段，需重新编译。

单图 CDF/RD 从每个 SB 的 committed bank 复制到 scratch；失败或未提交候选丢弃，成功重放后才提交。模式、partition、系数上下文、重建同步回滚。固定前缀缓存仅减少重复重建，不改变完整 tile/CDF 率或候选域。

新增配置为公共结构尾附字段，调用者须重新编译；正式 ABI/版本尚未升级。旧多帧工具和关闭配置须保持完整码流及重建字节一致。主线未启用 IBP、独立 UV 树、滤波闭环、完整变换族或 TCQ；第二轮隔离候选通过规范核对与整合验证后才修改本节边界。

## 当前状态

第三轮记录：`task-reports/M6-intra-avm-tools-20261025.md`。冻结r3已通过Windows严格74/74、45组最终CLI和979模型×4的独立CDF核对，长条full语法864流固定AVM逐像素一致；Clang与GCC ASan/UBSan各73/73通过，GCC原72/73后单项超时复测通过，三平台各45组流/重建SHA一致。后续LUT、长条CfL与mixedrect另有专项及整合验证，不套用冻结r3全套结果。已测最好完整曲线已为r4c组合：对AOM+11.8334%，直接对r4b−0.8450%，直接对最初directional−34.4712%；单帧CPU2306～3817秒，2倍目标未达。旧r4b对AOM+12.9993%，直接对r4a−0.8120%，直接对冻结r3−12.5782%。旧AIλ单项补齐同Q网格后对AOM+20.8975%，r4a/r4b直接对该同网格分别−5.9819%/−6.7522%；原六点对AOM+20.6497%仍保留。旧rect/top2对AOM+24.6374%。冻结r3对AOM+28.0453%，直接对旧top2回退2.3347%；各对比使用各自公共区间，不累加百分比。归一化λ单项六点已完成并独立核验；λ加长条CfL完整六点对AOM+20.9075%，直接对λ单项VMAF BD回退0.1973%，色度PSNR改善，冻结r4a/r4b均已完成Q190/160/130/110/90/70新增组合；r4c439已合mixed8/MHCCP并完成fresh MSVC构建，完整联合门槛与原生六点独立审计均已PASS。r4d含thin/mixedthin、三档RDOQ及MH两链，最终联合门槛已PASS，原生六点后按用户要求中断；r4e默认off联合Y模式/TX已通过原完整104项、公开45项、off兼容6项及独立四route10组，原生曲线亦按用户选择中断。当前所有测试已结束，任务暂停。正式仍0.0.5-m5，I-GATE **NOT_RUN / 未达标**；20张holdout未编码评分，不进入帧间。
