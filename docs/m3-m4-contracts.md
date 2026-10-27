# M3→M4 接口与数据契约

日期：2026-10-11。状态：**开发要求草案，生产接口尚未实现**。
范围/工程约定由本任务包给出；具体AV2字段和filter规则须在正式开发G0/G1核对并冻结。
执行入口：[M4任务包](../../M3-M4-AI-TASKS/README.md)、
[唯一文件属主与调度](../../M3-M4-AI-TASKS/00-集成与调度.md)。

## 1. 范围与兼容性

配置仅8-bit420、10-bit420、10-bit444，QP100、threads1、8倍数尺寸至1920×1080。先10-bit420，再10-bit444，8-bit420回归。保留M3默认ALL-INTRA，显式启用LOW_DELAY_P才走M4链。第0帧key，之后单向P，编码/显示顺序相同，一个逻辑LAST，单tile/SB64/DCT_DCT。

旧M1/ALL-INTRA入口与M3流/recon指纹保留；不扩8-bit444、422、其他位深/QP、多参考、B帧、完整RD、SIMD或运行期多线程。只为正确420运动补偿实现必要色度分数相位，不加入亮度subpel搜索。

## 2. 公共API最小提案（待00落地）

- 新SvtAv2PredictionStructure：SVT_AV2_PRED_ALL_INTRA=0、SVT_AV2_PRED_LOW_DELAY_P=1。
- SvtAv2EncoderConfig末尾追加prediction_structure；默认ALL_INTRA。
- 追加SVT_AV2_PARAM_PREDICTION_STRUCTURE，已有parameter数字不动，COUNT相应更新。
- CLI新增 --pred-structure all-intra|low-delay-p，缺省all-intra，其他字符串明确报错。
- config/public struct发生布局变化，要求调用者重编，不承诺旧二进制ABI兼容。
- 不新增keyint、lookahead、强制key、重排序或公共MV dump接口。

B提出精确签名/类型patch，00核对最小性后修改include；本段并不声明这些符号已存在。语义变动必须先同步A/B，再落盘，不做彼此不一致的私有枚举。

## 3. 图像与数值

| 数据 | 单位/表示 |
|---|---|
| source/recon/reference planes | 字节地址，planes和strides为byte |
| 8bit样本 | uint8值0..255 |
| 10bit样本 | u16le，低10位，值0..1023；允许非对齐/奇数字节stride |
| 420几何 | Y全尺寸，U/V每轴一半 |
| 444几何 | Y/U/V同尺寸 |
| prediction | uint16样本，按bd clip |
| residual / inverse residual | int16 |
| coeff / qcoeff / dqcoeff | int32；必要乘加int64 |
| coefficient packed grid | min(tx_w,32)×min(tx_h,32)，row stride=min(tx_w,32) |
| SAD / frame成本累计 | uint64，不与MV有符号算术混用 |

保持M3输入copy-in原子拒绝和QP100映射：8bit qindex100、10bit qindex148。inter TX/CDF若有专用规范约束，核对后记录，不能未经依据照抄intra配置或改变M3分支。

## 4. 参考存储与提交事务

A提供reference的存储/读view/prepare/commit/release，B拥有Codec状态和调用次序；无需预设通用多槽池。最小深拷贝允许，当前source、当前recon、已提交reference必须互相独立。

状态流程：

~~~text
初始化：reference.invalid，frame_index=0
send_picture：验证并copy输入；reference不变
encode：读已提交reference，写当前recon和块结果
serialize：完整header/tile/packet产生成功
commit：发布该帧packet/recon、更新计数，并且一次性更新LAST
get_packet容量不足/重试：返回required size，既有commit不再执行
EOS：排出pending packet后终止，不产生额外P
deinit/reinit：释放全部reference，下一序列从key重新开始
~~~

发布不是调用者取走packet之后才刷新：reference在完整编码成功的内部事务中提交。不能在块重建完成但packet失败时刷新，也不能在get_packet容量查询时重新编码。若commit需要分配，须在发布前准备完；任何失败不得留下“packet已成功但reference没更新”的状态。

可复用current recon buffer之前保证上一帧reference已持有自己的样本。失败路径禁止向get_recon暴露半帧；旧reference保存到成功commit或deinit。错误恢复沿用既有API约定，不新增隐式跳帧。

逻辑reference身份以完整frame_index为准，同时记录bd/chroma/尺寸；order_hint可能环绕，不能当唯一身份。参考可见区以外读取使用已冻结规范边缘扩展，普通padding不自动视为有效边缘。

## 5. 运动矢量和候选集合

内部SvtAv2MotionVector概念为有符号mv_x/mv_y，单位均为**亮度整数像素**。具体struct/函数签名正式G0由A提出、00冻结，不为任务书先创建空生产接口。

prediction在当前位置(x,y)使用reference(x+mv_x,y+mv_y)。图像内容shift(+4,-2)对应内区MV(-4,+2)。语法定点精度、predictor和difference由B转换且仅转换一次。

第一版每轴[-8,+8]全搜索，覆盖289个整数候选；规范legal range可进一步约束，最终候选集要落盘。ME成本为亮度块SAD，tie按(SAD, abs(mv_x)+abs(mv_y), mv_y, mv_x)升序。边界如何使用扩展、哪些候选非法，与预测器及独立模型一致。

420奇数亮度MV的色度位置包含分数相位，负数不能靠C的除法截零决定坐标。A落实规范filter/round/clip，B落实相应字段。444整数位置也按实际AV2规则核对；禁止未经核对套bilinear或AV1表。

## 6. 帧核心与语法共用结果

A每个合法coding block只选一次模式：DC intra或单参考inter。ME先按亮度cost找候选；候选间最终比较使用对应Y/U/V全部可见样本SAD，tie选intra。该规则是最小工程启发式，不声明是规范要求或完整RD。

块结果概念字段：geometry/plane grouping、intra/inter mode、logical ref及完整参考身份、绝对MV、实际TX、skip/zero信息、qcoeff所有权和数量。B根据规范定义wire mode/ref/predictor/CDF，不能在tile writer重选mode、替换MV或重算qcoeff。

现有SvtAv2TileCoeffBlock不包含这些信息，不能仅新增P帧头而仍写纯intra tile。拓展现有结构或增加小伴随记录由A/B提案、00冻结；不把全部语法上下文塞进公共API。

qcoeff必须是实际重建用dqcoeff的来源，生命周期覆盖整个同步tile serialization。scratch重用前保存快照。coded tree与YUV顺序遵循规范；P中intra邻居来自当前帧已重建区，inter邻居/reference不可混读。

## 7. B必须冻结的规范支持表

正式启动优先核对，下列项本轮未给出未经验证的bitstream常量：

| 项目 | 必须记录 |
|---|---|
| frame type/output | AV2实际非key单向帧型、show/immediate输出推导 |
| reference map/refresh | key物理槽初始化、逻辑LAST映射、P读槽/更新槽 |
| order hint | 8bit环绕和规范相对顺序推导 |
| MV | wire单位、允许精度、合法范围、predictor栈、difference编码 |
| motion compensation | frame/block filter选择、420相位、round/clip及边界 |
| entropy | frame context初始化/继承、CDF更新及参考上下文条件 |
| block mode | 单参考inter、P内intra、skip、partition/TX及coeff条件 |
| disabled tools | 实际关闭字段与仍必需分支，不省略规范必选步骤 |

每行给固定规范节/页、只读AVM文件/行号、encoder对应分支、受控测试和独立decode证据。不能把逻辑LAST名称直接当AV2枚举。A/B意见冲突由00查规范/decoder裁决，不询问无法提供技术答案的用户，也不使用未经证实的默认。

## 8. 验收契约

所有内部recon在decoder运行前产生并冻结。固定avmdec只用于独立解码/对照；不调用avmenc代生产编码，不修改oracle，不回填recon。

48项原M3 AI回归 + 108项新P矩阵全部zero mismatch。实际P帧/refresh与inter块须有独立码流/decoder证据，encoder自报仅作诊断。ME最优cost/MC期望由独立模型判定。

静止128×128×64帧三配置分别P_bytes≤0.5×AI_bytes；同源同QP，P总MSE≤1.05×AI总MSE+1，并报告逐plane质量。三配置MSVC与sanitizer各1000帧P链、MSVC/GCC/Clang全部平台、Clang36项hash核对按T035/T036执行。

各测试数、artifact schema和工具CLI以实际实现落盘，不填假PASS。00终验通过才更新0.0.4-m4；当前已完整终验并更新，见第12节。

## 9. 写权限、报告与状态

属主以00任务书为准。A参考/ME/MC/核心，B语法/Codec/app，00公共头/CMake/工具/独立验收。A/B互相通信，重要决定抄送00。构建目录隔离，移交文件双方先停写并记录SHA，不覆盖M3基线。

状态：TODO / RUNNING / BLOCKED / READY_FOR_INTEGRATION / DONE。准备时各工作单元TODO；正式启动及冻结见第11节，当前完整DONE见第12节。没有实际结果不能写运行中或完成。

## 10. 当前准备记录

2026-10-11：任务包13份Markdown与本契约完成；M3 97文件清单静态一致。没有本轮新编译/编码测试，没有生产源码改动，没有恢复FFmpeg队列。后续是否启动以用户新指令为准。


## 11. M4正式启动与首批冻结

用户后续授权正式开发；状态RUNNING，00=/root，A=/root/m2_pipeline，B=/root/m2_syntax。UI/FFmpegFreeUI无需操作。准备记录保留为历史。

公共模式接口已由00应用：SvtAv2PredictionStructure枚举SVT_AV2_PRED_ALL_INTRA=0 / SVT_AV2_PRED_LOW_DELAY_P=1；config末尾prediction_structure；PARAM_PREDICTION_STRUCTURE追加在OUTPUT_PATH之后、COUNT之前。B负责默认/验证/setter/CLI行为。版号仍M3，直到最终终验。其余MV/ref/core签名及实际AV2语法待A/B依据规范冻结。


### 11.1 reference/MV/core与MC已冻结

A四模块header已实际实现。SvtAv2ReferenceFrame持有committed/prepared两个预分配独立FrameBuffer；prepare复制staging、commit无分配交换，frame_index为uint64且必须连续。Codec在完整序列化成功之后仅一次prepare/commit，再发布packet/recon/计数。

SvtAv2MotionVector.mv_x/mv_y为int32亮度整数像素；MotionSearchResult含uint64 SAD及候选数289，保持[-8,+8]全集合、严格大于当前最优的partial SAD可提前停止，不改变最终cost/tie。

SvtAv2InterBlockResult按Y/U/V同步hook，包含plane/block/luma_block/mode/mv/full reference_frame_index/all_zero；scratch qcoeff仅hook内有效。B采用SvtAv2TileBlockMode伴随数组，每YUV group一条is_inter/mv_x/mv_y，保持旧TileCoeffBlock布局，owned qcoeff快照同步收集。

固定BILINEAR=3已由A/B与00独立核对。依据：规范PDF p484–485 §7.13.3.16 round0=3、单参考round1=11、post0；p488插值过程；只读AVM filter.h32–35及75–84、convolve.h146–148、convolve.c103–153/477–523。整数亮度对应420色度phase0/8，64/64系数可严格化简为2/4样本一次圆整平均。负MV必须floor除二，边缘逐sample clamp。00独立m4_model采用8tap两级round，未复用生产平均公式。

LOW_DELAY_P专用sequence采用explicit map、一个物理slot；P显式num_total_refs=f3(1)，slot索引0bits、refresh slot0=1。规范decoder实际验证完成之前是已接受实现方案，尚非终验PASS。默认AI sequence路径不变。

### 11.2 固定版本P语法已核对

参考版本以固定oracle包对应AVM v1.0.0 commit 966a7d7cd6fcf60360caf5dc413b2aeeb65e144d为准，不能用本地AVM junction的较新HEAD代替。只读副本与SHA在artifacts/m4-root/oracle-v1-reference/manifest.json。

P帧显式PRIMARY_REF_NONE、disable_cdf_update=1、SINGLE_REFERENCE=0、skip_mode_flag=0、reduced_tx_set=2。固定mvref_common.c4555及decodeframe.c9337表明单参考仍会读取skip_mode_flag，不能省略这一位。此配置不读取逐块skip_mode符号。亮度整数MV内部单位像素，wire单位1/8像素；实际MVP相同用NEARMV+DRL0，否则NEWMV+DRL0及非零difference。

reduced_tx_set=2在inter小于64的亮度非零残差块仍需DCT_IDTX类型符号，固定entropymode.h391/404的DCT_DCT索引为1；不能沿用intra隐含DCT或误发IDTX索引0。零残差受控用例不经过此分支，必须保留176×144等边缘多块纹理验收。

motion-r11独立反事实12/12通过；rate-r11三配置静止码率/质量通过。完整矩阵、跨平台与长跑仍待00终验，当前RUNNING。

## 12. 最终冻结与验收

M4 0.0.4-m4 DONE。11节为开发过程记录，最终运动predictor使用固定版本允许的9个候选，删除不合法的远左-5两项。所有108 P/48 AI、motion12、rate3、MSVC/GCC/Clang及sanitizer、三配置两平台各1000帧、Clang36项、Valgrind/Massif实测完成；版本元信息更新后七配置clean与AI/P/长链指纹复核通过。默认AI与M3逐字节相同，接口及限定范围不扩。证据、完整命令和116文件最终源清单见docs/task-reports/M4-integration.md及artifacts/m4-root/final-verification.json。本轮未操作FFmpegFreeUI。
