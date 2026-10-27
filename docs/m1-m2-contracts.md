# M1→M2 接口契约（00 冻结）

日期：2026-10-07。负责人：00 集成与调度会话。本文件冻结 T007–T016 之间的**最小接口**。每项记录 owner、使用者、单位、支持范围、失败方式。

本文件只冻结**跨模块边界**。模块内部实现（算法选择、内部缓冲、helper 命名）由属主决定，不需要经 00 批准；但**一旦有第二个模块依赖**，就必须按本文件。

路径相对 `svt-av2\`。文件归属见 `docs\m1-m2-paths.md`。

## 0. 本轮冻结的编码配置（先于一切模块）

沿用 M1 已验收配置，**不扩大**：

| 项 | 冻结值 | 说明 |
|---|---|---|
| bit depth | 8-bit | 10-bit 是 M3 |
| chroma | 4:2:0 | |
| 线程 | 1 | worker/pipeline 是 M8 |
| QP | `base_q_idx = 100` | 固定；`>0` 保持非 lossless |
| 帧类型 | All-Intra / CLOSED_LOOP_KEY | 无 inter、无参考帧 |
| 预测 | DC_PRED | 额外模式只有闭环确需才加 |
| CDF 更新 | `disable_cdf_update = 1` | 但 **context 必须按实际 q/plane/size 选择**，不得一律用 M1 首块的 ctx |
| 滤波 | 全关（deblocking[0..1]=0） | 故 final recon 等于重建阶段输出 |
| 目标尺寸 | 64×64 → 176×144 → 1280×720 → 1920×1080 | G2 先只做 64×64 |

**尚未冻结**（T009/T010/T011 必须合作选定后报 00 记入本节）：transform 尺寸/类型组合与 partition 组合。要求：必须是规范允许的**最小**组合，且能表达一般测试残差的非零 DC **和** AC。**不要**预设 AV1 4×4 可直接用，也**不要**为省事沿用 M1 的 64×64 最大 transform。

## 1. T007 → T008 / T012 / T013：picture plane 与块几何

**Owner**：T007。**使用者**：T008（取邻居）、T012（读写 recon）、T013（同步复制输入）。

### 1.1 memory / stride / 边界可读范围

- 样本类型 8-bit：`uint8_t`。**stride 单位为字节**，不是样本数。二者在 8-bit 下相等，但类型上必须区分，10-bit（M3）时不再相等。
- 采样地址公式（全项目统一）：`plane[row * stride + col]`。
- 可见区域：`width` × `height`（luma）；chroma 为 `width >> 1` × `height >> 1`（4:2:0）。
- 分配高度与可见高度**分离**：缓冲可分配 `aligned_height` 行（含 padding），但**可见区域之外的内容不属于契约**，任何模块不得依赖其数值。
- 可读边界：`row ∈ [0, aligned_height)`，`col ∈ [0, stride)`。**预测邻居提取**只允许读可见区域内已重建样本 + 由 §1.3 规则填充的 padding，不得读越界内存。
- **调用者责任**：API/接口只接收 stride，无法验证外部缓冲的真实长度。凡不能证明长度处，必须写入接口注释，**不得**虚构"自动检测指针容量"。

### 1.2 所有权

- 输入 picture（源）：**调用期消费**。`send_picture` 返回后库内不得保留外部 plane 指针（同步复制或当帧内用完）。
- recon picture：**库拥有**，生命周期至下一帧（或 `deinit`）。导出见 §7。
- 源图像与 recon **必须是两个可区分的缓冲**。禁止把 source 直接当 recon（§9 明确禁止）。

### 1.3 padding / 邻居可用性（与 §2 的分工）

- T007 负责**物理扩容**（分配 `aligned_width`/`aligned_height`、stride 对齐）。
- 邻居**是否可用**由规范决定，属于 T008；**不得**用"扩边填充"替代规范的不可用邻居规则。
- 二者接口：T007 提供 padding 区域与访问宏，T008 决定何时使用及取替代值。

### 1.4 失败方式

空指针、`stride < visible_width`、宽高为 0、尺寸溢出（`width * height` 超 `size_t`）、不支持的奇数尺寸：**返回明确负值，不部分写入**。溢出检查必须在乘法前做，不得依赖事后判断。

## 2. T008 → T012：预测输出、邻接可用性、residual 类型

**Owner**：T008。**使用者**：T012。

### 2.1 预测输出

- 输出写入 T007 的 picture 布局，**遵循同一 stride**（不得假设紧密排列）。
- 值域 `[0, 255]`。

### 2.2 邻接可用性

- 预测邻居**必须**从内部 recon 提取，**不得**从未编码 source 提取。
- 首块、图像边界、tile 边界、padding 可读范围**分开处理**。
- **分工（T008 提、00 登记，2026-10-07）——「几何可用性」与「tile 内无前驱」是两件事**：
  - `svt_av2_block_neighbors()`（T007 属主）**只报几何有效性**，且**只覆盖图像边界**。它**没有** `_flags` 参数，也**不得**为 tile/superblock 边界扩展语义。
  - **tile 内"无前驱"由调用方（T012）显式声明**：通过 `svt_av2_intra_reference_build_flags()` / `svt_av2_intra_dc_predict_flags()` 的 `have_left`/`have_above` 入参传 0。
  - 依据：`block_geometry.h:228-231`（`svt_av2_block_neighbors` 签名仅 `picture/plane/block/neighbors`）；`intra_prediction.h:123,161`（`_flags` 变体）。
  - **越权边界**：`_build_flags` 在对应 flag 为 0 时**不得读取**该方向 picture 样本（避免读越界/未初始化）。
- 跨块必须使用**此前已重建**样本；禁止读未来/未初始化数据。首块与后续块输出不得依赖未初始化内存。
- 具体取代值（DC 类模式的不可用邻居处理）须逐项给出规范条款号，写入 T008 报告。

### 2.3 residual 有符号类型

- `residual = source - prediction`，范围 **-255 .. +255**。
- 类型：**`int16_t`**（明确有符号，宽度足够容纳 -255..255 及后续变换中间量）。
- **禁止**在 `uint8_t` 中相减后截断——这是本项目最容易犯且最难查的一类错误。

## 3. T009 ↔ T010：transform 尺寸/类型、整数尺度、coeff 类型

**Owner**：T009 与 T010 **共同**（尺度必须双方一致，任一方单独改即为回归）。**使用者**：T011（scan/系数排序）、T012（重建链）。

- **transform 类型（00 裁定，2026-10-07）**：**只做 `DCT_DCT`**（DC_PRED 允许集内），不引入 ADST/IDTX/DDT。AV2 的逆变换是**矩阵乘**（`avm/av2/common/txb_common.c` 的 `tx_kernel_dct2_sizeN[INV_TXFM]`），**不是** AV1 蝶形；禁止按名称推定复用 AV1 kernel。
- **transform 尺寸（00 裁定，2026-10-07）**：M2 首版实际用到的是 **luma `TX_64X64` / chroma `TX_32X32`**，由冻结帧头推导，不是 8x8：
  - 帧头硬编码 `tx_mode_select = 0`（`svt-av2/Source/Lib/Bitstream/frame_header.c:245-248`，T015 更正行号）→ `TX_MODE_LARGEST`；
  - `tx_mode_to_biggest_tx_size[TX_MODE_LARGEST] = TX_64X64`（`avm/av2/common/common_data.h:703-706`）；
  - 顶层 `PARTITION_NONE` @ 64x64 经 `tx_size_from_tx_mode()`（`avm/av2/common/blockd.h:2580-2589`）得 luma `TX_64X64`；chroma 经 `av2_get_tx_size(plane>0, xd)` → `av2_get_max_uv_txsize()` 得 `TX_32X32`（`blockd.h:3237-3241`、`:3208-3215`；`get_plane_block_size(BLOCK_64X64,1,1)=BLOCK_32X32`，`common_data.h:726`；`max_txsize_rect_lookup[BLOCK_32X32]=TX_32X32`，`common_data.h:429`）。
  - 该推导与 M1 已冻结 21B 向量一致（`block_writer.c:55-56` 已写 `txSzCtx 4`(luma)/`txSzCtx 3`(chroma)）。
  - **帧头不新增 tx_size 语法**；T011 的 scan/EOB 按 `av2_get_max_eob` = **1024**(luma TX_64X64) / **1024**(chroma TX_32X32) 做。
    - `av2_get_max_eob()`（`avm/av2/common/blockd.h:3477-3490`）对 `TX_64X64`/`TX_64X32`/`TX_32X64` 特判返回 **1024**；`TX_32X32` 走默认 `tx_size_2d` = 1024。
    - 与 `inv_txfm_c` 只读 `min(1024, w*h)` 个系数一致（`idct.c:705,708`）——即 64x64 只编码 32x32 低频系数，再由 2× 复制上采样到 64x64。
    - **更正记录**：00 首版裁决书曾误写 4096，已在 T011 的更正消息与本节更正为 1024。
  - 路径 (C)（`tx_mode=TX_MODE_SELECT`）**会读 tx partition 符号**：`allow_tx_horz_split/vert_split(BLOCK_64X64, TX_64X64)` 均为真（`get_tx_size(64,32)=TX_64X32`、`get_tx_size(32,64)=TX_32X64` 合法，`avm/av2/common/av2_common_int.h:5198-5214,5058,5035`；`coding_block_disallows_tx_partitioning(BLOCK_64X64)=false`，同文件 `:5193-5196`），故 `read_tx_partition` 会读 `do_partition`（`decodeframe.c:1456-1460,1380-1385`）。**注意**：即使 (C) 也需要写新符号才能保持 `TX_64X64`（写 `do_partition=0`），且单独 (C) 仍到不了 `TX_8X8`。此条更正 00 首版裁决书的括号说明（原写"不允许再分"有误）。
  - 模块支持集合：4x4/8x8/16x16 + 4x8/8x4/4x16/16x4/8x16/16x8 + 32x32 + 64x64（矩形 16x32/32x16/32x64 同族）。
  - **正/逆不对称（T009 复核属实，00 采纳并更正首版描述，2026-10-07）**：首版裁决书只描述了逆变换，误把结论外推到正变换。实际两者结构不同：
    - **逆变换**（`avm/av2/common/idct.c`）：`width=min(32,64)=32`、`height=32`（`:648-649`），`skipWidth=skipHeight=0`（`:687-688`），只读 `min(1024,w*h)=1024` 个系数（`:705,708`），跑两遍 **32 点核**（`tx_wide_index=min(5,6)-2=3` → `inv_txfm_dct2_size32_c`，`:724-728`），再做 **2× 复制上采样**（`:730-751`）。→ "32×32 核 + 2×2 复制"。
    - **正变换**（`avm/av2/encoder/hybrid_fwd_txfm.c`）：`width=tx_size_wide[TX_64X64]=64`、`height=64`（**无 min 钳位**，`:952-953`），`tx_wide_index=6-2=4` → `case 4` → **`fwd_txfm_dct2_size64_c`（64 点核）**（`:955-956,939`），`skipWidth=64-32=32≠0`（`:984`），全 64×64 计算后**高半部置零**，再 **repack 到前 32×32**（`:1003-1008`）。→ "64 点核 + 置零 + repack"。
    - 共同点：`sqrt2=((log2w+log2h)&1)`，64×64 为 `(6+6)&1=0`，**两向都不触发 sqrt2**（`idct.c:695`、`hybrid_fwd_txfm.c:1012`）。
  - **coeff 长度契约（00 裁决 (甲)，2026-10-07）**：`coeff`/`qcoeff`/`dqcoeff` 数组长度 = **`min(w,32) * min(h,32)`**，即 `av2_get_max_eob(tx_size)`。故 **TX_64X64 → 1024**（32×32 网格，**不是 4096**）、TX_32X32 → 1024、TX_16X64 → 512、TX_64X16 → 512、TX_8X64/TX_64X8 → 256、TX_4X64/TX_64X4 → 128；w,h ≤ 32 时即 `w*h`。
    - 依据：正变换出口经 repack 后为前 32×32（`hybrid_fwd_txfm.c:1003-1008`）；逆变换入口只读 1024（`idct.c:705,708`）；码流侧 `av2_get_max_eob` 特判（`blockd.h:3477-3490`）。三者自洽。
    - 选 (甲) 而非 (乙)（保持 4096 + 高半部恒 0）：(甲) 与 AVM 出口、`max_eob`、scan 表完全一致，T011 无需在 4096 长数组上只扫前 1024。
    - 布局仍为 **raster 行主、DC@0**（对本轮 64×64 即 32×32 网格的 raster 行主）。
    - **行 stride 澄清（00 裁定，2026-10-07，T012 提出分歧后补）**：`coeff`/`qcoeff`/`dqcoeff` 的**行 stride = `min(w,32)`**（即 `kw`），**不是** `block_w`、**不是** `scratch->stride`。TX_64X64 时三者均为 **1024 项、行 stride 32**；索引一律 `coeff[row * min(w,32) + col]`。
    - **依据（本澄清消除一处真实歧义）**：T011 的 scan 建立在 32 宽调整网格上（`coeff_syntax.c:83-86 adjusted_dim = min(d,32)`、`:144 scan[n] = row*w + (d-row)`、`:165/:742-747 qcoeff[scan[c]]`）；若 `qcoeff` 被写成 64 宽 raster，则 scan 的 index 会指向**零填充区**，T011 只编出第一行系数——**静默错值**（"承载网格 ≠ tx_size" 同族）。故 00 裁定 **(甲) 且行 stride = min(w,32)**；`intra_frame.h` 中 "the block-local stride used for every block buffer (scratch->stride)" 一句对 **coeff 数组不适用**，仅适用于样本类缓冲（prediction / residual / inverse_residual）。
  - **scan 表（T015 曾误推为 luma 4096，00 用 AVM 源码更正，2026-10-07）**：TX_64X64 在**所有 tx_type** 下都使用 `default_scan_32x32`（1024 项）——AVM 注释即写"tx64 高频半数为零，故用 tx32 的 scan 序"（`avm/av2/common/scan.c:1804-1806`（`av2_default_scan_orders[TX_SIZES]`）、`:1888-1890`（`av2_scan_orders[TX_SIZES_ALL][TX_TYPES]` 的 TX_64X64 段））。故 **luma 与 chroma 的 scan 均为 1024 项**，无 4096。
  - **调用约定（T010 实测提出、00 采纳，2026-10-07）——「承载网格 ≠ tx_size」**：TX_64X64 的量化必须用**原始 tx_size `(64,64)`** 初始化参数（`tx_scale=2`）；承载网格 32×32 / `count=1024` **不改变** `tx_scale` 推导。`tx_scale` 依据原始 `tx_size` 枚举（`avm/av2/common/idct.c:761` `av2_get_tx_scale(tx_size)`），**非**网格尺寸。
    - 反例（实测）：`init_tx(32,32).tx_scale=1` → 反量化少移一档 → `dqcoeff` **系统性偏大 2× 且不报错**。证据 `artifacts\T010\tx64x64-layout-probe.log`。
    - 该陷阱与"降尺度归属""正向漏 `>> log_scale`"同族：2 的幂次移位在 `tx_scale=0` 时恒等、在 32/64 下才暴露。
- **整数尺度（00 裁定，2026-10-07）**：
  - `tx_scale = (pels > 256) + (pels > 1024)`，**以代码为准**（`avm/av2/common/idct.c:761-765`），不用 `quant_common.c` 的注释分级。完整表：4x4..16x16 → **0**；16x32/32x16/32x32 → **1**；32x64/64x64 → **2**。
  - **`log_scale ≡ tx_scale`，是同一个量**，唯一赋值点 `avm/av2/encoder/encodemb.c:819`（`qparam->log_scale = av2_get_tx_scale(tx_size)`）。**不得**按位深另立第二套规则。
  - `inv_tx_shift` / `fwd_tx_shift` 表见 `avm/av2/common/common_data.h:805-857`（T009 落盘 `docs/task-reports/T009-scales.md`）。**32/64 家族实测值（T009 复核，00 采纳）**：`inv TX_32X32={6,13}`、`inv TX_64X64={6,13}`、`inv TX_16X32={6,12}`、`inv TX_32X64={6,12}`；`fwd TX_32X32={3,12}`、`fwd TX_64X64={3,14}`、`fwd TX_16X32={2,13}`、`fwd TX_32X64={3,14}`（`common_data.h:809-810,815,817,837-838,843,845`）。
  - 舍入：**在幅度上做 floor，符号在舍入后还原**；不得引入 away-from-zero。AVM 量化/反量化用的都是非负版本（`avm/avm_ports/mem.h:45,53` 的 `ROUND_POWER_OF_TWO`/`_64`；反量化 `decodetxb.c:918-926`、正量化 `av2_quantize.c:101-128` 均在幅度上舍入）。
  - 逆变换逐步：sqrt2 预处理仅 `log2w+log2h` 为奇数时触发（`NewInvSqrt2=2896`，`round_shift` 为**算术右移**）；第一维 clamp 到 `±(1<<(bd+7))`（8-bit: -32768..32767），第二维 clamp 到 `[-256,255]`（8-bit）。`>>` 是算术右移，`add = 1<<(shift-1)`，即"加半个后向下取整"。
  - overflow：8-bit 下 8x8 中间和约 2.33e7，远小于 INT32_MAX；32/64 尺寸用 **int64 累加**。
- **coeff 布局（00 裁定，2026-10-07）**：**raster / 行主 `coeff[y*w + x]`，DC 在 index 0**，T009/T010/T011/T012 四方一致；scan 由 T011 按规范做。
- coeff 类型：**`int32_t`**（提供足够的中间位宽，不在此处过早收窄）。`|qcoeff| <= 8191` 是 **T010 自选的保守生产上限**（**不是**解码器要求：`decodetxb.c:907` 的实际检查是 `(level >> 20) != 0`，即接受 `2^20-1`；见 §4 未决项）；`dqcoeff` clamp 到 `[-32768, 32767]`。
- **数据流方向（00 确认）**：T009 coeff → T010 qcoeff+dqcoeff → T009 `inv_transform(dqcoeff)` → T012 recon。T009 是 coeff 生产者、dqcoeff 消费者；**T010 从不生产 coeff**。
- **降尺度归属（00 裁定，2026-10-07，T010 提出、T009 原始主张、00 采纳）**：**降尺度/升尺度由量化器完成，transform 出口不降尺度。**
  - 依据：lossy 正变换出口 `fwd_txfm_c`（`avm/av2/encoder/hybrid_fwd_txfm.c:948-1018`）**零 tx_scale 引用**，只有 `fwd_tx_shift[]`（`:995-1001`）+ sqrt2 归一（`:1013-1017`）；`tx_scale` 在编码器侧消费于 `encodemb.c:376 → :401-405/:418-422` 的 `>> shift`（是反量化重建，不是 transform），解码器侧消费于 `decodetxb.c:526/:673 → :625/:922` 的 `dq_coeff >> shift`。
  - **为什么现在冻结**：本轮 `tx_scale=0` 时"transform 自带降尺度"与"量化器降尺度"**不可观测地等价**；但扩到 32×32/64×64（M2 真实尺寸）后若按前者接线，系数会整体偏大 `2^tx_scale` 倍，T012 重建直接错。这与 T010 第 11 项（正向漏 `>> log_scale`）是**同一类**"2 的幂次移位在 tx_scale=0 时恒等、长期不可见"的 bug。
  - **例外（登记备查，本轮不涉及）**：`av2_lossless_fwd_idtx_c`（`hybrid_fwd_txfm.c:27`，lossless 专用，且是**升**尺度 `2^(3-tx_scale)`，方向相反）与 IST 二次变换的能量估计（`:1140`，非系数输出）。**开 lossless 或 IST 时不得混用本套尺度语义。**
- **禁止**假设 `forward + inverse == 源残差`：有整数舍入与量化时该等式不成立。测试必须按各自路径的期望值写，不能用往返相等当唯一判据。
- 边界块：采用冻结的合法 padding/crop 规则，不得读有效区域外未初始化数据。
- 帧头 `tx_mode` 不在单个会话内擅改；需变更报 00。

## 4. T010 → T011 / T012：qcoeff / dqcoeff / EOB

**Owner**：T010。**使用者**：T011（写 syntax）、T012（逆路径重建）。

- `coeff`（T009 输出）→ `qcoeff` → `dqcoeff`。
- 类型：`qcoeff` 为 **`int32_t`**（含符号与幅度）；`dqcoeff` 同为 `int32_t`。`coeff`/`qcoeff`/`dqcoeff` 均 `int32_t`。
- **冻结量化参数（00 登记，2026-10-07，base_q_idx=100）**：
  - `dequant[DC] = dequant[AC] = qlookup(100) = 704`，三平面相同（`avm/av2/common/quant_common.c:130-146`）。
  - 反量化：`dq = ROUND_POWER_OF_TWO(level * 704, 3) >> tx_scale`（`decodetxb.c:918-926`）。
  - 正量化：`deadzone = ceil(dequant / 2^(1+log_scale+3))`（q=100,log_scale=0 → **44**）；`level = ((|c| + ROUND(33,log_scale)) * 11915) >> (16 - log_scale + 4)`（`avm/av2/encoder/av2_quantize.c:97-128`）。**`log_scale`/`tx_scale` 随尺寸变化，见 §3**——不再只对 ≤16x16 有效。
  - **正量化的 dequant 重建必须含 `>> log_scale`（T010 第 11 项实现错，已修，2026-10-07）**：`dqcoeff = ROUND_POWER_OF_TWO_64(abs_qcoeff * dequant, QUANT_TABLE_BITS) >> log_scale`（`avm/av2/encoder/av2_quantize.c:118-121`）。**先按 `QUANT_TABLE_BITS` 舍入，再 `>> log_scale`，顺序不可交换。** `log_scale=0` 时恒等（故 ≤16x16 曾全绿），但 32×32/64×64 下漏掉会让 dqcoeff 偏大 2×/4×。反量化路径 `svt_av2_dequant_coeff` 本就有 `>>= tx_scale`（`quantization.c:234-235`），只有正向漏了。锚点（T010 实测）：`log_scale=1`: shift 19/deadzone 22/首个非零 28→level1 dq44；`log_scale=2`: shift 18/deadzone 11/首个非零 15→level1 dq22。
  - 数组长度（`coeff`/`qcoeff`/`dqcoeff`）：**`min(w,32)*min(h,32)`**，见 §3。TX_64X64 → 1024（**不是 4096**）。
  - 只支持 `base_q_idx=100`；其他值一律 `UNSUPPORTED`（不 clamp、不静默）。
  - 未纳入项（仅登记）：`decodetxb.c:621-629/:918-926` 反量化前的 `& 0xffffff` 在 q≈100 恒等（8191*704≈5.77e6 < 2^24）；扩 QP 到 q≈255 时非恒等，须重新核对。
  - **LEVEL_OVERFLOW 余量（00 终裁，2026-10-07，取代 T010 两版读数）**：`residual ∈ [-255,255]` 且冻结 `fwd_tx_shift` 下该分支**不可达**，但余量**随尺寸变薄，不得用 ≤16x16 的值外推**。**权威口径 = 实测**（下表 `|coeff|` 与 `level`）：4x4 **88.1x** / 8x8 **44.3x** / 16x16 **22.1x** / **32x32 11.0x（M2 chroma）** / **64x64 5.5x（M2 luma）**（`|coeff|` 上界 8160/16320/32640/32640/**32640**，`level` 上界 93/185/371/742/**1483** < 8191）。
    - **判据来源**：`artifacts\T010\probe_coeff_range.c`（**实测**：对每个几何扫 flat±255 / 棋盘 / 冲激四类合法极值残差）。00 本轮在 `build/00-wsl-gcc-verify` 上**亲自复跑**，逐值复现。
    - **解析上界已由 00 重建**：`artifacts\00\probe_level_bound_exact.c` 从**真实核**测 `R = max_freq Σ_k |M[f][k]|`（实测 R = 64·N：256/512/1024/2048/**4096**，**非** T010 用的松散界 `N·90`），得**精确**上界 8160/16320/32640/32640/**32640**，与实测**逐值相等**（flat 255 的 pass-1 输出为 DC-only，故该上界可达）。⇒ 解析与实测**一致**，**均指向 5.5x**。
    - **更正记录（2026-10-07）**：64x64 曾出现三个读数 —— 首版 `2934 / 2.8x`（错，按单轴 N=64 两次累加）、T010「更正版」`32273 / 1467 / 5.6x`（**仍错**：既用松散界 `N·90`，又把 pass-2 **累加长度**误当 32；`transform.c:596` 的 `fwd2_lines` 钳的是 pass-2 的**输出行数**，1-D 累加长度仍是 `size`=N）。**00 裁决：以实测 `1483 / 5.5x` 为准**；`5.6x` 读数**作废**。
    - 方向不变且更应强调：22.1 → 11.0 → 5.5 **单调变薄**，**不得**用 ≤16x16 的值外推。
- **零块与非零块必须可区分**：需要明确的 `all_zero` 判定输出供 T011 使用。
- **`qcoeff` 与 `dqcoeff` 不得共用同一 buffer 互相覆盖**：T011 写 syntax 需要 qcoeff 的符号/幅度，T012 重建需要 dqcoeff，二者必须同时有效直至使用完毕。buffer 生命周期写入契约。
- **EOB 归属（重要）**：**scan/EOB 由 T011 按规范计算**，不由 T010 输出。T010 只保证 qcoeff 数组内容正确；T011 负责 scan 顺序、last position、EOB。若 T011 认为需要 T010 提供额外信息，发消息协商并在本文件登记，不各自实现一份。
- **写入 T011 的 qcoeff 与内部重建使用的系数必须同源**（同一个 qcoeff 数组经 T010 逆量化得到 dqcoeff），不得各自量化一遍。
- 逆量化必须与 decoder 规定一致；**不得**因为"forward→inverse 误差小"就判正确。
- 反量化 overflow 必须有测试。dequant 表/常量记录版本与来源，**不得**未经核对复制 AV1 表。
- QP 范围：只支持 100。API 允许 0..255 **不代表**全 QP 支持；扩大范围须先经 00 确认并补对应 CDF/表测试。

## 5. T011 ↔ T012 / T013：真实 block syntax、tile writer 输入、context、容量

**Owner**：T011。**使用者**：T012（提供 qcoeff）、T013（组装完整流）。

- 复用现有 `EntropyCoding\`（`entropy_encoder.*`、`cdf_tables.*`、`block_writer.*`）与 `Bitstream\tile_writer.*`。**禁止另写第二套 range coder**（现有 coder 已与规范 §8.2 及 AVM `entdec.c` 交叉验证）。
- context 选择必须按**实际** q / plane / transform size / 邻居状态；**不得**一律使用 M1 首块的 ctx12/ctx0/ctx6。`disable_cdf_update=1` 只表示不更新，**不表示**不存在其他 context。
- scan / EOB / last position / zero-run / 幅度条件 / 符号规则逐项给出规范依据。
- **scan / coeff 长度已冻结（00，2026-10-07）——不得设计"通用 scan 尺寸抽象"**：`count = min(w,32) * min(h,32)` = `av2_get_max_eob(tx_size)`；本轮 luma `TX_64X64` → **1024**、chroma `TX_32X32` → **1024**。scan 表：`TX_64X64` 在**所有 tx_type** 下用 `default_scan_32x32`（1024 项），依据 `avm/av2/common/scan.c:1799-1807`（`av2_default_scan_orders[TX_SIZES]` 的 TX_64X64 项）与 `:1887-1894`（`av2_scan_orders[TX_SIZES_ALL][TX_TYPES]` 的 TX_64X64 段），AVM 注释原文 "Half of the coefficients of tx64 at higher frequencies are set to zeros. So tx32's scan order is used."。
  - **反模式警告**：`w*h` 的直觉（64x64→4096）是本轮已被 4 处独立证据否定的错误，且**"先做通用抽象、等冻结再填"正是产生该错误的路径**——通用抽象的默认值必然是 `w*h`。实现必须直接按 `av2_get_max_eob(tx_size)` 取长度（**不硬编码 1024**，以便将来扩 TX_16X64 自然得 512），但**不为其设计抽象层**。
  - 同源错误链（全部为"≤16x16 全绿、扩到 32/64 才暴露"）：`max_eob 4096→1024`、`正/逆不对称`、`漏 >> log_scale`、`承载网格≠tx_size`、`降尺度归属`、`LEVEL_OVERFLOW 余量外推`、`scan 4096→1024`。
- tile/frame 重置：每 tile 的 symbol coder 初始化与结束、frame 保存状态按规范处理。
- **容量**：tile / header / coded / precarry 容量必须随实际负载管理，接口**必须返回明确的"空间不足"**状态。**禁止**保留 M1 的小栈缓冲（`SVT_AV2_TILE_PAYLOAD_MAX_BYTES 32u`）来承担 1080p / 多帧——该常量仅对 M1 配置有效，M2 必须替换为按实际 payload 推导的容量。
- 右/下边界：fixed partition 在边界可能有强制 split 或不同 syntax；合法规则由 T011 与 T015 共同确认并通知 T013。**禁止**靠写超出图像的块来凑整帧。
- **边界 SB 的 partition 符号（00 裁定，2026-10-07，T007 从 decoder 侧核实）**：固定顶层 `PARTITION_NONE` 时，**右/下不完整 SB 不写 partition 符号**；完整 SB 仍写 `do_split=0`。
  - 依据：`avm\av2\decoder\decodeframe.c:1764-1782`——`is_do_split_implied()` 为真时 `do_split` 由 `implied_do_split` 直接赋值，**不调用 `avm_read_symbol`**；只有 else 分支（`:1777-1780`）才读 `do_split_cdf`。边界判定见 `av2_common_int.h:4545-4546`（`has_rows`/`has_cols`）。
  - **适用条件**：仅"顶层 PARTITION_NONE + 单 SB + BRU inactive"成立。BRU/bridge frame 会改变 `do_split` 取值（`decodeframe.c:1766-1770`），本轮不实现；将来引入必须重新核对。
  - **分工**：T007 只提供几何事实（哪些 SB 在边界、有效尺寸），**是否写符号由 T011 决定**。
  - 该结论**不改变邻居可用性规则**：边界块的上/左邻居是否可用仍按规范逐块判断（T008）。
- 旧 M1 显式向量测试必须仍可解释；**不得**为让新流通过而修改 oracle。

## 6. T012 → T013 / T014：frame core、重建生命周期、recon 导出

**Owner**：T012。**使用者**：T013（接线）、T014（比较）。

- 每块**只允许这一条链**：
  `source → prediction → residual → coeff → qcoeff → dqcoeff → inverse residual → clipped recon`
- 8-bit clipping 到 `[0, 255]`。
- **内部 recon 必须在 decoder 运行之前产出**；调用 decoder 是**外部检查**。严禁先跑 `avmdec` 再复制其 raw 命名为 internal recon。
- 导出布局：**紧密排列 8-bit 4:2:0 raw**（与 `tools\compare_recon.py` 一致），
  长度 = `W*H*frames + 2*(W/2)*(H/2)*frames`。导出前**移除内存 padding**。
- 必须注明**可见区域**与**帧序号**。
- 滤波全关时，final decoder 输出应等于该重建阶段输出；若某必需工具无法关闭，必须说明实际 final-recon 阶段，**不得**用未滤波中间图去比最终图。
- 每帧复用 scratch/系数缓冲，**不 per-block malloc**。
- All-Intra 无 inter 参考，但帧内邻接状态必须正确清零/重置。
- buffer 所有权/有效期明确写入接口。
- **导出描述符（00 登记，2026-10-07，T012 交付）**：`SvtAv2ReconExport` 描述符 = `frame_index` + 可见区域（w/h）+ `byte_count`；打包复用 T007 唯一原语 `svt_av2_picture_export_raw`（**禁止**另写第二份 raw 拷贝）。
- **变换/量化接缝（00 登记，2026-10-07）**：frame core 通过函数指针接缝 `SvtAv2IntraTransformPath`（`Source/Lib/Encoder/transform_path.h`）调用 T009/T010；`forward`/`inverse` 任一为 `NULL` 时 encode **立即返回 `SVT_AV2_ERROR_UNSUPPORTED`**（失败关闭，绝不把 source 写进 recon、绝不当零残差）。真实绑定只在一处（`transform_path.c`）。

## 7. T013 → app / 调用方：send_picture / get_packet 与 packet 生命周期

**Owner**：T013（公共头 `include\svt_av2.h` 由 00 串行修改，T013 提方案）。

- 最小行为冻结（同步单线程）：
  - 输入消费时点：`send_picture` **调用期消费**（或同步复制），返回后不保留外部指针。
  - 是否允许未取 packet 前继续 `send_picture`：**不允许**（单帧在途），返回明确状态而非静默覆盖。
  - packet 所有权：**调用方提供缓冲并拥有**；库不得保存悬垂指针。
  - buffer 有效期：至下一次 API 调用。
  - EOS / flush：`send_picture(NULL)` 表示 EOS。
  - 无输出与失败：**可区分**的状态；不得返回虚假成功。

### 7.1 packet 出流接口（00 裁定并已落地，2026-10-07）

T013 在 `include\svt_av2.h` 落地的公共接口（00 串行修改，T013 实现）：

```c
typedef struct SvtAv2Packet {
    void*   data;     /* caller-owned output buffer (required)                */
    size_t  capacity; /* bytes available at data (in)                         */
    size_t  size;     /* bytes written, or required size when too small (out) */
    int32_t pts;      /* presentation timestamp of the packet (out)           */
} SvtAv2Packet;

SVT_AV2_API SvtAv2Error svt_av2_enc_get_packet_ex(SvtAv2Handle handle, SvtAv2Packet* packet);
```

语义（冻结）：

| 情形 | 返回 | size |
|---|---|---|
| 成功 | `SVT_AV2_ERROR_NONE` | 实际字节数；`pts` 为该帧 pts |
| 尚无输出 / EOS 后 | `SVT_AV2_ERROR_NONE` | `0`（与失败可区分） |
| 容量不足 | `SVT_AV2_ERROR_OUT_OF_MEMORY` | 所需字节数；**不写部分数据** |
| 未取 packet 前再 `send_picture` | `SVT_AV2_ERROR_INVALID_STATE` | — |
| EOS 后再 `send_picture` | `SVT_AV2_ERROR_INVALID_STATE` | — |

- 旧签名 `svt_av2_enc_get_packet(h, void*, int32_t*)` **保留**，但只报告可用性、不再假装投递字节（它没有容量参数，无法安全投递）。这是对 M0 遗留行为的收窄。
- 缓冲有效期至下一次 API 调用；库不保存悬垂指针。
- **NULL=EOS 裁定**：`send_picture(NULL)` 为 EOS 并返回 `SVT_AV2_ERROR_NONE`（与本 §7 冻结一致）；`tests\unit\test_api.c:147` 的旧断言（期望 `INVALID_PARAMETER`）属 M0 遗留，按本节改写。
- CLI 与库**必须走同一路径**（`app` 不得直调内部 keyframe 入口）；shared 与 static 行为一致。
- `frames` 语义：`0` = 不编码任何帧；正数 = 最多编码该数量；默认 = 处理所有可用帧。尚不支持多帧时**明确拒绝，不静默丢帧**。
- 输入检查至少验证：支持配置、picture 元信息、Y/U/V 非空、stride、生命周期。**不接受任意 `void*` 强转后假装验证**。
- 内部 recon 导出采用 §6 的最小方式（debug/API/CLI 之一，由 00 冻结）。原措辞"不扩成复杂永久公共 API"**已被 00 本轮裁决修正**：为让只 include `svt_av2.h` 的 app 能落盘 recon，须开**一个**与 `SvtAv2Packet` 同形的公共访问器 `svt_av2_enc_get_recon`（见 §7.2）。这是**最小扩张**（单函数、caller-owned、复用 §6 唯一打包原语），不是"复杂永久 API"。
- **容量**：输出缓冲依据实际 payload，空间不足要报告；不得沿用 app 的 4096 或 tile 小栈缓冲承担任意内容。

### 7.2 recon 导出接口（00 裁定并落地公共头，2026-10-07）

T013 经 `include\svt_av2.h` 暴露的公共访问器，**由 00 串行加入公共头**（T013 只实现，不改该头）：

```c
typedef struct SvtAv2ReconFrame {
    void*    data;           /* caller-owned output buffer (required)          */
    size_t   capacity;       /* bytes available at data (in)                   */
    size_t   byte_count;     /* bytes written, or required size when too small */
    uint32_t frame_index;    /* frame this recon belongs to (out)              */
    uint32_t visible_width;  /* luma samples (out)                             */
    uint32_t visible_height; /* luma rows (out)                                */
    uint32_t chroma_width;   /* visible_width  >> 1 (out)                      */
    uint32_t chroma_height;  /* visible_height >> 1 (out)                      */
} SvtAv2ReconFrame;

SVT_AV2_API SvtAv2Error svt_av2_enc_get_recon(SvtAv2Handle handle, SvtAv2ReconFrame* recon);
```

语义（与 `SvtAv2Packet` 逐条同形）：

| 情况 | 返回 | `byte_count` |
|---|---|---|
| 成功 | `SVT_AV2_ERROR_NONE` | 实际写入字节数；五个 meta 字段填好 |
| 缓冲不足 | `SVT_AV2_ERROR_OUT_OF_MEMORY` | **所需字节数**；**不写任何数据、不改 meta** |
| 无可用 recon（未 encode / EOS 后 / handle 无效） | `SVT_AV2_ERROR_INVALID_STATE` | 不写 |

- **唯一打包来源**：实现必须复用 T012 的 `svt_av2_recon_export_frame()`（其内部走 T007 的 `svt_av2_picture_export_raw`）；**禁止**出现第二份 raw 打包实现。
- **不返回内部指针**、不做 I/O；`data` 有效期 = 下次产出新 recon 的 API 调用前。
- **`--export-recon` 归属**：`app\SvtAv2EncApp\main.c`（T013）。app 只 include `svt_av2.h` + `y4m.h`，看不到私有 `SvtAv2EncoderContext.recon`，故必须经本访问器；**不**把 app 写权限划给 T012。
- **公共头先落盘的合法性**：`__declspec(dllexport)` 的声明在无定义时**不导致链接失败**（00 以 `cl /LD /W4 /WX` 实测：仅有声明、无定义的导出函数 → exit 0）。故 00 先加声明、T013 后补实现，不打断任何人的构建。

### 7.3 公共导出面清单（00 维护，2026-10-07）

`include\svt_av2.h` 的公共符号是**全局契约面**，任何人新增/删除都必须走契约变更流程（报 00）。当前 11 个：

| # | 符号 | 引入 |
|---|---|---|
| 1 | `svt_av2_error_to_string` | M0 |
| 2 | `svt_av2_enc_init_handle` | M0 |
| 3 | `svt_av2_enc_set_parameter` | M0 |
| 4 | `svt_av2_enc_init` | M0 |
| 5 | `svt_av2_enc_send_picture` | M0 |
| 6 | `svt_av2_enc_get_packet` | M0 |
| 7 | `svt_av2_enc_get_packet_ex` | M0 |
| 8 | `svt_av2_enc_deinit` | M0 |
| 9 | **`svt_av2_enc_get_recon`** | **M2（§7.2，00 落地）** |
| 10 | `svt_av2_enc_get_config` | M0 |
| 11 | `svt_av2_enc_get_build_info` | M0 |

- **守卫**：`artifacts\T013\p1_evidence.ps1` 的 `$expectedExports` 按**字面**断言这 11 个。**意外导出会 FAIL，新增必须刻意加入本表** —— 这是正确行为，不许为了让守卫变绿而放宽。
- **本表的权威性**：以 `include\svt_av2.h` 中的 `SVT_AV2_API` 声明为准（`#define` 不计）。当前实测 11 个。

## 8. T015 → T011 / T013：序列/帧配置、尺寸、level、多帧

**Owner**：T015（从 T013 划出；见 `docs\m1-m2-paths.md` §4）。

- 交付物：最小 sequence/frame 配置与 writer 调用约定、首帧/后续帧状态、header 生命周期。
- **不得**把 M1 的 `seq_level_idx = 0` / level 2.0 当作所有尺寸的合法值——1080p 需要重新核对 level、尺寸位宽、SB/tile 与边缘依赖。
- 尺寸位宽：`frame_width_bits_minus_1` / `frame_height_bits_minus_1` / `max_frame_*_minus_1` 必须按**真实尺寸**推导。
- **single_picture_header_flag**：M1 冻结为 1。多帧序列所需取值、帧头复用/更新、closed-loop key 条件、temporal unit 边界由 T015 核对实现，**不得**默认"拼接多个 21B still-picture 流"就是合法视频序列。
- 对 T011 交付影响 block/tile syntax 的开关与尺寸规则。tile 内部 partition/context 由 T011 实现，**不得**用 header 配置掩盖缺失的 block 编码。
- 对 T007 核对可见尺寸、编码几何与边缘有效范围。**不得**以修改输入尺寸或静默 crop 凑合法流。
- 流拼接 / 输出 / PTS / EOS 由 T013 实施，不属 T015。
- 保留可解释的 M1 固定 header 向量；**不得**为让新尺寸通过而改写 M1 期望值。
- **冻结的序列级开关取值（T015 定、00 登记，2026-10-07，本阶段全为常量）**：`enable_sdp=0`、`enable_ext_partitions=0`、`reduce_pb_aspect_ratio=0`（`max_pb_aspect_ratio_log2_m1` 保持默认 2）、`sb_size=64x64`（partition_config 写 2 位 `00`）、`enable_bru=0`、`enable_adaptive_mvd=0`、`enable_mvd_sign_derive=0`、`enable_flex_mvres=0`；帧头侧 `uniform_tile_spacing_flag=1`、单一 tile（每轴停止位全 0）。依据 `avm/av2/decoder/decodeframe.c:5975-5992`（partition）、`:6128-6131`（bru/mvd），字段表 `docs/t015-header-field-reference.md` §3.3。→ T011 可按「**PARTITION_NONE + 无 BRU + 无扩展分区**」固定分支实现，**无需判运行时开关**。
- **T011 的两个唯一接口（不得重复推导）**：`svt_av2_frame_header_sb_grid(cfg, &sb_cols, &sb_rows)`（64x64→1×1、176x144→3×3、1280x720→20×12、1920x1080→30×17）与 `svt_av2_frame_header_tile_params_bits(cfg)`。**`tile_params()` 位数不是固定值**：64x64=1、176x144=5、1280x720=10、1920x1080=11 位——**取接口，不要硬编码**。分区上下文 `miCols = 2*((W+7)>>3) = ceil(W/4)`。`frame_width_bits_minus_1`/`frame_height_bits_minus_1`/`max_frame_*_minus_1` 由 `svt_av2_sequence_header_write_multi()` 按尺寸统一写，T011 不碰。
- **单 tile 时**：本阶段 `seq_tile_info_present_flag=0`，tile 几何完全由帧头 `tile_params()` 推导（`NumTiles==1`）；`tile_start_and_end_present_flag` 与 `tile_size` **不写位**（spec 5.19.x，`NumTiles==1` 分支）。
- **M1 冻结路径未动（00 独立复验）**：多帧写成独立函数 `svt_av2_sequence_header_write_multi()`，`seq_level_idx=0` 保持；改 `frame_header.c` 后 64×64 出流仍为 21B `7AAB5986…CE0F7`，与 `artifacts\T005\frame_min_64x64.bin` **逐字节 0 差异**。
- **`ns(n)` 位宽**（T015 认知修正，规范 4.11.8 p.48）：`ns(n)` 位宽 = `FloorLog2(n)+1`；`ns(5,0)=2 位`、`ns(3,0)=1 位`。**T003 的 `bit_writer.c` `write_ns()` 实现本就正确，未改**（此前"少 2 位"是参考模型误用 `CeilLog2` 满宽所致）。

## 9. T014 → T016：runner / 素材 manifest / 结果格式

**Owner**：T014。**使用者**：T016。

- T014 交付：manifest 格式、CLI/API 调用方式、真实元信息检查、recon 比较入口、退出码/结果 schema、smoke 命令。
- **T016 复用这些入口**，使用独立平台/长跑目录（`build\T016\`、`artifacts\T016\`），**不复制** decoder 或比较算法。
- 几何判定必须来自 **decoder 真实输出元信息**，不能由调用者参数或文件长度反推（这是 M1 已复现的误报 bug）。
- 结果必须区分实际 `encoded_frames`，单测/端到端状态与未验证项分开记录。
- 缺 oracle / 缺 python / 缺规范时**不得 PASS**；"缺工具"必须显式失败或 BLOCKED。
- 若 T016 发现 runner 问题，先交 T014 修复并**交接版本**，再继续其测试。T014 修正后 T016 不得继续使用旧版本判定。

## 10. 全局禁止项（对本轮所有会话有效）

1. **不得**用占位实现（返回全零/常量）冒充模块完成。
2. **不得**复制 decoder 输出充当 internal recon。
3. **不得**通过关闭全部残差、或让路径恒等来规避测试。
4. **不得**修改 oracle、`avm\` 源码或比较工具来让测试通过。
5. **不得**在 correctness 未解决时做 SIMD 或性能优化。
6. **不得**在单线程不正确时引入多线程。
7. **不得**建立未来 M8 的 FIFO/worker 体系。同步单线程、每帧可复用缓冲足够。
8. **不得**把"能编码 / 能解码 / 文件更小 / exit 0"当作正确性证据。
9. 没有独立依据（规范条款或固定 AVM 向量）的算法交叉验证，必须标为**缺失**，不能由自比较替代。
10. 上游未就绪时，**推进本任务可独立完成的研究/测试**，不要把另一任务的 DONE 当作自己的验收。

## 11. 未定项（禁止猜测，需报 00）

- transform 类型/尺寸/整数尺度具体取值（T009+T010+T011 合作选定后登记 §3）。
- partition 组合与边缘块合法规则（T011+T015+T007 确认后登记 §5）。
- 多帧 header 具体字段与 single_picture_header_flag 取值（T015 核对后登记 §8）。
- level / 尺寸位宽对 720p/1080p 的取值（T015，§8）。
- recon 导出的最终形态（debug 文件 / API / CLI 三选一，00 冻结后登记 §7）。

## 12. 2026-10-10 最终 M2 契约（主窗口终验 DONE）

本节冻结本轮最终实现，覆盖 §11 及旧队列记录中的未定项；验收证据见 `task-reports/m2-codex-integration.md`。

- 公共 API/CLI 接真实帧核心，qcoeff 由 Codec 复制持有；编码树遍历每个叶块按 Y/U/V 顺序提供系数，tile 同步消费。输入可在调用后复用。
- tile 与重建统一 Common coded traversal。SB64，合法二叉边缘分区，全可见 8/16/32/64 方块叶；不使用 ≤64 square SPLIT。变换为固定 DCT_DCT，M2 `reduced_tx_set=2`。
- 支持 8-bit progressive420、QP100、threads1、8 倍数尺寸至 1920×1080；单 tile、关闭滤波；不扩展到帧间或任意 QP。
- M2 多帧序列，首包 sequence header、每包 temporal delimiter、8-bit order hint；sequence motion mode flags 为 4 个。目标 QCIF/720p/1080p 的 uniform tile 语法共 3 bit；每轴 unary 遇首个 0 停止，不能按最大 tile log2 写多个 stop bit。
- M2 色度 delta 字段写 f5 literal23，实际 base_uv_ac_delta_q=0；显式 M1 保留 literal0（实际 -23），其零残差 21 字节回归保持原样。
- CLI `--frames` 和 `--export-recon` 已完成，逐帧紧凑 raw yuv420p8；API 的 PTS/EOS、pending packet 防覆盖、容量不足重试均有单测。
- MSVC/GCC/Clang Debug/Release 与 sanitizer 验收通过；Linux 编码器原生运行，oracle 使用 Windows 固定 AVM。连续 1000 帧全部逐字节一致且哈希各不相同。
