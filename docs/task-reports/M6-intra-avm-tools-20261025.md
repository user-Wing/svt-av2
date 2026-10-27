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

旁边已完成会话证据为 `C:/Private/ffmpeg-native/tests/avm-aom-singleframe-20261024`：官方AVM9档对AOM0档，三张原生1080p/10bit420开发帧各六Q。2D VMAF BD-rate −22.37%，VMAF90/93匹配体积节省17.05%/16.39%。这是该开发帧的证据，不代表最慢AVM上限或50图验收。

本轮 `C:/Private/ffmpeg-native-m6-tests/intra-r3-20261024/agent-avm/analysis.md` / `analysis.json` 保存14个新消融点与20个诊断流。隔离诊断decoder的输出均与固定主oracle逐像素一致；没有修改原AVM树或固定decoder。当前live AVM源码已更新，不能冒充被测二进制的构建提交；新原语依据旧v1源码并经固定decoder核对。

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

外置根：`C:/Private/ffmpeg-native-m6-tests/intra-r3-20261024`。286文件baseline-source与上一轮完整SHA逐项一致，baseline程序为8c7906e08244e61b32644b93d62c4c98b725097e546f9ee1468b270c996e1c89；原有冻结材料只读。

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
