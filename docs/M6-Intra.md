# M6 帧内研发

## 进展与证据边界

帧内经历从早期同质量比 AOM 约大 28.99% 到不同内容最佳研究点约 0.88–1.00 倍的迭代。各数字涉及不同样本、质量区间及 V1/NEG 约束，不能拼接成统一全样本结论。当前记录到 r13；正式 0.0.5-m5 不等于研究候选已全面验收。

- 早期细分区和方向预测提升压缩；第三轮 CfL、primary transform、RDOQ、矩形和方向组合相对上轮省 22.96%，但当时仍较 AOM 大 28.99%。见 第三轮（见下方历史原文）。
- r4c 单开发帧 VMAF BD 比 AOM 大 11.8334%；当时速度远未达标。见 暂停报告（见下方历史原文）。
- 真实熵前缀与有界 CCSO 在各自条件下改善；带内稠密测点后候选对 AOM 主 V1 93–95 为 1.1851–1.1866 倍。NEG 的共同域不足完整 93–95，不能外推。见 V1 第三轮终报（见下方历史原文）。
- stack1 和 fine16 试验分别记录约 1.074 和 1.055 倍；EOB 完整主区间退化，停止扩展。见 r5（见下方历史原文）。
- r13 的 fine16 pair、色度权重、自动分流和扩展 CCSO 经测试未能稳定替换旧最佳；清平乐 UHD 双约束均值为 1.0895/1.0988，主 V1 为 0.9988/1.0017，但 NEG 下降。实拍 1080 历史双约束均值约 0.881；动画和 UHD 尚未稳定达到 0.90。研究记录 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/size-r13-20261030/STATE.md` 和 `FINAL_STATUS.txt`。
- 分类器冻结终测 23/24；这不等于对所有内容可靠。r13 发布候选含分类器、调用和质量工具；源及测试证据以其冻结清单为准。速度没有完成正式验收。

失败和不稳定结果、重要数值及验证边界已归并在本文历史记录中；部分原始产物已按清理要求删除。今后优先查技术规范，不以 AVM 实现作为主要技术参考。

## r9 至 r13 的可比较结果与取舍

以下为各自冻结开发帧的 V1、NEG 双约束质量带几何均值，分别采用 PCHIP 和线性插值；不是全部素材的平均，也不是当前发布构建全部重新测得的曲线。

| 样本和历史最佳配置 | 相对 AOM p0 的体积比 |
| --- | --- |
| 2D 1080p，r6 | 1.030190 / 1.027603 |
| 3D 1080p，SATD | 1.089011 / 1.088844 |
| 真人 1080p，lambda150 | 0.880928 / 0.881838 |
| 2D 2160p，lambda150 | 1.069609 / 1.070596 |
| 3D 2160p，lambda150 | 1.028779 / 1.029540 |
| 真人 2160p，lambda150 | 1.089529 / 1.098773 |

3D 1080p 的 SATD 搜索相对旧 lambda150 双约束均值约改善 2%，但增加搜索成本；MPM 三点平均 V1 下降，未扩展。相邻系数联合搜索、活动加权 SSE、CCSO 幅度、IST16、受限 split64、TCQ 及静态 TCQ-rate 的有限负结果保留在 r9 研究索引。r11 方向搜索裁块没有明确推广收益；lambda 试验不同裁块方向不一致，不能从局部结论推广整帧。分类器最终以独立 24 帧测得 23 帧正确，不能推断所有内容准确率。

r13 pair 对 2D 1080p 的 Q100 文件变小，但 V1 从 95.786448 降到 95.259939，不能单凭大小推广。调整色度权重后，该帧三点局部双指标同质均值相对 r6 为 0.999866 / 1.009317，无法稳定替换；UHD pair 局部均值 1.003590 / 1.015691 同样未替换。扩展 CCSO band 的包成本试验也未胜过旧方案。详见 `tests/r13/STATE.md` 和 `tests/r13/partial-full-curves.json`。


## 详细历史记录与实验依据

为便于公开，历史文本中的本机路径前缀已替换为占位符；SHA-256 仍指向脱敏前的原始文件，历史命令需按实际环境调整。

以下是清理前相关报告、技术合同、任务记录和汇总数据的归并文本。每条保留来源和 SHA-256；内容按当时的时间和实验边界阅读，其中“当前”“进行中”、旧目标、旧版本、临时路径及未来计划均为历史记录，不覆盖本文开头的最新状态。旧目录及关联的大型测试产物已按本次清理要求删除，原文中的链接和路径可能失效；个别实验只有有限样本或尚未完成验收，不能推广为项目整体结论。


<details>
<summary>001 · m6-intra-contract.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\m6-intra-contract.md`  
原文件 SHA-256：`32f2e2811971de1b270272f56d0a741a326817133892c337c3418b60802d094a`

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

</details>


<details>
<summary>002 · M6-intra-avm-tools-20261025.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-intra-avm-tools-20261025.md`  
原文件 SHA-256：`ca2423e7e878098a3e7911102d3fc070847c7c9bac1ab2de182179696f263873`

# M6-I 第三轮：官方2D归因与单图工具补齐



2026-10-24～25。**进行中，阶段目标尚未达到，I-GATE NOT_RUN，正式版本仍0.0.5-m5。** 用户最新授权4路队列、多轮测试及全部CPU，覆盖旧串行/OpenCode流程；本轮采用主调度+3并发子智能体，任务完成后复用队列。内部编码保持threads=1。



阶段目标先同质量 AV2/AOM raw OBU≤1.0；原最终0.85体积及单线程2×耗时门槛保留。上一版2D对AOM约+70.86%，追平约需再省41.5%；不能将同QP少字节或对自身改进等同于追平AOM。



## 最新接续状态（2026-10-25）



**本轮已结束并暂停。** r4c六点及独立审计PASS：对AOM VMAF BD **+11.8334%**（55.324948..95.230828），直接对r4b−0.8450%，对最初directional−34.4712%；VMAF80/90/93体积比1.1649/1.2174/1.2269。六Q单帧CPU2306.4～3816.7秒、墙时45.8～74.7分钟；AOM CPU6.984～16.516秒。在VMAF90按实测相邻点线性插值约2799/10.40 CPU秒（269倍），是开发量级不是隔离五轮正式门槛，2倍目标显然未达到。r4d/e按用户选择中断，无完整质量曲线结论；所有子智能体、native、CTest、audit及controller均结束。最终报告为`M6-intra-pause-20261025.md`与外置`pause-after-current-round-summary.json`。不继续开发/测试，等待用户恢复。



收尾检查闭环：原r4f Windows完整107项最终105PASS/2FAIL，分别为漏改selector10 setter（生产错误）及将新合法8×64/64×8当非法的旧transform_path负例。前者MAIN一常量修正并fresh r4f2、整项API+三格式公开fixed短复测PASS；后者MAIN两负例移至仍非法4×64/64×4，与原f九个static libs外置整项重编190检查PASS。`integrated-r4f-test-addendum.json`记录全部身份与范围；不是原107一次全绿，也不是新f2完整组合/SAN验收。未启动f原生。



用户进一步明确暂停范围：**仅完成最早r4c，停止较晚r4d/r4e。** 已结束d/e两个controller、两个curve父进程、八个native encoder及对应audit等待进程。各自原RUNNING记录先另存，当前状态STOPPED_BY_USER；所有不完整点、源/程序/协议和进程CPU/启动时间保留，不能据此给d/e六点收益或速度结论。只读收尾程序改为新编号`finish-current-round-pause-r2.py`等待r4c，旧程序停止且源码保留。



用户要求本轮结束后暂停：停止新增实验、后续UV生产及新原生曲线，仅收齐已经启动的r4c/r4d/r4e六点，独立审计后汇总体积与速度；20张holdout不动。收尾程序`finish-current-round-pause.py`只读现有队列/审计结果，终态写`pause-after-current-round-summary.json`及本目录暂停报告，不启动任何编码或开发。



r4e最终联合已PASS，closure SHA `2b62d58becb3af747dcba3168d867e760aba5e1c64d2ed58fa19da7e3c83f279`；5 native+5 fresh SAN、6867916检查、四案单次368544B新malloc真实NULL恢复，public/internal/fixed像素及默认off桥接通过。第三条原生六点已实际启动。



thin64 7core+6test及root selector10接线已合主线并冻结r4f448；但公开setter遗漏上限9→10，导致原45组中36集成初始化失败、9旧流通过，原107完整回归也保留该失败。MAIN已只修这一常量并fresh冻结r4f2；必要整项API与三格式64²/Q100/CDFon公开全工具/fixed重试通过。原f源/producer不热改，新f原生从未启动，用户暂停后已取消其等待controller；其联合gate与原生收益均未验收，不混入r4c/d/e压缩结论。selector9的joint on/off/default9组与旧e逐字节相同。



13×4扩大模式域实验六点局部无最终收益，一个裁块J+264/流+3B，其余不变，独立固定解码/反序及off通过；该变体未合MAIN。61×4用户消息前已启动四案，只收这四案，其余五案保留未运行，不能声称完整通过。UV真实inverse探针29artifact/1517源经root独立重核：Q128/Q96实际ADST占UV非零inverse调用78/904及425/2446，全部五frame/三sequence量化偏移0；UV非自由TX信令，须由UV mode与序列DCT-only/shape派生，EOB1也可保UV ADST。仅只读方案，生产未接。



08:00接续：r4d最终联合门槛已PASS，closure SHA `e8c850f52023a395d5e84c41694f3864fa480e1cd38d58b5482654a965c8f162`；源445文件、三格式及裁块native/fresh SAN各4组、强制新thin六流、368544字节新分配真实失败恢复均闭环。Windows原101/3保留，三项旧负例外置修正后整项通过；`integrated-r4d-test-addendum.json`记录。r2 launcher已启动原生六点，程序77887e59...不变。r4c已完成四点、另两点运行，不由未完成曲线给BD结论；当前完整最好仍r4b对AOM+12.9993%。



r4e冻结445文件（manifest `81e0657f118f8b89f7db4797611ff8ab338eef60bb19a3be054b161b20bf4307`，app `4d69ad95cf84ae5ae7009d90296725f31cd3bb5b6a9bb9d8156e084b9a2c1ac4`），fresh严格MSVC192步通过，原完整CTest **104/104一次通过**；公开CLI45/45通过；三格式省略默认及显式off共6/6与r4d旧流、重建和fixed实际像素逐字节一致。root统一四route的独立native五案例通过，partial和预定原图64²裁块含真实joint收益，fresh SAN五案例仍在运行。仅满足完整组合门槛后才自动启动显式joint-on的原生六点；私有双malloc证据不冒套此版。



官方滤波分组消融12个新点已独立复核，`official-loopfilter-split-independent.json`为24点/126artifact PASS。只关CCSO和保CCSO但关闭deblock/CDEF/GDF/restoration的VMAF BD分别+6.4310%/+6.3394%，全部关闭为+4.2926%；重新搜索导致交互，不能相加。全部关闭在VMAF90/93的同质量体积反而约为原官方的0.9841/0.9726；在AOM/r4b共同域全部关闭仅+3.4171%，而r4b仍比该官方无滤波曲线大40.7481%。这些结果不足以将当前高画质差距主要归于缺滤波，也不代表本实现新增滤波可直接获得组消融收益。



官方未接工具完整probe已独立复核49artifact及1517源SHA，`official-extra-tools-independent.json` PASS。六Q的DIP/palette均未用；Q128/Q96完整Y系数出口分别5433/7165次，IST非零0，完整UV实际CCTX回调2095/3811次覆盖518400像素且类型全NONE。FSC flag面积0.2932%/0.7346%，真实非零FSC系数29/135次；面积不是bit份额。旧intra包装日志漏35/42次Y调用，完整ADST计数修正为1030/2269，原1021/2258仍保留为子域计数，不追改旧证据。palette/IST合法域细节以agent补充文件为准。下一隔离实验去掉DCT-J top2模式截断，比较13个合法basic Y模式×4现有primary TX；官方UV实际逆变换类型及frame量化偏移另作只读核查，尚无生产改动或原生体积结论。



本次接续：r4c的12/12联合公开/内部/fixed像素组和3/3真实malloc失败恢复已通过，closure SHA `e8734bdb69443857a7fc04e53a1b682d23a7a33b4950442764bb56b5c4b18efe`；原97/1及外置修正证据保留。r2 launcher已实际启动冻结r4c六点，仍为原439源及程序76810d3c...，不含下列后续改动。



下一候选r4d主线已合长条RDOQ、三proposal `/16,/4,1x`、MH完整链并集、thinrect及mixedthin几何/公开搜索。三proposal从相同scalar系数开始，保存被接受qcoeff/type重放；phase0旧ID不变，phase1/2用still域bit29，该位与互斥inter motion复用。MH两链在同MH启用语法下分别提出/不提出MH，均完成系数搜索后比较J，计数为聚合搜索工作。独立union fresh检测器三格式1,229,305检查及新增分配失败恢复通过，closure `8ab45f5c08e8408fb3fe04461d2fdb86c903d26f2da628a2cb293d87c1c13922`。这些为单项证据，r4d组合及原生体积尚待验证。



`thinrect` selector8追加uniform plan880/881（Y8×32/32×8）；`mixedthin` selector9再追加四象限各两种thin形状，共最多66次树试验。base7形状为16²/8²/16×8/8×16/32²/8×32/32×8，共2401编码；旧几何canonical回旧ID，两uniform回880/881，新稀疏计划882+code仅1774项合法（首887、末3281），882等别名拒绝。partial SB沿用plan0，新thin只DCT/MRL0，方向预测/UV4×16与16×4已补。mixedthin单项11,508固定流、18旧CLI及fresh SAN通过。API transform_selection0/1/2对应frame reduced_tx_set2/1/0，历史标签勘误不改变实际流。



新selectors8/9的wide使用global4+pre-long top2+pre-thin top2，去重最多8；pre-thin快照在index56入口，保留完整旧mixed8基础候选域。selectors0..7仍原wide最多4。不承诺跨语法开关或完整贪心链全局单调。局部top8三裁块仅一例J改善90，其余不变，尚无此项原生收益。



只读官方诊断：r4b-Q90小矩形面积12.44%对官方邻近质量点13.02%已接近，而现有合法primary域EOB>1的ADST占18.75%对75.17%；Q130为29.36%对80.33%。按形状/模式重加权后差距仍在，但残差、CDF及质量未受控，不能据此归因。下一隔离实验先测有限Y mode×TX联合并保旧完整路线，尚未改主线。



补充验证：r4d公开CLI45/45（36新组合+9旧路径）已PASS。完整CTest发现mixed32/intra_frame/inter_frame三个旧负例把新合法plan880当非法；MAIN只将这三个token改canonical-invalid882，原445冻结源/程序不动，原9个static libs重编的三项外置整测均PASS。原完整日志尚收尾，不能改写成一次全绿。



官方全部环路滤波关闭六Q已独立审计，`official-loopfilter-ablation-independent.json`：对同官方baseline VMAF PCHIP BD +4.292554%（linear +4.066486%），PSNR-Y/U/V为+8.952292/+11.696480/+14.004735%。只新编码五点，Q128及原baseline六点逐身份复用；18点含r4b并列均重算实际SSE/PSNR/VMAF，53artifact重核。关闭后官方重新搜索，不能归因单一滤波或视作本实现可得收益。官方六Q的restoration均NONE；CCSO与其它滤波组进一步消融已排队，不直接开发restoration。



Y×TX私有实验六局部配对全PASS：J变化[0,-428,-4757,-1007,0,-379]，字节变化[0,+1,+10,0,0,-1]；三预定原图64²裁块有两点J改善，只有六叶同入口joint-only收益合计1474，其余可能来自第二轮模式。存在新增完整路线J更差但正确选回旧路线的三棵树。完整反序、真实故障恢复和固定AVM像素均PASS，尚无该项整图收益；新默认off公开gate和fresh检测器在隔离准备，未进入r4d。



新主线已三方合入默认off的intra_joint API/CLI及root四路调度（同basic seed×MH/noMH×joint/nojoint），不嵌套两套快照；same-J保joint0，再按旧hash/id规则，关闭路径保持原调度。独立静态审查无阻断，helper与隔离r3逐字相同；新组合检测器/公开入口/原生曲线尚待验证，不把私有双malloc SAN套到root单次2*RdWork分配。三旧plan880负例已在主线改882，冻结r4d生产不变。



以下为此前阶段记录，较早的待运行状态由以上记录覆盖。



最新r4b六点及独立审计均PASS，`integrated-r4b-2D-summary.json`：直接对AOM **+12.9993%**（55.324948..95.230828），对r4a−0.8120%，对冻结r3−12.5782%。VMAF80/90/93体积比1.1767/1.2438/1.2554，尚未追平。`lambda-same-grid-summary.json`已补齐原λ程序Q110/90，其余四点复用原已审计证据、未重编码：同网格λ对AOM+20.8975%，r4a/r4b直接对它−5.9819%/−6.7522%，确认收益非网格差异造成。每个BD对比各用其共同质量域，不把各百分比相加。



原r4c Windows98项结束97PASS/1FAIL，唯一FAIL为旧API负例仍把新合法partition7当非法，main一行改8；外置fixture与原冻结9个static libs重编后完整单项PASS，`integrated-r4c-test-addendum.json`记录原97/1、链接首attempt缺shared DLL、static r2及生产SHA未变。原首次曲线launch因此NOT_STARTED保留，r2 launcher等待联合SAN全过后才启动同一冻结程序；公开CLI45/45与组合六组MSVC像素均PASS，联合SAN尚收尾。



r4a六点已完成，`integrated-r4a-2D-summary.json`独立审计PASS：直接对AOM VMAF BD **+14.6783%**（公共区间55.324948..95.230828），对冻结r3−11.8383%，对AIλ单项−5.8073%，对最初directional−33.4033%。VMAF80/90/93的AOM体积比分别1.2140/1.2622/1.2496，尚未追平。r4a的Q网格与λ单项原网格不同，已保留原四个重合点，仅补旧程序Q110/90以生成同六Q对照；在补齐前不把−5.8073%视为排除网格影响的净工具收益。r4b曲线继续运行。



大矩形RDOQ的四行跳过限制已在独立验证后合主线，冻结r4c仍保留原行为供配对测量。增量439源manifest `3ae53b05e68649b7ad40c253454fb2c4d064c3f2bf89ced98f9a67d54c4f89fe`；4000官方变换案例、80脉冲归一化、54条完整RD流、fresh O1检测器及确切RDOQ阶段恢复、关闭12组字节一致通过。六形状同入口真实J严格下降，只是机制证据，尚无原生六点体积收益。归一化是近似proposal代价，最终仍用完整SB重建/clip和实际CDF率验收。只删4行guard、同步一处测试eligibility，未混入proposal强度调整。



MHCCP与RDOQ的顺序另有隔离实验：同输入f0启用MH后最终J21745→22549，plan368实际进入完整链，回退并非漏搜。保留“不提出MH”和“提出MH”的两条完整RDOQ链、同MH启用语法/CDF下最终竞争，首例J降为21984（原MH0仍低239，不能跨语法开关保证单调）。`mh-chain-union-r1-source`只改私有调度和快照，尚未合主线、完整检测器待验；不能将此合成代价变化称为原生压缩收益。



mixed8与MHCCP已三方合入主线，公开入口为`--partition-select mixed8`和`--mhccp-search off|rd`，后者默认off。r4c冻结439文件，manifest `b59ae1c34656268a4857e320fd158934fd92476956fb01bc36fa7d6337b0aea0`，fresh MSVC严格构建通过，程序SHA `76810d3c77e0a358a99a50acd513d86531e0fd9d84a82011809fee4b76eed9d3`。98项Windows CTest、45组公开CLI及跨工具联合检测器检查正在运行；尚未启动r4c原生曲线，不套用单项PASS作为组合PASS。



mixed8：原0..254计划不变，内部计划改uint16；255..879表示四个32象限各选32²/16²/8²/16×8/8×16的625种组合，搜索只追加16次坐标候选，别名回旧编号。候选ID扩uint64，新计划置高32位，旧ID及其hash路径不变。独立4434强制流、1530旧流、68130条旧选择轨迹、旧CLI45组均通过；自然新plan368三格式各有真实J改善，O0/O1 ASan/UBSan高位ID与故障恢复均400259检查通过。这些是机制证据，不代表原生BD收益。合入mixed8后的主线6专项及三格式公开CLI/driver/fixed AVM字节与像素桥接通过。



MHCCP：基于fixed AVM的因果多假设跨分量预测，每平面三个Q16回归参数，共同UV方向三选一；仅使用当前重建Y及可用UV邻居，当前/未来UV不会参与拟合。新增五CDF保留原986模型前缀。15种UV形状17280例、3024完整流、120816因果poison案例及三格式深层故障恢复通过；r2 fresh producer对原流/SAN字节桥接通过，关闭功能15/15对冻结r4b字节一致。审查补正非对齐/加法溢出边界及4:1尺寸组，原失败记录保留。原r1增量构建漏重编header使用者，程序无效；r2全新构建才作为公开程序证据。主线合入同步适配uint16计划与uint64候选，不混用旧二进制。



λ+长条CfL+LUT六点已完整独立复核，对AOM+20.9075%，直接对λ单项回退0.1973%的VMAF BD；三平面PSNR BD则改善约0.41%/3.87%/2.73%。该阶段最好为λ单项+20.6497%；最新r4a已更新为+14.6783%，不能把长条CfL的色度收益累加到VMAF结论。旧r4a/r4b曲线、程序和源保持冻结，完成后独立审计。



04:53接续：新组合冻结r4b共432文件（manifest `a7ee461e93c0a595b1fc6cb3420782fbaa51276c4fc1ae569dbb3d3f139309cd`），程序SHA `c4826f54e427be349f692629e50d4de43e03655bce8768129865258993ae8ba6`。公开CLI的AIλ+wide+classic CfL全工具36组和旧多帧9组全部PASS。Windows原完整CTest为86 PASS/2 FAIL，均为旧intra/inter fixture将已合法的plan93当非法；只改两行负例93→255，独立两项复测PASS，原冻结源/原日志保留、生产app SHA不变，不能改写原88项为全部一次通过。`integrated-r4b-test-addendum.json`记录闭环。r4b保持与r4a同六Q网格和全部协议，仅追加classic CfL、小矩形ADST、大矩形MRL，第二次启动已真实运行，原被测试门槛阻止的启动记录保留。GCC ASan/UBSan全目标fresh严格构建通过，专项与组合检测器继续。



`integrated-r4b-static-audit.json`重新逐项核对432源SHA、默认λ函数与冻结r3逐字相同、AIλ函数与r4a逐字相同，以及旧979模型ID/nsyms和3916状态完整保留；新CfL仅追加7模型/28状态。下一队列为混合8×16/16×8几何的uint16计划迁移、兼容64位审计ID及官方MHCCP原语/接线，均在隔离副本推进，尚未算主线能力或压缩收益。



AI基础λ单项六点刚完成，独立重新核对所有程序/输入/工具/模型、实际解码与重建、三平面SSE/PSNR及VMAF JSON：相对冻结r3 VMAF BD-rate **−6.3430%**，直接对AOM **+20.6497%**，对最初directional−29.3181%，成为当前已测最好曲线。各自公共区间独立积分，不能连乘旧百分比。Q40为371153B/VMAF96.943681；其余五点与原任务全部保留，证据`lambda-ai-independent-summary.json`。原生单项只改全局λ，不含后续长条CfL、新几何、长角或四候选。2×速度未验收。



旧rect/top2的Q40原3600秒超时保留；同程序SHA的独立21600秒预算恢复完成后，配对12点均经固定AVM、实际解码像素及原始质量JSON独立复核。top2对同版单树VMAF BD-rate为−3.0454%，对AOM为 **+24.6374%**，目前是已完成整条曲线中的最好结果；对最初directional为−25.7050%。冻结r3直接对旧top2为+2.3347%，不能把增加工具域当成必然改进，也不能只由整帧曲线证明候选池挤占。



为此新增显式`--tree-refine wide`：保留全域最佳两个基本树，再加入plan87之前的旧域最佳两个，去重后最多四个，各自完整工具复评。原180秒CTest超时保留，同一test程序改用1200秒独立复测通过507886检查，并有三格式固定AVM像素核对；专项小图进一步实际触发四个不同候选，不只检查静态容量。`rd`原两候选入口保留。新增AI基础λ公开选项已通过510整数点官方C探针对照、45组CLI和6组默认/显式original旧流等价。



冻结r4a（428文件，程序SHA `8a313c851a1ae712c2cf54879041b76b4f92385fc3623aa79e4b59a7bfa20f6c`）使用mixedrect/full/长条角度/长条CfL/AIλ/wide，独立Q190/160/130/110/90/70六点已启动。新网格集中于AOM共同质量区间，原Q40任务及其全部点保留。各编码threads=1、质量任务并发，不作为正式速度结果。



小矩形8×16/16×8的三种ADST已合主线：72000官方前/逆变换case及26352固定AVM完整流、同组fresh SAN通过；另有自然选择ADST并配合RDOQ的三格式真实流。该贪心链新增TX后有两例最终J小幅回退，不能宣称对旧整链单调。大矩形MRL已以2322432组官方原语/SAN及62208条完整固定AVM流闭环；经典显式CfL以3120条规范流、24条同叶CfL+MRL流、3944模型状态、真实深层故障/反序/replay及fresh SAN闭环。两项已三方合主线，仅接feature hunks，旧λ保持原函数且AI选项默认关闭；新增`--cfl-search off|rd`。fresh MSVC /WX 167步编译通过，组合CTest及公开CLI继续。MRL前缀缓存仅收等价证据，尚未合入；其并发环境CPU差异不足以证明稳定提速。



证据根为外置测试目录的`top2-independent-audit.json`、`tree-wide-rerun/result.json`、`lambda-option-proof.json`、`next-tools-main-cli`、`lambda-option-compat`与`integrated-r4a-curve-launch.json`。以下分节保留各阶段历史状态，不能套用旧冻结版的整套验收给当前继续改动的主线。



## 官方AVM实测与归因



旁边已完成会话证据为 `<LOCAL_PRIVATE>/ffmpeg-native/tests/avm-aom-singleframe-20261024`：官方AVM9档对AOM0档，三张原生1080p/10bit420开发帧各六Q。2D VMAF BD-rate −22.37%，VMAF90/93匹配体积节省17.05%/16.39%。这是该开发帧的证据，不代表最慢AVM上限或50图验收。



本轮 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-r3-20261024/agent-avm/analysis.md` / `analysis.json` 保存14个新消融点与20个诊断流。隔离诊断decoder的输出均与固定主oracle逐像素一致；没有修改原AVM树或固定decoder。当前live AVM源码已更新，不能冒充被测二进制的构建提交；新原语依据旧v1源码并经固定decoder核对。



2D/Q128官方流：5427个独立Y叶、2084个独立UV叶、11个共树叶。Y矩形3181/5427（58.6%），8×8/8×16/16×8/16×16最常见；非零angle约55.2%，非零MRL约33.8%，非DCT Y TX约19.5%。独立UV树1560块选CfL类，其中classic272、implicit830、MHCCP458。使用率不等同收益。



- 禁CfL/MHCCP/CCTX组：16242B/VMAF88.6415→17200B/88.6029，只是单QP诊断。

- 单独禁rect：28293B/79.6704，但树退化到507个Y叶、455个为64×64；该开关连H/V细分一起阻断，不能把退化全归因矩形形状。

- 单独禁SDP、扩展分区、完整DCT-only的局部差别较小，不能据单点推断整曲线贡献。

- 本次9档流TCQ/parity均未实际启用，关闭开关逐字节不变；DIP/palette也未选中。



## 本轮主线子集



所有新功能默认关闭，旧工具与旧流保留。



- `--partition-select mixed`：保留fine候选，新增完整8×8树及四个32×32象限各选32/16/8的有界坐标搜索。byte计划0..3是统一深度，4..84是四个三进制象限深度；partial SB仍保留原树。`rect` 在此基础上追加 plan85/86：完整 SB 统一16×8或8×16，支持对应UV矩形、真实coeff/scan与partition上下文；仍不是任意混合二叉树。

- `--transform-select primary`：亮度4/8/16方形DCT_DCT与固定v1 tuned ADST_ADST二元选择；UV与32/64仍DCT。EOB≤1隐式DCT，不能写ADST系数冒充DCT。旧/新AVM的ADST矩阵存在版本差异。

- `--intra-select cfl`：13个Y方向加UV隐式单alpha CfL；经典显式alpha和MHCCP尚未接。每叶先完成Y逆变换、clip及重建，再生成CfL UV预测，不能读取候选前的Y。

- `--coeff-opt rdoq`：有界level邻点/EOB候选，局部变换域代价与真实系数语法筛选，之后逐块完整SB重建与committed CDF真实J验收；保留原候选及完整回滚。温和proposal用全局lambda的1/16，最终SB验收lambda不变。不是完整trellis/TCQ。ADST候选禁止隐式DCT的EOB≤1；保存qcoeff与实际TXtype重放。

- `--angle-search rd`：补48个非零Y角度，原13模式ID保留；48方向亮度因果SAD取前4，与原方向最多6个偏移合并，再做真实完整SB R+D。UV V/H同基模式需继承Y偏移，UV使用规范2tap、Y使用4tap IDIF。MRLS/IBP/edge filter/wide-angle仍未接。

- 前缀缓存只减少refine_basic已固定早期叶的重复重建；后缀重建与完整tile/CDF率试验保留，候选顺序不变。93,551条候选/commit D/R/J一致，原生2D Q70完整流/recon一致，forward调用少39.37%、该次CPU少40.57%。这不是正式2×速度验收。



公共配置尾部新增transform_selection和angle_search；调用者须重新编译。正式发布ABI/版本未升级。



## 已完成的六点曲线



同冻结输入、模型、主oracle、raw OBU口径。负号表示省体积；各对照使用各自合法共同画质区间，不将不同单项百分比相加。



| 候选 | 样本 | 相对上一版directional | 相对AOM0档 |

|---|---|---:|---:|

| 温和RDOQ、fine | 2D | −6.604% | +60.134% |

| 温和RDOQ、fine | 3D | −9.232% | +40.810% |

| 温和RDOQ、fine | Real | −9.833% | +22.463% |

| 二元primary TX、fine | 2D | −0.627% | +69.722% |

| implicit CfL、fine | 2D | −2.702% | +65.821% |

| CfL+primary+RDOQ、fine | 2D | −8.274% | +55.532% |

| implicit CfL、fine | 3D | −3.128% | +48.780% |

| CfL+primary+RDOQ、fine | 3D | −11.839% | +34.694% |

| implicit CfL、fine | Real | −0.900% | +36.079% |

| CfL+primary+RDOQ、fine | Real | −9.765% | +21.512% |

| directional、mixed | 2D | −3.998% | +63.516% |

| 非零角度、fine | 2D | −12.984% | +47.857% |

| CfL+primary+RDOQ、mixed，无angle/rect | 2D | −10.606% | +51.560% |

| CfL+primary+RDOQ、rect + 方形angle | 2D | −22.725% | +29.853% |

| CfL+primary+RDOQ、rect + 方形/小矩形angle | 2D | −22.960% | +28.992% |

| CfL+full+RDOQ、longrect + angle/MRL/top2，冻结r3 | 2D | −23.986% | +28.045% |

| CfL+full+RDOQ、rect + angle/MRL/top2，旧配对完成版 | 2D | −25.705% | +24.637% |

| 冻结r3仅换AI基础λ | 2D | −29.318% | +20.650% |

| AI基础λ+长条CfL+等价LUT | 2D | −29.201% | +20.907% |

| r4a：AIλ+wide+mixedrect+长条方向/CfL | 2D | −33.403% | +14.678% |

| r4b：再加classic CfL/小矩形ADST/长条MRL | 2D | -33.923% | +12.999% |



RDOQ三图线性插值复核约−6.54/−9.18/−9.76%；部分U/V PSNR曲线有小幅退化，完整三平面数据均保留。CfL的2D U/V PSNR BD-rate分别−19.06%/−15.37%，不能只看VMAF或把中高分四点约6%的变化替代整曲线2.7%。工具组合2D线性复核相对旧版−8.375%、相对AOM+59.173%。



AIλ加长条CfL/LUT的六点已完整独立核验，证据`lambda-cfl-lut-independent-summary.json`：直接对λ单项+0.1973%，对冻结r3−6.1488%。LUT另有严格全流等价证明，因此这组不支持长条CfL的正BD收益，仍保留原始候选与全部点。r4a四候选组合与r4b追加工具组合使用独立冻结程序继续测试，尚未收齐，不根据先出的单QP推断整曲线。



fine + CfL/primary/RDOQ/角度的2D曲线相对相同无角度工具组合省 **13.0248%**，对AOM仍 **+34.5080%**；PSNR-Y/U/V 对同组合分别−9.7315%/−9.4996%/−9.1995%。这是同配置角度边际，不将单项率累加。程序68646e...早于UV64修补，但本次420所有UV≤32，不触发该边界。



mixed原六Q已完整：Q40取早期串行点，Q70..190取并行队列逐点结果，均核对固定AVM。额外QP1在3600秒超时，原queue整体FAIL及日志保留，不能称七点全部通过。mixed工具组合与第一轮rect+方形angle的六点均已收齐，见表；新的小矩形angle及MRLS组合继续运行。



## 正确性与证据



外置根：`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-r3-20261024`。286文件baseline-source与上一轮完整SHA逐项一致，baseline程序为8c7906e08244e61b32644b93d62c4c98b725097e546f9ee1468b270c996e1c89；原有冻结材料只读。



- mixed强制6552流、primary TX3276流、implicit CfL1728流均与固定AVM逐像素一致，分别来自各阶段冻结程序，不将其相加冒称同一最终集成矩阵。

- primary前/逆变换14,400case/1,612,800sample一致；angle Y/U/V原语1,166,400case/1,272,775,680sample一致。angle强制30,744流已全部逐像素复核；验证脚本最初Y数量断言仍为13而FAIL，修为61后对原decoded/recon重核，原FAIL日志保留。

- 967个模型×4bank全部3868状态经固定AVM头文件独立C提取核对，原960个模型的四bank状态完全不变。

- RDOQ单项52/52、TX+mixed+RDOQ组合53/53与90/90 API/AVM/旧流兼容已通过；主线整合后的扩大API矩阵、MSVC/GCC/Clang/SAN、跨平台完整流仍待最终收尾。

- 首个整合单体RD测试超过600秒；已按mixed/primary/RDOQ/CfL/angle域拆独立进程，不删断言。两个旧API非法值已成为新合法值，负例上限作对应更新，单独复测2/2通过；原失败日志保留。

- UV64反例在128²/10b444跨SB时固定AVM成功解码但重建首差byte49278；主线已按规范限制其TR/BL。修后1708流逐像素PASS。低层原语相等不能代替上层可用性检查，before失败流保留。

- rect core 9408流、rect CfL 2304流（含原1728回归）逐像素PASS，各阶段独立冻结；最终重新生成stream/recon SHA复现原解码证据。

- 主线 rect-r1 的 MSVC **63/63 PASS**；最终 **45/45 CLI PASS**：36组3格式/2尺寸/3QP/CDF开关全工具组合及9组旧多帧配置。旧流和重建对8c7906基线逐字节一致。tools-r1扩大API矩阵306/306 PASS。Linux严格构建/检测器与第二轮仍在运行，不套用旧轮结果。

- 当前可复现angle-r1源295文件及程序冻结于同根：`angle-r1-source-manifest.json`，程序SHA68646e5284ddd41fbfbf864d7169c2a0311e27307b5a76c683ab16e0cd02a489。后续改动另建快照，不能套用此前程序SHA。



## 接续队列



第一轮rect-r1源300文件与程序c69ece9eec8a0b57c305a0e6d8472e32ce9f2f5c27a3ec821e0dce5b6248efb0已冻结，三路独立静态接合审查通过。继续收完整曲线与跨平台/检测器证据。



第二轮队列分别核对矩形角度预测、完整合法Y变换子集、多参考线MRLS，以及角度/变换/RDOQ前缀缓存等价提速。隔离实验尚不算主线已支持；矩形方向存在隐式广角映射，直接放宽square限制已在fixed decoder失败，失败材料保留；修正后的独立原语差分3110400case/323481600sample通过，完整帧矩阵继续。MRLS不能仅外移边线，涉及角度修正、SB顶边和邻居CDF；完整变换不能塞进现有binary primary语法。所有失败日志、程序和旧baseline保留。holdout仍未编码评分，不推进帧间，不用并发质量任务耗时冒充正式速度结果。





## 第二轮整合与第三轮队列（持续更新）



第二轮已整合到主线，源 `integrated-r2-source` 307文件，程序 `integrated-r2-av2.exe` SHA256 `33a146077be04f8dc16dea97bc1b0a81ba6a354133d72bc6708895e15eccf16b`。随后r2a仅改变SAN强制矩阵的测试超时元数据，生产逐SHA相同。最终45组CLI PASS：36组开启CfL/rect/full/RDOQ/angle/MRLS和9组旧多帧基线对照，全固定AVM逐像素，旧流逐字节相同。Windows首次68项67通过，旧负例仍将新增合法transform=2当非法，已改为3，该项独立复测PASS；原FAIL保留。后续Linux构建暂存源同样只覆盖此fixture，并记录before/after SHA，冻结快照不改。



- 小矩形角度：原语3110400case/323481600sample、整帧30744流全部PASS。两轮真实失败先后暴露隐式wide-angle和TR按TX宽/BL按TX高的上限；修正后按同case列表完整重发并批量固定AVM逐段比像素。before两轮原流/日志保留。

- `full`变换：43200原语case，19656强制流逐进程与批量两种方式均PASS；官方七符号CDF及4×4 Paeth映射正确，旧off/primary字节兼容。2D6Q相对原tools-angle VMAF BD−0.1848%，PSNR-Y/U/V−0.8498/−1.0143/−0.4020%；对AOM+34.5097%，不同合法区间下几乎持平，不能拼接单项率。

- 方形MRLS：三个非零index及single/multi六候选，16128强制流+6实际RD获选流逐像素PASS；最终重发stream/recon SHA复现全部16128及关闭MRL的旧rect9408，后者全字节兼容。原生六Q已全通过、12个开关点独立复算：VMAF BD−0.003380%（基本持平），PSNR-Y/U/V−2.061606/−2.410304/−2.085361%。基于rect-r1+方形angle+primary/CfL/RDOQ的私有程序，未包含full/top2/小矩形angle或MRL；不得外推r3。旧axis UB已由修后全部stream/recon SHA回放及fresh SAN补证像素等价，不冒称修后正式速度。官方非零MRL有68.47%位于矩形，不能把33.8%总体使用率当本子集收益。

- 976个CDF×4bank全部3904行经fresh C extractor直接include固定AVM头核对PASS，旧967模型×4状态完全不变。见 `r2-models/check.json`。

- 第二次前缀优化：223650条完整候选/commit轨迹相同，13组配对stream/recon完全相同，26次固定AVM像素PASS。原生fine/Q70 forward少13.33%，该次CPU少7.99%；单元rect域约少1.36%，不扩大为正式速度结果。



批量AVM验证保留每个独立still序列头，先证明跨CDF/mixed的拼接流与逐流decode/recon相等，再逐帧按offset/length核对完整像素和SHA。每批输入/输出SHA、命令、case范围都保存；不能仅比较总长。可复用入口 `tools/verify_intra_batch.py`。



第一轮主线GCC/Clang各62/62及各45组跨平台全流/重建SHA均PASS。旧Debug SAN的原角度强制矩阵180秒超时，停止其尚未结束的旧队列，原日志/STOPPED.json保留；新整合ASan/UBSan以显式1800秒强制矩阵预算和3600秒RD预算继续，未将timeout称为内存错误或通过。正式2×速度与I-GATE仍未执行。



第三轮聚焦两项已测出的缺口：当前Q130的4965个Y叶中小矩形最终为0，64/32方形仍覆盖71.11%面积；官方同图邻近QP的矩形覆盖约66.8%，包含大量长条域。几何选择又先于angle/MRLS/TX/RDOQ，因此后段工具无法挽回被淘汰的树。分路补长条矩形的transform/coeff/scan/语法完整闭环、保留两个树候选的完整工具重排，以及8×16/16×8 MRLS。它们尚未合入，不保证完整帧BD必降，继续以固定AVM与六点曲线验证。





第一轮rect+方形angle最终六点已收齐（Q40编码墙时3258.523秒，低于旧3600秒上限）。`rect-angle-2D-r1-summary.json`独立核对全部program/model/input SHA、stream/recon/fixeddecode、逐点SSE/PSNR及VMAF JSON；另外Q40/Q100重新运行VMAF分数完全相同。相对fine tools-angle再省3.1042%，对上轮directional省22.7245%，对AOM仍+29.8528%。三个对照各自区间独立，百分比不连乘。VMAF80/90/93的PCHIP匹配体积比分别1.3351/1.3571/1.3449；对AOM的PSNR-Y/U/V BD仍+58.01/+64.37/+67.59%，不能只用VMAF表示所有平面同幅改善。此配置尚无小矩形方向、MRLS、full TX或longrect/top2。



后续`intra_curve.py`显式记录编码deadline（默认10800秒），并随out复制自身和helper供复现；新旧attempt各自冻结，不追改已经启动的协议。holdout仍未编码评分。



## 第三轮整合（测试运行中）



小矩形方向搜索六Q完整通过，`rect-angular-2D-r1-summary.json`独立复核程序/输入/模型、逐点流与重建SHA、SSE/PSNR、原VMAF JSON。相对前述rect+方形angle边际VMAF BD−0.30946%，对AOM+28.99168%；VMAF90/93匹配体积比1.33316/1.32644。不是MRL或长条收益。



第二轮r2a检测器首次67项中64通过，三项报同一真实UB：MRL精确90°/180°预读了随后不用的导数表索引90。修为严格90<angle<180后axis原语65331checks通过ASan/UBSan及MSVC；原失败日志与修前反例保留。三项修后CTest复测仍在运行，不将逐像素一致当作内存安全通过。



第三轮已合入：内部selector5及公开`--partition-select longrect`六类长条；公开`--tree-refine off|rd`映射内部0/2；8×16/16×8 MRL；上述axis修复。长条基本预测+DCT+scalar的边界写入合同。full/reduced0长条也必须写LONG_SIDE符号，已将该分支从reduced==1改为<=1；独立修前18流中12个长边流失败、6个DCTONLY流通过，专项修后组合验证继续。



新源`integrated-r3-source`420文件，manifest SHA `506b3b3e0b191950f510906f32a6fe2ee970f0f4db98dfad1757d3533c675cae`；程序`integrated-r3-av2.exe` SHA `d7aa63ade4201c296dcaa37b96d874c5548020324f62f2259bea15cf10816c3d`。MSVC全新严格构建成功，74项CTest、45组最终CLI及GCC/Clang/ASan+UBSan全新构建/测试运行中。统一生成979个CDF模型（3916状态行），独立C核对排队中。新增模型后的旧增量obj不复用。



新组合原生2D六Q已启动`integrated-r3-2D-curves`，profile=`longrect-full`，包含CfL/长条/full/RDOQ/angle/MRL/top2；jobs4、每进程threads1，编码deadline21600秒从启动明确记录，防止高搜索成本点被旧3600秒截断。各单项和组合曲线独立冻结，不累加收益、不追改旧结果；正式速度与I-GATE仍NOT_RUN。



该冻结r3六Q现已全部PASS。`audit-native-six.py`重新核对程序/输入/模型/helper、全部stream/recon/原官方decoded及逐平面SSE/PSNR、VMAF单帧JSON；结果`integrated-r3-2D-summary.json`。相对上一rect-angular组合VMAF BD−1.12447%，对旧directional−23.98583%，对AOM仍+28.04533%；匹配VMAF90/93的体积比1.31795/1.31620。PSNR-Y/U/V对上一组合分别−5.2376%/−1.6540%/−2.8678%，对AOM仍+49.5313%/+57.9973%/+60.0737%。Q40为271431字节/V96.770708，编码墙时3725.871秒、CPU3266.594秒；有并发争用及重建导出，不是正式速度结果。它尚不包含后续长条CfL、mixedrect、大矩形角度、显式CfL或归一化λ，不将新增几何使用率当同等体积收益。



第三轮Windows严格CTest最终74/74 PASS（632.97秒），45组CLI全部PASS；新增长条full/reduced0独立矩阵864/864固定AVM逐像素PASS，测试源为完整r3逐SHA副本，仅fixture强制tx2，不套用只覆盖tx0/1的主fixture范围。独立CDF直接include固定AVM头的C提取及另一个C程序读取生产数组，3916行全部精确，旧r2的3904行ID与状态逐项保留；3个长条模型按语义追加976..978。



axis修后原失败三项CTest最终3/3 PASS（883.22秒），与此前64通过项共同闭合该r2a检测器队列；r3自身检测器另跑。r3 Clang+SAN首次因本机缺`libclang_rt.asan_static.a`/`libclang_rt.asan.a`链接失败，未运行codec，不记为源码缺陷或通过。日志和FAIL保留，新的GCC ASan/UBSan全新构建及全CTest继续；普通Clang仍独立测试。



普通Clang全73/73 PASS（1081.51秒）。GCC首轮72/73 PASS，`test_intra_rect_angles`180秒超时；相同测试程序SHA在单项显式900秒预算复测，182.125秒完成153774checks/0fail，原CTest TIMEOUT不改写。主线该测试预算改600秒，SAN仍1800。GCC/Clang/GCC ASan+UBSan各45组最终跨平台完整stream/recon SHA全部PASS，见`r3-cross-summary.json`及各平台result。GCC检测器全CTest最终73/73 PASS（3929.29秒）；`integrated-r3-linux-sangcc/result.json`另核对Linux暂存420个源文件与冻结manifest完全一致，附程序与全部日志SHA。此完整检测器结果只归属冻结r3，后续LUT/CfL另有专项证据。



## 单位归因与等价提速队列



`agent-coeff/RD-UNITS-HANDOFF.md`包含直接编译固定AVM真实rd.c函数/RDCOST宏及冻结SVT函数的510个8/10bit全CQP点。420的当前J归一到总SSE后，每bit罚项是lambda_q8/1024；AVM基础公式是rdmult/2048。AI分支下倍率约12.6984..12.8372；10bit QP130两者分别676与52.8125。literal、静态CDF及3916个adaptive模型16332符号的直接C探针确认R为Q12熵bit估计，无额外隐藏倍率；不是finalized range-coder字节数。官方旧运行态未插桩，报告分别列AI/LD/general公式；444的4Y+U+V权重也不能套单一420倍率。



这说明当前搜索更排斥增加比特的候选，但不证明全部粗树面积或BD差距都由lambda造成。隔离程序`7fab880558723a8b23901df0cb77c5db6a9aa5ee4967f55cdf77389789cbf838`只改lambda函数，按AVM AI精确整数顺序折算lambda_q8=(rdmult+1)/2，qstep/候选/444权重/RDOQ局部规则不动；510点数值与三格式partial全工具CLI像素通过。`agent-coeff/lambda-ai-2D-curves-r1`与root r3六Q协议相同，完整BD尚未得出，未合主线或宣布收益。



主线另合入严格等价的p15→Q12静态查表：65538字节只读表替换每符号12轮整数对数，生成器`tools/gen_rate_cost_table.py`。MSVC及GCC ASan/UBSan对32768全部合法输入、边界错误与NULL共32771项逐值等于原C函数，0fail；45组完整流/重建SHA与冻结r3一致，因流完全相同复用其fixed-AVM证据，未冒称重跑45次decoder。系数路独立GCC补到32775项零fail并复核全部流/重建SHA；主线新增全域精确回归后的fresh严格构建和2/2测试通过，证据`rate-lut-final.json`。函数微基准仅局部循环，Windows C clock为墙时，不能当整帧或正式2×速度。



长条CfL后续已合主线两生产文件与一个fixture：按宽/高独立处理64维均值，4:1仅长边8样本拟合，移除RD对新长条UV14的禁令。`agent-avm/longrect-cfl-final-result.json`冻结421源、10368完整帧固定AVM像素一致、2304旧流字节相同、5904直接官方库alpha/像素（含SAN）、18跨SB整帧SAN和15公开CLI。三个人工RD样本虽多28候选，最后流仍不变，不冒称自然压缩收益。主线CfL+LUT组合严格构建成功，新fixture 1/1 PASS（34.70秒，`longcfl-main-ctest.log`）；λ仍隔离。联合“归一化λ+长条CfL+等价LUT”六点按同一冻结协议另排队，和λ单项配对。



原生r3 Q130诊断由两条decoder分别复核像素：六种长条中四种自然获选，共71个full SB、210个Y叶，覆盖14.0247%画面；64²/32²覆盖从r2top2的69.9753%降至57.1358%。小矩形中96个叶非零MRL、72个多线，补足早期人工RD样本未命中矩形的验证范围。最终流看不到被拒seed，不能据此证明top2挤占或归因完整BD。冻结诊断decoder对frame reduced0的UV TX probe未套chroma_dctonly规则，原UV TX hist标为不可用，实际解码像素和Y TX统计不受影响；不修改冻结decoder。



## 混合矩形与下一轮缺口



`mixedrect`（公开partition selector6）已合主线，只取`agent-coeff/mixed32-core.patch`的五个几何/search文件及测试hunk；该分支基于归一化λ的私有423源，但三方合并保留主线原λ函数，不整文件覆盖。两个各81计划的32象限族占用byte93..254，旧0..92语义不变、255非法；93/174全零别名不进入搜索。旧24次树候选之外每族独立从uniform32/16真实J起步、四象限各试V/H，共最多40次，保留各族局部进展和原候选/top2池。不增加CDF、动态树或独立TX树。



隔离功能矩阵1332新流全部固定AVM像素PASS，最终程序重发stream/recon SHA一致；旧0..92三格式/CDF的558流及12别名逐字节一致，9条真实RD流也通过固定AVM。三格式full/partial真实J、tile replay、反序、容量及forward故障共833904checks零fail，见`agent-coeff/MIXED32-HANDOFF.md`和`mixed32-summary.json`。人工边缘图可自然选择plan100（V/H/32²/32²），全工具J14550低于旧selector5的14834；不冒称原生BD收益。



公开入口首轮36组新配置全部初始化失败：root接合时漏改parameter setter的旧上限5，底层4项专项仍通过。修为6并新增6可接受/7拒绝的公开回归后，冻结`mixed32-main-r2-av2.exe` SHA `f7641035ae848b510554304bab61800901e49792ecc8a10671eb965dd4554e1b`，45/45 CLI全部PASS（36组新配置固定AVM逐像素、9组旧多帧byteexact）；原FAIL在`mixed32-main-cli`保留，新attempt为`mixed32-main-cli-r2`。新几何尚无完整原生曲线，不套用前述累计压缩率。



大矩形方向预测另在隔离队列通过官方完整边界构造793152case/627803136pixel，以及26352条完整帧固定AVM验证；旧off兼容、公开真实RD和fresh SAN继续，未合主线。经典显式CfL原语`classic-cfl-prototype/result.json`通过100368case/48685824pixel及同范围GCC ASan/UBSan，包含q3全部−8..8和可用17类UV TX、三格式、边界；5904旧implicit原语回归通过。它目前只是原语，完整index0/sign/alpha语法与有界RD尚在隔离实现；主线仍仅implicit CfL，MHCCP未接入。

</details>


<details>
<summary>003 · M6-intra-first-20261024.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-intra-first-20261024.md`  
原文件 SHA-256：`e035707fa9b5e5dd420b998d7a3c8e31303a7640af5e570a3c65e5c8b1baea4b`

# M6-I 帧内首轮开发与实测（2026-10-24）

## 结论

**I-GATE 未运行、体积目标尚未达到，正式仍为 0.0.5-m5。** 已按用户指定的 `04-M6-intra-only-first.md` 切换到单图、单线程、体积优先；原帧间、内部多线程、preset/CRF 路线不列入当前开发。先达到同质量 AOM 体积约 −15%，再把编码耗时压到约 2×。

这轮完成了可运行的 single-picture 入口、基础七种 Y/UV 预测及单图自适应 CDF/RD 事务子集。没有完成方向角/angle delta/wide-angle、全部分区/TX、RDOQ、高级预测或滤波，不把这些子项的正确性通过称为 I1/I2/I3 全部完成。

证据根目录：`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-20261024`。现行合同：`../m6-intra-contract.md`。

## 已落地的编码闭环

1. 公共配置尾部新增 `still_picture`，参数 `SVT_AV2_PARAM_STILL_PICTURE`，CLI 为 `--still-picture`。使用生产系数链写真正的 `single_picture_header_flag=1` 和 `CLOSED_LOOP_KEY`，支持现有 8bit420、10bit420/444；API 拒绝第二张图片，CLI 只读一帧并拒绝显式多帧/帧间组合。
2. 修复旧 single-picture 序列头的色度量化偏移：零偏移必须写五位编码值 23，而非 0（固定 AVM 对后者解释为 −23）。此前零残差灰图会掩盖此错误；新非零残差样例与 AVM 重建已一致。对应旧单图字节向量随规范修正，旧冻结文件未覆盖；生产多帧默认关闭路径保持指纹。
3. 新增默认关闭的 `--intra-select basic`：DC/V/H + SMOOTH/SMOOTH_V/SMOOTH_H/PAETH，Y 与共用的 U/V 模式分别选择。同步实现预测、joint-mode 重排、方向/非方向 CDF 上下文、UV mode list、量化/逆变换/clip 后真实失真和最终语法。UV 显式置 `enable_chroma_dctonly`，避免非 DC 色度隐式选择尚未实现的变换。
4. AV2 的实际 highbd smooth 原语用于两种 bit depth；它使用额外 TR/BL 端点及整数有符号舍入，不能沿用旧 8-bit helper 的二次权重。端点仅从已解码 SB/前序叶读取，不能读尚未重建的源/缓存。
5. 原 DC/V/H 树搜索结果作为入口，在该树上做一轮逐叶 Y/UV 坐标搜索。每个候选从同一个 SB 入口重建完整叶序列，后块读取本候选的前块重建，再通过真实 tile 符号顺序计算 R/D/J。不是只比较 Y-SAD 或非零系数数目，也不是所有模式/树/TX 的全局最优搜索。
6. `--entropy-adapt on` 可与单图 RD 联用：committed/scratch bank 深拷贝；失败/未提交候选丢弃 scratch；赢家重放校验 D/R/重建/系数后再提交。自适应成本使用既有 C1 的 AVM 位置调整概率，覆盖相等 raw CDF 边界。多帧 RD+CDF 及系数/运动优化组合继续拒绝。

使用方式（实验功能，所有开关默认仍关闭）：

```powershell
build/i3-20261024/bin/SvtAv2EncApp.exe --input input.y4m --output image.obu `
  --still-picture --threads 1 --intra-select basic --partition-select adaptive `
  --rd-mode pruned --entropy-adapt on --qp 100
```

新增公共字段要求调用方重编。没有将 AVM 链入生产库，独立 oracle 只用于测试。

## 主基准与三张原生开发图

实际 libaom v3.15.1 使用 `--allintra`，该参数直接选择 `AOM_USAGE_ALL_INTRA`；文档中的 `--ai` 不存在。本地源码及官方参数定义已核对。保持 `--threads=1 --cpu-used=0 --end-usage=q --limit=1 --passes=1 --tile-columns=0 --tile-rows=0 --obu`，没有换快档或 AVIF。源码 commit `44d0a57786f432d933ff64b653347c66f4d0fa1d` 已在隔离目录重新构建，构建参数和 CMakeCache 保留；18 个 AOM 质量流及解码像素与原基线逐字节复现。

本轮实际开发曲线只用既有 2D/3D/Real 的固定第 48 帧，每张 1920×1080、10bit420，不缩放。冻结相同 Y4M 输入、VMAF v0.6.1 模型、三平面 SSE/PSNR、raw OBU 全字节和全部原始点。AOM Q 为 16/24/32/40/48/56；AV2 QP 为 40/70/100/130/160/190。按真实解码质量计算 PCHIP 对数码率 BD-rate，数值 Q/QP 不直接对齐。

| 开发帧 | 基础七模式相对旧 RD | CDF/RD 相对七模式 | 最终组合相对旧 RD | 最终组合相对 AOM |
|---|---:|---:|---:|---:|
| 2D | −6.29% | −10.91% | −16.50% | **+103.35%** |
| 3D | −3.18% | −12.05% | −14.87% | **+69.25%** |
| Real | −5.15% | −9.79% | −14.38% | **+55.82%** |

每个表格单元独立使用各对曲线的共同质量区间，不把分阶段百分比相乘。结果证明这两项在这些开发帧上的有限压缩收益，也证明与 AOM 仍有显著差距。没有把“对自己省约 15%”写成“比 AOM 省 15%”。三张开发帧不是 holdout，也不能外推到整段视频或其它题材。

`dev-curves-final` 保存 AOM/旧 RD/基础七模式的 54 点，`dev-curves-i3` 保存 CDF/RD 和额外 Tritium 对照的 36 点；每点有命令、stdout/stderr、raw OBU、内部/AVM 重建 SHA、VMAF JSON、三平面 SSE/PSNR 和进程 CPU/墙时。`development-summary.json` 可从这些点复算。质量运行含重建导出且部分与构建/检查并发，**不用于 2× 速度验收**；五次新进程同质量独立计时尚未执行。

首批辅助二进制的完整日志确认它是 **SVT-AV1-Tritium v4.2.0**，不能把它冒称官方 SVT-AV1。该额外对照保留但不代替合同辅助基准。已从官方上游纯 HEAD `24f54f73303fbe6fbc1dcb7a21755614e0050a18` 隔离提取并构建 **SVT-AV1 v4.2.0**，不带工作区已有补丁；固定 preset4/tune3/avif1/pred-struct0/lp1、单轮六点曲线共 18 点全部通过。最终组合相对官方辅助基准的 VMAF BD-rate 为 2D **+68.37%**、3D **+42.94%**、Real **+43.20%**，仍明显落后。完整身份和构建参数见 `svt-official-manifest.json`，原始结果在 `svt-official-curves`；`development-summary.json` 的 SVT-AV1 行现在只引用官方版本。辅助 lp1 不用作主速度门槛，不影响上表 AOM 主对照。

## 正确性与兼容性

- Windows MSVC `/W4 /WX`：52/52 CTest。GCC strict：51/51；Clang strict：51/51。Linux 少一项 Windows oracle smoke，另以完整流/像素交叉验证补证。
- GCC ASan/UBSan/LSan 全量 51/51；最终与本轮相关的五项重新运行通过。包含候选反序、失败后恢复、赢家重放、同语法整帧/逐 SB rate 一致和完整/partial SB。
- 直接编译只读 fixed AVM v1.0.0 的 `intrapred.c` 做原语差分：19 种方形/矩形、8/10 bit、16 组边界/端点条件、20 组固定随机参考，**48,640 组、34,775,040 个样本零差异**。测试程序为 `tests/conformance/i1_avm_prediction.c`。
- **882 条**强制 Y/UV 码流：七乘七组合、三格式、三几何、固定/混合邻居模式，fixed AVM 逐可见样本一致；`forced-modes-final/result.json`。混合模式扩大测试修复了“左侧非 DC 非方向、上方方向模式”的排序缺陷。
- **90 组** API/CLI 矩阵：72 组 single-picture（含 QP1/100/255、三格式、8/24/128/partial 尺寸、静态/自适应 CDF）全部 AVM 一致，18 组旧多帧关闭路径与修改前冻结程序的流/重建相同；`api-matrix/result.json`。
- 最终 MSVC/GCC/Clang 各重放 72 组，共 **216 条完整流及重建指纹一致**；`cross-platform-final/result.json`。API 还检查 17 次小输出缓冲重试，以及 1000 个独立单图 handle 的确定性，覆盖 1/2/3/17/1000 检查点。

验证中发现的失败保留在 `forced-modes`、`forced-modes-v2`、`dev-curves` 等目录，不并入成功结果。它们分别暴露了 AV2 highbd smooth、UV 隐式变换和混合邻居排序问题。当前最终矩阵已全部通过，不能删除旧失败点或把中途的 RUNNING 当完成。

## 数据集与尚未通过的门槛

`images-i0` 冻结 24 张 Kodak 与 26 张带 commit 的 Urban100 镜像图片，共 50 个独立来源图，按身份 hash 事先划分 **30 calibration / 20 holdout**。保存原 PNG、原/转换后 SHA、尺寸、BT.709 limited-range 420 转换命令、Y4M header 和像素 hash；所有编码器将读同一份转换像素。holdout 没有编码评分，不用于模式、lambda 或搜索参数调整。

这些图片主要覆盖摄影与建筑细线，尺寸并非全部 1080p；还缺足够的屏幕/文字、高 ISO、原生 HDR 等题材。不能称最终 I0 数据集完整，也不能用它们替代原生 1080p 的速度测试。筛查脚本拒绝直接消费 holdout manifest。

下一项应先扩大可合法选择的分区/TX（当前完整 SB 主要还是 64/32 的有限树）并与模式和系数 RD 联动，逐种强制码流验收，再测真实收益。随后继续系数优化/帧内专用工具/滤波的单因素实验。体积明显达标之前不把主要工作转去提速；最终仍需全部 holdout、分类稳健性、原始曲线与五次独立计时通过后才能宣布 I-GATE PASS。

</details>


<details>
<summary>004 · M6-intra-partition-directional-20261024.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-intra-partition-directional-20261024.md`  
原文件 SHA-256：`3249656b3322d1a921ff88557bc6031fe2a2d3e1416b108408452ea0f301ebf1`

# M6-I 第二轮：16×16 分区与斜向亮度预测（2026-10-24）



## 状态与目标



目标仍为同质量 raw OBU 体积 ≤ AOM-AV1 的85%，随后将单线程耗时比压到2.0。本轮补齐两项实际缺口；不以当前有限实现的结果判断 AV2 标准的上限。**I-GATE 为 NOT_RUN，正式版本仍0.0.5-m5，新入口默认关闭。**



本轮证据：`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-r2-20261024`。原始任务书 `../../../M5-M6-AI-TASKS/04-M6-intra-only-first.md`，现行合同 `../m6-intra-contract.md`，上一轮 `M6-intra-first-20261024.md`。



## 实现范围



- `--partition-select fine`：完整64×64超块可选择64、四个32、十六个16的合法二叉树。每棵树完成模式优化后再比较，保留原有粗分区候选；partial超块使用既有合法边界树。分区标识0/1/2对应64/32/16，重建、tile writer、rate trial和最终码流使用相同叶序。

- `--intra-select directional`：在原七种亮度模式外增加 D45/D67/D113/D135/D157/D203，色度保留原七种共享 U/V 模式。使用固定 AVM 的方向导数和四抽头 IDIF，并处理符号舍入、clip、TR/BL连续已解码样本及边界扩展。没有套用 AV1 的亮度插值。

- 每个64/32树保留原有 DC/V/H 有界种子；16树由一条因果 SAD 路径开始，再逐叶做一轮 Y/UV 坐标优化。每次候选从相同超块入口重建完整叶序列，比较真实 tile 符号路径的概率成本 R 与量化/逆变换/clip后的 D；自适应CDF在成功重放后才提交。不是全组合或全局最优搜索。

- 修正候选标识容量：十三模式使用每叶四位；16叶及partial树不再强塞入四叶路径字段，另用完整模式/UV/系数/重建hash校验重放。保留候选反序、失败恢复和整帧/逐超块rate一致检查。

- `fine` 限定 single-picture + basic/directional + RD；basic/directional继续与现有系数、运动优化互斥。旧默认、旧多帧及旧单图入口保持行为。



可运行的冻结开发程序：



```powershell

& <LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-r2-20261024/bin/SvtAv2EncApp.exe `

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

</details>


<details>
<summary>005 · M6-intra-pause-20261025.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-intra-pause-20261025.md`  
原文件 SHA-256：`21a573546865f1ce1da4c45813cdf6da78c7e7ed8a8bb7713238d649a4a77ca4`

# 单帧当前轮次收尾与暂停



完成时间（UTC）：2026-10-25T00:39:48.024005+00:00。按用户选择，只完成最早r4c六点；较晚r4d/r4e已中断并保留证据，没有完整曲线结论。本轮结束后暂停，不启动后续实验。正式仍0.0.5-m5，holdout20和I-GATE/正式2倍速度验收均未执行。



本轮已审计最好为 **r4c：相对AOM同质量VMAF BD-rate +11.8334%**。尚未追平AOM，也未达到0.85体积目标。



固定原生1920×1080、10bit420、2D开发帧。主指标VMAF及PSNR-Y/U/V；统计raw OBU完整字节。各BD采用自己的合法共同质量区间，不能相加。



|版本|对AOM体积BD|VMAF90体积比|单帧CPU秒范围|单帧墙时分钟范围|

|---|---:|---:|---:|---:|

|r4b|+12.9993%|1.2438|1393.5～2612.9|29.4～51.0|

|r4c|+11.8334%|1.2174|2306.4～3816.7|45.8～74.7|



AOM0档六点实际CPU 6.984～16.516秒，墙时 7.132～16.890秒。

在VMAF90处按相邻实测点线性插值，最好体积版r4c约2799.0 CPU秒，AOM约10.40 CPU秒，约269.0倍。此为开发诊断量级；并发争用、单次运行和重建导出使其不能代替隔离五轮中位数。当前显然未达到2倍耗时目标。



已通过的r4e组合包含原104项回归、45组公开入口、默认关闭兼容和10组native/SAN，固定AVM像素一致。thin64新增强制几何7656条通过，但原r4f公开入口漏改setter上限导致初始化失败，原失败证据保留；MAIN已一处修复，r4f2只做必要短复测，完整新组合及原生曲线未启动，不作为本轮体积收益。



扩大13模式×4变换的局部实验没有最终收益，并有一个裁块回退；后续61模式实验按用户要求只收已启动部分，未完成点明确保留为未运行。没有把局部J、ADST使用率或滤波消融相加当作压缩改善。



官方实际使用核查证实UV存在ADST而量化偏移全0；DIP/palette未用，两个重点流IST/CCTX实际零，滤波存在交互。UV接线仅只读方案，生产未改。



逐点原始结果、CPU/墙时、完整身份与队列状态见 [JSON汇总](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-r3-20261024/pause-after-current-round-summary.json)。



本轮r4c直接对上一完整r4b的VMAF BD为−0.8450%，直接对最初directional为−34.4712%；各用各自共同域，不将它们与对AOM百分比相加。VMAF90处r4c/AOM约26594.6/21845.2字节，即仍大21.74%；VMAF80/93体积比为1.1649/1.2269。



同批PSNR-Y/U/V匹配的对AOM BD为+31.9794%/+11.6380%/+19.6654%，未隐藏其与主VMAF排序/幅度差异。



|r4c QP|raw OBU字节|VMAF|CPU秒|墙时秒|

|---:|---:|---:|---:|---:|

|190|4435|44.200715|2306.438|2747.680|

|160|10317|71.610491|2457.047|2917.500|

|130|21480|87.249250|2685.438|3160.082|

|110|33565|92.138170|2887.266|3388.688|

|90|54236|94.722830|3396.719|4016.591|

|70|99259|96.040809|3816.688|4484.645|



原r4f完整107项为105PASS/2FAIL：漏改参数setter和旧transform_path负例；r4f2生产只修setter一常量，fresh构建、整项API与三格式公开fixed短复测PASS；旧负例两数值修正后与原static libs整项190检查PASS。原FAIL/冻结身份保留，未称完整新组合或原107一次全绿。详见外置`integrated-r4f-test-addendum.json`。



所有本轮编码/测试/子智能体已结束；r4d/r4e为用户主动中断，不作完整曲线推断。后续等待用户指示，不自动继续。

</details>


<details>
<summary>006 · M6-intra-r4-20261026.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-intra-r4-20261026.md`  
原文件 SHA-256：`4192ce603fd0194501bdcd5d00a11e5e36be6b5ee0eb7b79eaa2d4d1789fb4e7`

# M6 单帧第4轮：AVM差距分析与SMOOTH/TX有界观察器（2026-10-26）

## 一、结论与验收边界

本轮由 AgentDock-NuxBox1 连接本地 Codex App Server 研究 M6。**没有完成本轮 AV2/AOM≤1.00×体积的目标，更没有完成最终≤0.85×体积、≤2×耗时的总目标**。完成了：AVM工具/决策差距分析、冻结CCSO源码的453文件逐SHA独立副本、SMOOTH与合法TX联合试算观察器、Windows Release单线程构建、一个10-bit420小图的字节精确回归和固定AVM独立解码验证。没有建立新的1080p VMAF v1质量曲线，也没有计时基准或holdout测试。

用户明确NuxBox当前FFmpeg压制占用CPU，故本轮**不测速**，不触FFmpeg/FFmpegFreeUI、不点编码队列、不进行高并发1080p长测。检查时ffmpeg.exe仍在运行，历史MAIN与r3冻结证据保持不变。

## 二、冻结指标

- 测试口径：AOM AV1 3.15.1 cpu0/ALL-INTRA/threads1，AV2单帧10bit420。主VMAF `vmaf_v1.0.16_3d0h`；副`vmaf_v0.6.1neg`，旧普通0.6.1不作新选优。v1 93/93.5/94/94.5/95以及完整宽域、跨内容；PCHIP log bytes与线性均报告，实点双侧包围。
- r4c原参考最多允许相同质量1%体积回退；不得将新版本候选改写为参考，不偷用holdout20。
- 前轮CCSO(Y4wide)在v1 93–95相对AOM 1.185136518(PCHIP)/1.186641671(线性)；v1 95处1.261299。到1.00尚需高带相对当前降低约15.62%、95附近20.72%；到最终0.85还需从当前高带降约28.28%。这些只是所需幅度，不是本轮已实现的收益。

## 三、AVM为什么在已冻结测试中约0.85–0.90× AOM

这是**具体AVM cpu9、AOM cpu0、具体2D素材与VMAF v1的经验数据**，不可泛化为AV2标准的固定增益，也不是搜索成本相等的对比。机制为更丰富的合法划分/预测/变换/UV工具、较完整的联合RD搜索及自适应概率代价、滤波在视觉指标下的结构优势，最终以实际被选中的组合体现；工具在规范中出现不等于该素材上实际产生BD节省。

本机历史已冻结果与反例：
- Y4wide的SAD/DCT预选在完整TX/MRL/RDOQ之前发生；漏掉的SMOOTH预测即使后来更好也无法重回候选集。官方AVM参考搜索中有`intra_block_yrd`等对预测与变换的进一步评估。分区只用有界树集合也可能先淘汰合法的混合长条树。
- 官方AVM新版v1 93–95滤波分组消融：全关闭相对原官方BD体积约+4.19%，仅关闭CCSO约+2.86%；重新搜索引发交互，不能把两项相加、不能视为本项目新增工具的可拿收益。
- 旧官方样本DIP/Palette/IST未用、CCTX为NONE、restoration为NONE，FSC少量面积；移植整套工具不能推断取得当前缺少的15.6%。以前YALL全开放有局部正收益但其它QP/内容退化，dual-finalist RDOQ、phase1也曾混合甚至负向。
- 目前不能按“工具A占5%、工具B占8%”精确归因。应先做同因果前缀的具体遗漏模式观察、真实码流点与跨内容验证，再决定加入的模式/树。

扩展分析和候选优先级见`M5-M6-AI-TASKS/07b-M6-AVM-gap-analysis-20261026.md`。

## 四、本轮真实产物：SMOOTH/TX有界观察器

目录：`svt-av2/artifacts/intra-r4-20261026/`。本地Codex创建了`prepare.py`的完整补丁内容，但Codex补丁工具未完成写入；AgentDock从本地Codex会话日志恢复此唯一补丁，未修改MAIN。成功运行后`preparation.json`记录由冻结`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-rd/ccso-native-auto-r1/source-manifest.json`逐SHA验证并复制**453个文件**。该独立源仅改`Source/Lib/Encoder/rd_frame.c`，具体diff`smooth-tx.patch`，每个实验文件哈希见`source-manifest.json`。

观察器在`refine_tools_linear`的原候选因果前缀下额外对合法域SMOOTH(mode 4)+有限TX方案进行`linear_leaf_trial`，记录trials、wins、local_gain；编译时`/DR4_APPLY=0`表示**只观察不应用**，未将局部获益当整帧码率下降。普通fast/原冻结编码器的原始码流及重建须保持一致。

**真实执行：**
- `python -m py_compile prepare.py`：PASS
- 源冻结453文件SHA核对并复制：PASS
- MSVC/Ninja Release构建，`SVT_AV2_BUILD_TESTS=OFF`，`SVT_AV2_WARNINGS_AS_ERRORS=ON`，`/DR4_APPLY=0`，`--parallel 1`：PASS；编译日志`configure.log`、`build.log`，二进制`build-check/bin/SvtAv2EncApp.exe`
- 一张64x64、10-bit420、固定数学式生成的单帧，QP110，同一`--intra-select cfl --partition-select mixed8 --rd-mode fast --tree-refine wide --transform-select full --mrl-search rd --angle-search rd --coeff-opt rdoq --threads 1`：冻结程序及观察器均退出码0，AV2同为**1532字节**且SHA256 **e079092a4eec5f3514df612499e77d58a5cf2626f46a64490295c5ddad81a781**；重建raw同为**12288字节**、SHA256 **44ded899cf977b32babfbdda8f4a7295624e7c0e7205f47a9fa58379e6f6695b**。
- 固定`AVM-AV2-TEST/tools/bin/avmdec.exe`成功解码观察器流，`--rawvideo`重建恰好12288字节，与编码器重建SHA一致，**PASS**。
- `observer.stderr.log`三条实例：`decision=83 trials=208 wins=7 local_gain=34963`，`decision=412 trials=224 wins=8 local_gain=26717`，`decision=82 trials=192 wins=5 local_gain=26161`；均`apply=0 error=0`。此处局部J改善取决于原样本和前缀，**不得相加估算真实码流压缩率**。原始`smoke-comparison.json`、码流、重建和日志都在隔离目录。

## 五、明确未完成的测试

1. 未构建或执行`/DR4_APPLY=1`的真正应用候选、故障恢复/反序/SAN全矩阵；`/DR4_APPLY=0`小图字节相同不证明应用版安全。
2. 未做原生1080p多个QP、跨2D/3D/Real的VMAF v1/NEG曲线，也未计算新候选AV2/AOM体积比；**最佳经过验收的全帧值仍为上一轮的1.185x**。
3. 未完成新版速度验证，按用户要求不进行。
4. 未合MAIN、未动holdout20/I-GATE、未改3FUI/ffmpeg。

下一轮的最小验收策略和Codex交接见`M5-M6-AI-TASKS/08-M6-intra-r5-plan-20261026.md`，原始调用问题见`07c-M6-codex-execution-audit-20261026.md`。

</details>


<details>
<summary>007 · M6-intra-r5-agentdock-20261026.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-intra-r5-agentdock-20261026.md`  
原文件 SHA-256：`9dfa2ed92f5684253c5024145e5667cbe22b79743db11a50bf8536480a5655b1`

# M6 第五轮：AgentDock 直接执行，体积优先

更新检查点：2026-10-27 06:41 UTC。工作仍进行中；1.00 与 0.85 尚未达成。

## 当前结论

本轮固定 2D 开发帧、主模型 VMAF 93–95 区间，RDOQ 双完整路线保留全部16个原始点后，相对 AOM 的同质量体积比为 **1.1217015453（PCHIP）/ 1.1229013326（线性）**。其中Q106→105存在主质量反转，保留原始测量。冻结 R3 为 **1.1851365178 / 1.1866416706**，本轮约省 5.3%。这是单张开发帧的区间结果，不是全素材或 holdout 验收。

新的完整组合先达到 1.1386197753 / 1.1370184541；继续补量化搜索后得到上面的 RDOQ 结果。所有用于曲线的点均为原生 1920×1080、10-bit 420 实际编码，经固定 AVM 解码与内部重建逐字节相等，并使用原冻结双模型评分。

用户本轮明确只验体积，不验耗时；FFmpeg 正在运行。所有性能结果均标记 **NOT_MEASURED**。FWD 和候选数量是算法工作量计数，不能据此声称墙时加速，也不能把单独等价优化的 FWD 直接套到扩展工具的组合上。

## 执行边界与保存位置

- 全部设备操作、构建、测试和文件修改经 AgentDock-NuxBox1 执行；没有调用设备本地 Codex 或 OpenCode。
- 用户的 FFmpeg 未停止、未修改。只处理本轮自己的命令会话。
- 旧 Codex 自动唤醒计划任务已先导出保存再禁用：SVT-AV2-M6-Autopilot-20261026、SVT-AV2-M6-RestartWatch-20261026。
- 工作与全部证据根：<LOCAL_PRIVATE>\ffmpeg-native-m6-tests\agentdock-r5-20261026。
- 正式版本仍 0.0.5-m5；20 张 holdout 未使用；R3 冻结源码、程序、固定 AVM 和旧质量结果均保留。
- 本轮算法候选在独立源码快照与构建目录中，不以未完成的开发帧筛查替代正式 M6/I-GATE 验收。

## 冻结比较口径

输入为既定 2D、3D、Real 的 frame48；AOM 3.15.1 allintra、cpu-used 0、单线程、CQP。主模型为 vmaf_v1.0.16_3d0h；辅助为 vmaf_v0.6.1neg。

主区间固定 93–95，使用 log(bytes) 的 PCHIP 与线性积分，不外推；要求至少四个真实区间内点并有两端包围点。质量倒序保留原始点，不能删点择优。五个同质量位置为 93 / 93.5 / 94 / 94.5 / 95。

宽域继续相对原始 r4c 的最差同质量回退 ≤1% 检查，不能将参考换成更差的 R3，也不能把 mixed8 的已测宽域结论继承给新的组合。宽域加入后应重算主区间，因为 PCHIP 边界导数也会改变。

冻结输入：
- speed-r3-v1-20261025\ccso-auto-fullcurve-r1\curve-inputs.json
- SHA256：1b943cd6a1a6e4c81b76bba072ed784474bd4b7051e19a8b6493bd74aab91162
- R3 app SHA256：51def9e0c6015be713744980fa80475beca1a891fc8cf1c774b96648abfc8f5e
- R3 源清单 SHA256：cad286f01ea20af2c71d73dc0a65bc8640ebade90567cbfa2124f80cd1d431cd

## 实际加入与核对的能力

| 候选 | 实际内容 | 当前结论 |
|---|---|---|
| smooth | SMOOTH 与所选 TX 的联合补搜 | 九点；约1.1643，有105→104主分倒序，保留原始证据 |
| mixed8 | 将既有混合8几何与 CCSO 自动选择接通 | 13点；1.154774 /1.157672；104→102主分倒序 |
| asym | 新增四种根层三叶非对称合法二叉计划3285–3288 | 32条强制双编译器/固定AVM对拍；七点有倒序，单独收益有限 |
| flip2 | 新增合法 FLIP/FDST 主变换，并按实际宽角与 MRL 有效模式写符号 | 375条强制流双编译器及AVM对拍；修复flip首版原生不一致 |
| joint2 | 同一进入状态下联合搜索13个基本Y模式与合法TX，之后重做UV并用完整SB真实J验收 | 严格双编译器、93739项RD检查、实际原生解码通过 |
| combo | mixed8 + 根三叶 + joint2 + flip2 + 等价缓存/下界 | 九点，1.138620 /1.137018，无已测主/辅分倒序 |
| rdoq | 同一SB入口保留完整单强度路线，增加完整三强度路线，真实完整SB J择优 | 全16点保留；1.121702 /1.122901；宽域双插值通过，3D锚点混合、Real锚点改善 |
| context | 在上述两路线之外，增加真实前缀CDF/邻居的系数提案路线 | 两点通过；Q100改善、Q110有质量/体积取舍，未完成正式曲线 |
| shapes2 | 既有薄长几何首次接入fast路径；扩基础finalist到4，保留各候选组及新增根三叶 | 四夹具与两原生点AVM exact；两点不一致优于rdoq，暂不称主候选 |
| ccso | 新增Y平面和合法完整offset集合；独立保留旧UV路径作完整包J基准 | 288强制流双编译器/AVM exact；Q110主分小退、Q100相同，保留实验能力 |
| uvtx2 | UV模式派生的合法主变换实验 | 对拍通过；两点效果混合，不默认合并 |
| pixel | 真实像素域RDOQ提案，开放≤32维的矩形，保留前三完整路线 | 四夹具与两原生点PASS；Q110含6954矩形SSE对照，效果混合，未补全曲线 |
| deblock3 | AVM亮度单/双方向去块，独立合法delta，逐候选重新选择CCSO并比较完整封包J | 75强制流、四自动夹具和两个原生点PASS；仍无完整BD曲线 |
| treeq3 | 每棵finalist完整RDOQ；修复共享分块计划随最终赢家提交 | treeq2原生封包失败保留；treeq3四夹具PASS，原生正常/强制旧路线进行中 |

“亮度推导色度”不能计为本轮从零新增：R3 已有隐式/显式 CfL 和 MHCCP 搜索。本轮核对了它们的实际接线与缓存适用域，组合保留其生产重建；新增 Y-CCSO 属于另一项跨分量滤波工具，不把它冒称为新 CfL。

根三叶用 AV2 已支持的 HORZ/VERT/NONE 二叉语法表达，不照搬 AOM AV1 的 AB 分区符号。薄长块不强行启用其尺寸不支持的 FLIP、MRL 或 UV TX。

## AOM AV1 基础技术核对

实际阅读本机 libaom/av1/encoder/tx_search.c 的 try_tx_block_split，以及 encodemb.c 的 av1_optimize_b / av1_optimize_txb 接线。

AOM 将已完成子块的实际 RD 累加并传递剩余预算，超过预算则停止；其系数优化获得当前 TXB_CTX。M6 的最终完整SB gate虽使用真实率，但旧RDOQ提案仍使用默认CDF和清零邻居，可能漏掉好提案。因此本轮增加 live-prefix 提案路线，并保留完整旧路线作实际比较。

context 的关键约束：
- prefix 单独创建、释放；RdWork 快照仅借用，不误认为上下文随结构体复制回滚。
- anchor 系数内联存储；metadata 的 qcoeff 清空，使用时重新绑定，避免自引用指针随快照复制失效。
- active 仅围住 proposal；确认试写前后 prefix rate 不变，且累计 trial rate 不小于 prefix。
- proposal仅固定当前叶其他平面；接受必须重建完整后缀、比较完整SB真实J。
- 禁用新路线时，夹具逐字节复现原双路线。

pixel 路线已用生产反量化、生产逆变换和生产 clipped recon writer生成局部像素，保存最终赢家 SSE，并在随后正常重建中逐次断言相同。先不开放含64维截频的矩形；保留原来所有完整路线。它旨在修正提案尺度，不把像素SSE改善当作VMAF改善。

## 等价计算量优化

冻结R3的2D Q100为5,626,835次实际FWD。下列候选输出均与同一冻结R3的码流和重建相同：

| 候选 | 实际FWD |
|---|---:|
| R3 | 5,626,835 |
| Y复用 cache | 5,529,299 |
| txcache2，8192槽、SB清空 | 2,092,287 |
| txcache3，跨SB复用且校验生产callback | 2,080,748 |
| bound2，真实Y失真+真实前缀Y符号率下界 | 1,987,059 |

bound2在Q100尝试312,925次，严格下界剪掉86,983次；较R3实际FWD少约64.7%，**尚未达到几十万**。只在严格大于 incumbent 时剪枝，保留平局顺序。shadow构建另外核对Y像素一致、Y率≤完整率、下界≤完整代价；四夹具双编译器与固定AVM一致。

缓存哈希仅选择槽，命中必须比较所有键与有效残差样本；强制单槽碰撞及ASan/UBSan专项通过。8192槽缓存约160.3MiB分配量，不作为RSS或速度数据。

新增联合工具会扩大候选集合，combo Q110自身FWD约589万，不能宣称新组合也是198万。rdoqbound已经将更强bound2接入rdoq；四夹具与shadow对照、原生Q100均逐字节等价。其Q100实际FWD为4,854,243；下界试探2,373,981次、剪掉1,645,343次。不能将1,987,059的R3范围计数套用到这个新工具集合。

## 主要验证证据

工作根下：
- 每个 <variant>-source-manifest.json、<variant>.patch、build-<variant> 及构建日志。
- native-<variant>-2D-q*/result.json 与 job JSON，完整命令、程序/runner/model/输出哈希。
- validation/<variant>/results.json，MSVC/GCC、固定AVM及等价参考结果。
- forced-flip2-r1/result.json：375条强制FLIP/宽角/MRL流。
- forced-ccso-r1/result.json：288条8/10-bit、64/80/128、纹理/上下界、Y及YUV offset流。
- collision-sanitize-r3/result.json：强制冲突及检测器证据。
- curve-analysis-20261026T143621Z.json：本检查点全部16个rdoq原始点及双插值分析；早先分析快照保留。

失败或不足的记录也保留：
- flip首版原生解码不一致，未评分；由flip2修复。
- shapes首版首先在旧fast参数域被拒绝，未用失败输出做质量结论；shapes2补通域后独立重测。
- pixel首次准备脚本遇到“两处phase循环”的匹配断言，未构建；原复制源码逐文件核验未改后由resume脚本继续。
- 不完整两点候选即使分析JSON有局部积分，也不能称其93–95正式BD结果。

## 宽域与跨内容最新证据

rdoq共16点，包含主区间9点、190/186/160/130/90/70宽域点和105诊断点。Q186=4700B，v1=51.460547，NEG=45.231711。加入186之前，低端190到160跨度导致主模型线性最坏比值1.0372615；该失败快照仍保留。补186之后：

| 比较原始r4c | 实际共同质量域 | 最坏体积比 | 最坏位置 |
|---|---|---:|---:|
| v1 PCHIP | 47.100970–97.839094 | 0.9868177057 | 47.100970 |
| v1 线性 | 47.100970–97.839094 | 0.9946287991 | 47.100970 |
| NEG PCHIP | 41.015477–95.057018 | 0.9668730253 | 49.346502 |
| NEG 线性 | 41.015477–95.057018 | 0.9459823415 | 41.015477 |

这证明所测共同域的≤1%回退约束通过，不证明整条质量轴或holdout通过。主区间五个同质量位置对AOM的PCHIP比值仍约1.1339/1.0779/1.1317/1.1571/1.1632。

3D Q110：94832B，v1=94.639097，NEG=91.619121；R3为98476B/94.765919/91.315009，结论混合。Real Q100：76766B/94.795961/92.832566，优于R3锚点79329B/94.736682/92.611070。仅一锚点，不计算跨内容BD。

pixel Q110=30715B/93.452017/90.661325，Q100=38916B/95.042381/92.404493。Q110生产逆变换+clip的SSE自检20700次，其中矩形6954次；新增inverse调用2258593次。FWD并不代表全部工作量。context和pixel两点效果接近，暂不为两者额外铺完整曲线。

## 补查AOM基础与新候选

另实读libaom/av1/encoder/partition_search.c:3144,3480,4593,5664的子块/矩形/递归分区路径，intra_mode_search.c:1619的逐模式TX搜索，tx_search.c:2223–2227的系数trellis接线与2688的TX分割。M6仍以有限几何计划和每预测叶单TX为主，不能声称基础功能已经齐全。

发现M6 fast的refine_tree_finalists先按工具+标量量化J选树，只有胜树再做RDOQ。treeq2新增完整逐树RDOQ路线，完整旧路线保留，严格J改善才接受；重播核对真实D/R/J及hash，统计累计两路线。测试专用retain-old构建会执行新搜索后强制恢复旧结果，检查共享prefix与图像状态泄漏。

deblock首版失败为封包内部重新建立帧头丢失去块字段，编码器改了像素而实际码流通知关闭。deblock2将参数完整传至封包，两种CCSO分支都使用完整帧头；原失败源码、程序和forced-deblock-r1保留。deblock2的forced-deblock2-r1全部27例PASS，24个开启样例均实际改像素；validation/deblock2四例PASS。已有两点原生解码闭环，尚无完整同质量曲线结论。

shapes2补充同源cache0/bound0对照，validation/shapes2-exact-r2四例PASS。旧validate.py命令记录存在可变列表后续被修改的元数据问题，现已改为记录副本；旧夹具输出哈希有效，旧commands数组不作最终命令依据。第一次shapes2-exact脚本因此在启动编码前StopIteration，保留失败，修订版使用不变baseline命令重建。

## 2026-10-26 15:20 UTC 新增验证与实际计数

主93–95完整曲线仍以rdoq的16个保留原始点为准：1.1217015453 /1.1229013326，1.00与0.85均未达成。以下两点只用于候选筛选，不能写成正式BD收益。

| 候选 | Q | 字节 | 主模型v1 | 辅助NEG |
|---|---:|---:|---:|---:|
| deblock2 | 110 | 30889 | 93.591913 | 90.669003 |
| deblock2 | 100 | 39095 | 95.059508 | 92.395467 |
| deblock3 | 110 | 30888 | 93.635714 | 90.667997 |
| deblock3 | 100 | 39094 | 95.086184 | 92.398969 |

deblock3同时保留关闭、四档单V、四档单H和16种V/H独立delta，共25个完整封包候选。处理顺序为逐SB行V(当前列)、H(前一列)、行尾H(最后列)，不是整帧V再整帧H。forced-deblock3-r1/result.json的75例全部MSVC/GCC码流及重建相同、固定AVM解码相同，原有索引0..8还与deblock2逐字节相同；validation/deblock3/results.json四例PASS。

### 八路精确变换缓存

cache8保持8192个完整entry，改为1024组×8路和组内LRU。哈希只选组，每次命中仍比较完整标量键和全部有效残差；缓存初始生产FWD/量化结果，RDOQ后改动不写回缓存。recency与cache位于共享heap，不随RD快照回退；时钟溢出只清空缓存。

四夹具两编译器、固定AVM、单路关闭对照PASS。Q100实际：

- 总查询：16,320,526，前后不变。
- FWD：4,854,243 → 4,395,159，减少459,084次（9.4574%）。
- 完整候选161786、coeff_trials88932、accepts1641、最终D/R/J/winner不变。
- 码流SHA 5f908b38fd5f74fe3db4a18b9dd88e8f3331ae2d2464300e2586d86ce24c582b；解码SHA 5a8af3a32113b4ab0d142d04f14198fa6a989b7d7291a519c3ac8c3cf2fc2897，与rdoqbound和rdoq均一致。
- 仍是439万次FWD，不能称为几十万；不验实际耗时。

### 实际失败与修复候选

treeq2四夹具虽通过，但都没有实际新树获胜。原生Q100完成480个全SB、60次实际换树后，在最终封包返回INTERNAL。原因是RdWork快照借用output->decisions数组，恢复旧赢家时未同步该数组；RD用私有decision核验通过，封包却读取试算留下的计划。失败源码、二进制、native-treeq2-2D-q100/FAILED.txt和encode.log均保留。

treeq3从按值保存的winner_decision重新建立几何，并增加最终output plan==best_decision不变量；随后仍完整回放D/R/hash、append_winner、提交实际CDF。四夹具已PASS。正常原生100/110和执行新搜索后强制旧结果的原生100正在运行；这些结果未完成前不接受候选。

tuple的第二轮angle/MRL继承非默认TX，在4:4:4前缀Y率计算处失败。tuple2为每个第二轮提案先检查继承TX是否合法，再独立提供默认TX；tuple3进一步纳入共享计划修复。已通过原失败444用例和8bit用例，444有repeat_wins=2、illegal_pair=45、default_pair=2241；其余检查/原生尚未完成。原失败版本保留。

### 帧内参考边滤波

edge从实际AVM av2/common/reconintra.c移植三个5-tap核、强度规则和corner滤波；保留版权与来源SHA。顺序为矩形wide-angle解释、参考过滤、IDIF延伸。固定AVM在外层MRL>0时关闭此滤波，包括multi-line内部的零号参考行；未自行加入AV1旧版upsampling规则。

跨SB仅查询已经提交的Y/UV smooth-mode网格；同SB仅查询当前重放前缀ids/uv_ids覆盖的那个上/左坐标，找不到不回退全局旧状态。每个SB最终提交后才写模式网格。四夹具MSVC/GCC/AVM已PASS，功能关闭与rdoqbound逐字节一致；原生100/110进行中。独立强制工具正准备288个流，检查四种邻居组合、矩形方向、跨SB及96个MRL开启/关闭滤波的亮度等价病例；尚未宣称强制矩阵通过。

## 2026-10-26 15:41 UTC 后续结果与分块基础扩展

主合同、RDOQ16点1.1217015453/1.1229013326和未达1.00/.85的结论不变。下面是新增真实原生点，不能作为正式BD曲线。

| 候选 | Q | 字节 | 主模型v1 | 辅助NEG |
|---|---:|---:|---:|---:|
| treeq3 | 110 | 30820 | 93.332772 | 90.668071 |
| treeq3 | 100 | 39179 | 94.793031 | 92.376397 |
| edge | 110 | 30560 | 93.271077 | 90.580182 |
| edge | 106 | 33780 | 94.114158 | 91.410412 |
| edge | 100 | 38842 | 95.125201 | 92.321334 |
| tuple3 | 100 | 38807 | 95.062236 | 92.444951 |

### 原生恢复闭环与候选取舍

treeq3原生100/110已完成固定AVM解码与重建闭环。Q100正常路线60次实际新树获胜，最终共享计划修复1次。执行新路线但强制保留旧赢家的独立原生控制有61次潜在新树获胜、61次计划修复，最终码流、重建SHA与rdoqbound完全一致。treeq2的最终封包故障已用真实非零覆盖闭合；treeq3两点质量/体积未提供继续扩大曲线的证据，暂存。

tuple3四自动夹具全部PASS。原生Q100比RDOQ少289字节，主模型和NEG均提高；147个SB实际采用二轮工具路线，497个非法继承变换组合被筛掉，默认变换提案66768次。这个点的实际FWD为9424316，候选726681，不能只报告体积好而隐去增加的计算。Q110仍待完成，不据单点宣布BD获益。

edge三个原生点体现质量位置和主/辅模型的取舍，暂不加入领先组合或扩大曲线。384条强制码流全部MSVC/GCC/fixedAVM一致，144个MRL亮度用例与滤波关闭一致；Y四mask计数4650/3000/750/2010，UV2088/408/1584/960，实际改动预测样本分别1831370/308922。第一次288例因测试选择的Y偏角不属于V/H而没有UV继承覆盖，失败证据保留；第二次修正仅测试夹具、所有生产Source文件SHA保持一致。

### 精确搜索去重

sharedq把treeq3重复执行的每棵树线性工具搜索F合并为一次，再执行完整RDOQ G。分别按F键选择旧路线、按G键选择新路线，内部保留incumbent/plan/hash原tie，外层平局保留旧结果；F hash按值保存，未被G hash覆盖。每个seed恢复同一工作入口和重建区域，实际统计继续累计，正式CDF只在最终赢家提交。

四夹具正常输出与treeq3逐字节一致；强制旧路线也与旧控制逐字节一致，双编译器/AVM一致。原生Q100正常及强制旧路线进行中，待确认实际F/G和FWD减少；不把共享掉的工作计成虚拟次数。

### 更细的32/16/8分块

fine2在rdoqbound上独立扩展局部分块，未引入已经观察到取舍的treeq3路线。内部计划改为uint32：bit20是family，bit16–19控制四个32象限保留，低16位按4×4行优先控制每个16单元细分为四个8。keep32覆盖的split位清零，非法高位或非canonical输入明确拒绝。描述符不写入码流，仍使用现有合法HORZ/VERT/NONE树；最多64叶，部分SB继续旧域。

旧coarse winner几何投影后，最多1+12+16个完整basic RD提案，从所有去重候选中保留两棵新树。每一坐标的三个替代从同一进入plan生成；每个局部flip可先展开keep32再细分一个16单元。旧finalists及完整工具/RDOQ路径不被挤占，两个新seed各自做完整工具+RDOQ和D/R/hash重播后，与完整旧结果严格比较，旧路线赢外层平局。共享输出plan重新绑定最终赢家。

MSVC/GCC严格构建已通过；forced-fine2-r1中96条强制码流跨编译器和固定AVM解码一致，其中36条全32/16/8端点与旧plan1/2/3逐字节一致；单bit、canonical、精确覆盖与叶数公式检查通过。自动四夹具包括功能关闭、实际搜索后强制旧结果、同坐标选项反序以及跨编译器，正在运行。首版仅构建阶段的size_t链式归零警告和测试缩进警告原样保留，fine2无算法改变。

### 当前组合

corepack只组合deblock3与已原生证明精确的cache8。四自动夹具已全部PASS，完整码流与重建均与deblock3相同；双编译器和固定AVM一致。它没有引入额外搜索算法。原生身份和完整同质量曲线尚未通过，不提前宣称组合已经达到体积目标。

## 2026-10-26 15:57 UTC 原生闭环与下一轮组合

此节取代上节仍在运行项目的状态，历史试验及失败记录保留。正式最佳完整曲线仍是RDOQ16点PCHIP1.1217015453341768/线性1.1229013325595267。目标1.00和0.85仍未完成。

| 候选 | Q | 字节 | 主v1 | 辅助NEG |
|---|---:|---:|---:|---:|
| tuple3 | 110 | 30665 | 93.729218 | 90.736508 |
| tuple3 | 100 | 38807 | 95.062236 | 92.444951 |
| fine2 | 100 | 38737 | 95.033430 | 92.386216 |
| corepack | 100 | 39094 | 95.086184 | 92.398969 |

### 二轮搜索与局部分块

tuple3两原生端点均比RDOQ更小、主v1和NEG均提高，支持集成测量；还不是完整BD收益。

fine2四自动夹具全PASS：功能关闭和执行新搜索后强制旧结果均与RDOQbound逐字节一致，同一坐标选项倒序与正常一致，双编译器和固定AVM一致。四小夹具都确实做了每SB25个去重basic和2个full，wins为0。原生Q100实际出现93/480个fine赢家，11997次basic、960次full，38737B，主v1略升、NEG略降。原生强制旧路线有98次潜在新赢家、480次共享计划修复，最终码流/重建/质量与旧RDOQ完全相同。真实回退和winner路径均已覆盖。Q110继续运行，未扩大单分支曲线。

### 精确复用的原生证据

sharedq正常输出码流、重建、SSE、两个质量分与treeq3完全相同；强制旧输出与RDOQbound完全相同。正常F=G=1733，FWD6847479→5277806（约−22.9%），candidate443312→360951；rollback F=G=1739，FWD5292136。这里的计数都是实际执行，不是折算；百分比只适用于此路线和原生样本，不能继承给下一组合或换算成耗时。

corepack原生Q100与deblock3的完整码流、重建、SSE及两模型分数完全相同；FWD4395159，与cache8对应路径一致。新组合必须另外验证，不直接相加各工具收益。

### 正在构建的独立候选

stack1把tuple3的三条竞争路线、已验证的shared F/G复用、cache8和deblock3合在一起。shared仅合并前两路线相同的F计算，二轮工具搜索保留独立第三路线。验证配置同时准备ref（不共享/单路cache）、nodb（应与tuple3逐字节相同）、执行后强制旧路线（应与corepack相同）。

primary1d2从RDOQbound独立加入一维主变换数学、typed扫描、LF/HF/BR上下文、轴DC符号及typed RDOQ。实际候选仍由固定AVM mode-dependent七符号合法表控制；IDTX9/FSC未实现，不强行加入。当前Y>=8域可达10/11/12/13；14/15仅数学/扫描域，不冒充可达码流覆盖。准备288条强制流，包含矩形identity、多个DC-like正负符号、c0为零、轴外系数及单非零但EOB>1，另做实际CDF更新/邻居写回检查。

首版primary1d因Windows构建脚本占位符误替换CMAKE_C_FLAGS、关闭分支常量条件警告及测试Quantization include依赖未通过构建。已保留首版源和日志，在primary1d2修正关闭分支与测试链接，Windows脚本增加CMakeCache完整feature flags断言。stack1同一脚本模板问题在任何测试/原生测量前发现，错误构建已停止并备份配置日志；用正确参数重新构建，冻结源未改变。

## 2026-10-26 16:16 UTC 一维变换闭环、细分组合与CDEF接入

此节更新上节运行状态，不删除历史失败和候选。最佳完整主93–95曲线仍为RDOQ16点PCHIP1.1217015453341768/线性1.1229013325595267。1.00与0.85均未完成；两点和内部SSE/J不能代替匹配质量曲线。

| 候选 | Q | 字节 | 主v1 | NEG |
|---|---:|---:|---:|---:|
| primary1d2 | 100 | 38936 | 94.941346 | 92.400566 |
| primary1d2 | 110 | 30867 | 93.283059 | 90.692456 |
| fine2 | 100 | 38737 | 95.033430 | 92.386216 |
| fine2 | 110 | 30553 | 93.378050 | 90.612861 |

### primary1d2：正确性完成，当前不默认合并

288强制码流MSVC/GCC完整流和重建一致、固定AVM逐样本一致；四自动夹具通过，工具关闭逐码流回到RDOQbound。扫描、EOB、轴DC符号CDF和邻居写回均有直接覆盖。自动搜索实际命中一维变换，且其typed RDOQ确实修改系数，非只做不可达强制测试。

原生100少0.41%字节但主分略降；110仅少20字节而主分下降约0.2363。保留实现和全部验证记录，暂停额外曲线及默认组合。此处只是降低本轮优先级，不宣称所有内容无收益。

fine2原生110已完成：30553B、主93.378050、NEG90.612861。相对旧RDOQ更小但两个分数也降低，两个端点不能宣布BD改善；组合中的作用必须与stack1对照。

### stack组合

stack1四自动夹具全部PASS，包括noDB逐码流复现tuple3、关闭精确复用与正常相同、实际试算后强制旧路线逐码流复现corepack、GCC和固定AVM对拍。原生Q100控制正在测量。

stack2在stack1上加入fine2：内部plan全面uint32，fine_prepare的seed数组在RdWork之外保留，fine_refine明确重置二轮控制状态，只做原fine的一轮full+RDOQ；回退对象是完整stack1赢家。最终几何同步/append完成后，DB从最终blocks建立边界图。准备阶段的窄化函数误替换和旧final-plan锚点问题都在冻结前发现，失败阶段源留存；成功源460文件已冻结。MSVC四臂和GCC严格构建完成；96强制细分码流及36旧端点别名通过MSVC/GCC/固定AVM。

当前stack2自动状态：{'status': 'RUNNING', 'cases': [{'name': 'partial80', 'status': 'PASS'}, {'name': 'chroma444', 'status': 'PASS'}, {'name': 'eightbit', 'status': 'PASS'}, {'name': 'mixed420', 'status': 'RUNNING'}]}。通过全部门禁后测正常100/110和强制旧100；旧100必须与实际stack1同流。若继续选择stack2，还需要stack1 Q110对照，不能拿RDOQ端点替代组合控制。

### CDEF：AOM/AVM基础环路工具，尚未宣布收益

固定AVM decodeframe.c在DB后、CDEF前复制ext_rec_y，后续CCSO使用该pre-CDEF亮度参考。第一版限定Y-only CDEF、UV-only BO CCSO、8/10bit420、尺寸为8倍数、单帧/单tile/64SB、无lossless/segmentation。单组uniform strength和on_skip=1，不引入tile强度索引。

保留完整25臂DB+CCSO原赢家作为真实off；从其DB像素建立不可变CDEF快照，边界用AVM 0x4000哨兵，方向/variance只复用同一DB快照。搜索strength1..63与damping3..6共252个像素候选。每个头部长度类别保留最低Y SSE，再分别用显式pre-CDEF亮度reference重选UV CCSO及实际完整packet J。两类代表精简只在当前固定DB、Y-only、UV-only、单组语法内成立，不宣称完整DB×CDEF联合最优，也不宣称SSE支配等同VMAF支配。

cdef1冻结460文件，GCC严格构建通过；MSVC因AVM原int16 sum/y隐式窄化在Werror下失败。cdef2保留失败日志，把两个有界临时变量拓宽int（本域sum绝对值不超过912），不改规范值、不关闭警告。cdef2源460文件冻结，GCC已通过，MSVC四臂继续构建。准备60强制流覆盖8/10bit、64/80/128、跨64/部分SB、主/次/混合强度与不同damping、真实off及DB+CDEF+CCSO实际组合；还验证固定CCSO计划下同类packet等长、tile与UV不变。自动门禁要求off/执行后回退均逐码流复现corepack。

## 2026-10-26 16:45 UTC 组合端点、CDEF闭环及精确剪枝

最佳完整主93–95曲线仍是RDOQ16点PCHIP1.1217015453341768/线性1.1229013325595267。下表是逐点实测，不是新的BD结论。1.00和0.85均未达成。

| 候选 | Q | 字节 | 主v1 | NEG |
|---|---:|---:|---:|---:|
| stack1 | 100 | 38808 | 95.139198 | 92.441447 |
| stack1 | 110 | 30666 | 93.847813 | 90.737885 |
| stack2 | 100 | 38661 | 95.130204 | 92.432823 |
| stack2-rollback | 100 | 38808 | 95.139198 | 92.441447 |
| cdef2 | 100 | 39096 | 95.044517 | 91.868201 |
| cdef2 | 110 | 30891 | 93.591082 | 90.075600 |

stack1两个端点相对RDOQ均更小且两模型分数更高。stack2加入fine2后Q100再少147字节，主分/NEG均小幅降低，等待第二点判断是否值得补完整曲线。stack2四自动夹具、96强制流和36旧别名均PASS，原生强制旧路线Q100的码流与重建SHA逐字节复现stack1。只选择一个最有实测支持的组合补完整曲线，不把实现完成等同收益成立。

### CDEF2：所测规范域通过，当前uniform版本暂停默认合并

原60条强制流均通过MSVC/GCC逐码流和固定AVM逐样本对拍；原aggregate FAIL保留，原因是小夹具auto UV CCSO全部off，未满足三滤波实际联动覆盖。补充test-only hook2强制非均匀BO U61/V23，8/10bit两个病例均使DB、CDEF、UV CCSO实际改变像素；正确pre-CDEF DBY参考全部吻合，错误post-CDEF参考会分别使44/66个UV采样不同。hook关闭与原生产源测试臂完全一致，生产CDEF2源和二进制未改。

四自动夹具PASS：真正off和执行所有252候选后回退均逐码流复现corepack。生产原生Q100自动选择DB19+CDEF17/damping5+UVmask3，Q110选择DB19+CDEF29/damping5+UVmask3，均由固定AVM重建验证。闭环汇总cdef2-closure-r2.json保留原60例失败文件的SHA；不把test-only LUT描述为自动算法结果。

两个端点相对直接corepack/DB3基线均没有有利质量证据，NEG分别下降约0.531/0.592。因此保留独立实现和全部验证，暂不进入默认组合，不追加此版本曲线。此结论限于当前uniform Y-only版本与所测点，不代表所有CDEF方式无收益，也不是完整NEG BD预算失败。固定DB、Y-only CDEF/UV-only CCSO、单组语法内的两类长度代表筛选，不扩展为全联合最优或VMAF支配。

### rdobound：RDOQ后缀精确下界

独立prefix writer与nested/live RDOQ状态分离；已完成YUV叶子的非负失真加实际prefix符号码率构成同一SB lambda下界，只在严格大于incumbent且仍有后缀时剪枝。pruned分支不产生伪D/R/J或hook，不作为完成候选；已有无界赢家重建保留。shadow完整运行并断言maximum bound≤完整J，完成prefix rate与原full writer rate必须相等。

MSVC三臂/GCC严格构建通过。partial80生产出现2892次尝试、2791完成、101实际剪枝；shadow2892全部完成、101 would-prune、0实际剪枝，均与stack1码流和重建相同。首次GCC调用因WSL启动报“找不到元素”返回4294967295，失败JSON及UTF16日志保留；同一命令重试通过，不改源掩盖启动问题。其余门禁状态：{'status': 'RUNNING', 'cases': [{'name': 'partial80', 'status': 'PASS'}, {'name': 'chroma444', 'status': 'PASS'}, {'name': 'eightbit', 'status': 'PASS'}]}。

### rdosave：跳过即将被保存系数覆盖的初始量化

在rdobound独立源上新增生产回放旁路：rd_mode3、生产transform函数/量化器身份与QP/位深一致、coeff_opt2、coeff_policy0、非target且已有saved。资格与cache分配成功无关；自定义回调保留旧路径。只跳过被覆盖的forward/EOB fallback/policy段，预测/残差、saved qcoeff/TX恢复、fresh dequant、inverse、clip、SSE和metadata仍按旧顺序执行。target必须正常forward，scratch原始coeff唯一消费者是target RDOQ；all_zero不在跳过段之后读取。

MSVC两臂与GCC严格构建完成；四自动同流门禁进行中：{'status': 'RUNNING', 'cases': [{'name': 'partial80', 'status': 'PASS'}]}。日志skips是省去的初始量化请求，target_forward也是请求计数，均不能当实际FWD。non_dct按保存TX统计。实际FWD仍取真实调用计数，待原生Q100与stack1/rdobound对照；尚未达到几十万量级。

### 参考实现的缺口定位

已隔离复制固定AVM源码并记录git身份/原文件hash，构建只读解码语法统计工具，解码已有冻结参考码流，统计实际分块、调色板、CfL/MRL、预测块内TX分割和首TX类型。未替换SVT编码后端、未运行本地Codex/OpenCode。原inspection的TX size有partition TODO，因此不把该字段或稀疏raw tx_type map当作完整TX面积统计。先验证新研究decoder重建SHA与冻结参考一致，再据实际工具使用确定下一项基础技术。

## 2026-10-26 17:06 UTC 主候选选择、AVM/AOM实际工具统计

完整曲线最佳仍是RDOQ16点主93–95 PCHIP1.1217015453341768/线性1.1229013325595267，目标1.00/0.85均未达成。stack1已选为下一条完整曲线候选；新增116/113/108/106/105/104/102/96主域点正在分三队完成。逐点状态如下，不计算两点BD，也不把SB J改善视为VMAF支配。

| 候选 | Q | 字节 | 主v1 | NEG |
|---|---:|---:|---:|---:|
| stack1 | 116 | 26634 | 92.462363 | 89.394717 |
| stack1 | 113 | 28750 | 93.032161 | 90.208339 |
| stack1 | 110 | 30666 | 93.847813 | 90.737885 |
| stack1 | 105 | 34506 | 94.273344 | 91.658347 |
| stack1 | 100 | 38808 | 95.139198 | 92.441447 |
| stack2 | 110 | 30331 | 93.487548 | 90.715114 |
| stack2 | 100 | 38661 | 95.130204 | 92.432823 |
stack2 Q110=30331/93.487548/90.715114，相对stack1 Q110=30666/93.847813/90.737885，少1.09%字节但主分下降0.3603；当前优先级证据支持补stack1曲线，fine2组合保留为独立候选。不是宣称fine2全域失败或已违反宽域1%预算。

### 同流RDOQ工作

rdobound四夹具prod/ref/shadow与GCC/固定AVM全部PASS；rdosave四夹具也全部PASS。生产test_m6_rd rdoq既有测试254294检查零失败。partial80的rdobound真实FWD46646、shadow46696；rdosave继续降到46608，skips907、non_dct218、target_forward2892（后者是请求数）。该夹具大部分省去的请求原本已是cache hit，不能据此宣传量级变化。rdobound原生Q100正在进行；完成后同流比较并启动rdosave原生Q100。正式FWD量级目标仍未达到。

### 参考统计器与观察边界

固定AVM隔离decoder已验证两个官方冻结流和两个SVT候选流的重建SHA；实际PB模式读取、intra TX重建完成和IBC块完成事件分别记录。Y/UV owner无重复、可见平面覆盖恰好一次；intra TX加实际IBC重建覆盖闭合。保留原r1 FAIL：只挂intra visitor漏了IBC路径，不删除失败后宣布原版通过。旧UV-on-Y字段仍标UNVALIDATED，不用于结论；实际owner统计替代它。

AVM Q96实际Y7164PB、UV3811PB；对应亮度面积2073600中，UV区域大于Y为1135744、小于Y为241856、面积相同但几何不同182208、几何相同513792。不是仅简单“UV都更粗”。UV有2823个CfL owner，其中derived1543、multi-param837、explicit443。全部实际CfL开始预测前，其整个Y区域已经重建。Q128为Y5438/UV2095，CfL1568。SVT stack1 Q100为Y7782/UV7782，仍强制同几何；它已有derived/explicit/MHCCP，缺口是独立UV树及其正确归属，不能称为没有CfL工具。

AVM两点少量Y IBC为45/53块；palette/DIP在所测点未采用。thin32/thin64实际约7.98%亮度面积；Q96对应intra TX事件576次，非零inverse DCT430、IDTX6、零EOB140；MRL非零231次。该计数不是码率收益。当前SVT薄块MRL域尚未开放，因此需单独验证其规范路径，不能仅据受限工具组合的薄块结果否定形状。

AOM AV1隔离decoder与冻结参考重建一致。inspect公共reader默认raw OBU为Annex B，r1读取失败保留；r2仅添加单帧IVF包封，payload SHA保持不变，并再次解码核验。Q24有7561PB，filter intra实际use标志覆盖19018/129600 mi（14.67%）；Q32覆盖12445/129600（9.60%）。AOM使用4x4/4x8/8x4及4x16/16x4等窄块；paletteY两个点均0。TX size按PB统计且包括skip/default，不等同自适应TX搜索收益。需按AV2实际合法工具核对对应预测能力；不移植不兼容语法，也不从覆盖率推断节省率。

### thinpack隔离实现

基于冻结stack1，复用已存在的thin32/thin64/mixed7语法。完整旧三路线完成后才映射其几何产生独立seed；最多13个去重basic候选，仅thin进入top2，每个seed分别从相同入口运行一轮和两轮完整工具/RDOQ。完整旧赢家在平局时保留；不改变旧ROOT_ASYM路由、不挤掉旧finalists，不放开RDOQ尺寸域。source460文件已冻结，MSVC四臂/GCC构建及随后自动门禁进行中。初次准备脚本错误替换refine标识触发断言，未产出候选，原目录/脚本保留，r2限定标识前缀后成功。

## 2026-10-26 17:31 UTC stack1完整主区间、薄块MRL与SDP/DIP推进

**完整10点2D主93–95曲线：相对AOM PCHIP 1.074377950905682 / 线性 1.075113556565606。1.00与0.85仍未达成。** 这是新的完整主区间成绩，宽域/跨内容尚待验收，不提前晋升正式版本。10点没有主模型或NEG质量倒序；全部固定AVM重建一致。额外审计确认预定十点齐全、同程序/源清单/runner/模型及非QP编码参数；五个固定质量位置均可插值、没有外推。

证据：stack1-main-audit-20261026T172920Z.json，引用的curve-analysis文件及每点job/result SHA均保留。主模型包含实际色度特征，不按Y-only处理。

| Q | 字节 | 主v1 | NEG |
|---:|---:|---:|---:|
| 116 | 26634 | 92.462363 | 89.394717 |
| 113 | 28750 | 93.032161 | 90.208339 |
| 110 | 30666 | 93.847813 | 90.737885 |
| 108 | 32297 | 94.149029 | 91.140316 |
| 106 | 33824 | 94.253944 | 91.599094 |
| 105 | 34506 | 94.273344 | 91.658347 |
| 104 | 35603 | 94.596838 | 91.849225 |
| 102 | 36699 | 95.004799 | 92.105221 |
| 100 | 38808 | 95.139198 | 92.441447 |
| 96 | 42990 | 95.662947 | 92.961978 |

相对AOM，PCHIP五个固定质量比值为93:1.084598、93.5:1.040671、94:1.046300、94.5:1.124078、95:1.092048。区间并非处处相同收益，不能以平均比值隐藏最差点。新增wide190/186、160/130、90/70已分队运行；宽域完成后重新拟合，因为区间外点可能改变PCHIP导数。

### 同流RD优化：原生证据已闭合

rd-native-exact-r2.json PASS：stack1、rdobound、rdosave在2D Q100的码流SHA、重建SHA、38808字节、主95.139198/NEG92.441447、D15497029、Rq12=1268320027、J27205619、lambdaq8=9680和最终赢家完全相同。

真实FWD：stack1 6853929 → rdobound 6824250 → rdosave 6823813。后缀下界减少29679次；保存系数回放再减少437次，总减少30116次（约0.4394%）。rdosave跳过56070次请求，其中大多数原本已有cache命中，所以请求数不能当作真实FWD节省。rdobound实际剪枝13935个完整RDOQ试算，避免26221个后续叶。当前更广工具集合的工作量仍在百万级，几十万目标没有达成。

### 薄块与MRL

thinpack四自动fixture（partial80、10bit444、8bit420、mixed420）全部通过：off/执行后强制旧结果等于stack1，逆序选择等于prod，MSVC/GCC与固定AVM一致。原生Q100/Q110已启动。

thinmrl在thinpack上仅开放8x32/32x8/8x64/64x8的MRL合法域及规范8:1 wide-angle阈值82；不放开薄块非DCT集合。MSVC四臂及GCC严格构建PASS；2304强制码流全部双编译器/固定AVM一致，覆盖非零与multi-MRL、10bit420全部56方向、8bit420/10bit444、跨SB和部分边界。四自动门禁继续；原生比较要等其通过。证据forced-thinmrl-r1/result.json，计数不是压缩收益。

### 修正参考工具统计的解释

冻结AVM对照是cpu-used=9。实际av2/encoder/speed_features.c:464–470,1175–1182在speed>=3关闭DIP搜索并令sequence.enable_intra_dip=0，同时关闭TCQ；speed>=2还禁用扩展/不均匀分区搜索。因此已有cpu9码流DIP为零不能说明该工具没有收益。IST默认开启，未发现相同的硬关闭，实际由TX搜索门限/候选筛法限制；同样不能由零使用量排除它。

本机AOM filter intra实际覆盖已确认；AVM遗留av2_sub_block_filter_intra_taps_4x4只有定义未找到生产调用。符合当前AV2的对应实验是DIP整数矩阵预测（6模式×2转置），已有独立dip1实现任务进行中，先合法预测/语法/真实off率与小夹具验证，未跑原生质量。不能把AV1预测直接写成普通AV2 DC语法。

### 独立UV树规范基础

SDP1源462文件冻结：显式sequence+frame开关，每64SB先完整Y树、再独立UV owner的U/V对；不伪造Y块，不沿用三平面等量索引。单独UV分区CDF/邻居bank，位置仍用亮度mi；处理Y前两层导致的UV根强制分区及UV最小块/方向限制。首版刻意限定full64单位DC/DCT，无CfL/MRL/滤波，仅用于闭环，不作为质量候选。

GCC严格构建及84个SDP强制流固定AVM重建全部一致，包含三格式、static/adaptive、不同Y/UV叶数、强制/自由UV根、最小UV矩形和多SB上下文。MSVC /WX发现rate helper可能未初始化cost警告，失败保留；SDP2正在准备独立修复。下一步先完成双编译器和off控制，再加入真实owner CfL/MHCCP、部分画面边界、新语法率事务与UV搜索。此基础通过不能当作已完成原生SDP压缩功能。

## 2026-10-27 00:11 UTC 独立色度树全语法闭环与原生筛选结果

**目标仍未完成。** 已验证的完整10点2D主93–95比值仍为PCHIP1.074377950905682 / 线性1.075113556565606，未到1.00或0.85。stack1补测现在共17点齐全，宽域精确门槛审计待执行。所有点按原始数据保存。

新增Q192为3908字节、主45.357348、NEG39.719322，覆盖原r4c主分下界；Q190为4065字节、主47.124543、NEG39.630199。二者有NEG倒序，不能删除190或192以制造单调。倒序二点低于原r4c NEG下界40.964051，但仍可能影响PCHIP边界导数，因此宽域必须使用全部原始点重新计算。Q70完成88759字节、主97.931436、NEG95.227301。

### 细长分块与MRL：两点筛选完成

| 候选 | Q | 字节 | 主v1 | NEG |
|---|---:|---:|---:|---:|
| stack1 | 100 | 38808 | 95.139198 | 92.441447 |
| stack1 | 110 | 30666 | 93.847813 | 90.737885 |
| thinpack | 100 | 38778 | 95.338431 | 92.422799 |
| thinpack | 110 | 30745 | 93.596110 | 90.742661 |
| thinmrl | 100 | 38871 | 95.266398 | 92.455918 |
| thinmrl | 110 | 30612 | 93.097015 | 90.716711 |

thinpack、thinmrl四自动门禁均PASS；thinmrl2304强制流已PASS。当前两点显示Q100有改进，而Q110主分下降，不能由Q100单点宣布新工具获益或替换stack1；完整旧赢家保持为主候选。暂缓为这两个组合扩张曲线，将计算资源投向独立UV树/DIP。此结论只针对当前搜索组合，不认定工具本身无效。

### stack1跨内容原生点

| 内容 | Q | 字节 | 主v1 | NEG |
|---|---:|---:|---:|---:|
| 3D | 110 | 94448 | 94.659604 | 91.651877 |
| Real | 100 | 73588 | 94.667904 | 92.880789 |

固定AVM重建均一致。以上是同QP诊断，不能由单点声称同质量跨内容验收完成。holdout20未使用。

### SDP规范路径已覆盖原生所需语法域

SDP2基础84流及off4通过；SDP3增加真实独立UV owner上的隐式/显式CfL、MHCCP、MRL、非DC模式，228个流双编译器/固定AVM逐字节一致。

SDP4扩展8像素对齐边缘、完整既有Y变换集合、Y端CCSO侧信息与DB/CCSO滤波，372流测试保留一次失败：f0-c10-k0-a0亮度完全一致，色度首差(8,8)。查明测试生成器遗漏AVM decodemv.c1568–1572规定：UV基础方向等于共址Y基础方向时继承Y角度偏移。SDP5只修正测试生成器的共址Y模式记录和UV角度预测，不修改SDP4生产代码。

SDP5全部372流PASS：8bit420/10bit420/10bit444，static/adaptive，独立Y/UV树、边缘、完整Y TX、CfL/MH/MRL、UV根权限、U零/V非零、DB+CCSO。SDP4生产off4保持stack1精确一致；本检查点验证SDP4/5全部Source/文件SHA一致。因此当前可以继续码率事务和UV搜索集成，尚未进行原生SDP质量编码，不将强制码流计数作为压缩收益。

当前新增sdp_auto_wrapper_template.inc准备整帧保留旧路线：独立UV提案与旧路线均经过同一DB/CCSO，再以真实完整码流字节和实际重建SSE比较。模板尚未集成，不能声称自动选择已实现或通过。

DIP由AVM规范整数矩阵/六模式/转置/真实use0语法推进，尚无构建或原生收益结论。AOM基本工具参考及现有cpu9限制结论仍有效。

设备执行仅AgentDock，不调用本地Codex/OpenCode，不干预FFmpeg；时间NOT_MEASURED。正式版本仍0.0.5-m5，未晋升候选。

## 2026-10-27 00:40 UTC 全17点宽域验收、独立色度率事务与DIP规范闭环

**目标仍未完成。** stack1全17点2D主93–95同质量AOM体积比为PCHIP **1.0743774136093873** / 线性 **1.0751135565656058**，未达到1.00或0.85。新增邻点改变PCHIP端点导数，故用17点结果替代此前10点1.0743779509056819。所有原始点仍保留。

### 宽域精确审计已完成

审计文件：stack1-wide-audit-20261027T002558948377Z.json，状态PASS_AUDIT_COMPLETED。验证458个源码文件、程序、模型、原图、全部17点身份以及固定AVM重建。跨内容核验纳入各自全部12项身份；其中共享八项一致，额外R25 manifest/closure按实际记录。首版因严格身份数量假设失败的结果和脚本保留，未掩盖或改写。

在原r4c完整共同域、无外推的最差同质量体积比（stack1/r4c）如下；这些数值是最大比值，不是平均节省：

| 指标 | 原r4c质量域 | PCHIP最大比值 | 线性最大比值 |
|---|---|---:|---:|
| 主v1 | [47.10097,97.839094] | 0.9475654723255651 | 0.9249330786392409 |
| NEG | [40.964051,95.057018] | 0.9777191866503101 | 0.9289979628331984 |

四项均通过≤1.01门槛。Q192→190的NEG倒序39.719322→39.630199仍存在且原样保留；虽低于原r4c NEG下界，仍参与完整插值及邻导数。不能删除倒序点来制造通过。3D/Real仍只有同QP诊断，未完成跨内容同质量验收。宽域优于r4c并不等于主区间已胜过AOM。

### SDP6：独立色度率事务通过372流

SDP6从已验证SDP5冻结源码克隆，新增完整TileTree/邻居/独立UV CDF/普通熵模型事务。验证A/B/A非提交回滚、真实UV qcoeff改动、最后V系数无效的提交失败、提交一次及旧cursor拒绝、逐SB率求和等于全帧率、非三元组block顺序。MSVC/GCC逐字节相同，372流与SDP5的码流和重建均相同，重新核对已冻结固定AVM解码产物。

实际UV搜索代码模板和整帧旧路线保留包装已准备，SDP7尚未构建或通过原生验证。候选按真实独立UV owner枚举、恢复同一邻居重建、全Y+UV率和真实重建失真决策；最终旧/新整帧均经过DB/CCSO并按真实完整包字节比较。失败或拒绝也必须累计真实额外forward工作，不可隐去。当前是固定Y的有限贪心UV后处理，不能宣称全局联合最优。新增UV仅DCT且尚无UV RDOQ。

### DIP2：规范预测/语法366流全部通过

DIP1首个自适应概率流失败，原因是六符号CDF的速率参数偏移用了2/3/4，AVM PARA6实际要求3/4/5。DIP2仅更正该表及记录，不改预测或静态概率，保留DIP1失败证据。

DIP2全部366流在MSVC/GCC以及固定AVM中码流/重建逐字节通过，覆盖8bit420/10bit420/10bit444、16×16/8×16/16×8、六模式及转置、边缘和自适应CDF。DIP1关闭路径四门禁也与stack1完全一致。原生RD集成继续，需DIP改变Y重建后重做UV/CfL/MH，并检验非DCT变换语法；尚未把强制流计数算作压缩收益。

### AOM AV1基本工具的因果对照

冻结AOM默认设置在Q24/32都精确重现旧码流、重建和模型分数，再仅关闭enable-filter-intra。固定AV1解码器和同一色度感知主模型/NEG评分：

| Q | 设置 | 字节 | 主v1 | NEG |
|---|---|---:|---:|---:|
| 24 | default_enabled | 34350 | 95.144493 | 91.983045 |
| 24 | disabled | 34245 | 94.964761 | 91.966236 |
| 32 | default_enabled | 22712 | 91.396920 | 88.158730 |
| 32 | disabled | 22811 | 91.353803 | 88.243143 |

Q24关闭后字节略少但主分下降0.179732；Q32关闭后字节略多、主分下降0.043117，而NEG略升。仅两个同QP点不能计算可靠同质量收益，不能把参考编码器收益转记为SVT收益。AVM旧filter-intra表没有对应当前规范消费者，不能将AV1预测器冒充AV2 DC；以合法AV2 DIP/CfL等实现继续补足预测能力。

### 真实FWD与项目边界

stack1 Q100真实FWD6853929；已精确通过的rdobound+rdosave为6823813，仅下降30116次（约0.4394%）。请求数/缓存命中数不是FWD次数，几十万目标尚未达成。正在检查固定最大块cache entry导致小块容量浪费；尚无新FWD减少结果。

仅AgentDock执行，不调用本地Codex/OpenCode，不干预FFmpeg；时间NOT_MEASURED。正式版本仍0.0.5-m5，未晋升候选；holdout20未使用。

## 2026-10-27 01:05 UTC SDP实际搜索与DIP原生门禁检查点

**目标仍未完成。** 主结果仍是stack1全17点93–95同质AOM体积比PCHIP1.0743774136093873 / 线性1.0751135565656058，宽域验收保持通过，1.00/0.85尚未达到。没有用强制码流通过代替压缩曲线。

### SDP7→10：实际搜索已经开始规范解码闭环

SDP7首次严格构建保留两类失败：GCC测试头test_util.h引入未使用static计数器，MSVC对uint32头字段与enum source chroma_format比较报C4389。SDP8只删无用测试头；SDP9加显式uint32_t转换后双编译器构建通过。

SDP9第一次API测试还未进入新色度搜索便返回INVALID_PARAMETER，原因是fixture把快速rd_mode=3的tree_finalists设成8，而现有fast契约最多4。SDP10只把fixture改成真实native用的4，全部Source/与SDP9字节相同。此类失败未改写为成功，脚本、日志、源码快照均保留。

SDP10冻结478文件；MSVC、GCC构建和force/retained-old/reverse三种控制构建完成。GCC第一次构建在46/48链接前以9退出、无编译诊断；使用原源码及原配置单线程续完最后两个链接成功，原构建日志保留。

当前完整API矩阵状态：PASS，已写入最终解码case数：8。独立提前检查的128×128 10bit420和80×88 10bit420两条实际搜索流均已fresh固定AVM解码且重建逐字节一致；其C fixture在导出前已验证实际反向候选枚举结果、Y plan/像素不变和末SB容量失败的完整回滚。完整矩阵包括4:4:4与8bit，状态以validation/sdp10-native-r1/result.json为准。

实际App整帧包装的单图强制开启检查与完整控制验证继续：旧、新均经过DB/CCSO，以真实完整packet字节和最终重建SSE比较；强制开启仅用于验证，不作压缩收益结论。新增UV只用DCT、尚无UV RDOQ，仍是固定Y的有限贪心搜索。

### DIP3：下界事务、真正关闭路线和实际开启路线分开核验

DIP3冻结467文件，MSVC/GCC严格构建PASS。新增DIP Y-prefix A/full/A、Y率≤完整率、commit一致性等366项事务通过；所有输出码流/重建逐字节匹配已冻结的DIP2，并重新核对DIP2真实固定AVM decoded文件。此处复用的是完全相同码流的已重哈希解码证据，不声称重复调用了366次decoder。

显式SVT_AV2_M6_DIP=0的四个实际App输入与stack1完全一致：partial80、chroma444、eightbit、mixed420。开启路线独立做MSVC/GCC、M6_Y_BOUND_VERIFY=1完整搜索对照及fresh固定AVM解码。当前开启矩阵状态：FAIL，完成case数：4；最早的partial80虽全部通过但selected=0，不足以证明实际DIP赢家被覆盖。整个native gate要求至少一个真实自动选中的DIP块，不会强改统计。

DIP3仍是显式forced-on实验；on内部旧route包含use0成本，不冒称真正sequence-off保底。DIP4正在准备两个独立codec context重跑完整off/on、滤波和打包，再严格按真实J选出整帧赢家。亮度重建缓存同步当前CfL/MH元数据的修正已加入DIP3；Y像素/系数缓存保持相同。

### 精确缓存下一步

memo1按相同168173752 B预算设计按实际块大小存残差/原始coeff/qcoeff并精确dequant恢复dqcoeff的缓存。使用完整key和逐样本残差比对，全局LRU，保留plane身份；压实只移动payload，不改RD状态。此刻尚未构建或产生native FWD收益，不能把容量估算当成减少次数。SDP新增候选的全部额外forward也必须记录，拒绝方案不抹去工作量。

仅AgentDock，无本地Codex/OpenCode；FFmpeg不干预，时间NOT_MEASURED。正式版本0.0.5-m5，holdout20未用。

## 2026-10-27 01:34 UTC SDP完整控制与1080p零收益检查点

目标仍未完成。stack1全17点93–95同质AOM体积比仍为PCHIP 1.0743774136093873 / 线性1.0751135565656058；宽域主/NEG双插值验收通过，跨内容仍只有同QP诊断。不能把1.074记成1.00，也不能把新增工具规范通过记成压缩收益。

SDP10的4输入正反共8项API输出全部fresh固定AVM解码exact，MSVC/GCC相同；多SB末尾容量失败验证完整重建回滚、输出清空和真实工作量保留。实际App强制mixed420路线也通过，旧5091/new5082 B，J3772438/3760131，只是小图诊断。

默认/force/retained-old/reverse四臂、GCC production及4输入完整控制PASS。r1在编码开始前因WSL cat stdout恰8192字节截断CMakeCache而失败；r2只用awk读取CMAKE_HOME_DIRECTORY和CMAKE_C_FLAGS两行，仍严格检查源码目录和全部宏。原失败脚本、日志、结果保留；subset hash已明确命名gcc_cmake_key_lines_sha256。对最终选择旧路线的reverse控制，未导出的新proposal只有J/长度/率/块数对比，不冒称该未选码流逐字节已验证；API矩阵的实际正反码流已独立验证。

1080p两点均通过固定AVM和主/NEG评分，但自动选择旧路线，码流/重建与stack1完全一致：

- Q100: 38808 B / 95.139198 / 92.441447；新proposal 39220 B，J26909865高于旧26803497。
- Q110: 30666 B / 93.847813 / 90.737885；新proposal 31051 B，J36069226高于旧35922554。

真实额外FWD分别4052534和3093310，最终总FWD10906463和8936526；拒绝proposal的工作量未抹去。本版独立UV树暂无原生体积收益，不扩展同版完整曲线。下一版补UV系数RDOQ，保留完整旧整帧路线；局部SB严格改善不等于相对SDP10 scalar整帧单调。

DIP3 native-on四个输入的MSVC/GCC/full-bound-verify/fresh固定AVM逐项通过，但全部selected=0，整体门禁明确FAIL: No native DIP winner exercised。保留失败，继续DIP4完整sequence-off/on独立context选择及真实原生DIP赢家输入，未以强制fixture替代。

memo1同168173752 B预算的变长精确缓存仍在准备，尚无原生FWD收益。基础量化核查确认固定AVM当前encoder默认也使用equal_ac_dc_q=1、base Y/UV deltas均0；不能把这一点误报为SVT缺失的AVM默认技术。

所有操作仅AgentDock；不调用本地Codex/OpenCode，不干预FFmpeg；时间NOT_MEASURED。正式0.0.5-m5、holdout20未用。

## 2026-10-27 06:04 UTC 新工具与真实FWD检查点（r14）

目标仍未完成。通过完整17点验收的stack1主93–95同质AOM体积比仍为PCHIP 1.0743774136093873 / 线性1.0751135565656058；宽域主/NEG双插值通过，跨内容仍限单点诊断。没有把新工具局部J下降、同QP字节下降或小图规范通过计成完整曲线收益。

### 精确工作量优化

memo2已通过MSVC/GCC、四输入与固定AVM门禁。相同168173752 B预算，Q100真实FWD从6853929降到5853188，packet/recon/最终D/R/J/赢家不变。prefwd1另经完整1080p VERIFY复算通过，生产FWD进一步降到5815129；128138次over_bound只对应38059次实际FWD节省，二者不混淆。仍未到几十万。prefwd2增强非零EOB/末位base/sign下界已准备，正在独立构建，未记收益。

### 新工具证据与去留

SDP11加入UV RDOQ后，API、实际App、默认/off/force/旧/reverse与GCC控制通过；原生100/110仍选择旧整帧路线，stream/recon/评分完全同stack1，不扩同版曲线。DIP4补齐真正native赢家以及独立sequence off/on控制，完整规范门禁PASS，但原生两点匹配主质量均约1.6–1.9%差，保留实现、不提升为领先路线。

fine16选择性16到8细分覆盖7000几何方案，480个强制码流均MSVC/GCC与固定AVM一致。原native四例全部控制通过但无赢家的FAIL报告保留；独立levels见证选中plan5095，J11253降到10080，完整控制PASS。anglefull完整48角差与eob1精确suffix及末位联合优化均通过四输入原生控制、反向遍历、MSVC/GCC、固定AVM；eob1实测联合候选获胜。

1080p初筛如下，数值依次bytes / 主 / NEG；仅单点，不是BD曲线：

|路线|Q100|Q110|
|---|---|---|
|stack1|38808 / 95.139198 / 92.441447|30666 / 93.847813 / 90.737885|
|fine16|38416 / 95.337151 / 92.440082|30110 / 93.593052 / 90.778757|
|eob1|38302 / 95.187421 / 92.484201|30201 / 93.272726 / 90.653706|
|anglefull|38721 / 95.134710 / 92.388474|30640 / 93.343840 / 90.769414|

fine16 Q100主同质量初筛相对stack1为0.94658/0.95234，Q110为1.00498/1.00186；eob1 Q100为0.97426/0.97771，Q110为1.02981/1.03067。二者各补104/106/108/113来形成主区间曲线，不能只取有利Q100。anglefull Q110约1.0398，暂不扩曲线。

CDEF分区图强制小图通过；r1自动控制因WSL共享stdout/stderr句柄导致日志前缀覆盖而FAIL，r2改分开捕获后所有控制通过，但没有混合map赢家，标记COVERAGE_INSUFFICIENT。原生1080p进一步暴露固定AVM解码失败EOB token。代码复核定位到四行CDF training参数中间offset误写1，固定AVM宏应展开为2；保留失败源/流/日志，独立cdefmap2只改四个字节，必须重新跑候选、长context负例/正例和native控制。不采纳坏流体积。

### AOM基础能力与下一步

已审计固定AVM/AOM预测、变换、系数与分块基础。AOM q24/q32已有统计的严格下界显示33.25%/39.24%预测块使用小于预测块最大尺寸的变换；不是逐块联合精确计数。SVT当前此项缺失。独立TX32细分原型采用32预测块内四个16变换，完整SELECT语法、因果重建、独立UV及内部TU去块；先通过全NONE与强制拆分解码，再接真实搜索及旧整帧回退。

小尺寸IDTX原型及312流fixture已准备；idtx1生产代码编译通过到对象阶段，但fixture缺transform/quantization公共include导致双编译器build FAIL。保留冻结失败版，idtx2仅修测试目标依赖后重新构建，尚未宣称规范通过或收益。

所有执行与写入仅AgentDock；不调用本地Codex/OpenCode、不干预FFmpeg。时间NOT_MEASURED；正式0.0.5-m5与holdout20未动。

## 2026-10-27 06:41 UTC 主曲线与基础工具检查点（r15）

1.00与0.85均未达成。已完整验收stack1的17点主93–95同质量AOM比仍为1.074377414/1.075113557（PCHIP/线性）。新增结果仅按对应证据范围报告，正式0.0.5-m5不变，holdout20未使用，时间NOT_MEASURED。

### 主区间新结果

fine16六点（113/110/108/106/104/100）有4个区间内点及双侧包络，主同质量AOM体积比1.055501629/1.055945014，相对stack1为0.982430955/0.982170681。宽域尚未完成，正在补到17点；后补Q105为33853B/主94.386195/NEG91.735630，相对Q106发生局部主质量倒挂，必须保留并重新积分，不沿用六点数字作最终验收。

eob1同六点完整主比1.093124830/1.093528248，相对stack1为1.017449563/1.017128137。因此停扩该版，不把Q100局部有利结果当整段收益。DIP4/anglefull/SDP11仍保留实现与失败或回退证据，未提升为领先路线。

### IDTX与变换细分

idtx4修正的只是夹具矩形plan映射；idtx1缺fixture依赖、idtx2关闭宏unused参数、idtx3夹具宽高断言失败均保留。idtx4完整312码流，MSVC/GCC各on/off四组逐字节一致，所有不同码流fresh固定AVM重建一致。真实搜索四输入全部PASS，实际IDTX赢家共196；相对off的字节变化依次为-58/-269/-43/-212，完整帧J也均下降。实际FWD反而分别增加1419/20595/1295/1940，不能算剪枝收益。1080p Q100为38659B/95.113333/92.413593，尚需同质量曲线，Q110在测。

txsplit1语法层636码流全部双编译器/fresh固定AVM PASS（498 SELECT、138 LARGEST；138全NONE对照、360实际split）。r1仅WindowsPath传给WSL导致exit127，原报告保留；r2修runner路径后通过，源码不改。独立txsplit2追加owned帧回放/搜索及后滤波完整包J回退，467文件已冻结，开始构建。旧RD、memo预算、核心变换/coeff和stage1语法均不变；实际新增FWD分别计Y/UV，非CfL/MHCCP色度保留旧RDOQ系数。尚未宣称原生split选中或压缩收益。

### CDEF与精确下界

cdefmap2仅改四字节CDF训练参数，强制小图通过，四输入自动搜索全部控制通过但未覆盖实际混合map赢家。controls-r1的WSL实例终止证据保留，r2运行通过。60单元长序列已复现旧all_on固定AVM解码失败，并得到修复MSVC/GCC均精确解码的正例；剩余分支及native1080控制尚在执行。坏流体积不纳入任何曲线。

prefwd2的30次小编码（旧prod/旧VERIFY/新off/prod/VERIFY/GCC，五设置）全部码流/重建/D/R/J/赢家及对应实际工作不变量通过，但新增拒绝和FWD节省均为0，保留INVARIANTS_PASS_NO_ADDED_REJECTION。1080p prod/VERIFY正在执行。独立下界审计r1遗漏cl /utf-8、r2对六种4宽矩形请求了当前不支持的full-TX signalling，均保留失败；r3仅把这六种完整writer对照改为已有合法implied-DCT set2，矩阵/下界断言/计数不删，源码未改，结果待定。

后续组合使用新名称stack2typed1，避免覆盖旧stack2实验。以fine16/memo2为底，CDEF只提取相对stack1的真实功能增量，不能带回旧cache/RD。IDTX与prefwd组合必须补真实PB flag、typed skip上下文及CDEF prefix顺序，并经过生产/VERIFY完整对照，不能直接叠加未经证明的下界。

所有命令、读取与写入仅AgentDock；未调用本地Codex/OpenCode，未干预FFmpeg。

## 仍需完成

1. 完成fine16的17点主/NEG宽域检查，保留原始倒挂；继续评价IDTX及组合。
2. CDEF长序列和native控制、txsplit2 owned回放/原生搜索/完整packet回退门禁。
3. prefwd2独立下界审计及1080p实际FWD/全量VERIFY，安全合并typed路径。
4. 完整同质量曲线先到1.00，再到0.85，并完成跨内容回归。

</details>


<details>
<summary>008 · M6-intra-speed-20261025.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-intra-speed-20261025.md`  
原文件 SHA-256：`556d487dc21e07034275baaa92f06fbe79cee7d914609d1df04e613668efd953`

# 单帧速度优化（已按用户要求暂停）



2026-10-25 本轮测试收尾后停止，未启动新的性能队列。暂停时的体积、隔离耗时、逐步优化过程与未完成项见 [暂停报告](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/docs/task-reports/M6-intra-speed-pause-20261025.md)。下文保留研发过程，各段“正在/尚未合入”等表述是当时的历史状态；以暂停报告与本文“最新状态”为当前口径。



用户在上一轮暂停后明确恢复速度开发：体积以完成并审计的 r4c 为基线，同画质体积最多回退 1%，优先接近 AOM cpu0 ALL-INTRA 单线程 2×耗时。继续四路队列，先排除重复计算，再验证分层筛选。此报告不替代上一轮冻结的暂停报告。



基线为 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-r3-20261024/integrated-r4c-source` 的 439 项清单，SHA256 `b59ae1c34656268a4857e320fd158934fd92476956fb01bc36fa7d6337b0aea0`；程序 SHA256 `76810d3c77e0a358a99a50acd513d86531e0fd9d84a82011809fee4b76eed9d3`。不混入 MAIN 后来新增、尚未完成原生质量评分的工具。



本轮试验源码、程序和测试置于 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025` 独立副本，冻结后不热改。已通过验证的等价优化和显式fast3入口已合回MAIN，默认旧0/1/2路线保留；后续未过门槛的筛选仍是私有试验。正式版本仍 0.0.5-m5，holdout20、I-GATE 和正式五次隔离速度验收尚未执行。



## 最新状态



- MAIN现已合r26八个生产文件：既有r23基础上加可移植AVX2量化、intra_round2一行非负除法改移位，以及Y SADtop2+进入模式。默认旧rd_mode0/1/2保留，fast仍须显式`--rd-mode fast`。当前推荐已有参数组合为longrect、`--tree-refine rd`（r28，只留全局两棵树）和冻结其余全工具；r26的wide结果保留作对照。

- 当前MAIN新构建程序为 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/main-r26-verify-build/bin/SvtAv2EncApp.exe`。版本仍0.0.5-m5，未覆盖安装的Bin/旧build。main-speed-r26-merge.json及main-speed-r26-verify.json锁8文件before/after、备份和程序身份。

- MAIN fresh专项4/4 PASS及新原生Q110全流/重建/全部最终统计/fixed exact。portable内部header/state变更独立stage完整107/107 PASS（1014.90s）；最终组合另有20CLI/10fixed、freshSAN4/4与六原生点字节桥接，未重复宣称最终MAIN又跑全107。

- 当前六点VMAF BD对r4c−2.535753%，共同域4097网格最差仍减少1.645721%，通过严格1%门槛；PSNR-Y/U/V BD−1.287436%/+2.816501%/+6.321110%，不是所有指标改善。对AOM VMAF BD+8.932677%，VMAF90体积25482.59B，仍比AOM大16.6509%。

- 团队暂停其它CPU任务、CPU8完整暖机后的原生隔离：r23 CPU中位37.953125秒，r25(quant+round)35.390625，r26(+Y2)33.390625；r26共5次，其余各3次，总降低12.0214%；r26墙中位33.447552秒。全部输出核验通过，根目录root-integrated-native-isolated-r2/result.json。

- 同窗口冻结AOM3.15.1/cpu0/allintra/threads1 Q24/Q32各暖机一次+5新进程，CPU中位12.390625/10.375。两个质量锚点夹住当前AV2 V92.441237，局部线性时间插值AOM11.621942秒，对应AV2约2.873068×，仍需约30.4%降耗接近2×。只是局部锚点诊断，完整曲线五次速度/I-GATE/holdout20仍NOT_RUN；不拿旧硬件/旧负载耗时作本次倍率。

- r28现有global2参数已通过六点质量、逆序、真实故障重试和SAN：VMAF BD相对r4c−2.309018%，共同域最差仍减少1.273664%；PSNR-Y/U/V BD−0.828760%/+3.518636%/+4.716145%。同一个MAIN程序、CPU8、各暖机一次后3对交错隔离，CPU中位wide33.187500秒→global2 31.453125秒（−5.225989%），墙中位33.213009→31.565879秒。八次输出分别与各自原六点流、重建、全部最终统计一致，证据root-r28-wide-native-isolated-r1/result.json。AOM未在该窗口重测，不能把上一窗口锚点换算当正式同窗口速度验收，2×仍未达到。

- Y1/r27因grid最大+3.105345%淘汰；r19/r22旧失败保持。胜叶现场复用128新CLI隔离group10复测仅−1.20%/−4.12%，未双3%封负。DCT64跳过丢弃高频11行私有候选数值/SAN通过但隔离group10无净收益（+0.69%/0%），封负不MAIN；r29两阶段basic因grid+2.281894%淘汰。

- 最后队列已停止：AVX2递归odd-dot与省纯预测中间frame写入均完成有限正确性验证，性能尚未启动；当前叶U后的J下界observer只准备源码，未编译/编码，无命中率实测。三项均未合MAIN。用户要求本轮结束暂停，后续测速、提前终止接入和新候选均等待恢复指令。



## 已确认的开销



r4c 原生 Q110 有 843 万 RD 候选、3.40 亿 forward 回调。逐叶候选反复重建后续叶和完整 SB 码率；最多 56 棵树都做基本模式细化。分区递归还逐节点重复校验全帧计划（1080p 为 510 SB）。Release 构建已经启用优化，并非误用 Debug。



128×128 裁块的独立 QPC 1/127 调用抽样保持完整流、重建及候选轨迹一致：build_path 约占墙时 70%，其内 DCT 正向回调约 21%、逆向回调约 37%；外部 tile rate 约 16%。回调包含布局、量化等工作；这些是包含子调用的时间，不能相加当独立份额。WSL1 gprof 没有累计采样时间，只能使用调用数，不将其作为时间占比证据。



## 等价优化



- DCT 对称求和和四路 ADST 累加：单文件内核对照、固定官方 oracle、MSVC 与 GCC ASan/UBSan 通过；专项 CPU 吞吐约提升 1.2～2.2 倍，尚不能外推为整帧倍数。

- 基本模式前缀增量推进：431567 条完整候选记录及流、像素、系数、计划一致。64/128 裁块三次 CPU 中位数分别减少约 4.5%/2.7%。

- RDOQ 非负码率下界：只跳过不可能严格改善的 writer 调用，保持继续截尾和全部候选语义。writer 次数减少约 62%，在整体中的收益很小。

- 分区校验移到公共入口，私有事务只校验本次改动的 SB。四项专项 CTest、两个原生裁块流/重建逐字节一致；5000 次相同 SB 率试算在 1080p 几何下，单次 CPU 约 0.469→0.250 秒。此为微基准。

- 上述四项合并版 `integrated-exact-r2-source/build` 在两个裁块逐字节保持原 r4c 输出，CPU 单次约减少 24%。旧 r4c 单测中非法分区值 7 已成为合法值，原失败保留；外置改为 8 的负例在旧/新程序各通过 8028 项检查。

- 全零反量化系数跳过 DCT inverse：输出、padding、错误路径和计数器对照通过；128 裁块五次配对 CPU 中位数约减少 5.6%，有并发载荷，不称正式速度验收。

- DCT 常量残差复用第一遍变换：43656 项、1.788 亿系数/padding 元素穷举对照与 sanitizer 通过；两个裁块保持原 r4c 字节。MRL 因果前缀复用已通过完整轨迹对照。UV 候选复用不变 Y 链的缓存已在独立副本验证，尚未合 MAIN。

- 熵概率成本化简保持完整 raw CDF 非法检查，943 万项对照、sanitizer 与四组编码轨迹通过；整体收益尚无稳定证据。



MAIN 合并记录为 `speed-r1-20261025/main-exact-r2-merge.json`：七个生产文件和一个已过期负例边界的修正。合并前 stage 完整 CTest **107/107 通过**，三种像素格式的 thin64 公开命令与原 f2 码流、重建逐字节一致，固定官方解码像素一致。保留 MAIN 的新增分区合法域，未用旧文件覆盖新功能。



一维 inverse 的尾部零频率裁循环已作为下一项等价内核合 MAIN，记录 `main-exact-r3-merge.json`，只新增该单文件 hunk，保留已有 forward 常量优化。独立数值域、官方 oracle、sanitizer 与三项 TX 单测通过；MAIN 新 stage 的三项 TX 单测和三种格式 thin64 码流/重建桥接、固定解码均通过。128 裁块配对 CPU 中位数减少约 13.2%，但有并发载荷且 dense 微核部分回退 1%～3%，不能累计微基准倍数。首次新 shell 缺少 ctest PATH 未运行测试，激活环境后 r2 重试通过，原日志保留。



## 筛选试验



使用同一 1080p 2D 原始帧，无缩放抽取预定三个 128×128 裁块，坐标为 (256,192)、(896,448)、(1536,768)。六 Q 沿用 190/160/130/110/90/70，固定官方解码逐像素核验，VMAF PCHIP 同域匹配。裁块用于快速淘汰，不能替代原生整帧 1% 门槛。



首版按粗种子代价每域仅留四棵树，CPU 约为基线 13%～17%，但第三裁块 VMAF BD 回退 16.575%，已拒绝。原始失败结果保留，没有为了速度启用。



局部模式 top1/2 预筛、完整后缀真 SB J 决定是否接受的版本，三裁块对 r4c 的 VMAF BD 分别为 −0.05395%、−7.96608%、−1.78023%，CPU 合计比约 0.2775/0.3133/0.3032。PSNR-Y/U/V 全部保存；个别色度指标存在回退（例如中部裁块 V 通道 +2.684%），不隐藏。该版本保留为质量候选，但原生整帧和正式 2×速度尚未验收。



下一队列用逐叶实际量化/重建的因果局部搜索构建整树，再用真实 SB 代价挑入围树，改善粗种子排名遗漏；保留原完整搜索作对照。最终仅接受原生同质量体积回退不超过 1% 的版本，再进行同质量独立暖机与五进程计时。



## 原生整帧结果（最新）



固定同一 1920×1080 10b420 单帧、六 Q、原 r4c 模型与官方解码。以下 CPU 为并发诊断，未达到同质量正式速度验收条件。



| 候选 | Q110 字节 / VMAF | Q110 CPU 秒 | 六点 VMAF BD 相对 r4c | 结论 |

|---|---:|---:|---:|---|

| 冻结 r4c | 33565 / 92.138170 | 2887.266 | 0% | 质量基线 |

| r4：完整 56 树、局部 top1/2 | 33579 / 92.240438 | 580.750 | +1.557028% | 超过 1%，不合入筛选 |

| r5：因果局部选叶、少数树完整细化 | 33750 / 92.108786 | 352.547 | +2.443129% | 超过 1%，不合入筛选 |

| r6：种子树 top4/域再局部筛选 | 34052 / 92.001560 | 123.375 | 未跑六点 | 单点已有质量风险，不启用 |

| r7：r6 再加 Y SATD top4 | 34166 / 92.016239 | 165.422 | 未跑六点 | 无原生加速信号，不扩展六点 |

| prefix：因果选叶用真实累计熵码率 | 33172 / 92.262802 | 423.891 | −3.536955% | 通过本帧 1% 质量门槛，未过速度目标 |

| r9：linear-r2 + inverse sparse | 31448 / 92.447320 | 184.094 | −7.327564% | 通过本帧 VMAF 主指标门槛，V 色度有代价 |

| r10：r9 再做 basic Y-SAD top4 | 31663 / 92.421479 | 177.781 | 仅单 Q | forward 减少 33%，整帧 CPU 净收益未隔离验证 |

| r11：r10 再缓存 UV 搜索中的固定 Y | 31663 / 92.421479 | 135.141 | 与 r10 同流，未跑六点 | 原生流/重建/除 forward 外统计逐字节一致 |

| r12：Y-only 分层 + UV-Y cache + sparse | 31616 / 92.396912 | 126.844 | −6.636289% | 原生六点通过 VMAF 主指标，色度有代价 |

| r13：r12 再加普通 UV SAD top2 | 31806 / 92.371124 | 110.094 | −5.860741% | 原生六点通过 VMAF 主指标，色度有代价 |



r4、r5 六点审计分别在 `native-r4-six-r1/audit-summary.json`、`native-r5-six-r1/audit-summary.json`。两者 Q110 显式复用同一冻结程序先前单点，其余五点为新进程。逐点重算像素 SSE、固定解码与重建一致，并核对码流体积及 VMAF 文件。r5 的 VMAF80/90/93 同质量体积分别回退 1.8836%/1.2270%/0.5151%；共同域密网格最大回退 3.6653%，不能用高质量单点掩盖失败。



低码率段 top2 修复已完成：仅用户 QP≥150 的分支重测原生 Q190/160，其余四点显式复用未改分支的旧 r4 结果。复合六点 VMAF BD 回退 1.467511%，仍超 1%，已停止该路线。VMAF80/90/93 三点虽全部过门槛，共同域网格最大回退仍有 3.2192%，未据三点改口。证据在 `agent-coeff/r4-lowq-top2-r1/native-points/audit-summary.json`。



因果搜索的另一项修复是使用真实熵前缀累计码率，避免默认 CDF 近似把本来好的路径淘汰。真实前缀 API 已通过旧 whole-SB writer 的逐字节 CDF、邻居上下文及码率对照。



真实熵前缀集成的三裁块六 Q 已完成，(256,192)/(896,448)/(1536,768) 的 VMAF BD 相对 r4c 为 −7.9989%/−4.4495%/−7.7761%，明显修复旧因果局部搜索在第三块的回退。中部 V 色度 PSNR BD 仍回退 7.5185%，完整指标保存在 `crop-curves-prefix-r1-summary.json`，不能将裁块结果当作原生验收。该集成的 13 组正常原生驱动、4 组 sanitizer、4 组固定解码、3 组公开命令与实际新分配故障路径均已验证。



真实熵前缀的**原生六点已完成并独立审计**（`native-prefix-six-r1/audit-summary.json`）：VMAF BD 对 r4c −3.536955%，PSNR-Y/U/V BD −2.61908%/−1.63118%/−1.16004%；对 AOM VMAF BD +7.872241%。VMAF80/90/93 同质量体积对 r4c −3.52995%/−3.07552%/−2.43535%；共同域 [44.924927,96.040809] 的 4097 点网格最差仍减少 0.253806%，没有用均值隐藏局部回退。六点 CPU 为 264.453/309.563/371.578/423.891/479.594/472.969 秒，属并发诊断。完整曲线显式复用同冻结程序 Q110，其他五点为新进程；输入、程序、码流、解码像素和指标文件全部核验。此为本开发帧质量通过，不能推广为 holdout 或 2×速度通过。



**r9 原生六点也已审计完成**（`native-r9-six-r1/audit-summary.json`）：VMAF BD 对 r4c −7.327564%，共同域最差仍减少 3.341717%，主指标 1% 门槛通过。PSNR-Y/U/V BD 分别 −7.11656%/+0.61764%/+2.47704%，不能将主指标通过说成所有质量指标都改善。对 AOM VMAF BD 为 +2.973075%；VMAF90 匹配体积仍比 AOM 大 11.1013%，不能将 BD 均值当作该点结果。六点 CPU 为 197.516/209.094/228.438/184.094/257.578/266.781 秒，Q110 复用，均未作隔离五轮速度验收。



r6 私有阶段诊断在两个 128 裁块保持输出和全部最终统计一致：tools 占整帧 forward 58.15%/64.82%，占墙时 51.82%/56.94%；basic 占墙时 37.21%/33.60%。这些阶段包含其子调用。下一步将 angle/MRL/TX/CfL/MH 的逐候选完整后缀重建改为目标叶评分和前缀提交；首版保留原末尾 RDOQ。2×目标尚未达到。



完整动态分区序列仍需保留：将真实熵前缀与旧种子每域 top4 组合成 r8，第三裁块仍回退 8.7922%，已在三裁块阶段淘汰，没有继续跑原生整帧。linear-r1 三裁块六点对 r4c 的 VMAF BD 为 −8.24853%/−6.34093%/−7.05597%；linear-r2 跳过第二轮 screened 基本模式细化后为 −2.93082%/−3.24519%/−4.91397%，这一步是有损搜索缩减，不能称严格等价去重。r1/r2 已通过三格式、逆序、真实深层故障重试、SAN、部分边界和固定解码验证。r9 为 r2 加已通过 MAIN 的等价 inverse sparse 单 hunk，两裁块对 r2 逐字节桥接和固定解码通过，原生结果见上表。



linear-r2 两裁块新的阶段诊断显示热点已改变：basic 占墙时 83.38%/75.91%，forward 91.11%/74.76%；linear 工具本体（不含 RDOQ）仅占墙时 6.59%/6.80%，RDOQ 占 2.81%/10.70%。局部代价与真实 prefix writer 属子层，不能再与 basic 相加。



Y-SAD top4 试验保留全部动态树和进入 Y，只在完整 SB 的 basic Y 模式先做便宜预测再以真实熵码率选优；UV 九候选保持。三裁块六点 VMAF BD 为 −7.07622%/−5.34828%/−4.48452%，生产仅加 25 行，三格式/逆序/故障恢复/SAN/partial 旧域均通过。加 inverse sparse 后 r10 两裁块与冻结 Y4 流/重建逐字节相同，原生 Q110 固定解码通过，forward 从 r9 的 32338467 降到 21659460；CPU 为 177.781 秒，对 r9 单点 184.094 秒的净收益尚未在相同并发/频率下隔离测定，不能将调用数降低当作耗时降低。



固定官方 AVM 对照封在 `agent-tx/avm-partition-readonly-r1/REPORT.txt`，两份固定源码清单及 22 个关键文件已核同：真实 counterpart 仅复用相同因果入口的首子块链，仍需 dry-run 重建上下文；几何分区记忆会限制搜索，不能冒称等价。官方纯帧内先 Y-only 选 Y，再只给胜出 Y 搜 UV。当前两项独立队列分别做 UV 搜索中不变 Y 的等价重建缓存，以及 Y-only 分层决策的最小内部熵率支持。后者是有损决策，需独立曲线，不能套用前者的字节一致结论。首叶跨树候选 SSE/deltaR 复用在严格范围可证明，但还未实现，先量潜在收益。



上述两项均已完成有界正确性验证并独立组合：固定 Y 缓存只改 RD 的 40+/2−，两个评分 helper 逐字不动，UV 中继续试写完整三平面真实 prefix 码率；Y-only 分层新增内部 Y 子集率，独立对旧完整 writer 的逐符号 Y 子集检查 812644 项及旧 m6_rate 131444 项通过，候选子试算不提交。r11 完整 1080p Q110 与 r10 码流/重建逐字节相同，除 forward 外所有最终 CLI 统计一致，fixed 解码通过。r12 组合已与冻结 Y-only 的两个裁块字节桥接，原生 Q110 fixed 通过；三裁块 VMAF BD −5.99348%/−8.57114%/−6.71934%，原生六点已完成，五个非 Q110 点以五进程并发采集，非正式速度。



UV 层再用普通 UV 的真实预测 SAD 保留 top2，始终保留进入 UV 和隐式 CfL14，不把 CfL 的 DC 占位当作真实 CfL 排名。独立 29 行候选三裁块 VMAF BD −3.16462%/−9.55881%/−6.04980%，色度 PSNR 回退仍保存；尚未取原生六点，不能套用 r12 的结果。



公共前缀完整观测（`agent-tx/basic-prefix-count-r1/HANDOFF.txt`）在两裁块发现重复 raw SSE/leaf deltaR 全部相同，完整 key 包含每个已选前叶的几何与模式/工具状态；重复占 local_cost QPC 44.33%/45.03%，占全帧 forward 约 30%。首叶本身仅约 6%～8% 帧时间。正在设计只限 causal basic、工具与 saved 状态为零的有界 score memo：当前树累计 R 与整数 J 仍重新计算，不缓存 winner，不复用任意相同几何或 suffix。



两项低收益方向已用数据停止：真实熵 prefix 的 TileTree/CDF 每次合计复制 33536 字节，但 1/127 抽样显示复制只约占测试裁块整帧墙时 1.49%，不据逻辑字节量引入事务回滚架构；一维 forward 常量向量在两个裁块仅命中 1.15%/1.55%，没有新增常量快路径。诊断保持原流和重建一致，均非正式速度验收。



结果目录中的失败 attempt、原始日志、程序和清单均保留；组合封包第一次插入 hunk 合并失败，误构建的原基线已标 `FAILED_INTEGRATION_INSERT_HUNK`，不计入候选。





r12 原生六点独立审计见 `native-r12-six-r1/audit-summary.json`：VMAF BD 对 r4c −6.636289%，对 AOM +3.641074%；PSNR-Y/U/V BD −5.70354%/+1.10498%/+2.60895%。VMAF80/90/93 同质量体积对 r4c −7.72633%/−8.72722%/−9.21431%，共同域 4097 点网格最差仍减少 2.47355%。VMAF90 体积仍比 AOM 大 11.1165%。六点 CPU 为 135.750/140.078/149.969/126.844/181.234/199.594 秒。Q110 显式复用，其余五点为同冻结程序新进程；固定解码、像素 SSE、体积、VMAF 和程序身份全部通过复核。2×速度尚未达到。



粗细两层 basic 搜索 `basic-twolevel-r1` 虽保留完整树枚举，粗决策仍改变动态计划及入围池，第三裁块 VMAF BD 回退 4.20416%，已淘汰，不扩原生、不继续调 topN。公共前缀 score memo 在独立副本完成两裁块轨迹、分 kind 原始 SSE/deltaR、arenaNULL、容量饱和及深故障对照和 sanitizer；等待独立代码审查后再组合内核验证原生同流。当前 MAIN 仍只含上列等价优化，未开启这些私有筛选。





r13 原生六点独立审计 `native-r13-six-r1/audit-summary.json`：VMAF BD 对 r4c −5.860741%，对 AOM +4.848444%；PSNR-Y/U/V BD −5.50401%/+1.51490%/+3.46881%。VMAF80/90/93 体积对 r4c −6.57758%/−8.16854%/−8.25650%，共同域网格最差仍减少 3.74802%。VMAF90 体积比 AOM 大 11.7967%。CPU 六点 92.406/99.141/109.078/110.094/126.203/151.578 秒，Q110 复用、其余五点新进程；皆并发诊断。





score memo 原始包已封 `agent-coeff/basic-score-memo-r1/HANDOFF.md`，具体 hunk 独立审查无阻断。3 文件 107+/5−，仅完整 SB 的 causal basic/工具为零域；每 frame 可选独立 arena 1867784B，RdWork 仅增16B，NULL/饱和回旧路。22 audit/12 fresh SAN 覆盖 forced miss、真实 hit 后 miss 故障及 UV 首次 capture。root r15（memo+sparse）两个裁块桥接通过，原生 Q110 对 r12 整流/重建及所有非 forward 统计完全相同，CPU104.922秒，fixed 解码通过。r16 再组合 UV2，两个裁块及原生 Q110 对 r13 同流，CPU100.984秒；全部六点现已逐字节桥接通过。并发单次耗时不当作正式提速倍数。



低收益后续观测：在 memo producer 上重复 Y 粗排 mask 的时间上限只有整裁块约4.27%/3.92%，未实现额外缓存；量化 raw dq 去重微核省28%～33%，两裁块整体仅约1.56%/1.43%（一个 Windows CPU 计时量级），暂封存、不用微核数字外推整帧。r14 准备脚本误用默认GBK读取 transform，插 hunk 前失败、后续 shell 构建了未组合 producer；已单独标 FAILED_PREPARATION_ENCODING，不计候选；r15 使用显式 UTF-8 重建，失败文件和日志保留。





r16 六点现已完成独立像素/质量审计，并对 r13 六点的流、重建、非 forward 最终统计全部逐字节核同（`native-r16-six-r1/bytebridge-summary.json`）。质量曲线完全相同；CPU 六点为85.109/88.453/96.375/100.984/109.188/124.156秒，墙时98.872/104.040/112.284/104.955/124.411/139.275秒，仍是并发诊断、不是正式2×验收。Q110复用该冻结程序既有新进程结果，其余五点均本轮新进程。





完整1080p的独立observer保持r16整流/重建/全部CLI统计一致，480完整SB占100.2455墙秒及10123565 forwards，30不完整SB只占0.5131秒及78151 forwards（墙时0.5092%、FWD0.7661%），见 `root-native-sb-profile-r1/result.json`。不完整SB成为瓶颈的假设被否定，实际linear入口已覆盖partial；停止此方向。



预测入口m5>=2外层reference构造未被消费，内部basic/directional/MRL仍构造实际所需参考。最小guard单hunk已合MAIN，`main-exact-r4-merge.json`：新stage两项RD/故障CTest及三格式thin64对冻结f2流/重建、fixed像素全通过。私有r16两裁块20run完整统计与流保持相同，配对CPU中位约−3.33%/−2.98%，非正式速度。



便携n8 forward常数展开虽微核省62%～66%，两个裁块合计仅约2.04%，未达本轮约3%明确整体信号门槛；只封有限实验，不扩数值/SAN矩阵、不合MAIN。





后续原生六点与编译优化：



| 候选 | VMAF BD 对 r4c | PSNR-Y/U/V BD 对 r4c | Q110 字节 / VMAF / CPU秒 |

|---|---:|---|---|

| r16 longrect（仅24树、不是等价缩减） | −4.501873% | −3.69508% / +2.94827% / +5.46960% | 32186 / 92.396911 / 65.813 |

| r17 mixed8（胜树才RDOQ + reference去重） | −5.529158% | −5.53675% / +0.70728% / +2.44795% | 31798 / 92.477643 / 100.938 |

| r18 longrect（r17同源、IPO /GL /LTCG） | −3.295682% | −2.76772% / +3.30329% / +5.84089% | 32396 / 92.253549 / 55.547 |



longrect 独立六点见 `agent-coeff/tree-domain-longrect-native-r1/audit-summary.json`，root 又独立核全部原始文件、像素、逐flag和BD，见 `root-longrect-native-audit-r1/result.json`。r17、r18六点在 `native-r17-six-r1/audit-summary.json`、`native-r18-longrect-six-r1/audit-summary.json`。r18共同域4097网格最差仍减少2.31383%，VMAF80/90/93体积对r4c−2.36915%/−3.73872%/−5.63357%；对AOM BD仍+8.357962%，VMAF90体积仍大17.1896%。CPU六点48.125/47.563/49.969/55.547/54.172/62.063秒，均非正式五次隔离速度。不要因单点调用数降低或裁块提速宣称整帧同倍数降低。



r17-longrect普通构建与r18 IPO在原生Q110流/重建/全部最终统计完全相同，CPU61.734→55.547秒（两进程并发诊断），`native-r18-longrect-q110-r1/ipo-bytebridge-r1.json`。纯IPO两裁块三对CPU中位比0.8500/0.8571，全部流/统计exact；没有更改源或算法。



仅删6个大矩形提案的rect版三裁块有两处主指标+1.68226%/+1.64940%，另有middle PSNR-V +28.9987%，已停在裁块负包；r17 longrect/tree-refine off（三24树basic只对胜树做工具）origin主指标+4.87579%、middle U/V+17.0754%/+10.3353%，也停在裁块，不扩大native。未为速度静默启用这些失败配置。



sample_load/store 是内部逐像素跨TU调用；相同body移为picture.h static inline、picture.c删原定义（2文件7+/9−）已独立验证，23,429,120数值/非对齐/重叠probe两平台及fresh SAN/picture专项通过，全部CLI统计/码流/重建保持。两裁块中位CPU约−24.4%/−11.9%，有离群/争用；不能和IPO收益累乘。现已与显式fast入口组合通过验证并合MAIN，见下述合入记录。



显式 `--rd-mode fast`=3的隔离MAIN接合已完成有界验证，旧0/1/2的三phase RDOQ、MH×joint、thin64和plan3284域保留；fast只still/m5>=3/partition<=7/joint off/无motion/非legacy coeffRD。默认不分配新prefix/memo。8文件接合与最小sample inline已组合为 `main-fast-inline-r1-source`449；严格IPO构建与107/107全套CTest已通过（891.83秒）。三个格式旧thin64、fast对r17的两裁块和对r18原生Q110的流/重建/最终统计桥接全部通过。10文件已按before/after SHA合MAIN，记录main-fast-inline-r1-merge.json；原默认0/1/2继续保留。公共1D极值域保int64不动；仅2D的int32 inverse有界原型虽然微核部分更快，整裁块没有稳定5%信号，已封负、不扩SAN/oracle、不合MAIN。受限fwd32原型微核虽快，两裁块未同时达到稳定5%信号，已封负、不合MAIN；其有限数值/码流对照通过不等于完整极值域验收。





公开 `--rd-mode fast` 的实际组合程序现已完成原生六点，独立逐像素SSE/固定解码/质量/身份审计见 `native-mainfast-six-r1/audit-summary.json`，对r18六点码流、重建和全部最终统计（只归一mode=3/2标签）的独立桥接见同目录 `bytebridge-summary.json`。实际程序SHA77408c2f9ac6cc3c4a815b1b9d06dfcd4888216337a28c1e55ca2e1a1a5e2c3f。六点质量与r18相同，VMAF BD对r4c−3.295682%、对AOM+8.357962%，PSNR-Y/U/V为−2.76772%/+3.30329%/+5.84089%；VMAF90体积25600.26B，比AOM大17.1896%。六Q190/160/130/110/90/70的CPU秒为46.578125/46.000000/52.562500/42.765625/55.937500/63.500000，墙秒为50.2020/49.2919/55.3772/53.1536/59.6869/67.3205。Q110显式复用该同冻结程序先前单点，其余五点新进程。全部为并发诊断，未作隔离暖机+各点五次新进程，正式速度仍NOT_RUN，不能称2×或I-GATE通过，holdout未使用。



整程序/arch:AVX2编译对照（源同449、IPO同开、仅host专用产物）两128全流/统计exact，但一块CPU中位比0.82353、另一块1.06061，未形成稳定整体收益，没有改便携默认构建或扩大native。





真实prefix D+累计R下界提前终止15行原型在两128裁块3对交错12次编码保持流/重建/全部非forward统计exact，但forward完全没有减少、CPU中位均比1.0；未另观测实际abort，不宣称验证abort恢复，未扩SAN/fault，不合MAIN，见agent-coeff/causal-prefix-j-bound-r1。仅将SSE逐像素累计改成本地uint64再写回的2新增行/1替换行原型也未形成明确整体信号：两crop中位均只省一个15.625ms CPU量级（约2.9%），16次编码全部流/最终统计exact，封root-sse-local-r2-closure.json，不据此扩测或合入。准备r1因冻结目录有10个非清单运行文件而449 cardinality断言失败，未build；r2按冻结449清单逐SHA复制，原失败记录保留。





两域各1精细树的固定候选（保留pre18域最佳+完整域最佳，未删basic动态树枚举）三128六Q主指标为+0.48836%/−4.96489%/−1.56196%，随后以MAINfast449同基底、IPO、唯一offer_tree hunk形成r19，正常/逆序/真实末RDOQ深故障恢复/freshSAN和旧rd2字节对照全部通过。原生六Q全为新进程，独立审计 `native-r19-six-r1/audit-summary.json`：BD对r4c−0.544157%，V80/90/93对r4c−0.348927%/−2.725318%/−4.495399%，但共同域4097网格最大回退+1.099354%（V64.033095），超过严格1%上限，已拒绝合入，不能用均值或四舍五入放行。PSNR-Y/U/V BD为−0.195345%/+5.619445%/+7.912218%。CPU190/160/130/110/90/70为26.265625/39.656250/30.218750/46.484375/50.046875/57.937500秒，单次并发、Q110无确认净改善，不外推普遍加速。源/程序/失败质量证据完整封存，未做Q阈值修补或扫N，MAIN仍是上一公开fast组合。





当前MAINfast+IPO/longrect新observer两128流/重建/全部原统计保持，固定像素通过，见agent-coeff/profile-fast-inline-r1/REPORT.md。inclusive墙时basic约54.80%/52.17%，predict25.21%/23.90%，actual forward TX17.31%/16.55%、quant7.21%/7.28%、inverse TX9.78%/7.86%；各层有嵌套且热调用1/127抽样，不全部相加。quant参数初始化<0.6%，RdWork snapshot约0.1%，不做缓存或快照框架。逐row memcpy估计子层竟高于父层且有0tick，时间预算不可用；只能使用2.68/4.06M调用、273/404MB逻辑量，不能声称memcpy占21%～29%。



两个后续计数最初在mixed8域封负：不同Y模式但完全相同预测的可避FWD仅占全帧1.5493%/1.4066%，不实现额外缓存（agent-tx/basic-y-prediction-duplicates-r1）；saved RDOQ replay已被prefix缓存大多避免，剩余可省FWD仅9/50992和12/72091（0.01765%/0.01665%），不做skip-forward（agent-tx/saved-rdoq-forward-potential-r1）。计数都排除已有memo/UV-Y cache已经省掉的调用，base/observer所有最终统计和流保持，不等于时间收益。



删除fast basic中三个每trial整SB恢复的精确候选r20正在验证，保留组件末/extend-prefix/错误恢复；两12816次配对全流/重建/统计exact，中位CPU−5.88%/−5.56%。原生Q110对MAINfast码流/重建/全部最终统计exact、fixed通过，32396B/V92.253549，45.296875 CPU秒/45.806474墙秒；这与旧并发Q110不直接比较速度，正在做顺序交错三对同QP诊断，fresh SAN/逆序/deepfault多格式随后全部通过，三行删除现已合MAIN，见下述main-exact-r5合入记录。





r20三行删除已合MAIN，`main-exact-r5-merge.json`记录before/after源SHA、独立closure、targeted CTest和原生证据。独立7case normal/reverse完整64bit候选trace/五输出/driver统计exact，7真实末RDOQ后fault4+retry、7fresh ASAN/UBSAN/leak重试、7强制memoNULL和7SANNULL通过；14公开CLI流/重建/全部最终统计exact，7fixed像素通过。root新构建test_m6_rd/test_mhccp_fault均PASS（30.26秒）；未因同一fast局部删除重跑已过的全107。原生Q110顺序交错三对，base CPU40.9375/40.953125/40.671875，新CPU39.390625/39.453125/39.765625，全部流/重建/最终统计exact，中位40.9375→39.453125（−3.62595%），墙中位41.4167→40.0161。此为同QP精确候选诊断，仍不是AOM同质量五次正式速度验收，版本号保持0.0.5-m5。





现MAINfast（末RDOQ/IPO/inline）补测参数rect18树原生六点r22，与旧r16 rect裁块不同基底，全部新进程/固定像素独立复核。`native-r22-six-r1/audit-summary.json` VMAF BD对r4c−1.102547%，V80/90/93对r4c−2.79859%/−5.65213%/−7.48759%，但低质量共同域端点最大+4.555808%，拒绝启用；PSNR-Y/U/V−0.81602%/+8.35689%/+10.07095%。不因平均或V90好看而覆盖1%局部上限，MAIN推荐仍longrect24。



仅fast decision1两处coarse paths16→ordinal0单一路的seed1候选通过三crop主指标，但实际FWD只省1.1%～2.3%、CPU一块好一块差。原生单Q110 r21 32321B/V92.315505，49.671875 CPU秒，未形成加速信号，停止扩六点，不合MAIN。该单点与同期不同负载旧点不可作正式回退倍率。



同arch AVX2的第一pass八列32行内核试验两12816编码全流/统计exact、fixed通过，但CPU中位两块都ratio1.0，封agent-rd/AVX2-COLUMNS-R1-STOPPED.txt；不据微核推导整帧收益，不扩强数值/SAN/native/runtime dispatch，不改便携构建。重复decision计数亦只有1.26%/1.93%frame FWD，已封负、不新增几何缓存。



参数口径补注：root-basic-copy两128性能对照为mixed8/fast，原生三对为longrect/fast；AVX2八列和direct16两128性能臂均实际longrect/fast，root独立命令提取见root-diagnostic-parameters-audit-r1.json。TX同预测重复与saved replay两项最初纯计数实际沿旧mixed8 bridge，不能套用为longrect；原结果保留，正用同冻结observer补longrect addendum，不改变生产或已有结果。basic-copy七case有界驱动是明确复用旧MAINfast mixed8域，原生longrect流/统计桥接另行通过。





原纯计数的longrect addendum已完成且未改旧mixed8结果：同预测可省FWD上限1.89510%/1.14062%，saved replay仅0.01444%/0.01253%，仍分别低于10%/3%停止线；每包新4base/observer CLI全部stats/流/重建exact+2fixed通过，路径在原包LONGRECT-CONFIG-CORRECTION.txt。



r23 direct16与r20三copy删除的组合现已合MAIN，main-exact-r6-merge.json锁before/after、程序/独立closure和全部证据。仅fast3、10b、native小端时预测原语直接写现有uint16 prediction对象表示，省bytes暂存后的解包；其它模式/位深/大端保旧路，inter不变。原语所有错误在dst首写前、字节别名与stride静态审查无阻断。独立7case当前longrect fulltrace/raw全部stats/qcoeff/plans/流/重建在normal/reverse与两类真实深fault重试精确一致，freshSAN/UBSAN/leaks 10双故障项、14CLI/7fixed均PASS；新targeted RD/fault CTest2/2 PASS。原生Q110全流/重建/全部统计对774 exact，32396B/V92.253549，单次36.09375CPU秒/37.29052墙秒。顺序交错三对相对r20 CPU中位37.28125→33.703125（比0.9040），墙中位43.42151→39.73642；baseline CPU31.390625～40.21875波动显著，期间其它agent构建可能争用，故仅诊断，不与旧批次的39.453125倍数累乘，不宣称AOM2×。当前可测IPO程序integrated-speed-r23-build/bin/SvtAv2EncApp.exe，正式速度NOT_RUN、版本仍0.0.5-m5。



Y SAD top4→top2+进入Y的五行有损候选root-y2，在三crop VMAF BD为+4.522965%/−2.989944%/−1.849291%，已封负不扩SAN/native、不合MAIN；两crop CPU虽约−5.6%/−9.5%，不能据此接受质量失败。UV普通top2→top1与方向族两层粗排都未达双crop约3%整体速度门槛，已停在小样本负包。SAD AVX2同arch对照一块−6.25%一块0%，同样封负不扩数值/SAN/native。linear工具阶段ordinaryUV精确复用计数仅0.92%/0.57%frame FWD，不新增缓存。





AVX2量化原生隔离结果封于 `root-quant-avx2-native-isolated-r1/result.json`：同arch IPO基准与37行原型，固定CPU8、各一次整帧暖机、交错三对新进程。base CPU[40.734375,40.484375,40.9375]，quant[39.203125,37.5625,40.421875]，中位减少3.75911%；墙中位减少3.70709%。测试期间团队无其它构建/编码任务，整机仍观测0.41～0.75等效核其它负载。8次流、重建、全部最终统计exact，不是AOM同质量五轮门槛。独立代码审查无功能阻断；972万mask-wrap计数包含标量回退，不能全称SIMD命中。portable接入以r23为基底，独立AVX2对象关闭IPO，内部state init一次能力检测选函数，公共scalar API保持；首构建因头文件include传播失败保留，修复用相邻目录引用，不扩大公共ABI或CMake include传播。尚未MAIN。



Y2后续原生复核（r24）明确覆盖此前“裁块失败即不扩原生”的试验决定，但保留原裁块失败证据：三裁块正负方向相反，局部不能证明整帧超限。固定唯一五行候选、未扫N/阈值，原生六Q全为新进程，`native-r24-six-r1/audit-summary.json`独立逐像素/固定解码/体积/VMAF复核PASS。VMAF BD对r4c−2.535753%，共同域[44.200715,96.040809]4097点网格最差仍−1.645721%，V80/90/93体积−1.692517%/−4.181179%/−7.351033%，通过本帧严格1%门槛。PSNR Y/U/V BD为−1.287436%/+2.816501%/+6.321110%，保留色度代价。对AOM VMAF BD+8.932677%，V90体积仍大16.650925%。六点bytes[4302,10295,20993,32593,52376,97694]；Q110 V92.441237、38.984375CPU秒/50.35864墙秒，属并发诊断，不作2×验收。正在将唯一五行hunk移到r23，补normal/reverse/真实fault/freshSAN/旧rd2/offscope和组合字节桥接；未合MAIN。





本轮组合r25=(r23+portable quant r2+prediction roundshift)，r26再加Y2。roundshift仅一行：intra_round2非负rounded除2^bits改为unsigned右移，负路径原样；所有调用bits2..7。MSVC和freshGCC ASan/UBSan各24585990数值检查PASS，预测/MRL两专项各2/2 PASS；两128三对CPU中位−6.25%/−5.263%（各只一tick，诊断），所有流/重建/全部统计exact+fixed。原生Q11038.625CPU秒为并发单次，不能比较旧批次。默认路径同样等价，尚未MAIN。首次WSL未指定发行版失败且脚本未启动，改用既有ARXChem-Ubuntu-22.04-WSL1成功，保留事实。



两个组合已冻结451项源，r25 app14dd0e00bbfe59572851252288cb4c65faf24cd70b5a738d3da6501aac5d043d，r26 app74ff83461b2b314de9d0f31e7e0a1a7f7f5eb00b4deebdd08f106b29ee9ace67。20公开CLI（3格式64和2crop、各前后）全部流/重建/最终统计exact、10fixed通过；r26六个原生Q全新进程对原Y2/r24全部流、重建、最终统计exact和6fixed通过，因此继承r24已审质量而非重跑模型或重新假定质量。freshGCC组合SAN的prediction/transform_path/m6_rd/mhccp_fault4/4 PASS（123.4秒）。portable内部state/header变更的完整107消费者检查正在收尾。未MAIN，正式速度NOT_RUN。



保留进入Y、仅SAD第一名精细评分的单行筛选r27已完成六个新原生点并独立审计：平均VMAF BD对r4c−0.465036%，但共同域4097网格最大回退+3.105345%（V53.414482），拒绝，不追加SAN/fault/正式测速；不按V90−5.918396%单点放行。PSNR-Y/U/V BD−0.658085%/+3.873989%/+5.743164%。因此保留Y2/r26。



胜叶现场复用前置计数在两crop物理FWD潜力9.356%/6.059%（只last-trial-live，无snapshot）；最小50增/3删、RdWork64B候选16次编码除物理forward外全stats/流/重建exact+2fixed。但并发r1 CPU比.8125/1.1333、pair波动.526～1.778，未达双块3%门槛；原STOP结果保留，最终性能判断挂起。已准备CPU8每组10新CLI×3交错组的独立r2，待其它CPU测试完成再隔离运行，不新增snapshot/SAN/native。





MAIN r26八文件合入已完成，before源逐个核对r23，新增quant ISA源/内部头之前明确不存在，备份在main-speed-r26-before，合入与新构建核验各用独立JSON而非热改旧冻结记录。freshMAIN四项检查4/4 PASS（34.48s），新MAIN原生Q11032593B与原r24流/重建/全部最终统计exact+fixed；33.90625CPU秒/34.601336墙秒仅单次正确性诊断。最终可测MAIN app身份见main-speed-r26-verify.json。





r28仅`--tree-refine wide→rd`（rd_mode仍fast3），同r26程序，无生产代码变化。wide池实际为all24 globaltop2 + 其子集pre18 top2，去重≤4；rd参数正是globaltop2，不能等同旧r19 global1+pre18one。新六Q全进程+fixed+像素审计对r4c VMAF BD−2.309018%、4097grid最差仍−1.273664%，连续导数根最差−1.273664%于V50.466162。PSNR-Y/U/V BD−0.828760%/+3.518636%/+4.716145%。相对r26wide自身则VMAF BD+0.238123%、高质量局部最坏+4.782386%，这是使用相对r4c的余量，不能声称每次迭代相对上一版也≤1%。实际FWD减少2.52%～7.07%，Q110−2.605%，不要由并发Q11026.78125CPU秒推大幅加速。root独立全部pixel/PCHIP/model/decoder/实际CLI复核通过；首whole-dict断言仅因为agent多了附加metadata而失败，common fields全部精确等同，原文件不热改。20MSVC driver+8public+8fixed+12SAN通过，使用r26当前headers新编（非r23旧内部state ABI）。正常/逆序仅accept次数随顺序变，赢家D/R/J/hash、计划/coeff/流一致；两真实deepfault重试全部trace/stats/bytes exact。正在root-r28-wide-native-isolated-r1：同MAIN新程序、CPU8完整暖机+3对，未完成前不推荐变更参数。



r29先全部树Y、仅finalists补UV的23增/9删候选已淘汰。有限正逆序D/J/wholeR/公开CLI/fixed和作用域外控制通过（首parser把合法被拒工具trial假定必须更低J，保FAIL并修外置审计，未热改源/重跑已完成normal）；六原生新点fixed/SSE/VMAF全部合法，但对r4c平均BD+0.846833%、共同域最坏+2.281894%于V61.172777，严格失败，不补深fault/SAN/全107/扫阈值。PSNR-Y/U/V BD+1.259000%/+1.436169%/+4.214799%。更进一步的全树Y下界exact两阶段也被静态证明阻断：除最终D和lambdaR舍入相位，Y/U/V还共享EOB_EXTRA adaptive CDF，跳过过去UV会改后续Y率与赢家；不能用r29的YIDs当原baseline的精确Y路径。



完整原生RIP统计诊断封于agent-tx/native-rip-profile-r1。451源和采样app的.text与r26完全相同，仅link新增MAP；只Suspend/读取自己child主线程RIP并finally恢复，7986成功样本、所有previousSuspendCount0。收尾SuspendThread WinError5（线程退出竞态推断）保留FAIL，未声称OS自然exit0，未为尾部错误重编码；已完成stream/recon/全部最终统计及fixed与r26完全一致，仅将成功样本作定位、不把该次耗时当速度。flat self比例fwd1d15.314%、reconstruct_leaf_planes8.740%、rd.predict7.976%、directional7.213%、basic5.522%；不是inclusive父子链时间预算。离线MAP+pdata链无nearest猜测，fwd1d全部1223点为真实指令边界：size4body仅全体0.1503%，dot循环6.1733%已执行128bit VPMULLQ自动向量路径，不能将15.31%都当未向量化。VCRUNTIME导出E9 thunk+精确unwind界证明332点为memcpy|memmove共享代码4.1573%，141点为memset1.7656%，228点2.855%仍未知；不从共享代码推断源调用是memmove。后续只评估有范围证明的2D private odd-dot 8×32→64，public1D全INT域、原round与int64sum保留。

</details>


<details>
<summary>009 · M6-intra-speed-pause-20261025.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-intra-speed-pause-20261025.md`  
原文件 SHA-256：`671a8a1a3ce74ca4a963927c3dbda400ffb49503a3eda3459289b99989db4555`

# AV2 单帧速度优化暂停报告（2026-10-25）

用户要求：本轮测试结束后暂停，说明性能如何逐步优化，并报告体积与用时。已停止新增编译、编码和性能队列；本轮已启动的重建正确性检查完成，未启动的内核三臂测速和 UV 下界计数保持未运行。本报告是本次暂停入口，完整研发过程保留在 [持续记录](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/docs/task-reports/M6-intra-speed-20261025.md)。

## 1. 暂停时的结果

当前可用结果是 **r26 生产代码 + r28 已有参数 `--tree-refine rd`**。固定同一张 1920×1080、10-bit、4:2:0 的 2D 开发帧，单线程，Q110：

| 项目 | 当前结果 | 口径 |
|---|---:|---|
| raw OBU 体积 | **32,491 字节** | 包含合法码流头，不计容器 |
| 解码 VMAF | **92.428470** | 固定模型、固定官方解码器 |
| 编码 CPU 时间 | **31.453125 秒** | 暖机后 3 次新进程中位数 |
| 编码墙钟时间 | **31.565879 秒** | 同批 3 次中位数 |
| 六点同画质体积相对冻结 r4c | **−2.309018%** | 共同质量域 PCHIP BD-rate |
| 同域 4097 点最差体积变化相对 r4c | **−1.273664%** | 通过最多回退 1% 的约束 |
| 六点同画质体积相对 AOM | **+9.062324%** | 当前仍比 AOM 大 |
| AOM 2×耗时目标 | **尚未达到** | 完整曲线五次速度验收未执行 |

测速机器为 AMD Ryzen 7 7840HS，8 核 16 线程；编码进程固定逻辑 CPU8，affinity mask=256。不是用户提到的另一台 7940HX，不按核心数或另一设备的秒数换算性能。各编码器内部仍单线程；多路队列用于同时开发和正确性检查，隔离测速时团队其它 CPU 任务停止。

暂停前最后一个性能窗口是同一个 MAIN 程序，只切换 tree-refine 参数：wide 的 CPU 中位 **33.187500 秒**，rd/global2 为 **31.453125 秒**，减少 **5.225989%**；墙钟减少 **4.959291%**。两臂各完整暖机一次，3 对交错新进程，共 8 次输出分别与各自原六点码流、重建、全部最终统计完全一致。

证据：[最后隔离测速](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/root-r28-wide-native-isolated-r1/result.json)、[六点质量审计](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/native-r28-six-r1/audit-summary.json)。

## 2. 距离 AOM 还有多远

上一隔离窗口同时测了冻结 AOM 3.15.1、cpu0、ALL-INTRA、单线程。AOM Q24/Q32 各暖机一次、各 5 次新进程：

| 编码点 | VMAF | CPU 中位秒 | 墙钟中位秒 |
|---|---:|---:|---:|
| AOM Q32 | 90.431183 | 10.375000 | 10.463233 |
| AOM Q24 | 93.680344 | 12.390625 | 12.452093 |
| AV2 r26 Q110 | 92.441237 | 33.390625 | 33.447552 |

在两个 AOM 锚点之间按 VMAF 对时间做局部线性插值，r26 对应 AOM 约 **11.621942 CPU 秒**，AV2 为 **2.873068×**。这是同窗口局部诊断，不是完整曲线速度验收。最新 r28 的 31.45 秒与该旧窗口锚点相比约 **2.7×**，只能作为跨窗口粗略参考；AOM 没有在 r28 窗口重测。按约 11.62 秒估算，2×约为 23.24 秒，当前仍需减少约四分之一耗时。

体积也尚未达到 AOM 的 85%，当前甚至尚未与 AOM 持平。按真实解码画质匹配的体积如下，避免拿相同 QP 数字当相同画质：

| VMAF | 冻结 r4c 字节 | 当前 r28 字节 | AOM 字节 | 当前相对 r4c | 当前相对 AOM |
|---|---:|---:|---:|---:|---:|
| 80 | 14,466.80 | 14,196.84 | 12,418.90 | −1.8661% | +14.3164% |
| 90 | 26,594.56 | 25,337.17 | 21,845.17 | −4.7280% | +15.9852% |
| 93 | 37,844.01 | 35,189.14 | 30,845.79 | −7.0153% | +14.0809% |

上述体积是曲线插值值。六点 BD 的 +9.06% 是共同区间平均，不能代替 VMAF90 的 +15.99%。本阶段按用户最新指示冻结体积方向、优先速度；没有把原来的 85% 目标标记完成。

VMAF 为既定主指标，PSNR-Y/U/V 同时保留。当前相对 r4c 的 PSNR BD 分别为 **−0.828760% / +3.518636% / +4.716145%**，色度仍有代价。开发帧通过不能推广为全数据集或盲测通过；holdout20、I-GATE 均未执行。

## 3. 为什么早期会慢到分钟级

主要是搜索组织存在大量重复工作，Release 构建本身已经开优化：

1. r4c 的单帧 Q110 约有 **843 万 RD 候选、3.40 亿次 forward 回调**。试一个局部模式时反复重建后续叶、重新计算完整 SB 熵码率；最多 56 棵树都做完整细化。
2. 分区递归内部反复校验全帧计划；预测参考重复构造，像素 load/store 还存在逐像素跨编译单元调用。
3. UV 搜索反复重算未变化的 Y；同一因果前缀的重复候选没有复用。部分 RDOQ 工作在最终不会选中的树上也执行。
4. 整 SB 备份恢复、纯预测像素暂存、字节到 uint16 解包，以及通用变换/量化内核，叠加在数量过大的候选上。

当前 r28 Q110 已降到 **58,865 个 RD 候选、4,781,260 次 forward 回调**。这是计算工作量减少的直接证据，但调用数不是耗时倍数，不能据此声称等比例提速。

早期六点曲线、多个候选构建和验证曾并发运行，会改变频率、资源竞争和墙钟等待。因此历史 r4c 的 2887 秒与现在 31 秒不能直接作为同条件加速比。当前只把暖机、交错配对、团队停止其它 CPU 工作后的数据用于具体速度判断。

## 4. 优化是怎样一步一步发生的

### 4.1 先减少相同结果的重复计算

- **变换内核**：利用 DCT 对称性、ADST 多路累加；全零反量化系数直接跳过 inverse，inverse 跳过尾部零频率，常量残差复用第一遍变换。保留公共 1D 极值域与正确舍入，数值、官方 oracle 和 sanitizer 对照。
- **熵率与分区校验**：把基本模式评分改为真实熵前缀增量推进；候选只试算、不提交。全帧分区校验移到公共入口，内部事务仅验证改动 SB。RDOQ 非负率下界跳过不可能改善的 writer 调用。
- **精确复用**：UV 搜索复用不变 Y 的重建/系数/SSE；相同完整因果前缀用有界 basic-score memo 复用局部结果，仍重新计算累计 R 和整数 J；NULL/饱和时回原路。

这些步骤通过码流、重建、系数/计划和候选轨迹对照；不能把各微核加速倍数相乘当整帧加速。

### 4.2 再减少昂贵搜索覆盖的范围

参考固定官方 AVM 的因果重建和先 Y 后 UV 搜索组织，将工具候选从反复重建完整后缀改为目标叶评分、因果前缀提交；采用 Y/UV 便宜预筛，再用真实熵率和重建质量决策。RDOQ 移到胜出树，树域从 mixed8 的最多 56 棵改为 longrect 的 24 棵。

这类变化可能改变赢家，属于搜索筛选，不是严格等价优化。每项保留六点质量审计；不接受只看平均 BD 或一个高质量点通过的候选。相对 r4c 的质量约束始终固定，没有每次重新把上一版当更宽松基线。

### 4.3 把已选路径的常见开销降下来

启用 IPO/LTCG，把内部 sample load/store 小函数 inline；跳过不被消费的外层参考构造。显式增加 fast 模式入口，保留默认旧 rd_mode 0/1/2。随后去掉 fast basic 每次 trial 的三处整 SB 恢复，仍保留组件末、前缀扩展和错误恢复；10-bit 小端预测直接写现有 uint16 缓冲，省中间字节缓冲解包。

最后加入可移植 AVX2 量化：内部初始化只检测一次 CPU/OS 能力，独立 AVX2 对象，未支持平台走 scalar；没有把整个便携程序强制编译为 AVX2。预测的一行非负除法改等价右移，负数路径保持原样。

### 4.4 原生整帧里程碑

下表列出历史实际记录，帮助定位各阶段。**除单独列出的隔离表，均为并发诊断单次 CPU，不能用相邻行相除归因。** 相同 Q110 也不代表相同画质，因此同时列 VMAF 和六点同画质 BD。

| 阶段 | 主要变化 | Q110 字节 | VMAF | 当次 CPU 秒 | 六点 BD 对 r4c |
|---|---|---:|---:|---:|---:|
| r4c | 冻结质量基线、原完整搜索 | 33,565 | 92.138170 | 2887.266 | 0% |
| prefix | 因果局部搜索使用真实累计熵率 | 33,172 | 92.262802 | 423.891 | −3.536955% |
| r9 | 目标叶/因果工具搜索 + sparse inverse | 31,448 | 92.447320 | 184.094 | −7.327564% |
| r16 mixed8 | Y/UV 预筛、不变 Y 缓存、score memo | 31,806 | 92.371124 | 100.984 | −5.860741% |
| r17 mixed8 | 胜树 RDOQ、参考去重 | 31,798 | 92.477643 | 100.938 | −5.529158% |
| r18 longrect | 24 树域、IPO | 32,396 | 92.253549 | 55.547 | −3.295682% |
| MAIN fast | 公开 fast、sample inline，六点与 r18 同流 | 32,396 | 92.253549 | 42.766 | −3.295682% |

r20 三处恢复删除的单独原生三对诊断中位为 **40.937500→39.453125 CPU 秒（−3.62595%）**，输出不变。r23 直接 uint16 预测也保持输出不变，但早期配对负载波动明显；最终以之后隔离窗口重新测得的 37.953125 秒为参考，不累乘旧批次收益。

### 4.5 最近可直接比较的隔离窗口

第一个窗口：所有团队其它 CPU 工作停止、固定 CPU8，各完整暖机一次；r23/r25 各 3 次、r26 5 次新进程，交错执行。

| 阶段 | 变化 | CPU 中位秒 | 墙钟中位秒 | 同窗口耗时变化 |
|---|---|---:|---:|---:|
| r23 | 三处恢复删除 + uint16 直接预测 | 37.953125 | 38.126928 | 参考 |
| r25 | + 可移植 AVX2 量化 + 预测 roundshift | 35.390625 | 35.536928 | 对 r23 −6.751750% |
| r26 | + Y SAD top2，保留进入 Y | 33.390625 | 33.447552 | 对 r25 −5.651214%；对 r23 −12.021408% |

r25 与 r23 码流/重建/最终统计一致；Y2 改变搜索结果，经原生六点审计后 r26 的 BD 对 r4c 为 −2.535753%。量化与 roundshift 在这个组合窗口一起测，不能将组合收益拆成两个独立整帧比例。

第二个窗口：同 MAIN 程序，仍固定 CPU8；wide 与 rd/global2 各暖机一次、3 对交错。

| 参数 | CPU 中位秒 | 墙钟中位秒 | 对同窗口 wide |
|---|---:|---:|---:|
| tree-refine wide | 33.187500 | 33.213009 | 参考 |
| tree-refine rd/global2（当前） | **31.453125** | **31.565879** | CPU −5.225989% |

wide 的 finalist 是全局 top2 加 pre18 子集 top2，去重后最多 4 棵；global2 只留全局 top2。它仍对原 24 棵树做 basic 评分，不能说只搜索两棵树。这是已有参数调整，没有新生产代码。其相对 r26 wide 的平均体积变化约 +0.238%，高质量局部可超过 1%；用户约束基准是固定 r4c，相对 r4c 的全域严格检查通过。

## 5. 本轮淘汰和封存了什么

| 候选 | 观察 | 处置 |
|---|---|---|
| Y1/r27 | 平均 BD −0.465%，局部最坏 +3.105% | 超过 r4c+1%，淘汰 |
| rect18/r22 | 平均 BD −1.103%，局部最坏 +4.556% | 淘汰，保留 longrect24 |
| r29 所有树先 Y、仅 finalist 补 UV | 平均 +0.847%，局部 +2.282% | 淘汰，不继续扫阈值 |
| DCT64 跳过最终丢弃高频 | 数值/SAN通过，两裁块隔离 CPU +0.69%/0% | 无净收益，未合 MAIN |
| 胜叶现场复用 | 实际少做 FWD，但隔离 CPU 仅 −1.20%/−4.12% | 未通过双块稳定收益门槛，封存 |
| 全树 Y 下界的严格等价重排 | Y/U/V 共享 EOB_EXTRA 自适应 CDF，跳 UV 会改变后续 Y 率 | 等价前提不成立，未实现 |

还排除了大量微核更快、整帧不稳定的候选，以及潜力不足的缓存方案。保留失败原包，不把调用减少当成速度通过，也没有为达到 2×静默启用超体积上限的筛选。

## 6. 暂停时最后三条队列

| 队列 | 已完成 | 暂停状态 |
|---|---|---|
| 2D DCT 递归 odd-dot AVX2 | MSVC 与 GCC sanitizer 各 10,844 例；两裁块三臂 6 次编码的全部统计、流、重建和 fixed 解码一致 | **三臂性能未启动**；仅 host 原型，无 portable dispatch，未合 MAIN |
| 重建省纯预测中间 frame 写入 | 7 组正/逆序、5 组 builtin 真错误后重试、custom/wrapped fallback、旧模式、7 组公开 CLI/fixed、9 组 fresh sanitizer；补齐旧 RD 单测通过 | **正确性完成，性能未启动**；未合 MAIN |
| 当前叶 U 后的 V 下界 observer | 完成最小源码/补丁准备，只读 memo、严格下界断言设计 | **未编译、未编码、无命中率数据**；未接提前终止，未合 MAIN |

不为这次暂停补开上述性能或计数测试。恢复时先从这些已冻结产物继续，不重跑已通过的完整矩阵；需先确认实际整帧收益，再决定 portable 接合和 MAIN 合入。当前没有依靠这些未测速候选降低报告中的 31.45 秒。

## 7. 可复现产物与验证边界

可用 MAIN 程序：

`<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/main-r26-verify-build/bin/SvtAv2EncApp.exe`

SHA256：`954d66fb507a3565de1a0d87dcfc4d9efe90a9cf26db09aee3aa3e73d18e5947`。私有 r26 程序 SHA 为 `74ff8346…ace67`，两者经输出桥接，不能混写二进制身份。MAIN 的 8 文件合入清单、备份与新程序验证记录见 [MAIN 验证](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/main-speed-r26-verify.json)。版本仍为 `0.0.5-m5`，未覆盖安装 Bin/旧 build。

复现当前 Q110 结果的 PowerShell 命令（输出文件名自行选择；暂停时未再执行）：

```powershell
& '<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/main-r26-verify-build/bin/SvtAv2EncApp.exe' `
  --input '<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-20261024/dev-input/2D-frame48.y4m' `
  --output 'result.obu' --export-recon 'recon.yuv' `
  --still-picture --threads 1 --qp 110 --intra-select cfl `
  --partition-select longrect --rd-mode fast --entropy-adapt on `
  --transform-select full --coeff-opt rdoq --angle-search rd --mrl-search rd `
  --tree-refine rd --cfl-search rd --mhccp-search rd --rd-lambda avm-ai
```

当前六个原生点，均为固定官方解码逐像素一致且已独立核验体积、VMAF、SSE：

| AV2 Q | raw OBU 字节 | VMAF |
|---|---:|---:|
| 190 | 4,329 | 43.672394 |
| 160 | 10,281 | 71.974563 |
| 130 | 21,059 | 87.390246 |
| 110 | 32,491 | 92.428470 |
| 90 | 52,670 | 94.852980 |
| 70 | 97,850 | 96.152680 |

六点原质量采集时并发运行，不能将该批 CPU 时间当最新逐 Q 隔离速度。最新 31.45 秒只对应上述 Q110 点。

验证覆盖：fast/inline 集成曾完整 CTest 107/107 通过；portable quant 内部 state/header 接入阶段再次完整 107/107 通过，最终 r26 组合另有专项 4/4、fresh sanitizer 4/4、20 CLI/10 fixed 和六点字节桥接。没有宣称最终所有后续组合都重新跑过 107 项。默认旧模式保留，优化入口须显式 fast。

本次停止状态与证据身份写入 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/root-pause-20261025.json`。正式同质量五次全曲线速度、holdout20、I-GATE 保持 **NOT_RUN**。任务已暂停，等待用户后续恢复指令。

</details>


<details>
<summary>010 · M6-intra-v1-r2-20261025.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-intra-v1-r2-20261025.md`  
原文件 SHA-256：`e1df7c35910b3fe016d38944437c971a3d200531c49e1683318928eeea1ce7a1`

# AV2 单帧第二轮：VMAF v1 与速度优化（2026-10-25，检查点）

用户检查 [05指南](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/M5-M6-AI-TASKS/05-M6-intra-next-round-guide.md) 后明确恢复。本轮独立目录为 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025`。启动时C盘空闲58,159,214,592字节，约54.2GiB；没有删除冻结证据或旧失败包。内部编码单线程，正确性/质量任务可并发，性能窗口独占。

## 当前结论

已完成新版指标接入、72个冻结重建重评分、独立像素审计与高区间补点。**旧普通0.6.1的质量通过不能沿用为v1通过**。目前代码仍为上轮MAIN r26，没有本轮生产合入。Y4/wide原六点曾通过冻结r4c质量门，补密至11点后，PCHIP局部最坏为+1.6818%而线性仍通过，因此当前局部质量门应标为待稀疏r4c参考补点确认。Y2/wide、Y2/global2、Y4/global2也不能作为新版已验收配置。0.85体积与2×耗时仍未达标。

下表为最初稀疏六点诊断，后续密点与四组合结论见文末；不能将不同点集的BD当作相同精度结果。

| 2D候选 | v1全域BD对AOM | v1的93–95局部BD对AOM | v1全域BD对r4c | 相对r4c网格最坏 |
|---|---:|---:|---:|---:|
| 冻结r4c | +9.8988% | +26.0918% | 0% | 0% |
| r26/wide | +9.8250% | +25.6524% | −0.4937% | +1.3200%，V87.0047 |
| r28/global2 | +9.5335% | +23.9013% | −0.3103% | +4.9143%，V96.6512 |
| 官方AVM9 | −25.4646% | −14.2765% | — | — |

以上复用每曲线原六个QP点，暂是稀疏曲线诊断。global2相对wide在新模型下的93–95平均排序与旧模型不同，不能照旧评分恢复wide就假定解决问题。普通0.6.1历史与新v1/NEG分列，未覆盖旧文件。

NEG宽域对AOM：r28 +10.3183%、r26wide +9.5212%；相对r4c网格最坏分别+0.7121%/−0.4674%，两者通过1%门。NEG原AOM最高点只有93.848370，因此不能报告其完整93–95 BD；与v1明确分开。

## M0：评分器与冻结重建

模型：`vmaf_v1.0.16_3d0h.json`（主）与`vmaf_v0.6.1neg.json`（副）。程序及模型身份同05指南，独立`vmaf.exe`版本输出3.2.0。单帧自比较、AOM Q24与AV2 Q110各两次，全特征逐值一致、每次恰有一帧、motion3为0。

| 验证输入 | v1 | NEG |
|---|---:|---:|
| 原始像素自比较 | 100.000000 | 97.424896 |
| AOM Q24 | 95.144493 | 91.983045 |
| AV2 r28 Q110 | 93.180143 | 90.569108 |

使用10-bit raw像素；只剥离Y4M文件头与FRAME标记，没有像素转换、重复帧或补帧。首attempt因CLI显式raw参数把Y4M头读入像素，报告大于1023的样本并异常退出，原日志保留；`metric-smoke-r2`改为显式raw后通过。CUDA初始化不可用时评分器按日志回退CPU；本轮评分结果与编码耗时分开。

72个点包括r4c/r26wide/r28、三张开发帧的AOM/AVM、2D三组官方滤波消融。逐个核源、码流体积/SHA、解码像素SHA；旧记录缺SSE时从像素重算并核PSNR。TX独立审计全部三平面SSE/PSNR通过，另补mainfast六点；两个Q110单模型运行的15+15个同名特征与组合模型逐值一致。

重评分证据：

- [单帧smoke](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/metric-smoke-r2/result.json)
- [72点新评分](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/rescore-existing-r4/results.json)
- [新模型曲线初算](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/rescore-curve-summary-r1.json)

重评分r1/r2/r3在准备阶段分别遇到旧记录schema差异，均在启动评分前失败，旧目录保留；r4显式处理artifact清单、r26六点字节桥接和缺失SSE后一次完成72个点。未重新编码或解码。

## S1/S2：已有精确候选性能筛选

S1省纯预测中间frame写入：global2两裁块4CLI/2fixed桥接通过；CPU8、40暖机+120实测进程，全部流/重建/公开统计exact。origin CPU中位2.078125→2.093750秒（+0.7519%），BR2.546875→2.500000秒（−1.8405%），三组不稳定，未过双块3%信号。**封负，不合MAIN、不扩原生**。

S2 DCT odd-dot：global2三臂6CLI/6fixed桥接通过；6暖机+180实测，每子进程核mask256，流/重建/统计exact。C/A三组origin为1.0305/1.2016/1.0317，BR0.9040/0.9244/1.0183；C/B也未稳定通过。BR相对generic的部分收益来自hostarch基准，不能归给新内核。**封负，不接portable、不扩原生、不改MAIN**。最初性能脚本仅protocol变量名错误，0暖机/0计时即退出；新编号r2为唯一性能批，原空r1保留。

性能证据保存在原独立候选目录：

- [S1正式交付](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/agent-tx/recon-direct-pred-global2-perf-r1/PERFORMANCE-HANDOFF.txt)
- [S2真实性能批](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/agent-coeff/fwd-butterfly-avx2-global2-r1/perf-group10-r2/results.json)

S1/S2不能降低当前报告中的31.45秒。该耗时是上轮Q110隔离结果；本轮尚未做新模型同画质正式速度验收。

## M1：密点与四组搜索参数

新基准共15个原生编码点：初始12点全部完成，再补AOM Q27、r28 Q112，以及其紧邻Q113以包围93下端。r28 Q112=93.008618仍高于93，Q113=92.714044；没有丢弃Q112或挑选有利点。r28/AOM/AVM各有至少4个直接点落在93–95，且两端都有包围。14点已由TX独立审计源、流、像素、SSE和评分；Q113为后补点。

| 参数组合 | v1全域BD对AOM | 93–95局部BD对AOM | 对r4c连续最坏体积差 | 1%门 |
|---|---:|---:|---:|---|
| Y4/wide（原六点，后有11点补密） | +7.7122% | +19.4415% | −1.0532% | 当时六点通过，现待确认 |
| Y2/wide（r26wide） | +9.6962% | +23.3162% | +1.3200% | 未过 |
| Y2/global2（r28，含新增密点） | +9.5684% | +21.5322% | +6.1697% | 未过 |
| Y4/global2（新增六点） | +10.3044% | +20.0275% | +2.8769% | 未过 |
| 官方AVM cpu9（含新增密点） | −25.3306% | −13.8361% | — | 对照 |

表为首次四组合的PCHIP对数体积积分与分段三次导数驻点极值；[完整四组合结果](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/four-config-summary-r1.json)同时保存线性结果。该表的r4c和三组wide/Y4仍是原六点，部分局部损失受稀疏插值影响，不能称为同分直接实测；六点阶段Y4/wide在线性与PCHIP下均通过1%门。密点AOM/AVM在93–95的PCHIP与线性BD差小于0.01个百分点。NEG的AOM最高93.848370，仍不宣称覆盖完整93–95。

随后Y4/wide新增固定Q108/104/100/96/113五点，11点的93–95对AOM为PCHIP **+19.7035%**、线性**+19.8585%**，插值差明显缩小。但对稀疏r4c的PCHIP连续最坏变为**+1.6818%@94.833**，线性仍为−0.8840%，因此撤回其当前“安全通过”判定；需要补r4c实点后消除参考插值歧义，而不是换插值方法来宣称通过。五点CPU33.03–34.69秒为并发诊断。[11点完整结果](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/y4wide-dense-summary-r1.json)。

新原生点全部固定AVM解码、内部重建逐字节相等，保存三平面SSE/PSNR和两模型全特征。编码单线程，但多点并发，CPU/墙钟是质量诊断，**不是正式速度验收**。例如r28 Q110仍为上轮隔离CPU31.45秒；本轮Q108 CPU28.55秒、AOM Q28 CPU11.06秒、官方AVM Q110 CPU164.56秒来自并发点，不能混条件宣布提速。

## Q1：fast大矩形RDOQ原生筛选

独立r26基底只删除fast guard四行。既有4000官方变换数值证据按函数SHA复用；新增18强制流、31fixed、normal/reverse/真实错误恢复、六fresh SAN和旧rd0/1/2回归通过。没有重跑4000并冒称新增。

| QP | 原r28字节/v1 | 补大矩形后字节/v1 | 结论 |
|---|---|---|---|
| 110 | 32491 / 93.180143 | 32701 / 93.047104 | 更大且分数下降 |
| 100 | 41362 / 94.706097 | 41563 / 94.654008 | 更大且分数下降 |
| 96 | 45747 / 95.433894 | 45636 / 95.431669 | 体积约−0.24%，近同分 |

三点不足计算BD，现有信号弱/负，停止扩六点，不合MAIN。新点CPU31.83–32.58秒，仍是并发诊断。

## S3：同叶UV严格下界

先后完成松界、加入当前同叶真实Y率的紧界observer，均不改变决策。紧界验证8584个fresh候选、6242个有效可比较候选、3122个UV入口取得Y memo，未覆盖为0；逐项核率前缀与完整J下界，4CLI/2fixed的流、重建和全统计exact。实际可省V FWD为origin 4/23494（0.0170%）、BR 1/49565（0.0020%），远低于3%筛选线，**封负，不实现提前返回**。

## 旧质量候选改用新模型复核

复用已存像素新增24次评分，r29/r27/r22/r19四条候选v1对r4c的连续最坏差分别+6.1232%/+3.6374%/+7.2801%/+9.0283%，均未过全域1%门。不因少数高区间点改善恢复候选；没有新增编码、解码、构建或速度测试。部分旧包未存历史流SHA，本次只冻结当前SHA并核历史SSE/PSNR及包身份，不冒称核过不存在的旧SHA。

## 本轮工具与构建实验

新版模型还改变了滤波消融排序：固定官方AVM在v1的93–95稀疏曲线中，全关滤波相对原配置体积+4.1868%，只关CCSO为+2.8631%，保CCSO而关其余组为+3.1287%。旧普通0.6.1中“全关更好”的结论不适用于新v1。此为重新优化后的分组消融，不能把各组收益相加，也不代表直接移植到当前编码器必有相同收益；本轮尚未启动滤波实现。[完整独立计算](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/agent-tx/model-audit-r1/curves.json)。

1. Q3：独立r26副本，新增默认关闭的420 UV派生TX闭环，分工为配置/系数语法、typed变换内核、独立官方oracle/强制流。覆盖4×8/8×4与长32轴DCT的合法尺寸，UV不独立信令TX；EOB1的Y特例与UV分开。strict MSVC fresh全部消费者构建成功；16项受影响旧回归通过。新增256条强制流含1919个typed EOB0、2033个EOB1、2032个AC块和406次MH，全部固定AVM解码逐像素一致。独立官方5856个类型推导、5440个数值例、1,069,056个系数及3,207,168个逆变换clip像素检查通过。有限RD反序、三次真实typed UV错误恢复、六fresh ASAN/UBSAN、默认off旧流和新开关拒绝边界全部通过。源清单454项，新增文件3个，其余来自451项基底；没有合MAIN。
2. PGO单因素：独立r25/Y4-wide源码，MSVC `/GENPROFILE`训练两个冻结开发裁块，合并PGC后`/USEPROFILE`重链接。不改码流算法、不动holdout；4 CLI+4 fixed及全统计exact桥接通过，CPU8的4暖机+120实测全进程亲和核验通过。两裁块三组CPU比分别0.9141/0.9341/0.9521与0.9521/0.9737/0.9526，中位降耗6.59%/4.74%。一组未过预冻结的每组至少3%门，因此暂不扩原生或合MAIN，保留正向证据。训练时间不计编码性能，也没有正式AOM2×结论。[结果](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/pgo-r1/summary.json)。
3. S4：根据真实调用代理和源代码，把4896字节系数level缓冲清零延后至非零EOB分支，只移动三行。PGO两开发裁块中writer入口114968次、非零EOB函数47812次，差额58.41%是全零调用代理，含可能的早错误路径，不能当直接精确计数；更不能把19.3%动态指令占比当CPU收益。独立r25副本通过352个writer完整trace/CDF/邻居/状态对照、4CLI/2fixed全统计exact。CPU8的4暖机+120实测仍exact，但六组CPU比1.0000/0.9872/1.0366与0.9843/1.0106/1.0162，无稳定收益，封负不原生、不MAIN。

### Q3原生三点与最后一个组合

| QP | 原r28字节/v1 | UV派生后字节/v1 | NEG变化 | CPU秒（并发） |
|---|---|---|---:|---:|
| 110 | 32491 / 93.180143 | 32620 / 93.359362 | −0.005138 | 29.2500 |
| 100 | 41362 / 94.706097 | 41281 / 94.827253 | +0.017209 | 30.3438 |
| 96 | 45747 / 95.433894 | 45837 / 95.411627 | +0.022587 | 30.5938 |

两点v1提升、一点微负，仅为趋势；不计算三点BD，也不把同QP字节差称为同画质节省。证据见[原生三点](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/q3-native-pilot-r1/results.json)。最后一个有界候选为UV派生与恢复原Y4/wide组合，只恢复r25原top4小段、不叠Q1/PGO/S4。fresh strict全部消费者、off对r25两crop精确桥接、on正常/反序/真实故障恢复/fixed与fresh SAN均通过，随后沿用三个冻结QP筛选。

| QP | Y4/wide基准字节/v1 | UV＋Y4/wide字节/v1 | NEG变化 | CPU秒（并发） |
|---|---|---|---:|---:|
| 110 | 32396 / 93.188800 | 32247 / 93.161964 | +0.074406 | 32.9844 |
| 100 | 40883 / 94.666762 | 40725 / 94.549736 | −0.014228 | 35.0625 |
| 96 | 45247 / 95.432086 | 45325 / 95.531809 | +0.023029 | 35.0781 |

组合仍为混合趋势，尤其Q100的v1下降约0.117分。以原Y4/wide密11点曲线匹配Q100候选实际分数94.549736，同分体积差为PCHIP **+4.2435%**、线性**+4.1988%**，两种方法均出现局部负信号；这是单点匹配诊断，不是三点BD。因此没有把两点字节略减当作确定收益，不扩11点曲线，不合MAIN。[组合三点](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/q3-y4wide-native-pilot-r1/results.json)。派生UV类型闭环已具备可靠正确性资产，但仅补合法工具并不保证当前RD搜索在视觉指标下选得更好。

## 本轮检查点与后续入口

本轮新增1080p单帧编码共**35点**：15个密基准、Q1三点、Y4/global2六点、Y4/wide五点、Q3三点、UV＋Y4/wide三点。全部固定解码并评分v1/NEG，且已有点保持原始记录。[35点汇总](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/native35-index-r1.json)收齐每点体积、用时、分数与程序/结果身份，收尾时重核全部流、解码和评分文件。当前没有本轮生产合入，没有改变安装程序或旧默认模式；[MAIN的80个生产文件](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/main-production-end-audit-r1.json)仍逐SHA等于冻结r26。未使用holdout20。

本次2D开发帧可复核的高区间差距：r28对AOM为约**1.2153×体积**，Y4/wide为**1.1970×**；目标仍为**0.85×**。官方固定AVM cpu9的同区间为约0.8616×，只是该固定版本/档位对照，不能代表AV2上限。宽域与高区间分别报告。

速度方面，上一轮r28 Q110隔离CPU约31.45秒仍是已有冻结计时；本轮各原生候选CPU约29–35秒来自质量并发批，不能与旧值相减后宣称提速。PGO裁块中位降耗6.59%/4.74%有正向信号，但未过预冻结的每组3%门，也未做原生确认。**没有新的正式AOM同画质2×验收结果**。

下一轮先补**原冻结r4c的Q100一个真实参考点**：它位于Q110/v1=93.208734与Q90/v1=96.265574之间，接近争议极值94.833，并与候选已有Q100对应。必须使用原r4c程序、mixed8/pruned/wide模板，仅改QP/输出路径，不能用当前fast程序替代。[精确未运行计划](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/agent-tx/q3-y4wide-final-audit-r1/r4c-q100-notrun-plan.json)已核原程序和439个源文件，仅准备命令，没有启动或复制。该参考可能耗时数十分钟；一个点不保证消除所有插值歧义，不应直接启动一整条昂贵曲线。补点前，局部1%门保持待确认。

再依据UV实验实际的Y/UV误差和最终选择，集中检查预测/TX/树的最终RD排序与proposal真实上下文；不继续仅因官方有工具就逐个移植。PGO保留供未来最终候选重新训练/确认，其它本轮负性能方向不无证据重跑。

截至检查点，主队列与三条子队列的测试均已结束，无后台编码、计时或评分。最终组合的[独立审计](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/agent-tx/q3-y4wide-final-audit-r1/REPORT.txt)与[源码/正确性闭包](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/agent-rd/q3-uv-y4wide-r1/closure.json)均已保存。

本轮不动holdout20，不改默认旧模式，不推进帧间。密点[初始协议](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r2-v1-20261025/dense-baseline-r1/protocol.json)与全部原始点保留。

</details>


<details>
<summary>011 · M6-intra-v1-r3-20261025.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-intra-v1-r3-20261025.md`  
原文件 SHA-256：`800a3f8ada8e7129bf362d3b1e81e93afe34eb1ba5d38a6f750cc5ca4374439b`

# AV2单帧第三轮：冻结目标、参考补点与RDOQ排序（测试与独审已结束）



最终体积、正式五次计时和后续指南见[本轮最终报告](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/docs/task-reports/M6-intra-v1-r3-final-20261025.md)。以下保留各检查点的原过程，“待/当前”均按当时状态阅读。



2026-10-25，用户明确继续下一轮并冻结现有要求。合同见[06冻结说明](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/M5-M6-AI-TASKS/06-M6-frozen-requirements-20261025.md)及[机器合同](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/requirements-freeze-r1.json)。主v1、副NEG、AOM的0.85×体积/≤2×用时、原r4c的1%约束均不变。



## 本轮启动



原冻结r4c Q100已完成，输出为`speed-r3-v1-20261025/reference-r4c-q100-r1`，结果详见后文。原程序SHA为`76810d3c77e0a358a99a50acd513d86531e0fd9d84a82011809fee4b76eed9d3`；439个源文件、输入、评分器和两模型逐SHA通过。原Q110命令只改QP和stream/recon两条输出路径；仍是mixed8/pruned/wide，不用当前fast代替。7200秒超时属于该批执行边界，不是验收门槛。该点与所有并发开发任务均不用于正式速度对照。



参考已从原六点扩至七点，重新计算Y4/wide密11点、r28密12点相对它的v1/NEG全域及93–95曲线、PCHIP与线性局部最坏。所有旧点保留；密度及其它插值不确定性见后文。



## 两条候选队列



1. 两个finalist的RDOQ后排序：独立r26基底，仅在fast、完整SB、实际两个候选时不延后RDOQ。复用原phase0和大矩形guard，不改其它候选池、λ、默认CDF或UV工具。唯一生产改动为`count>1`→`count>2`。有限正常/反序/三种真实故障恢复、旧路与fresh SAN通过；裁块8个count2未换赢家，先只做原生Q100，出现正信号后才补预定Q110/96。

2. 系数proposal：独立r26基底，仅把`refine_rdoq_fast`的phase0(λ/16)替换为既有phase1(λ/4)。不追加pass，不改全局λ、真实整SB/CDF/邻居验收、树池或proposal上下文。有限正常/反序/真实faultretry/publicfixed通过；fresh ASAN/UBSAN/leaks十次、旧rd0/1/2六CLI精确兼容通过。原生Q110/100/96已完成，质量信号混合，已封存，详见后文。



## 已完成的原生初筛



| finalist排序候选 | 原r28字节/v1 | 候选字节/v1 | 候选NEG | CPU秒（并发） |

|---|---|---|---:|---:|

| Q110 | 32491 / 93.180143 | 32724 / 93.195609 | 90.464457 | 32.7969 |

| Q100 | 41362 / 94.706097 | 41387 / 94.867491 | 92.307329 | 35.1563 |

| Q96 | 45747 / 95.433894 | 45872 / 95.478657 | 92.861287 | 35.6250 |



这三点不计算BD。独立审计以候选实际v1在原r28密12点上匹配体积，PCHIP/线性分别为Q110 **+0.482%/+0.544%**、Q100 **−2.435%/−2.151%**、Q96 **−0.571%/−0.845%**。Q96实际v1高于95；Q110 NEG下降0.104651、NEG同分体积约+1.9%，Q96 NEG及V PSNR也有微负，均保留。尚无全域1%门或正式速度通过结论。[五点独立审计](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-tx/native-five-audit-r1/REPORT.txt)。



### 整帧成本诊断



额外做一次Q100观察器编码，流、重建及CLI公开12项统计与上述生产候选精确一致。480个完整SB均实际有两个finalist，其中**10/480（2.083%）**在RDOQ后严格换赢家、无tie。新候选对冻结原r28整帧增加**1,274,886次FWD（+24.80%）**及21,114次coeff trial；不能由并发CPU秒数代替这一物理工作量证据。



同SB入口下，额外处理pre-loser实际用了1,181,152次FWD，最终换赢家的十条链只占38,239次（3.24%），其余未换赢家。十次纯排序J收益合1468；所有phase0优化前后J差106868包含原本winner的RDOQ收益，不能全部归因新排序。跨SB因果漂移使局部和不等于整帧D/R/J差。四次反转发生在原pre-winner受既有大矩形guard而不做RDOQ的情形，没有因此复活上轮大矩形RDOQ负候选。[完整观察器证据](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-rd/native-q100-observer-r1/result.json)。随后按此证据冻结通用J差距筛选并验证第二版，结果见后文。



## 跨内容基线



使用原开发集3D与Real帧，冻结当前r26/Y2/global2参数，各补一个点；不使用holdout，也不假定它们恰好等于94.5。



| 开发帧 | QP | 字节 | v1 | NEG | CPU秒（并发） |

|---|---:|---:|---:|---:|---:|

| 3D | 110 | 98783 | 94.517552 | 91.313055 | 35.3281 |

| Real | 100 | 79318 | 94.461052 | 92.701605 | 30.1719 |



匹配原AOM稀疏六点后的单点体积比，3D为PCHIP1.2205/线性1.1905，Real为1.0738/1.0451；仅诊断，不当BD或正式质量门。原AVM六点同样稀疏，两插值结果完整保存于[跨内容单点汇总](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/cross-content-singlepoint-summary-r1.json)。准备脚本r1仅括号语法错误、未创建目录或启动编码；r2单处修正后完成，失败源保留。



phase1在Q100两裁块的实际预算为：proposal writer origin242→501、BR502→748；target数60/345均不变，FWD仅+16/−1。没有新增RDOQ pass；writer增量会计入后续成本评估，不伪称同流或零成本。[独立阶段结果](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-coeff/fast-rdoq-phase1-r1/budget-r1/results.json)。



## 已有的新诊断证据



复用上轮Q100已存流的诊断解码表明，Y4/wide加入UV派生后，510个SB中186个几何布局改变、481个block tuple改变，Y叶8296→8486，约73.55%的Y像素改变。Y、U的SSE下降而V上升；v1的chroma SpEED误差与CAMBI均恶化。因此UV工具会通过RD/CDF/重建依赖改变Y和分区，不能把其效果当孤立UV核的效果，更不能由SSE/J下降保证视觉分数提高。源与码流未重新编码；只新增诊断解码产物。



MAIN生产代码仍为冻结r26。所有实验在独立副本中进行，候选未过既定门槛不合入。当前没有新的0.85×体积或2×速度通过结论。



## 后续执行与第二版筛选



用户已明确解除本轮提前收尾时限，允许必要实验持续到次日。质量与速度要求保持冻结。3FUI“加入编码队列”只在本轮所有实验和正式计时结束后执行一次，并验证结果，避免编码争用CPU。



phase1原生三点已完成：Q110为32856字节/v1 93.045676/NEG 90.626624，Q100为41383/94.887289/92.257642，Q96为45791/95.355545/92.863237。相对基线，Q110增大1.1234%且v1下降0.134467，Q100增大0.0508%且v1上升0.181192，Q96增大0.0962%且v1下降0.078349。质量信号混合，不扩曲线、不组合；并发编码CPU分别29.625/30.28125/31.46875秒，不作为正式速度结论。



finalist第二版在看后续QP及跨内容结果前冻结通用门：两树预估整SB代价差不超过较优者的1%，才对第二树追加原phase0 RDOQ；第一树保持原RDOQ流程。这里1%是搜索筛选阈值，与最终同画质体积的1%质量约束不同。离线Q100观察中该门覆盖全部10次反转，预计可省去额外第二树约80.86%的FWD工作，但这尚不是第二版原生实测。改动限于同一个函数，451文件身份已核；静态独审、有限动态与fresh SAN已通过，原生结果见后文。



### 原参考Q100完成与后续补点



原r4c Q100实测42574字节，v1 **94.884128**，NEG **92.193969**；编码CPU2337.4375秒、墙钟2372.6580秒，属并发参考采样，不能与新fast作正式速度比。固定解码逐像素一致，独立审计核439文件/argv/SSE/features通过。



保留原六点加入Q100后，Y4/wide密11点相对参考的v1全域连续最坏，PCHIP为**−0.224471%@94.814606**、线性为**−0.883972%@74.466941**，两者目前均通过1%数值门；此前稀疏PCHIP的+1.681786%争议因此真实点改变。参考目前仅两点处于93–95，尚未满足四点密度，不能据此宣布正式验收完成。r28全域仍失败：PCHIP最坏+6.302898%、线性+6.570043%。



在看新增结果前冻结并启动原r4c **Q105、Q102**：105细化原110..100段的方法分歧，102细化上半段；仍为原程序/mixed8/pruned/wide，只改QP及输出路径。新六点批次还包含v2的2D110/96、3D110、Real100，全部作为质量诊断，不用于正式性能。



上述补点现已完成：Q105为**37854字节/v1 94.047435/NEG91.316188**，CPU2123.546875秒、墙钟2344.066653秒；Q102为**40467/94.458854/91.790200**，CPU2308.703125秒、墙钟2348.444371秒。独立只读脚本核全部439文件、像素、SSE与features通过，没有重复媒体计算。



原参考9点含4个带内直接点、r28密12点含5个、Y4/wide密11点和AOM密11点均含4个，各曲线两端有包围。Y4/wide相对原参考的v1全域连续最坏：PCHIP **−0.300282%@94.882006**、线性 **−0.883972%@74.466941**，两法均满足1%数值门与最低密度要求；其高区间相对原参考分别改善4.6643%/4.6444%。这不证明插值误差全消，也不替代holdout。相对AOM的93–95体积仍为PCHIP **1.197035×**、线性 **1.198585×**；旧普通0.6.1的1.09×不能移植为新v1结果。r28全域最坏仍+6.3029%/+6.5700%，不能据高区间过门掩盖宽域失败。[9点参考独审](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-tx/reference-q100-audit-r1/followup-r1/curves.json)。



### 第二版原生结果：收益并非跨点一致



v2有限正常/反序/真实错误恢复/fixed/public/fresh SAN全部通过。原生Q100流与内部重建、官方解码逐SHA等于第一版；D/R/J及赢家哈希相同。FWD由6416175降至5461095（**−14.8855%**），coeff trials由43299降至26604；编码CPU31.96875秒仅作单次诊断。



| v2点 | 字节 | v1 | NEG | CPU秒（并发） |

|---|---:|---:|---:|---:|

| 2D Q110 | 32724 | 93.195609 | 90.464457 | 30.640625 |

| 2D Q100 | 41387 | 94.867491 | 92.307329 | 31.968750 |

| 2D Q96 | 45588 | 95.213052 | 92.896357 | 27.843750 |

| 3D Q110 | 98824 | 94.379972 | 91.300540 | 37.078125 |

| Real Q100 | 79474 | 94.618114 | 92.717004 | 35.906250 |



2D Q100保留第一版约2.15–2.43%的v1同分体积改善，但Q96在其实际v1=95.213052上相对r28密12点变差约2.75–3.01%，即使NEG同分改善约0.88–0.95%也不掩盖主指标负结果。3D同QP体积+41字节而v1下降0.137580；Real体积+156字节且v1上升0.157062，两者只有单点，不能算BD。该固定筛选规则暂不扩完整曲线、不组合、不根据已见QP结果改阈值。自然赢家分支观察已闭合：480个完整SB中near93/far387，93次实际恢复runner后10次获胜、83次落选；10个获胜SB的最终plan与commit J、全帧码流/重建及12项公开统计均与生产v2精确一致。该正确性证据不改变混合质量结论。



## 最小CCSO机制与离线潜力



固定AVM v1源码审查表明，可先只接单64×64单元、420 UV、BO-only四亮度band、scale0、offset={0,+1,−1}的内部强制闭环。Y关闭、不接公共CLI、不叠候选搜索，先证明序列/帧/tile flag语法、官方ctx0、全帧预测结束后的滤波顺序及内部recon==固定解码；默认off必须与原流精确桥接。多SB与1080p编码尚未放行。[官方语法及实现边界](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-tx/ccso-readonly-r1/REPORT.txt)。



该机制首版在真实固定解码中暴露帧头落点错误：CCSO块被放在reduced_tx_set之后，正确顺序是deblock→GDF/CDEF/restoration→CCSO→TXmode→reduced_tx_set。先前静审只核块内位序而漏掉完整调用点，不能据其“未见阻断”准入。原失败源/日志与更正记录均保留；首fixture的444系数arena不足也仅在外置harness修复，旧失败exe未在重编前另存，不能声称可重放该旧exe。



独立source-r2只移动CCSO块，其余452文件与首版相同，manifest为`65be5bb598b6a6168c85285528c37c673ca8a190cee9e027705f5ee327c9161c`。修正后**32条CCSO流+12条oldoff流，44/44固定AVM逐像素通过**，normal/reverse 120个流/重建/未滤重建文件SHA相同；rate/prefix、晚期失败后retry控制也通过。官方ctx轨迹、独立primitive与fresh SAN尚待齐全。单unit直接raw-CDF率估计仅证明本实现full/prefix事务一致，不冒称官方adaptive adjusted Q12成本精确；未来多SB/RD须重新核。



单unit机制后续已闭包：官方32流trace中48次enabled UV读取全部ctx0且每plane一次；独立官方BO核8构造/16384个UV像素/320000个含padding存储字节相等；fresh GCC ASan/UBSan/leak完整44流与四旧header/tile单元通过，120产物精确等MSVC；三格式公开oldoff对冻结r26流/重建/统计及fixed也相等。[单unit最终交付](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-tx/ccso-bo-forced-r1/HANDOFF.txt)。



下一独立多SB机制包仅增加6文件71增18删：保持最大128×128、既有8对齐420 partial、单帧单tile，固定四band/scale0/±1，不搜索、不原生。immutable unit map按raster借用，96字节独立CDF按现TileTree值事务复制；adaptive flag率改为adjusted probability与canonical update。ctx0/2的左邻简化必须先由官方多SB trace和fixed证明，不能沿用单unit结论。当前在有限48on+12off、边界/事务/错误恢复/独立率与SAN验证中，尚未声称支持1080p。



只对已存重建做有限数值分析：在固定四band内穷举81个±1 LUT与理想每UV32单元on/off，2D Q100的U/V SSE最大可降1.2008%/1.9225%，3D降1.0143%/1.3380%，Real降0.3154%/0.4523%；未计header或flag率。放宽到完整scale0码表的4096种LUT基本无额外收益。这里是该固定子集的失真界，不是VMAF或压缩收益上界。若先按全部像素训练LUT再选flag，会在2D/Real得到全零LUT；联合选择与该简单训练不同。



将上述事先按SSE选择的±1 LUT/理想flag应用为**尚未编码的假想raw重建**，另做三次双模型评分：2D v1+0.047740、3D+0.090718、Real+0.039938，NEG均精确不变，Y像素不变。没有按VMAF选择参数，没有新编码或解码，没有合法流及码率证据；这些分数只支持继续最小机制验证，不能用于验收或合入。[有限潜力分析](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/ccso-bo-bound-r2/results.json)、[假想重建评分](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/ccso-bo-visual-probe-r1/results.json)。



## 真实系数入口上下文：先计实际成本再实现



phase1强度实验失败后，不调整其λ复试。另一独立缺口是当前proposal率使用默认CDF、零邻居，V的U非零上下文也固定零，而最终整SB率才是真实上下文。真实入口必须包括预测语法、前序Y/U系数及共享EOB_EXTRA，不能简单挂一个CDF指针或把Y全部提前。



外置test-only计数器在唯一2D Q100原生编码中与原r28码流/重建及12项公开统计精确一致，测得真正的`rdoq_rate`入口**54695次**，全部phase0：Y37603、U8523、V8569。它不同于22185次coeff trial，不能混用。每probe完整恢复23784字节CDF和1920字节neighbour，逻辑复制量约**1.406 GB（读写合2.812 GB）**；并非实际DRAM流量或净耗时。[计数证据](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-rd/rdoq-rate-call-counter-r1/result.json)。



据此批准独立r26最小原型：每target准备一次真实plane系数入口，每probe只调用原目标coeff writer并恢复上下文；复用frame-owned prefix，不引入dirty-undo框架，不逐probe重写整SB，保phase0/largeguard和最终整SB像素J验收。先code-ready静态审及有限writer率/状态等价、反序/错误恢复、oldoff/fixed/SAN，后才放唯一原生Q100。当前不叠加finalist筛选、UV派生或CCSO。



真实prefix-r2三文件139增12删已通过静态及有限动态闭环；451源manifest为`86f90451908b35d468d32cc86688673f630946c1943068566395ed0a50f0e12d`，app为`6695e2d2c374a56b8d6d0225442268d6373897786bf1736331b27544c88f163f`。目标coeff入口率增量、CDF及neighbour与独立完整writer逐项相等，每平台646522检查，含U零/非零、共享EOB_EXTRA及typed EOB0/1/AC；正常、反序、真实错误后retry、固定像素与freshSAN通过。



| 真实prefix点（Y2/global2控制） | 字节 | v1 | NEG | CPU秒（质量诊断） |

|---|---:|---:|---:|---:|

| 2D Q110 | 32780 | 92.741679 | 90.433088 | 29.093750 |

| 2D Q100 | 41308 | 95.054533 | 92.358050 | 31.546875 |

| 2D Q96 | 45693 | 95.353316 | 92.821127 | 30.765625 |

| 3D Q110 | 98808 | 94.573685 | 91.279688 | 35.640625 |

| Real Q100 | 79269 | 94.639244 | 92.718693 | 33.515625 |



Q100相对原r28在其实际v1上匹配体积改善4.8617%/4.8340%，NEG同分改善1.2890%/1.4127%；但Q110 v1同分退化6.6317%/6.6308%、Q96退化1.2402%/1.0025%。Q110实际已低于93、Q100和Q96高于95，全部保留，不外推或将三点称BD。3D v1+0.056133但NEG−0.033367，Real v1+0.178192且NEG+0.017088，只有单点，不能据此全域放行。[完整初筛](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/true-prefix-initial-matched-r1.json)。



原Y2控制本身未通过新v1宽域约束，因此另做有界的**已冻结Y4/wide控制基底桥接**，不调λ/阈值或按QP定规则。独立审计证明r25→r26仅Y shortlist原5行top4→top2；feature三文件13hunk在r25逐行精确匹配，不与该改动重叠。新桥接只保Y4及原`--tree-refine wide`命令、加同一feature；不混其它新候选，不继承控制的质量通过结论。有限必要桥接后只放Q110这个已见最坏点，若仍退化即停止，不自动扩100/96或整曲线。[基底桥接只读证明](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-coeff/true-prefix-y4-bridge-readonly-r1/REPORT.txt)。



该桥接严格构建及必要normal/reverse/真实fault/oldoff/fixed/freshSAN通过后，唯一Q110实测**32265字节/v1 93.112554/NEG90.535576**，控制为32396/93.188800/90.450754。主v1同画质体积仍退化**0.335103%/0.395308%**，NEG改善1.260866%/1.155446%。按先冻停止条件封存整个真实prefix家族，不补100/96或曲线，不调整规则，不合入。[最差点结果](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/true-prefix-y4-q110-matched-r1.json)、[停止闭包](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-rd/true-prefix-family-final-r1/closure.json)。



五点独立特征审计进一步表明，Y2 Q110不仅主分数下降，最终D/R/J也分别增加0.1928%/0.8923%/0.5160%；Y SSE变差而U/V SSE改善，但chroma SpEED特征由2.623826升至2.974305。Q96则J略降、两模型仍降。真实的局部入口率不保证跨SB最终统计改善，UV像素SSE改善也不保证结构特征改善。主v1模型使用CAMBI、chromaUV、ADM3及motion四输入，特征方向分裂，RBF不是加和模型，不归因单项贡献。[五点全部特征审计](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-coeff/true-prefix-fivepoint-readonly-r1/REPORT.txt)。



## 待释放的正式速度窗口



固定Y4/wide r25与AOM cpu0，各六个覆盖v1 93/93.5/94/94.5/95的实际点；每点一次独立暖机、五次新进程，共12次暖机+60次计时。先另做6次AV2非计时recon导出与固定解码桥接，计时双方均stream-only；AV2计时流和公开统计须与桥接精确相同，评分/哈希不计编码时间。两臂交错并逐轮反序，父子亲和mask256，记录AC与电源方案、CPU/wall的raw/median/min/max。



CPU窗口仅在原参考全部完成、质量门复核且所有子队列CPU空闲后释放。当前机器快照为Ryzen7 7840HS、8核16线程；不能套用其它设备的秒数。准备完成不等于测量通过，也不构成2×达标结论。





## CCSO多块机制闭包与原生自动选择准备



多SB机制已完成，独立source-r2仅将内部frame header临时容量从8改为16字节；静态计算的65位极值需9字节，原未构建草稿不能称动态失败。最终61条流全部固定AVM逐像素通过，normal/reverse各约344万检查、171产物SHA相等；官方49条on流共224次flag读取，实际ctx仅0/2且每unit的enabled UV各一次；独立BO核24构造、约360万检查含padding存储精确，fresh ASan/UBSan/leak及旧off三格式公开流/重建/统计桥接均通过。它仍是最大128×128的强制机制，不能据此宣称1080p自动选择或压缩收益。[机制最终闭包](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-tx/ccso-bo-multisb-r1/closure.json)。



原生auto独立版本已冻结453文件，基底为原r25/Y4wide，rd_frame、公共API/CLI、λ和其它算法均保持原样。使用单个约100KB heap，按真实clip后SSE表枚举每UV的81个固定LUT，raster flag贪心及四subset选择；writer和chooser共用canonical CDF及adjusted Q12。只生成一个on提案，最终必须以原λ、全帧失真及完整off/on实际包字节严格比较，接受才apply一次，拒绝则用原off wrapper重写。最多三次完整writer及容量重试、扫描和额外UV一致性复核都计编码成本。预估头不含未知tile长度/熵终结，不冒称全局最优；SSE/J改善也不保证v1改善。当前仅code-ready，静审与新增域/生命周期/固定解码验证通过前不运行原生媒体。[设计及边界](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-rd/ccso-native-auto-r1/DESIGN.txt)。



为避免将旧Y2/global2误当Y4/wide跨内容对照，本轮先补原r25控制：3D Q110为98391字节/v1 94.704965/NEG91.315009；Real Q100为79329/94.736682/92.611070。二者固定解码/全部特征finite通过；并发CPU39.671875/38.609375秒仅属质量诊断，单点不计算BD。[匹配基线](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/cross-content-y4-baseline-r1/results.json)。



## 追加的有界预筛遗漏诊断（Q2）



只读核查纠正一个细节：基本Y并非固定7个候选，允许shape中最多13个合法模式先以SAD选top4并保进入mode，再做Y-only DCT真实prefix J；后面的角度、MRL、TX仍串行，不为已淘汰基本Y重开完整工具链。由此批准独立原r25/Y4wide源的一行YALL诊断：只放开原SAD mask，保原合法域、DCT-J、UV2、树池、工具和RDOQ时机、lambda全部原样。它检验SAD层遗漏及实际开销，不声称解决联合Y/MRL/TX或树排序问题。



先冻结代码并静审，必要有限验证通过后才由root放唯一2D Q100。更宽搜索有质量信号才用独立observer定位廉价筛选方向；不在质量app中加入巨量逐叶日志，不为无信号方向先做完整矩阵，不叠CCSO或前述封存候选。[范围与停止含义](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-tx/q2-basic-wide-readonly-r1/PLAN.txt)。



### YALL首点正向，四点扩展后停止铺曲线



一行候选451文件的有限正常/反序/真实错误恢复、partial/旧rd2与rd0、fixed及freshSAN全部通过，app为`5e21a0c1c6b5bf5453d20e5be1e7580ce738563cb5871c530e73a0e5452fa18c`。唯一Q100实测40498字节/v1 94.821908/NEG92.313053；对原Y4控制40883/94.666762/92.309126，主分同质体积改善3.724656%/2.957577%，NEG改善1.006782%/1.017617%。但FWD5626835→6754023（+20.0324%），CPU45.65625秒仅质量诊断；D/R/J下降仍不能保证其它点的视觉分数。[首点独审](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-coeff/q2-yall-q100-readonly-r1/REPORT.txt)。



按预声明补2D Q110/Q96及原Y4 paired跨内容：



| YALL点 | 字节 | v1 | NEG | CPU秒（并发诊断） |

|---|---:|---:|---:|---:|

| 2D Q110 | 31927 | 92.965083 | 90.588925 | 43.562500 |

| 2D Q96 | 44966 | 95.412108 | 92.835913 | 45.625000 |

| 3D Q110 | 98167 | 94.467485 | 91.345686 | 50.484375 |

| Real Q100 | 80280 | 94.639038 | 92.691344 | 50.781250 |



Q110在其实际v1（低于93）匹配原完整11点，体积退化0.832918%/0.892159%；Q96改善0.331930%/0.357579%。3D同QP少224字节而v1下降0.237480；Real多951字节且v1下降0.097644。单点跨内容不BD，也不将它们塞入2D曲线。因此停止YALL密曲线/新筛选阈值/合入；仅完成已批准唯一Q100外置observer，定位真实SAD遗漏及代价，不能把局部正向当可发布质量/速度收益。[四点及保留的负结果](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/q2-yall-followup-matched-r1.json)。



### CCSO原生首点（最终错误路径闭包仍待补核）



原生Q100已固定解码逐像素通过：40968字节/v1 94.822057/NEG92.309126；UV全局LUT44/50，510unit，实际mask3并严格接受，完整off40883/on40968字节，Jbytes28205343→28188684。Y和原public RDstats/FWD保持控制，UV SSE6117451/6042703→6038297/5952371；新增85字节。主v1同分体积改善2.609572%/1.833288%，NEG同分代价+0.207910%。CPU35.015625、墙钟35.427880秒均属质量诊断，不是正式速度比。[首点完整结果](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/native-ccso-auto-q100-r1/result.json)。



有限验证已报1461controls与789787桥接检查、18新fixed流、9公开程序case及freshSAN通过，root据owner初报开始该唯一原生点。随后owner封包复核发现旧“late_NO_SPACE”夹具的full包字节减1会撞上prefix+16余量guard，属于真实早期容量失败，不能宣称coeff后late失败。原结果/失败与夹具修订保留，生产453/app未变；批准只补两格式的真实entropy-finalizer后段失败、完整重试与最小freshSAN。最终闭包前本原生结果暂列provisional，不能以旧标签冒称所有动态门已通过。



### CCSO有限闭包完成与完整评估范围



最终有限闭包已通过，453源/app与原code-ready完全相同：每平台1461 controls、789787共享桥接、独立324真实late检查均零失败；18新流、2真实late原off流及9公开case×2程序固定像素一致。root核最终closure、programfreeze、全部源及证据SHA，Q100独立Python clip/CDF/Q12/原全帧bytesJ审计通过；原“late”错标签与后补证明均保留。当前原生Q100不再依赖暂定闭包状态。



四点扩展得到：2D Q110为32481/v1 93.293859/NEG90.450754（主同分相对Y4改善0.647904%/0.617185%，副代价+0.262378%）；Q96为45313/95.419284/92.828698（主对Y4退化0.333806%/0.315912%，副+0.145866%）。3D Q110为98476/94.765919/91.315009，比Y4多85字节、主+0.060954、副相同；Real Q100为79329/94.736682/92.611070，完整流与Y4精确相同。全部点保留，跨内容单点不BD。



这里需明确执行修订：root四点脚本内的“mixed quality停止扩展”措辞过于粗略，不应被当作相对Y4任何退化即失败的新质量红线。用户冻结的是相对**原r4c最多1%**，Q96在其实际质量对r4c9仍改善1.564572%/3.094792%，而主band两点改善、3D改善、Real相同。故保留旧协议与全部结果、在新8点结果出现前另冻结一份[评估追加说明](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/ccso-auto-evaluation-addendum-r1.json)：对**同一个未改候选**补全部剩余8个原控制QP，直接评估完整11点的实际用户约束；不调LUT/λ/参数，不换模型/参考/门槛，不删Q96或任何不利点。这是追加完整约束评估，不将局部结果重新标成PASS。



仅完整曲线满足原质量预算后才准备候选专属正式计时方案，明确标记CCSO而非把r25标签偷换。正式速度窗口尚未释放，3FUI仍待所有本轮测试结束后最后点击。



### CCSO完整11点初步数值



所有剩余8个原QP均已实测，未删Q96，不改候选。原生11点包含QP110/108/104/100四个v1 93–95直接点，QP113/96双端包围；原r4c9、Y4wide11、AOM11均满足同样最低覆盖。根计算主v1相对原r4c连续全域最坏为PCHIP **−1.053507485%**、线性 **−0.885784660%**；NEG为−1.571456181%/−1.774240768%，当前数字满足1%预算，独立逐点/曲线复核仍在进行。



主v1 93–95对Y4wide的BD体积改善约**0.993992%/0.996449%**，对AOM体积为**1.185136518×/1.186641671×**，尚远离0.85×。NEG的93–95对Y4wide代价+0.091654%/+0.085014%；NEG对AOM只有到93.848370的共同域，不外推95。更高QP190/160/70实际选择off且保原流，其余8点实际接受一个或两个UV plane；全部无新增FWD。这里原生编码CPU约28.45–41.48秒仅作诊断，不作为正式2×判据。



[完整输入与根计算](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/ccso-auto-fullcurve-r1/summary.json)。只有独审通过后才创建root质量gate及候选正式计时方案，目前没有正式速度结果，亦没有holdout/I-GATE或MAIN通过声明。



## 本轮收尾



正式五轮及独立重算完成，全部60计时样本保留。最终结果与下一步入口见[最终报告](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/docs/task-reports/M6-intra-v1-r3-final-20261025.md)。所有测试结束后已通过computer-use点击一次3FUI“加入编码队列”，界面确认添加82个任务；三个子队列均空闲，本轮停止。总目标0.85/2尚未达到，未触holdout/帧间/MAIN新合入。

</details>


<details>
<summary>012 · M6-intra-v1-r3-final-20261025.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\task-reports\M6-intra-v1-r3-final-20261025.md`  
原文件 SHA-256：`b19e3d3b9b6fc110e74f8ec152e55600da48bc60fd114ddea54b401a19ebe7a6`

# AV2单帧第三轮最终报告与下一步入口



本轮编码、评分、完整曲线及正式重复计时均已完成，独立审计通过；**0.85×体积、≤2×耗时的总目标未达到**。主模型固定 `vmaf_v1.0.16_3d0h`，副模型 `vmaf_v0.6.1neg`，质量参考仍是原r4c，允许最多1%同画质体积回退。



## 最终体积与用时



同一1920×1080、10-bit 420的2D开发帧，AOM固定3.15.1/cpu0/ALL-INTRA/threads1。当前实验候选是原r25/Y4wide加最小BO-only CCSO；主v1 93–95的BD体积比为 **1.185136518×（PCHIP）/1.186641671×（线性对数）**，比安全Y4控制再改善约**0.994%/0.996%**。五个固定质量位置的CPU比为 **2.901824–3.035794×**，墙钟比 **2.894406–3.022779×**。



以下P为PCHIP(log bytes/log time)，L为分段线性(log bytes/raw time)。体积使用完整11点曲线；时间使用六个实际点各五次新进程的中位数。表中同分位置是插值，均有真实点包围，不外推，不能与实际QP样本混同。



| v1同画质 | AV2/AOM体积 P/L | AV2/AOM CPU P/L | AV2/AOM墙钟 P/L | AV2 CPU秒 P | AOM CPU秒 P |

|---:|---:|---:|---:|---:|---:|

| 93 | 1.189233/1.187229 | 3.003995/3.035794 | 2.990942/3.022779 | 33.631088 | 11.195455 |

| 93.5 | 1.156517/1.158798 | 3.007079/3.005897 | 3.001710/2.998739 | 33.858280 | 11.259524 |

| 94 | 1.164001/1.169606 | 3.001519/2.993002 | 2.991068/2.982858 | 34.211094 | 11.397926 |

| 94.5 | 1.201034/1.200010 | 2.941400/2.939970 | 2.929687/2.928349 | 34.539497 | 11.742535 |

| 95 | 1.261299/1.253823 | 2.909604/2.901824 | 2.903483/2.894406 | 35.363421 | 12.154031 |



95分附近仍比AOM大约25.4%–26.1%，高区间均值不能掩盖这一点。旧“1.09×体积、约2.7×时间”来自普通0.6.1及Y2/global2口径；新v1下该搜索配置未过原质量约束，不能把旧数字直接移植到本表。



## 正式计时证据



本机实际为Ryzen 7 7840HS（8核16线程），Windows11，AC供电、同一Balanced方案，父子进程固定逻辑CPU8/mask256。先6次AV2重建导出与6次固定AVM解码桥接，再双方12次独立暖机、60次正式计时，共78个编码进程；计时双方均stream-only。五轮交错、逐轮反序，所有码流和公开统计与质量样本一致，CCSO私有字段仅排除ticks、保留hz后也精确一致。评分及编译不计编码时间。



| 实际点 | v1 | 字节 | CPU中位秒 [min,max] | 墙钟中位秒 [min,max] |

|---|---:|---:|---:|---:|

| ccso-q113 | 92.795741 | 30706 | 33.593750 [33.453125,33.750000] | 33.616066 [33.543578,33.862044] |

| aom-q30 | 92.791465 | 25458 | 10.843750 [10.812500,12.812500] | 10.904757 [10.866683,12.849930] |

| ccso-q110 | 93.293859 | 32481 | 33.734375 [33.703125,33.843750] | 33.846036 [33.774088,33.892620] |

| aom-q28 | 93.250899 | 27733 | 11.375000 [11.343750,11.390625] | 11.449716 [11.372326,11.454958] |

| ccso-q108 | 93.824594 | 34076 | 34.109375 [34.046875,35.640625] | 34.213960 [34.133964,35.735173] |

| aom-q27 | 93.625826 | 28982 | 11.218750 [11.187500,12.625000] | 11.276168 [11.253119,12.660795] |

| ccso-q104 | 94.492240 | 37755 | 34.531250 [34.421875,34.671875] | 34.542132 [34.481909,34.752746] |

| aom-q26 | 94.225022 | 30515 | 11.562500 [11.562500,11.578125] | 11.633163 [11.607893,11.661461] |

| ccso-q100 | 94.822057 | 40968 | 35.156250 [35.125000,35.281250] | 35.184670 [35.161726,35.324070] |

| aom-q25 | 94.659378 | 32122 | 11.859375 [11.812500,11.953125] | 11.898171 [11.883483,12.029445] |

| ccso-q96 | 95.419284 | 45313 | 35.656250 [35.437500,35.703125] | 35.772546 [35.469653,35.803462] |

| aom-q24 | 95.144493 | 34350 | 12.296875 [12.265625,12.437500] | 12.351689 [12.336929,12.471817] |



全部五次raw值保存在[正式原始结果](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/formal-ccso-aom-native-v1-r1/results.json)，独立重算见[性能交接](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-coeff/formal-ccso-result-readonly-r1/PERFORMANCE-HANDOFF.txt)。第五轮AOM Q30/Q27及AV2 Q108出现慢样本，全部保留、未挑样本/补跑；中位数、min/max分列。系统中原有两个Python GUI进程未停止，预窗口采样其中一个有少量CPU活动，已记录；独占指项目测试队列全部停止，不能宣称所有系统后台绝对空闲。温度、package power未取得，不推断慢样本原因。



## 性能如何走到现在



| 步骤 | 实际变化与意义 |

|---|---|

| 减少重复工作 | 基本模式使用真实熵前缀增量；目标叶局部试算、因果提交，减少反复重建整SB后缀；分区校验移到入口。 |

| 精确复用 | UV试算复用不变Y重建/系数/SSE；完整因果前缀的有界memo复用局部评分；零系数、尾部零频率及常量残差走已证明的精确路径。 |

| 分层搜索 | Y/UV便宜预筛后仍用真实率失真；RDOQ推迟到胜树，longrect有限24树；这些会改赢家，因此受原r4c质量预算约束。 |

| 内核和常见开销 | IPO与sample inline，省不消费的参考构造和basic三处整SB恢复，10-bit预测直接写uint16；可移植AVX2 quant及等价roundshift已加入。 |

| 新模型校正 | Y2/global2在新v1宽域不合格，回到Y4/wide安全控制；原r4c由6补到9个真实点，解决稀疏插值对1%判断的分歧。 |

| 本轮新增 | 接入最小合法CCSO：先强制单块、多块，再原生自动选择；不增加FWD，用实际完整包字节J严格决定开关，带来约1%主band改善。 |



同QP100的工作量例子：原r4c为8680343候选、363464162次FWD；安全Y4及新CCSO为59922候选、5626835次FWD。它证明大量计算已去掉，不是同条件速度倍数；原r4c的2337秒是并发单次诊断，与本轮35.15625秒的五次CPU中位口径不同。历次时间和同流优化的配对窗口详见[历史性能报告](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/docs/task-reports/M6-intra-speed-pause-20261025.md)，其中普通0.6.1仅作历史。



## 本轮保留与停止的方向



- **保留CCSO实验候选证据**：固定四亮度band、UV offset0/±1、每plane81 LUT、逐单元自适应flag；单heap100304字节。完整11点主v1对原r4c连续最坏为−1.053507485%/−0.885784660%，NEG也过数值预算；主band四点及双端包围满足最低要求。NEG对AOM共同域只到93.848370，不报告其完整94–95比较。3D单点主分改善，Real完整流保持控制；这不是全数据集或holdout通过。

- **停止YALL整曲线/合入**：仅放开基本Y的SAD mask，Q100主同分体积改善2.96%–3.72%，FWD却增加20.03%；Q110主退化，3D/Real也不一致。外置observer证明原mask有真实遗漏，约26.86%的basic叶访问选到mask外模式，主要SMOOTH；只有前4096条细节、95个最终赢家叶的有界关联，不能外推全帧阈值或因果收益。

- **停止两个finalist RDOQ、phase1与真实coeff-prefix扩展**：有限正确性通过，但原生点/跨内容的主v1收益混合；Y4桥接最坏点仍未给主指标收益。不按结果改λ、QP专用门或混合负候选。



完整过程和失败保留见[持续记录](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/docs/task-reports/M6-intra-v1-r3-20261025.md)。CCSO有限验证曾把早期NO_SPACE误标为late，封包时已更正；另补真实四叶写完、熵finalizer入口error0后返回NO_SPACE、完整重试/分配失败和freshSAN。两平台1461 controls、789787共享CDF/率桥接及324真late检查均通过，固定AVM像素闭环通过，生产实验源未因夹具问题修改。



## 下一步优化空间



1. **先定位分层筛选的实际遗漏和成本。** 继续以Y4/wide及本轮CCSO为可复核控制，针对主v1 93–95的真实点和新开发内容。Q2表明单纯扩大SAD候选既慢又不稳定；有界记录的SAD/cut分布不能直接变成全帧门槛。下一诊断应区分SAD层遗漏、DCT粗J淘汰但联合角度/MRL/TX后才好的模式，以及树在RDOQ前后排序的影响，并记录实际最终赢家、FWD和完整包成本。

2. **检查联合工具与分区表达。** 只给少量额外合法mode seed完整工具链机会，必须从相同规范因果前缀重放；当前同decision去重的seed池不能冒充已支持多mode联合。分区按相同空间区域比较官方AVM的几何、残差、符号率，先找当前有限24树遗漏，避免枚举所有树或用固定坐标缓存。

3. **按新的实际热点做内核/早停。** 本轮正式结果给出仍需约三分之一降耗的量级；已有低收益S1/S2/UV严格下界、PGO等负结果继续保留。新内核或更大cache先有当前控制上的热点/饱和计数证据，再做同流交错整帧测试；不累乘微核倍数。SSE/J改善不能代替v1/NEG实测，色度SSE下降也不能保证结构分数变好。



冻结模型、原r4c参考、1%预算、0.85体积/2×耗时及全部坏点保持。仍先开发集，最终候选冻结后才进入holdout20/I-GATE；本轮未触盲测、未推进帧间/GOP/多线程。



## 文件与最终操作



实验程序：[SvtAv2EncApp.exe](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/agent-rd/ccso-native-auto-r1/build-r1/bin/SvtAv2EncApp.exe)，SHA `51def9e0c6015be713744980fa80475beca1a891fc8cf1c774b96648abfc8f5e`；453文件源manifest `cad286f01ea20af2c71d73dc0a65bc8640ebade90567cbfa2124f80cd1d431cd`。仍为隔离实验程序，无新公共CLI；现有Y4/wide实验参数才触发此内部子集，严格域外走原off。MAIN的Source/include/app/CMake代码已逐SHA核对保持r26，本轮未合入新算法。



所有编码、评分、正式计时及独审完成后，已使用computer-use找到FFmpegFreeUI，**点击一次“加入编码队列”**。立即刷新的界面明确提示**“已添加 82 个任务”**，准备文件列表已清空；没有重复点击。此后未启动任何基准测试。队列加入结果已核验，不额外宣称未观察到的编码进度。



[操作记录](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/3fui-queue-click-receipt-r1.json)。本轮会话到此停止，三个子队列均空闲，无继续测试或自动任务。

</details>


<details>
<summary>013 · 04-M6-intra-only-first.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\04-M6-intra-only-first.md`  
原文件 SHA-256：`94cd2ac0a6b524f5680a7cc4770fd83edd3c06c4e2347cfd02106173a2da7e99`

# M6 新阶段合同：先做 AV2 单帧 / ALL-INTRA，达标后才开发帧间

> 编制时间：2026-10-20。此合同优先级高于旧路线中先做亚像素/多参考/B 帧的排法。  
> 项目：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2`。  
> 执行方式：NuxBox1 本地 AI，00 集成 + A 编码内核 + B 规范/评测。  
> 本阶段范围：**只有一张独立图像/单帧闭环**，内部固定单线程。没有通过 I-GATE 前，不推进 MV 搜索、多参考、B 帧、GOP、时域前瞻等帧间项目。  
> 本文件是研发合同与验收目标，不宣称 15% 收益已经发生。

## 1. 为什么先从单帧开始：本地实现差距是可证的

本轮只读核查截至 2026-10-20 的实际源码、旧合同与 AV2 v1.0.0 规范，确认如下事实：

1. **独立单图码流路径已经存在。** `Source/Lib/Bitstream/sequence_header.c` 有 `single_picture_header_flag=1` 分支；`Source/Lib/Bitstream/keyframe.c` 有对应 single-picture path。这可以直接作为 still-image 验收入口，不必为了测试图片先实现帧间。
2. **帧内预测远未完整。** M5.1 的真实编码闭环主要支持 Y 的 DC / Vertical / Horizontal 自适应选择，UV 仍为 DC；`Prediction/intra_prediction.h` 的基础枚举还明确只有 DC 核心模式和实验性 scalar axis helper。它不是完整 AV2 帧内预测工具集。
3. **规范定义的多种帧内开关目前全部关闭。** `sequence_header.c` 的多帧与 single-picture 分支均把 `enable_dip`、`enable_intra_edge_filter`、`enable_mrls`、`enable_cfl_intra`、`enable_mhccp`、`enable_ibp` 写为 0。
4. **主要帧内变换/系数工具没有在生产闭环中启用。** 当前序列头把 `enable_fsc`、`enable_idtx_intra`、`enable_intra_ist`、`enable_cctx`、`enable_tcq`、`enable_parity_hiding` 写为 0；`Encoder/transform_path.c` 的现有生产路径标识为 DCT_DCT，并使用单一 forward/quant/inverse 接口。增加 enum 或打开语法位不等于算法已实现。
5. **滤波也不是现有能力。** 当前 sequence flags 关闭 CDEF、GDF、restoration、CCSO；当前 frame header 的 deblocking 参数也被编码为 0。对单图的显示重建与相同分数下的率失真都有潜在价值，需要逐项验收。
6. **单图还有 AV2 独有机会。** v1.0.0 正式规范定义 DIP（data-driven intra prediction）、MRLS（multiple reference line selection）、IBP（intra bi-prediction）、CfL、MHCCP、FSC/IDTX-intra、intra IST、CCTX、TCQ 和 parity hiding 等。它们并不是当前三模式预测 + DCT_DCT 路径的等价别名。另一方面，`enable_inter_ddt` 只出现在非 single-picture-header 的多帧序列语法中，因此**不要将 inter DDT 当作本阶段的单帧开发项**。
7. **主对照直接使用 libaom 自己的 `aomenc`，不使用 `avifenc`。** 官方 `aomenc` CLI 的 `--ai` 选择 `AOM_USAGE_ALL_INTRA`；all-intra 模式强制单 pass 并关闭多帧前瞻，适合作为单张高压缩图片的主参考。使用 `--threads=1`、`--cpu-used=0` 和 raw OBU 输出，让速度与码流大小都直接来自 libaom 编码器；不引入 AVIF 容器、元数据或 libavif 的质量映射。

官方依据：
- AV2 v1.0.0 正式规范（2026-05-28）：https://av2.aomedia.org/v1.0.0/index.html
- 固定参考软件 AVM v1.0.0：https://github.com/AOMediaCodec/avm/tree/v1.0.0
- libaom 官方 `aomenc` 源码（`--ai` / `AOM_USAGE_ALL_INTRA`、`--obu`、线程与 pass 处理）：https://aomedia.googlesource.com/aom/+/refs/heads/main/apps/aomenc.c
- SVT-AV1 官方参数（`--pred-struct`、`--avif`、`--tune`、`--preset`、`--obu`）：https://gitlab.com/AOMediaCodec/SVT-AV1/-/blob/master/Docs/Parameters.md

**结论**：帧内编码有明显开发空间，先做它是合理路线；但公开 AV2 的 RA 视频 BD-rate 提升不能直接证明 AV2 still-image 一定能省 15%。15% 必须成为冻结后的项目验收门槛，而不是开工前的承诺。

## 2. 顺序覆盖旧 M6 路线

此顺序**覆盖** `03-M6-single-thread-compression-roadmap.md` 里旧的“C1 率事务→C2 亚像素→C3 帧内”执行顺序；后续章节里已写好的亚像素、多参考、B 帧设计仍保留为参考，但在 I-GATE PASS 前不排入 active work。

```text
I0  冻结 still-picture header、基线、解码 oracle、数据集与计时方案
 ↓
I1  完整可用的基础帧内模式 + UV 预测 + 正确 mode syntax
 ↓
I2  合法块/变换分区、TX 选择与量化/coeff syntax
 ↓
I3  单帧专用 CDF/rate 计量、事务安全的 intra-RD/RDOQ
 ↓
I4  AV2 帧内专有工具：DIP / MRLS / IBP / CfL / MHCCP
 ↓
I5  FSC / IDTX-intra / intra IST / CCTX / TCQ / parity hiding
 ↓
I6  帧内相关 loop filters 与单因素收益评估
 ↓
I-Q  单帧压缩竞争力验收（所有测试固定单线程）
 ↓ PASS
M6-INTER  亚像素、帧间预测、多参考、B 帧/GOP、时域 RD
 ↓ PASS
MT → PRESET → CRF
```

每一阶段只实现一个明确的技术单元或一组不可拆分的语法闭环；每个工具先由 fixed AVM 验收规范正确，再测单图真实质量/字节/耗时。某工具若收益低或速度代价不划算，保留 off 并转向下一个，不要求所有 AV2 工具都启用后才过 I-GATE。**I-GATE 一旦在盲测集通过，立刻冻结单线程 still-image 基线并开始帧间；不得为了实现完整工具清单无限拖延。**

## 3. I0：still-image 的对照必须完全公平

### 3.1 输入与容器

- **主基准：libaom `aomenc` 原生 AV1 all-intra/still-picture。** 每个样本输入必须是单帧 Y4M/YUV，固定使用 `--ai --threads=1 --cpu-used=0 --end-usage=q --cq-level=<Q> --limit=1 --passes=1 --tile-columns=0 --tile-rows=0 --obu -o output.obu input.y4m`。`--cpu-used=0` 是本轮高压缩主档；不能看完结果再改用更快、压缩更差的档位替代主结果。先用冻结版本的 `aomenc --help` 核实参数；若没有 `--ai` 或 raw `--obu` 输出，先固定/构建合适的 libaom 版本，不得回退到 `avifenc`。
- libaom、SVT-AV1、SVT-AV2 均读取同一份冻结 Y4M/YUV 像素输入，保留 bit depth、chroma sampling、range、色彩矩阵/transfer、尺寸和样本 hash。禁止一个工具从 RGB/PNG 走不同转换而另一个直接读取 Y4M。
- **辅助基准：SVT-AV1 只测一组 still-image 质量曲线**，不做多 preset 网格，也不做 5 次重复计时。固定偏压缩效率的 `--preset 4`，每个样本一张图、每个 QP 点一轮，命令模板为 `SvtAv1EncApp -i input.y4m -b output.obu --obu --pred-struct 0 --preset 4 --tune 3 --avif 1 --lp 1 --rc 0 --aq-mode 0 --qp <Q> -n 1`。这里的 `--avif 1` 是 SVT-AV1 内部的 still-picture coding optimization 开关，不是 AVIF 容器，也不调用 `avifenc`；`--tune 3` 是 still-image IQ 模式，`--pred-struct 0` 为 all-intra。固定版本 `--help` 必须确认这些参数可用；若 `--avif` 不可用，应隔离构建支持该优化的固定版本。`--lp 1` 是最低并行等级而不是严格的线程数，因此这条曲线只作为压缩率辅助参照，不用于 I-GATE 的单线程速度门槛；记录实际版本与线程配置。
- AV2 使用现有 single-picture-header still-picture 输出路径，不要用“多帧 sequence header + 一张 key frame”冒充 single-picture-header。固定 AV2 decoder 和解码选项。
- **主压缩指标是原生 codec raw OBU 总字节数**：libaom 与 AV2 均计入各自码流中的 sequence header、frame header、tile/coeff 等合法 OBU，不计 IVF/AVIF 容器、文件系统开销或额外 metadata。SVT-AV1 如可输出 raw OBU，也按相同口径统计。不得为了让某个编码器占优而选择性剥离其码流语法。
- 8-bit 4:2:0 与 10-bit 4:2:0 是共享输入格式的主基准；10-bit 4:4:4 另行测试，只有 libaom 基线也能以相同 4:4:4 样本无转换编码时才可进入直接对比。

### 3.2 版本和线程

本次检查的 `<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2` 子树中没有找到 `aomenc.exe`。I0 必须先搜索其他本地依赖/工具目录；找不到时，在隔离工具目录获取或构建固定 tag/commit 的 libaom `aomenc`，记录 build flags、编译器、二进制 SHA256 与 `--help`。同时固定支持 still-picture 选项的 SVT-AV1 版本。不得使用 `avifenc` 替代主基准，也不得因为缺少本地二进制就只跟 AVM 编码器自比。

- 主 AOM 命令：`aomenc --ai --threads=1 --cpu-used=0 --end-usage=q --cq-level=<Q> --limit=1 --passes=1 --tile-columns=0 --tile-rows=0 --obu -o output.obu input.y4m`。每个样本每个 QP/Q 点均保存实际命令、完整 stdout/stderr、输出 raw OBU 和解码质量。
- 主速度门槛只比较 `aomenc --ai --threads=1 --cpu-used=0` 与 SVT-AV2 单线程编码；二者在同一硬件/电源计划下至少 5 次新进程计时，暖机单独记录，统计 median/min/max 与 CPU time。SVT-AV1 辅助曲线每个 QP 点只运行一轮，不额外做重复测速。
- 至少 6 个独立质量点：libaom 使用 `--cq-level` 扫点，AV2 使用冻结的 CQP QP 点，SVT-AV1 使用固定 preset 下的 `--qp` 曲线。**各编码器的 Q/QP 数值不要求相同**，必须按解码后真实画质匹配，不能拿相同数字直接比较。

### 3.3 数据集

至少 50 张图片或固定视频帧，分为开发/标定集和盲测 holdout（建议 30/20，至少 20 张 holdout）：

- 自然摄影：人物/皮肤、植被、纹理、天空/渐变、建筑/细线；
- 高频和噪声：夜景、高 ISO、细颗粒、锐化边缘；
- 动画与图形：平坦区域、斜边、线稿、文字、UI、截图；
- 颜色挑战：高饱和纯色、色度边界、渐变、HDR/10-bit（源素材允许时）。

当前 2D/3D/Real 素材中固定帧可纳入，但不能只用它们三个主题；也不要把正在用于调模式的图片同时称为 holdout。保存原始输入、逐帧或逐图的 Y4M/YUV hash、尺寸和样本类别。小图片只用于快速机制测试，不能代表 1080p 单帧性能。

### 3.4 质量分数与匹配规则

- 先冻结本地目前用于比较的主质量分数与容差。为了和旧项目比较一致，可保留 VMAF 为连续性指标；另必须保存 PSNR-Y/U/V，若可稳定部署则加 SSIMULACRA2 或 Butteraugli 作为 still-image 感知指标。指标不可在看完结果之后按图片切换。
- 对每张图每个编码器运行质量曲线，在相同主指标值内匹配 AV1 与 AV2 的 output bytes；匹配容差事先固定。还要保留所有原始 Q 点和直接支配点，不能删坏点或只报告某个最有利 Q。
- 汇总报告每图 byte ratio、每类别中位数、总几何均值、BD-rate（每条曲线至少 4 个共同质量点）和 holdout 方向一致性。若曲线没有合法共同质量区间，记 N/A 并说明原因，不强行插值。

## 4. I1：先补足基础预测，别再停留在 DC/V/H

### 4.1 首批要形成的真正编码闭环

当下模式集合相对 AV2 v1.0.0 非常窄。优先实现可快速验证的基础 intra 预测，且**预测原语、mode signaling、CDF 上下文、R/D/J 选择、reconstruction 必须同步闭环**：

1. DC、Vertical、Horizontal 之外的 PAETH、SMOOTH、SMOOTH_V、SMOOTH_H；
2. 规范定义的 directional intra modes、angle delta、wide-angle mapping、reference sample filtering / edge availability；
3. 与当前 plane sampling 对应的 UV mode 选择，随后再加 CfL；
4. 合法 intra transform/block 形状对应的预测颗粒度；transform block 在一个 coding block 内按 raster 依赖时，后块必须看前块规范重建样本。

不能把 enum 写进去就算实现。必须完成固定 AVM 单块逐像素 differential test、合法语法写入和解码、全帧严格重建一致。模式 winner 不可只由 source 对 prediction 的 Y-SAD 决定后就沿用旧 coefficient/语法输出；最后必须进入同一候选的完整编码路径。

### 4.2 单帧测试

flat/color field、正/负斜边、弧线、checkerboard、渐变、文本/细线、自然纹理、噪声；每种覆盖 block size、左/上引用可用/不可用、角落、subsampling 与 bit-depth。输出按模式分类的命中数、残差 SSE、EOB/level histogram、真实 payload bytes、CPU 时间。单纯模式使用比例不能证明压缩变好。

主要文件：
`Source/Lib/Prediction/intra_prediction.c/.h`、
`Source/Lib/Encoder/intra_frame.c`、
`Source/Lib/Encoder/rd_frame.c`、
`Source/Lib/EntropyCoding/block_writer.c`、
`Source/Lib/Bitstream/tile_writer.c`。

## 5. I2：单帧的合法块划分、变换分区与系数链

当前生产路线是 DCT_DCT + 现有量化路径，并未形成完整 AV2 transform selection。帧内工作应先扩大**可合法选择**的块/变换形状；不要擅自扩展规范里 TX64 的 packed coded grid。

依序完成：

1. fixed AVM v1.0.0 的 intra 允许的 partition/split、最小块、边缘和 partial 块枚举；每种新几何先固定强制编码并验证，而不是立即搜索组合。
2. legal TX sizes/partitions 和模式约束表；实现已定义并适用于 intra 的 DCT/DST/ADST 等 TX paths（以 v1.0.0 的允许组合为准），TX scan/EOB/packed coeff/quant/inverse/reconstruction 一致。
3. FSC + intra identity transform：从 sequence flag、块级 `fsc_mode`、special coefficient coding、decoder reconstruction 到 RD rate 完整实现。必须覆盖 `FSC_MAX` 块大小与模式的规范条件。
4. intra IST：secondary transform 只在规范条件满足时进入候选（如合法 primary tx、EOB、block size、lossless 状态、YMode 限制）；语法 bit 与 forward/inverse、系数成本一起测。
5. chroma TX 与 CCTX：正确处理 U/V 跨分量依赖、downsampling、420/444 的条件、逆变换次序与独立 oracle；不能简单把 enum 接入 luma path。
6. TCQ（trellis-coded quantization）与 parity hiding：它们改变系数量化/概率/可隐藏 parity 的合法条件，必须在单帧 lossless/non-lossless、符号奇偶和上下文模型中逐步验证。不能声称它们是一个同名“RDOQ flag”。

主要文件：
`Source/Lib/Transform/transform.c`、
`Source/Lib/Encoder/transform_path.c/.h`、
`Source/Lib/Quantization/quantization.c`、
`Source/Lib/Reconstruction/reconstruction.c`、
`Source/Lib/EntropyCoding/coeff_syntax.c`、
`Source/Lib/Bitstream/sequence_header.c`、
`Source/Lib/Bitstream/frame_header.c`、
`Source/Lib/Bitstream/tile_writer.c`。

## 6. I3：先单帧把 rate 与 RD 决策做对

R7 自适应 CDF 有“相同重建、真实 bytes 变小”的证据，但它还不等于自适应 CDF 与 RD trial 组合已完整可用。针对单帧建立独立最小闭环：

- 在单 picture header 语义下读取 v1.0.0 的默认/average CDF 和允许的更新过程，不把 multi-frame 的 entropy inheritance 直接搬进来。规范对 `single_picture_header_flag=1` 规定 `enable_avg_cdf/avg_cdf_type` 的推导规则；按固定 AVM 的实际解码上下文实现。
- 对每个 intra candidate，从相同前缀状态复制 committed/scratch probability bank；候选符号顺序真实写/计费；候选失败/退出丢弃 scratch，winner 重放成功后才 commit。必须复位 partition/mode/neighbour/recon/writer cursor。浅拷贝 TileTree 不保证没有共享可变 cells。
- 率模型需要对所有 reachable mode/partition/coeff contexts 闭合。adaptive CDF 的 raw 相邻边界可能相等时，按固定 AVM `get_adjusted_prob` / position padding 计价，不能拿 raw CDF 差值、epsilon 或非零 level 数冒充真实 R。
- 在这条单帧路径实现 exhaustive 小块参照和有界 production search。对候选记录真实 pixels D、同语法 R、λ、J、候选数、被剪掉的最佳 J、最终码流实际 bytes。保留 CQP；不按 holdout 调 λ。
- 对 coefficient level 的局部 RDOQ/trellis 必须考虑 EOB、level/sign、context、TCQ 状态和 quant→inverse→clip 后真实 D。变换域近似只用预筛，不能当完全等价。
- 先让 mode/partition/TX 的少量候选正确联动；每种高级 tool 再通过开关作独立效果 ablation。不要一开始把所有 intra flags 全部置 1，造成不合法组合或无法归因的回归。

主要文件：
`Source/Lib/EntropyCoding/entropy_models.c/.h`、
`entropy_encoder.c/.h`、`coeff_syntax.c`、
`Source/Lib/Bitstream/tile_writer.c`、
`Source/Lib/Encoder/rd_frame.c`、
`Source/Lib/Codec/encoder.c` 与公共 API/CLI（由 00 唯一编写）。

## 7. I4：逐工具接入 AV2 帧内专有工具

这些工具需由 00 先读固定规范与 AVM v1.0.0 源码，明确有效模式和状态；每个工具独立开关/实验目录，一次只打开一个家族。

| 技术 | 适用价值假设 | 必须闭合的实现链 |
|---|---|---|
| DIP（data-driven intra prediction） | 用已解码邻居特征产生数据驱动预测，可能优于少数 hand-coded 模式 | `use_dip` 条件、DIP features/transform/resample、weights 固定来源、整数舍入与预测 syntax |
| MRLS（multiple reference line selection） | 对方向纹理使用相邻参考线以外的线，提高局部预测 | `mrl_index`/secondary index syntax、可用邻居距离、reference availability、mode/CDF |
| intra edge filter | 平滑或处理预测引用边界，减少边界误差 | sequence flag、edge kernel/taps 与 block size conditions、固定 AVM sample match |
| CfL | 从已重建 luma 预测色度，利用 Y/UV 相关性 | `UV_CFL_PRED`、合法 block/tree/subsampling、downsample filter index、alpha/sign、UV 重建 |
| MHCCP | 多假设跨分量色度预测 | 可用的 cfl index/multi parameters、仅合法 chroma blocks、decoder 逐样本和完整语法模型 |
| IBP | 混合两种 intra prediction hypotheses，而非单一模式 | enable bit、block size 与 mode restrictions、规范权重、所有适用 plane |
| DPCM / palette / intra block copy | 特定方向残差、纯色或 screen content 中高收益 | 各自适用条件、scan/level 或 block copy reference、屏幕内容 flags；不将屏幕收益外推到自然照片 |

不要只以 1080p 照片对 DIP、palette/IBC 下结论；屏幕文字/动画单独分类。对自然图片不划算的工具可以默认 off，但技术必须在目标格式的固定 AVM 码流/逐样本约束下才算“实现”。

## 8. I5：loop filters 在 still picture 上的收益

deblocking、CDEF、GDF、restoration、CCSO 等过滤可能改善还原后的观感或允许在同等 score 下节省 residual bits。它们应在基本 intra prediction/transform 路径稳定后单独接入，不需要等待帧间编码。

每次只加一个工具：固定 v1.0.0 sequence/frame flags、滤波参数 syntax、实际 filter kernels、舍入/clipping 和最终 still-image reconstruction；以 fixed AVM 对比正确的滤波后可显示图片。检查 `--export-recon` 是滤波前还是滤波后，否则不能对着不同 stage 的 raw 误报 mismatch。随后比较开启前后的多质量点和编码耗时，不能把更好画面免费换来更多码率。

## 9. I-GATE：用户指定的单线程竞争力门槛

**没达标，不开始帧间编码功能开发。** 建议先做 `I0–I5`，但无需机械实现每个工具；如果已实现的组合提前达到所有条件，则立即冻结并通过本门槛。

### 9.1 主放行条件

对至少 20 张 holdout（不用于调参），在相同质量分数下：

1. **体积**：AV2 raw OBU 总字节数 / libaom `aomenc --ai --cpu-used=0` raw OBU 总字节数，在同质量匹配后的 holdout 几何均值 ≤ **0.85**，也就是平均至少省 **15%**；同时报告每图 ratio、每题材中位数与 BD-rate。SVT-AV1 的固定 preset 曲线另报 AV2/SVT-AV1 的 ratio 与 BD-rate，但它是辅助参照，不替代 libaom 主门槛。
2. **速度**：AV2 单线程相同质量点的中位编码吞吐 ≥ libaom `aomenc --ai --threads=1 --cpu-used=0` 的 **50%**，等价于 `T_AV2/T_AOM ≤ 2.0`；编码时间不包括解码、VMAF/SSIM 计算或额外封装。AOM 与 AV2 各质量点至少 5 次新进程计时；SVT-AV1 只跑一轮曲线，不承担主速度门槛。
3. **画质与格式**：主评分、PSNR-Y/U/V 与解码重建均正确；不得通过把 10-bit 444 改成 420、改变矩阵/range、降分辨率或额外 blur 省字节。主分数容差要在实验前冻结。
4. **稳健性**：全部 20 张 holdout 按原始样本完整展示；报告每类中位数和输赢个数。推荐另设稳定性条件：至少 80% holdout 图片没有大于 5% 的体积回归，且重要题材不能整体严重倒退。
5. **正确性**：AV2 v1.0.0 fixed AVM 独立 decoder 对整个 still picture 解码并逐可见 sample 匹配 encoder normative reconstruction；MSVC/GCC/Clang、strict CTest 和必要 ASan/UBSan/LSan 完成；1/2/3/17/1000 次独立单图/API 重试无状态污染。
6. **可重现**：冻结 SVT-AV2 source/exe SHA、固定 AVM spec/decoder SHA、libaom `aomenc` 与辅助 SVT-AV1 的版本/commit/构建参数/二进制 SHA、Y4M hashes、所有命令行、原始时间与质量分数、raw OBU 和 BD-rate 脚本。否则视为测量无效。

“同质量”必须通过 AOM 与 AV2 各自多点曲线匹配实现，不是给相同的 Q / QP 数字。若 15% 目标未达成，按单帧残差、coeff syntax、partition、filter、每阶段时间定位下一项，继续在 intra-only 范围内迭代；**不能把部分功能通过或单个图片获胜当作 I-GATE PASS**。

### 9.2 I-GATE 通过后的后续顺序

1. 冻结通过门槛的 single-picture 稳定基线、程序和数据。
2. 才进入已有的 M6 帧间工作：motion/subpel、reference prediction、多参考、inter 工具、GOP/B 帧、scene-cut/lookahead/temporal RD。
3. 再按总路线进入多线程、preset 和 CRF；单帧基线的质量/码率回归作为所有阶段的固定防回退项。

## 10. 本地 AI 的第一批可执行任务

| Task | Owner | Deliverable | Acceptance |
|---|---|---|---|
| I0-00 | 00 | 冻结当前程序/代码 SHA、单 picture header 路径、旧测试状态、libaom `aomenc --ai` 主基线、SVT-AV1 辅助曲线和文件 owner | 找到或隔离构建固定 libaom；确认 `--ai`/`--obu`/单线程参数；不使用 `avifenc` 替代 |
| I0-B | B | ≥50 张样本，30 calibration/≥20 holdout，Y4M/格式/色彩/帧 hash、libaom `--cq-level` sweep、SVT-AV1 单 preset 曲线、raw OBU 统计和计时脚本 | AOM/AV2 主计时 `threads=1`；SVT 仅一组、不重复计时；同像素与质量指标冻结；raw JSON 可复算 |
| I1-A | A | AV2 normative intra mode primitive 计划与基础 directional/smooth/paeth 预测模块 | 每个原语与 fixed AVM 逐样本一致；新开关默认 off |
| I1-B | B | mode syntax / CDF / decoder trace 与独立原语测试 | 证明实际码流 mode 生效，非单元测试中“函数能运行” |
| I1-00 | 00 | API/CLI/CMake 与开关、集成验收 | 默认 off 指纹保持，非法组合 fail closed |
| I2-A/B | A/B | TX/partition/coeff 候选的固定开关闭环 | 解码重建一致；至少一组单图质量/字节/耗时数据 |
| I3-A/B | A/B | intra-only rate/CDF transaction + intra RD / RDOQ | 反序/失败/重放时完整状态一致；真实候选用真实语法成本 |
| I4/I5 | A/B | 每个 AV2 新 intra/filter tool 独立 ablation | 一个工具一套源/程序/result hash 与收益/成本结论 |
| I-Q-00 | 00 | I-GATE 独立终验 | 体积 ≤0.85、speed ≥0.5、holdout/decoder/platform 都 PASS 后冻结仍用单线程 |

## 11. 明确禁止事项

- I-GATE 前，不把亚像素运动、多参考/B 帧/GOP、INTER DDT 或多线程列入 active implementation。
- 不一次把 sequence flags 全部改为 1；每个工具只在自己规范合法条件下可达。
- 不把 AV2 inter DDT 错称为 still-picture 帧内收益；本规范的 `enable_inter_ddt` 属非 single-picture-header 的 inter 相关语法。
- 不按相同 Q 与 QP 数字比较 AV1/AV2；不按一张图片、不按最弱的 AOM 速度点声称平均改善 15%。
- 不把 file container overhead 当纯 codec efficiency；不使用不同像素源/色彩转换的 bytes 作公平 payload 结论。
- 不因为当前 `aomenc` binary 不在已查目录就跳过基线或改用 `avifenc`；先定位 libaom 版本或在隔离目录构建。
- 不覆盖既有 M5、R5、R6、R7 frozen builds、原始质量/速度测试、固定 AVM oracle 与 old reports。
- 不因一个工具 syntax/CTest PASS 就写“有竞争力完成”；I-GATE 数值结果必须基于真实 holdout 实测。

## 12. 实施依据

- [AV2 v1.0.0 规范](https://av2.aomedia.org/v1.0.0/index.html)：§5/§6 sequence flags、§7.13.2 intra prediction、§7.13.4–§7.13.6 palette/CfL/MHCCP、§7.14 reconstruction。
- [固定 AVM v1.0.0](https://github.com/AOMediaCodec/avm/tree/v1.0.0)：独立规范/解码/预测原语 oracle。
- [libaom `aomenc` 官方源码](https://aomedia.googlesource.com/aom/+/refs/heads/main/apps/aomenc.c)：`--ai` all-intra、raw OBU、线程和 pass 处理。
- [SVT-AV1 官方参数](https://gitlab.com/AOMediaCodec/SVT-AV1/-/blob/master/Docs/Parameters.md)：still-image `--avif`、`--tune 3`、all-intra `--pred-struct 0`、preset 与 raw OBU。
- 本地：`..\svt-av2\docs\task-reports\M5.1-integration.md`、`..\svt-av2\docs\task-reports\M6-basic-capability-assessment.md`、`..\svt-av2\docs\task-reports\basic-capability-progress.md`、`..\svt-av2\docs\task-reports\next-A-adaptive-rd-design.md`、`..\svt-av2\docs\basic-entropy-contract.md`。

**本轮的交付是路线冻结，不是编码实验通过。** I-GATE 之前保持线程数 1，只围绕单图压缩率/质量/速度开发；达标后再进入帧间。

</details>


<details>
<summary>014 · 05-M6-intra-next-round-guide.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\05-M6-intra-next-round-guide.md`  
原文件 SHA-256：`77f1a7b53e90d7b7937d9c78a70223fd6b8fcc646c7014c372c5e498d9c5591a`

# AV2 单帧下一轮优化指南：速度与 VMAF v1 高画质区间

日期：2026-10-25。状态：**用户已检查并授权执行**。本指南形成时只做只读分析；用户随后明确“直接按照这个开始下一步”，现已恢复评测与独立候选验证，进度见 [第二轮执行记录](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/docs/task-reports/M6-intra-v1-r2-20261025.md)。原暂停产物保留，未过门槛的候选不合 MAIN。

## 1. 结论与下一轮顺序

仍有明确优化空间，建议按以下顺序推进：

1. **先修正评测依据**：以新版 VMAF v1 为主，v0.6.1 NEG 为副，利用已保存的重建像素重评分，重新确定 93–95 区间和 AOM 同画质时间。普通 v0.6.1 只保留历史结果。
2. **先兑现已准备的精确提速候选**：省纯预测中间写入、2D DCT odd-dot 内核；完成性能筛选再决定接合，不重新开一轮无边界的内核开发。
3. **高画质压缩优先检查 RDOQ 和筛选遗漏**：fast 大矩形 RDOQ 尚未覆盖；量化 proposal 使用默认 CDF/零邻居；树在 RDOQ 前决定胜负。先测这些缺口的实际影响。
4. **按新模型的误差来源补工具**：优先考虑色度变换、预测/TX 联合决策、分区表达能力，再由色带/块边界证据决定滤波。DIP、IST、TCQ 等不能仅因“尚未实现”就全部启动。

目标继续为 AOM 同画质体积 **0.85×**、单线程编码用时 **≤2×**，重点独立验收 **VMAF v1 的 93–95**，同时保留宽质量区间和跨内容检查。不能以宽域均值通过掩盖高画质局部退化，也不能为某一张开发帧写专用规则。

## 2. 先切换正确的模型，历史数字不能直接沿用

用户已明确：使用视觉优化后的 **v1 模型族**，其次 `vmaf_v0.6.1neg`，不建议继续用普通 `vmaf_v0.6.1` 作主要依据。此要求覆盖旧评测脚本的默认模型选择；冻结的旧结果仍保留。

### 2.1 本轮已核实的模型与工具

| 用途 | 下一轮固定选择 | 当前本地位置 |
|---|---|---|
| 1080p 主指标 | `vmaf_v1.0.16_3d0h.json` | `<LOCAL_PRIVATE>/ffmpeg-native/deps-full/vmaf/model/vmaf_v1.0.16/vmaf_v1.0.16_3d0h.json` |
| 副指标 | `vmaf_v0.6.1neg.json` | `<LOCAL_PRIVATE>/ffmpeg-native/deps-full/vmaf/model/vmaf_v0.6.1neg.json` |
| 独立评分程序候选 | `vmaf.exe`，版本输出 `3.2.0` | `<LOCAL_PRIVATE>/ffmpeg-native/deps-full/bin/bin/vmaf.exe` |

三者 SHA256 分别为：

```text
v1 1080p3H cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f
v0.6.1neg d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f
vmaf.exe   1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5
```

指南形成时只执行了版本查询；用户恢复后，已完成单帧双模型评分、重复执行与独立审计，结果见顶部执行记录。本地 VMAF 源码 HEAD 是 `f85a853692a8c730d0270cd733c8bb30b5b93b7c`，不能据此推定已安装二进制必然由该提交构建。

官方将 1080p@3H 指定为标准 1080p 模型，并建议 SDR 评分保留 10-bit 精度。当前原生输入本来就是 10-bit，应直接保留精度。phone、4K、HFR 是不同观看条件，不能在看到得分后选择更有利的模型。[官方 v1 模型说明](https://github.com/Netflix/vmaf/blob/v3.2.1/resource/doc/models_v1.md)

v1 默认已启用 NEG，并增加对色带、色度和压缩伪影的处理；其标准模型包含 CAMBI、chroma、ADM3、motion3 特征。因此旧模型下色度 PSNR 退化的候选需要重新排序；“减少色度位数换亮度分数”未必仍成立。这是需要重评的推断，不是已测结论。[Netflix v1 介绍](https://netflixtechblog.com/vmaf-v1-good-is-not-good-enough-60d7e4244ea8)、[固定模型文件](https://github.com/Netflix/vmaf/blob/v3.2.1/model/vmaf_v1.0.16/vmaf_v1.0.16_3d0h.json)

### 2.2 下一轮第一步：只重评分已有码流

先做一次最小评分器验证：同源比较、已知 AOM/AV2 重建各一个；确认模型加载成功、四类 v1 特征齐全、flush 后恰有一个有限的逐帧结果、重复执行可复现。单帧的时域边界行为必须记录；不靠重复帧、补黑帧或拼接其它内容人为改变 motion 特征。不要假定自比较必定精确等于 100。

随后复用已保存的重建，不重新编码，至少重新评分：

- 冻结 r4c、当前 r28/global2、r26/wide 的 2D 六点。
- 冻结 AOM cpu0 与官方 AVM cpu9 的 2D 六点。
- 官方 AVM/AOM 另外两张开发帧，以及已保存的代表性滤波消融，用于确认高画质区间排序是否改变。

每个点分别输出 v1、NEG、PSNR-Y/U/V、v1 特征与模型/评分器 SHA。v1 与 NEG 各自建立曲线，不平均两个分数、不混在同一个 BD 积分里。旧码流体积、旧编码用时和像素身份不变；重评分的 CPU 不计入编码耗时。

**旧普通 0.6.1 的 93 分不等于同一编码点在 v1 上也得 93 分。** 现有“1.09×体积、约2.7×时间”不能改标签后当成 v1 的实测基线；新模型下真实差距尚未知。

## 3. 93–95 为什么值得单列

本次对冻结数据重新计算了高区间曲线，下面全部是**旧普通 v0.6.1**，仅用于提出问题，不能作为新版验收数据。数值是 PCHIP 对数体积插值，不是恰好该分数的直接编码点。

| 旧模型分数 | 当前 r28 / AOM 体积 | r26 wide / AOM 体积 | 官方 AVM9 / AOM 体积 |
|---|---:|---:|---:|
| 93.0 | 1.1408 | 1.1367 | 0.8361 |
| 93.5 | 1.1484 | 1.1350 | 0.8433 |
| 94.0 | 1.1632 | 1.1387 | 0.8510 |
| 94.5 | 1.1778 | 1.1421 | 0.8524 |
| 95.0 | 1.2068 | 1.1578 | 0.9000 |
| 93–95 局部 BD | +16.4653% | +14.0102% | −14.8127% |

三个具体信号：

1. 当前宽域平均 +9.06%，但高区间是 +16.47%；global2 在高区间比 wide 有更明显的压缩代价。恢复 wide 的诊断价值高，但它本身不足以弥合全部差距。
2. 即使旧模型，官方 AVM9 在 2D 的 95 分附近也只省约 10%，没有满足 15%目标；另两张开发帧有官方 AVM 局部输给 AOM 的现象（3D 的95约+8.22%，Real的93约+1.65%）。这是固定 cpu9 实现的证据，不是 AV2 标准的压缩上限。
3. **采样过稀**：r28 在这个区间只有 Q90/V94.852980 一个直接点；官方 AVM 只有 Q96/V94.492548，AOM只有 Q24/V93.680344。官方 AVM 在95处用 PCHIP 得到0.9000，用分段线性对数插值得到0.9802，差异很大。4097个插值网格不能替代更多真实编码点。

旧口径下，1.0906→0.85 仍需相对当前减少约22.1%体积；95附近的1.2068→0.85约需减少29.6%。约2.7×→2×也需约四分之一降耗。进展很大，但剩余不是调一个小参数就必然完成；新版模型重评后应重新计算这些差距。

离线计算、输入哈希和所有精确值见 [既有曲线分析](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/next-round-analysis-20261025/existing-curve-analysis.json)。没有新增编码或评分。

## 4. 速度路线：先验证已封存候选

| 优先级 | 候选 | 已知状态 | 下一步与停止条件 |
|---|---|---|---|
| S1 | 省纯预测中间 frame 写入 | 3文件20增4删，有限错误/重试/旧路/SAN/官方解码通过，性能未测 | 两128裁块固定CPU交错group10×3；保持流/重建/统计一致。稳定收益才在当前global2配置测原生高质量点 |
| S2 | 合法2D范围内的DCT递归odd-dot AVX2 | 两工具链各10844数值例、三臂编码正确；仅host原型 | 必须同时比较generic基准A、同arch基准B、候选C；两块各组三次C/A与C/B均至少约3%降耗，才投入portable dispatch |
| S3 | 当前叶U之后的严格J下界 | observer仅准备，未编译/编码，无命中率 | 先只计实际可省V的fresh FWD；不计memo已省/首项无界情况。双块潜力不足约3%frame即停止，不直接写早停 |
| S4 | 预测与重建内核 | 原生Q110采样仍是明显热点 | 前三项后仅按新的实际热点选一个内核；先看机器码与数值界，避免再做已被编译器向量化的工作 |

S1/S2 correctness 基底是 r26/wide，当前推荐是同源码的 r28/global2。复用已有正确性证据，同时补最小的当前参数流/统计桥接；不能用旧参数测速代替当前结果，也不必因此重跑全部矩阵。

采样中 fwd1d 自身约15.3%，预测相关三个独立 self 类别约8.0%/7.2%/5.5%，reconstruct自身约8.7%。这是旧r26、单Q110的flat采样，包含内联归属限制，末尾采样收尾失败保留。fwd内积已有编译器向量化；即使整个15.3%热点快一倍，整帧理论降耗也仅约7.7%。不要承诺一个SIMD内核直接解决全部25%速度差。

如果 S1/S2 无稳定收益就封负，不为了“代码已经写好”继续扩平台或原生矩阵。通过者合并后再测整体，不累乘各项百分比。S3只在当前真实因果前缀内证明下界，不能复活已被共享CDF否定的跨树Y-only等价重排。

## 5. 压缩路线：优先修搜索和系数链的具体缺口

### Q1：fast 大矩形 RDOQ 与 proposal 的质量

这是本次源码只读分析中最直接的新入口：

- [`refine_rdoq_fast`](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/Source/Lib/Encoder/rd_frame.c:1255) 明确跳过 `w!=h && (w>16 || h>16)`，所以当前fast并未覆盖所有大矩形RDOQ。这个条件按亮度叶几何，在三平面循环内判断，UV也会随该叶跳过，应分平面统计。旧普通路径或工具文档写“长条RDOQ已接”不能代表fast也接了。
- [`rdoq_propose`](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/Source/Lib/Encoder/rd_frame.c:452) 的局部率用默认CDF、零邻居；最终才做真实整SB/CDF验收。fast单proposal使用phase0，即全局lambda的约1/16；它不是完整trellis或TCQ。
- [`refine_tree_finalists`](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/Source/Lib/Encoder/rd_frame.c:1941) 在多个finalist时先不做RDOQ、选树后只处理胜树，可能发生优化前后排序不一致。

建议分开三个实验，不做一个大补丁：

1. **矩形覆盖**：先计高区间胜树中被跳过块的面积、非零系数、系数率与像素SSE；核实各形状变换域能量归一化、tx_scale及packed32维规则，再只补winner大矩形RDOQ。不能直接删guard就声称正确。
2. **proposal强度/上下文**：保留scalar入口和完整SB实际验收，先尝试一个额外既有lambda档，或仅为当前块取得真实入口上下文；两种变化分别测。避免先引入完整CDF事务框架。J下降不等于v1必然提高，必须看新曲线。
3. **排序诊断**：离线只对原两个finalist都做RDOQ，记录是否换赢家及最终D/R/J差；这是一条压缩上界诊断，不默认恢复所有树RDOQ。若改善集中在少量、可由低成本特征识别的块，再设计有限重评。

验证：合法形状强制流、EOB≤1/全零、实际clip后重建、候选反序、真实错误后恢复、SAN；通过后才扩大原生93–95点。重用现有正确性资产，不先跑所有格式×所有QP×所有组合。

### Q2：查明预筛在高画质漏掉什么

保留已有global2/Y2作快速基准，准备**仅用于诊断**的较宽搜索：wide、Y4、必要时少量选定SB的更宽合法模式。先统计被粗排淘汰但在完整工具链胜出的候选，以及其mode/TX/分区/纹理特征。

优先检查：基本Y按DCT/SAD排序、角度SAD top4、先角度后MRL后TX、树在最终RDOQ前排序。这些近似与高画质最终率失真目标可能失配。只恢复“遗漏确实存在且收益足够”的小域；不要全局再缩top1，也不要根据用户图片名、固定坐标、输出VMAF分数或单张图专用QP阈值决定搜索。

若要内容自适应，可用正常编码可得的粗J差、残差/边缘强度、EOB或合法形状；规则在开发集上冻结，再到未参与调规则的开发图验证。不把完整VMAF评分塞进每个RD trial。

### Q3：UV 变换与 Y/MRL/TX 联合决策

v1纳入色度后，该方向优先级上升。当前UV仍固定DCT；固定官方实流Q128/Q96的有效UV inverse调用中，ADST类约8.63%/17.38%。这是使用率，不能当节省百分比，但说明真实工具缺口。[官方UV inverse核查](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-r3-20261024/official-uv-inverse-independent.json)

先实现规范实际要求的UV类型推导与预测/变换联动，不允许把UV当作能任意独立信令TX的平面；同步改系数合法域、扫描、EOB推断及逆变换，不能只关 `chroma_dct_only`。首版限制一个已确认合法尺寸域，fixed AVM逐像素验证后再扩。

Y侧当前非DCT仅覆盖小方形和8×16/16×8。先利用Q2证据，判断是已有合法类型没被选到，还是尺寸覆盖不足；先解决前者，避免同时扩所有TX。MRL、角度和TX共同改变残差，有限联合重评应保留进入tuple，并单独测新增候选收益/耗时。

### Q4：分区表达能力，按误差定位后补

当前longrect24包含有限混合与统一矩形提案，并非官方任意递归混合分区。官方2D存在大量8/16矩形叶，值得对齐比较**相同空间区域的几何、残差和符号率**，不只比较块大小直方图。

先尝试在已有合法plan域中有界细化胜树邻域；必要时再补一种明确缺失的混合划分。不要恢复枚举所有树笛卡尔积，也不要直接写按坐标缓存选模：共享CDF、因果重建邻居、参考可用域和预算都可能不同。

独立UV树、任意TX split潜力较大，但涉及语法、上下文、几何和回放，放在前面小改动之后；每轮只补一个闭环。

## 6. 滤波与其余AV2工具如何排序

| 方向 | 现有证据 | 下一轮建议 |
|---|---|---|
| deblock/CDEF/GDF/restoration/CCSO | 旧模型官方分组消融有明显交互；全部关闭在旧高区间反而更好 | **先用v1/NEG重评分原消融**，观察色带/块边界/色度特征，再选一个工具；不把旧组消融收益相加 |
| FSC/IDTX | 官方Q96有135次真实非零FSC系数块，所占面积仍较小 | 在v1高区间重新确认相关区域的bit/误差份额；有信号才补最小闭环 |
| IBP/edge filter | 旧全域消融边际小且方向不一致 | 暂低于Q1–Q3；新模型出现稳定收益再提升 |
| DIP/palette/IST/CCTX/TCQ/parity | 固定2D旧探针中部分未用或为零；其它内容可能不同 | 不按工具名列清单全开；先查新高区间和代表开发图实际使用/消融 |
| 内容自适应量化/失真权重 | 可能改善视觉分配，也可能牺牲色度/渐变 | 先看v1特征与误差图；不继续以普通0.6.1亮度分数导向随意削色度或锐化 |

旧模型“关闭滤波更高分”不证明实际视觉更好；v1/NEG的重评分可能改变排序。本机已有全部关闭、只关CCSO、保CCSO关其它滤波的完整官方曲线，先复用它们，不重复编码。若滤波排序仍不稳定，就把主力留给系数、预测与分区，不一次移植整套后处理。

## 7. 下一轮实际执行与验收

### 第0轮：冻结新指标基线

完成第2节重评分。记录r4c/r28/AOM/AVM在v1与NEG上的真实曲线、共同区间、93–95覆盖、色度和色带特征。优先原生1080p，不从128裁块推断v1高区间性能。

冻结的r4c编码结果继续作为质量约束参考，**对同一批r4c像素重评分后**，将最多1%的同画质体积回退约束应用于新主指标；不是更换参考编码器。当前r28另列作工程前后比较，防止多轮小回退累积。NEG分别报告，若与v1明显反向先检查，不用平均分掩盖。

### 第1轮：补密93–95，再做有限候选

主目标点固定为v1 **93、93.5、94、94.5、95**。每个编码器先用重评分曲线找QP包围，再补真实点；不能沿用旧0.6.1的QP→分数映射。每条曲线应有至少4个直接点处于该区间，并有两端包围点；若达不到，先补点，不外推。

QP补点仅用于基准曲线校准，候选沿用冻结点集；如候选未覆盖，则按事先固定的包围/二分规则补点。保留所有原始点，出现非单调先复核，不能挑点强行让PCHIP变好。PCHIP与线性对数插值同时报告；差异足以影响决策时继续补采样，不把4097网格当精度证明。

并行分工建议：主线程管指标、冻结与集成；一条S1/S2内核队列；一条Q1系数队列；一条Q2/Q3诊断队列。正确性/准备可并发，**所有正式性能对照顺序独占**。最多3个新候选在途，单因素通过后再组合。

### 第2轮：先小样本筛选，再原生曲线与速度

1. 先按变化做必要数值/强制语法/错误恢复检查。精确优化必须保持码流/重建和适用统计一致；物理调用计数变化单列。搜索或新增工具则独立验质量，不冒称exact。
2. 两128裁块仅用于筛性能与机制，group多新进程汇总，避免15.625ms CPU量化主导。低收益或质量明确失败就停止，不自动补完整107项和六Q。
3. 原生先用冻结的3个高区间点筛趋势；可能改善的再跑完整高区间密曲线和旧宽域覆盖。重修改内部结构/header时重编所有消费者，按实际影响范围做完整回归。
4. 压缩候选看v1高区间体积/NEG/PSNR及耗时Pareto；小改动的稳定小收益可保留，重功能无明确收益不扩。速度候选在当前推荐参数下核对原生输出并计时。

最终速度在新模型的**相同画质**上比较，重点93/94/95；不是同QP，不沿用Q110/V92.4的约2.7×诊断。AOM保持冻结cpu0/ALL-INTRA/threads1。两边同硬件/电源/亲和，每个实际计时点单独暖机，各至少5次新进程，记录CPU和墙钟median/min/max；评分不进入编码计时。

开发中先争取AOM体积1.0，再向0.85推进；只有v1高区间与宽域、色度检查及实际≤2×时间共同支持时才宣称目标通过。现有2D开发帧不能替代原合同holdout20/I-GATE；调参只用开发集，最终候选冻结后才动holdout。

### 控制每轮长度

建议每轮约2小时形成一次可检查包，事先列候选和最大新增点数。到检查点汇报已完成结果、正在跑的唯一批次和剩余预算；不自动串联新阶段造成又一次8小时长跑。失败原包保留，修复另编号；没有新证据不复活旧负方向。

## 8. 本轮明确不做与恢复入口

本指南最初在暂停期间形成；用户已检查并授权下一轮恢复，原“不启动”边界已被该明确授权解除。恢复后的候选与新评分见执行记录。不改生产默认档、不推进帧间/GOP/多线程、不用锐化或模型切换制造压缩收益。

恢复入口：

- [暂停报告：当前代码、体积、用时与历史优化](<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/docs/task-reports/M6-intra-speed-pause-20261025.md)
- [冻结暂停记录](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/root-pause-20261025.json)
- [S1重建候选交付](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/agent-tx/recon-direct-pred-r1/HANDOFF.txt)
- [S2 DCT候选正确性冻结](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/agent-coeff/fwd-butterfly-avx2-r1/correctness-phase-freeze.json)
- [S3 UV下界准备状态](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/agent-rd/uv-v-bound-count-r1-prepared-pause.json)
- [固定官方AVM源码与搜索组织核查](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/agent-tx/avm-partition-readonly-r1/REPORT.txt)
- [官方未接工具与滤波既有证据](<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-r3-20261024/official-extra-tools-independent.json)

当前可用MAIN程序仍为 `<LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r1-20261025/main-r26-verify-build/bin/SvtAv2EncApp.exe`，SHA256 `954d66fb507a3565de1a0d87dcfc4d9efe90a9cf26db09aee3aa3e73d18e5947`；参数fast/longrect/tree-refine rd，其余见暂停报告。版本仍0.0.5-m5。此处保留指南形成时的程序入口，不代表它已通过新v1质量门；恢复后的评分和候选结果见顶部执行记录。

</details>


<details>
<summary>015 · 07-M6-intra-r4-20261026-codex-brief.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\07-M6-intra-r4-20261026-codex-brief.md`  
原文件 SHA-256：`0127fdfd7ddc81dcfdc3f1c67ae9a122d7ad00e9479387423e0dcaf8a9d06a20`

# M6 单帧第4轮：AVM机制差距与 1.00×AOM 中间目标（2026-10-26）

## 立即执行的用户指令
从 NuxBox1 本地 Codex 继续 AV2 编码器 M6 单帧压缩优化；最终固定目标：同画质体积 ≤0.85× 冻结 AOM AV1 cpu0/ALL-INTRA/threads1，单线程编码用时 ≤2×；**本轮先尽力达到 ≤1.00×体积，之后数轮推进到0.85×；本轮不做速度测试**，因为 FFmpegFreeUI/ffmpeg 目前压制视频并争用CPU。切勿停止、降速、改动 FFmpeg/3FUI，不要再次点击“加入编码队列”。不修改默认稳定发行版或推进帧间/GOP/多线程，不触 holdout20/I-GATE。

## 不变的验收合同
阅读同目录 06-M6-frozen-requirements-20261025.md、05-M6-intra-next-round-guide.md 和 svt-av2/docs/task-reports/M6-intra-v1-r3-final-20261025.md、svt-av2/docs/task-reports/M6-intra-v1-r3-20261025.md、AGENTS.MD；原 SHA/冻结参考在 <LOCAL_PRIVATE>/ffmpeg-native-m6-tests/speed-r3-v1-20261025/requirements-freeze-r1.json。
- 主 VMAF: vmaf_v1.0.16_3d0h，副 vmaf_v0.6.1neg；普通0.6.1仅历史。1920x1080 10-bit 420 开发帧，93/93.5/94/94.5/95 同分位置与完整宽域，PCHIP 对数体积+线性插值必须并报，实测点包围，不能外推或抹去坏点；高带每曲线至少4个直接点。比较冻结AOM 3.15.1 cpu0 ALL-INTRA threads1，不能换 AV1 still-picture 为 avifenc。
- 保留原r4c质量回退限制：相同像素、同画质体积最多允许1%回退，不能用新candidate替代r4c。官方固定AVM cpu9 在新v1 93–95 中大约 0.86×AOM（旧不同模型 0.85–0.9），这是**可研究的实现结果不是AV2天然优势**。
- 前轮最新独立CCSO(Y4/wide r25 基底) 在主93–95的BD体积为 1.185136518×(PCHIP)/1.186641671×(线性)，95分处约1.261×，并未达目标。其 11 QP、参考、解码、截图证据在旧r3隔离目录；MAIN并未合入。避免把旧0.6.1下1.09×称为v1新基线。

## 你要做的工作（按实际证据逐项收敛）
1. **只读核查环境与真实 FFmpeg/CPU 负载**、旧报告/参考源/官方AVM二进制及可能的官方AVM源码定位。确认不同编码器配置/质量曲线确实可比，辨认官方AVM的具体版本与enable工具/搜索策略。尽量以实际源码、码流统计、可独立消融的原生实验支持结论，不将AVM标准全部工具罗列成有效收益、不把不同配置的性能归因混为一谈。
2. **回答为什么官方 AVM cpu9能做到约0.85–0.9×AOM**：区分真实压缩工具（partition、intra prediction/MRL、UV-derived TX、滤波/CCSO/restoration、entropy/RDOQ/CDF、transform、内容自适应等）的可证实贡献，和其更大搜索预算、质量模型的差异；给具体源码路径/函数/证据，指出哪些技术在本SVT-AV2编码器中已有简化实现、哪些缺失、哪个压缩损失最大、可信度分级。优先使用已存AVM解码/官方消融数据、同空间区域分区/语法计数、已有 v1 重评分，避免不必要的重复长跑。
3. **收敛本轮试验到有证据支撑的1–2个高价值单因素压缩候选**：优先针对当前Y4/wide+CCSO损失，定位 (a) early SAD粗选漏掉经MRL/TX全评成为赢家的模式，(b) 有限24树分区对官方空间布局漏项，(c) coeff/RDOQ/CDF当前前缀不一致，以及UV联合TX/filt/CCSO。保持因果真实状态、合法语法、相同质量与像素；此前YALL/phase1/dual-finalist负样本不能不加区别重试。不要为某图片坐标或固定QP定制特判。先记录压缩上界与每SB开销再选实现，不扩大无效搜索。
4. **实际实现并校验至少一个候选（在资源许可时）**。在独立实验副本/新目录，不改旧r3证据、 MAIN 默认或旧程序；最小编译、固定AVM解码逐像素、trace/公开统计、faultretry/SAN按受影响范围补；只做原生质量编码、v1/NEG及Q/size/bytes/曲线核查，不做任何正式或非正式速度 benchmark，也不要因CPU占用波动解释编码效率。FFmpeg繁忙时可以改为代码和低负载小图验证，严禁高并发或长时间占用全机CPU；实际编码压缩测试串行低优先级。
5. **交付落盘**：工作区 svt-av2/docs/task-reports/M6-intra-r4-20261026.md 写截至本轮的实验结果、未达标差距、最新成果及下轮路线，另写一份面向下一轮Codex可直接实施的细化设计到 M5-M6-AI-TASKS/08-M6-intra-r5-plan-20261026.md。测试数据和独立副本统一存 <LOCAL_PRIVATE>/ffmpeg-native-m6-tests/intra-r4-20261026，写manifest和复测命令；不声称尚未完成的测试已通过。
6. 当前这一轮不受期望的阶段体积目标驱使篡改数据。若未达到1.00×则诚实报告目前的最佳候选及真实剩余缺口；**不启动无人监管、无上限的CPU长测**，用户将决定下一轮继续。尽最大努力完成具有实现/正确性/画质证据的工作。

本任务经 AgentDock-NuxBox1 发给本机 Codex；不要停止/修改3FUI视频压制、不要改变其他工作项目。

</details>


<details>
<summary>016 · 08-M6-intra-r5-plan-20261026.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\docs\history\M5-M6-AI-TASKS\08-M6-intra-r5-plan-20261026.md`  
原文件 SHA-256：`80d409abb56cdb91d95b3f41fd5fd07ed059372466bf233e6d2a6eadb1ff938d`

# M6 单帧第5轮操作计划（承接2026-10-26 r4）

## 起点与固定目标
r4已独立生成453文件源副本与`SMOOTH/TX`观察器，MSVC Release单工作构建PASS，64×64 10b420小图观察器`R4_APPLY=0`码流、重建与冻结原程序SHA完全相同，固定AVM逐像素解码PASS；在三组分区决策实际产生局部J改善（7/8/5次，**非BD体积**）。具体事实/日志：`svt-av2/docs/task-reports/M6-intra-r4-20261026.md`及`svt-av2/artifacts/intra-r4-20261026/`。

本轮阶段目标v1 93–95同画质AV2/AOM ≤1.00，长期≤0.85体积/≤2时间。最后一次正式已审核的高带体积比**1.185136518 PCHIP /1.186641671 线性**，95位置约1.2613；本轮尚未达标。固定源/模型/成本/r4c1%预算详见`06-M6-frozen-requirements-20261025.md`。**当前FFmpeg仍在运行时，绝不做速度基准，不停止/干扰FFmpegFreeUI。**

## 0. 独立源码身份及预算
1. 只在`svt-av2/artifacts/intra-r4-20261026`里实验，核对其`preparation.json`及453个源SHA；绝不覆盖r3冻结候选、MAIN、holdout或3FUI参数。
2. 比较`smooth-tx.patch`与当前隔离`source/Source/Lib/Encoder/rd_frame.c`，找出其控制流及潜在副作用。r4只编译`/DR4_APPLY=0`用于观察；不能直接将`/DR4_APPLY=1`标成通过。
3. 性能资源受限时，采用低优先级、单进程、有超时的小帧质量测试。不要创建高并发或无人监管长测。

## 1. 先把观察器升级为正确的可控实验
A. 检查`linear_leaf_trial`的候选状态、`anchor`与`probe_cost`更新、`leaf_tools_set`恢复、CDF/重建/coeff前缀和`goto cleanup`错误路径。观察器应测相同因果前缀下单独SMOOTH+合法TX，而不是不恰当混用前一候选修改后的状态。禁用时应保证所有有效公开统计/完整流一致。

B. 设计单因素候选：只增加被原SAD/DCT筛选遗漏的SMOOTH与至多合法TX子集，并保留原候选旧值。改变模式或Tx选择须经**完整叶 YUV RD**及最终完整SB/CDF复评，不能只凭局部`J`优化就强制提交。先设小图对照`apply=0/1`、固定判错/重试/候选反序；若影响真实重建语义，必须先修闭环再编码原生图。

C. 工程验收：MSVC严格新构建；Linux GCC/Clang SAN如工具可用；固定AVM独立解码逐像素，公开/内部统计、全流、原off兼容。所有真实失败原样记录，禁止改写为PASS。

## 2. 压缩率优先验证
先一张小图或少量公开开发裁块，保证`apply=1`不崩溃且合法，观察实际码流变化及v1、NEG、YUV SSE/PSNR（有条件）。只有2D/3D/Real内容都未出现严重反向趋势，才针对原**1080p开发片**少量QP补真实点；原始数据不删，随后补到v1 93–95每条曲线≥4个直接实点、双端包围。AOM cpu0 ALL-INTRA threads1始终用冻结版/冻数据；绝不能换用avifenc、旧VMAF 0.6.1、画质不匹配或同QP bytes当BD。

同时计算PCHIP(log bytes)与线性(log bytes)两个版本及固定VMAF点93/93.5/94/94.5/95；对原r4c重新复算最大1%退化门，NEG单列；v1 95局部不得被93–95平均所掩盖。对质量失败或轻微负的候选保留数据，不准调图像名/固定坐标/只针对单QP特判。任何速度结果均标记NOT_MEASURED。

## 3. 官方AVM的下一层压缩归因（有真实依据才决定实现）
优先本机冻结`avm/av2/encoder/intra_mode_search.c`中的winner TX完整评估和当前`rd_frame.c`SAD/DCT shortlist的差异，按被淘汰模式、变换、MRL联合结果分组，统计**真正改变完整链赢家**的比例；普通更多FWD并不等于体积收益。

若SMOOTH只产生局部J下降但全帧无稳定BD收益，就不要无限扩其搜索：第二方向选**一个**官方布局常见而目前有界计划不能表达的空间混合细长分区，或者实际完整熵上下文的树winner重排序。两者必须从同一SB真实入口快照比较；不要将跨SB局部改善简单相加。CCSO/其它滤波、UV派生TX和MHCCP仅在VMAF色带/色度特征与统计显示收益潜力后再做，DIP/palette/IST/FSC此前已显示低使用率，暂不盲目移植。

## 4. 交付与终点
下一轮记录到`svt-av2/docs/task-reports/M6-intra-r5-YYYYMMDD.md`，实验副本新目录不可覆盖r4。每个候选至少提供`source-manifest`、源码diff、MSVC编译日志、small-case流+AVM一致性、原生质量点（如果实际执行）、两种曲线算法与当前剩余缺口。若没有完整曲线必须报告NOT_RUN，不宣称≤1.0完成。最终候选通过开发集及冻结合同后，未来单独申请holdout20/I-GATE和在设备空闲状态的正式2x性能验收。旧M5稳定版与MAIN保持不变。

</details>


<details>
<summary>017 · README.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\size-r10-20261029\README.md`  
原文件 SHA-256：`2cac5808fa7ba9a7d910161e12c7b7bbfdef996d5ad29a47dc0c72e11a2f7722`

# M6 r10 — 2D / UHD 定向优化

开始于2026-10-29。用户要求直接通过AgentDock继续；不使用Codex/OpenCode，不操作FFmpegFreeUI现有任务，不跑速度验收。任务 tsk_72cf149ec6b31206。

## 目标与边界

- 保留AOM p0、VMAF v1 93–97、NEG联合约束。PCHIP与线性区间几何平均均≤0.90×才可报告本阶段区间平均达标；另报最差采样点、93/95/97锚点。不能以平均代替每点达标。
- 优先未达标的2D 1080p、2D/3D UHD及实拍UHD。目标前不新增内容类别、时间段或分辨率矩阵；1920×1080、3840×2160为重点。
- 原r6-r9、主源码和inter-20261028只读。源码来源为r9/uhd-lambda100-work/source；基线程序baseline.exe SHA256 b9f92e06d7b72bdfab6c14996ef8762455cb32bafd695640d46f41237fffbe9b。
- 评分协议、原六样本清单及r9质量快照已拷贝并保留；不得原地变更正在运行任务的清单。
- 用户允许F:/Media/4K*的实拍替换样本。Article.20实测3840×1608、10-bit420、BT.709，不拉伸/补边冒充3840×2160。清平乐/01.mp4为3840×2160、8-bit420、未标注色彩；仅几何已确认。任何替换样本单独标识，不混入旧FoodMarket曲线，也不宣称无损/原生10bit或拍摄母版。

## 本轮首个假设

x265对应术语是tune animation而非位流profile。借鉴按内容调节的原则，不直接照抄HEVC数值。参考：https://x265.readthedocs.io/en/master/presets.html

r9的逆活动量加权在既有裁块出现负收益。本轮使用不同机制：仅对源图中梯度足够强且结构张量方向一致的8×8区域提高失真权重；平坦区域保持基础权重，逐平面归一，避免仅改变全局lambda。权重在一帧中固定，不依赖候选重建，不对VMAF结果作反馈优化。编码器原候选、规范语法与解码器保持不变。

先实现独立edge候选、关闭路径和单位权重控制；验证旧基线逐字节一致、固定AVM重建一致、边界及双编译器。之后仅在既有2D裁块筛选，有收益才推进原有1080/UHD完整帧。不得将裁块分数或同QP体积减少代替完整同质量曲线。

当前状态：研发中，尚未达到0.90×。实际执行与失败证据见evidence及后续STATE.md。

</details>


<details>
<summary>018 · README.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\size-r11-20261029\README.md`  
原文件 SHA-256：`b57fdf25011e912f9152300a0a311d5c297622826cbdcd1c6473cac3de44e462`

# M6 r11：动漫识别、内容质量控制与2D/UHD单帧压缩

本轮于2026-10-29启动，直接通过AgentDock在NuxBox执行。任务：`tsk_157020f4f211b613`。

## 当前范围

- 最终争取原未达标帧在相同质量下达到AOM AV1 p0体积的≤0.88×；先验收≤0.90×。VMAF v1 93–97与旧版NEG联合约束、PCHIP及线性两种区间平均口径保留，另报最差点与93/95/97锚点。
- 从`<LOCAL_MEDIA>\4K*`单独抽取几十帧，按作品/系列分离开发集与验证集，测试动漫/真人识别准确率是否超过90%。这些帧只用于分类，不扩入本轮压缩验收集。
- 大规模优化仅针对1920×1080与3840×2160。旧Real-2160p-frame48实际4096×2160，仅保留历史证据。r10已有清平乐3840×2160研究帧，色彩元数据未标注，保持研究限定，不与旧实拍帧混成一条曲线。
- 旧r6-r10、生产目录和inter-20261028保持只读。新源码源自r9/uhd-lambda100-work/source；身份见baseline-freeze.json。正式版本未更改。

## 独立质量约束

`quality_control.py`按原图定义边缘、平坦区域、纹理及边缘邻域掩码。动画检查线条梯度/亮度误差、平坦YUV误差和边缘邻域高频误差；真人检查纹理梯度/细节及全局亮度误差。上述为可审计回归检查，不能代表经过主观实验校准的视觉阈值，也不能替代冻结的VMAF协议。质量策略在新候选结果产生前冻结于quality-policy.json。

零失真原图与线条模糊反例已通过验证。r10的edge与SATD在同QP下局部边缘误差改善，但主VMAF下降，不能据此晋升为最佳编码器。证据：initial-local-quality-review.json。

## 已启动

- 原2D-2160p-frame48的λ1.0参考Q88/Q120完整帧编码；输出在full/。结果只有固定AVM解码与重建逐字节一致、双评分和输入/工具身份核验通过后才计入。
- 分类样本准备与检测器实现并行进行，压缩候选正在审查。尚无90%分类通过或0.90/0.88压缩目标已实现结论。

## 恢复入口

先读取本README、任务状态和每个full/*/result.json；RUNNING必须结合真实进程核对。baseline-summary.json保存旧曲线简表。不要改动冻结输入或正在运行的源码、程序、评分脚本。失败与负结果保留。

</details>


<details>
<summary>019 · STATE.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\size-r11-20261029\STATE.md`  
原文件 SHA-256：`d73585d1c54f9d6e7624e28a36508047ae850c28c4430feb1c7778ab4865cfc4`

# r11 当前进度

目标尚未达成：不能宣称分类验证>90%或固定帧<=0.90/0.88×AOM。

## 范围与基线

原压缩帧/原模型/固定AVM不变。仅1920×1080及3840×2160用于完整帧优化；旧4096×2160 Real不进入新优化。新60帧仅分类开发与验证，不加入压缩语料。主源码、r6–r10、inter源码只读；本轮在独立source及各work/source。

## 分类

<LOCAL_MEDIA>\4K*中20作品，每作品3帧，共60。按作品固定36开发/24封存；标签人工视觉复核0更改、0排除，暗场/转场/电影特效全部保留。manifest SHA4920fa2e79efd9df2e11935494074371bb8ba482630f3962542f643b662c8525。最终24帧未运行预测。

v1开发61.1%未通过；v2提高分辨率仍不足。v3对比归一化与平衡片源对验证，最佳开发88.9%、嵌套86.1%，不是独立验收结果。单源LOO下曾出现RBF伪100%，已通过验证输入全部替换为训练均值的反证确认是分折先验伪像，弃用；见classifier/cv-independent-review.json。当前v4加入局部色块与色度统计，仍仅使用36开发帧。UV重解全部Y哈希一致。

## 编码候选

1. directional-work：Windows严格、GCC严格、固定AVM回归、Linux/Windows字节一致及ASan/UBSan控制通过。LSan因环境/proc限制未通过，不宣称泄漏检查通过。原small8非法继承方向/TX组合已诊断修复，失败v1保留。三个裁块均无明确推广收益：2Da+7B且双评分下降；2Db完全相同；2DUHD+1B且双评分取舍。冻结不进入完整帧。见independent-final-review.json、linux-summary.json。
2. lambda-work：原9流固定解码复评分及18个新编码共27PASS。原UHD裁块已统一2160模型；不可用早期1080模型旧分数。共同区间相对λ1.0：075在2Da为1.0806/1.0859，2Db为0.9863/0.9714，2DUHD为1.0150/1.0162（PCHIP/线性）；050均负。仅裁块局部证据，不推广完整帧，更不是AOM目标验收。
3. 原2DUHD λ1补点：Q88=42018B/V1 96.5508/NEG99.539199；Q120=21241B/93.320247/97.568309；固定AVMexact。复核旧Q96/Q104/Q112，五点在共同93.4–96.4区间对AOM均值1.07013/1.07227，旧λ1.5为1.06309/1.06394。λ1仍未覆盖整个93–97，不给全带结论。见uhd-lambda-baseline-review.json。

## 质量控制决策

动画保护源图强边缘与平坦色块；实拍审查纹理/细节。最终通过分类器后，仅用类别决定动漫PCW滤波否决保护，保留已验证λ配置，不再根据二分类自动改λ。全局V1+旧NEG与局部质量门槛都要通过。guard C独立14项测试通过，最终模型/C集成尚未完成。

## 下一步

完成v4开发模型与Python/C一致性，冻结后单次独立24帧验证。审查小矩形UV的RDOQ漏选资格，先在原小控制和三个旧裁块筛查，有真实收益才进入原完整帧。自动化编码是有限任务，不代表后台自行优化到目标。




## 2026-10-29 13:04 UTC: independent classifier accepted; native active path pending



- Frozen CLIP ViT-B/32 zero-shot classifier: selected-model SHA256 `0bc8cfdc648707790575631aed5d121d7938cf822794fb0531903d12ae3ca80f`, at `<LOCAL_AGENTDOCK>-AV2-r11\semantic-selected\selected-model.json`. Official weights SHA `40d365715913c9da98579312b702a82c18be219cc2a73407c4526f58eba950af`. CPU environment and assets are on F:.

- Development: fixed prompts 35/36; linear C0.1 paired-source and nested CV also 35/36. Selected zero-shot before external test because no observed development gain from local fitting. Pre-test frozen Runtime replay of two development frames was exact.

- Final independent test: **23/24 = 95.8333%**, animation 12/12, real 11/12, eight distinct test works, zero exclusions. Only error is free_guy-1 classified animation (cosine margin +0.0038199723). Full report/audit/progress in `<LOCAL_AGENTDOCK>-AV2-r11\semantic-test-v1`. This is observed small-set accuracy, not a population 90% lower confidence guarantee.

- `classifier/semantic-test-once.marker.json` is retained. Do not alter model/prompts/threshold after this test or re-open it for tuning. No v3 hand-feature final test was run.

- Content API/CLI guard off/on strict MSVC builds pass; public API probes each 28 checks, 10 unchanged public-config snapshots; CLI malformed values rejected before encoding. All 16 old inactive-domain routes are byte-exact to frozen base and fixed AVM recon. `content-guard-work/controls-results.json`.

- Automatic raw-preview -> selected classifier -> explicit `--content-type` -> native score runner is being completed. No claim of active-domain filter correction yet; original 2D1080 Q100 is the next target.

- Compression target remains unmet. UV narrow RDOQ common-band mean .988999/.991447 vs crop baseline but high-end worst 1.017813/1.022291; no full-frame promotion. Lambda/direction experiments remain finite negatives. Main/source and r6-r10 are unchanged.

</details>


<details>
<summary>020 · STATE.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\size-r13-20261030\STATE.md`  
原文件 SHA-256：`fc387137fb0e21e5b764065f1cb463e70bae482368742be8f1b81a816d7a95f4`

# r13 当前进度（2026-10-30）



用户目标：基于最佳单帧AV2，对未低于AOM 1.0×的2D/UHD开发帧继续向0.88×以下推进，先0.90×；不扩压缩素材，优化域只1920×1080、3840×2160。完整验收仍93–97双指标与局部QC；另报主V1及NEG代价。0.88目标尚未达标。



- 网页Pro r11已冻结CLIP分类器：20作品60帧、开发36/终测24，独立23/24=95.83%。r13/classifier-review独立核445个非像素身份与分组/单次终测记录，不再读取最终24像素/预测/调参。开发集中有6帧非严格分辨率，不扩入编码集。

- 原guard active但25次零veto，自动Q100码流exact r6 38154B；source-freeze是补丁前基线，实际源码应绑定score source_manifest。score_content_frame未调用quality_control，外部同质量QC接线由sub8在content-qc完成中。

- r13 corpus-frozen SHA b59bcacd1633f9b03aa0b7919eb909ebe32da5189e4c0ce04d8e94b636ce4759：原5个strict帧+已存在r10清平乐t600 3840×2160，原4096 FoodMarket仅历史。清平乐色彩未标注/8bit×4研究限制保留，不宣称认证母版。无新增时间段或内容。

- fine16-pair继承r11未完候选，r13验证冻结：source473文件SHA149dcd9d…；pair-on.exe 757253db7c38b5a22d1918d05a9cd6416c060e1c4f4f2b750dfb7a74943c275c，production-freeze SHA c2b07291…；48强制流、29400几何、san/双平台exact/off旧exact通过。3裁块中心QP packetJ均降。2Db三点相对λ100双指标共同域均值.91938/.93476；UHD高端联合覆盖不足，不外推。

- 完整帧pair pilot已启动：2D1080 Q100、2DUHD Q104，run_batch pair-full-pilot.json（session66300），使用strict清测试env旧入口score_frame.py，输出F:/AgentDock-AV2-r13/full。无新final frame gate，实际最终质量必须测。

- rdoq_search在独立fine16-polish-work继续：原pair first mask锁定，若pair选中再枚举同pair单mask坐标26basic+≤8full，不扩plan域，旧winner严格tie。尚无结果，不触已冻pair。

- 已完成清平乐AOM新增Q16/20/24/28，连r10 Q22/26/30已够5内点+双侧包围。主参考AOM5a5c…3.15.1，r9旧2D1080六点的encoder是另一个3.15.1 SHA548568…且显式profile0。root启动同6QP canonical补验 aom-2d-canonical.json（session待日志），不静默改r9冻结旧结果。

- prediction_search已完成content-review与transform-options规范研究；当前准备cleanup-plan，只读allowlist。保护所有源/patch/脚本/Git、当前源/模型/工具/venv、inter独立研发分支；用户授权优化完成后删除M5前生成输出与M6 10x→1x早期单帧/速度输出，留文档，保1x→.85研发阶段（含负例）。尚未删除。

- C盘59GiB+，新大输出F盘。此前cleanup-20261030把inter quality/benchmarks与M1-M5.3纯生成输出压缩备份迁D并保Cjunction；不得盲删junction或源。后续M5旧8个tar.zst有manifest确认均生成输出，可列删除、保manifest/摘要。

- GitHub目标核实user-Wing/svt-av2，main=5891fe209f9156f15aa1b833086007e6ad0aea8e。旧git fetch连接reset；GitHub connector已读main确认，无新上传。授权为最佳帧内版本上传，先完成验证/具体源码再push，不强推、不改远端历史。当前两个本地snapshot不能代表全部旧源码Git历史，清理不能据此删独有源。



当前root未修改旧r6-r11/主工程，所有候选独立。仍需收齐pilot、QC、polish，选择/整合最佳配置并按完整帧曲线验证；之后文档清理、发布和远端SHA核验。



## 续行（2026-10-30 11时）

- pair原完整2D Q100已PASS：37986B/V1 95.259939/NEG92.611835，对r6 38154/95.786448/92.575408，−168B但V1−.526509，不推广2D原pair。局部QC已测F full/pair-q100/local-quality.json。UHD Q104仍RUNNING（66300批次同一session）。

- 已定位模型响应：v1为CAMBI/SPEEDchroma/ADM3/motion3四特征，pair chromaUV从1.425109变1.978483，其他Y指标微升。chroma-diagnostic独立RBF重算/反事实解释色度项−.537、其它+.010；不是像素/编码反事实。rdoq在chroma-rd-work新原型420归一权重3Y+6UV，代替4Y+4UV、保持总6N和lambda，RDOQ/partial/cache/filter一致；尚未完整帧，待freeze后只测原2D Q100。256crop SPEED均0，不能据此筛该完整帧色度机制。

- fine16-polish已冻结为负/取舍，UHD同码率也双降，不再推广。父pair源不改。

- content-qc已冻结：measure/compare、真实r6十点自比通过、12项边界/身份数值验证。原冻结critical阈值不改，只加最后filtered decoded质量验收。

- integration-work已冻475源（e7273885…），程序F:/AgentDock-AV2-r13/integration/integration-on.exe SHA6d4f5179bb5748a53e71ed20dc68392f24177cb239b7386a6084a58c3bc9a112；pair1/guard1/NUM4，--rd-lambda avm-ai150新mode2仅AVMbase×6/4。public config112B/旧ID不变，30小图fixedAVM+旧lambda100/150exact均通过。仍无新组合完整帧验收。

- 真自动wrapper两完整帧已启动：F auto-pilot/3D-2160p-photo-q104(session24092)，Real-2160p-photo-q96(session53884)，两者content auto、显式photo profile，原输入不改；raw source原5及Qingping既有帧。调用publish/tools/single_frame.py r2 SHA927ed…（150行、有可选--recon），classification包7小文件44KB、无private rows/权重/帧。旧终测不重读，开发两图新旧margin exact。输出JSON标NOT_QUALITY_VERIFIED。

- 完成后用root score_wrapper_output.py 只做existing-stream/recon固定解码评分，并写独立bound result，原score/result不改。helper已16项非法拒绝，不曾native/伪造PASS。

- AOM六旧2D点canonical重编码全byteexact且分数exact，root canonical-aom-compatibility.json；session8172已close。Qingping7点AOM密度完整。

- GitHub main仍5891fe2，git HTTP/1.1 ls-remote成功；已本地clone publish，无push。public classifier+wrapper已复制tools，待bestcore验证后同步核心和重写README，勿发布私人帧/权重/私有manifest。

- cleanup-plan已只读完成：D8个历史tar.zst纯生成输出约21.66GiB；旧C输出路径筛124102条还需依赖闭包/实路径审查，不可直接删。inter独立分支和Cjunction/D目录保护。所有source/patch/script/git与工具环境保护。用户授权优化完成后执行，尚未删除。



## 11时后继续

- 新色度权重三crop方向符合预期（UV SSE降、Y升）；crop SPEED全0，root批准正确性冻结后只在原2D1080 Q100测完整非零色度探针，不能以crop拒绝该机制。rdoq正在san/verify收尾。

- content-review额外SDP语法/架构研究由prediction只读进行。wrapper r2+composer已完成，source/CLI/输入文件身份严格验证。

- Git本地publish已clone，HTTP/1.1 ls-remote成功；当前仅tools暂存，尚未同步发布核心、尚未push/删除。

- comparison-work由sub8汇总r13 strict6基线+现有新PASS；不借旧点填候选，primaryV1/NEG/dual两方法完整密度分别报告；Qingping颜色未标注限制不隐藏。



## 11:40 后

- pair完整2DUHD Q104 PASS：29694B/95.279973/98.766922，对冻结lambda100 Q10429767/95.265641/98.727792，−73B且双指标小升。其local-quality已测；root追加原Q88/Q120两个点形成局部三点曲线，pair-uhd-local-curve.json(session83342)，仍同源码/配置，没有扩大素材。

- chromaRD source manifest3f58bbee…，on SHA9db6e57c7d68ff1bd45f3b1cc6bf8ea3dace8a2df4ff24b2e950ceea326829f6，production-freeze f5f3f004…；off7例、444exact、numeric400009、670480下界验证/402186prune与no-pruneexact、GCC san均通过。root已启动唯一2D1080 Q100 full，chroma2-full-pilot.json（见log/session另查），不加4x/新QP。

- comparison-work 首snapshot6组基线与r9精确一致；新pair两个点仍INCOMPLETE，auto两STARTED非FAIL。Qingping AOM7点局部QC <LOCAL_AGENTDOCK>-AV2-r13/qc/qingping-aom全部测完。


## 12:20 后
- chroma2原2D1080 Q100 full完成PASS：37367B/V1 95.458910/NEG92.336536。对r6 38154/95.786448/92.575408：−787B(−2.06%)、V1−.327538、NEG−.238872。对原pair补回V1+.198971，但不是同质量证明。local-quality SHA87a4becfe2ec6ba4fb4cdf16f0e99729ccc9783a03539566f7982840b4175134。
- 为确认同质量曲线，新增同一原2D1080 Q90/Q110，chroma2-local-curve.json/session83901，workers2；不扩素材，暂不推广。pair UHD Q88/Q120/session83342仍运行。
- integration独立终审完成：review SHA0499016b554b5992d9ce27d2215fbd4c6d83bcbbc0f8fadb117a05c8eb432504，freeze c420c029cb2e017d0894f3f0d926dce5d9b8cd962f7226a36e1dcbe442c16e70；新12次native+固定AVM、6组MSVC/GCC exact、san/guard162均PASS，不替代full质量。
- publish本地核心暂存+portable CMake opt-in preset已验证：base/pair构建、20选择CTest各PASS、16旧integration控制exact。尚未commit/push。sub8正将r9已验证3D1080最佳SATD最小5文件作为明确实验开关合入publish，默认OFF且不动冻结integration/full测试；它有额外完整帧搜索成本。
- prediction与rdoq分别只读检查UV邻居/CFL/回放一致性、UV4量化资格缺口。不重复静态权重，不动冻结源。
- cleanup r2精确生成输出候选1474499条，尚未actual-stat/reparse/ref刷新审查，未删除。源/脚本/工具/current/inter全部保护。C剩59GiB。


## 12:30 后
- 自动Real UHD wrapper完成并bound固定评分PASS：33411B/V1 96.435932/NEG99.632977，对r10原λ150 33530/96.741838/99.638680，不能晋升。auto-scored/Real-2160p-photo-q96/result.json及local-quality已保存，QC SHA18ad8e41b8753ee324e4af9e2daac0fe199397c160444bf147328d79523038f1。3D UHD wrapper session24092继续。
- 启动既有清平乐原λ150六补点Q88/100/108/116/124/136，qingping-native-baseline.json，session46217/workers6；旧r9/uhd-work source+cb7271…原程序，补齐研究基线，不加素材。
- 发布SATD最小合入完成：publication-satd-work/report SHA392b649a…、freeze437a0852…，22流MSVC固定AVM/off旧exact/on旧6crop exact，三构建lambda/still全通过。只改原5文件+CMake默认关闭开关；原base/pair不变。当前源码manifest476 SHAa01fb6af84a5ce5a348a9d2b29e76852fce8e4b1ce2f7f73f3283252f96b2ce5。
- 从此manifest复制476文件到F:/AgentDock-AV2-r13/publication-source-frozen，排除Git且只copy哈希匹配文件，正式全帧source身份用此不随README变化。root启动发布base Real1080Q92(session4697)、SATD 3D1080Q100(session11406)，使用r2 wrapper，content real/animation、显式photo，后需composer bound评分。两个输出在F/publication-pilot。未宣称完整帧等价。
- 清理actual-audit：1474499条全部普通文件、祖先无reparse，47627718813B逻辑大小(44.36GiB)；rows SHA818ef01f225b374c9e377d86dd0d220debef13053e58c23e1924e7a71b8e0244。D历史8tar再次逐SHA及manifest复核，共23254707185B，均未删除。
- root准备publication-draft README/数值脱敏导出/QC两便携原始模块，尚未改public README、未提交/推送。不要在3个r2 wrapper评分完成前改publish/tools/single_frame.py，composer严格绑定旧r2 SHA927ed…。
- prediction确认无UV邻居/CfL/回放/cache错误，新增UV4区域误差增加以低频为主；独立uv-basic-small-work仅补扫420小UV遗漏basic，原顺序tie保留。rdoq在uv-pixel-rdoq-work原4/4下保留旧3路+小UV真实inverse/clip提案，正在小图/3旧crop验证。两者未full、不叠chroma3/6、不改冻结源。


## 13:00 左右
- 原pair UHD Q88/Q104/Q120全部PASS：41745/96.490260/99.498601；29694/95.279973/98.766922；21026/93.414200/97.577839。session83342已close。chroma2 Q90/100/110全部PASS：47928/96.416798/93.588488；37367/95.458910/92.336536；29493/93.928498/90.580844；session83901已close。全部QC已测。
- comparison-work新增summarize_r2.py SHA3555eaf72e6c04903b2f96803738431d7af4480dc0da34a1202cc405484df346，修正旧script只扫pair/auto-pilot而漏chroma/auto-scored/清平乐native补点的范围；旧summarize不改。r2-regression证实原5组baseline+AOM完全相同。snapshot20261030T045905984415Z SHA00c920eb…；再跑会收新完成点。
- partial-full-curves.json：2D1080 chroma在局部94–96对原r6双指标等质均值.999866/1.009317；2DUHD pair在既有r11诊断域93.4–96.4对λ150最佳1.003590/1.015691。都不能替换最佳。候选只有三点，非93–97全域验收，暂不补dense。
- Qingping native六补点session46217：Q10029851/96.358204/99.560456，Q10819039/94.608424/99.139106，Q11612191/92.807207/98.587349，Q1248165/90.708351/97.976037，Q1365420/88.485825/96.915193已PASS；Q88待收。因实际区间内点不足，新增同帧Q98/Q104 density-fill/session40179，勿再扩素材。
- sub8接管auto3DUHD+两发布wrapper完成后的bound评分/QC（只评分，不编码），r2wrapper不得改直到其报告3个完成。auto3DUHD已PASS57770/95.909177/97.819937，对原λ150Q10458108/96.037970/97.797254，−338B/V1−.128793/NEG+.022683，不promote。25个guard active日志considered2676/veto319（试算访问非唯一RU），说明非空保护，但非感知质量通行证。bound result SHA ded1eb10…；QC b9cc5be8…。
- 两UV最小原型都不升full：uv-basic-small-work report4f0568ad…，2Db同字节双降、UHD exact；uv-pixel-rdoq-work review3c96ae54…，474源/404证据冻ef4364f2…，3真实crop均parentexact，2GCC生产san exact，仅合成partial80少7B。诊断字段不是原路线反事实漏失数，见其report映射。
- palette规范可做但静态覆盖不足：仅旧2D两帧完整16×16源恰好2值均0；原DC16近似候选覆盖.679%/.0586%，不投入1000–2000行生产实现。coverage summary e4484167…、Git换行审计 d1ffe935…。不读heldout。
- 已在publish新增定向.gitattributes使classifier7文件及single_frame/QC模块-text；原runtime/predict实际CRLF，默认Git clean会变LF破坏固定SHA。独立core.autocrlf=true add/checkout7文件rawSHA全部保持。未改Gitindex/提交。新增public quality_control.py、joint_quality.py byteexact旧冻结模块及QUALITY_CONTROL.txt；native结果重算metrics exact、joint自检PASS。README尚在publication-draft未替换。
- root已对发布base完整build并启动全部114 CTest（session80495），此前相关20+22streams仍PASS，当前在40附近，未发现失败。不要在其结束前改核心。
- 清理r3 frozen plan66de8024…/manifest1a9ecfd1…，新增14059条；actual-audit-r3全部ordinary/no-reparse，883934317B。prediction正在写默认dryrun执行器并做dryrun；原r2的1474499与最新refs交集0。禁止在其准备阶段删除。D8tar再次SHA+祖先普通审核通过，23254707185B，保manifests。
- rate-band-research报告fe16ac90…与domain supplement ac5544f7…：λ2在旧Real/3D裁块已试，原2D/UHD未找到；M6 AC步长与AVM DC步长不逐值等价。r9 UHD与r13 pair CCSO/滤波源码完全相同，支持4096/2160/2176units；旧r9 SATD限1080；integration增mode2+guard。旧summary笼统“UHD CCSO禁用”不可用于当前pair。
- rdoq新接ccso-bands-work：基于r9 UHD最佳源NUM6/DEN4、pair0，不动publish/冻结源；计划8/16/32/64 band有限两个起点，原4band81穷举+NONE/PCW实际完整包incumbent保留，CDF上下文4不变。先forced规范/offexact/固定AVM/san/小crop，不full。未宣称收益。


## 13:45 前后：研究本轮收尾/发布清理准备
- 全部114 CTest原运行session80495最终112PASS、2FAIL（thin64_domains与intra_mixedthin）。不是生产validator错误：旧invalidID3285已合法ROOT_ASYM；仅publish两测试3285→3289并加解释，原非法/partial回调断言保留。publication-test-fix-work/report SHA c25242c5…，8相关定向回归全部PASS，97生产构建输入/程序SHA未变。不可声称修复后单次114全绿。
- 三wrapper完成，sub8 wrapper-completion-work/report SHAaef7a637…、freeze255a1dcf…70绑定通过；旧R2代码另存r2-bound-code。Real1080Q92=88780/95.401826/93.635571，旧λ150同点stream/decoded exact。3D1080SATD Q100=112682/95.842621/92.989975，无旧同Q100全帧；新SATD112656B因J84625852>84603724被拒。其guard50active calls considered1208/veto77非唯一RU。三条均有fixedAVM/localQC。
- root已替换publish/tools/single_frame.py为R3 SHA454e5bc1b7b7d69491cf7970eb3bb01b220c3ef4e4f7de00eceafd6b0d638814：1080动画auto λ1，real/UHD auto λ1.5，manual覆盖不改标签；JSON记录wrapper self-SHA并末尾验证。12映射/两实际UHD R2photo命令等价通过，独立审ebe9aca…无阻断。原score_wrapper_output.py依旧绑定旧R2，在未来对R3不要直接调用它；旧bound code归档完整，不改旧记录。
- public README已替换并明确目标未全面通过、调用端需新头重编，链接最终numerical-evidence与SINGLE_FRAME_RESULTS（sub8待Q80完成后写）。新增QC两portable模块和QUALITY_CONTROL.txt、BUILD_VALIDATION.txt、BUILD_IDENTITIES.json；97生产输入与publication-source-frozen byteexact，只有两test fixtures+文档/tools不同。未提交/上传。
- root prepare_github_upload.py/read_git_blob.py已准备本地Git staged blob规划/有界base64读取，尚未运行、未stage。推荐GitHub connector create_blob/tree/commit/update_ref expected_sha，无force；目标user-Wing/svt-av2 main仍5891fe209f9156f15aa1b833086007e6ad0aea8e（最新ls-remote）。source-only不上传私人帧/权重，generator含private alias映射不可公开。
- Qingping已有9native点、7AOM点，两者5inside+brackets，但nativeNEG在AOM V1 96.905–97.0仍不可达，dual未完成。primaryV1均值.9979936/1.001717。为此加仅Q80上包围原帧，qingping-native-dual-upper.json/session46893，约SB1700/2040，仍运行。sub8接管完成后QC q80、另10点manifest（旧9保留）、最新summarize_r2快照/公开数值与结果TXT，不做周期progress写入。旧9点QC已F/qc/qingping-native完成。
- 新bands最终正确preCCSO诊断：ccso-preinput-work/report564fb896…/freeze4a8f27eb…，只复制旧decoder cache，1TU+link4.94秒、一次decode.84秒，原AVM1520源/38linkinput/fixeddecoder不改；final SHA exact固定AVM，preCCSO三planeSSE/原lut69/48/510flagmap/estimatedJ exact。8/16 band新header多3/7B，flag省.987bit且V SSE差76，J反增890/2100；不full、不新机制，研究JSON稳定。CCSO prototype及准确输入研究均负例保留，不合入publish。
- 全清单正式dry-run已启动：cleanup-plan/final-dry-run，PS session9196，-IncludeHistoryArchives但无-Execute，绝未删除。最终guard PASS，5471 JSON重hash/35532 protected/1418已知历史/inferred errors。逐文件当前已约932080条均dry_run_ready、无skip；1,488,558总条目仍跑。journal打开时FileInfo.Length可能0，Python可读实时记录。之后需审summary所有statuses，再等Q80/sub8/本轮报告都稳定，运行同脚本fresh -Execute -IncludeHistoryArchives（会自动重扫依赖/重stat、D全SHA），非递归精确删除、保source/env/inter/refs。
- C约58.7GiB，用户40GiB目标已保持。D8tar只读全SHA及祖先审核23254707185B；待用户已授权的最终清理执行，仍未删除。
- 本轮新候选均未能可靠替换各组旧最佳，0.9/.88组目标未达。必须如实表述；发布为当前已验证最佳源码与分类/QC，不宣称所有内容体积目标已实现。收齐Q80后完成清理/上传并核remote tree/hash。


## 最终发布与清理（2026-10-30 14时）
- 两批实际执行均完成，无skip/failed：media1488558、逐case日志85656、D历史纯媒体归档8，共1574222文件、71781772798逻辑字节(66.852GiB)。所有manifest/源码/工具/环境/当前引用和inter两分支保留。C可用105.17GiB。机器汇总cleanup-plan/final-summary.json、可读FINAL_STATUS.txt，完整删除journal在final-execution与r4/run-*。
- 清理后现行553个引用文件逐SHA验证PASS；r9冻结41报告身份PASS。
- Qingping Q80完成50712/97.959769/100，十native/七AOM点双约束93–97已完全覆盖、各5内点及双侧包围。新完整dual均值1.089529/1.098773，primaryV1.998789/1.001717但NEG均降，不达标。最新comparison snapshot20261030T055034152195Z SHA6139a641…；sub8最终43绑定report994fa10d…。
- GitHub已通过connector上传62新/改blob并逐OID核验，创建tree与本地暂存完全一致9467f0b95d99cfbc76196ea2b31b7ecb365a4429，父5891fe2，commit95a64a8d443fe4539057053a119614b3e88cf07f。main用expected_sha+force=false更新成功；已GET远端验证commit/tree，并fetch对齐本地publish HEAD，status干净。未创建PR。
- 提交链接https://github.com/user-Wing/svt-av2/commit/95a64a8d443fe4539057053a119614b3e88cf07f 。新分类/QC/数值包不含私人素材/权重；parent既有107文件的本地历史路径不改，不宣称整树无路径。最终只读发布审计cleanup-plan/publish-preflight/report.json SHAc2ab311f…通过。
- 本轮可验证工作已交付：内容分流/QC、候选研究及否决、冻结证据、早期输出清理和最佳已知源码上传。0.90/.88所有内容组目标仍未达成，>16×速度未验收；不得称质量/速度目标完成。所有编码/子任务写入已结束。

</details>


<details>
<summary>021 · FINAL_STATUS.txt</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\size-r13-20261030\FINAL_STATUS.txt`  
原文件 SHA-256：`899b4b2df127e00a650491d8732fc74751b18b9c0b7178f627809b91e80a91ae`

本轮记录（2026-10-30）



删除确认的早期生成媒体1,488,558个、逐次运行日志85,656条、历史纯媒体归档8个。合计1,574,222个文件，逻辑大小66.852 GiB，逐文件删除结果无跳过或失败。D盘归档对应manifest全部保留且SHA复核通过。C盘可用空间105.17 GiB，超过40 GiB目标。



保留独有源码、脚本、工具/环境、当前基准引用、原结果文档及两个独立inter分支；用途未确认的输入不按扩展名删除。阶段历史汇总见stage-summary.json和r4/README.txt及r4/*-cases-numeric-summary.json。执行逐文件清单与结果在final-execution和r4/run-*；这些是审计记录。



清理后当前数据553项SHA全部通过，r9冻结41份报告身份通过。



本轮新候选未能稳定替换旧最佳；动画/UHD尚未达到0.90倍，实拍1080历史双约束均值约0.881倍。95.83%为原冻结分类终测23/24；终测未重新调参。源码、分类与质量工具、六帧93–97结果已上传主分支：

https://github.com/user-Wing/svt-av2/commit/95a64a8d443fe4539057053a119614b3e88cf07f

远端tree与审阅暂存树完全一致；源码无强推，旧历史保留。新增62文件未包含私人素材/权重，旧历史文档中的原工程路径保持原样。



所有编码已结束；>16倍速度未验收。目标未达到的组和全部负例保留在size-r6及以后研发记录，不能以单点更小替代同质量比较。

</details>


<details>
<summary>022 · README.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\size-r6-20261027\README.md`  
原文件 SHA-256：`1db43b4643ca2faa09f3ffbb07bc7250e276dca8cc627e1d032858635417f21a`

# M6 单帧体积优化：r6

目标是在相同质量下达到 AOM AV1 的 **0.85× 文件大小**。本轮尚未达到该目标。

本轮证据最完整的组合为真实上下文 RDOQ + AV2 PC-Wiener。七点实测在主区间相对 AOM 为 **1.015456×（PCHIP）/ 1.017686×（线性）**；相对本轮 fine16 基线缩小 **4.47% / 4.30%**。它仍比 AOM 大约 1.5–1.8%，距离 0.85× 还需在当前结果上进一步缩小约 16.3–16.5%。这些是单个 2D 帧的区间筛查结果，尚未完成宽质量范围和跨内容验收。

比较口径沿用冻结的 1920×1080、10-bit 4:2:0、2D frame48；主区间为 VMAF v1 93–95，同时检查 NEG。保留全部原始质量点，不删除倒序点，分别积分 PCHIP 和线性插值的 log(bytes)，禁止外推。并行编码仅用于质量测量，不作为速度成绩。

## 已完成的改动

- `rdoq-context-work`：将真实前缀和已适应熵上下文用于新增 RDOQ 提案，保留旧提案和完整 SB 成本比较。该比较不能保证整帧始终优于旧版本。
- `pcwiener-work/source`：实现 AV2 PC-Wiener 标量核、序列／帧／RU 语法与生产路径。每个 RU256 比较 NONE/PCW，随后按完整码流成本决定保留或回退。Y 使用去块后的像素，UV 保持原 CCSO 路径。
- `context-pcw-work/source`：以上两项的独立组合；没有直接相加两项实验收益。
- `ibp-work/source`：实现 AV2 IBP 的 DC 边界混合、方向第二预测与规范权重。这仍是对照候选，没有替换默认版本。
- `context-pcw-ibp-work/source`：三项组合，IBP 关闭时逐字节复现 context+PCW，开启时已通过小图固定 AVM 解码。

## 验证与证据

- `audit/fine16-17points.json`：原 fine16 的完整 17 点复核。相对 AOM 主区间比值为 **1.062941× / 1.063434×**，不是 0.85×。
- `context-band-7points.json`：上下文候选七点区间筛查，五个点位于 93–95 内，原始点无倒序。相对 AOM 为 **1.033099× / 1.034034×**；相对 fine16 的 PCHIP 比值 **0.971926×**。共同 NEG 区间比值 **0.990798× / 0.991950×**，局部最大回退约 0.64%。这仍缺少候选完整宽区间及跨内容验收。`context-screen-summary.json` 为早期两个点的逐点比较，不是完整曲线收益。
- `context-pcw-band-7points.json`：双项组合七点全部通过冻结源码、程序身份及独立解码检查；五个点位于主区间内，两个质量模型均无倒序。相对 AOM 的主区间比值为 **1.015456× / 1.017686×**。在共同 NEG 区间 89.680725–92.575408，相对 fine16 为 **0.978015× / 0.979094×**，相对 AOM 为 **1.007628× / 1.007774×**。主模型相对 fine16 仍有约 0.15% 的局部 PCHIP 回退，区间平均收益不是每个质量位置都改善的保证。
- `ibp-band-3points.json`：独立 IBP 候选只有三个点、主区间内一个点，不能正式验收。三项组合 Q110 为 29494 bytes、VMAF v1 93.688425、NEG 90.928216；双项组合对应为 30013 bytes、93.872384、90.827214。三项组合呈现质量取舍，未替换当前单帧入口。
- `baseline-cross/summary.json`：3D、实拍各两个 fine16 基线点。它们只提供质量包络，区间采样不足，不能正式验收。
- `pcwiener-work/kernel-check/final-results.json`：20 组标量核与 AVM 逐像素对照、1080p 混合恢复单元对照。
- `pcwiener-work/validation-r3/results.json`、`validation-ru-r2/results.json`：实际码流、强制开关与混合 RU 的独立解码验证。关闭后与 fine16 的码流及重建逐字节一致。
- `pcwiener-work/validation-san/results.json`、`review-summary.json`：生产路径 sanitizer、9 个现有单测及 36 组外置熵前缀测试。独立估率 API 尚未完整镜像 PCW 非法元数据检查；当前原生编码入口的合法参数域已验证。
- `ibp-work/result.json`：2196 条强制模式、1344 条 MRL 码流及跨编译器 sanitizer 结果。
- `results-summary.json`：已完成原生单帧结果；失败和未完成记录单独保留。

旧 CDEF-map 提案在本帧同时损失主模型与 NEG 质量，未合入。变换细分的 scalar 和 phase0 RDOQ 提案均未获得最终 packet 收益，保留在 `txsplit-work`，未合入。

## 复现

`encode-single-frame.ps1` 是 context+PCW 候选的单帧入口，已用 8-bit 小图验证输出可被固定 AVM 精确解码：

```powershell
& .\encode-single-frame.ps1 -InputPath C:\path\input.y4m -OutputPath C:\path\frame.obu -ReconPath C:\path\recon.yuv -Qp 110
```

`score_frame.py` 冻结输入、源码、程序、模型和解码器身份，逐字节比较编码器重建与固定 AVM 解码结果，再运行 VMAF。示例：

```powershell
& 'C:\Program Files\Python311\python.exe' -B .\score_frame.py --encoder .\context-pcw-work\combo.exe --source .\context-pcw-work\source --label context-pcw-fine16 --content 2D --qp 110 --threads 1 --out .\screen\new-combo-q110
```

每次使用不存在的新输出目录。PCW 构建使用 `pcwiener-work/build.ps1 -Arm on`；该脚本进行干净构建，避免本机中文 MSVC 依赖扫描漏掉头文件布局变化。`source-bundle.zip` 包含完整 fine16 基线与五种候选的增量，文件及程序 SHA-256 见 `source-manifest.json`。原始 r5、旧速度实验及 AVM 参考源码均未修改。

当前单帧入口使用 `context-pcw-work/combo.exe`，SHA-256 为 `0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d`。本轮计划内原生编码均已结束；历史失败记录保留用于审计，不计入收益。

下一项已审计但尚未实现的方向见 `ibp-work/sub8-feasibility.json`：在保持 UV 所有权与几何正确的前提下，将部分 8×8 亮度块细分到 4 像素维度，并重新评估后续因果预测及完整码流成本。直接降低最小块尺寸会破坏现有假设，不能作为安全改动。该方向的收益尚无实测证据。

</details>


<details>
<summary>023 · README.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\size-r7-20261028\README.md`  
原文件 SHA-256：`e97c9a48127459bc52080246a64062a70c3098e320440659c703e42c80abed30`

# M6 单帧压缩优化 r7

目标为相同质量下不超过 AOM AV1 的 **0.85×**。当前默认参考仍是 r6 的真实上下文 RDOQ + PC-Wiener：2D frame48、1080p 10-bit420、V1 93–95 七点区间为 **1.015456× / 1.017686× AOM**（PCHIP / 线性）。r7 已找到局部收益和双指标取舍，但尚未证明全面优于 r6，更未完成 0.85× 目标。并行诊断时间不用于速度比较，本轮没有新的加速成绩。

用户明确：**V1 与 NEG 都是评价标准；V1 基本不变、NEG 提升也应保留。** 本轮保留每个候选的两个模型原始分数、同码率变化及同质量体积；不因单一模型平均回退丢弃局部收益，也不擅自给“基本不变”设置数值阈值。另保留 Y/U/V SSE，避免两个 VMAF 模型未反映的色度改善被遗漏。

本轮已完成 **24 个 M6 完整帧点**、**4 次 AOM 参考编码**（含 Q24 复现），没有遗留编码任务。所有 M6 完整帧输出均与固定 AVM 解码逐字节一致。正确性原型、失败候选和局部收益均保留；本轮完成不等于 0.85× 或 16× 目标完成。

## 可直接调用的版本

`encode-single-frame.ps1` 提供单帧入口，默认 `r6`。启动前核对程序 SHA256，并临时清除测试环境变量，结束后恢复。8 个选项均通过实际编码和固定 AVM 解码检查；r6、QM10 还逐字节复现原有小图证据，见 `entrypoint-check/result.json`。

```powershell
.\encode-single-frame.ps1 -InputPath C:\path\input.y4m -OutputPath C:\path\output.obu -Profile qm-y10 -Qp 116 -Threads 1
```

| Profile | 内容 | 使用边界 |
|---|---|---|
| `r6` | 真实上下文 RDOQ + PC-Wiener | 保留的默认参考 |
| `uv-directional` | 补齐六种 UV 斜向预测 | Q108 有完整帧局部收益，其他区域有退步 |
| `rdoq-sweeps` | 交替扫描 RDOQ | 保留 NEG 取舍候选；三点主模型均退步 |
| `combo-rdoq-uv` | 上述两项组合 | Q110 NEG 提升较大，V1 也下降更多 |
| `qm-y10` | Y level10 量化矩阵，UV flat15 | 低码率点双指标提升，其余点有取舍 |
| `qm-y8` | 较强的 Y 量化矩阵 | 实拍完整帧 Q109 双指标提升，Q96 同码率两项下降 |
| `qm-y12` | 较弱的 Y 量化矩阵 | 3D 完整帧两点同码率双指标小幅提升 |
| `lambda-125` | λ × 1.25 | 实拍两点 V1 同质量体积诊断约比 r6 省 5.5%，尚需密集曲线 |

这些是独立实验选项，不是自动按内容切换的最终编码策略。源码与二进制映射见 `profiles.json`，没有覆盖主工程或旧 r6。

`tested-profiles-source.zip` 包含这 8 个可执行版本、7 份完整源码、可运行入口和编译宏清单；已逐个核对 3309 个文件哈希。归档 SHA256：`fd63ac9b46aa635fca047ae392d7d2d7747adc2e5edf8e981bf12a4793833adc`。实验原型及未进入入口的候选仍保留在各自工作目录。该包是已验证选项的归档，不代表全部选项都优于 r6。

## 2D 完整帧结果

以下都是固定 AVM 解码与编码器重建逐字节一致之后的原始点。差值为**同 QP**相对 r6，不能直接当作同质量节省。

| 候选 / QP | 字节 | Δ字节 | ΔV1 | ΔNEG |
|---|---:|---:|---:|---:|
| UV / 108 | 31468 | −269 | +0.006448 | −0.026102 |
| UV / 110 | 29857 | −156 | −0.091256 | +0.066442 |
| UV / 113 | 28018 | −40 | −0.609423 | −0.095501 |
| UV / 116 | 26055 | −243 | +0.049066 | −0.025547 |
| QM10 / 110 | 29946 | −67 | −0.321311 | +0.163586 |
| QM10 / 113 | 28071 | +13 | −0.219057 | +0.081734 |
| QM10 / 116 | 26325 | +27 | +0.392618 | +0.120415 |
| RDOQ+UV / 110 | 29678 | −335 | −0.323893 | +0.112277 |

`dual-metric-summary.json` 在候选真实字节数处对 r6 七点曲线插值，保留 PCHIP 与线性两种结果。典型同码率差异如下（PCHIP，候选原始分数减 r6 插值分数）：

| 候选 / QP | ΔV1 | ΔNEG | 解读 |
|---|---:|---:|---|
| UV / 108 | +0.070 | +0.047 | 局部双指标正向，线性同方向 |
| UV / 110 | −0.062 | +0.106 | 轻微 V1 取舍候选 |
| UV / 113 | −0.599 | −0.084 | 该点退步，不用附近赢家掩盖 |
| QM10 / 110 | −0.308 | +0.181 | 明显模型取舍 |
| QM10 / 113 | −0.222 | +0.078 | 模型取舍 |
| QM10 / 116 | +0.372 | +0.107 | 局部双指标正向，线性同方向 |
| RDOQ+UV / 110 | −0.265 | +0.193 | 不能简单相加单项收益 |

UV Q116 字节数低于 r6 覆盖下限，不外推该原始点的同码率差异。UV 五点 V1 93–95 区间为 **1.031907× / 1.035910× AOM**，整体仍不如 r6；这不否定 Q108 的局部收益。QM10 四点与 RDOQ 三点的区间报告也保留，稀疏曲线不当作正式验收。

## 跨内容完整帧结果

以下原始点均为本轮重新编码、独立解码和双模型评分；r6 对照使用 context+PCW 冻结版本，未拿旧 fine16 冒充。

| 内容 / Profile | QP | 字节 | V1 | NEG |
|---|---:|---:|---:|---:|
| Real / r6 | 96 | 92908 | 95.505316 | 93.438480 |
| Real / λ1.25 | 96 | 80225 | 95.090697 | 93.312713 |
| Real / QM8 | 96 | 88680 | 95.012968 | 93.156940 |
| Real / r6 | 109 | 48912 | 92.101545 | 91.328090 |
| Real / λ1.25 | 109 | 45949 | 91.950001 | 91.072093 |
| Real / QM8 | 109 | 47045 | 92.470763 | 91.467010 |
| 3D / r6 | 106 | 103620 | 95.402540 | 92.375467 |
| 3D / QM12 | 106 | 103943 | 95.474976 | 92.438561 |
| 3D / r6 | 125 | 66480 | 92.150687 | 88.561009 |
| 3D / QM12 | 125 | 66809 | 92.260334 | 88.705742 |

Real QM8 Q109 是直接的压缩/质量赢家：比 r6 少 **3.817%** 字节，V1 **+0.369218**、NEG **+0.138920**。但 Q96 同码率下两项都退步，因此不能按低码率结果全局推广。

各组只有两个点，以下都属于**插值诊断**，不是正式验收：

| 内容 / 候选 | V1 93–95 相对 r6 体积 | NEG 共同区间相对 r6 体积 | 同码率变化 |
|---|---:|---:|---|
| Real / λ1.25 | 0.944998× | 0.947737× | V1 +0.201～+0.364；NEG −0.005～+0.357 |
| Real / QM8 | 0.984689× | 0.979153× | V1 +0.525～−0.245；NEG +0.243～−0.128，随码率升高收益转负 |
| 3D / QM12 | 0.991767× | 0.991902× | V1 +0.050～+0.073；NEG +0.037～+0.102 |

NEG 的共同区间随候选而异，表中不用于候选之间直接排名。两点曲线自身 PCHIP 与线性相同，不能把方法一致当作样本充分。

### AOM 参考补密

`aom-reference-work` 重跑原 Real Q24，码流、解码、字节数、V1、NEG 和全部特征均 exact；随后使用同一冻结程序/命令参数补 Q22、Q25、Q23，旧六点保留。

| AOM QP | 字节 | V1 | NEG |
|---|---:|---:|---:|
| 22 | 79796 | 94.623695 | 92.825463 |
| 23 | 73608 | 94.149089 | 92.494208 |
| 24（复现） | 67488 | 94.017038 | 92.325053 |
| 25 | 59863 | 93.418421 | 91.787268 |

原参考在 V1 93–95 只有一个直接点，PCHIP/线性可造成约 2%～4% 差异；补密后有四个区间内直接点。新旧 AOM 比较都保留在 `cross-metric-summary.json`，`aom_refined` 才是新增九点参考。

以加密 AOM 为参考，Real λ1.25 的 V1 93–95 体积诊断为 **0.960040× / 0.955054×**，NEG 共同区间为 **0.899357× / 0.891124×**；仍未达到 0.85×，且候选本身仍只有两点。Real QM8 的 V1 区间约 **1.000362× / 0.995167×**。3D QM12 对旧六点 AOM 的 V1 区间约 **1.116485× / 1.099670×**，仍有明显差距，不能用实拍结果代替。

## 其他实现与证据

| 路线 | 已验证事实 | 当前结论 |
|---|---|---|
| RDOQ 交替扫描 | 24 条生产流 AVM exact；关闭宏复现 r6；跨编译器及 sanitizer 通过；真实裁块有成本赢家 | 保留 NEG 取舍点，未全局启用 |
| UV 方向预测 | 强制方向、部分边界、位深/色度格式、跨编译器和生产路径均验证 | 保留完整帧局部收益和退步区域 |
| PC-Wiener RU64/128/256/512 | 56 条实际流一致，含高位 mask/部分边界；576×584 省 1 byte、重建相同 | 微小正向，保留独立实现 |
| CCSO 扩展偏移 | 288 条强制流一致；6 个生产点全部保留旧输出 | 尚无净收益，不合入默认 |
| Y DC 独立量化 | −8/−16/+8 共 20 条生产流一致，裁块结果混合 | 保留，不推广 |
| Y-only 8→4 细分 | 修正 Y/UV 参数归属后，464 条规范/生产流一致，容量回滚及 sanitizer 通过 | 6 个真实裁块无最终成本赢家；旧归属错误下的收益不算数 |
| Y 量化矩阵 | 矩形/64轴、逐位置反量化、RDOQ、重放/缓存均同步；QM10 26775 条强制流一致；每档 17093875 个 sanitizer oracle 检查 | level8/10/12 收益依内容而异，分别验证完整帧 |
| λ 校准 | 36 个裁块点均解码一致；λ=1 字节 exact；用统一 λ 比较完整包成本 | Real 裁块 λ=1.25 正向，不推广为全内容默认 |
| QM8 + λ1.25 | 24 曲线点固定解码一致，λ=1 对照与 QM8 exact；纹理块同码率对 r6 V1 −0.003、NEG +0.196，对单独 QM8 两项均提高 | 首块比单独 QM8 明显差，保留组合适用性，不相加单项收益 |
| 固定 Y 的 UV 后处理 r1 | 24 条最终/被拒裁块流一致，6 例 Y/off exact，小图 sanitizer 及跨编译器通过 | 3D 裁块 3519 B 不变、Y/V1/NEG exact、U/V SSE 下降；3D 完整帧 Q125 最终回退，码流/重建/全部评分 exact r6 |
| 固定 Y + UV RDOQ r2 | 两个 2D 裁块，8 条最终/被拒流 exact，Y 与两个模型分数相同 | 候选更大，均回退；未重复 sanitizer，不借用 r1 检查冒充 |
| AV2 IST | 20160 核向量、420 强制流一致；动态开关另验证 20 流、9268 checks；真实候选 cache-off/GCC sanitizer/MSVC exact | Q110 裁块 +22 B，V1 −0.006201、NEG +0.008855，完整包回退 exact；DCT16 留待后续 |

固定 Y 方案保存被拒候选的码流、重建和评分，避免回退隐藏语法错误。门控为实际 bytes 不增、完整包成本降低、U/V 各自 SSE 不升、最终 Y exact。该门控不等于所有感知质量的最优解，因此被拒结果仍保留。

固定 Y 的完整帧日志记录：原包 66480 B，候选 66756 B；候选 U SSE 降 6089、V SSE 降 95425，Y exact，但包长及最终成本变大，故正确回退。完整帧的独立解码验证针对最终输出；不把裁块候选的解码验证冒充该被拒完整帧候选已独立解码。

## 测量与复现口径

- `score_frame.py` 冻结输入、源文件清单、程序、模型和解码器身份；只有 `PASS_NEW_SINGLE_FRAME_FIXED_DECODE_V1_NEG` 才是有效点。重建必须与固定 AVM 解码逐字节一致。
- `collect_results.py` 汇总 `screen`、`screen-real`、`screen-3d`，保留未完成/失败记录；`results-summary.json` 不声称达标。
- `audit_dual.py` 输出 2D 原始点、同 QP 差异、各分量 SSE 和同码率双指标。参考为 r6 context+PCW 七点，不是更旧的 fine16。
- `audit_band.py` 保留全部点，对 log(bytes) 做 PCHIP/线性插值；V1 固定 93–95，NEG 另用 r6 覆盖内预先选定的 90–92，并保留共同覆盖区间。都不外推。
- `audit_cross.py` 按 Real/3D 分开，与本轮实测 r6 对照；AOM 来自冻结原记录。两点候选的 PCHIP 与线性本就相同，不能把两者一致当作密度足够。出现质量随码率反转时，保留同码率曲线但不报告歧义的逆插值区间。
- `cross-metric-summary.json` 保留全部跨内容原始点、同 QP Y exact/SSE、同码率双指标和新旧 AOM 诊断比值；本轮任务已全部完成。
- `inputs/manifest.json` 记录 3D/实拍裁块来源。裁块、单个完整帧、跨内容正式验收属于不同证据级别，不能互相代替。

所有源码和正确性原型均留在独立快照中。没有删除退步点，也没有用工具数量代表压缩目标完成。

</details>


<details>
<summary>024 · README.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\size-r8-20261028\README.md`  
原文件 SHA-256：`16586cdc76c0b0cda95d3008493702cafae97551ec7da9408ffe3341a55e7afe`

# M6 单帧压缩优化 r8



用户的新目标：在 V1、NEG 质量保持的条件下，将体积推进到 AOM AV1 的 **0.95–0.90×**。已有收益主要来自实拍内容；2D、3D 的差距仍单独报告，不把实拍结论外推到所有内容。r6/r7 原证据和程序保持冻结，本轮仅写 r8 独立实验目录。



1. 补密 Real 的 r6 与 λ1.25 曲线：新增 Q98/100/102/104，复用已冻结 Q96/109。验证固定 AVM 解码 exact、输入/源码/程序/模型身份一致，检查曲线反转与插值敏感性。

2. 在独立源码中筛查 λ1.5/2.0、较弱 QM12+λ1.25、DCT16 IST。先规范与裁块验证，通过后才推进完整帧；保留原候选和最终码流回退。

3. 对同一个 AOM 质量点同时满足 V1 和 NEG，再比较实际或插值所需字节。两个模型的独立区间结果也保留，但不把它们误当成同时保持质量。



使用冻结的单帧 1920×1080、10-bit 4:2:0 frame48；`score_frame.py` 与 r7 严格评分入口字节相同。并行编码用于质量诊断，耗时不作为速度成绩。



新增候选由裁块开始筛查；不以同 QP 的更小文件代表同等质量。任何范围不足、曲线反转、模型取舍、失败点都保留并标注，不补造结果。



## 已确认的直接实测收益



下表使用同一个 Real frame48 的实际编码、独立解码和评分，不使用插值。M6 的两个分数均不低于表内对应的 AOM 点。不同编码器 QP 不可直接对应，因此比较的是实际分数和字节。



| AOM 对照 | AOM 字节 / V1 / NEG | M6 配置 | M6 字节 / V1 / NEG | 体积比 |

|---|---|---|---|---:|

| Q22 | 79796 / 94.623695 / 92.825463 | λ1.5 Q96 | 70604 / 94.701499 / 93.132214 | 0.884806× |

| Q23 | 73608 / 94.149089 / 92.494208 | λ1.5 Q98 | 65635 / 94.523825 / 92.899686 | 0.891683× |

| Q24 | 67488 / 94.017038 / 92.325053 | λ1.5 Q99 | 60996 / 94.037543 / 92.662787 | 0.903805× |

| Q25 | 59863 / 93.418421 / 91.787268 | λ1.25 Q104 | 55317 / 93.499948 / 92.117692 | 约 0.924× |



前两点证明本帧的局部目标已达到：文件约小 11.5% 和 10.8%，V1/NEG 同时提高。它们不证明每个质量位置、每种内容都达到相同比例。λ1.5 在部分较低码率处仍存在 V1 与 NEG 的取舍。



## 运行与复现



`encode-single-frame.ps1` 必须显式选择 Profile，避免把某个内容的优胜版本直接变成通用默认。入口核对程序 SHA256，编码时清除测试环境变量，结束后恢复；固定输出一帧。



```powershell

.\encode-single-frame.ps1 -InputPath C:\path\input.y4m -OutputPath C:\path\output.obu -Profile lambda-150 -Qp 96 -Threads 1

```



可选 `r6`、`lambda-125`、`lambda-150`、`qm12-lambda125`。4 个入口已做实际小图编码并通过固定 AVM 解码；r6 与两个 λ 版本还逐字节复现此前输出。程序和源码映射见 `profiles.json`，完整编译宏随源码归档保存。所有原 r6/r7 文件不变。



`tested-profiles-source.zip` 保存四个可运行版本、完整源码、入口和编译宏，1888 个归档文件哈希已核对。归档 SHA256：`ddb93c23041907e1366f2cce732ef646cec01f269c42935c7f6554c67f230608`。未推广的 λ2.0 与 IST16 实验仍留在各自工作目录；质量结果以本目录完整汇总为准。



## 正确性与未推广的尝试



- λ1.5/2.0：36 个裁块点及 8 个小图固定 AVM exact；10 个复用点重新解码、评分全特征一致；λ=1 对照复现 r6。生产源文件与已冻结 r7 λ 版本相同，仅使用不同编译比例。λ1.5 在 RealA 明显改善，RealB 与 3D 有回退；λ2.0 没有稳定双模型优势。见 `lambda-work/all-analysis.json`、`verification.json`。

- QM12+λ1.25：48 个裁块曲线点固定解码一致，λ=1 对照字节 exact，实际包成本统一基准 λ 重算；两个 Real 裁块正向，一个 3D 裁块回退，另一个主要改善 NEG。见 `qm-lambda-work/analysis.json`、`analysis-3d.json`、`test-freeze.json`。

- DCT16 IST：20160 核向量、168 强制码流、23524 项检查、跨编译器和 sanitizer 通过。两个 Real 裁块没有净收益；2D 候选增加 22 字节且两个分数下降，默认完整包回退 exact。因此保留实现与候选证据，不进入本轮编码入口。见 `ist16-work/README.md`、`result.json`、`ist16.patch`。



## 联合质量比较方法



`audit_real.py` 校验旧原始结果哈希和新码流/重建/评分身份。参考使用 r7 冻结的 AOM 九点，其中 V1 93–95 内有四个直接点；补点不会改动旧 AOM 程序、输入或设置。



`joint_quality.py` 在同一个 AOM 参考码率上取得 V1 和 NEG，然后求候选同时达到两项分数所需的最小插值字节数。候选按 log(bytes) 的原始顺序建立分段 PCHIP/线性曲线，不按质量重排、不删掉回退点、不外推。每段取两项约束可行区间的交集，支持候选质量反转。



区间平均是 V1 93–95 的 401 点积分估计；最差值包含这些采样点及直接 AOM 锚点，并非连续区间最坏值证明。参考覆盖、无解交集、共享端点、候选质量反转、直接锚点不可达等检查已验证。数值求根容差不是允许画质下降的阈值。



`real-quality-summary.json` 保留所有原始点、直接实测配对、每项独立曲线和联合质量结果。`native-review.json`、`independent-native-review/review.json` 是首批快照的独立复核；复核使用另一套反求算法，4001 点与自适应积分和原结果一致，同时从解码像素重算 SSE。后续补点须重新汇总，不能把首批复核当成补点已经通过。



最终 `independent-native-review/review-final.json` 已复核 λ1.5 的九点和 QM12 组合的七点，覆盖本次最终汇总。独立算法逐段求两项约束的可行交集，保留 QM12 的 V1 回退；401 点结果复现，4001 点和自适应积分下结论不变。QM12 的自适应平均为 0.884294 / 0.880935×，λ1.5 为 0.896059 / 0.892124×（PCHIP / 线性）。这说明积分数值稳定，不代替更多内容和实际编码点的验证。



`plots-work/` 提供中文 PNG/SVG 曲线和可重跑脚本：原始双指标点、目标区放大、联合质量体积比。图中保留全部版本及退步点，未完成点不绘制，覆盖不足处留空。



本轮完整帧结论限定在 Real frame48；裁块与此前 2D/3D 数据不替代新版本跨内容完整帧验收。补密造成的基线估算变化与真正编码收益分开记录，不能把对 AOM 的全部优势归于本轮修改。



## 最终完整帧汇总



本轮完成 26 个新增 Real 完整帧编码，全部固定 AVM 解码 exact；另复用 6 个冻结旧点。下表是在同一 AOM 参考点同时满足 V1 与 NEG 后的区间平均体积比，不是把两个指标的独立平均拼接。



| 版本 | 实测点数 | V1 93–95 内点 / 两端包围 | 联合平均 PCHIP / 线性 | 最差已采样比率 PCHIP / 线性 |

|---|---:|---|---|---|

| r6 | 7 | 4 / 是 | 0.922369× / 0.919011× | 0.959890× / 0.943152× |

| lambda-125 | 7 | 5 / 是 | 0.913014× / 0.909868× | 0.957652× / 0.952499× |

| lambda-150 | 9 | 6 / 是 | 0.896059× / 0.892124× | 0.967582× / 0.963323× |

| qm12-lambda125 | 7 | 5 / 是 | 0.884268× / 0.880913× | 0.952871× / 0.935934× |

| qm-y8 | 2 | 0 / 是 | 1.000182× / 0.995167× | 1.030627× / 1.025046× |



以上仅证明一个开发帧的测量结果。最差值只涵盖已计算的网格与 AOM 直接锚点，不是连续全区间证明。旧 QM8 两点保留作对照，不能与密集候选同等验收。



### λ1.5 的全部直接 AOM 配对



每行选取已实测且 V1、NEG 都不低于对应 AOM 的最小文件，没有插值。



| AOM QP | M6 QP | AOM / M6 字节 | 体积比 | ΔV1 | ΔNEG |

|---:|---:|---|---:|---:|---:|

| 22 | 96 | 79796 / 70604 | 0.884806× | +0.077804 | +0.306751 |

| 23 | 98 | 73608 / 65635 | 0.891683× | +0.374736 | +0.405478 |

| 24 | 99 | 67488 / 60996 | 0.903805× | +0.020505 | +0.337734 |

| 25 | 101 | 59863 / 57375 | 0.958438× | +0.013176 | +0.629688 |



### 曲线与冻结证据



![联合质量体积比](plots-work/output/joint-quality-ratio.png)



其他原始点图与放大图见 `plots-work/output/`。`final-evidence.json` 核对本轮源码、程序、输入、解码像素、评分及归档身份。独立复核的首批与最终快照分别保留，不能混用不同点数的比率。

</details>


<details>
<summary>025 · README.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\size-r9-20261029\README.md`  
原文件 SHA-256：`dc61842c71c56a3f57b612287004292181f92349353bdff0218848e7d621e61c`

# M6 单帧压缩优化 r9



本轮目标：以 AOM AV1 为固定对照，在 V1 93–97 范围以主 V1 保持为首要条件，同时检查 NEG 并单列双指标均不退步的结果，继续向 0.85×、0.80× 体积推进。测试覆盖 2D、3D、实拍，分别测试 1080p 与现有 2160p 素材，按存储尺寸抽帧。目标是研究方向，不预先宣称能够达到。



1. 冻结素材、模型、程序身份；验证新评分入口能逐字节复现旧码流的解码与评分。

2. 先读论文与技术文档，在隔离源码中实现候选机制；验证关闭时基线 exact、开启后固定解码器 exact，并测量实际完整包成本。局部RD保留旧候选不保证后续因果路径或最终滤波后的整帧成本下降；各实验分别说明回退范围。

3. 裁块筛查后扩展完整帧与质量区间；所有退步、覆盖不足和未完成点保留，各内容和分辨率单独报告。



1080p 沿用旧版 V1 3H 与 NEG；2160p 使用 V1 1.5H 2160 与 4K NEG，原尺寸评分，不缩放。两组均使用 10-bit 4:2:0 单帧。93–97 表示每组自身模型的质量带，不能混合两个分辨率的绝对分数。[VMAF v1 官方模型说明](https://github.com/Netflix/vmaf/blob/master/resource/doc/models_v1.md)和[NEG/4K 说明](https://github.com/Netflix/vmaf/blob/master/resource/doc/models_v0.md)提供模型选择依据。



2160p 的 2D/3D 文件为 3840×2160，未标 transfer/primaries，前 64 个解码帧未发现 HDR 元数据；上游渲染/缩放链未知，不能认证为原生渲染 4K 或 SDR 母版。本轮无缩放、色调映射或色彩变换，使用 SDR 模型作固定研究比较并保留这一限制。实拍 FoodMarket 为 4096×2160、既有官方 QP15 SDR 预编码参考，不当作无损母版。详见 `corpus-work/manifest.json`。



技术依据优先采用原始文献；AVM 保留为固定解码正确性参考。量化搜索仅改变现有码流允许的系数，不引入新的解码语法。预测搜索保留原候选并加入变换域筛选候选；具体来源与试验保存在各工作目录。



旧 r6/r7/r8、主源码与旧证据保持冻结。并行测试是质量诊断，耗时不作为速度验收。完整帧主调度由 root 负责，子任务只运行单进程小图/裁块编码。



当前状态见 `STATE.md`；`audit_results.py` 生成按内容/分辨率分组的 `quality-summary.json`，保留全部原始失败及其独立补验链接。`full-candidate-comparison.json` 单独保存完整帧同QP对照。`plots/` 的原始点图与目标区域放大图来自同一个已保存的审计快照，不合并不同内容或模型。



源码保存在各独立实验目录，包括：`uhd-work`（尺寸与数组范围扩展）、`prediction-work`（SATD）、`quant-work`（相邻系数联合搜索）、`quant-perceptual-work`（一致活动权重）、`mpm-work`（因果邻居模式）、`ccso-scale-work`（偏移幅度）、`ist16-work`（二次变换16×16）、`txsplit64-work`（64×64编码块内四个32×32亮度变换，有限负结果）。目录存在或正确性通过不代表压缩收益，具体状态以该实验冻结清单和实际测量为准。



曲线验收要求：候选与AOM在93–97内各至少五点，且两侧均有实测包围；两种插值都必须满足V1与对应AOM NEG的联合约束。`eligible_band_comparison`将这一条件与单纯质量可行分开。平均比率是质量带上的几何平均，不代表每个质量点均达到该比率，也不代表其他帧或内容泛化。



按主V1固定的诊断另存`primary_v1`，同时给出所选码率处相对AOM的NEG平均变化与最差采样点变化；`eligible_primary_v1_comparison`只判断这一单约束曲线的密度与覆盖。它不替代`joint`双约束结果，也不把伴随NEG下降的体积节省写成双指标不退步。原评分协议与原始测量保持不变。



2026-10-30 完整帧阶段结果（`round-results.json`、`quality-summary.json`）：155 条 PASS 记录经独立审计，全部调度结束。六组 AOM 与下表配置各有 93–97 内至少五点及双侧包围；所有 native 重建与固定 AVM 解码逐字节一致。



下表为各开发帧列出的独立配置。比率均为相对 AOM 的质量带几何平均，两数字分别为 PCHIP/线性，不是置信区间；不能当作每个质量点的保证，也不是跨内容通用配置。



| 素材 | 配置 | 主 V1 保持 | V1、NEG 同时保持 |

|---|---|---:|---:|

| 2D-1080p-frame48 | r6 | 1.010414×/1.010455× | 1.030190×/1.027603× |

| 3D-1080p-frame48 | satd | 1.087955×/1.088189× | 1.089011×/1.088844× |

| Real-1080p-frame48 | lambda-150 | 0.877182×/0.880565× | 0.880928×/0.881838× |

| 2D-2160p-frame48 | lambda150-uhd | 1.030022×/1.033076× | 1.069609×/1.070596× |

| 3D-2160p-frame48 | lambda150-uhd | 1.014476×/1.016630× | 1.028779×/1.029540× |

| Real-2160p-frame48 | lambda150-uhd | 1.056007×/1.054073× | 1.056029×/1.054073× |



Real1080 的双约束均值约 0.881×，V1=95 的局部锚点约 0.790/0.793×，但 93 与 97 分别约 0.948/0.947× 和 0.925/0.938×；整带最差采样点约 0.967/0.963×。尚未达到整带 0.85× 或 0.80×。主 V1 单约束结果伴随的 NEG 平均和最差变化全部保存在 `round-results.json`，不能把伴随 NEG 退步算作双指标收益。



SATD 的完整 3D1080 七点使双约束均值从旧 λ150 的 1.111219/1.111706× 降至 1.089011/1.088844×，约改善 2%。候选源码与程序仍在 `prediction-work`，正式入口清空所有测试环境；尚无新速度验收。MPM 三点的平均 V1 下降，NEG 的平均符号随插值方法变化，停止扩展。2D UHD 的 λ100 三点在共有码率区间平均 V1 +0.069/+0.061、NEG −0.034/−0.029，未扩展完整曲线。



技术实验索引见 `experiment-index.json`。相邻系数联合搜索、活动加权 SSE、CCSO 幅度、IST16、受限 split64、TCQ 及静态 TCQ-rate 的有限负结果均保留。TCQ-rate 已知模式映射遗漏另见 `tcq-rate-review-work/errata-mode-mapping.json`；真实码流与评分证据仍有效，内部估计成本须带该限制。新 `tcq-prefix-work` 使用正式因果上下文接口修正已测组合的模式映射；独立定向夹具双平台439161检查、两小图san及三裁块 fixedAVM exact均通过。三个裁块的最终包J仍全部回退SQ，暂不扩大完整帧。3D单点同码率V1约+0.002585/−0.038061、NEG约−0.128178/−0.139614（PCHIP/线性），不称稳定收益；详见其 `report.json` 与 `same-rate-point-diagnostic.json`。



复核入口：`corpus-work/final-analysis-audit/final/report.json`。该报告绑定全部 155 条结果、分析脚本和 Python/NumPy/SciPy 版本，并独立核对新增曲线积分、极值、最小率及同 QP 差值。`plots/manifest.json` 绑定最终图像与同一审计摘要快照。

</details>


<details>
<summary>026 · STATE.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\Legacy\rounds\size-r9-20261029\STATE.md`  
原文件 SHA-256：`bddc1191185a08f31035a63543818f0f63440af4111feff8f1a5c3c9ac3ff3c2`

当前状态：2026-10-30，本轮完整帧扩展、候选曲线、TCQ正式上下文修订及独立验证已完成；0.85×/0.80×目标尚未达到。此前过程记录保存在 `STATE.before-20261030.md`。



- 目标：主 V1 93–97 保持，逐点检查 NEG；继续研究 0.85×/0.80× AOM 体积，尚未达标。旧 >16× 单帧速度目标本轮未验收。

- 完整帧：155 条 PASS，六组 AOM 与基线均满足五个带内点和双侧包围；无 RUNNING、无未解释失败。10 个旧清单身份失败已独立补验、6 个旧程序 UHD 尺寸不支持保留原记录。

- 定量结果见 README 表与 `round-results.json`，完整原点、双约束、主 V1 及 NEG 代价见 `quality-summary.json`。Real1080 双约束均值约0.881×；其余五组双约束均值仍高于1。SATD 3D1080 约比原 λ150 改善2%，仍约1.089× AOM。

- 图像已刷新，`plots/manifest.json` 与 155 点审计摘要快照一致；独立最终分析审查 `corpus-work/final-analysis-audit/final`。

- 新 TCQ-prefix 仅修改隔离源码的 `tcq_pass.c`，使用已有正式 writer 的因果 CDF/邻居、宽角映射与叶提交；global λ1，最多32位置/73成本调用，最终完整包J严格回退。`production-freeze.json` 绑定476源文件与off/诊断/生产三程序。两小图各候选/最终 fixedAVM exact；关闭对照八产物旧byteexact；生产与诊断最终一致且不导出测试候选。独立定向夹具双平台439161检查通过；GCC ASan/UBSan/LSan两小图四产物与MSVC exact。三类真实裁块的候选/最终均fixedAVM exact，raw SSE/D/J经独立复核；三个候选均被包J拒绝、final exact SQ。3D单点同码率V1的结论随插值变号，NEG下降，未建立稳定压缩收益。`report.json`、`validation-freeze.json`与独立审查绑定全部身份。

- 旧 TCQ-rate 模式映射缺陷与负结果保留独立勘误；不修改已冻结证据。其他有限负结果与边界见实验索引。

- 主工程与 r6/r7/r8 均只读，质量诊断并发耗时不作为速度证据。

</details>


<details>
<summary>027 · README.md</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\r13\publication-draft\README.md`  
原文件 SHA-256：`0da77ce9893aedb77d98a56e8b1e207d7e53aef23d3e05f2b460839018a4f99a`

# SVT-AV2 单帧研发版本

这是独立 AV2 编码器的 M6 帧内研发版本。当前优化范围为 **1920×1080、3840×2160 的 10-bit 4:2:0 单帧**。单帧搜索配置需要显式启用；版本号仍沿用原项目，不代表完成了视频、速度或正式发布验收。

当前目标是同质量体积达到 AOM AV1 的 0.90×，进一步向 0.88×、0.85×推进。尚未实现所有内容和质量区间达标。实拍 1080p 的既有开发帧已接近 0.88×；不能把它推广为动画、UHD 或其他素材的结果。

## 构建

需要 CMake、Ninja 和 C11 编译器。Windows 在 MSVC 开发环境运行，Linux 使用 GCC/Clang。

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSVT_AV2_STILL_PROFILE=ON -DSVT_AV2_WARNINGS_AS_ERRORS=ON
cmake --build build
```

程序位于 `build/bin/SvtAv2EncApp`，Windows 带 `.exe` 后缀。参考 AVM 解码器独立提供；有解码器时可以通过 `SVT_AV2_ORACLE_DIR` 配置相应验证。

`SVT_AV2_STILL_PROFILE` 默认关闭；启用后编译既有变换缓存、CfL、细分块、RDOQ、PC-Wiener 与内容保护配置。以下两个实验选项仍默认关闭，均要求先启用单帧配置：

- `SVT_AV2_EXPERIMENTAL_SATD_ANGLES=ON`：增加角度候选与完整帧候选包比较；保留了此前 3D 1080p 开发帧的最佳已测方案，会增加搜索成本。
- `SVT_AV2_EXPERIMENTAL_FINE16_PAIR=ON`：扩大部分细分块计划搜索。完整 2D 1080p 已出现 VMAF 回退，不应因同 QP 文件更小而默认采用。

两个实验选项的组合没有完整质量曲线验收。详细构建条件见 [STILL_PROFILE_BUILD.txt](publication/STILL_PROFILE_BUILD.txt)。

## 单帧编码与内容分流

标准库入口接受恰好一帧、逐行、`C420p10` 的 Y4M，原编码输入不缩放：

```sh
python tools/single_frame.py --input frame.y4m --output frame.obu --recon recon.yuv --encoder build/bin/SvtAv2EncApp --qp 100 --content-type animation
```

实拍可指定 `--content-type real`。自动 profile 对 1080p 二维动画使用 λ1.0，对 1080p 实拍与 UHD 使用 λ1.5；动画标签仍保留给内容保护。`--profile animation|photo` 允许显式覆盖 λ 配置。3D 动画不能仅凭二分类标签推断最优压缩配置，应使用对应已验证的 `photo` profile。

自动分类使用冻结的 CLIP ViT-B/32 导出，需另行安装 [分类依赖](tools/public-classifier/requirements.txt)，并提供本地官方 checkpoint 与 FFmpeg：

```sh
python tools/single_frame.py --input frame.y4m --output frame.obu --encoder build/bin/SvtAv2EncApp --qp 100 --ffmpeg /path/to/ffmpeg --checkpoint /path/to/ViT-B-32.pt
```

只有分类预览缩放到 640 宽，编码使用完整原始帧。既有冻结验证集为 20 个作品、60 帧，开发 36 帧、按作品隔离的独立终测 24 帧；终测为 **23/24（95.83%）**。公开导出只用两张开发预览做等价检查，没有重开终测或重新调阈值。这是该冻结集上的观察准确率，不是所有视频的保证。模型权重与私人素材不包含在仓库中。

## 质量控制与结果解读

内容类型参与编码 profile 选择；原生动画保护约束 PC-Wiener 候选的边缘梯度与平坦亮度回退。外部最终质量控制对动画检查边缘、平坦色块和色度，对实拍检查纹理与亮度。原生保护不覆盖全部最终滤波与外部指标，最终仍必须对固定参考解码后的帧验收，详见 [QUALITY_CONTROL.txt](tools/QUALITY_CONTROL.txt)。

主 VMAF V1 与 NEG 分别报告；默认体积比较同时满足相同 AOM V1/NEG 目标。93–97 完整区间要求双方各至少五个区间内实测点与双侧包围，分别用 PCHIP、分段线性复核，不外推。单个 QP、裁块收益或编码成功均不表示通过这些条件。

包装工具输出 JSON、日志及 SHA256；`ENCODE_COMPLETED_NOT_QUALITY_VERIFIED` 只代表编码完成。当前没有完成 >16× 提速验收。分类和本地误差筛查也不能替代固定解码与质量比较。

原项目历史保留在 Git 历史和既有 `docs/` 中。原 [LICENSE](LICENSE) 的授权状态保持不变；分类器上游组件的授权单独记录在其目录中。

</details>


<details>
<summary>028 · report.json</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\r13\rate-band-research\report.json`  
原文件 SHA-256：`fe16ac90b2a654a9e2eecf4fd987d6e23fe8dda9d329a385863fa0d0953eee22`

{

  "status": "READ_ONLY_COMPLETE_NO_ENCODE",

  "snapshot_utc": "2026-10-30T04:57:14.419569+00:00",

  "scope": "仅历史与规范/代码研究；没有实现、编码、重新解码或重新VMAF。历史fixedAVM状态来自冻结证据，本轮重哈希并重算9点D/J。",

  "sources": [

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\frozen.json",

      "sha256": "6bae5b86f6707eabdf6b6f1d50dc1225fed6ccf30e019dba0abe46f6c1cc2368"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\source-manifest.json",

      "sha256": "882500cb85969f33f74e01e49ec18e91aa12477111c31c4f0247630554e5ecbb"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\build.ps1",

      "sha256": "cc102f7d1a7e3e8cb8e2f2c3f1c262118e9cef3d0e4799949bd1a6b813c0828b"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\all-analysis.json",

      "sha256": "61d388823e0289bb45ceaad324879a3d612a4bd79e0b2eee36c68c81d8565c1a"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\first-results.json",

      "sha256": "dac1a7f60c7835874e92a4340650ebbee8f084969cb47bd001dec32fd1c0b722"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\rest-results.json",

      "sha256": "d0b2f8a03515d7b2048ccad168c496af815cc3222da6de5de03e5da0b8f50785"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\verification.json",

      "sha256": "38f78210cbf161e0f8b81d24acee9ebf79b481f9751942ac9e2115ed0df479a5"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\README.md",

      "sha256": "16586cdc76c0b0cda95d3008493702cafae97551ec7da9408ffe3341a55e7afe"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r9-20261029\\ccso-scale-work\\source.patch",

      "sha256": "01ff3769df4ff1fc48571988e1fa4bcf9264f470a3b4800164c1a99d53b4e10f"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r9-20261029\\ccso-scale-work\\quality-comparison.json",

      "sha256": "c2215f09b0598ac0abf24433fbd2232a3729443e1f0b5fba7f11ecacfca561bc"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r9-20261029\\txsplit64-review-work\\spec-text.txt",

      "sha256": "70d1327075eb472a8dd177f886ab604b958102c1ea4f96304492ae7fb63a4efb"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native\\deps-full\\AV2\\docs\\spec\\AV2_Spec_v1.0.0_20260528_38f28e7.pdf",

      "sha256": "e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r9-20261029\\bitrate-diagnostic-work\\origin.json",

      "sha256": "24a940fd224a2c7fc4f9bc66bdfb8086e28b8e6c200e0957cc6a822b9547a1a0"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\integration-work\\source\\Source\\Lib\\Encoder\\rd_frame.c",

      "sha256": "a25647802723c290b8273a467f3b6e8c880dfb2c7c4d669f84ad37f278e71111"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\integration-work\\source\\Source\\Lib\\Codec\\encoder.c",

      "sha256": "0c4ac26e15fae64865b82b2f2db7a73b72f00719328db45b5b87b6d2bbf086e6"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\integration-work\\source\\Source\\Lib\\Bitstream\\ccso64.c",

      "sha256": "a4c307d9b06973ae75e253794d08c4064ad8b167a3b41d9af3a1d161471bd637"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\integration-work\\source\\Source\\Lib\\Bitstream\\ccso64.h",

      "sha256": "f6f4da047eee81138017ce96218d5957fd3502da2f0fb880afe8d51327e126a5"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\integration-work\\source\\Source\\Lib\\Bitstream\\frame_header.c",

      "sha256": "7c5b4e765d809043cf584abd361c00767ead9faadb02a873bac1f259b99c078b"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\integration-work\\source\\Source\\Lib\\Bitstream\\keyframe.c",

      "sha256": "c27d28f36c89185ce316be3067c948d0476fe04fcea0116deaaf6ae621758aaf"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native\\deps-full\\AV2\\svt-av2\\artifacts\\m53-b\\diagnostic-decoder\\source\\av2\\common\\enums.h",

      "sha256": "4afd9d61b0c9fa4a68fb6dcb4237db740ea3c3835c2556d465dd3a33dbd13b99"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native\\deps-full\\AV2\\svt-av2\\artifacts\\m53-b\\diagnostic-decoder\\source\\av2\\common\\ccso.c",

      "sha256": "25189336376b67ff05240dc567f80ec5fdc0a77444f51ba622c20ff2b319229d"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native\\deps-full\\AV2\\svt-av2\\artifacts\\m53-b\\diagnostic-decoder\\source\\av2\\decoder\\decodeframe.c",

      "sha256": "02ba0263536acb55f0317647dfbeaa33d867f07a4583a28c1f1dc5aeb530a0db"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native\\deps-full\\AV2\\svt-av2\\artifacts\\m53-b\\diagnostic-decoder\\source\\av2\\encoder\\rd.c",

      "sha256": "4bb506d83e7fc6c72cc2c5bb45f4476d23303e8246f2c1d5aec6cf8e34ef3c2f"

    }

  ],

  "optional_unresolved_paths": [

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261030\\STATE.md"

  ],

  "lambda": {

    "identity_checks": [

      {

        "kind": "200",

        "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\lambda-200.exe",

        "sha256": "ea5f8b279456e1ccfb9c137a72e9005dc5ed02644d924c7ab99a6020aed9409d"

      },

      {

        "kind": "100",

        "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r6-20261027\\context-pcw-work\\combo.exe",

        "sha256": "0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d"

      },

      {

        "kind": "150",

        "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\lambda-150.exe",

        "sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

      },

      {

        "kind": "decoder",

        "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\intra-r3-20261024\\integrated-r4c-2D-curves\\avmdec.exe",

        "sha256": "110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac"

      },

      {

        "kind": "scorer",

        "path": "<LOCAL_PRIVATE>\\ffmpeg-native\\deps-full\\bin\\bin\\vmaf.exe",

        "sha256": "1b1f9478066c1cbf0dcaba213e6a3ea42b01f15547d87b16a58890c1c5e78ec5"

      },

      {

        "kind": "v1",

        "path": "<LOCAL_PRIVATE>/ffmpeg-native/deps-full/vmaf/model/vmaf_v1.0.16/vmaf_v1.0.16_3d0h.json",

        "sha256": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f"

      },

      {

        "kind": "neg",

        "path": "<LOCAL_PRIVATE>/ffmpeg-native/deps-full/vmaf/model/vmaf_v0.6.1neg.json",

        "sha256": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

      }

    ],

    "points": [

      {

        "case": "RealA",

        "q": 90,

        "bytes": 1353,

        "v1": 81.634929,

        "neg": 84.438188,

        "lambda_q8": 10890,

        "final_distortion": 838223,

        "j_at_own_lambda": 1298666,

        "encode_seconds": 35.80383539199829,

        "fixed_avm_exact": true,

        "input": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r7-20261028\\inputs\\Real-256-256-256.y4m",

          "sha256": "a35034b1256cb086ddb7d73f5b0240b084404bfb087aeb03de1c1973f02df762"

        },

        "stream": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealA-q90-200\\stream.obu",

          "sha256": "033e6f39e9319211197b9b00004f8669d9e5b8de0fdcf9dcbc188403fe68e4b6"

        },

        "recon": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealA-q90-200\\recon.yuv",

          "sha256": "33a0b4b8a053ff2f7b773509ac43436c4f7b74909c714c29d2765a17030b87da"

        },

        "result": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealA-q90-200\\result.json",

          "sha256": "056553b9c52d908cafafad810c1c6c6eecae6887b5934369e64dc8e267ff549b"

        }

      },

      {

        "case": "RealA",

        "q": 100,

        "bytes": 819,

        "v1": 77.071425,

        "neg": 80.804122,

        "lambda_q8": 19360,

        "final_distortion": 1020645,

        "j_at_own_lambda": 1516140,

        "encode_seconds": 25.996062517166138,

        "fixed_avm_exact": true,

        "input": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r7-20261028\\inputs\\Real-256-256-256.y4m",

          "sha256": "a35034b1256cb086ddb7d73f5b0240b084404bfb087aeb03de1c1973f02df762"

        },

        "stream": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealA-q100-200\\stream.obu",

          "sha256": "feb2acdecf352e91d60e30c2695b586c28d55cf72cfcfc4bd339e344a3f68b8c"

        },

        "recon": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealA-q100-200\\recon.yuv",

          "sha256": "a51e35c98ab289d388b0a356fdc4db8619f8b7407e15ccaa9596dd88783b2d85"

        },

        "result": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealA-q100-200\\result.json",

          "sha256": "15cb9691cf802530af0fd4aef3f0804d2108c9b7b7f01d023ac916dc7b03ef94"

        }

      },

      {

        "case": "RealA",

        "q": 110,

        "bytes": 561,

        "v1": 70.927079,

        "neg": 75.730881,

        "lambda_q8": 34810,

        "final_distortion": 1157675,

        "j_at_own_lambda": 1767938,

        "encode_seconds": 19.34874963760376,

        "fixed_avm_exact": true,

        "input": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r7-20261028\\inputs\\Real-256-256-256.y4m",

          "sha256": "a35034b1256cb086ddb7d73f5b0240b084404bfb087aeb03de1c1973f02df762"

        },

        "stream": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealA-q110-200\\stream.obu",

          "sha256": "ccb5a43e706622f25b6b48696eac32613a521779a57a713f6ace4ca60e60ef8d"

        },

        "recon": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealA-q110-200\\recon.yuv",

          "sha256": "6370bd8f1d413174c0180c357f64fa0236bd22cf4bda69074829005cbb99fde3"

        },

        "result": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealA-q110-200\\result.json",

          "sha256": "3e91892c8e35e11181bbfb1ca5ddc574e13a245a9b086054a3e7cdd99e1c7452"

        }

      },

      {

        "case": "RealB",

        "q": 90,

        "bytes": 10146,

        "v1": 97.501962,

        "neg": 94.221931,

        "lambda_q8": 10890,

        "final_distortion": 916211,

        "j_at_own_lambda": 4369022,

        "encode_seconds": 152.54293656349182,

        "fixed_avm_exact": true,

        "input": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r7-20261028\\inputs\\Real-256-832-384.y4m",

          "sha256": "17b162f39488c384b4848824bd2b0429e835333dc8a60b30b834fd0274a15dba"

        },

        "stream": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealB-q90-200\\stream.obu",

          "sha256": "c89dec87c82adbe243330172dcfe4a5da6da798df6dc10756387d12bb8e37c26"

        },

        "recon": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealB-q90-200\\recon.yuv",

          "sha256": "d967b5a9bd5259a911d952966a6f88b74757fed0a4d51cbe89b8585c37eac572"

        },

        "result": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealB-q90-200\\result.json",

          "sha256": "cece3ca7b2e14b5f54721f8d10017af71e977219e3a158ac77c0661fff4678c7"

        }

      },

      {

        "case": "RealB",

        "q": 100,

        "bytes": 8481,

        "v1": 96.526441,

        "neg": 93.022677,

        "lambda_q8": 19360,

        "final_distortion": 1502123,

        "j_at_own_lambda": 6633128,

        "encode_seconds": 144.04837894439697,

        "fixed_avm_exact": true,

        "input": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r7-20261028\\inputs\\Real-256-832-384.y4m",

          "sha256": "17b162f39488c384b4848824bd2b0429e835333dc8a60b30b834fd0274a15dba"

        },

        "stream": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealB-q100-200\\stream.obu",

          "sha256": "759db4f01796ae54a762e286a2536c64dca3b310a36a383cb0bb28dafd5ac593"

        },

        "recon": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealB-q100-200\\recon.yuv",

          "sha256": "eb63745aaaf42d189b3bed7dd739033afdb1062ee1999c7e743e860749c70229"

        },

        "result": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealB-q100-200\\result.json",

          "sha256": "62e6e3b8c9f2f9b61c5d1d03bba862e8c557bbf83c3421b7820158a5dd791bec"

        }

      },

      {

        "case": "RealB",

        "q": 110,

        "bytes": 6789,

        "v1": 94.965504,

        "neg": 91.201623,

        "lambda_q8": 34810,

        "final_distortion": 2503575,

        "j_at_own_lambda": 9888734,

        "encode_seconds": 130.2475941181183,

        "fixed_avm_exact": true,

        "input": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r7-20261028\\inputs\\Real-256-832-384.y4m",

          "sha256": "17b162f39488c384b4848824bd2b0429e835333dc8a60b30b834fd0274a15dba"

        },

        "stream": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealB-q110-200\\stream.obu",

          "sha256": "7b40c5f1cd5f142e7b21276475a6418441fa5c36821127b3d5c1897d53edf3f8"

        },

        "recon": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealB-q110-200\\recon.yuv",

          "sha256": "485362dcb521bdeb4a693027713bd724bdfb8d3f9f5d3446d2cb0183f04fe4b1"

        },

        "result": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\RealB-q110-200\\result.json",

          "sha256": "7896e8e73ea5f57bbd2ae7f44d755bdfa7f790bc882e4e162a397347bbe48493"

        }

      },

      {

        "case": "3D",

        "q": 90,

        "bytes": 5020,

        "v1": 96.858483,

        "neg": 94.45999,

        "lambda_q8": 10890,

        "final_distortion": 830901,

        "j_at_own_lambda": 2539270,

        "encode_seconds": 80.36305451393127,

        "fixed_avm_exact": true,

        "input": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r7-20261028\\inputs\\3D-256-256-256.y4m",

          "sha256": "eecfda47d486f4128386a27fc7f8b9d8190d6e5587c58b9ad87da787965487b9"

        },

        "stream": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\3D-q90-200\\stream.obu",

          "sha256": "967b6d8f567a11b71f489c14396e3c9176fc22517ca0ff0238616ebadead4f81"

        },

        "recon": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\3D-q90-200\\recon.yuv",

          "sha256": "d41ca0f4d90de859e996db295693bf5dd1da5bb11c46b9a097162cc179084faa"

        },

        "result": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\3D-q90-200\\result.json",

          "sha256": "c6a7c1ec0db0766185155626210f75a43b0bbb993375bbdd9d5e71cb77775b10"

        }

      },

      {

        "case": "3D",

        "q": 100,

        "bytes": 4062,

        "v1": 95.816453,

        "neg": 93.254881,

        "lambda_q8": 19360,

        "final_distortion": 1146175,

        "j_at_own_lambda": 3603685,

        "encode_seconds": 67.46294689178467,

        "fixed_avm_exact": true,

        "input": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r7-20261028\\inputs\\3D-256-256-256.y4m",

          "sha256": "eecfda47d486f4128386a27fc7f8b9d8190d6e5587c58b9ad87da787965487b9"

        },

        "stream": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\3D-q100-200\\stream.obu",

          "sha256": "a4783c1e328151f9d0fc898666ba0b30b8a428ecc61f196902e4c63bf1a1280f"

        },

        "recon": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\3D-q100-200\\recon.yuv",

          "sha256": "acd919f722b0936ba377bc7f7a727408db4d83005bb1a26cbbf1ba6e8fa7c6e7"

        },

        "result": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\3D-q100-200\\result.json",

          "sha256": "1ce3864328f268a2f94985c75791829752f6b6c2040896a3610eb2e2ebd21bc9"

        }

      },

      {

        "case": "3D",

        "q": 110,

        "bytes": 3229,

        "v1": 94.594315,

        "neg": 91.836574,

        "lambda_q8": 34810,

        "final_distortion": 1600140,

        "j_at_own_lambda": 5112687,

        "encode_seconds": 63.102683782577515,

        "fixed_avm_exact": true,

        "input": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r7-20261028\\inputs\\3D-256-256-256.y4m",

          "sha256": "eecfda47d486f4128386a27fc7f8b9d8190d6e5587c58b9ad87da787965487b9"

        },

        "stream": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\3D-q110-200\\stream.obu",

          "sha256": "38ea3328f516bea83c98c9ac51ff4ad0e28fee988b18ae3aea1bfeafb386e895"

        },

        "recon": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\3D-q110-200\\recon.yuv",

          "sha256": "e27c2b5bf227f13d2fff85cd4a141854159b7ea37b71f453bed1ae8896f84cfb"

        },

        "result": {

          "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r8-20261028\\lambda-work\\3D-q110-200\\result.json",

          "sha256": "208d225a0f3b27fb33b8d6c1e1dfcb39f7be2bd747e4aa76036b597aa0a0317d"

        }

      }

    ],

    "historical_comparisons": [

      {

        "case": "3D",

        "arm": "200",

        "reference": "100",

        "common_all_four_bytes": [

          3519,

          5020

        ],

        "same_rate": {

          "v1": {

            "mean_delta_pchip": -0.09756170714404878,

            "mean_delta_linear": -0.11415729596656092,

            "quality_monotone": true,

            "equal_quality_overlap": [

              95.287864,

              96.858483

            ],

            "equal_quality_byte_ratio_pchip": 1.0191600121926072,

            "equal_quality_byte_ratio_linear": 1.0224536653947198

          },

          "neg": {

            "mean_delta_pchip": 0.006675876681084794,

            "mean_delta_linear": 0.007370204380396157,

            "quality_monotone": true,

            "equal_quality_overlap": [

              92.467262,

              94.45999

            ],

            "equal_quality_byte_ratio_pchip": 0.9980361489940863,

            "equal_quality_byte_ratio_linear": 0.9975996979974159

          }

        }

      },

      {

        "case": "RealA",

        "arm": "200",

        "reference": "100",

        "common_all_four_bytes": [

          705,

          1353

        ],

        "same_rate": {

          "v1": {

            "mean_delta_pchip": 0.21458119614636995,

            "mean_delta_linear": 0.08747899529931266,

            "quality_monotone": true,

            "equal_quality_overlap": [

              74.573753,

              81.634929

            ],

            "equal_quality_byte_ratio_pchip": 0.9871004609311496,

            "equal_quality_byte_ratio_linear": 0.9895217301066112

          },

          "neg": {

            "mean_delta_pchip": 0.512348133604693,

            "mean_delta_linear": 0.4134682903466625,

            "quality_monotone": true,

            "equal_quality_overlap": [

              78.727341,

              84.438188

            ],

            "equal_quality_byte_ratio_pchip": 0.9545811500901299,

            "equal_quality_byte_ratio_linear": 0.953320342135647

          }

        }

      },

      {

        "case": "RealB",

        "arm": "200",

        "reference": "100",

        "common_all_four_bytes": [

          7452,

          10146

        ],

        "same_rate": {

          "v1": {

            "mean_delta_pchip": -0.11900439063153277,

            "mean_delta_linear": -0.08135955553534224,

            "quality_monotone": true,

            "equal_quality_overlap": [

              95.664083,

              97.501962

            ],

            "equal_quality_byte_ratio_pchip": 1.0265258348753117,

            "equal_quality_byte_ratio_linear": 1.01365681710141

          },

          "neg": {

            "mean_delta_pchip": 0.0004732035319806184,

            "mean_delta_linear": 0.045023911512191504,

            "quality_monotone": true,

            "equal_quality_overlap": [

              91.911181,

              94.221931

            ],

            "equal_quality_byte_ratio_pchip": 1.0036937805085437,

            "equal_quality_byte_ratio_linear": 0.9925829850342727

          }

        }

      }

    ],

    "comparison_warning": "旧三点曲线相对λ1，分别V1/NEG，不是联合AOM比率；本轮只引用原分析，未重新插值。",

    "search_scope": "<LOCAL_PRIVATE>/ffmpeg-native-m6-tests 下≤2MiB json/py/ps1/md，排除source/*-source/build；<LOCAL_AGENTDOCK>-AV2-r13限定result/command/profile/ps1，搜lambda200/250及NUM8/10；有界文本检索不是穷尽所有二进制构建。",

    "finding": "找到r8 λ2.0三素材×3QP；未找到原2D/UHD λ>1.5实际结果，也未找到λ2.5。RealA V1仅70.9–81.6；RealB/3D高域V1平均负，NEG接近零或插值敏感，不能推广。λ075/050与局部RDOQ λ/16、λ/4不属于高λ。",

    "normalization": "420 D=round(4*(SSE_Y+SSE_U+SSE_V)/2^(2*(bd-8)))；444为4Y+U+V。J=D+round(lambda_q8*rate_q12/2^20)，实际packet rate_q12=bytes*32768。",

    "m6_ai": "q为Y AC QTX；rdmult=(q*q*80/32+32)>>6，10bit再(rdmult+8)>>4，至少1；baseλq8=(rdmult+1)>>1；编译倍率四舍五入。",

    "avm_difference": "固定AVM rd.c:657–680同AI80/32及位深归一形式，但q取av2_dc_quant_QTX(含base_y_dc_delta_q)，M6取AC；不能宣称完全相同λ或直接拿宏值比较两个encoder。",

    "testability": "如另授权，mode1(avm-ai)+NUM8/10 DEN4可隔离λ2/2.5，QP不改、420 D不改；integration mode2固定6/4，不能靠NUM改变。当前无质量理论下界可保证改善。保留λ1/1.5曲线，要求同素材/同V1+NEG覆盖，不以同QP省字节判优。"

  },

  "ccso": {

    "normative": "固定v1 spec p134–135/344，CCSO_BAND_NUM=64；BOonly下log2读3bit且≤6。固定AVM decodeframe.c:3351–3389与common/enums.h:148一致。",

    "wire": [

      {

        "bands": 4,

        "log2": 2,

        "bo_scale_band_6bits": "100010",

        "offset_bits_per_enabled_plane_min": 4,

        "offset_bits_per_enabled_plane_max": 12,

        "table_bytes_max_domain": 417792,

        "ternary_luts_per_plane": 81

      },

      {

        "bands": 8,

        "log2": 3,

        "bo_scale_band_6bits": "100011",

        "offset_bits_per_enabled_plane_min": 8,

        "offset_bits_per_enabled_plane_max": 24,

        "table_bytes_max_domain": 835584,

        "ternary_luts_per_plane": 6561

      },

      {

        "bands": 16,

        "log2": 4,

        "bo_scale_band_6bits": "100100",

        "offset_bits_per_enabled_plane_min": 16,

        "offset_bits_per_enabled_plane_max": 48,

        "table_bytes_max_domain": 1671168,

        "ternary_luts_per_plane": 43046721

      },

      {

        "bands": 32,

        "log2": 5,

        "bo_scale_band_6bits": "100101",

        "offset_bits_per_enabled_plane_min": 32,

        "offset_bits_per_enabled_plane_max": 96,

        "table_bytes_max_domain": 3342336,

        "ternary_luts_per_plane": 1853020188851841

      },

      {

        "bands": 64,

        "log2": 6,

        "bo_scale_band_6bits": "100110",

        "offset_bits_per_enabled_plane_min": 64,

        "offset_bits_per_enabled_plane_max": 192,

        "table_bytes_max_domain": 6684672,

        "ternary_luts_per_plane": 3433683820292512484657849089281

      }

    ],

    "wire_notes": "保持Yoff、BOonly1、scale0和offset0/+1/-1，对应tu(7)码0/10/110。6bit BO/scale/log2长度固定；更多band增加LUT而非flag上下文。旧4band恒量34，新8/16/32/64为35/36/37/38。",

    "current_contract": "ccso64.h offset[2][4]、ccso64.c validate/apply的4及bd-2、frame_header.c固定34/4、encoder.c table[...,4,3]/81枚举/lut uint32均需同步。CDF[2][4][6]的4是context；headers[4]是U/V子集，两者不扩。",

    "uhd": "现64×34=2176单元支持4096×2160，3840×2160为2040；增加bands不改unit旗标/边界。最大64band表6,684,672B，非像素recon副本。动态容量与size_t算术、header_size 256B缓冲/packet+256上界仍需实际边界验证，不能仅据表大小宣称已测试。",

    "minimal_proposal": "只读建议先固定8band，±1不变；保存原4band及NONE。由旧4band每项复制到两个相邻band可生成同重建8band种子（信令更大）；另用按band聚合的clipped三偏移SSE提出有限LUT，逐unit因果flag/CDF试算，严格tie保旧。不要穷举3^B；坐标改动会改变后续flag/CDF，必须完整重放而非局部率相加。最终全部滤波后的实际packet与D/J对比旧完整结果，候选被拒仍保存。这里只评估，未实施。",

    "scale_history": "r9 ccso-scale仅scale_idx0..3使±1→±1..4，仍4band/原bd-2；不能当作多band负结果。其CCSO阶段包J回退与后续PCW可能排序不同，应避免把局部门控冒充最终门控。",

    "input_and_upper_bound": "固定AVM decodeframe先deblock，9947–9962复制ext_rec_y；之后CDEF、CCSO、restoration。当前M6 CDEF关闭，CCSO使用deblocked Y的偶数坐标分类，PCW后export Y不能代替。缺已绑定preCCSO快照，本轮不做数值上界。若以后获快照，可sum(unit,plane,band)min(SSE[-1],SSE[0],SSE[1])得允许每unit独立offset且不收费的乐观D下界，非合法全局LUT收益，更非VMAF保证。",

    "required_checks": "off=父exact；4band=父exact；每band边界/±1clip/8和10bit/zero/all/mixedflag；4096×8、8×2160、4096×2160partial末单元；固定AVM exact、CDF回放、最终packet+recon回退及PCW交叉；san与跨平台。"

  }

}

</details>


<details>
<summary>029 · report.json</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\r13\sdp-research\report.json`  
原文件 SHA-256：`77b9c9d49d3ad419f768f68d0e03d3d461db8f0ec53669e25c2947d094498806`

{

  "status": "READ_ONLY_RESEARCH_COMPLETE",

  "recommendation": "表示能力针对性成立，但不是小补丁；先完成chromaRD，再单独立项SDP有限UV树特例。",

  "coverage": {

    "packet_bindings": {

      "old": {

        "result_sha256": "61c57e69b7f8493168b21aaf5b6661602e152c7e4a597dbcfd3a567eaf5e1ed4",

        "stream_sha256": "52c510b7e4751ff4b11c1bf15739df9a7c0fe0831bda8e1355dc268e00b9fdab",

        "status": "PASS_NEW_SINGLE_FRAME_FIXED_DECODE_V1_NEG",

        "v1": 95.786448,

        "neg": 92.575408

      },

      "new": {

        "result_sha256": "9c95724f20b384e7dd839f79aeef7f819a3d903f3460645772c43748c6dbdf5e",

        "stream_sha256": "7a4979b6fb2a0552b348ce65f7a5be12cf8ac7403843fe2c94de0fcccb6816fb",

        "status": "PASS_NEW_SINGLE_FRAME_FIXED_DECODE_V1_NEG",

        "v1": 95.259939,

        "neg": 92.611835

      }

    },

    "log_counts": {

      "old": {

        "FINE16_SB": 480

      },

      "new": {

        "FINE16_SB": 480,

        "FINE16_PAIR_SB": 480

      }

    },

    "changed_full_sb": 271,

    "final_pair_plan_count": 68

  },

  "y8_uv4": {

    "old_area": 245248,

    "new_area": 266240,

    "net_area": 20992,

    "old_percent": 11.82716049382716,

    "new_percent": 12.839506172839506,

    "added_area": 89600,

    "removed_area": 68608

  },

  "direct_pair_y8_area_delta": 12544,

  "critical_limits": [

    "现有事件不含最终CFL/alpha/局部speed_chroma，无法证明UV4边界因果。",

    "enable_sdp为序列开关，intra64节点统一两树，不支持只给选中叶开旗标。",

    "独立CDF/size上下文、UV owner与Y映射、因果prefix/replay/RDOQ、滤波边界均需接线。",

    "有限特例可保留Y树并在亮度16域截断更深UV树，但仍须根推断与CFL合法性、partial处理。",

    "权重改变决策目标；SDP扩展表示域；两者收益必须各自真实码流验证。"

  ],

  "references": [

    {

      "url": "https://av2.aomedia.org/v1.0.0/index.html",

      "sections": [

        "5.4.2",

        "6.4.2",

        "5.20.3.1",

        "5.20.3.2",

        "5.20.4.1",

        "5.20.5.3",

        "5.20.5.6",

        "8.3.2",

        "7.17"

      ]

    },

    {

      "url": "https://research.google/pubs/overview-of-the-block-partitioning-framework-in-av2/",

      "scope": "作者机构官方摘要；全文IEEE418，未使用工具收益数字。"

    },

    {

      "url": "https://ieeexplore.ieee.org/abstract/document/11630267",

      "access": "418"

    }

  ],

  "evidence": [

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\sdp-research\\README.txt",

      "sha256": "2a7bae89f0e2091e10efa1452ea28d5f2c7eead9dfaf020d137365836dd7f50b",

      "bytes": 8080

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\sdp-research\\coverage.py",

      "sha256": "98dc9b293aece027e647e25e83276cc9ef79ce52b01d3e48f55d86867ec8e56a",

      "bytes": 6871

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\sdp-research\\coverage.json",

      "sha256": "d25f97b2fdcb5770d20db9709d7567b677e28d43760c587d7618cb5ce3b11a8a",

      "bytes": 158184

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\sdp-research\\freeze.py",

      "sha256": "0d88a02bd7840f842054abb72b11b5bc9551625c410f4174dd0d64d6218b1479",

      "bytes": 2807

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native\\deps-full\\AV2\\docs\\spec\\AV2_Spec_v1.0.0_20260528_38f28e7.pdf",

      "sha256": "e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f",

      "bytes": 3904791

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r9-20261029\\transform-research-next\\spec-extracted.txt",

      "sha256": "e3e63bdea33811bc52a3be652323e19b86924a440fe1d9b9da97a0ac26b68c27",

      "bytes": 3068351

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\fine16-pair-work\\source\\Source\\Lib\\Bitstream\\sequence_header.c",

      "sha256": "e94aa93126fb9f2851daca8b7b735ab3301330ce4a005514fffc674e4e9c0c01",

      "bytes": 31163

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\fine16-pair-work\\source\\Source\\Lib\\Bitstream\\tile_writer.c",

      "sha256": "f7150181939ac380acad59220115e457bdfabc8324fc2a7276ee405716ed35d9",

      "bytes": 49633

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\fine16-pair-work\\source\\Source\\Lib\\Encoder\\rd_frame.c",

      "sha256": "8d612e95765cf9320d01158c46821c9487c152f5f0e422a6b77574fb266fed41",

      "bytes": 187486

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\fine16-pair-work\\source\\Source\\Lib\\Encoder\\fine16_pair_search.inc",

      "sha256": "b4aad782a8847333abbb0dd0c55f90366f5e1b1b3e45675698f06fe367de5f72",

      "bytes": 8010

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\fine16-pair-work\\source\\Source\\Lib\\Common\\block_geometry.c",

      "sha256": "8595c0aae1d9cdefae6a05ad19f94204caa0cafcd5c4b50579307ae9be43b2dc",

      "bytes": 42235

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\fine16-pair-work\\source\\Source\\Lib\\Encoder\\deblock_luma.c",

      "sha256": "04a546ff7815c82694c94c0388631df0340016e82dbdbc4bc435eadaeeb66bff",

      "bytes": 16921

    }

  ],

  "encoder_source_modified": false,

  "encodes": 0,

  "decodes": 0,

  "compiles": 0,

  "vmaf_runs": 0,

  "pixel_files_read": 0

}

</details>


<details>
<summary>030 · report.json</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\r13\content-review\report.json`  
原文件 SHA-256：`1e877d4b59a7a3a96eda8683815519f6e3659e8b1fe948f884d6a6e0d301b68f`

{

  "status": "只读审查完成；未修改源码、未编码、未解码、未重评分；候选机制等待主代理选定",

  "scope": "动画内容分类与 PCW 否决保护、离线局部 QC、r10 负结果；不重做 SATD 或活动权重",

  "conclusion": "现有自动分类接入合法，但已测 2D1080 Q100 的保护完全未触发；不能将此记录作为内容保护改善质量或压缩率的证据。",

  "identity": [

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\README.md",

      "sha256": "b57fdf25011e912f9152300a0a311d5c297622826cbdcd1c6473cac3de44e462"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\STATE.md",

      "sha256": "d73585d1c54f9d6e7624e28a36508047ae850c28c4430feb1c7778ab4865cfc4"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\quality-policy.json",

      "sha256": "50e2fd487e95fb67a847282ef462cad25856504f2b54b1ee49bb61453d4f892e"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\quality_control.py",

      "sha256": "5798b4d11db53dd52d13894f0b5fa67ba135fb3b96833359d29c0cec43d9491f"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\score_content_frame.py",

      "sha256": "72014fe481447b7e95e3a36a76f304f4edd0dedd67c3b79874a94f450be1072c"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\initial-local-quality-review.json",

      "sha256": "0c3386e1d77c5adb97cec3fe54ed7bb1097e133a432e1a8c734f7efa16d4045b"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\content-guard-work\\controls-results.json",

      "sha256": "ecf15249da04614ab92a7cbf30d48b1dbed193003a55b90e69ca583e9bf581f6"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\content-guard-work\\source\\Source\\Lib\\Encoder\\content_filter_guard.c",

      "sha256": "ccc67fd1ce7b08a664b344f1b1ef38fc4f86c922d8c507b3ba4a6bef9bcbbcd4"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\content-guard-work\\source\\Source\\Lib\\Codec\\encoder.c",

      "sha256": "78afe260af776680ff9ea0b2cf29f7bb348964f3fe2e746115d51c52c9fec322"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\auto-guard-2d1080-q100\\score\\result.json",

      "sha256": "7d0cad3e0c46dec3943f8342b8a409f19fa12e603a29e9caadc1e2b49780751f"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\auto-guard-2d1080-q100\\score\\encode.log",

      "sha256": "e640e12a0b85ac51fa8503b09ea1ffd794c2f3d2cca6ed73ae914c038b6db8e1"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r9-20261029\\reused\\2D-1080p-frame48\\r6-q100\\result.json",

      "sha256": "721a69fbc5f42fe57c9d47cb342d1f7a75f92a9fed59a6c9da5d936c83715a75"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r10-20261029\\quality-review.json",

      "sha256": "918161b9eedc615318291f0af30c18d371725ca0a3849bc03ada7408ee07f632"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r10-20261029\\full\\2D-1080p-edge-q100\\result.json",

      "sha256": "fb9c67866364c77f36ee75cb85f0a2e94dec588100ad106679c08dc6e03dfb1e"

    },

    {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r10-20261029\\full\\2D-2160p-edge-q104\\result.json",

      "sha256": "4b59405dbe1417d6d90a5ee602c3a107edbd33d8a94f90aa1409ef8422a02ced"

    }

  ],

  "actual_output_audit": {

    "guard": {

      "bytes": 38154,

      "v1": 95.786448,

      "neg": 92.575408,

      "stream_sha256": "52c510b7e4751ff4b11c1bf15739df9a7c0fe0831bda8e1355dc268e00b9fdab",

      "decoded_sha256": "5505abf1d574532406389289aadf71585e2cb14ec3b93952615a8a67f98d624d"

    },

    "baseline": {

      "bytes": 38154,

      "v1": 95.786448,

      "neg": 92.575408,

      "stream_sha256": "52c510b7e4751ff4b11c1bf15739df9a7c0fe0831bda8e1355dc268e00b9fdab",

      "decoded_sha256": "5505abf1d574532406389289aadf71585e2cb14ec3b93952615a8a67f98d624d"

    },

    "byte_comparison": {

      "stream.obu": {

        "guard_sha256": "52c510b7e4751ff4b11c1bf15739df9a7c0fe0831bda8e1355dc268e00b9fdab",

        "baseline_sha256": "52c510b7e4751ff4b11c1bf15739df9a7c0fe0831bda8e1355dc268e00b9fdab",

        "byte_exact": true

      },

      "decoded.yuv": {

        "guard_sha256": "5505abf1d574532406389289aadf71585e2cb14ec3b93952615a8a67f98d624d",

        "baseline_sha256": "5505abf1d574532406389289aadf71585e2cb14ec3b93952615a8a67f98d624d",

        "byte_exact": true

      }

    },

    "guard_calls": 25,

    "unique_guard_lines": [

      "CONTENT_GUARD type=animation active=1 considered=31 vetoes=0 edge_samples=110401 flat_samples=785163 forced=-1 accepted=1"

    ],

    "deblock_trials": [

      "DEBLOCK_TRIAL index=0 mode=0 delta=0 delta_h=0 edges=0 changed=0 bytes=38150 J=26426937",

      "DEBLOCK_TRIAL index=1 mode=1 delta=0 delta_h=0 edges=28082 changed=136132 bytes=38150 J=26314837",

      "DEBLOCK_TRIAL index=2 mode=1 delta=-1 delta_h=0 edges=28082 changed=118854 bytes=38149 J=26334807",

      "DEBLOCK_TRIAL index=3 mode=1 delta=1 delta_h=0 edges=28082 changed=141442 bytes=38151 J=26308969",

      "DEBLOCK_TRIAL index=4 mode=1 delta=-2 delta_h=0 edges=28082 changed=83834 bytes=38148 J=26355886",

      "DEBLOCK_TRIAL index=5 mode=2 delta=0 delta_h=0 edges=25424 changed=133378 bytes=38152 J=26297142",

      "DEBLOCK_TRIAL index=6 mode=2 delta=0 delta_h=-1 edges=25424 changed=116345 bytes=38154 J=26322333",

      "DEBLOCK_TRIAL index=7 mode=2 delta=0 delta_h=1 edges=25424 changed=138205 bytes=38153 J=26287786",

      "DEBLOCK_TRIAL index=8 mode=2 delta=0 delta_h=-2 edges=25424 changed=79590 bytes=38153 J=26349718",

      "DEBLOCK_TRIAL index=9 mode=3 delta=0 delta_h=0 edges=53506 changed=251457 bytes=38150 J=26189630",

      "DEBLOCK_TRIAL index=10 mode=3 delta=0 delta_h=-1 edges=53506 changed=236835 bytes=38151 J=26214117",

      "DEBLOCK_TRIAL index=11 mode=3 delta=0 delta_h=1 edges=53506 changed=255618 bytes=38154 J=26180567",

      "DEBLOCK_TRIAL index=12 mode=3 delta=0 delta_h=-2 edges=53506 changed=205094 bytes=38151 J=26239691",

      "DEBLOCK_TRIAL index=13 mode=3 delta=-1 delta_h=0 edges=53506 changed=236205 bytes=38154 J=26209003",

      "DEBLOCK_TRIAL index=14 mode=3 delta=-1 delta_h=-1 edges=53506 changed=221263 bytes=38154 J=26233063",

      "DEBLOCK_TRIAL index=15 mode=3 delta=-1 delta_h=1 edges=53506 changed=240554 bytes=38154 J=26199193",

      "DEBLOCK_TRIAL index=16 mode=3 delta=-1 delta_h=-2 edges=53506 changed=188950 bytes=38154 J=26259295",

      "DEBLOCK_TRIAL index=17 mode=3 delta=1 delta_h=0 edges=53506 changed=256176 bytes=38154 J=26185189",

      "DEBLOCK_TRIAL index=18 mode=3 delta=1 delta_h=-1 edges=53506 changed=241689 bytes=38151 J=26208504",

      "DEBLOCK_TRIAL index=19 mode=3 delta=1 delta_h=1 edges=53506 changed=260335 bytes=38154 J=26175658",

      "DEBLOCK_TRIAL index=20 mode=3 delta=1 delta_h=-2 edges=53506 changed=210077 bytes=38154 J=26233815",

      "DEBLOCK_TRIAL index=21 mode=3 delta=-2 delta_h=0 edges=53506 changed=205454 bytes=38154 J=26229918",

      "DEBLOCK_TRIAL index=22 mode=3 delta=-2 delta_h=-1 edges=53506 changed=190098 bytes=38154 J=26253929",

      "DEBLOCK_TRIAL index=23 mode=3 delta=-2 delta_h=1 edges=53506 changed=209877 bytes=38154 J=26219810",

      "DEBLOCK_TRIAL index=24 mode=3 delta=-2 delta_h=-2 edges=53506 changed=156435 bytes=38154 J=26280576",

      "DEBLOCK_FINAL winner=19 forced=-1 bytes=38154 Joff=26426937 Jfinal=26175658"

    ],

    "interpretation": "25 个分支各考虑31个开启区域、均零否决，所有PCW候选被接受，最终winner19。因此不只是输入未改变，实际码流与最终解码也严格相同。"

  },

  "classification": {

    "development": "35/36",

    "independent": "23/24",

    "animation": "12/12",

    "real": "11/12",

    "interpretation": "分类质量不等于编码质量；不重新调独立验证集。"

  },

  "code_findings": [

    {

      "location": "Source/Lib/Codec/encoder.c:907",

      "finding": "保护仅animation、10bit420、单帧all-intra、1920x1080或3840x2160正式路径启用；控制报告active_domain_tested=false，小图off一致不能证明主动保护收益。"

    },

    {

      "location": "Source/Lib/Encoder/content_filter_guard.c:34-73",

      "finding": "只评估拟开启RU内的Y边缘梯度误差与Y平坦区SSE；2% RMSE相对门限，无0.02绝对容差，空mask不否决。未检查边缘亮度、UV、ring。"

    },

    {

      "location": "quality_control.py:34-84",

      "finding": "离线动画QC检查6项；flat使用5x5最大梯度向量范数<=2，编码guard使用5x5内部各轴差绝对值<=2；前者有halo访问，后者排除RU边界。二者非相同mask、样本域、指标或门槛。历史flat_pixels80058与guard每次flat_samples785163不可当同一统计。"

    },

    {

      "location": "Source/Lib/Codec/encoder.c:982-1020",

      "finding": "先单RU因果CDF贪心开关，再仅将这一组mask与PCW全关作actualpacket J比较。guard仅能否决原本on的RU，不生成改善候选；一次局部否决可改变后续CDF，当前该效应未发生。"

    },

    {

      "location": "Source/Lib/Codec/encoder.c:1130-1170",

      "finding": "最终在25个deblock+CCSO+PCW结果中选最低真实包J；guard以各branch的PCW前重建为锚，不能约束deblock造成的变化，也不验证最终全帧局部QC。"

    },

    {

      "location": "score_content_frame.py:282-322",

      "finding": "正式runner检查固定解码exact、V1/NEG与SSE，没有导入或调用quality_control.py。auto PASS仅为分类/解码/双模型流程PASS，没有局部质量验收。"

    }

  ],

  "negative_controls": {

    "edge_1080": {

      "bytes": 39648,

      "v1": 95.343804,

      "neg": 92.714337,

      "stream_sha256": "9d9401ebdbfea380533ff92d8faea9e6b4e41386813a38333245305a4956f6a5",

      "decoded_sha256": "0415176f2fa201b21b10a7e116596dfe43b7ab7fa759305777d5cd12b0830275"

    },

    "vs_same_qp_baseline": {

      "bytes": 1494,

      "v1": -0.44264399999998716,

      "neg": 0.13892900000000452

    },

    "local_screen": "PASS_LOCAL_REGRESSION_SCREEN",

    "interpretation": "边缘误差改善和局部门通过不能替代同质量压缩率及V1/NEG；同QP不是同质量。不得复做活动权重。",

    "edge_uhd": {

      "bytes": 31389,

      "v1": 95.364072,

      "neg": 98.797174,

      "stream_sha256": "205aa273cba2a764879b057f10c45227b7cd77bac3485826518fd855b65c8f12",

      "decoded_sha256": "ea5a7054ffdbad0e4b2e2ca308b3137cd1b1c9be7daa0eb127f00b2e2d228a80"

    },

    "uhd_limitation": "此报告仅保存该实际点，不在缺少明确同基线配对时从其数值推导收益。"

  },

  "recommended_next": {

    "priority": "先将既有局部QC作为外部验收补齐；编码器方向仅建议有界PCW mask组合试验，优先级低于有真实结构性收益的系数/分块候选。",

    "mechanism": "在最终获胜deblock分支的原始PCW方案上，按既有RU两选择J差绝对值最小选固定4个RU（稳定坐标tie），枚举4个单翻转及6个双翻转mask，共至多10个新候选，保留原mask。对每个候选完整重打包并重建对应Y，用原global lambda和实际包字节比较strict J；保护仍有效，不以宽松阈值偷质量。",

    "reason": "现实现只评估贪心mask及全关，未比较相邻mask组合；自适应旗标后缀成本和实际包舍入可能使组合优于单RU估率。此为源码推导的机会，不是已证实收益。",

    "minimal_state": "保留获胜分支PCW前Y及完整过滤Y、RU SSE与选择差、原CCSO计划/头参数，或只重放该获胜分支一次；不重做预测/变换/量化，不扩展25个deblock模式。",

    "cost": "最多10次额外整包写出以及RU像素拼接；UHD内存需保存两张Y，约33.2MB，亦可复用已有缓冲。只在获胜branch运行，避免对25个branch各自增加10次打包。",

    "verification": [

      "宏off逐字节复现选定最新基线；单/双flip和跨64位mask的定向合法性夹具；固定AVM exact。",

      "原/候选所有mask、packet SHA、raw SSE、实际J与strict tie可审计；所有被拒候选可导出。",

      "首裁块若所有输入均原mask/原包不变，记录未找到改进并停止，不扩大完整帧。",

      "有真实变化才做预先固定2D与UHD区域的双模型点；V1/NEG同质量曲线与同口径QC均为晋升条件，负点保留。"

    ],

    "expected_limit": "现Q100为32 RU中31个开，整帧PCW仅增加3字节并明显减SSE；该搜索很可能收益小或零，不能据此主张达到0.90/0.88。无新候选收益前，不建议大图投资。",

    "not_recommended": [

      "继续调分类阈值或收紧guard，追求触发数而非压缩质量。",

      "给deblock添加+2偏移：当前接口合法域[-2,1]，不能把现winner+1当作可自由扩档。",

      "仅以原输入不变、active=1、局部screen PASS或者单点PSNR改善称优化成功。"

    ]

  },

  "external_qc_minimum": {

    "implementation": "保持冻结quality_control.py不变；在新验收runner中显式调用或只读import，分类real映射natural、animation映射animation；绑定同一reference、正式decoded、明确同质量baseline、脚本hash。",

    "missing_masks": "None样本应明确列出coverage，不据跳过指标声称完整保护。",

    "not_encoder_improvement": "补验收是诊断完整性工作，不计压缩收益。"

  },

  "awaiting_root_selection": true

}

</details>


<details>
<summary>031 · report.json</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\r13\classifier-review\report.json`  
原文件 SHA-256：`e0de2a60e3f3adcd18081666a5beaa92fa07f8e9c9bdf84b494174f8c3f37af3`

{

  "status": "PASS_WITH_SCOPE_LIMITATIONS",

  "created_utc": "2026-10-30T02:33:22.128504+00:00",

  "audit_scope": "只读 JSON、源码与工具身份；未读取任何分类/压缩像素；未运行预测、编码、解码或评分",

  "selected_model": {

    "path": "<LOCAL_AGENTDOCK>-AV2-r11\\semantic-selected\\selected-model.json",

    "sha256": "0bc8cfdc648707790575631aed5d121d7938cf822794fb0531903d12ae3ca80f",

    "bytes": 59048

  },

  "test_report": {

    "path": "<LOCAL_AGENTDOCK>-AV2-r11\\semantic-test-v1\\test-report.json",

    "sha256": "e0180fc83290e8cb9333e743e077998d731fa2bd48fecb73a686ef0edf73a46f",

    "bytes": 21617

  },

  "model_checkpoint": {

    "name": "OpenAI CLIP ViT-B/32",

    "url": "https://openaipublic.azureedge.net/clip/models/40d365715913c9da98579312b702a82c18be219cc2a73407c4526f58eba950af/ViT-B-32.pt",

    "sha256": "40d365715913c9da98579312b702a82c18be219cc2a73407c4526f58eba950af",

    "bytes": 353976522,

    "path_at_freeze": "<LOCAL_AGENTDOCK>-AV2-r11\\weights\\ViT-B-32.pt"

  },

  "inference": {

    "device": "cpu",

    "dtype": "float32",

    "batch_size": 1,

    "torch_threads": 2,

    "torch_interop_threads": 1,

    "preprocess": "Compose(\n    Resize(size=224, interpolation=bicubic, max_size=None, antialias=True)\n    CenterCrop(size=(224, 224))\n    <function _convert_image_to_rgb>\n    ToTensor()\n    Normalize(mean=(0.48145466, 0.4578275, 0.40821073), std=(0.26862954, 0.26130258, 0.27577711))\n)",

    "preprocess_definition": {

      "resize_short_edge": 224,

      "interpolation": "BICUBIC",

      "center_crop": [

        224,

        224

      ],

      "convert": "RGB",

      "mean": [

        0.48145466,

        0.4578275,

        0.40821073

      ],

      "std": [

        0.26862954,

        0.26130258,

        0.27577711

      ],

      "antialias": true

    },

    "feature_dimension": 512,

    "feature_normalization": "L2 per image"

  },

  "environment": {

    "clip_revision": "d05afc436d78f1c48dc0dbf8e5980a9d471f35f6",

    "clip_direct_url": {

      "archive_info": {

        "hash": "sha256=c665a33987c7365078c9988fdae5699026ec75cc4424bc0d59ab2599df22229a",

        "hashes": {

          "sha256": "c665a33987c7365078c9988fdae5699026ec75cc4424bc0d59ab2599df22229a"

        }

      },

      "url": "file:///<LOCAL_AGENTDOCK>-AV2-r11/vendor/CLIP-d05afc436d78f1c48dc0dbf8e5980a9d471f35f6.zip"

    },

    "clip_package_files_sha256": {

      "__init__.py": "d7930eb290ebed432116795c91473200d098451a94c0aedd4ae0f934f97955e8",

      "clip.py": "9540f200fbf8145479fa655382a56dab048d238cc698b9cbd8df3b6d86d3f1b6",

      "model.py": "9902cbe5ee90a1da2aa3e6f043e8a23dc1f8831193b963785c9af03d5c7bef2c",

      "simple_tokenizer.py": "d1b09f10ed3d1e2c343619cb98cbfacb92363dd8607526a43aa7fe2d60d15bad",

      "bpe_simple_vocab_16e6.txt.gz": "924691ac288e54409236115652ad4aa250f48203de50a9e4722a6ecd48d6804a"

    },

    "python": "3.11.0 (main, Oct 24 2022, 18:26:48) [MSC v.1933 64 bit (AMD64)]",

    "platform": "Windows-10-10.0.22631-SP0",

    "packages": {

      "torch": "2.7.1+cpu",

      "torchvision": "0.22.1+cpu",

      "clip": "1.0",

      "numpy": "2.4.6",

      "Pillow": "13.0.0",

      "ftfy": "6.3.1",

      "regex": "2026.9.29",

      "tqdm": "4.70.1",

      "packaging": "26.3",

      "scikit-learn": "1.9.1",

      "scipy": "1.17.1",

      "threadpoolctl": "3.7.0"

    }

  },

  "split": {

    "train_frames": 36,

    "test_frames": 24,

    "train_groups": 12,

    "test_groups": 8,

    "frames_per_group": 3,

    "deterministic_group_split_verified": true,

    "train_test_source_paths_disjoint": true,

    "group_scope": "按作品/剧集系列标识分组；每组当前只有一个电影或一集三个时刻，跨分集无交叉，但不是多集独立性实验",

    "train_non_strict_size_frames": 6,

    "groups": [

      {

        "group": "bocchi",

        "split": "test",

        "label": "anime",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\Bocchi the Rock 216048\\Bocchi.the.Rock.2022.BDRip.2160p.HEVC-YUV420P10.FLAC.03.mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "unspecified"

      },

      {

        "group": "bloom_into_you",

        "split": "train",

        "label": "anime",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\Bloom Into You 2160p48F\\Bloom.Into.You.2019.2160P.48F.BDRip.HEVC-10bit.Flac.03.mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "bt709"

      },

      {

        "group": "ghost_in_the_shell",

        "split": "train",

        "label": "anime",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\Ghost in the Shell 攻壳机动队系列\\攻壳机动队第一季 Stand Alone Complex 2002\\Stand.Alone.Complex.03.AV1.1080p.opus.x2.CN-JP.mkv",

        "stored_dimensions": [

          1920,

          1032

        ],

        "transfer": "unspecified"

      },

      {

        "group": "kon",

        "split": "test",

        "label": "anime",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\K-ON!\\[VCB-Studio] K-ON! The Movie [Ma10p_1080p]\\[VCB-Studio] K-ON! The Movie [Ma10p_1080p][x265_flac].mkv",

        "stored_dimensions": [

          1920,

          1080

        ],

        "transfer": "unspecified"

      },

      {

        "group": "violet_evergarden",

        "split": "train",

        "label": "anime",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\Violet.Evergarden.The.Movie.2020.4K.UHD.Blu-ray.REMUX.H265.10bit.Dolby.Vision.TrueHD.Atmos.mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "smpte2084"

      },

      {

        "group": "yuru_camp",

        "split": "test",

        "label": "anime",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\[VCB-Studio] Yuru Camp\\[Airota&Nekomoe kissaten&VCB-Studio] Yuru Camp Movie [Ma10p_2160p]\\[Airota&Nekomoe kissaten&VCB-Studio] Yuru Camp Movie [Ma10p_2160p_SDR][x265_flac].mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "bt709"

      },

      {

        "group": "adachi_shimamura",

        "split": "test",

        "label": "anime",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\Adachi to Shimamura 2160p\\Adachi.to.Shimamura.BDRip.2160p.AV1-10bit.FLAC.03-HDRFixed.mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "bt709"

      },

      {

        "group": "guilty_crown",

        "split": "train",

        "label": "anime",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\Guilty Crown 2160p AV1\\Guilty Crown 2011 EP03 [BD 3840x2160 23.976fps AV1-yuv420p10 FLACx2].mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "unspecified"

      },

      {

        "group": "kikis_delivery_service",

        "split": "train",

        "label": "anime",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\魔女宅急便\\魔女宅急便2160p24F AV1-10bit-HDRFixed.mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "bt709"

      },

      {

        "group": "shinkai",

        "split": "train",

        "label": "anime",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\新海诚电影系列\\默认分享-按需下载-2160p标准版\\Your.Name.2016.JAPANESE.2160p.BluRay.REMUX.HEVC.DTS-HD.MA.5.1-FGT.mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "smpte2084"

      },

      {

        "group": "article20",

        "split": "train",

        "label": "real",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\Article.20.2024.2160p.WEB-DL.H265.HQ.60fps.DDP5.1.Atmos-BATWEB.mkv",

        "stored_dimensions": [

          3840,

          1608

        ],

        "transfer": "bt709"

      },

      {

        "group": "delicioso",

        "split": "test",

        "label": "real",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\Delicioso.2021.BDRip.1080p.AV1.FR_CN_EN.mkv",

        "stored_dimensions": [

          1920,

          1080

        ],

        "transfer": "bt709"

      },

      {

        "group": "shawshank",

        "split": "train",

        "label": "real",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\The.Shawshank.Redemption.1994.BluRay.1080p.x265.10bit.5Audio.MNHD-FRDS.mkv",

        "stored_dimensions": [

          1920,

          1080

        ],

        "transfer": "unspecified"

      },

      {

        "group": "qingpingle",

        "split": "train",

        "label": "real",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\清平乐\\03.mp4",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "unspecified"

      },

      {

        "group": "breaking_bad",

        "split": "train",

        "label": "real",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\绝命毒师 1080 AV1 DTS-HD\\绝命毒师-2008-S01E03-1080p-AV1-DTS-HD.mkv",

        "stored_dimensions": [

          1920,

          1080

        ],

        "transfer": "bt709"

      },

      {

        "group": "harry_potter",

        "split": "test",

        "label": "real",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\Harry Potter Collection v2\\Harry Potter and the Philosophers Stone (2001)\\Harry Potter and the Philosophers Stone (2001) {imdb-tt0241527} {edition-Theatrical Cut} [Remux-2160p][HDR10][DTS-X 7.1][HEVC]-FraMeSToR.mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "smpte2084"

      },

      {

        "group": "tenet",

        "split": "train",

        "label": "real",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\信条.Tenet.2020.2160p.DV.UHD.BluRay.REMUX.DTS-HD.MA.5.1.HEVC.mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "smpte2084"

      },

      {

        "group": "interstellar",

        "split": "train",

        "label": "real",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\星际穿越.Interstellar.2014.2160p.DV.UHD.BluRay.REMUX.DTS-HD.MA.5.1.HEVC.mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "smpte2084"

      },

      {

        "group": "free_guy",

        "split": "test",

        "label": "real",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\Free.Guy.2021.2160p.BluRay.REMUX.HEVC.DTS-HD.MA.TrueHD.7.1.Atmos-FGT\\Free.Guy.2021.2160p.BluRay.REMUX.HEVC.DTS-HD.MA.TrueHD.7.1.Atmos-FGT.mkv",

        "stored_dimensions": [

          3840,

          2160

        ],

        "transfer": "smpte2084"

      },

      {

        "group": "legal_high",

        "split": "test",

        "label": "real",

        "source_path": "<LOCAL_MEDIA>\\4K电影+电视剧\\胜者即是正义2 1080p AV1\\胜者即是正义2 リーガルハイ2 S02E03.1080p..BluRay..opus.320k live.libsvtav1.6.crf36.mkv",

        "stored_dimensions": [

          1920,

          1080

        ],

        "transfer": "bt709"

      }

    ]

  },

  "evaluation": {

    "correct": 23,

    "total": 24,

    "accuracy": 0.9583333333333334,

    "class_recall": {

      "real": 0.9166666666666666,

      "anime": 1.0

    },

    "confusion": {

      "real": {

        "real": 11,

        "anime": 1

      },

      "anime": {

        "real": 0,

        "anime": 12

      }

    },

    "errors": [

      {

        "id": "free_guy-1",

        "label": "real",

        "prediction": "anime",

        "anime_score": 0.003819972276687622

      }

    ],

    "no_exclusions": true,

    "metadata_and_progress_reconciled": true,

    "heldout_pixel_hashes_reverified": false,

    "stored_resolution_breakdown": [

      {

        "width": 1920,

        "height": 1080,

        "frames": 9,

        "correct": 9,

        "accuracy": 1.0

      },

      {

        "width": 3840,

        "height": 2160,

        "frames": 15,

        "correct": 14,

        "accuracy": 0.9333333333333333

      }

    ],

    "limitation": "95.833% 是这 24 帧的观测准确率；每组三帧相关，不构成泛化准确率大于 90% 的置信保证。标记/时间戳和当前代码支持单次协议，不能证明历史上绝无旁路访问。"

  },

  "pipeline": {

    "sample": "2D-1080p-frame48",

    "q": 100,

    "dimensions": {

      "width": 1920,

      "height": 1080,

      "bitdepth": 10,

      "chroma": "420",

      "frames": 1

    },

    "encoder": {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\content-guard-work\\content-on.exe",

      "sha256": "70bd02a2dc089034fc3f85d312f8e55524f0b9f7c0e04990883c085c8493361f"

    },

    "code_files_verified": 405,

    "source_pixel_fixtures_not_read": [

      "tests/corpus/checkerboard_64x64_1f_420.y4m",

      "tests/corpus/flat_64x64_1f_420.y4m",

      "tests/corpus/gradient_64x64_1f_420.y4m",

      "tests/corpus/noise_64x64_1f_420.y4m",

      "tests/corpus/matrix/checkerboard_176x144_17f_420.y4m",

      "tests/corpus/matrix/checkerboard_176x144_1f_420.y4m",

      "tests/corpus/matrix/checkerboard_176x144_2f_420.y4m",

      "tests/corpus/matrix/checkerboard_176x144_3f_420.y4m",

      "tests/corpus/matrix/checkerboard_64x64_17f_420.y4m",

      "tests/corpus/matrix/checkerboard_64x64_1f_420.y4m",

      "tests/corpus/matrix/checkerboard_64x64_2f_420.y4m",

      "tests/corpus/matrix/checkerboard_64x64_3f_420.y4m",

      "tests/corpus/matrix/chroma_stress_176x144_17f_420.y4m",

      "tests/corpus/matrix/chroma_stress_176x144_1f_420.y4m",

      "tests/corpus/matrix/chroma_stress_176x144_2f_420.y4m",

      "tests/corpus/matrix/chroma_stress_176x144_3f_420.y4m",

      "tests/corpus/matrix/chroma_stress_64x64_17f_420.y4m",

      "tests/corpus/matrix/chroma_stress_64x64_1f_420.y4m",

      "tests/corpus/matrix/chroma_stress_64x64_2f_420.y4m",

      "tests/corpus/matrix/chroma_stress_64x64_3f_420.y4m",

      "tests/corpus/matrix/flat_black_176x144_17f_420.y4m",

      "tests/corpus/matrix/flat_black_176x144_1f_420.y4m",

      "tests/corpus/matrix/flat_black_176x144_2f_420.y4m",

      "tests/corpus/matrix/flat_black_176x144_3f_420.y4m",

      "tests/corpus/matrix/flat_black_64x64_17f_420.y4m",

      "tests/corpus/matrix/flat_black_64x64_1f_420.y4m",

      "tests/corpus/matrix/flat_black_64x64_2f_420.y4m",

      "tests/corpus/matrix/flat_black_64x64_3f_420.y4m",

      "tests/corpus/matrix/flat_grey_176x144_17f_420.y4m",

      "tests/corpus/matrix/flat_grey_176x144_1f_420.y4m",

      "tests/corpus/matrix/flat_grey_176x144_2f_420.y4m",

      "tests/corpus/matrix/flat_grey_176x144_3f_420.y4m",

      "tests/corpus/matrix/flat_grey_64x64_17f_420.y4m",

      "tests/corpus/matrix/flat_grey_64x64_1f_420.y4m",

      "tests/corpus/matrix/flat_grey_64x64_2f_420.y4m",

      "tests/corpus/matrix/flat_grey_64x64_3f_420.y4m",

      "tests/corpus/matrix/flat_white_176x144_17f_420.y4m",

      "tests/corpus/matrix/flat_white_176x144_1f_420.y4m",

      "tests/corpus/matrix/flat_white_176x144_2f_420.y4m",

      "tests/corpus/matrix/flat_white_176x144_3f_420.y4m",

      "tests/corpus/matrix/flat_white_64x64_17f_420.y4m",

      "tests/corpus/matrix/flat_white_64x64_1f_420.y4m",

      "tests/corpus/matrix/flat_white_64x64_2f_420.y4m",

      "tests/corpus/matrix/flat_white_64x64_3f_420.y4m",

      "tests/corpus/matrix/gradient_176x144_17f_420.y4m",

      "tests/corpus/matrix/gradient_176x144_1f_420.y4m",

      "tests/corpus/matrix/gradient_176x144_2f_420.y4m",

      "tests/corpus/matrix/gradient_176x144_3f_420.y4m",

      "tests/corpus/matrix/gradient_64x64_17f_420.y4m",

      "tests/corpus/matrix/gradient_64x64_1f_420.y4m",

      "tests/corpus/matrix/gradient_64x64_2f_420.y4m",

      "tests/corpus/matrix/gradient_64x64_3f_420.y4m",

      "tests/corpus/matrix/noise_176x144_17f_420.y4m",

      "tests/corpus/matrix/noise_176x144_1f_420.y4m",

      "tests/corpus/matrix/noise_176x144_2f_420.y4m",

      "tests/corpus/matrix/noise_176x144_3f_420.y4m",

      "tests/corpus/matrix/noise_64x64_17f_420.y4m",

      "tests/corpus/matrix/noise_64x64_1f_420.y4m",

      "tests/corpus/matrix/noise_64x64_2f_420.y4m",

      "tests/corpus/matrix/noise_64x64_3f_420.y4m",

      "tests/corpus/matrix/thin_lines_176x144_17f_420.y4m",

      "tests/corpus/matrix/thin_lines_176x144_1f_420.y4m",

      "tests/corpus/matrix/thin_lines_176x144_2f_420.y4m",

      "tests/corpus/matrix/thin_lines_176x144_3f_420.y4m",

      "tests/corpus/matrix/thin_lines_64x64_17f_420.y4m",

      "tests/corpus/matrix/thin_lines_64x64_1f_420.y4m",

      "tests/corpus/matrix/thin_lines_64x64_2f_420.y4m",

      "tests/corpus/matrix/thin_lines_64x64_3f_420.y4m"

    ],

    "prediction_score": 0.007993578910827637,

    "guard_invocations": 25,

    "guard_active_all": true,

    "considered_per_invocation": 31,

    "vetoes_total": 0,

    "bytes": 38154,

    "v1": 95.786448,

    "neg": 92.575408,

    "fixed_decoder_exact_record": true,

    "stream_sha256_record": "52c510b7e4751ff4b11c1bf15739df9a7c0fe0831bda8e1355dc268e00b9fdab",

    "decoded_sha256_record": "5505abf1d574532406389289aadf71585e2cb14ec3b93952615a8a67f98d624d",

    "effect": "保护已激活，但现有 25 次 PCW 调用全部零否决；仅证明自动接线，不证明压缩或质量改善。未重新验证像素/码流哈希。"

  },

  "routing": {

    "strict_dimensions": [

      [

        1920,

        1080

      ],

      [

        3840,

        2160

      ]

    ],

    "bitdepth": 10,

    "chroma": "420",

    "frames": 1,

    "encoder_domain": "animation + still_picture + ALL_INTRA，M6_PC_WIENER=1 且 M6_CONTENT_GUARD=1；强制 PCW 测试模式不激活保护",

    "classification_resize": "全源帧仅生成 640×360 JPEG，CLIP 再按短边 224 缩放并中心裁剪；送给编码器的原尺寸 Y4M 不缩放",

    "excludes": "4096×2160、其它宽高/位深/色度不属于严格策略域；未找到本次核查记录中的 UHD 自动链完整帧实测",

    "guard": "仅 Y 的 PCW restoration-unit 否决。源图定义强边缘/5×5 平坦区域，局部 SSE 比限 1.0404 即 RMSE 比 1.02；空掩码不否决，RU 边缘邻域不跨界。不是 V1/NEG 保证，不控制 DB/CCSO/量化。"

  },

  "gaps": [

    {

      "priority": "P2",

      "location": "score_content_frame.py:237",

      "detail": "content_type_applied=bool(args.encoder) 只表明外部编码器存在/选项传入，未验证宏或 CONTENT_GUARD active。真实 content-on 日志弥补当前样本，但同一自动入口接受 content-off.exe 时仍可 PASS；需把 hint_delivered、guard_active、vetoes 分开记录，不能以该布尔值证明保护生效。"

    },

    {

      "priority": "P2",

      "location": "content-guard-work/source-freeze.json",

      "detail": "此文件的 source 指向 r9/uhd-lambda100-work/source，清单是补丁前基线；patch-result 仍记 PATCHED_NOT_YET_BUILT。复用最终实现应绑定当前 score/result.json 的 source_manifest、content-on.exe、build.ps1 和 CMakeCache，而不能单独使用此基线冻结件。"

    },

    {

      "priority": "scope",

      "location": "content_filter_guard.c:29-70; encoder.c:1009-1020",

      "detail": "完整 packet J 只与 PCW-off 比较；未保留未加 guard 的 PCW-on 最终候选，不能宣称相对旧编码策略永不变差。当前零否决样本并未触发这项差异。"

    },

    {

      "priority": "scope",

      "location": "classifier/clip_frozen_model.py:281-307",

      "detail": "二分类是 anime/real，没有 CGI 或 mixed 类。低幅度余弦差不是校准概率；固定阈值保留，不利用终测误分调参。"

    },

    {

      "priority": "scope",

      "location": "classify_and_score_frame.py preview_command; selected-model inference",

      "detail": "HDR/色域采用 FFmpeg 默认预览，无显式 tone-map；16:9 中心裁剪仅保留约 56.25% 横向区域。模型只见预览而非整幅原生像素；不把存储 2160 尺寸解释为原生渲染。"

    },

    {

      "priority": "scope",

      "location": "classifier/manifest.json",

      "detail": "最终测试 24 帧均为严格尺寸（9 帧 1080、15 帧 UHD）；开发集另含 ghost_in_the_shell 1920×1032 与 article20 3840×1608，共 6 帧，不能把全部 60 帧称为严格 1080/2160。仅据现有 JSON 分层统计，不读取终测像素或改变阈值。"

    },

    {

      "priority": "scope",

      "location": "content-guard-work/controls-results.json",

      "detail": "16 个 off/on 对照均属 inactive domain，不能替代 UHD active/veto 路径全帧证据。现有自动 1080 样本零 veto，未证明自然内容的非零否决收益。"

    }

  ],

  "reusable": {

    "classifier": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\classifier\\clip_frozen_model.py",

    "probe": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\classifier\\clip_train_probe.py",

    "selected_model": "<LOCAL_AGENTDOCK>-AV2-r11\\semantic-selected\\selected-model.json",

    "test_report": "<LOCAL_AGENTDOCK>-AV2-r11\\semantic-test-v1\\test-report.json",

    "entry": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\classify_and_score_frame.py",

    "scorer": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\score_content_frame.py",

    "guard_source": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\content-guard-work\\source",

    "guard_encoder": {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\content-guard-work\\content-on.exe",

      "sha256": "70bd02a2dc089034fc3f85d312f8e55524f0b9f7c0e04990883c085c8493361f"

    },

    "baseline_controls": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r11-20261029\\content-guard-work\\controls-results.json"

  },

  "authorized_actions_performed": {

    "classification_pixels_read": 0,

    "predictions": 0,

    "encodes": 0,

    "decodes": 0,

    "scores": 0,

    "deletions": 0,

    "publishes": 0

  }

}

</details>


<details>
<summary>032 · report.json</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\r13\palette-review\report.json`  
原文件 SHA-256：`93e19b31ce9e187d1a46163ad6764142966c6a17347fa900c756004e1a5a115c`

{

  "status": "READ_ONLY_FEASIBILITY_COMPLETE",

  "scope": "只读现有源码和本地规范；未改publish、未编码、未解码、未测量图像、未扩大样本。无覆盖率或压缩收益实测。",

  "conclusion": "可实现限定的Y-only palette合规候选；single_picture并不禁用。比SDP改动边界窄，但当前仅有一个写0的mode符号，绝非只打开帧头即可。",

  "normative": [

    {

      "reference": "PDF §5.18.3.3 pp113–114；固定AVM decoder/decodeframe.c:6852",

      "finding": "screen-content允许标志可由帧头选择；置1后本配置还须写force_integer_mv位，可保持0；allow_intrabc另保持0。single_picture没有禁止palette。"

    },

    {

      "reference": "PDF §5.20.5.3 p172、§5.20.8.1 pp235–237；固定AVM common/blockd.h:3372、decoder/decodemv.c:1041",

      "finding": "仅Y、DC模式、非CHROMA_PART，合法block枚举>=BLOCK_8X8且宽高<=64；palette大小2..8。初版可进一步限制完整可见16x16，不需SDP。"

    },

    {

      "reference": "PDF §5.20.8.4–5 pp240–243；固定AVM decoder/detokenize.c:27、common/entropymode.c:38",

      "finding": "索引图含方向位（两轴均<64时），每行3符号identity-row，首像素NS(N)，其余按left/top-left/top推导5上下文和ColorOrder。首行不能copy-previous-row；右/底裁边按规范扩展。"

    },

    {

      "reference": "固定AVM common/pred_common.c:594–641；PDF get_palette_cache pp237–238",

      "finding": "上邻缓存每64像素行边界禁用；左邻依可用性。当前固定实现交错加入上/左颜色，palette_add_to_cache直接追加，保留重复，不能套用AV1缓存去重假设。初版可全写cache-use=0，但仍须发正确数量的cache标志及维护邻居状态。"

    },

    {

      "reference": "固定AVM common/reconintra.c:1699；encoder/bitstream.c:2624",

      "finding": "palette颜色索引替代Y预测；其后仍是正常变换残差链路，索引图独立于coeff tokens且位于系数前。UV没有palette，不等于可以沿用与新Y预测不一致的旧CfL重建。"

    }

  ],

  "missing_chain": [

    {

      "where": "frame_header.h / frame_header.c:241；sequence_header.c:400",

      "gap": "帧配置无palette/screen-content开关，当前单帧固定0；sequence已选SELECT，通常无需新增sequence能力。需同步正式writer/rate配置并计入新header位。"

    },

    {

      "where": "tile_writer.c:443、625、843；entropy_model_data.h:60",

      "gap": "现有MODEL_PALETTE只写symbol0，且tree->modes非空才写；单帧rate/writer强制modes=NULL，所以不是现成单帧palette入口。须对所有合格DC块写0/1，而不只新候选。"

    },

    {

      "where": "entropy_model_data.h/.c；entropy_models.h；固定AVM entropy_inits_modes.h:378",

      "gap": "缺size、identity-row、color-index模型。完整2..8新增40行（1 size+4 row+35 color）；仅2色可先新增10行（1+4+5），size仍是7符号CDF、row仍3符号。须含固定AVM training offsets，正式/估率/提交/回退共享更新。"

    },

    {

      "where": "tile_writer.h:SvtAv2TileCoeffBlock；tile_writer.c:TileTree",

      "gap": "无每PB颜色表/索引图描述、无owned索引池、无above/left palette size/color状态。需明确存储、clone/memo/cache键和rate-session试算回滚；不能把索引图塞进qcoeff。"

    },

    {

      "where": "rd_frame.c / codec/encoder.c",

      "gap": "缺palette预测与候选搜索、颜色量化、残差重算、UV及因果后缀重放。完整packet要重新计熵/滤波成本；palette局部J赢不能替代最终包比较。"

    },

    {

      "where": "codec/encoder.c:936及deblock/PCW路径",

      "gap": "即使采用重建保持的零残差palette替换，coeff skip/非零状态变化仍可能改变最终滤波；不能仅凭prefilter Y相同就保证最终recon相同。"

    }

  ],

  "minimum_steps": [

    "规范夹具：8/10bit、Y-only、现有共享分区、完整可见16x16、DC/mrl0、2色、方向固定0；先恒identity-row0、cache-use全0，仍写合法CDF符号和所有未用块flag0。固定AVM exact验证后再加相邻palette/cache边界及裁边。此阶段不称压缩候选成熟。",

    "生产最窄实验可先只对原DC完整16x16叶增加2色palette，保留原候选；默认off，不改变分区。若用新Y重建，重算UV和受影响后续预测/系数及熵状态，再跑现有滤波并比较实际整包。",

    "更保守的低成本探针：只寻找原未滤波重建Y本身恰好2值的原DC叶，用相同两值索引+零残差保持Y，再完整重写熵及重跑滤波；预计覆盖很窄，不能用它的零收益否定一般palette。此处只是建议，未扫描像素。",

    "全包回退保留原screen-content=0路径；若没有palette赢家应输出原包，避免全帧flag0开销。质量仍按既定同AOM双指标及局部QC验收，同QP只诊断。"

  ],

  "estimated_effort": {

    "qualification": "仅静态工程估计，不是已完成工作或承诺",

    "forced_normative": "约8–10个文件、400–800行实现及CDF数据，另加针对性夹具",

    "bounded_production": "约12–16个文件、1000–2000行总实现及测试；主要复杂性在状态所有权和因果重放，不是2色聚类",

    "relative_to_sdp": "无需4轴geometry、拆Y/UV树、扩大变换存储或重写分区枚举；但是增加新的预测/索引语法/邻居缓存，不属于数十行参数扩展"

  },

  "expected_coverage": {

    "likely": "少色平面、硬边界、文字/线稿等Y亮度分区；同样可存在于1080/2160，不由分辨率直接决定",

    "limited": "高质量抗锯齿、渐变、纹理和多色块常需更多颜色/残差；原DC+16x16+2色约束进一步减小覆盖；UV独立色差不会直接受益",

    "not_measured": "未扫描当前压缩集、未按VMAF选区域，不能给出覆盖百分比或推算0.90/0.88目标"

  },

  "files": {

    "<LOCAL_PRIVATE>\\ffmpeg-native\\deps-full\\AV2\\docs\\spec\\AV2_Spec_v1.0.0_20260528_38f28e7.pdf": "e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publication-satd-work\\source-manifest.json": "a01fb6af84a5ce5a348a9d2b29e76852fce8e4b1ce2f7f73f3283252f96b2ce5",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publish\\Source\\Lib\\Bitstream\\frame_header.c": "7c5b4e765d809043cf584abd361c00767ead9faadb02a873bac1f259b99c078b",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publish\\Source\\Lib\\Bitstream\\frame_header.h": "da6b79cce52a09e003fae315e57cfabaaae2e2796ebbe7b037e0d32150494ab4",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publish\\Source\\Lib\\Bitstream\\sequence_header.c": "e94aa93126fb9f2851daca8b7b735ab3301330ce4a005514fffc674e4e9c0c01",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publish\\Source\\Lib\\Bitstream\\tile_writer.c": "f7150181939ac380acad59220115e457bdfabc8324fc2a7276ee405716ed35d9",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publish\\Source\\Lib\\Bitstream\\tile_writer.h": "e36551ef61ad2715572c14f0b5c63f5970c641cc3dde4c3aa39a6d89e68b05a3",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publish\\Source\\Lib\\EntropyCoding\\entropy_model_data.h": "4ba19cfad8709951dffc89abd9337a7b8cb9df45bd431b2a16e89cef3d3144df",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publish\\Source\\Lib\\EntropyCoding\\entropy_model_data.c": "eeb99b329cbc04fad4aae24096cdf7609ad1883a679f8d10cf0a8b7a79cccdc3",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publish\\Source\\Lib\\EntropyCoding\\entropy_models.h": "cc4405957cebf1e0049af33f0b67717d8d4975d540b43d739c2ef4f32df9791b",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publish\\Source\\Lib\\Codec\\encoder.c": "bc2f4e4f43dd5c09ca9fbd0e5fc42d4ca143a05c1909225406462c4853949044",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publish\\Source\\Lib\\Encoder\\rd_frame.c": "a54eb27b96b4414ad04e80fae2866dc14fe07f09c8c21063af35ae81daa07a2c",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\publish\\Source\\Lib\\Encoder\\rd_frame.h": "06667784e8323c82cb0d3f05722b60b110bb7f05cf631708ce3d823f73b7f526",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\agentdock-r5-20261026\\avm-audit-source\\av2\\common\\blockd.h": "0d7caebc200e6eb062b757a439365877f280fa738bf32c545492c19dd6cb3b05",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\agentdock-r5-20261026\\avm-audit-source\\av2\\common\\entropymode.h": "6f51253d7c40878600080bd37e256d582b2510eebd7120df46ee3acb13410042",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\agentdock-r5-20261026\\avm-audit-source\\av2\\common\\entropymode.c": "724324f4beb7b0f94d3e3116538e2b9d0e0805fd3ab5809699bfaca6a21559c5",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\agentdock-r5-20261026\\avm-audit-source\\av2\\common\\entropy_inits_modes.h": "704e1a99cad1e6d024ffe8420be140c205d916c2bfa437d812ad1bff6b64e0d2",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\agentdock-r5-20261026\\avm-audit-source\\av2\\common\\pred_common.c": "5bb952dbc95b04fe40e07ba24850866981c6e472d5563aaf5eeb6289b1618e0c",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\agentdock-r5-20261026\\avm-audit-source\\av2\\common\\reconintra.c": "7e7716f9702b1f175e11c37fcda06bcb03c1a2f9f5b3219dd882f3df134e6589",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\agentdock-r5-20261026\\avm-audit-source\\av2\\decoder\\decodemv.c": "a4e52109f713a490004f40a09f61d02847db45d5fa5625f60f524593ee9d0976",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\agentdock-r5-20261026\\avm-audit-source\\av2\\decoder\\detokenize.c": "0ef35264a92e7e5fa3c88968d8277c23a5f9ff04235bafaed01edd7bb83bada7",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\agentdock-r5-20261026\\avm-audit-source\\av2\\decoder\\decodeframe.c": "a115107c076d8b3a5021b732a74c8887838c1008ece78f883c5b9edd9cc8c7fd",

    "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\agentdock-r5-20261026\\avm-audit-source\\av2\\encoder\\bitstream.c": "793f573e417facd900fd2f2d3c80f988a89011a05aa2f2285a0560eab88140b2"

  }

}

</details>


<details>
<summary>033 · report.json</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\r13\ccso-bands-screen\report.json`  
原文件 SHA-256：`de92a713744bbc64c7224982a83b80de39c2d70758366f5e98b3412a62ccbc4d`

{

  "status": "NON_NORMATIVE_FINAL_PIXEL_POTENTIAL_ONLY",

  "utc": "2026-10-30T05:27:50.906561+00:00",

  "result": {

    "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r9-20261029\\reused\\2D-1080p-frame48\\r6-q100\\result.json",

    "sha256": "721a69fbc5f42fe57c9d47cb342d1f7a75f92a9fed59a6c9da5d936c83715a75"

  },

  "source_raw": {

    "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r9-20261029\\corpus-work\\inputs\\2D-1080p-frame48.yuv",

    "sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09"

  },

  "stream": {

    "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r6-20261027\\screen\\context-pcw-fine16-q100\\stream.obu",

    "sha256": "52c510b7e4751ff4b11c1bf15739df9a7c0fe0831bda8e1355dc268e00b9fdab"

  },

  "decoded": {

    "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r9-20261029\\reused\\2D-1080p-frame48\\r6-q100\\decoded.yuv",

    "sha256": "5505abf1d574532406389289aadf71585e2cb14ec3b93952615a8a67f98d624d"

  },

  "encode_log": {

    "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r6-20261027\\screen\\context-pcw-fine16-q100\\encode.log",

    "sha256": "010b1076090ce86315f01fe5c943f7b5731ec921818ec4a9c8f6e1138aa8342c"

  },

  "frozen_ccso_prototype": {

    "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\ccso-bands-work\\production-freeze.json",

    "sha256": "78a1d20f1f4368312b0d80e2def4138b42b07e1cd8de0fce76a14a0c4b51a810"

  },

  "original_scores": {

    "bytes": 38154,

    "v1": 95.786448,

    "neg": 92.575408

  },

  "winning_filter_log": [

    "CCSO_AUTO,mask=3,accepted=1,units=510,lutU=69,lutV=48,writers=2,rewrite=0,no_space=0,work_bytes=100336,table_bytes=97920,off_bytes=38072,on_bytes=38151,Joff=26598123,Jon=26585422,estimate=26585095,lambda=9680",

    "CCSO_AUTO_SSE,pre=48389151/5958718/5977502,proposal=48389151/5899120/5890703,final=48389151/5899120/5890703,flagR=1083073/1365222,seq=16,frame_bits=64,frame_bytes=8,ticks=9/3/45/2,hz=1000",

    "CCSO_AUTO_MAP,000200000100100000100000000000000110000000000000100000000000000000003133202000020000000022020003313003300000032000000021000001331033200000222101000000000002033313000012322000000000000000010320000002202000000000000002020000000002322000000000000000000000000000001002000000000000002200000000001002000000000000000022000000100111020200000000002000000001332000000002000002222000032022001300300000000022020000000000000222100000001002100000000002100131200000000022002030000000100000100000000022302200002230000200000000",

    "PC_WIENER accepted=1 bytes=38151/38154 SSE=48389151/46746467 J=26585422/26175658 none_mask=2147483648",

    "DEBLOCK_TRIAL index=19 mode=3 delta=1 delta_h=1 edges=53506 changed=260335 bytes=38154 J=26175658"

  ],

  "summary": [

    {

      "bands": 4,

      "global_uniform_apply_sse_gain": 0,

      "same_global_lut_free_unit_flags_gain": 0,

      "independent_per_unit_per_band_offset_relaxed_gain": 306969,

      "continuous_global_mean_energy": 4719.039951367314,

      "extra_global_uniform_gain_vs4": 0,

      "extra_global_uniform_D_gain_vs4_approx": 0.0

    },

    {

      "bands": 8,

      "global_uniform_apply_sse_gain": 442,

      "same_global_lut_free_unit_flags_gain": 591,

      "independent_per_unit_per_band_offset_relaxed_gain": 453523,

      "continuous_global_mean_energy": 9660.168083059905,

      "extra_global_uniform_gain_vs4": 442,

      "extra_global_uniform_D_gain_vs4_approx": 110.5

    },

    {

      "bands": 16,

      "global_uniform_apply_sse_gain": 5700,

      "same_global_lut_free_unit_flags_gain": 9074,

      "independent_per_unit_per_band_offset_relaxed_gain": 609322,

      "continuous_global_mean_energy": 20895.798812119494,

      "extra_global_uniform_gain_vs4": 5700,

      "extra_global_uniform_D_gain_vs4_approx": 1425.0

    },

    {

      "bands": 32,

      "global_uniform_apply_sse_gain": 9391,

      "same_global_lut_free_unit_flags_gain": 30822,

      "independent_per_unit_per_band_offset_relaxed_gain": 794337,

      "continuous_global_mean_energy": 25662.14668532405,

      "extra_global_uniform_gain_vs4": 9391,

      "extra_global_uniform_D_gain_vs4_approx": 2347.75

    },

    {

      "bands": 64,

      "global_uniform_apply_sse_gain": 9467,

      "same_global_lut_free_unit_flags_gain": 29468,

      "independent_per_unit_per_band_offset_relaxed_gain": 1002940,

      "continuous_global_mean_energy": 35516.44632834388,

      "extra_global_uniform_gain_vs4": 9467,

      "extra_global_uniform_D_gain_vs4_approx": 2366.75

    }

  ],

  "planes": [

    {

      "plane": "U",

      "bands": 4,

      "raw_sse": 5899120,

      "band_counts": [

        50906,

        162947,

        225219,

        79328

      ],

      "band_mean_residual_native_10bit": [

        0.18671669351353476,

        0.04872750035287547,

        0.09747845430447698,

        0.029006151674062123

      ],

      "continuous_global_mean_energy": 4368.423665450484,

      "global_lut_offsets": [

        0,

        0,

        0,

        0

      ],

      "global_uniform_apply_sse_gain": 0,

      "same_global_lut_free_unit_flags_gain": 0,

      "independent_per_unit_per_band_offset_relaxed_gain": 152957

    },

    {

      "plane": "U",

      "bands": 8,

      "raw_sse": 5899120,

      "band_counts": [

        100,

        50806,

        109595,

        53352,

        75918,

        149301,

        79328,

        0

      ],

      "band_mean_residual_native_10bit": [

        2.06,

        0.1830295634373893,

        -0.018467995802728225,

        0.18675963412805519,

        0.14755393977712794,

        0.072015592661804,

        0.029006151674062123,

        0.0

      ],

      "continuous_global_mean_energy": 6518.558169046068,

      "global_lut_offsets": [

        1,

        0,

        0,

        0,

        0,

        0,

        0,

        0

      ],

      "global_uniform_apply_sse_gain": 312,

      "same_global_lut_free_unit_flags_gain": 317,

      "independent_per_unit_per_band_offset_relaxed_gain": 227989

    },

    {

      "plane": "U",

      "bands": 16,

      "raw_sse": 5899120,

      "band_counts": [

        0,

        100,

        23734,

        27072,

        68570,

        41025,

        25280,

        28072,

        27715,

        48203,

        103637,

        45664,

        75227,

        4101,

        0,

        0

      ],

      "band_mean_residual_native_10bit": [

        0.0,

        2.06,

        0.16836605713322658,

        0.1958850472813239,

        0.030217296193670702,

        -0.09984156002437539,

        0.08350474683544304,

        0.27974494157879737,

        0.4096337723254555,

        -0.0031325851088106547,

        0.06980132578133293,

        0.07704099509460406,

        -0.0014090685525143898,

        0.5869300170690076,

        0.0,

        0.0

      ],

      "continuous_global_mean_energy": 11820.514145757252,

      "global_lut_offsets": [

        0,

        1,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        1,

        0,

        0

      ],

      "global_uniform_apply_sse_gain": 1025,

      "same_global_lut_free_unit_flags_gain": 3135,

      "independent_per_unit_per_band_offset_relaxed_gain": 300511

    },

    {

      "plane": "U",

      "bands": 32,

      "raw_sse": 5899120,

      "band_counts": [

        0,

        0,

        0,

        100,

        2663,

        21071,

        9964,

        17108,

        32972,

        35598,

        26322,

        14703,

        11446,

        13834,

        14497,

        13575,

        14350,

        13365,

        19644,

        28559,

        51185,

        52452,

        25197,

        20467,

        32287,

        42940,

        3959,

        142,

        0,

        0,

        0,

        0

      ],

      "band_mean_residual_native_10bit": [

        0.0,

        0.0,

        0.0,

        2.06,

        0.6214795343597447,

        0.1111005647572493,

        -0.019269369731031716,

        0.3211947626841244,

        0.007036273201504306,

        0.051688297095342436,

        -0.1170883671453537,

        -0.06896551724137931,

        0.12493447492573825,

        0.04922654329911812,

        0.27205628750776023,

        0.2879558011049724,

        0.6060627177700348,

        0.19872802095024317,

        0.1310323762981063,

        -0.09541650618018838,

        0.07597929080785386,

        0.06377259208419125,

        0.11100527840615947,

        0.03522743929251967,

        -0.11924303899402236,

        0.0871914299021891,

        0.5506441020459713,

        1.5985915492957747,

        0.0,

        0.0,

        0.0,

        0.0

      ],

      "continuous_global_mean_energy": 16009.519818801702,

      "global_lut_offsets": [

        0,

        0,

        0,

        1,

        1,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        1,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        1,

        1,

        0,

        0,

        0,

        0

      ],

      "global_uniform_apply_sse_gain": 4716,

      "same_global_lut_free_unit_flags_gain": 24883,

      "independent_per_unit_per_band_offset_relaxed_gain": 391418

    },

    {

      "plane": "U",

      "bands": 64,

      "raw_sse": 5899120,

      "band_counts": [

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        100,

        436,

        2227,

        7854,

        13217,

        4678,

        5286,

        8200,

        8908,

        15940,

        17032,

        18411,

        17187,

        14979,

        11343,

        8324,

        6379,

        5490,

        5956,

        6858,

        6976,

        7681,

        6816,

        6244,

        7331,

        7110,

        7240,

        6153,

        7212,

        10992,

        8652,

        11155,

        17404,

        23857,

        27328,

        26114,

        26338,

        14315,

        10882,

        9106,

        11361,

        14354,

        17933,

        25873,

        17067,

        2458,

        1501,

        142,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0

      ],

      "band_mean_residual_native_10bit": [

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        2.06,

        0.5825688073394495,

        0.6290974405029187,

        0.057168321874204224,

        0.14314897480517516,

        -0.36532706284737065,

        0.2869844873250095,

        0.3568292682926829,

        0.28839245621912885,

        -0.00959849435382685,

        0.022604509159229687,

        0.07620444299603497,

        0.025426194216559027,

        -0.03538286935042393,

        -0.22498457198272062,

        -0.1746756367131187,

        0.06897632857814705,

        0.2526411657559198,

        0.007219610476830087,

        0.005249343832020997,

        0.0924598623853211,

        0.28720218721520635,

        0.2549882629107981,

        0.2014734144778988,

        0.3616150593370618,

        0.6410689170182841,

        0.5716850828729282,

        0.3560864618885097,

        0.06447587354409318,

        0.2872088791848617,

        -0.06738326398520574,

        -0.3221873599282833,

        0.049931050333256725,

        -0.04082659177599866,

        0.17794935597189696,

        0.00524622807689362,

        0.12180119978737945,

        0.13754802654558157,

        0.07608895423635362,

        -0.019986821875686362,

        0.07948243992606285,

        -0.10624216246342483,

        -0.12964924998605923,

        -0.023499400919877865,

        0.2549950196285229,

        0.48454027664768107,

        0.6588940706195869,

        1.5985915492957747,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0

      ],

      "continuous_global_mean_energy": 21430.3126769039,

      "global_lut_offsets": [

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        1,

        1,

        1,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        1,

        1,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        1,

        1,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0

      ],

      "global_uniform_apply_sse_gain": 4792,

      "same_global_lut_free_unit_flags_gain": 23529,

      "independent_per_unit_per_band_offset_relaxed_gain": 492167

    },

    {

      "plane": "V",

      "bands": 4,

      "raw_sse": 5890703,

      "band_counts": [

        50906,

        162947,

        225219,

        79328

      ],

      "band_mean_residual_native_10bit": [

        -0.011589989392213099,

        0.01736761032728433,

        0.027124709726976854,

        -0.04031363453005244

      ],

      "continuous_global_mean_energy": 350.6162859168297,

      "global_lut_offsets": [

        0,

        0,

        0,

        0

      ],

      "global_uniform_apply_sse_gain": 0,

      "same_global_lut_free_unit_flags_gain": 0,

      "independent_per_unit_per_band_offset_relaxed_gain": 154012

    },

    {

      "plane": "V",

      "bands": 8,

      "raw_sse": 5890703,

      "band_counts": [

        100,

        50806,

        109595,

        53352,

        75918,

        149301,

        79328,

        0

      ],

      "band_mean_residual_native_10bit": [

        1.15,

        -0.01387631382120222,

        -0.07170035129339843,

        0.20032988454041087,

        0.025250928633525647,

        0.02807750785326287,

        -0.04031363453005244,

        0.0

      ],

      "continuous_global_mean_energy": 3141.609914013838,

      "global_lut_offsets": [

        1,

        0,

        0,

        0,

        0,

        0,

        0,

        0

      ],

      "global_uniform_apply_sse_gain": 130,

      "same_global_lut_free_unit_flags_gain": 274,

      "independent_per_unit_per_band_offset_relaxed_gain": 225534

    },

    {

      "plane": "V",

      "bands": 16,

      "raw_sse": 5890703,

      "band_counts": [

        0,

        100,

        23734,

        27072,

        68570,

        41025,

        25280,

        28072,

        27715,

        48203,

        103637,

        45664,

        75227,

        4101,

        0,

        0

      ],

      "band_mean_residual_native_10bit": [

        0.0,

        1.15,

        0.12378865762197691,

        -0.1345670803782506,

        -0.021744203004229256,

        -0.15519804996953077,

        0.17207278481012658,

        0.22577657452265604,

        -0.013783149918816526,

        0.0476941269215609,

        0.04369095979235215,

        -0.007358093903293623,

        0.014954736996025363,

        -1.0541331382589612,

        0.0,

        0.0

      ],

      "continuous_global_mean_energy": 9075.28466636224,

      "global_lut_offsets": [

        0,

        1,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        -1,

        0,

        0

      ],

      "global_uniform_apply_sse_gain": 4675,

      "same_global_lut_free_unit_flags_gain": 5939,

      "independent_per_unit_per_band_offset_relaxed_gain": 308811

    },

    {

      "plane": "V",

      "bands": 32,

      "raw_sse": 5890703,

      "band_counts": [

        0,

        0,

        0,

        100,

        2663,

        21071,

        9964,

        17108,

        32972,

        35598,

        26322,

        14703,

        11446,

        13834,

        14497,

        13575,

        14350,

        13365,

        19644,

        28559,

        51185,

        52452,

        25197,

        20467,

        32287,

        42940,

        3959,

        142,

        0,

        0,

        0,

        0

      ],

      "band_mean_residual_native_10bit": [

        0.0,

        0.0,

        0.0,

        1.15,

        -0.0015020653398422831,

        0.13962317877651748,

        -0.059213167402649536,

        -0.17845452419920504,

        -0.07309232075700595,

        0.02581605708185853,

        -0.16400729427855026,

        -0.13942732775624023,

        0.16713262275030577,

        0.17616018505132283,

        0.26984893426226114,

        0.1787108655616943,

        -0.04808362369337979,

        0.023045267489711935,

        0.06521075137446548,

        0.035645505795020835,

        0.05646185405880629,

        0.031228551818805766,

        -0.03246418224391793,

        0.023550105047149068,

        0.05894013070275962,

        -0.018118304611085236,

        -1.0586006567314978,

        -0.9295774647887324,

        0.0,

        0.0,

        0.0,

        0.0

      ],

      "continuous_global_mean_energy": 9652.626866522347,

      "global_lut_offsets": [

        0,

        0,

        0,

        1,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        -1,

        -1,

        0,

        0,

        0,

        0

      ],

      "global_uniform_apply_sse_gain": 4675,

      "same_global_lut_free_unit_flags_gain": 5939,

      "independent_per_unit_per_band_offset_relaxed_gain": 402919

    },

    {

      "plane": "V",

      "bands": 64,

      "raw_sse": 5890703,

      "band_counts": [

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        100,

        436,

        2227,

        7854,

        13217,

        4678,

        5286,

        8200,

        8908,

        15940,

        17032,

        18411,

        17187,

        14979,

        11343,

        8324,

        6379,

        5490,

        5956,

        6858,

        6976,

        7681,

        6816,

        6244,

        7331,

        7110,

        7240,

        6153,

        7212,

        10992,

        8652,

        11155,

        17404,

        23857,

        27328,

        26114,

        26338,

        14315,

        10882,

        9106,

        11361,

        14354,

        17933,

        25873,

        17067,

        2458,

        1501,

        142,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0

      ],

      "band_mean_residual_native_10bit": [

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        1.15,

        0.4426605504587156,

        -0.08845981140547822,

        0.26266870384517443,

        0.06650525837935992,

        -0.07182556648140231,

        -0.048051456678017405,

        -0.29024390243902437,

        -0.07555006735518635,

        0.05501882057716437,

        -0.19298966651009863,

        0.0032046059420998314,

        0.05003781928201548,

        -0.18772948794979638,

        -0.13268094860266244,

        -0.10271504084574724,

        -0.18733343784292208,

        0.2040072859744991,

        0.13314304902619206,

        0.10177894429862934,

        0.24928325688073394,

        0.41986720479104284,

        0.10079225352112677,

        0.06053811659192825,

        0.27936161505933704,

        0.011533052039381153,

        -0.10662983425414364,

        0.015927189988623434,

        0.029118136439267885,

        0.023198689956331876,

        0.11858529819694869,

        -0.008068130883012102,

        0.06366352562629281,

        0.2138156515907281,

        -0.08090603044496487,

        0.10775829057210691,

        -0.04465031513402688,

        -0.07153335661893119,

        0.018930343686822275,

        0.000658906215681968,

        0.041897720271102896,

        0.1176675491152292,

        0.011933307310544806,

        0.07513624241487264,

        -0.15948907247905314,

        -0.7815296989422295,

        -1.5123251165889406,

        -0.9295774647887324,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0,

        0.0

      ],

      "continuous_global_mean_energy": 14086.13365143998,

      "global_lut_offsets": [

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        1,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        -1,

        -1,

        -1,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0,

        0

      ],

      "global_uniform_apply_sse_gain": 4675,

      "same_global_lut_free_unit_flags_gain": 5939,

      "independent_per_unit_per_band_offset_relaxed_gain": 510773

    }

  ],

  "limits": [

    "分类Y是PCW后的finalY，UV已经过旧CCSO，因此它不是新原型应读取的preCCSO重建；测得额外后处理偏移不等于可编码收益。",

    "不修改像素文件、不评分、不编码；这里只统计最终残差偏差。",

    "global LUT不计header/flag rate；free-unit-flags不计上下文率；per-unit-per-band offset允许每unit不同LUT，仅极乐观放松，不是合法全局LUT。",

    "均值分离和SSE改善不保证V1/NEG或SPEED改善；这些分数只引用原结果，未计算反事实质量。"

  ],

  "normative_snapshot_recovery": {

    "route": "独立固定v1诊断decoder只在deblock之后、CDEF之前把当前cur_frame三plane紧密u16le导出，然后继续原decode直到final。固定现有码流无需编码。",

    "hook": "av2/decoder/decodeframe.c，use_ccso之前(约9942)，放在if(use_ccso)外可覆盖原NONE码流；此处ext_rec_y还未复制。当前M6 CDEF关闭，捕获Y与UV即本工具preCCSO合法像素。若将来CDEF开启，Y仍此处捕获、UV需改在CDEF后CCSO前。",

    "source": "<LOCAL_PRIVATE>/ffmpeg-native/deps-full/AV2/svt-av2/artifacts/m53-b/diagnostic-decoder/source",

    "required_proof": "独立构建新目录；原固定AVM final与诊断final逐字节一致；记录hook源/程序/stream/input/snapshot SHA与dims；snapshotY核对CCSO_AUTO_SSE preY、UV核对preUV；再从该快照统计band或执行合法LUT提案。",

    "cost": "只需一次诊断decoder构建+一次原帧decode（历史原固定解码约0.141秒，不计新构建）；无需重编码RD搜索。尚未实施。"

  }

}

</details>


<details>
<summary>034 · report.json</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\r13\uv-basic-small-work\report.json`  
原文件 SHA-256：`4f0568adf6ebe47f6a2adc7024d7090c262f7efcf207ecebe4c154c218042231`

{

  "status": "PASS_SMALL_CONTROLS_AND_THREE_ORIGINAL_CROPS",

  "scope": "仅独立420 UV4x4/4x8/8x4 basic shortlist补扫；pair开启，lambda4/4，无新色度权重、无inverse+clip原型。",

  "source": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\uv-basic-small-work\\source",

  "production_freeze": {

    "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\uv-basic-small-work\\production-freeze.json",

    "sha256": "27906d2780b975f03dc83dd0fbe88a4481917671cc4f22931465bcfe6fea5915"

  },

  "change": {

    "files": [

      "Source/Lib/Encoder/rd_frame.c"

    ],

    "macro": "M6_UV_SMALL_FULL",

    "default": 0,

    "old_order_and_ties": "原mask所有候选先按原顺序；新增候选后追加且只接受strict<。",

    "domain": "非partial SB basicprefix下，420 UV4x4/4x8/8x4；444及其他UV尺寸不扩。",

    "unchanged": "预测、变换、CDF、RDOQ和原4/4失真权重保持原实现。原完整SB候选精修/重建/率回放检查及滤波实际包门控保持。",

    "limit": "这是局部模式搜索扩展；没有自动双编码整帧或以off包为最终全帧J保底。必须据两臂实际包成本与双质量选择，不保证每个点全帧J/V1/NEG都单调。"

  },

  "apps": {

    "off": {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\uv-basic-small-work\\uv-basic-off.exe",

      "sha256": "b246532356b77390fb344f5cfbce683d0af9dff972b3d46b8d9a8ff7867d9b8b"

    },

    "on": {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\uv-basic-small-work\\uv-basic-on.exe",

      "sha256": "b29d120066c4eeffbc5b8387dbd9d489f8e23dfcd30ee462c678e0c6ad40d2c0"

    }

  },

  "validation": {

    "total": 14,

    "controls": 8,

    "crops": 6,

    "off_exact_cases": 7,

    "fixed_avm_exact_cases": 14,

    "source_checked_before_after": true,

    "msvc_clean_build": true,

    "independent_sanitizer_run": false,

    "result": {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\uv-basic-small-work\\validation-results.json",

      "sha256": "edddeaf1a91ce3e618e95d100547ebedd6c7c0649ca5cb21ef8474f04290d03e"

    },

    "frozen_baseline_metadata": {

      "path": "<LOCAL_PRIVATE>\\ffmpeg-native-m6-tests\\size-r13-20261030\\uv-basic-small-work\\baseline-evidence-check.json",

      "sha256": "d0889b701566c6f04d0899c327179059adbdd8c7cd15a97c00089caa8de29878"

    }

  },

  "comparisons": [

    {

      "case": "2Da",

      "q": 90,

      "model_family": "1080",

      "old": {

        "bytes": 1413,

        "v1": 95.159632,

        "neg": 93.32468,

        "packet_j": 582272

      },

      "new": {

        "bytes": 1394,

        "v1": 94.911707,

        "neg": 93.267268,

        "packet_j": 577767

      },

      "delta": {

        "bytes": -19,

        "v1": -0.24792499999999507,

        "neg": -0.05741199999999935,

        "packet_j": -4505

      },

      "sse_delta": [

        -4251,

        -568,

        -269

      ],

      "candidate_dominates_fixed_q": false,

      "baseline_dominates_fixed_q": false,

      "same_stream": false,

      "matched_quality_claim": false

    },

    {

      "case": "2Db",

      "q": 100,

      "model_family": "1080",

      "old": {

        "bytes": 734,

        "v1": 94.478126,

        "neg": 90.636769,

        "packet_j": 586261

      },

      "new": {

        "bytes": 734,

        "v1": 94.37275,

        "neg": 90.571454,

        "packet_j": 585871

      },

      "delta": {

        "bytes": 0,

        "v1": -0.1053760000000068,

        "neg": -0.06531499999999824,

        "packet_j": -390

      },

      "sse_delta": [

        -24,

        -2057,

        524

      ],

      "candidate_dominates_fixed_q": false,

      "baseline_dominates_fixed_q": true,

      "same_stream": false,

      "matched_quality_claim": false

    },

    {

      "case": "2DUHD",

      "q": 100,

      "model_family": "2160",

      "old": {

        "bytes": 525,

        "v1": 94.153304,

        "neg": 96.516645,

        "packet_j": 302045

      },

      "new": {

        "bytes": 525,

        "v1": 94.153304,

        "neg": 96.516645,

        "packet_j": 302045

      },

      "delta": {

        "bytes": 0,

        "v1": 0.0,

        "neg": 0.0,

        "packet_j": 0

      },

      "sse_delta": [

        0,

        0,

        0

      ],

      "candidate_dominates_fixed_q": false,

      "baseline_dominates_fixed_q": false,

      "same_stream": true,

      "matched_quality_claim": false

    }

  ],

  "conclusion": "只报告三个固定QP小裁块，不外推同质量体积曲线或完整1080/UHD收益。控制图用于正确性，不用于质量收益声明。",

  "evidence": [

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\small8-q110-off\\result.json",

      "sha256": "9efd6ee6dd7b229a5f6c4489962c77df56d86b69787f8ef82c79cf0a3e3d9b71"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\small8-q110-on\\result.json",

      "sha256": "22c38645239a10229ed59c846a5ac6478a21d3f899018f69fe1dce5972729789"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\partial80-q110-off\\result.json",

      "sha256": "ade6b5567ebfce7a7a3a00edf34a7de785acdee3eb185f203225897b39f87b9c"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\partial80-q110-on\\result.json",

      "sha256": "5fafb811fdfa816aaf793243f44963a48c5bad12da622136034aa95742974fed"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\edge24-q110-off\\result.json",

      "sha256": "effa7242281690d5d3e1e7564ea95a48bc5805c58fa98523f35d53c5efe800ba"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\edge24-q110-on\\result.json",

      "sha256": "1009c88a2f6f59f7d2ccdff89e5a921915713545255665d8814e84975c2d8853"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\flat8x2160-q110-off\\result.json",

      "sha256": "ddcdad7bfabc1827f4142e2b8913dc04321511d26391c4c0bc4dff367920573a"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\flat8x2160-q110-on\\result.json",

      "sha256": "eabb0257dad62a5ecf33b3533a19ce7a40057afaf3ea1f715382d8acd37ef84d"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\2Da-q90-off\\result.json",

      "sha256": "45894119f7a2ae798485d24e5d691093744b16306e05ba1a18634f34ffc5ce8e"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\2Da-q90-on\\result.json",

      "sha256": "01cea5a6ad6db455ca4c4e9e043b1f31dc13fa5966ce447e6ee3ecfebffcd1f8"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\2Db-q100-off\\result.json",

      "sha256": "91b9bf6c8eb3fb8d7885bb418c0755654b56215a1feeb4b249ccadd24dfb02e7"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\2Db-q100-on\\result.json",

      "sha256": "ac1123fbe61dce9f173af5ef95514617f523554fd3fc1b6690f8da4a8b4daa09"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\2DUHD-q100-off\\result.json",

      "sha256": "8b81073f2c6c7a62521033715433d9f426ead7945b14c61ea42c533e86baf576"

    },

    {

      "path": "<LOCAL_AGENTDOCK>-AV2-r13\\uv-basic-small\\validation\\2DUHD-q100-on\\result.json",

      "sha256": "c3c7cc6b7d3bb5848a5efd1a61b4ac1db550f5373dc5113bb0c59bb3dfd0a30b"

    }

  ],

  "encoders_concurrent_max": 1,

  "full_frame_encodes": 0,

  "heldout_reads": 0

}

</details>


<details>
<summary>035 · SINGLE_FRAME_RESULTS.txt</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\r13\publish\publication\SINGLE_FRAME_RESULTS.txt`  
原文件 SHA-256：`1becc25ee42bfd4186f01775817eb16196113424f01272385e6e39c8c7a5853c`

单帧研发结果（2026-10-30）

这些结果来自六张冻结开发帧，不代表素材总体性能。逐点字节数、V1、NEG、
输入/码流/重建/程序哈希及独立配置标识见 numerical-evidence.json。
各分辨率使用各自冻结的 V1/NEG 模型，不混合比较绝对分数。

比较口径
在 AOM V1 93–97 范围内，沿同一 AOM 参考曲线取得 V1 和 NEG 目标，
分别寻找候选同时满足两项的最小可行字节数。对 log(bytes) 曲线分别采用
PCHIP、分段线性插值；不外推，不删除质量反转，不拼接不同配置。
下表均值为沿参考 V1 区间对体积比取对数积分后的几何均值。
每组双方均有至少五个 V1 区间内实测点，并有低于 93、高于 97 的包围点；
两种方法的双约束覆盖均完整。最坏检查点与主 V1 单约束诊断另见 JSON。
这只满足全局曲线研究条件；最终局部内容 QC 和素材来源限制仍须单独满足。

历史最佳独立配置及新增实拍 UHD 基线
组别 / 公开标识                 配置             native/AOM点数  PCHIP      linear
2D 1080 / animation-1080-a       r6                    10/10     1.030190    1.027603
3D 1080 / rendered-1080-a        SATD                   7/10     1.089011    1.088844
实拍1080 / live-1080-a           lambda150             13/12     0.880928    0.881838
2D 2160 / animation-2160-a       lambda150              8/7      1.069609    1.070596
3D 2160 / rendered-2160-a        lambda150              9/10     1.028779    1.029540
实拍2160 / live-2160-a           lambda150             10/7      1.089529    1.098773

前五组为历史完整曲线，并非本次加入 content guard 的发布构建重新完成的整条曲线。
2D 1080 的六个旧 AOM 点已由 canonical 程序重编码，码流和评分逐点完全一致。
实拍2160新增 Q80 为 50,712 字节，V1 97.959769，NEG 100.000000，补齐双约束
高端覆盖。该帧颜色元数据未完整标注，保持 research-only，不声称已认证 SDR/HDR。
它的主 V1 单约束均值为 0.998789 / 1.001717，但相应最小率上的 NEG 平均差为
-0.092634 / -0.097857；不能将这个单约束结果当成同等 V1+NEG 的压缩收益。
2160 表示存储分辨率原尺寸输入，不证明动画原生渲染分辨率。

新候选的局部结果
fine16 pair 的 2D 1080 Q100：37,986 字节，V1 95.259939，NEG 92.611835。
同 QP 旧基线为 38,154 字节，V1 95.786448，NEG 92.575408；体积小了 168 字节，
但 V1 下降 0.526509，不能据此默认启用。
2D UHD pair 的三个点仅支持 93.4–96.4 局部诊断：相对旧最佳的双约束体积比
几何均值为 1.003590 / 1.015691，两种方法均更大。不是完整 93–97 验收。
2D 1080 chroma2-pair 的三个点仅支持 94–96 局部诊断：相对旧最佳为
0.999866 / 1.009317，两种插值判断不一致，未证明稳定收益，不纳入默认发布配置。
上述有限点没有借用旧基线补齐新候选曲线，也没有因此放宽局部内容 QC。

本次发布构建的完整帧回归（与历史曲线分开）
base 实拍1080 Q92：88,780 字节，V1 95.401826，NEG 93.635571。
该点码流与固定解码重建均与历史 lambda150 同 QP 点逐字节一致。
SATD 3D1080 Q100：112,682 字节，V1 95.842621，NEG 92.989975。
没有历史同 QP 完整帧，不能声称该点与旧 SATD exact。其额外 SATD 提案虽为
112,656 字节，但整包 J 为 84,625,852，高于原分支的 84,603,724，实际已回退。
这两点均已完成固定解码、V1/NEG 和局部指标测量；单点测量不等于完整曲线 QC 验收。
另有 integration pair+guard 的 3D2160 Q104 为 57,770 字节、95.909177/97.819937，
以及实拍2160 Q96 自动包装结果；它们按独立配置记录，不冒充 base 曲线。

构建与测试范围
原一次 CTest 记录为 112/114 通过。两张旧非法参数夹具仍把已合法的 3285
视为非法；修正为首个保留值 3289 后，两项及六项相关定向回归共 8 项通过。
生产源码和 native 程序未因这项夹具修正而改变；没有重跑并获得单次 114 项全绿。
默认关闭 fine16 pair 和 SATD 实验选项；SATD 可显式选择，未完成二者组合质量验收。

目标状态
0.90×、0.88× 尚未在所有内容组全面达成。实拍1080历史均值低于 0.90×，
仍高于 0.88×；均值也不表示每个质量目标都达到相同比例。
当前未完成 >16× 速度验收。没有将裁块、单点、插值方法不一致的局部改善，
或只完成编码/分类的输出作为全面质量达标依据。

</details>


<details>
<summary>036 · numerical-evidence.json</summary>

原位置：`<LOCAL_PRIVATE>\ffmpeg-native\deps-full\AV2\svt-av2\tests\r13\publish\publication\numerical-evidence.json`  
原文件 SHA-256：`c2b0d5d547c5c4fc1fa98ffc8888e941cb46c2c9949b54f68e50d46c4722c0f5`

{

  "schema_version": 1,

  "scope": "Frozen development frames; no generalization claim",

  "snapshot_sha256": "6139a641202ba607b39cc43e7fcea02f8515d2b7a2db592de9b2981c412a3863",

  "quality_band": [

    93,

    97

  ],

  "speed_acceptance": "NOT_MEASURED",

  "published_build_full_frame_equivalence": "REPORTED_SEPARATELY",

  "groups": {

    "animation-1080-a": {

      "content": "2D",

      "dimensions": [

        1920,

        1080

      ],

      "input_bitdepth": 10,

      "original_bitdepth": 8,

      "color_limitations": "Color metadata incomplete; research-only, no certified HDR/SDR claim",

      "aom_points": [

        {

          "q": 56,

          "bytes": 6083,

          "v1": 57.455761,

          "neg": 52.329141,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "ff62e59f6ff287bf918aa179721971add303ccc8f13ce90b3113fdb1929b9a8d",

          "result_sha256": "1025e2980ee452b2736ba661373d8184fd54d8fb5cd814d95574c4f73192dcf0",

          "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

          "stream_sha256": "27105afa8995cc64255c666708c76a0ac67073c00d26456d59a40cee6afb8e0c",

          "decoded_sha256": "d55c96d9a6caa048762eee0b05ee1ef68c48d0dedad3c5b7e9d79825afb01d60",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 48,

          "bytes": 9928,

          "v1": 75.591019,

          "neg": 70.397417,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "ff62e59f6ff287bf918aa179721971add303ccc8f13ce90b3113fdb1929b9a8d",

          "result_sha256": "936da8410f25855fe831727b937e99641884e8a26109852a0f4cda4eaedfef67",

          "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

          "stream_sha256": "9fb50f13895eb54f7622356cd7112cc03d83644f2856e21f8c6d1a613462d06f",

          "decoded_sha256": "c735298230fabc2d03130cbc866492e52290aef0bcbd9ee1abfb74b216c033a9",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 40,

          "bytes": 15022,

          "v1": 85.797904,

          "neg": 81.571989,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "ff62e59f6ff287bf918aa179721971add303ccc8f13ce90b3113fdb1929b9a8d",

          "result_sha256": "94b79921169949b8b7b1198c95596c7a2602311a6b9571a82b6c6cffd64bdae7",

          "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

          "stream_sha256": "7f296a48ee748fb427f8a766739ae798ce31cfed39cd7eee5a998df1dc56bcbf",

          "decoded_sha256": "96dd4b1dcbcf2e3cc822b48741b9d12910578800480ec55c94a2e7cb8ea2e5fc",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 32,

          "bytes": 22712,

          "v1": 91.39692,

          "neg": 88.15873,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "ff62e59f6ff287bf918aa179721971add303ccc8f13ce90b3113fdb1929b9a8d",

          "result_sha256": "34cdff417d5065b3c713abdad328cd7ce60d2642ae9c3bd533e2a5e1e21be37b",

          "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

          "stream_sha256": "a67a2c04c028178665b7d2e7c2bb6b8a7511a9e1b45f322dd1ff7123b94cacb2",

          "decoded_sha256": "a98949eb059560da4b1aa6f38f6d62df1117edf259e16fb71e1691462fa77c28",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 28,

          "bytes": 27733,

          "v1": 93.250899,

          "neg": 90.268264,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "ff62e59f6ff287bf918aa179721971add303ccc8f13ce90b3113fdb1929b9a8d",

          "result_sha256": "6bc746917c97870bb5c0db9c3990db40603ad5be818f4afd42c513e2b427dd34",

          "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

          "stream_sha256": "46d3c0a1f6b4ed87af8d5294b7f74220189f277aa7dfdf7b41c8a0d2c61ed91e",

          "decoded_sha256": "235c2178850afacdaed24bf60bc72c1e387d65fd6a6c0b2c9abe005efc623f05",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 26,

          "bytes": 30515,

          "v1": 94.225022,

          "neg": 91.167142,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "ff62e59f6ff287bf918aa179721971add303ccc8f13ce90b3113fdb1929b9a8d",

          "result_sha256": "3aaeff9b7df8513d93fe48ac142753dc5551c62db8376c98b07852b5b814da24",

          "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

          "stream_sha256": "bcc5eaea52d8a80314b8a95405c932e7e7b3c3f14aba17dc34323ade7cee028a",

          "decoded_sha256": "437bf6bf794036a596f9809706dee1ef3bd5c46f53275932afdb87402267e4c0",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 24,

          "bytes": 34350,

          "v1": 95.144493,

          "neg": 91.983045,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "ff62e59f6ff287bf918aa179721971add303ccc8f13ce90b3113fdb1929b9a8d",

          "result_sha256": "8455d0c543560393a3812a8a4629c1dfffb6a472d769c56eadb0945e2a077aa4",

          "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

          "stream_sha256": "a867787c0500b3ac47fb3033b664f162492440e26729dd11438cc4b9020d3df3",

          "decoded_sha256": "37486a1918243cc244f10d434d866c892665a75609d09b94ad78df9033e10f60",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 20,

          "bytes": 40084,

          "v1": 95.768759,

          "neg": 92.970398,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "ff62e59f6ff287bf918aa179721971add303ccc8f13ce90b3113fdb1929b9a8d",

          "result_sha256": "0e183f91deefce79c6e6b197e88b567a1fd52c4bde7be4517ed579a136fcbb24",

          "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

          "stream_sha256": "9051ab4b16c67e913b02cab0a853d020d89288d1dacabce30ff0f5d6fce41c4d",

          "decoded_sha256": "143070ef52362c4855c9f8ced2fd0970807bd82a85e0672130dc06b2b6855f07",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 16,

          "bytes": 48502,

          "v1": 96.524045,

          "neg": 93.84837,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "ff62e59f6ff287bf918aa179721971add303ccc8f13ce90b3113fdb1929b9a8d",

          "result_sha256": "4e4c0732c1fbb1a194acdc1e19dea3f6d0a41081dab1cd4e70aa976965ef9084",

          "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

          "stream_sha256": "184988e271635e7a9ba445e7bea373f5733440870dfae584e21e82cb6a284922",

          "decoded_sha256": "6b8a0954ceada0346a82fa46cdee1a29e703272db77bc198253790c2c8b62ec4",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 12,

          "bytes": 64530,

          "v1": 97.568711,

          "neg": 94.770391,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "ff62e59f6ff287bf918aa179721971add303ccc8f13ce90b3113fdb1929b9a8d",

          "result_sha256": "f7d54d87f4ea2c24ec0fc9fb2e765601c3a3e11a2a281e12e8495d3e38e7a5cb",

          "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

          "stream_sha256": "e94335db47679ff95bc0bd6cd5a4721d6066acb5226625469a27cd335de3eb16",

          "decoded_sha256": "5f8f2bea44493738216c364cf4a63e04805f3e4d11cd9d3108376bdd9f2fe4bb",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        }

      ],

      "aom_coverage": {

        "points": 10,

        "inside_93_97": 5,

        "below_93": true,

        "above_97": true,

        "eligible": true

      },

      "previous_best": [

        {

          "status": "COMPLETE_GLOBAL_RESEARCH_COMPARISON",

          "coverage": {

            "native": {

              "points": 10,

              "inside_93_97": 8,

              "below_93": true,

              "above_97": true,

              "eligible": true

            },

            "aom": {

              "points": 10,

              "inside_93_97": 5,

              "below_93": true,

              "above_97": true,

              "eligible": true

            }

          },

          "points": [

            {

              "q": 116,

              "bytes": 26298,

              "v1": 92.591462,

              "neg": 89.680725,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "dd5a95f8918a1192d8d481a8676b497b199d4bbbd7525c7342e98569eacefad9",

              "configuration_key": "0a0c802eab80571455ca736863dc44a95a0cd2799829e3e216ec63b16b892d92",

              "result_sha256": "8693c4ee1101e23be4d7976f3fcc8b8a17a24debed266273f08256d2a648c059",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "34f38e3ef0ed11b8ba78375b53a64c014e2b7e4d48431cc667ae46d59f19202e",

              "decoded_sha256": "a50aef24220ca60ab283e77e6cbfd4fd4206ca6612a96fdb0c64f48edca84d79",

              "encoder_sha256": "0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d"

            },

            {

              "q": 113,

              "bytes": 28058,

              "v1": 93.555932,

              "neg": 90.39076,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "dd5a95f8918a1192d8d481a8676b497b199d4bbbd7525c7342e98569eacefad9",

              "configuration_key": "0a0c802eab80571455ca736863dc44a95a0cd2799829e3e216ec63b16b892d92",

              "result_sha256": "6b880efce4c6f1e8b0ed90271839183934ab4ad9b09c8b589e764329a290e8de",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "ca34144809fea5daaee3fce9020ab3744f2cf5e768dc6684d8cc84c66597d7de",

              "decoded_sha256": "a2d6a5931dcc9d48b1758a27209d7d30b4d96c83b16c4b5e05918a3a270c9dff",

              "encoder_sha256": "0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d"

            },

            {

              "q": 110,

              "bytes": 30013,

              "v1": 93.872384,

              "neg": 90.827214,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "dd5a95f8918a1192d8d481a8676b497b199d4bbbd7525c7342e98569eacefad9",

              "configuration_key": "0a0c802eab80571455ca736863dc44a95a0cd2799829e3e216ec63b16b892d92",

              "result_sha256": "1e26fe311f02d86923810fccf531ea5c1eb7ed2d9f5bbef332e8f810d7c2b823",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "180d580b216b18f94379010692dfc6440ccf3810168ffead0ccd8628153fd124",

              "decoded_sha256": "d1455d3c1eb23d75c9644a3b941641aabb1a0d50d5108e92d4659c84ca00f47a",

              "encoder_sha256": "0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d"

            },

            {

              "q": 108,

              "bytes": 31737,

              "v1": 94.315891,

              "neg": 91.371511,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "dd5a95f8918a1192d8d481a8676b497b199d4bbbd7525c7342e98569eacefad9",

              "configuration_key": "0a0c802eab80571455ca736863dc44a95a0cd2799829e3e216ec63b16b892d92",

              "result_sha256": "162a6a981bfa7455ac59e9b44e73b6fb34cc21a7dfd7de69504cc990a3103b0f",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "7a0991c9bdd2fdee46cf3d81177733003b75825f8a517fdde849b0dae83efc6d",

              "decoded_sha256": "c49b3a4fbe24efbe02255a47ca6c733a49066ca51fbaa7deecfa10e3fd8b260a",

              "encoder_sha256": "0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d"

            },

            {

              "q": 106,

              "bytes": 33372,

              "v1": 94.62689,

              "neg": 91.695465,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "dd5a95f8918a1192d8d481a8676b497b199d4bbbd7525c7342e98569eacefad9",

              "configuration_key": "0a0c802eab80571455ca736863dc44a95a0cd2799829e3e216ec63b16b892d92",

              "result_sha256": "08057bea492c550a633873c86c115bd1b20fd3ca44514958036fa1fe1d673c6a",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "d3d3eca1476f9d002f009416c00ad3b9944188a595ab87d36cfce231d28ed60d",

              "decoded_sha256": "86b9cd39f4bfb576451c071544b436f9aeeecfde2a9b78f66e735f3dd2c49c48",

              "encoder_sha256": "0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d"

            },

            {

              "q": 104,

              "bytes": 35005,

              "v1": 94.855093,

              "neg": 92.057093,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "dd5a95f8918a1192d8d481a8676b497b199d4bbbd7525c7342e98569eacefad9",

              "configuration_key": "0a0c802eab80571455ca736863dc44a95a0cd2799829e3e216ec63b16b892d92",

              "result_sha256": "f46e134078a68ef8f64438089f17c1436dd832849fe857a05172336e60d5e4b7",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "eb73c120ed19c251cac97ce33f158d917dbf290ec642bdc221de3a129749a355",

              "decoded_sha256": "3a510674550bda6b674cb2f71e9747bc5cd3df6f9ae301ed098e4601ae206688",

              "encoder_sha256": "0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d"

            },

            {

              "q": 100,

              "bytes": 38154,

              "v1": 95.786448,

              "neg": 92.575408,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "dd5a95f8918a1192d8d481a8676b497b199d4bbbd7525c7342e98569eacefad9",

              "configuration_key": "0a0c802eab80571455ca736863dc44a95a0cd2799829e3e216ec63b16b892d92",

              "result_sha256": "721a69fbc5f42fe57c9d47cb342d1f7a75f92a9fed59a6c9da5d936c83715a75",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "52c510b7e4751ff4b11c1bf15739df9a7c0fe0831bda8e1355dc268e00b9fdab",

              "decoded_sha256": "5505abf1d574532406389289aadf71585e2cb14ec3b93952615a8a67f98d624d",

              "encoder_sha256": "0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d"

            },

            {

              "q": 92,

              "bytes": 46420,

              "v1": 96.329948,

              "neg": 93.508396,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "dd5a95f8918a1192d8d481a8676b497b199d4bbbd7525c7342e98569eacefad9",

              "configuration_key": "0a0c802eab80571455ca736863dc44a95a0cd2799829e3e216ec63b16b892d92",

              "result_sha256": "f25cbb1ac36271781dad002c431591d80ddd7f78e17bb743fc417bd0c8d31538",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "0b44d895f470f3d24b5a80df09fce72f06109cddc1cc0726d1e126a4023753ce",

              "decoded_sha256": "96504cdec78bfb25632d651c3459ed43b8585f8ba7eec8107c7574ca45c83d20",

              "encoder_sha256": "0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d"

            },

            {

              "q": 84,

              "bytes": 57391,

              "v1": 96.991942,

              "neg": 94.313859,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "dd5a95f8918a1192d8d481a8676b497b199d4bbbd7525c7342e98569eacefad9",

              "configuration_key": "0a0c802eab80571455ca736863dc44a95a0cd2799829e3e216ec63b16b892d92",

              "result_sha256": "6a186c50c9c0b764618f3375afbc1dd32331b536f1812256c1f2a6b2d26357c2",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "9a2dc4d3910002e8b45ac2fb6bac95e24373e7ac26e879b786e9afd541092fdd",

              "decoded_sha256": "a28018bcbf4ab46358137246b0d327d3ebc9e6e2acd796ad9513391af7146456",

              "encoder_sha256": "0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d"

            },

            {

              "q": 82,

              "bytes": 60770,

              "v1": 97.187936,

              "neg": 94.520488,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "dd5a95f8918a1192d8d481a8676b497b199d4bbbd7525c7342e98569eacefad9",

              "configuration_key": "0a0c802eab80571455ca736863dc44a95a0cd2799829e3e216ec63b16b892d92",

              "result_sha256": "286facd8a094e6ebca9b53b4a3b1e50e6a9668ae795bc9b1709f6d48f6054856",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "a3dc7056657f26de5b3ff9166f6de1e790dfb0dab17fd35b8e0a582284be6bce",

              "decoded_sha256": "c69a24c52b801e73593c66fa29a2861bd2df7cd79cb76a09d57e3f5eb549668c",

              "encoder_sha256": "0a3a7f206dca7c97df948b4c6f76b4adcc67e229b6b7a2b24b54a839d160d46d"

            }

          ],

          "dual": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 1.0301900496049456,

              "worst_sample_ratio": 1.0651890426310864

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 1.0276025429361555,

              "worst_sample_ratio": 1.0577480835409454

            }

          },

          "primary_v1": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 1.0104144364439613,

              "neg_delta_at_selected_rate": {

                "mean": -0.035578381226369984,

                "worst_sample": -0.42710735483868234,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 1.0651890426310864

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 1.01045468407951,

              "neg_delta_at_selected_rate": {

                "mean": -0.02709335079526439,

                "worst_sample": -0.4146749676743582,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 1.0577480835409454

            }

          },

          "eligible_band_comparison": true,

          "local_quality_acceptance": "SEPARATE_FINAL_DECODED_GATE_REQUIRED"

        }

      ],

      "new_candidates": [

        {

          "status": "INCOMPLETE_GLOBAL_COMPARISON",

          "coverage": {

            "native": {

              "points": 3,

              "inside_93_97": 3,

              "below_93": false,

              "above_97": false,

              "eligible": false

            },

            "aom": {

              "points": 10,

              "inside_93_97": 5,

              "below_93": true,

              "above_97": true,

              "eligible": true

            }

          },

          "points": [

            {

              "q": 110,

              "bytes": 29493,

              "v1": 93.928498,

              "neg": 90.580844,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "3f58bbee5155926b2b1799cf8470ffbd9ca97659de9ded8b60e6236435da6a7d",

              "configuration_key": "c4173eb4a50798b9625225c8e99681bce7cfdbe2efae0f20df181ade85e6426e",

              "result_sha256": "e34365d7e9ec8ca2a3a031c952479ba92514070e6af7be8b1758fcec2d072fb9",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "fac38a6b5379a3e428b2122b18942ef96e2f3c03a4e57c53f0f2b97af9ef45b3",

              "decoded_sha256": "524467f49454a5aa59d0a1a6aa2d6f6d970d118a6a553031206e482495a45d27",

              "encoder_sha256": "9db6e57c7d68ff1bd45f3b1cc6bf8ea3dace8a2df4ff24b2e950ceea326829f6"

            },

            {

              "q": 100,

              "bytes": 37367,

              "v1": 95.45891,

              "neg": 92.336536,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "3f58bbee5155926b2b1799cf8470ffbd9ca97659de9ded8b60e6236435da6a7d",

              "configuration_key": "c4173eb4a50798b9625225c8e99681bce7cfdbe2efae0f20df181ade85e6426e",

              "result_sha256": "51ae249c34579d89f2ed4c1357e776f5bcdcea715cb6963b25d0e5fd6410858d",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "b0319beec9b748dd7411582354c168ca974eb542f25054e8d4e48c9af8987043",

              "decoded_sha256": "2603e976010c115fdec0db801ef2ef9ef6ff85fa9bc715568ca28a535cb8934b",

              "encoder_sha256": "9db6e57c7d68ff1bd45f3b1cc6bf8ea3dace8a2df4ff24b2e950ceea326829f6"

            },

            {

              "q": 90,

              "bytes": 47928,

              "v1": 96.416798,

              "neg": 93.588488,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "3f58bbee5155926b2b1799cf8470ffbd9ca97659de9ded8b60e6236435da6a7d",

              "configuration_key": "c4173eb4a50798b9625225c8e99681bce7cfdbe2efae0f20df181ade85e6426e",

              "result_sha256": "d5f5822738d3aede14512de5a16dfac47ae5525f0b07566ed6344725bfc91389",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "3f5ad18728401bb8d4ae85a53d13a29661f7c566f5c772c186543bbcc6337f03",

              "decoded_sha256": "df31ba7ddb0349872c53745af338a9113e21255638564a7be38bc13b36073bf4",

              "encoder_sha256": "9db6e57c7d68ff1bd45f3b1cc6bf8ea3dace8a2df4ff24b2e950ceea326829f6"

            }

          ],

          "dual": {

            "pchip": {

              "full_coverage": false,

              "worst_sample_ratio": null

            },

            "linear": {

              "full_coverage": false,

              "worst_sample_ratio": null

            }

          },

          "primary_v1": {

            "pchip": {

              "full_coverage": false,

              "worst_sample_ratio": null

            },

            "linear": {

              "full_coverage": false,

              "worst_sample_ratio": null

            }

          },

          "eligible_band_comparison": false,

          "local_quality_acceptance": "SEPARATE_FINAL_DECODED_GATE_REQUIRED"

        },

        {

          "status": "INCOMPLETE_INSUFFICIENT_POINTS",

          "coverage": {

            "native": {

              "points": 1,

              "inside_93_97": 1,

              "below_93": false,

              "above_97": false,

              "eligible": false

            },

            "aom": {

              "points": 10,

              "inside_93_97": 5,

              "below_93": true,

              "above_97": true,

              "eligible": true

            }

          },

          "points": [

            {

              "q": 100,

              "bytes": 37986,

              "v1": 95.259939,

              "neg": 92.611835,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "149dcd9d441c67742768258f03cd7c46d65f802e6f39613c29967d848af7bca4",

              "configuration_key": "466c7926ce63471c276d26b4b9cc878728c112ae0b12c66f6271a9b65fa1661a",

              "result_sha256": "9c95724f20b384e7dd839f79aeef7f819a3d903f3460645772c43748c6dbdf5e",

              "source_sha256": "c0ef42a0f71c4e3326a7bcb3d26679916a0ebd768ed8c0c5b59aed83cac2ae09",

              "stream_sha256": "7a4979b6fb2a0552b348ce65f7a5be12cf8ac7403843fe2c94de0fcccb6816fb",

              "decoded_sha256": "8c899f07312e27b23c41ec64b03d6ef80259b64a7f16f2983a85dfed1c6c4938",

              "encoder_sha256": "757253db7c38b5a22d1918d05a9cd6416c060e1c4f4f2b750dfb7a74943c275c"

            }

          ],

          "dual": {},

          "primary_v1": {},

          "eligible_band_comparison": false,

          "local_quality_acceptance": "SEPARATE_FINAL_DECODED_GATE_REQUIRED"

        }

      ]

    },

    "rendered-1080-a": {

      "content": "3D",

      "dimensions": [

        1920,

        1080

      ],

      "input_bitdepth": 10,

      "original_bitdepth": 8,

      "color_limitations": "Color metadata incomplete; research-only, no certified HDR/SDR claim",

      "aom_points": [

        {

          "q": 56,

          "bytes": 16368,

          "v1": 65.792658,

          "neg": 55.262968,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "2b370321fdd15068933fbf915eab4474f22d268163a9e59ba93f4f63fb57d8a6",

          "result_sha256": "c272badfa88e512c705ef598dd265df6ae6a7f13c985111ad64f7b96ba4a984f",

          "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

          "stream_sha256": "73b5f4a04a4c731242ad355154011a6618a292f6f16ff7076b9379fcd838d3ce",

          "decoded_sha256": "8268bef0af966d65c2cc365371c552e5a3589a5604bc543a2db7cc1db137dca1",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 48,

          "bytes": 28858,

          "v1": 81.782898,

          "neg": 73.285499,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "2b370321fdd15068933fbf915eab4474f22d268163a9e59ba93f4f63fb57d8a6",

          "result_sha256": "577909c9fae7d4a77d706e46b6dc20fff35c61778d5feb69f47d3995f0a064e1",

          "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

          "stream_sha256": "63d907a0e3a71e8586a3116c566c53b0fb0238838149fb1a25da8e4292951b9d",

          "decoded_sha256": "0d96037dbd5761a6b06858d66ea57fef5352828453092dc5f9ae256c9c8c88ff",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 40,

          "bytes": 46361,

          "v1": 88.553228,

          "neg": 83.822789,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "2b370321fdd15068933fbf915eab4474f22d268163a9e59ba93f4f63fb57d8a6",

          "result_sha256": "e4c74ed679dcdbdb954bbbb2e8182df8789434141a8a086749faef260bb684ce",

          "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

          "stream_sha256": "91b9719fdb8083efaae484bbbe25fc25174ad5b9188378b7fd84182d06aa5389",

          "decoded_sha256": "b160fbfdc14fe6f4f0b3c91c9e28cbc1e0fc47556b9ca73f3b64113b6ad7be06",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 34,

          "bytes": 64587,

          "v1": 92.823646,

          "neg": 89.067221,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "2b370321fdd15068933fbf915eab4474f22d268163a9e59ba93f4f63fb57d8a6",

          "result_sha256": "a8ca98b307390828f7e64fd146c777708fb5da9216574bbd21d7042e022528c6",

          "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

          "stream_sha256": "68549d32ce83f910c56d70b16219a7c14f716754bf659f0f3e677ea25095f048",

          "decoded_sha256": "b863ef436fca67d5cfae44072cc2af2e7a553959805005a064f2858cc79d6b46",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 32,

          "bytes": 72176,

          "v1": 93.592912,

          "neg": 90.054771,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "2b370321fdd15068933fbf915eab4474f22d268163a9e59ba93f4f63fb57d8a6",

          "result_sha256": "b6b7f874a54ccbce759e1179c4ea7fea26c1f021321f89ddc1f2d0d5f9948c0f",

          "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

          "stream_sha256": "45a57ad6ca1927a7c4f77f283c1ee461eb79b7fbd1daffc988ff579ff06e10ce",

          "decoded_sha256": "ca6f93b0604451bcc80824a91da137f2b39c037719da80b3cfea5d1ec80a7924",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 28,

          "bytes": 88487,

          "v1": 95.081529,

          "neg": 91.938746,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "2b370321fdd15068933fbf915eab4474f22d268163a9e59ba93f4f63fb57d8a6",

          "result_sha256": "226d127dc278b84923f87577fe3441a0c7ee16742378450d15869262730571da",

          "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

          "stream_sha256": "7f4a5889b25c91a736acd2e0b031da22105fabdd6205d45ee46d8e3f0af01d18",

          "decoded_sha256": "bb7aa783afcd853d7c1c42269d2c7ff70a6013e3521b44fe4a20b2df580c56f1",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 26,

          "bytes": 98266,

          "v1": 95.741172,

          "neg": 92.663098,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "2b370321fdd15068933fbf915eab4474f22d268163a9e59ba93f4f63fb57d8a6",

          "result_sha256": "25a666437b94a21ec69f6b710d2e9aac88b2d57685b9a8b2eadbdb5c54b8a260",

          "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

          "stream_sha256": "af2f01c309f36daa87bfd652f9730d596fa194a1d9fcf22fe6763234f94b9e6b",

          "decoded_sha256": "64e016ac8cda1f55fe64515b35cb8b1f804044122b5891b9feb4f661de7fb944",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 24,

          "bytes": 110261,

          "v1": 96.402996,

          "neg": 93.29283,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "2b370321fdd15068933fbf915eab4474f22d268163a9e59ba93f4f63fb57d8a6",

          "result_sha256": "706e3218bcfdacf8e3b2b9ad247f7a397e9c04726c3c0b574c5cb073cb6ce846",

          "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

          "stream_sha256": "5de7cb04cc1876bc3de11962cdd4e7813e98c73b1ba101d8a1f60fa5c0a7b15d",

          "decoded_sha256": "7b6be105869dd95a092cb47419cf330daeccd311f34ccc5715364fee90468716",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 20,

          "bytes": 128865,

          "v1": 96.921231,

          "neg": 94.06776,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "2b370321fdd15068933fbf915eab4474f22d268163a9e59ba93f4f63fb57d8a6",

          "result_sha256": "f891035f5a39c328b51a8d8aa06d1b324ea7022ee14c65e4a85b012aa178049a",

          "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

          "stream_sha256": "82d5cb2fdb35b0b96d3ec23da659e0e82314393d668ca97a04f73ed80af26211",

          "decoded_sha256": "1c1315fdfa3189cc9439c2df0bd432265d98765d949550f4059b9d3346e74520",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 16,

          "bytes": 157136,

          "v1": 97.759618,

          "neg": 94.81013,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "2b370321fdd15068933fbf915eab4474f22d268163a9e59ba93f4f63fb57d8a6",

          "result_sha256": "ae27c500b907711c1eea1ccedd11a86ff5827fa4ac7582b37bcd450d4a9737e3",

          "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

          "stream_sha256": "cd96d6190b5ad845751391ecfea944b78e2d349d8a6bb2ba387f54d711faad52",

          "decoded_sha256": "8ba6e28362c21a2b1799d0ee6f9c292c53d0b6831ebae32fcae17c333a8f4f4f",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        }

      ],

      "aom_coverage": {

        "points": 10,

        "inside_93_97": 5,

        "below_93": true,

        "above_97": true,

        "eligible": true

      },

      "previous_best": [

        {

          "status": "COMPLETE_GLOBAL_RESEARCH_COMPARISON",

          "coverage": {

            "native": {

              "points": 7,

              "inside_93_97": 5,

              "below_93": true,

              "above_97": true,

              "eligible": true

            },

            "aom": {

              "points": 10,

              "inside_93_97": 5,

              "below_93": true,

              "above_97": true,

              "eligible": true

            }

          },

          "points": [

            {

              "q": 120,

              "bytes": 70703,

              "v1": 92.689871,

              "neg": 89.200126,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "94e64b2902dfab0aa0325f0af5d85731b3db8965a52fcfe16a85c086204b11a2",

              "configuration_key": "f88a35364628da64c1f095a4313d05279f220dab087efd285f56f23f02b8b3a7",

              "result_sha256": "8fd13df0d5f0e0c3f05837992c90131fe02b23c732e3526939ae0b885167b685",

              "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

              "stream_sha256": "b2a35d81e6b5ed2af1e1433b8b7b38e6ae29209c6f51c5abbc3afd73b6547971",

              "decoded_sha256": "62751a76515594431b8eb520acdb2ee08b0151ef28580316ef750716d261af1e",

              "encoder_sha256": "706637df13b429e96950ef8975a6343724a0a1e5849ab4b4bfe898e1fc144159"

            },

            {

              "q": 114,

              "bytes": 81145,

              "v1": 93.897629,

              "neg": 90.539664,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "94e64b2902dfab0aa0325f0af5d85731b3db8965a52fcfe16a85c086204b11a2",

              "configuration_key": "f88a35364628da64c1f095a4313d05279f220dab087efd285f56f23f02b8b3a7",

              "result_sha256": "8aa22593395757cafd535e55c90f7284f20b266ffd3f02cb346990215cb40881",

              "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

              "stream_sha256": "de3ade1259cd8dafafd6882943a45bbeac658d5fb59ad3668c3501ac19516225",

              "decoded_sha256": "8677e86d7008aa2dc4ef6a705d1893e3f6a1723be500f61174f5630f3216801b",

              "encoder_sha256": "706637df13b429e96950ef8975a6343724a0a1e5849ab4b4bfe898e1fc144159"

            },

            {

              "q": 110,

              "bytes": 88894,

              "v1": 94.626269,

              "neg": 91.414589,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "94e64b2902dfab0aa0325f0af5d85731b3db8965a52fcfe16a85c086204b11a2",

              "configuration_key": "f88a35364628da64c1f095a4313d05279f220dab087efd285f56f23f02b8b3a7",

              "result_sha256": "5fd7ad063c71d2205abc5df862469f0d7cd76524681703d3287f6142bdbdca7f",

              "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

              "stream_sha256": "a3932b229e31a9eb7e730a63421d4d86905744f183aa354b8b12171cc119400f",

              "decoded_sha256": "2154a53fa8b0d9509eb9ba2c1a1eb2dcbe561a34db79cdb485596beb6d859c14",

              "encoder_sha256": "706637df13b429e96950ef8975a6343724a0a1e5849ab4b4bfe898e1fc144159"

            },

            {

              "q": 104,

              "bytes": 103279,

              "v1": 95.550368,

              "neg": 92.43968,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "94e64b2902dfab0aa0325f0af5d85731b3db8965a52fcfe16a85c086204b11a2",

              "configuration_key": "f88a35364628da64c1f095a4313d05279f220dab087efd285f56f23f02b8b3a7",

              "result_sha256": "0df7990cf1f37d394b17fe8d691149aafa8c5fa268a165b55f56e34eff94962d",

              "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

              "stream_sha256": "4281b5486b0d00981e2ffc346b5803cbfa3f14e0701d9dc7bc6c715deb2cfc14",

              "decoded_sha256": "130170a9b9cf1f66c73b280821a00fbef5a35b96bcb228ffc634bf76eb1c3f1e",

              "encoder_sha256": "706637df13b429e96950ef8975a6343724a0a1e5849ab4b4bfe898e1fc144159"

            },

            {

              "q": 98,

              "bytes": 119213,

              "v1": 96.248986,

              "neg": 93.327969,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "94e64b2902dfab0aa0325f0af5d85731b3db8965a52fcfe16a85c086204b11a2",

              "configuration_key": "f88a35364628da64c1f095a4313d05279f220dab087efd285f56f23f02b8b3a7",

              "result_sha256": "8a98ae18f9085b43a1a3ba6c87c220f260e1af31c87fe57f22ebadfa0f4a8f60",

              "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

              "stream_sha256": "462d419d5a37d205501b4584ad43a190e5638e1b2796e667029a18b794e112b6",

              "decoded_sha256": "35692d306295bec314737c1c43268e3ec9dd3ba1e763fab0b01816ee51305054",

              "encoder_sha256": "706637df13b429e96950ef8975a6343724a0a1e5849ab4b4bfe898e1fc144159"

            },

            {

              "q": 94,

              "bytes": 129517,

              "v1": 96.612533,

              "neg": 93.724986,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "94e64b2902dfab0aa0325f0af5d85731b3db8965a52fcfe16a85c086204b11a2",

              "configuration_key": "f88a35364628da64c1f095a4313d05279f220dab087efd285f56f23f02b8b3a7",

              "result_sha256": "a7ec08576c6bbf3cfab152df10389e748f3d487b4235639e5270bc975837d03e",

              "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

              "stream_sha256": "03756d55c8b0357ffcc5870c667f4c0b007100f0fcb48578c66f06e59bf19530",

              "decoded_sha256": "0b2a6cb8bda6b7024fcb53acfca57f99b64303aaa1291705df6a3550c2a3f9e3",

              "encoder_sha256": "706637df13b429e96950ef8975a6343724a0a1e5849ab4b4bfe898e1fc144159"

            },

            {

              "q": 88,

              "bytes": 150416,

              "v1": 97.238824,

              "neg": 94.342407,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "94e64b2902dfab0aa0325f0af5d85731b3db8965a52fcfe16a85c086204b11a2",

              "configuration_key": "f88a35364628da64c1f095a4313d05279f220dab087efd285f56f23f02b8b3a7",

              "result_sha256": "6b64d614640f055e9f9ad52f9bea8c08bbd6f6100db06a9d9fe4847ae4e938fa",

              "source_sha256": "21ff01932c5146f4dfed95f9023ef8c1689674afab2758433fdf2464235b116b",

              "stream_sha256": "201c6842a846b4e3ad210f302a05bcc108bff012002bcf71bf43a0697a3c0617",

              "decoded_sha256": "d9be7823f1171b9b5cd65558723408cb60db1f870c13876f71abe00025225dda",

              "encoder_sha256": "706637df13b429e96950ef8975a6343724a0a1e5849ab4b4bfe898e1fc144159"

            }

          ],

          "dual": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 1.0890108449084144,

              "worst_sample_ratio": 1.119053803864801

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 1.0888441327633729,

              "worst_sample_ratio": 1.11983430204361

            }

          },

          "primary_v1": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 1.0879547455962226,

              "neg_delta_at_selected_rate": {

                "mean": 0.0766989429303695,

                "worst_sample": -0.040062227069014966,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 1.119053803864801

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 1.088189453283323,

              "neg_delta_at_selected_rate": {

                "mean": 0.08369030327636506,

                "worst_sample": -0.038448011189686326,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 1.11983430204361

            }

          },

          "eligible_band_comparison": true,

          "local_quality_acceptance": "SEPARATE_FINAL_DECODED_GATE_REQUIRED"

        }

      ],

      "new_candidates": []

    },

    "live-1080-a": {

      "content": "Real",

      "dimensions": [

        1920,

        1080

      ],

      "input_bitdepth": 10,

      "original_bitdepth": 8,

      "color_limitations": "Color metadata incomplete; research-only, no certified HDR/SDR claim",

      "aom_points": [

        {

          "q": 56,

          "bytes": 5291,

          "v1": 54.68551,

          "neg": 52.436179,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "d273db05ad6b4204fd085745c5db08a467c9bc807aa34dbdd9fb43d4d75396b7",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "e6cd88603cb328ecd00757ecfef2e0e47f635baf08d72a92d569b6583ca850d9",

          "decoded_sha256": "bbaa9255551cd55d1dc6e45b6da41d4d36ab3bdd2d7e17b6ac6c497294d9f7a1",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 48,

          "bytes": 9653,

          "v1": 71.925344,

          "neg": 69.141903,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "fb9234f21a55012199116f0c754fbc33d0ee5c033c65e513cf1e6f98b3d5eda1",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "5cb4afa519c09701fd201278929df89aa795d027c0421c84d1db0d3d5738beff",

          "decoded_sha256": "ebc40bd25976d1006feef0274f83c400a486d7e3ba904e4c689f53fec9b61a23",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 40,

          "bytes": 17208,

          "v1": 82.438056,

          "neg": 80.208585,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "50add40f92258546a022e1098540f0c59a7c6a45b26f32ad24c7fe9d0265303f",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "158843873e3e3f6f42cd19fced629c7991cb5713e97a6968fac2c31bd3959ff8",

          "decoded_sha256": "e9d643c8720b6004862308dffd416333d0dcdda8e6dd889158b750fbc75a60cc",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 32,

          "bytes": 34628,

          "v1": 89.011457,

          "neg": 87.933811,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "4e82405f24384f0e0d74697b551aa5b339574fcc76f93c73018e84662f81efc6",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "2b73ece8c1ca784b2dea5c44a543a5647404db35a4f39548e6ae3a8969d3ef47",

          "decoded_sha256": "de846e9ceea0bd38f868dbe6aeeade6404b87c3934449a0373e3913818c1fb3f",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 26,

          "bytes": 53642,

          "v1": 92.76597,

          "neg": 91.303541,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "80cdfe3a72e9bb5e649ccdb8b8d3111599d9d37bd11ec4eaa12fe6fe2d04aae6",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "0ba3aa806a2d287b9afc6c45bea9465c0bfda2caf4aae171b88ca2e6edb7ee32",

          "decoded_sha256": "e32579da982bfe2209212126bcb40a99e6c14f471481bea8fc2165f5b6788c02",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 25,

          "bytes": 59863,

          "v1": 93.418421,

          "neg": 91.787268,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "6da2dc45babbed4ab50087966b919f72f3de6ce6a3dec7302016d096a12023ce",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "49d046986ad8a992518c433ed86047eb452c682be51e9728162f1d00941471f1",

          "decoded_sha256": "481c47278252e5af2a0950ccb8e051516d287229ec5b881c22acf1c5fda879b1",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 24,

          "bytes": 67488,

          "v1": 94.017038,

          "neg": 92.325053,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "c204758bb192777e0d1660057a0980607c9b0c708c5f4c495b299898aa312b8d",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "a9c736cfd4b3530a766ecc4e36cd88672a45ccd35552b65184edc246aec9656a",

          "decoded_sha256": "f276195dcf12444d2d95e6b6da44d21b64fd21c7ca021818c0cea37cea6e3a3f",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 23,

          "bytes": 73608,

          "v1": 94.149089,

          "neg": 92.494208,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "ff3201a7c72cbd08a2556278a2858e5879f9f14e62e83e63b770aeee0d6d1217",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "233779a338815e4f843aea54a4af1e7efc43dd1317bf15175d3d732a590ec4ec",

          "decoded_sha256": "c762202bcf04f49eac4e3d9b4738336ca3cbdd5d882b03eec044aa5faf8d35e8",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 22,

          "bytes": 79796,

          "v1": 94.623695,

          "neg": 92.825463,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "25b5d945758e2675e0e070d31334e5e2a5a903f36053c6f7a9c5616c15f13fd7",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "475264a003aa4fb9fb82d9908bdc0c4b8ecc5cf7f839d2c1d07e94cd37ad36b2",

          "decoded_sha256": "681120cf64d1ca986a264c0744f23662b8554e6db2c84db9479a86fc3df99879",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 20,

          "bytes": 94223,

          "v1": 94.994211,

          "neg": 93.272228,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "945b42d675f320d6509b506bc685e4eceb4fe8471f0ecb15a4794ebb0de45d88",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "760d20c90642ddbf920c0982610382bc25d306ead356aa1a05383819622f91df",

          "decoded_sha256": "5294915a41dc3b70fe9d0e22c685c9f8489f5ae403f3f64a0cde1490b7b54f02",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 16,

          "bytes": 131743,

          "v1": 96.546144,

          "neg": 94.162419,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "68ea1adbd42444674432410b827b851060b784665995adf06c6be2c917749d35",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "0f617010718cda8a3c8757ffa4200cf55bfd71d4a0d2b55def63a4dcd0fc6335",

          "decoded_sha256": "8574b4ad0c7fa2f2c4df679c2127177c71e507bf343bc86d331dd594efe993ba",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 12,

          "bytes": 168741,

          "v1": 97.94639,

          "neg": 95.154792,

          "dimensions": {

            "width": 1920,

            "height": 1080,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

            "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "a2377e06001729ad4738709bdb28334dca48803b523f5a1b423a19b4b86a6d99",

          "result_sha256": "970695e0702bd122c24e4ca3e5ec6fd3e2e6000bb7dc2ce91eedd15f9ac4220e",

          "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

          "stream_sha256": "84e6986791ca00ce3ba90e4222f4b607938a85dd31d44b093d608dff62e707cb",

          "decoded_sha256": "e2993afcf03dbf66e0a941f2fe4445e11d147966fd79d27467d7197ac2f602fe",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        }

      ],

      "aom_coverage": {

        "points": 12,

        "inside_93_97": 6,

        "below_93": true,

        "above_97": true,

        "eligible": true

      },

      "previous_best": [

        {

          "status": "COMPLETE_GLOBAL_RESEARCH_COMPARISON",

          "coverage": {

            "native": {

              "points": 13,

              "inside_93_97": 10,

              "below_93": true,

              "above_97": true,

              "eligible": true

            },

            "aom": {

              "points": 12,

              "inside_93_97": 6,

              "below_93": true,

              "above_97": true,

              "eligible": true

            }

          },

          "points": [

            {

              "q": 106,

              "bytes": 48379,

              "v1": 92.274475,

              "neg": 91.385295,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "b9ab16478e50807c1c236bdc1ad3e0ba443d2e48efb0b63b16cade3592ccac9f",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "37f29f26ab168705e8ae54e9c73a4f4869d85cfca0907042648898fb4f546b56",

              "decoded_sha256": "acf64e00d48277a9eb1ac8dbd5fa1b30f207eef1e2b3ef9b336d413fd4af4c32",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 104,

              "bytes": 51822,

              "v1": 92.777984,

              "neg": 91.872785,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "4ddfe537edbb6d92407a1e58901fe0c83679b2a3ac9d10ad9ad2b2253dfb02a4",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "0b6cef13735e4c3016084f0b1a789a0e99c7974e4279e4e4172cd08714e17e9b",

              "decoded_sha256": "d60200ca08be93503bae06b12af8e5d7736a73118a901a0b668cc9f204a37d3a",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 102,

              "bytes": 54305,

              "v1": 93.327609,

              "neg": 92.135316,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "869e25048c09517a6366aaf9110459278aa824c7ea1a2a9d5d2adac6fb21f165",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "9445424c06a70da783502c3ebe0f6d3b229530d7cac8aa41e27ba0ec00985cee",

              "decoded_sha256": "00861dcde03df46159f209b5901ca367c378347763ad0c733be1e9f011ea2cc0",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 101,

              "bytes": 57375,

              "v1": 93.431597,

              "neg": 92.416956,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "3ed78184b0376d1965139a111c73e2662fc7e13492994d60ce9de4f6f7e54101",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "e830bd3e0c5403cc4999e06de1d9eba89e8565176eb48bcc9bb78e915e7c0d28",

              "decoded_sha256": "a26b2949defbdc483628242ff7e792da6b877626e4f668c67697ce7501819d5a",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 100,

              "bytes": 59206,

              "v1": 93.549637,

              "neg": 92.53512,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "edaf3298158953294bab63458ae9cbe726e7ef3d3a755ec3a0405c279788f96f",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "510c86429db1cb4a54e68a98e9058101e9039a4cf479af8211faadc25d30e479",

              "decoded_sha256": "df7dc754204b4e74d152c4972fcb0c48aa2a5a2644d028a6d0df2ccf1701737e",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 99,

              "bytes": 60996,

              "v1": 94.037543,

              "neg": 92.662787,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "31bf10024f3380bd1c5dc972025e1f6aaad864a88cca7c2b176f621215fd4d72",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "1f8f8df023dc273efad85ed922c4eaf7229f225b3f8b84e3f268f5078c871d7d",

              "decoded_sha256": "5cea713ca097a9f57fab521370b1f2e0600789fbd67f86b4a745fd1511a01a17",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 98,

              "bytes": 65635,

              "v1": 94.523825,

              "neg": 92.899686,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "a116b5f24fa5cf536cf28932984dc32b47fcbc2f8f5087bf7bc80b3f8193efba",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "67f471019d3f6ed4beaf1210e7e447d5cad6ecf9a9149bcfdfbe27758167619d",

              "decoded_sha256": "81dfb12130413782ea6c47a22f85d206ea79d56b26ce728762de2d6bc1107796",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 96,

              "bytes": 70604,

              "v1": 94.701499,

              "neg": 93.132214,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "ec27b62d60ca769983a5e02d2c6ecc59f03e1508c1430d92d905e0baee0a8b09",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "e4763e730c9b905badffaf5a7cb02754c477785c9e8ef6464aadc73def9a648d",

              "decoded_sha256": "f90148d15ae9269e3a01d6b7db6c6bb119f1ff5d6100efb92fa76dec4c6474de",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 94,

              "bytes": 79182,

              "v1": 95.328249,

              "neg": 93.418437,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "dc39f1828a3d7bb9da8a8b3201a5d0771e79a455970cec705f305aeaf8a913bd",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "996a9846e40a20f094343907242c1bb45f6a56f80f4715a3b898bd42d35e9b3a",

              "decoded_sha256": "65c94e2bdd4d8e4a4b660b222349e6d738398b033d96b73a48df33464918b596",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 92,

              "bytes": 88780,

              "v1": 95.401826,

              "neg": 93.635571,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "0ab7ecacdd5d153ca2c6cdb6cca2fe162c3efac711239783f2e620c744b77f33",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "2240c6b7a5a6d15b09cb473cad19ead7a53483588b8e43e44c1c9eb32e2d50b2",

              "decoded_sha256": "e9fbc715103d44b937156fc1767c3484368980816a054fbaf7ae4a02825106a2",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 88,

              "bytes": 113283,

              "v1": 96.307769,

              "neg": 94.112546,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "5560aff8420cf32430ef5106fe4a0285a0b8e425cbc159829ed5651fd3daa28d",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "a55ff565baf7550a4947f63a1ad1501a3a872d7010277a6197db795168ecee5d",

              "decoded_sha256": "e37c33ebce9a3657162c8225ae3513bf7bc7e37a4d49d74317c82aa1f7330e65",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 84,

              "bytes": 133657,

              "v1": 97.08612,

              "neg": 94.480743,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "628566011285f8dda1c34205e16fe022d30fc38e935b863109e07dcc8ac4adea",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "a37b4260283bb6c07be8e88391fe3889a9c617500b66c22e700795064264f2c7",

              "decoded_sha256": "98c607a8f9ebea055e26f294085ce0be0d85b7e0cbbe5f69f637386e9b956cda",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            },

            {

              "q": 82,

              "bytes": 143140,

              "v1": 96.944332,

              "neg": 94.648894,

              "dimensions": {

                "width": 1920,

                "height": 1080,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "cdb62c255f17a17b6dc2b97fba5429c4b303aa5523a8b0d0316d8a112cfd893f",

                "neg": "d93838a11ca89173a420e7cc0e44ea30635459151c4b0955b2ae129826780b0f"

              },

              "source_manifest_sha256": "557a6eedd61c33a85fd440ef4df008524ccfa9d82792c15c8292dee30907d192",

              "configuration_key": "b0f7a21adb158fb8a5d9996d9dbee3b3ee9fdc0a11a614a5886ccd72f2161bf9",

              "result_sha256": "6fc6434700d3a9950699ddbc8018ebd7486eb70ca6df7c0851d2c2c1795cda40",

              "source_sha256": "2bc4b47f61a45a77c363f521bc809cf8a63f169cee2902ea75025b2f4da7776a",

              "stream_sha256": "2f94d2bbb728159502b56c3a70ff0cb3a7db64e81c5e995d9b4ec81a15f04c8f",

              "decoded_sha256": "548336ce8c150a81e2639735c0534d9df53d2403ac7c91fef9716ba7696ed091",

              "encoder_sha256": "d381d26319ad4ac3d46b3341cc841a6e0f1d30f12b5fc1c517e39f9533cb0401"

            }

          ],

          "dual": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 0.8809283606235171,

              "worst_sample_ratio": 0.9665559317046447

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 0.8818384606320536,

              "worst_sample_ratio": 0.9633232535597371

            }

          },

          "primary_v1": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 0.8771824382663816,

              "neg_delta_at_selected_rate": {

                "mean": 0.1912737992643549,

                "worst_sample": -0.10043245601077899,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 0.9665559317046447

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 0.8805654686209163,

              "neg_delta_at_selected_rate": {

                "mean": 0.1965609645347287,

                "worst_sample": -0.045015793123511116,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 0.9633232535597371

            }

          },

          "eligible_band_comparison": true,

          "local_quality_acceptance": "SEPARATE_FINAL_DECODED_GATE_REQUIRED"

        }

      ],

      "new_candidates": []

    },

    "animation-2160-a": {

      "content": "2D",

      "dimensions": [

        3840,

        2160

      ],

      "input_bitdepth": 10,

      "original_bitdepth": 10,

      "color_limitations": "Color metadata incomplete; research-only, no certified HDR/SDR claim",

      "aom_points": [

        {

          "q": 34,

          "bytes": 18599,

          "v1": 92.830089,

          "neg": 97.422608,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "8ed0543877b1d9b9bf66da9b5c3098ed2844c717baf29172291c031fd05d215f",

          "result_sha256": "fc5bb20d05c64936f7662f760182936e72756c4835dd3e39c993f36435f123bb",

          "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

          "stream_sha256": "6cfc049aa0933ff44b06c9c91bcae2344f3e7116c20d34a67dfa9247431615fb",

          "decoded_sha256": "a2abb05e81917bb856b9abef12e3f4610fc1a7a7fdac41db585ee26810aa0f4f",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 32,

          "bytes": 20869,

          "v1": 93.400277,

          "neg": 97.830319,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "8ed0543877b1d9b9bf66da9b5c3098ed2844c717baf29172291c031fd05d215f",

          "result_sha256": "597eaf1878f05822ff528933f14baf7c4b59ccb2ff5266dc86a8589a38d1f6fc",

          "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

          "stream_sha256": "8fd09fcf0a238176822498c9ec3d0634046bd12e48bfa2f0f6555745c46bfe1d",

          "decoded_sha256": "2155fd89f5ac4552335b0d133bc4dd284df2eb3351c8020b1d9f8e9d58017f23",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 28,

          "bytes": 24805,

          "v1": 94.321156,

          "neg": 98.397904,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "8ed0543877b1d9b9bf66da9b5c3098ed2844c717baf29172291c031fd05d215f",

          "result_sha256": "69f7b50841271287e8580a584bd94fa04bff1b2479363666b877f044ec3cba08",

          "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

          "stream_sha256": "71177f8ef40a7579a426e0a81ec5dfe01b316518fdcf6cd05928750d95d4298a",

          "decoded_sha256": "cc6ad9baa06e30e28000b2dadb8463afdb2083164481386bffe33dc26000de5c",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 24,

          "bytes": 30088,

          "v1": 95.294846,

          "neg": 98.952113,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "8ed0543877b1d9b9bf66da9b5c3098ed2844c717baf29172291c031fd05d215f",

          "result_sha256": "bcc34948cd5775c3229a2cc333cbef3682dbfb81e029754aa6d363a95ef64338",

          "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

          "stream_sha256": "362fe59cb2db377e652c244abe457afdc21d099674424955d0082cc0e235eb11",

          "decoded_sha256": "348808b87c89e3694a5757e58a72c5b256084ecf74b8d78b34fef4f046f2d1e1",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 20,

          "bytes": 34366,

          "v1": 96.069702,

          "neg": 99.30489,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "8ed0543877b1d9b9bf66da9b5c3098ed2844c717baf29172291c031fd05d215f",

          "result_sha256": "cb222096ea464c5f026e93f8f03a2dfcf45b45d4a92c538c0964a8abaeae833e",

          "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

          "stream_sha256": "9d658fb1c48180e86bbfa3f4d9cd2cd937bdf7878204394675a401fa376e3277",

          "decoded_sha256": "c750a1f0d5e225ae70b175b23508dcc65d7d58e3dc01877395357fb571944c90",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 16,

          "bytes": 40308,

          "v1": 96.64718,

          "neg": 99.609002,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "8ed0543877b1d9b9bf66da9b5c3098ed2844c717baf29172291c031fd05d215f",

          "result_sha256": "87a14e4577777630ce7bdea280c599877da81633170e67a77475b6e1b882517a",

          "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

          "stream_sha256": "8011584897b592c32fa97a86ea95bb164ac80adfc66a825c966903f8c2e76c2f",

          "decoded_sha256": "28a36eae9cabccf41442d475ec934d81317c5a90755734eeae2657104b82fcc8",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 12,

          "bytes": 50712,

          "v1": 97.145924,

          "neg": 99.925756,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "8ed0543877b1d9b9bf66da9b5c3098ed2844c717baf29172291c031fd05d215f",

          "result_sha256": "f6918517f268bcdcd6b5e949158668054d9d669f294c8a37e141302b9a2a98ab",

          "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

          "stream_sha256": "00af5e1f453f64b7bb0333e8f04c14bb60c33329214e44945e90a54ab08a5533",

          "decoded_sha256": "137001cdd456a2308cd943eefb51dd46f4847d22ee737227217b385369ebb58d",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        }

      ],

      "aom_coverage": {

        "points": 7,

        "inside_93_97": 5,

        "below_93": true,

        "above_97": true,

        "eligible": true

      },

      "previous_best": [

        {

          "status": "COMPLETE_GLOBAL_RESEARCH_COMPARISON",

          "coverage": {

            "native": {

              "points": 8,

              "inside_93_97": 6,

              "below_93": true,

              "above_97": true,

              "eligible": true

            },

            "aom": {

              "points": 7,

              "inside_93_97": 5,

              "below_93": true,

              "above_97": true,

              "eligible": true

            }

          },

          "points": [

            {

              "q": 120,

              "bytes": 20135,

              "v1": 92.734477,

              "neg": 97.352909,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "f3f6593a5b87648a1a794bcd4b51ec53a8af17aceedd6127ec82cb754a8748c3",

              "result_sha256": "5fbdc5a945ec81b147459ce27906883322f550dc6b66b0492d5983a910eab0dc",

              "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

              "stream_sha256": "939b2a836bc386866636d1e1a42e68316589177a54d451085dfdb861a34cb257",

              "decoded_sha256": "423f7e2afc3f8344ff04ef3a495de25c81ba09ee562c58ea00039754dd2f15b7",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 112,

              "bytes": 23792,

              "v1": 94.016831,

              "neg": 98.039176,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "f3f6593a5b87648a1a794bcd4b51ec53a8af17aceedd6127ec82cb754a8748c3",

              "result_sha256": "1e51e8a0e7cc6a215a17e8bcb02fcc3969aaa27f430a75bf404c8cd5f82f026e",

              "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

              "stream_sha256": "68323df83c6b8512da8d100af4f241c2ae2b9977f495c10622c4831ee27d9dd0",

              "decoded_sha256": "8e27b1430ae04b1931670c48550a498d62a796ee9655e4296e005599382fdabc",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 108,

              "bytes": 25986,

              "v1": 94.32885,

              "neg": 98.328762,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "f3f6593a5b87648a1a794bcd4b51ec53a8af17aceedd6127ec82cb754a8748c3",

              "result_sha256": "c22da896427bfd44bf90d91ba80769eb282993a5126dd8c63a8d891a03346bae",

              "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

              "stream_sha256": "d1008d3c73ff28c63799ebc066791233603439697d805ac154951b5c011d5962",

              "decoded_sha256": "7bbdc62c59e0d6b04ce3cc24db5c14056cdf09178d0bde6c0730a8a110aadaaa",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 104,

              "bytes": 28369,

              "v1": 94.822787,

              "neg": 98.61588,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "f3f6593a5b87648a1a794bcd4b51ec53a8af17aceedd6127ec82cb754a8748c3",

              "result_sha256": "5642f1bfe5966d027907c5e40f1afd4c8c48346805c590c9727cca5286d2d3af",

              "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

              "stream_sha256": "23face6d98203ed4dcd2928d01677bb983f4c937cfe4534e777ca086bf414f16",

              "decoded_sha256": "767501f7fb4ea6841c46cf1a5d3828f90e2ac7c6100153fdd96d8f1a02610405",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 96,

              "bytes": 33431,

              "v1": 95.901134,

              "neg": 99.110772,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "f3f6593a5b87648a1a794bcd4b51ec53a8af17aceedd6127ec82cb754a8748c3",

              "result_sha256": "7e0db0510b66cb27898c73b88f2593b7c5c78d240c5551614088b274726c8dfe",

              "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

              "stream_sha256": "0d7a12bd0450c5e863270370df797a082073c280a5b9aadee0a4b35834892aec",

              "decoded_sha256": "ce553b9772e6a5af456dc44fa21f33235b51ac9cf46ab0f3827629b9165bf665",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 88,

              "bytes": 39664,

              "v1": 96.445718,

              "neg": 99.449541,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "f3f6593a5b87648a1a794bcd4b51ec53a8af17aceedd6127ec82cb754a8748c3",

              "result_sha256": "788b2901a20921be9795743f46261ccc6fb26bfbc9197098302456c3852bdc9a",

              "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

              "stream_sha256": "54667bab3741e9640ed63e46dbe06b3977d861b46674113a3230478f41017495",

              "decoded_sha256": "8d0742144eeb1d1d08faaa9d8edc01cfec137c5fa70fd82e8693da26a3ab1a9e",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 80,

              "bytes": 47657,

              "v1": 96.947316,

              "neg": 99.722499,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "f3f6593a5b87648a1a794bcd4b51ec53a8af17aceedd6127ec82cb754a8748c3",

              "result_sha256": "92de337dfdeea0fe86ac2ace99553755d4f4d16a950f4943134ab88ca231a420",

              "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

              "stream_sha256": "bd6f4fd3f40ad555e27446054e91cf031243560af12cf6d44cf64d16b5b6f339",

              "decoded_sha256": "a1abe93d12f4d784c09810c7c77680ceca1220123b90227470dc858c63d5a613",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 72,

              "bytes": 57184,

              "v1": 97.323725,

              "neg": 99.922152,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "f3f6593a5b87648a1a794bcd4b51ec53a8af17aceedd6127ec82cb754a8748c3",

              "result_sha256": "71fa6c994f7b32aa03d2043ddcd0bc9fb881576a0dbd43cb2f2526d166ecccff",

              "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

              "stream_sha256": "775b70ce05b6e23896135f835a3a4a493ac9d19703c652d687668e481121fe5a",

              "decoded_sha256": "f93911cb1eee8bb17f223db46452e589e1134aeda18a9f706105a86b624e0636",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            }

          ],

          "dual": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 1.0696088293474348,

              "worst_sample_ratio": 1.1136575246106573

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 1.0705959777132263,

              "worst_sample_ratio": 1.1118063423591265

            }

          },

          "primary_v1": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 1.0300216827458182,

              "neg_delta_at_selected_rate": {

                "mean": -0.10775461089404514,

                "worst_sample": -0.1944275019383923,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 1.0733660586343703

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 1.0330758744936916,

              "neg_delta_at_selected_rate": {

                "mean": -0.09777776406967531,

                "worst_sample": -0.1710082224564644,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 1.0828452340301904

            }

          },

          "eligible_band_comparison": true,

          "local_quality_acceptance": "SEPARATE_FINAL_DECODED_GATE_REQUIRED"

        }

      ],

      "new_candidates": [

        {

          "status": "INCOMPLETE_GLOBAL_COMPARISON",

          "coverage": {

            "native": {

              "points": 3,

              "inside_93_97": 3,

              "below_93": false,

              "above_97": false,

              "eligible": false

            },

            "aom": {

              "points": 7,

              "inside_93_97": 5,

              "below_93": true,

              "above_97": true,

              "eligible": true

            }

          },

          "points": [

            {

              "q": 120,

              "bytes": 21026,

              "v1": 93.4142,

              "neg": 97.577839,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "149dcd9d441c67742768258f03cd7c46d65f802e6f39613c29967d848af7bca4",

              "configuration_key": "d8e2ebb314f0685b56c7acaacc5c601a0e0ade5d06c5d99b6927a9385ca24be2",

              "result_sha256": "23923d1d5f90ae5a575645eed272d32e263bd3896532c029bb4d00a373a1a6ad",

              "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

              "stream_sha256": "fe78e46d482ea7aa10bbd9033cb725f0a9001a791528040e2be84de7975522bd",

              "decoded_sha256": "fa45f0717596d7058008c78a9d5f78ffa8f8e05aa88c2a2a84e2673b0ceeb664",

              "encoder_sha256": "757253db7c38b5a22d1918d05a9cd6416c060e1c4f4f2b750dfb7a74943c275c"

            },

            {

              "q": 104,

              "bytes": 29694,

              "v1": 95.279973,

              "neg": 98.766922,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "149dcd9d441c67742768258f03cd7c46d65f802e6f39613c29967d848af7bca4",

              "configuration_key": "d8e2ebb314f0685b56c7acaacc5c601a0e0ade5d06c5d99b6927a9385ca24be2",

              "result_sha256": "54e73c541a62c4e980aaaa7bff4957009185a7a0e60b8bcad686ec9ac38bec67",

              "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

              "stream_sha256": "e689a332d83bf08723a9f3d53d27e02a97a2725b5fed339755c1d975ac072d06",

              "decoded_sha256": "489722dbf5c6325486550b01b514d9d92a35fa1442061eac7220ae182a8267bf",

              "encoder_sha256": "757253db7c38b5a22d1918d05a9cd6416c060e1c4f4f2b750dfb7a74943c275c"

            },

            {

              "q": 88,

              "bytes": 41745,

              "v1": 96.49026,

              "neg": 99.498601,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "149dcd9d441c67742768258f03cd7c46d65f802e6f39613c29967d848af7bca4",

              "configuration_key": "d8e2ebb314f0685b56c7acaacc5c601a0e0ade5d06c5d99b6927a9385ca24be2",

              "result_sha256": "adee1c5532769c709d603cbb8d75689227c81f6cff5b4cd3575df1278a576c00",

              "source_sha256": "8392ba3c33d0f359e27b17d96e19874e62dc735e2f11bdcf59527d15cf8e6a13",

              "stream_sha256": "6245d47c4800a7e4bf7854ae432be06d4ca8328aeefa1a079b38c2cf6c8de40c",

              "decoded_sha256": "3996015880c6199b8c42d77664ebb879148d4b7485ccc558b569445185e85d31",

              "encoder_sha256": "757253db7c38b5a22d1918d05a9cd6416c060e1c4f4f2b750dfb7a74943c275c"

            }

          ],

          "dual": {

            "pchip": {

              "full_coverage": false,

              "worst_sample_ratio": null

            },

            "linear": {

              "full_coverage": false,

              "worst_sample_ratio": null

            }

          },

          "primary_v1": {

            "pchip": {

              "full_coverage": false,

              "worst_sample_ratio": null

            },

            "linear": {

              "full_coverage": false,

              "worst_sample_ratio": null

            }

          },

          "eligible_band_comparison": false,

          "local_quality_acceptance": "SEPARATE_FINAL_DECODED_GATE_REQUIRED"

        }

      ]

    },

    "rendered-2160-a": {

      "content": "3D",

      "dimensions": [

        3840,

        2160

      ],

      "input_bitdepth": 10,

      "original_bitdepth": 10,

      "color_limitations": "Color metadata incomplete; research-only, no certified HDR/SDR claim",

      "aom_points": [

        {

          "q": 36,

          "bytes": 31249,

          "v1": 91.712456,

          "neg": 95.113819,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "38dad76587973ed5972af6a3af27e2943c81e8f9bff1564a5837bf24e03f695c",

          "result_sha256": "11a4a79a185644e644f9ff2300876a6cfa3515b111745e681b2b542775cd53c8",

          "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

          "stream_sha256": "84c37ac9dea991ac7485c9e7b6d58d196722184c1018d626597319f0712c35fd",

          "decoded_sha256": "f7f606c8fc3c1c6f796cbead2b1df51dc5a80f09a482a5c6a58c79c716293f69",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 34,

          "bytes": 35106,

          "v1": 93.037408,

          "neg": 95.870203,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "38dad76587973ed5972af6a3af27e2943c81e8f9bff1564a5837bf24e03f695c",

          "result_sha256": "aab4d9ac562a4866b8c0047f3f0928fd980d3f3c9ff9201817994f1ba06b0cc4",

          "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

          "stream_sha256": "de89f1745663b876f1b1869251915fc01af21986187e72967d8781109c9a8471",

          "decoded_sha256": "b3eb09a4da387e58cfa1bc305fd58742d6550ddfa3aa395a8504aeec6610a2c5",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 32,

          "bytes": 40535,

          "v1": 93.513258,

          "neg": 96.426057,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "38dad76587973ed5972af6a3af27e2943c81e8f9bff1564a5837bf24e03f695c",

          "result_sha256": "d5b0ea475e65ea9b0cb77c40be0c93fcaed7f71dd0c3d10a8fc34046afe2c844",

          "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

          "stream_sha256": "92b782fb18c5cf06644e9a36fe436ad05a00da55d0d8cbcee0e38c94f1d9b8a0",

          "decoded_sha256": "407f5dd868de1cc56fee87d909f71a77df20e54b813926012a5f72855267ba5c",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 30,

          "bytes": 45542,

          "v1": 94.668843,

          "neg": 96.996044,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "38dad76587973ed5972af6a3af27e2943c81e8f9bff1564a5837bf24e03f695c",

          "result_sha256": "3652aa0ae21d51474fe0f7aacc3bcdef08fd388c8e7eed0080f401f4d92538a8",

          "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

          "stream_sha256": "07ead65e1f4ae177555f4ea0e97eaed6718b717deeae939fff92924e5d5442ab",

          "decoded_sha256": "68a93b1f92b1edb54adf72639d207af003706cd35b78f4bc365d43f05619273a",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 28,

          "bytes": 49348,

          "v1": 95.19185,

          "neg": 97.360169,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "38dad76587973ed5972af6a3af27e2943c81e8f9bff1564a5837bf24e03f695c",

          "result_sha256": "5debfe6cf5568bdcc45184752133f6524e50df7a2429ce0a57aa2c9959cf1cca",

          "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

          "stream_sha256": "356c3afdc6f79779d33c7e2cb071b1f3789d6262011c9d449023a44acf1b4acb",

          "decoded_sha256": "f5edb91218c443a74e64975eeba62512d19e08de7082d931eabb7f46862cffb0",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 26,

          "bytes": 54398,

          "v1": 95.790982,

          "neg": 97.705299,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "38dad76587973ed5972af6a3af27e2943c81e8f9bff1564a5837bf24e03f695c",

          "result_sha256": "3f27c409c05dff236a8ca52af755757e446ba59de8ec9d859c8d7d7cfe662e61",

          "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

          "stream_sha256": "5842db0c2999e69d28f5dbfd60dd248e2ce5ff7888b1dc7d82b597965d95a316",

          "decoded_sha256": "5738da8ab892036e0185df99b89e79e0ccc85f20d791d71cf7874d72ab445a7d",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 24,

          "bytes": 61114,

          "v1": 96.524961,

          "neg": 98.101618,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "38dad76587973ed5972af6a3af27e2943c81e8f9bff1564a5837bf24e03f695c",

          "result_sha256": "c1b5e9aae7a207d63ec9a763ca70c9c7caf4b3fd5868c1de949b1586c3a6672b",

          "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

          "stream_sha256": "02a4378ed39ef937709d2da6ab3311e0237ffbeed7cc4cb74d350d63bbf19527",

          "decoded_sha256": "b02d666bb6fedea22a7bc6d8bdd8dc68414c7b12359fdc12d5396ceba4af689f",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 22,

          "bytes": 66042,

          "v1": 96.758407,

          "neg": 98.325883,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "38dad76587973ed5972af6a3af27e2943c81e8f9bff1564a5837bf24e03f695c",

          "result_sha256": "c2f5248054a7829b6030dc060f037c7b0677f0ca4e0d0df684aef24a747b1f55",

          "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

          "stream_sha256": "3e9ca69d22e4548c423ac69c5eb811b0a2abfa1c4cc9dabd6ff609949313cc21",

          "decoded_sha256": "c69fd1df969e9b95b1f115b78e6282e075b13343e80a699c673f887207b99018",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 20,

          "bytes": 71210,

          "v1": 97.075499,

          "neg": 98.518372,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "38dad76587973ed5972af6a3af27e2943c81e8f9bff1564a5837bf24e03f695c",

          "result_sha256": "0ddb83cabaee8181ce985dec9771821af6eb33e98767be2b4b1b681666fc4956",

          "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

          "stream_sha256": "1857f3f6293573f6aac48da84a21913039f6dfecdbbc1cc214fbe55a9feeee2c",

          "decoded_sha256": "06869fe710aa9603f70456c8755fe31fc5e7d8a8f20d52b8bb0757b43fed539d",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 16,

          "bytes": 86954,

          "v1": 97.866234,

          "neg": 98.93613,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "38dad76587973ed5972af6a3af27e2943c81e8f9bff1564a5837bf24e03f695c",

          "result_sha256": "942a51319b4e165f2317b9ba4a9cd3a45b2b8cb2136f3dbfbe19ef1f0bbbc699",

          "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

          "stream_sha256": "453cad62a10f7ea17ae0660570a2913d354fea64a2d04133f504f77e70870064",

          "decoded_sha256": "bd8809281e3cdabd81fdacc09a46ae6fc0d33704db1811755e059e84dccbf20d",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        }

      ],

      "aom_coverage": {

        "points": 10,

        "inside_93_97": 7,

        "below_93": true,

        "above_97": true,

        "eligible": true

      },

      "previous_best": [

        {

          "status": "COMPLETE_GLOBAL_RESEARCH_COMPARISON",

          "coverage": {

            "native": {

              "points": 9,

              "inside_93_97": 5,

              "below_93": true,

              "above_97": true,

              "eligible": true

            },

            "aom": {

              "points": 10,

              "inside_93_97": 7,

              "below_93": true,

              "above_97": true,

              "eligible": true

            }

          },

          "points": [

            {

              "q": 136,

              "bytes": 26692,

              "v1": 89.136672,

              "neg": 93.744825,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "738d9eff098a9c1c63d9a2bc85c9e112dae14a8fff1eb1342035be9cc99c5f2b",

              "result_sha256": "31f0824602aa64d7fb21725e77a2eb62a4a6e53d1ba66ed309d5a3defff81f62",

              "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

              "stream_sha256": "3142f14fe52b68f8c0e495d931b4ff8c528ee56864d92043db8f4b3065f0c8b1",

              "decoded_sha256": "590b755b87f80d5b68c693d701238f773339b2244c1e9608c4ded38ad1508a5b",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 124,

              "bytes": 35180,

              "v1": 92.298481,

              "neg": 95.656765,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "738d9eff098a9c1c63d9a2bc85c9e112dae14a8fff1eb1342035be9cc99c5f2b",

              "result_sha256": "87d9ece2bb56d9e4dc1823e8a8ec77ca0f0af6127f90927efd362d757e2e5ad2",

              "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

              "stream_sha256": "ae0e2b313187952c2e3a1bc3a7e58c547e00f1ebbb6087ab1ebe49093999d320",

              "decoded_sha256": "161b6a606d759f1376436ef794f606a39bbbc5fe27a3ddb3fbdda370efa0b2bf",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 120,

              "bytes": 38858,

              "v1": 93.368872,

              "neg": 96.211937,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "738d9eff098a9c1c63d9a2bc85c9e112dae14a8fff1eb1342035be9cc99c5f2b",

              "result_sha256": "71ea84182f7464bc581d3642af9a4c1859cdce217a12d4cb874fba40a12adb56",

              "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

              "stream_sha256": "dcc04a24f7a847195305867f24ef95c0f3c352fc202486586856f4f29259b1c4",

              "decoded_sha256": "0418a77af51a39432d29d6ecbcfcc66f538d98b561b206a51a7e5607b41db80b",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 116,

              "bytes": 42506,

              "v1": 94.153061,

              "neg": 96.663575,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "738d9eff098a9c1c63d9a2bc85c9e112dae14a8fff1eb1342035be9cc99c5f2b",

              "result_sha256": "871024c93ed95d3b80c61bdd2a2ac774652e65fca7fbd82aa4948815932aead6",

              "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

              "stream_sha256": "ac862bda0d545a4fbf286cfd77e30d996a59256b47ba1c168bb59459470f4f3e",

              "decoded_sha256": "08db72bc5bb140e478a6b498c3eb5c8eeaec4e17ec61630a55e829f17f919097",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 112,

              "bytes": 47132,

              "v1": 94.714808,

              "neg": 97.095437,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "738d9eff098a9c1c63d9a2bc85c9e112dae14a8fff1eb1342035be9cc99c5f2b",

              "result_sha256": "1ece42fea71e050115d16adbbca78ac0f0b6446dda88d2249f52bcfcc57549e0",

              "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

              "stream_sha256": "ef05da32aeeb2d7a34a2a87cc28768f02eddb39fc88454beeaa2fa36408804be",

              "decoded_sha256": "49e7da3a698c39feea79c088f53401b05fef7dce7c291e1af87d6307df770a2c",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 108,

              "bytes": 52473,

              "v1": 95.416815,

              "neg": 97.48926,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "738d9eff098a9c1c63d9a2bc85c9e112dae14a8fff1eb1342035be9cc99c5f2b",

              "result_sha256": "17e56212b9ae1faee28e334fdfa6b4a4aac47c8da937d0c02c582c33164c3b6b",

              "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

              "stream_sha256": "3964442ec4ba6b0c2bee4454735795ff697c9499e43907597576deca54683094",

              "decoded_sha256": "930cb570214dbde064049ca589ff7613a3afb91e0cf711a84f50924269d0d05a",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 104,

              "bytes": 58108,

              "v1": 96.03797,

              "neg": 97.797254,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "738d9eff098a9c1c63d9a2bc85c9e112dae14a8fff1eb1342035be9cc99c5f2b",

              "result_sha256": "65d851d6c5ecbd1a8886edde18edd05a2298b3afc89c9bc3a2709a6eb3724364",

              "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

              "stream_sha256": "430d497cf9abc63940249f73bb446859c7170aee9a39537a5db65ca434016fc8",

              "decoded_sha256": "53271babf84d74c7ff9cf15e29595995ee201b6c595e61def4f6a64275b32fc0",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 96,

              "bytes": 71445,

              "v1": 97.064806,

              "neg": 98.368614,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "738d9eff098a9c1c63d9a2bc85c9e112dae14a8fff1eb1342035be9cc99c5f2b",

              "result_sha256": "48184ece7e54b5cf244423f0a895769980db9fa01a31e03bb8a6f26b100625fd",

              "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

              "stream_sha256": "66a4199dcbbb17529d45639fedaf8d0dd0ad3ae0e6b144ac20670b69159398da",

              "decoded_sha256": "d00829b75961f6bed1f895157020ae6146e999c47d82dc5ad0c36554158448a4",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 80,

              "bytes": 111840,

              "v1": 98.433732,

              "neg": 99.220095,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "738d9eff098a9c1c63d9a2bc85c9e112dae14a8fff1eb1342035be9cc99c5f2b",

              "result_sha256": "fbd8dad596be08afc2f67c2f89a7b24c083e6fc50c355dfba5fc210f9e8ddc8a",

              "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

              "stream_sha256": "d72166fc14313cd07cfdc7c8ecea1308f651c6a5a824835149090b0af396b408",

              "decoded_sha256": "6458a00246e615963f458a2dd64e84083a4e06f2b5f839eda9ba55e8cecc07c6",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            }

          ],

          "dual": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 1.0287785706509447,

              "worst_sample_ratio": 1.074725493615943

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 1.0295403215149168,

              "worst_sample_ratio": 1.0788881499029483

            }

          },

          "primary_v1": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 1.0144757200130785,

              "neg_delta_at_selected_rate": {

                "mean": -0.033854882960562674,

                "worst_sample": -0.15036393059131115,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 1.074725493615943

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 1.0166300236539212,

              "neg_delta_at_selected_rate": {

                "mean": -0.030052944708724924,

                "worst_sample": -0.13998659040362327,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 1.0733127248428236

            }

          },

          "eligible_band_comparison": true,

          "local_quality_acceptance": "SEPARATE_FINAL_DECODED_GATE_REQUIRED"

        }

      ],

      "new_candidates": [

        {

          "status": "INCOMPLETE_INSUFFICIENT_POINTS",

          "coverage": {

            "native": {

              "points": 1,

              "inside_93_97": 1,

              "below_93": false,

              "above_97": false,

              "eligible": false

            },

            "aom": {

              "points": 10,

              "inside_93_97": 7,

              "below_93": true,

              "above_97": true,

              "eligible": true

            }

          },

          "points": [

            {

              "q": 104,

              "bytes": 57770,

              "v1": 95.909177,

              "neg": 97.819937,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "c87df3fcd7e4c34aca53c9c516864a5c8ab3634550165442b6f28724970fb564",

              "configuration_key": "3448f095308d19647ead8dce55f471bee08195260f78ae9bb7dffa5cf9aec677",

              "result_sha256": "ded1eb10f2aa6f091c9cb822d20a3701edec70c41b94ca116c66b9df427172fc",

              "source_sha256": "9f4ecdf02ce2b2305eb1d1c0df020175a57370ed2c2038b76915b8c5f5ee2f9e",

              "stream_sha256": "b393ed509bd0e86ea665148270beb192ea7c6863ca750d616720b4d21d753547",

              "decoded_sha256": "52ffa8db78eb6b8fb8f0c332961e467af2d1ed050cf62894701c022122d13ee2",

              "encoder_sha256": "6d4f5179bb5748a53e71ed20dc68392f24177cb239b7386a6084a58c3bc9a112"

            }

          ],

          "dual": {},

          "primary_v1": {},

          "eligible_band_comparison": false,

          "local_quality_acceptance": "SEPARATE_FINAL_DECODED_GATE_REQUIRED"

        }

      ]

    },

    "live-2160-a": {

      "content": "Real",

      "dimensions": [

        3840,

        2160

      ],

      "input_bitdepth": 10,

      "original_bitdepth": null,

      "color_limitations": "Color metadata incomplete; research-only, no certified HDR/SDR claim",

      "aom_points": [

        {

          "q": 30,

          "bytes": 12646,

          "v1": 92.740007,

          "neg": 98.744367,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "1fe5ebf7456c5a02353e2309da34bcd7549c064d79ec2be4135cef00570bd2b0",

          "result_sha256": "d90c77ffc66dd53bd7d812e659be3a05875710e1cee3acfd10bbd9334f8475b4",

          "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

          "stream_sha256": "e16cfb8038b86f1ae71afc399b206fd0743f28179e448795513a8aacf93286a8",

          "decoded_sha256": "bf0543d0216d418c58d887086349a26d76468b1f225bca19936b0a4130d7d456",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 28,

          "bytes": 15504,

          "v1": 93.885268,

          "neg": 99.029578,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "1fe5ebf7456c5a02353e2309da34bcd7549c064d79ec2be4135cef00570bd2b0",

          "result_sha256": "6cb1e3c8a9f4a707185e2281b9aa757c90937bcdd183b98443c5f2c10b3ea438",

          "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

          "stream_sha256": "af026f020a170bb2e5768030b3f17673b93023cbc2adbf9763e6349df52cf7d4",

          "decoded_sha256": "4df1dcc591f69e3369d76ced74d208a4460306c093f480d9f98430f13274f432",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 26,

          "bytes": 20064,

          "v1": 94.717419,

          "neg": 99.216641,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "1fe5ebf7456c5a02353e2309da34bcd7549c064d79ec2be4135cef00570bd2b0",

          "result_sha256": "0b48c5bf80c5f28bea9ec0fdb3c84805d024c24ec178b356cf78fcab28f65ebb",

          "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

          "stream_sha256": "5b7fad4b1198070611a6c3a3d64f5240edfdc8113b8ad35a059ab1d96890360c",

          "decoded_sha256": "b88de25e867594a531ade2717e3bbf5c057f842f4c5ef09b534c7a6f85b6f8c2",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 24,

          "bytes": 26757,

          "v1": 95.923797,

          "neg": 99.493545,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "1fe5ebf7456c5a02353e2309da34bcd7549c064d79ec2be4135cef00570bd2b0",

          "result_sha256": "834cea4b08b72adb029ee9ab4331362713c93eb420e1d3717d7131ca2475f2d2",

          "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

          "stream_sha256": "325181ca289a9f798fd9ab90db83712220721afd91ea3420b3e2f75a18bfc150",

          "decoded_sha256": "9c15fb368dc8435fed9caa49a8c2dc848c8c4ca350589607406dfd4359cd73cb",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 22,

          "bytes": 29993,

          "v1": 96.423098,

          "neg": 99.629174,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "1fe5ebf7456c5a02353e2309da34bcd7549c064d79ec2be4135cef00570bd2b0",

          "result_sha256": "6ab805ab73853957b9a9aa6175aeb6a93ce1957e8b4059ce0f045b801a6fa987",

          "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

          "stream_sha256": "f46209d93a143db33395ff3b6af27684a4a55ad186e1489dea9e35113a8dfc0c",

          "decoded_sha256": "751ad99fd6241106649a15e509f736884543cd4e4d0791908b7e3f1eeb9a645d",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 20,

          "bytes": 32768,

          "v1": 96.631711,

          "neg": 99.750246,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "1fe5ebf7456c5a02353e2309da34bcd7549c064d79ec2be4135cef00570bd2b0",

          "result_sha256": "7a94f25b8eaeb46d62ad0f3bfe598805effe6e3f1485c56630f1d188508dcb7e",

          "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

          "stream_sha256": "ed3dcdeee6f2575e71a0b0c6f3b92b654a18c4ddd5293f8075b22d0ca83ac8eb",

          "decoded_sha256": "18cc2be3ff08b7cc0686c6520d4178ea051028c4955bb7ae5b9f7ce36fd28d53",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        },

        {

          "q": 16,

          "bytes": 39295,

          "v1": 97.228615,

          "neg": 99.976587,

          "dimensions": {

            "width": 3840,

            "height": 2160,

            "bitdepth": 10,

            "chroma": "420",

            "frames": 1

          },

          "models": {

            "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

            "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

          },

          "source_manifest_sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",

          "configuration_key": "1fe5ebf7456c5a02353e2309da34bcd7549c064d79ec2be4135cef00570bd2b0",

          "result_sha256": "6163e3db1b3484778f279c11d2c0bb70a3383bb194b3c428fed2d0ed5e2477c9",

          "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

          "stream_sha256": "930e85bf4ecdba3f281a66ba1f8ef9748fba724ff67ae4aed1d4c91eeb9017e9",

          "decoded_sha256": "95d4bee539e5649958d666b97c0623d60d6c1dd8a64193b10084477819d92ecb",

          "encoder_sha256": "5a5c13f153129aeca7cc1d97bf3b20dda502444d98029d27dbc5482537e78e13"

        }

      ],

      "aom_coverage": {

        "points": 7,

        "inside_93_97": 5,

        "below_93": true,

        "above_97": true,

        "eligible": true

      },

      "previous_best": [

        {

          "status": "COMPLETE_GLOBAL_RESEARCH_COMPARISON",

          "coverage": {

            "native": {

              "points": 10,

              "inside_93_97": 5,

              "below_93": true,

              "above_97": true,

              "eligible": true

            },

            "aom": {

              "points": 7,

              "inside_93_97": 5,

              "below_93": true,

              "above_97": true,

              "eligible": true

            }

          },

          "points": [

            {

              "q": 136,

              "bytes": 5420,

              "v1": 88.485825,

              "neg": 96.915193,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "6b135468aa06b930930ee9e3936cd6e61976e7cda8a69c306f70f73debce0590",

              "result_sha256": "3a0f81ad4f2333c84d6765395b8c98593b71e64c8287033bcc3f63d82890edf2",

              "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

              "stream_sha256": "97455fc4d714a0de79b52f21114d0d05a7113efd0d7c9f0e93a419cdf5a365fb",

              "decoded_sha256": "7e7ead8680b67c10bd34128c71d4b67f23d95e8a04c67c5956c2cfe77a41dfe2",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 124,

              "bytes": 8165,

              "v1": 90.708351,

              "neg": 97.976037,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "6b135468aa06b930930ee9e3936cd6e61976e7cda8a69c306f70f73debce0590",

              "result_sha256": "b13155fcc1fcab439c7defce182b06fb24269e25e47409ba1309e1625bb42325",

              "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

              "stream_sha256": "1b2219c373a895b50e563fde1577e6426ae794d56ff89ab619508584e718e7ef",

              "decoded_sha256": "bcc1a9b2da69cc1495c981088cf6f8e2a42a0d4237adaa13d06a7e894677e102",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 116,

              "bytes": 12191,

              "v1": 92.807207,

              "neg": 98.587349,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "6b135468aa06b930930ee9e3936cd6e61976e7cda8a69c306f70f73debce0590",

              "result_sha256": "b1527a848586266e1c79dd2cc8dc96bb4740b644bba43f13855fb775f8d26658",

              "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

              "stream_sha256": "2988e047a548cf462b46f284da0a06321b9ce25cac51544863425f688d0ba934",

              "decoded_sha256": "ac1eccb5ee5d807884f7277d62fa2b08d1c0a8d4fa33c5e7155847a7acd00dbb",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 108,

              "bytes": 19039,

              "v1": 94.608424,

              "neg": 99.139106,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "6b135468aa06b930930ee9e3936cd6e61976e7cda8a69c306f70f73debce0590",

              "result_sha256": "4889f49f17c306b89ee9ab1f5c798584b2f876afa429b40fa920b28d215966c1",

              "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

              "stream_sha256": "2de0af236b11e7b9b55ab2e91aae4fd8f757d2c3ed5bc729c505c6c9426a3b8e",

              "decoded_sha256": "d9b73c6f7619cdb1eaacb581af5499a596c1ce6a2ba847429b1777d58c3a7659",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 104,

              "bytes": 25015,

              "v1": 95.60345,

              "neg": 99.315788,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "6b135468aa06b930930ee9e3936cd6e61976e7cda8a69c306f70f73debce0590",

              "result_sha256": "0132f1a39b0b955fa97cef7fdef37dec0ad7cd75331250e1265cd1807cbe01d9",

              "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

              "stream_sha256": "d2fba8d61d7e61984f52f1932a58a39b83c7e610454c279b513dbf7581252765",

              "decoded_sha256": "144f7b61ae3681f4ab6e0fbc4749ae7e4b68c5ca64c62fd6e59b233766793c50",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 100,

              "bytes": 29851,

              "v1": 96.358204,

              "neg": 99.560456,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "6b135468aa06b930930ee9e3936cd6e61976e7cda8a69c306f70f73debce0590",

              "result_sha256": "aa30c4ef945fc4e1787bc7acad8cb115293a190ad1671deab97490596b1d6323",

              "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

              "stream_sha256": "5487b20b5774e0fcf5e19868f60219d43d5fab3905484f1714625b0e0daa2e73",

              "decoded_sha256": "61e32643fc0fc6337ca5d8a53c2076105b76c9d9a28b9a6db1fc325eac00792d",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 98,

              "bytes": 32031,

              "v1": 96.490403,

              "neg": 99.563319,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "6b135468aa06b930930ee9e3936cd6e61976e7cda8a69c306f70f73debce0590",

              "result_sha256": "51036f0ce57832607112c9d8aaaf31145fbd354507188c71687f7e2f2cfacaca",

              "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

              "stream_sha256": "2a05e0d00271599a24c71c513642029983a7a749116561c43f35970c3e86a8ba",

              "decoded_sha256": "16e8adfb8afd37f2c2b6f6ddd84650a787300c7838c4ec20f72954746bbc7827",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 96,

              "bytes": 33530,

              "v1": 96.741838,

              "neg": 99.63868,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "6b135468aa06b930930ee9e3936cd6e61976e7cda8a69c306f70f73debce0590",

              "result_sha256": "882141d3876935a4e6909a8f9f6fbec2d9422ebc6ca4c2d06d6228f013b0c030",

              "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

              "stream_sha256": "d97e9c12a2bfa2503409df5c69aafc0bca56658407e7c5de53354121f92c5b48",

              "decoded_sha256": "faa04ecf4d55568f50873d07460e3a46bf3a3d7186f40118cf059f6ba493d0fc",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 88,

              "bytes": 40651,

              "v1": 97.13179,

              "neg": 99.865864,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "6b135468aa06b930930ee9e3936cd6e61976e7cda8a69c306f70f73debce0590",

              "result_sha256": "0f60f8d5872c4d015c649aafb9c4256b172cbaa785b107b0d1cfdaae59404ebf",

              "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

              "stream_sha256": "0250d674598a4fbc19808608c1ff35e808d7163c24bf137da55c760fa05354be",

              "decoded_sha256": "fcb2b9686fee59ea226301da7f6059780403708466be020c71f960ade7b20916",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            },

            {

              "q": 80,

              "bytes": 50712,

              "v1": 97.959769,

              "neg": 100.0,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "5e9e520443c8b39fcd232892827176ded8588b82f7894e8740c5abc33521c673",

              "configuration_key": "6b135468aa06b930930ee9e3936cd6e61976e7cda8a69c306f70f73debce0590",

              "result_sha256": "739f57382f77137f3cd91bd884794557ee604ac4de8be467ca9c1d8fca2d1825",

              "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

              "stream_sha256": "dfebb6bc0a628194e3a8495515c470ff828b071837939908efa196891f74c14c",

              "decoded_sha256": "7ef45ca1cbf5c4c01394f098ea0525f0631168554c9dcd60233e745aa93b31be",

              "encoder_sha256": "cb7271f47182aa4045fc243900848b486a805c72cd9d67f30821a38ed5e45cbc"

            }

          ],

          "dual": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 1.0895287079620346,

              "worst_sample_ratio": 1.1534073331413737

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 1.0987730271463463,

              "worst_sample_ratio": 1.1538676156428211

            }

          },

          "primary_v1": {

            "pchip": {

              "full_coverage": true,

              "mean_ratio": 0.9987893474319904,

              "neg_delta_at_selected_rate": {

                "mean": -0.0926335971075531,

                "worst_sample": -0.16383387575640995,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 1.041721870496808

            },

            "linear": {

              "full_coverage": true,

              "mean_ratio": 1.0017170087433265,

              "neg_delta_at_selected_rate": {

                "mean": -0.09785715632612782,

                "worst_sample": -0.16270833680648877,

                "every_sample_nonnegative": false

              },

              "worst_sample_ratio": 1.0391573624314088

            }

          },

          "eligible_band_comparison": true,

          "local_quality_acceptance": "SEPARATE_FINAL_DECODED_GATE_REQUIRED"

        }

      ],

      "new_candidates": [

        {

          "status": "INCOMPLETE_INSUFFICIENT_POINTS",

          "coverage": {

            "native": {

              "points": 1,

              "inside_93_97": 1,

              "below_93": false,

              "above_97": false,

              "eligible": false

            },

            "aom": {

              "points": 7,

              "inside_93_97": 5,

              "below_93": true,

              "above_97": true,

              "eligible": true

            }

          },

          "points": [

            {

              "q": 96,

              "bytes": 33411,

              "v1": 96.435932,

              "neg": 99.632977,

              "dimensions": {

                "width": 3840,

                "height": 2160,

                "bitdepth": 10,

                "chroma": "420",

                "frames": 1

              },

              "models": {

                "v1": "3e696240ee7cc047e2867a004fe0a57caa50e8bfb24161726bbcb31cce3f3883",

                "neg": "27f91a94e5f2d59f68855711873bc2e2eefa5200cf0a302b90923774f3d1c54d"

              },

              "source_manifest_sha256": "c87df3fcd7e4c34aca53c9c516864a5c8ab3634550165442b6f28724970fb564",

              "configuration_key": "5fc6cb9a3e0964c15b2c1ba1b6b8f4fbe8cb29b417112bd225521e01db0c1b81",

              "result_sha256": "dcc3527fc7076005d5d3c53fc051b0711ca1a6868af05dccf83746e656896d30",

              "source_sha256": "c525954e12391536e795867d2e869b0ece2172dfb0cf5529a4008cc5b82b4664",

              "stream_sha256": "d21d440d06f04deaaca4f21a46dcc511ebc1ee00ae5716108a0cf6143aa7a669",

              "decoded_sha256": "ec221592f35987e7ffb5d0b99dbca3c1b0215af469ce4d5e39af363a5bedff98",

              "encoder_sha256": "6d4f5179bb5748a53e71ed20dc68392f24177cb239b7386a6084a58c3bc9a112"

            }

          ],

          "dual": {},

          "primary_v1": {},

          "eligible_band_comparison": false,

          "local_quality_acceptance": "SEPARATE_FINAL_DECODED_GATE_REQUIRED"

        }

      ]

    }

  }

}

</details>
