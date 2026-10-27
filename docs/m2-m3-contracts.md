# M2→M3 最小接口与数据契约

日期：2026-10-11。状态：**开发要求已登记；新接口签名及规范依赖项待00冻结。** 本文件不是M3已实现证明。M2最终事实保持 `m1-m2-contracts.md` §12和M2验收报告不变。

## 1. 本期执行模式与强制范围

本期由当前Codex主会话00调度两个子智能体A/B，直接原生通信；T017–T026为工作单元，不采用OpenCode或十队列。用户2026-10-11最新范围覆盖此前较宽范围：先10-bit420，再10-bit444，保留8-bit420回归。

| 组合 | M3要求 | 规范/profile/oracle核对 | 当前生产支持 |
|---|---|---|---|
| 8-bit420 | 保留M2逐字节回归 | 已有M2依据；G0复验 | 已实现M2 |
| 10-bit420 | 第一闭环，必做 | T022核对并冻结 | 尚未实现 |
| 8-bit444 | 本期排除 | 不开展支持性核对 | 明确拒绝 |
| 10-bit444 | 必做，在10-bit420闭环后接 | T022确认合法profile及字段 | 尚未实现 |
| 8/10-bit422 | 本期排除 | 不开展支持性核对 | 明确拒绝 |
| 12-bit/400 | 不在当前范围 | 不扩展 | API/CLI明确拒绝 |

沿用M2：ALL-INTRA、DC_PRED、DCT_DCT、单tile/SB64、关闭滤波、threads1、QP100、非零8倍数尺寸最大1920×1080。QP100在10-bit下的实际qindex/quant参数由T020/T022依据AV2共同核对，不先猜映射。

## 2. 单位与外部像素表示（M3必须遵守）

| 名称 | 单位/表示 |
|---|---|
| width/height/block x,y,w,h | 所属plane的样本/行，luma坐标转chroma只做一次 |
| API picture strides、internal frame byte strides | 字节/行；至少plane_width×bytes_per_sample |
| residual/coeff typed array stride | 对应元素数/行，接口名字或注释必须明示 |
| 8-bit外部raw | 每样本1字节，0..255 |
| 10-bit外部API/raw/Y4M payload | 每样本2字节，unsigned little-endian、低10位有效；0..1023，上6位必须0 |
| 内部10-bit样本 | 可使用uint16_t host-endian typed存储，必须在边界显式转换，不能把host-endian当portable raw |

外部plane为借用字节指针；读取期间调用者保持有效，返回后可复用，编码器不留外部指针。不要悄悄新增16-bit对齐假设，优先字节安全copy-in；如提出对齐限制00必须明确批准、验证并写公共API要求。过短stride、几何/位深/格式不匹配、样本>max返回明确错误，不能掩码或截断。

内部分配由picture唯一持有；描述符须能表达bit_depth、chroma与每plane几何/byte stride，新增字段/签名由T017提出、00冻结。padding按样本填充值，10-bit中点512不能用memset；size_t乘法与容量检查防溢出。不要求检测外部指针真实容量，调用者保证有效行可读。

## 3. 色度几何与合法编码树

| 格式 | subsampling_x/y | U/V可见宽高 |
|---|---|---|
| 420 | 1/1 | W/2,H/2 |
| 444 | 0/0 | W,H |

当前尺寸为8倍数，无奇数尺寸扩展。每plane独立stride/padding。所有chroma映射使用同一格式派生参数，不能分散写>>1。Common coded traversal继续作为真实系数、预测和重建唯一块序列。具体chroma TX及partition/CDF依赖T017/T021/T022据规范/AVM共同核对后冻结，不从内存plane尺寸反推语法必定合法。

## 4. 预测、残差、变换与量化

预测范围0..(2^bd-1)，不可用邻居规则有规范依据；DC默认中点8-bit128、10-bit512。残差source-pred理论8-bit[-255,255]、10-bit[-1023,1023]，int16_t可承载，但累加/变换中间位宽需单独证明。

复用既有DCT_DCT及合法TX，T019逐级登记high-bit-depth shift/round/clamp和系数范围，禁止简单缩放8-bit逆结果代替规范过程。输出coeff、qcoeff、dqcoeff是否保持int32_t由范围证明定案；必要的乘加使用int64_t。逆残差范围按AV2实际规则核对，不能假定只等于source residual范围。

T020与T019冻结coeff scale/tx_scale、DC/AC量化参数、Y/U/V delta状态及超范围错误语义。M2量化delta字段偏移规则继续保持：f5 literal23表示实际UV AC delta0。8-bit的旧正常路径不因高位深泛化改变。

## 5. 系数与tile生命周期

每个编码叶块以Y/U/V顺序交付真实qcoeff；coeff count、row stride和TX类型由已确认TX决定，不从luma面积猜色度长度。T023 hook与T024复制后持有的buffer须覆盖tile同步消费期间；qcoeff与dqcoeff不相互覆盖。T021依据位深/chroma/qindex核对CDF、scan/EOB、邻居上下文，确认不变项也写依据，不凭直觉复制或新建分支。

禁止清零残差、硬编码payload、用外部encoder代工或用decoder输出冒充内部重建。oracle只做独立验证。

## 6. 流头与公开配置

T022维护规范/AVM字段表：profile、bit_depth_idc、chroma_format_idc、条件字段、level范围、量化参数。现有源码注释0=10-bit、1=8-bit是核对起点，不是跳过规范验证的理由。10-bit444的profile值待证据冻结；8-bit444和8/10-bit422按用户要求排除，不预做支持性probe或开发。

保留每包temporal delimiter、首包sequence header、8-bit order hint回绕语义；首帧/后续帧采用合法M2多帧格式。M2 motion flags=4、uniform tile unary首0停止、UV delta偏移等已修正结论不得回退。

include中的枚举范围不是支持矩阵。T024必须将配置传播到整个生产链，不仅放宽validate。既有packet/EOS/PTS/输入复用/精确容量retry契约不变；one picture in flight。公开struct若需新增位深/格式信息，由00明确ABI影响，重新编译全部调用者。

## 7. 内部重建和导出

单一clipping规则：先宽有符号加法，再Clip3(0,(1<<bd)-1,pred+inverse_residual)。邻居来自内部recon。最终export来自编码器自己的逆路径，布局紧凑Y→U→V，每plane逐行，不含padding；8-bit u8，10-bit u16le低10位。

每帧raw字节数：sum(plane_width×plane_height)×bytes_per_sample，不能固定W×H×3/2。420为1.5×W×H样本、444为3×W×H样本；10-bit各乘2。recon元数据必须可辨位深/格式/plane尺寸；增加字段还是沿用配置查询由00在消费者出现前定案。容量不足返回需要字节数，不写部分输出，不覆盖尚未获取的packet/recon。

## 8. 独立判定和测试矩阵

T025先做人工正负例：错width/height但面积相同、错位深/格式、端序、U/V交换、低2位差、截短/尾随payload、错误frame count。decoder输出header独立解析，必须与预期配置一致，不拿input header代替decoder header。逐样本/字节精确比较在规范化端序/布局后进行，首错plane/坐标/值可定位。

三组合（8-bit420回归、10-bit420、10-bit444）×四尺寸×1/2/3/17帧共48项；新增功能32项，M2回归16项。两新组合各64×64连续1000帧，同一app进程，全帧oracle对照并检查独立设计的帧序号。0/max/near max、255/256/511/512/1022/1023、低2位信息、不同U/V与padding哨兵必覆盖。数学和邻居模块每实际支持尺寸/位深至少100个固定种子随机向量，独立逆路径golden不可由被测函数自产。

最终MSVC/GCC/Clang Debug/Release、实际ASan/UBSan/LSan均须通过。Linux若使用Windows decoder明确记录；缺工具/缺规范证据就是未验证，不借M2 PASS填M3 PASS。保持旧compare_recon默认参数与既有字段兼容，M3工具新入口，旧verify_m2冻结不改。

## 9. 待00冻结表（开发开始后填写）

| 项 | 提案负责人 | 冻结条件 | 当前状态 |
|---|---|---|---|
| FrameBuffer/BorrowedView新增字段/函数签名 | T017 | 单位/容量/生命周期及消费者确认 | TODO |
| 高位深预测/变换入口 | T018/T019 | 独立向量与整数范围 | TODO |
| 10-bitQP100实际参数/coeff尺度 | T019/T020/T022 | 规范/AVM与header一致 | TODO |
| 444 profile/字段/合法TX | T017/T021/T022 | 来源表与固定oracle probe | TODO |
| API/recon元数据最小扩展 | T024/00 | 不丢配置、ABI影响和容量语义 | TODO |
| M3结果schema扩展 | T025/00 | M2兼容与负例通过 | TODO |

主窗口在每条冻结后登记签名、依据、日期、双方确认与hash。尚未执行的内容不得改成DONE。

## 10. 2026-10-11 执行启动与首次接口冻结

用户已授权全自动开发和必要依赖补齐，本轮正式RUNNING。A为`/root/m2_pipeline`，B为`/root/m2_syntax`，00为当前主会话。两个子智能体直接通信；不使用OpenCode。

G0：86文件指纹核对并复制到`artifacts/m3-root/m2-baseline-source`，新的MSVC Release /WX clean构建28/28通过；四尺寸×四帧数M2矩阵16/16通过，证据`artifacts/m3-root/g0-matrix/acceptance.json`。规范PDF SHA256 `e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f`，oracle SHA256保持M2冻结值。

已批准A接口方向：FrameBuffer/BorrowedView携带bd/chroma；planes与strides保持byte、内部安全LE样本访问；新增picture alloc/view ex、geometry init ex、bd transform/quant/path入口，保留旧8/420wrapper。预测reference使用uint16样本；残差int16，coeff/q/dq int32且宽乘加。实际完整签名由A/B同步后在报告登记。

已批准公开ReconFrame末尾追加bit_depth/chroma_format，chroma宽高注释改为实际可见样本尺寸；Picture既有byte planes/strides布局不改。该Recon公开struct布局变化需调用者全量重编，不声称旧二进制ABI兼容。

B已从固定AVM核对profile0为42010、profile4为44410，bit_depth_idc LUT={10,8,12}；10-bit frame qindex字段9bits，8-bit8bits。profile与完整条件、量化参数须继续对照实际header和解码证据，生产通过前不置DONE。

## 11. 2026-10-11 最终冻结与验收 DONE

本文件待冻结表已由最终实现/两窗精确签名和主窗口验收关闭；§9的TODO是启动时历史状态。M3最终0.0.3-m3支持8-bit420、10-bit420/444，byte stride/u16le与非对齐读取、各格式元数据、容量/PTS/EOS真实通过。QP100→10bit qindex148、quant2816、CDF类别2；42010/profile0、44410/profile4。公开ReconFrame新枚举字段要求调用者重编。

48项生产矩阵、两种新格式各1000帧、七个构建配置与sanitizer全部通过。8-bit420的16项码流/重建及1000帧保持M2指纹。8-bit444/422/12-bit/400拒绝。完整签名、来源、范围和日志在两窗报告及 `task-reports/M3-integration.md`。不扩大到任意QP、帧间、SIMD、多线程或FFmpeg wrapper。
