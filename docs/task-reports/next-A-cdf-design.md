# 下一基础能力 A 只读设计：帧内 CDF 自适应

2026-10-18。仅审查当前生产代码与本地固定AVM源码，未修改生产、编译或运行测试。R5/R6源码/程序与其效果证据保持；本设计不是已实现能力，也不跳过既定基础能力顺序。

## 最小完整范围

首步可以只做**每帧单tile内自适应**，继续PRIMARY_REF_NONE，不引入帧间CDF继承、次参考平均、多tile或并行。

固定AVM证据：`decoder/decodeframe.c:6858-6861`在PRIMARY_REF_NONE/禁跨帧上下文时调用`av2_setup_past_independence`；`common/entropymode.c:1261-1274`重置coeff/mode/MV默认；`common/entropy.c:33`按实际base_qindex选coeff类别。`decodeframe.c:4741`将tile的allow_update_cdf设为disable_cdf_update的反值。当前`frame_header.c:231`显式关闭更新，P帧又显式PRIMARY_REF_NONE。

因此适应期间每帧从当前实际QP类别的默认模型和count=0开始，帧内按decoder相同符号顺序更新；下一帧仍重新初始化。decoder帧尾会保存上下文及衰减计数（`decodeframe.c:10036-10043`、`entropy.c:79-84`），但下帧PRIMARY_REF_NONE丢弃它。首步不必新增编码器DPB概率继承；若比较最终状态hash，要区分tile结束更新状态与decoder帧尾计数衰减后的状态。

不能只翻disable_cdf_update：该位控制所有被decoder以`avm_read_symbol`读取的模型。只更新coeff、漏掉mode/MV/partition/Tx等模型，会在第二次相关符号开始解码失配。bypass literal不更新CDF。

## 固定AVM更新模型的完整数据

`../avm/avm_dsp/prob.h:35-49`：CDF_SIZE(n)=n+4，AVM_CDFn保存n−1个inverse边界、终端0、count；AVM_PARAn再补三个训练offset。n=2/3与n>=4的offset基值不同。

`prob.h:223`的update_cdf按更新前count选区间（count<=15、16..31、>=32），rate=2+对应已转换offset；count饱和32。不是通用AV1按alphabet推固定rate的公式。生产表当前只保留forward边界，丢掉count与训练offset。

必须从固定source逐模型恢复：

- `entropy_inits_coeffs.h`：全部四QP类别的coeff默认模型与各自AVM_PARA。
- `entropy_inits_modes.h`：partition、Y/UV模式、intra/inter、skip、DRL、palette及DCT_IDTX Tx模型。
- `entropy_inits_mv.h`：ONE_PEL shell set/class、offset、pair与col模型。

三个训练值是固定默认模型的一部分，不是这轮可调参数。例：y_mode_set为PARA4(1,1,1)，palette_y为PARA2(-2,-2,-2)，同样不能用统一rate代替。frame qp类别也不能只按边界相同合并，因为offset可能不同。

建议新可复现提取器保留**表族、完整row坐标、实际alphabet n、边界与配对PARA triplet**，记录固定AVM文件SHA。现有`cdf_tables_coeff.c`引用的tools/gen_coeff_cdfs.py当前路径不存在；现有边界表及历史extend脚本不足以直接提供完整模型元数据。不得用简单正则剥掉PARA后再猜offset。尤其inter_ext_tx数组声明CDF_SIZE(16)，部分实际row却是AVM_CDF2+PARA2，必须按实际row的alphabet而非声明的padding宽度取count/offset。

可存canonical inverse模型以直接对应AVM，或存forward边界并严格转换更新公式；两者都要逐符号与固定update_cdf独立对照，验证舍入方向、15/16与31/32区间切换及count饱和。

## 当前生产可达模型清单与身份风险

| 语法组 | 当前调用/索引 | 应恢复的模型族 |
| --- | --- | --- |
| 分区 | tile_writer.c:181/190，size/raw与rect_map/raw | do_split_cdf[structure=0][ctx]、rect_type_cdf[0][ctx]；当前64SB结构实际使用split前16row、rect前28row，不能把同值row合并 |
| Y模式 | tile_writer.c:135-143及237-241 | y_mode_set，y_mode_idx[3上下文]，y_mode_idx_offset[3]；alphabet仍为4/8/6，不能因只发DC/V/H就缩小 |
| UV模式/调色板关位 | tile_writer.c:147/242/247 | uv_mode[2]、palette_y_mode；palette固定0仍必须更新计数/边界 |
| intra/inter与运动模式 | motion_syntax.c:7-12/109-115 | intra_inter[4]、inter_single_mode[5]、DRL[type0][5]、skip_txfm[3]；永远写DRL0也要适应 |
| MV | motion_syntax.c:32-59 | joint_shell_set、ONE_PEL set0 class、low_class[2]、class2 unary首bin、offset bits[6]、pair greater[2]、col_index[4]；sign和quasi-uniform literals仍bypass |
| 系数是否为零 | coeff_syntax.c:683/706 | txb_skip[predmode][TX][ctx]与v_txb_skip[ctx]，含U/V耦合索引 |
| EOB | coeff_syntax.c:731-761 | eob16/32/64/128/256/512/1024[3个plane/pred上下文]，eob_extra；固定decoder确实共享单个eob_extra模型，不应擅自按平面复制 |
| 幅值/低频/BR | coeff_syntax.c:791-979 | base/base_lf、eob版本、UV版本、BR/HF/LF；当前TCQ=0但TX/ctx必须保持身份，不误步进n而非n−1 |
| DC sign | coeff_syntax.c:1007 | dc_sign[Y][hidden=0][3ctx]；固定decoder U/V sign是literal（decodetxb.c:873-886），不能把它们也适应 |
| inter DCT_IDTX | coeff_syntax.c:773局部16384 | decoder decodemv.c:1164的inter_ext_tx_cdf[eset][eob_tx_ctx][square_TX]；当前多个均匀初值模型必须分别建state |

有两个方向都要处理：**不同模型即使初值相同也不能合并，同一规范模型即使当前有多个转录对象也应合一**。例如global y_mode_index_0与m5_y_cdf[0]是同一y_mode_idx[0]；历史do_split_ctx12别名也映射同一partition row。motion的symbol(e,cdf_value,v)在栈上取临时&cdf，按指针登记将把不同模型混在同地址，按数值登记又会把均匀row等错误合并。

遗留`block_writer.c`的M1 minimal路径使用历史7-symbol近似，生产完整coeff路径已用8-symbol。首步自适应应明确只支持当前完整coeff生产路径，对blocks==NULL的遗留minimal writer拒绝开启，保留其默认off冻结行为，不借适应任务改写旧M1流。

## 可落地接口与事务方案

建议新增独立模型模块（名称仅建议）：

- `SvtAv2CdfModelId`：明确表族+row语义，禁止以CDF值或临时地址作身份。
- `SvtAv2EntropyModels`：当前frame/tile的固定布局可变bank，含每row边界/count/训练offset；按实际QP初始化，仅需当前类别的可变副本。
- `svt_av2_entropy_encode_model(enc, models, id, symbol)`：用**更新前**模型编码或累计Q12成本，成功后严格更新同一row。原const `encode_symbol`保留给默认off与低层冻结测试，不用const-cast修改静态表。
- 每个语法调用点显式传model ID；coeff可从已有family+row算ID，但不靠pointer散列猜alias。启用适应时未知/遗漏模型必须报错，不静默退回静态CDF。

模型bank建议首版直接嵌入TileTree，避免过早设计复杂undo journal：现有`TileTree trial=s->tree`便同时复制可变CDF和所有计数，enc在trial期间绑定trial bank。commit时把整个bank随tree提交；失败/rollback丢弃trial bank，原motion cells仍按已有SB备份恢复。不能只在tree里放共享bank指针后继续浅拷贝。

rate_only目前在entropy_encoder.c:184分支提前return，新增更新必须同时覆盖此分支及正式编码器。每SB候选从相同committed bank开始，候选内符号顺序逐步更新；winner重放必须得到相同R与末尾bank hash，再commit。最终正式帧writer从fresh默认bank顺序写赢家，其逐符号模型轨迹、累计R和末尾bank应与RD提交链一致；不能边写最终帧边重用已被RD推进到帧尾的bank。

模型状态仅为当前帧固定有限布局；首版完整bank复制有明确额外内存带宽成本，要实测，不承诺免费，也不先上指针缓存/写时复制。new frame、编码失败重试、packet扩容重试都要按原提交纪律恢复，不能在失败encode后复用半推进CDF。

## 最小开发/验证闭环

1. 先生成模型元数据及更新内核，逐row与固定AVM边界/PARA/count完全对照；同初值不同row独立更新、alias同row共享，覆盖计数区间。
2. 完整wire接入后先以**rd_mode off固定决策**比较adapt off/on：预测、qcoeff、recon必须相同，只变熵字节。这样直接观察同质的纯熵收益，避免同时引入RD选择变化；三格式AI/P、多SB、多帧、四工具与QP类别边界用固定AVM逐样本验证。
3. 再接RD适应事务：候选换序、逐SB最低J、trial前后bank hash、故障/容量重试、最终writer轨迹与R一致；覆盖R5/R6/coeff可用组合，但不同时变其策略或lambda。
4. 全部正确性通过才按原冻结native样本单独评质量/速度。新模型可改变RD决策，不保证更小局部估计J就有整段收益。

核心预计触及模型/生成元数据新文件、entropy_encoder.c/h、coeff_syntax.c、motion_syntax.c、tile_writer.c、frame_header.c/h，约10至14个内核文件；另有公共API/Codec/CLI/CMake与专属单测/生成器。实际模型清单需实现前机械核验闭合，不能把本表当已经计算出的最终row数量。不是翻一个开关的小补丁，也不需要先强行做frame上下文继承。

单tile保持时`decodeframe.c:4254-4257`不读取context_update_tile_id，新flag不应凭空增该字段；当前PRIMARY_REF_NONE与sequence的avg CDF设定可以保持。真正语法风险是某个reachable模型的身份、alphabet、初始化offset、更新顺序或一次literal/symbol分类错误，而不仅是帧头那一位。

## 首关收敛补充：仅rd_mode=off（只读设计）

建议首关明确拒绝`entropy_adaptation != 0 && rd_mode != off`，只接正式writer和帧头；不接rate session、不改候选、lambda或rd_frame。当前off的预测/变换/量化选择不依赖实际writer的熵长度，故同输入必须逐项保持模式/MV/qcoeff/recon，仅熵字节可变。motion/coeff实验开关原本要求RD，不能与本首关组合。

正式writer每次调用初始化fresh bank，按base_qindex选择默认系数表，帧头允许CDF更新，PRIMARY_REF_NONE保持；每个符号先编码后更新。容量不足重试也重新初始化，禁止复用失败writer的半推进状态。这个范围不需要候选bank复制、rollback、commit、RD重放模型hash或最终writer/RD轨迹同步，明显收敛动态状态与验收范围。模型身份与全部reachable语法接线仍必须一次做全，不能只打开coeff适应。

### 模型规模估计与自动提取

现有`cdf_tables_coeff.h`的物理行形状可直接核算：每个qctx共773行、2819个现有forward boundary words。恢复terminal/count/三个offset后是6684个uint16，即约13.1KiB系数可变状态。若保留全部这些物理行，加当前非系数语法约100行（分区44、Y/UV/palette10、模式/skip17、MV17、inter-tx最多12），bank设计容量约873行；采用每行最多8符号的固定24字节槽约20.5KiB，另有少量映射元数据。这里只是保守布局估计，**不是已经机械证明的精确reachable数量**。TCQ固定0、Y隐藏符号固定0等可把系数候选行上界缩至499，但还可有TX_CLASS/上下文不可达行，首版不建议为省数KiB提前压缩它们。四个qctx默认数据都应生成，运行时只复制当前一个，不需要四个可变bank。

边界和训练offset可以从固定AVM自动生成：提取`entropy_inits_coeffs.h`、`entropy_inits_modes.h`、`entropy_inits_mv.h`的表族/完整坐标/实际AVM_CDFn/配对AVM_PARAn，并展开prob.h的PARA偏移。独立校验建议另用C extractor直接include固定头，逐行导出真正宏展开后的边界、terminal、count、三个offset，与结构化解析生成结果逐word比较；记录输入头和prob.h的SHA。实际row alphabet必须随row保存，不能按声明最大CDF_SIZE或相同边界猜测。随后以显式语义ID清单核对每个formal emit调用及alias；单纯导出所有AVM表不能自动证明当前writer的可达集合闭合。

### 相等raw CDF边界与rate_only限制

已确认固定AVM允许非严格单调raw CDF。`update_cdf`的整数舍入可令相邻boundary相等；`prob.h:248`的`od_ec_prob_scale`还加入位置相关`av2_prob_inc_tbl[nsym-2][n]`，正式算术编码并非直接把raw差值当作range。固定`entenc.c`的q15路径允许`fh <= fl`。当前本项目正式符号路径也使用同样的scale/padding，而`entropy_encoder.c:184-190`的rate_only路径对`lower >= upper`返回ERROR_RANGE，零raw概率不能使用现有log-cost估计器。

因此首关禁止RD是有实质价值的隔离：不把长期适应状态带入当前rate_only。不能为绕过拒绝而给raw CDF夹epsilon或强制严格递增，那会偏离decoder状态。正式writer的独立验收须包含长偏置符号序列、出现相等raw边界后的所有符号，以及与固定AVM算术输出和逐样本解码对照。下一关RD若启用，应另行定义与规范scale/padding相容的有限R估计，而不是直接沿用raw差值或调lambda掩盖问题。

首关预计仍涉及模型/生成元数据、entropy_encoder、coeff_syntax、motion_syntax、tile_writer、frame_header及配置接线和专属测试，但可完全排除rd_frame/rate事务改造。验证目标由“适应改变编码决策后的曲线”收敛为同一完整recon的实际bytes差及固定AVM一致性；收益正负仍由冻结样本实测，不预先承诺。
