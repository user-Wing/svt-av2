# 最小合法单帧码流（M1）— 依赖表与冻结配置

- 归属：T005（`docs/bitstream-frame-minimal.md`）
- 规范真值：AV2 Bitstream & Decoding Process Specification **v1.0.0**（Final Deliverable 2026-05-28），本地副本 `docs/spec/AV2_Spec_v1.0.0_20260528_38f28e7.pdf`，SHA256 `E9916F091E4E83446AAD6B4601641C5B292E569C144C4163B26A4497573B533F`。AVM 仅作交叉验证；冲突以规范为准。
- 上游依赖：T003 writer/OBU（已冻结，contracts §5）、T004 sequence header（DONE）。
- 本文对应 T005 任务书 **E1**：列出 sequence→frame→tile→block 的必需 syntax、规范条款、默认熵状态、终止规则、partition/mode/coeff 的最低要求，并与 T004 冻结尺寸/工具开关/固定 QP。

---

## 1. 结论摘要

M1 单帧码流的最简**已证明可解码**路径为：

```
OBU_TEMPORAL_DELIMITER
OBU_SEQUENCE_HEADER                     (T004，single_picture_header_flag=1)
OBU_CLOSED_LOOP_KEY = tile_group_obu()  (T005)
    f(1) is_first_tile_group = 1
    frame_header(1)                     (22 bits，见 §4)
    byte_alignment()
    tile_group_payload(sz)
        init_symbol(sz)
        decode_tile(): 1 SB → 1 block → 7 个熵符号（见 §7）
        exit_symbol()
```

整帧**熵符号只有 7 个**：`do_split`、`y_mode_set`、`y_mode_index`、`uv_mode`、`all_zero`×3。
**不引入** transform/RD/预测搜索；编码器只发出语法，不做重建。

### 1.1 为什么必须是 CLOSED_LOOP_KEY

`single_picture_header_flag=1` 已在 T004 冻结。AVM 只读 oracle 强制：

| 约束 | AVM 位置 | 含义 |
|---|---|---|
| 首帧 OBU 必须是 CLK/OLK/RAS | `avm/av2/decoder/decodeframe.c:7606-7608` | REGULAR_TILE_GROUP 作首帧被拒 |
| `single_picture_header_flag=1` 时必须 CLK | `avm/av2/decoder/decodeframe.c:8012-8016` | “Still pictures must be coded as closed loop keyframes” |

故 frame OBU **必须**用 `SVT_AV2_OBU_CLOSED_LOOP_KEY`（type 4）。T003 白名单原缺 CLK，已由 00 补入（contracts §5，2026-10-06）。**此阻塞已解除。**

---

## 2. 冻结配置（T005 提案，待 00 写入 contracts §6）

依据：8-bit、4:2:0、单帧、All-Intra、固定尺寸、固定 QP、单线程。

| 项 | 取值 | 规范条款 / 依据 | 备注 |
|---|---|---|---|
| 分辨率 | **64×64** | §5.18.4.4 `compute_image_size` | 恰为 1 个 64×64 SB；与 T002 corpus 一致 |
| 超级块 | 64×64（`use_256x256=0`、`use_128x128=0`） | §5.18.4.1 | T004 默认 |
| tile | 单 tile（`seq_tile_info_present_flag=0`） | §5.4.2 / §6.17 `tile_info` | `TileCols=TileRows=1` |
| `chroma_format_idc` | 0（4:2:0） | Table 6.2 | T004 冻结 |
| `bit_depth_idc` | 1（8-bit） | Table 6.3 | 注意 8-bit 的 idc 是 **1** |
| `single_picture_header_flag` | 1 | §5.4.1 | T004 冻结 |
| `seq_profile_idc` | 0 | Table A.1 | T004 冻结 |
| `seq_level_idx` | 0 | Table A.8 | 64×64 覆蓋；`seq_tier=0` |
| `base_q_idx` | **100**（>0） | §5.20 `quantization_params`，`f(8)`，range [0,255] | 必须 >0；见 §3 |
| `tx_mode_select` | 0（TX_MODE_LARGEST） | §5.20.5 `read_tx_mode` | 64×64 → 3 个 transform block |
| `disable_cdf_update` | 1 | §6.17 帧头 bit | 关自适应，熵状态=默认 CDF，降低 E3 风险 |
| `allow_screen_content_tools` | 0 | §5.18.6 `screen_content_params` | 关 palette/intrabc 相关 |
| `allow_intrabc` | 0 | §5.18.7 `intrabc_params` | 后续全部 0 bit |
| `uniform_tile_spacing_flag` | 1 | §6.17 `tile_params` | 单 tile 时 0 bit（见 §4） |
| `segmentation_enabled` | 0 | §5.20 `segmentation_params` | |
| `using_qmatrix` | 0 | §5.20 `setup_qm_params` | |
| `delta_q_present` | 0 | §5.20 `delta_q_params` | `base_q_idx>0` 故写 1 bit |
| `apply_deblocking_filter[0..1]` | 0,0 | §5.20 `deblocking_filter_params` | |
| `reduced_tx_set` | 0 | §6.17 | `f(2)` |
| 全部 filter（gdf/cdef/restoration/ccso） | 关 | T004 sequence 配置 | §5.20 对应段 0 bit |
| `film_grain_params_present` | 0 | T004 sequence 配置 | `apply_grain=0`，0 bit |

### 2.1 为什么 `base_q_idx > 0`（非 lossless）而不是 lossless

| 路径 | luma transform block | `coeffs()` 调用数 |
|---|---|---|
| `base_q_idx=100`，TX_MODE_LARGEST | TX_64X64 ×1 | 1 luma + 1 U + 1 V = **3** |
| `base_q_idx=0`（CodedLossless） | TX_4X4 ×256 | 256 + 2 = 258，另需 `use_dpcm_y/uv` |

lossless 还会把 `read_tx_size` 强制为 TX_4X4（§5.20.7.25），使 `all_zero` 符号从 3 个暴增到 258 个，且每块需多读 DPCM 符号。故选**非 lossless**。

---

## 3. 依赖表（sequence → frame → tile → block）

### 3.1 容器层（Annex B + §5.2）

| 层 | 语法 | 条款 | 写入 | 依赖 |
|---|---|---|---|---|
| framing | `num_bytes_in_obu` | Annex B.2 | `leb128()` | T003 `svt_av2_obu_write` |
| header | `obu_header()` | §5.2.2 | ext=0、type、tlayer | T003，`SVT_AV2_OBU_CLOSED_LOOP_KEY` |
| OBU | `temporal_delimiter_obu()` | §5.2.5 | 无 payload | 置 `FirstPictureInTU=1`、`SeenFrameHeader=0` |
| OBU | `sequence_header_obu()` | §5.4.1 | T004 | 已冻结 |
| OBU | `tile_group_obu()` | §5.2.6 / §6.18 | T005 | 见下 |

**OBU 顺序**（§7.3.7 时间单元）：TD → SEQ_HEADER → CLOSED_LOOP_KEY。TD 使 CLK 成为该 TU 首个 VCL OBU（AVM `handle_sequence_header` 需要）。

### 3.2 tile_group_obu

| 字段 | 条款 | 值 | 位数 |
|---|---|---|---|
| `is_first_tile_group` | §5.2.6 | 1 | `f(1)` |
| `frame_header(1)` | §6.17 | 见 §4 | 22 bits |
| NumTiles=1 → 无 `tile_start_and_end_present_flag` | §5.2.6 | — | 0 |
| `use_bru=0` → 无 `bru_tile_active` | §5.2.6 | — | 0 |
| `byte_alignment()` | §5.2.4 | 补 0 到字节 | 0–7 |
| `tile_group_payload(sz)` | §5.2.6 | §7 | — |

### 3.3 frame_header 字段序（single_picture=1、CLK、FirstPictureInTU=1）

依据 §6.17 `frame_header_info`。仅列**实际产生 bit** 的字段：

| # | 字段 | 条款 | 值 | 位数 |
|---|---|---|---|---|
| 1 | `cur_mfh_id` | §6.17 | 0 → `uvlc` = `1` | 1 |
| 2 | `seq_header_id_in_frame_header` | §6.17 | 0 → `uvlc` = `1` | 1 |
| 3 | `frame_size_override_flag` | §6.17 | 0（single_picture 推断） | 0 |
| 4 | `order_hint` | §6.17 | `OrderHintBits=0` | 0 |
| 5 | `refresh_frame_flags` | §6.17 | CLK+`max_mlayer_id==0` → allFrames | 0 |
| 6 | `allow_screen_content_tools` | §5.18.6 | 0 | 1 |
| 7 | `allow_intrabc` | §5.18.7 | 0 | 1 |
| 8 | `disable_cdf_update` | §6.17 | 1 | 1 |
| 9 | `uniform_tile_spacing_flag` | §6.17 | 1（单 tile 时仍写，见下） | 1 |
| 10 | `base_q_idx` | §5.20 | 100 | 8 |
| 11 | `segmentation_enabled` | §5.20 | 0 | 1 |
| 12 | `using_qmatrix` | §5.20 | 0 | 1 |
| 13 | `delta_q_present` | §5.20 | 0 | 1 |
| 14 | `apply_deblocking_filter[0]`,`[1]` | §5.20 | 0,0 | 2 |
| 15 | `tx_mode_select` | §5.20.5 | 0 | 1 |
| 16 | `reduced_tx_set` | §6.17 | 0 | 2 |
| | **合计** | | | **22** |

**`uniform_tile_spacing_flag` 说明**：`tile_params()`（§6.17）在 `sbCols=sbRows=1` 时：
- `f(1) uniform_tile_spacing_flag`（1 bit）；
- `tileColsLog2 = minLog2TileCols = tile_log2(maxTileWidthSb, 1) = 0`，`0 < maxLog2TileCols = 0` 为假 → 无循环 bit；
- `tileRowsLog2 = 0`，同理无 bit；
- `TileCols=TileRows=1` → 无 `context_update_tile_id`、无 `tile_size_bytes_minus_1`。

故 `tile_info()` 共 **1 bit**（含在 #9）。

**其余为 0 bit 的理由**：
- `screen_content_params` 只在 `seq_force_screen_content_tools==SELECT` 时读 1 bit；`allow_screen_content_tools=0` → `force_integer_mv=0`，无后续 bit。
- `intrabc_params` 只在 `allow_intrabc` 后展开。
- `quantization_params`：`y_dc_delta_q_enabled=0`（T004），`uv_ac_delta_q_enabled`/`uv_dc_delta_q_enabled=0` → 无 delta_q。
- `delta_q_params`：`base_q_idx>0` 读 1 bit `delta_q_present`；=0 则 `delta_q_res` 不写。
- `deblocking_filter_params`：非 lossless，`mfh_deblocking_filter_update[0]=0` → 读 2 bit；两者均 0 → 无 `[2]/[3]`。
- gdf/cdef/lr/ccso：对应 `enable_*` 在 T004 sequence 为 0 → 直接返回，0 bit。
- `frame_reference_mode`、`skip_mode_params`、`allow_bawp`、`allow_warpmv_mode`：`FrameIsIntra=1` → 0 bit。
- `global_motion_params`：`FrameIsIntra=1` → return。
- `film_grain_config`：`film_grain_params_present=0` → `apply_grain=0`。

### 3.4 decode_tile（64×64、1 SB、MiRows=MiCols=16）

| 步骤 | 条款 | 结果 |
|---|---|---|
| `decode_partition(0,0,BLOCK_64X64,...)` | §6.19.3.1 | 见下 |
| `read_lr(0,0,64×64)` | §6.19.7.x | 全 RESTORE_NONE → 0 bit |
| `partition_implied(0,0,64×64)` | §6.19.3.2 | `hasRows&&hasCols` → `(0, PARTITION_NONE)`，**非 implied** |
| `init_allowed_partitions` | §6.19.3.2 | NONE/SPLIT/HORZ/VERT 允许（ext off） |
| `S() do_split` | §6.19.3.2 | 值 0 → PARTITION_NONE |
| `decode_block(0,0,BLOCK_64X64)` | §6.19.3.3 | mode_info + tx + residual |

`partition_implied_at_boundary`（§6.19.3.2）：`numWide4x4=numHigh4x4=16`；`hasRows=(0+8)<16` 真、`hasCols` 真 → 返回 `(0, PARTITION_NONE)`。故 **必须**发 `do_split` 符号（不能省略）。

### 3.5 最小块路径（intra_frame_mode_info + residual）

| 语法 | 条款 | 值 | 熵符号 |
|---|---|---|---|
| `intra_segment_id` | §6.19.4 | `segmentation_enabled=0` → 0 | 无 |
| `read_gdf`/`read_cdef`/`read_ccso` | §6.19.4 | 全 0 | 无 |
| `read_delta_qindex` | §6.19.4 | `ReadDeltas=0` | 无 |
| `use_intrabc` | §6.19.4 | `allow_intrabc=0` | 无 |
| `y_mode_set` | §6.19.4 / §5.20.5.5 | **0** | `TileYModeSetCdf` |
| `y_mode_index` | 同上 | **0**（0≠MODE_INDEX_COUNT-1，无 offset） | `TileYModeIndexCdf[ctx]` |
| → `modeIdx=0` → `YMode=DC_PRED` | §5.20.5.5 | `get_intra_y_mode_set(0)`→0 | |
| `fsc_mode` | §5.20.5.5 | `allow_fsc_intra()=0` | 无 |
| `mrl_index` | §5.20.5.5 | `enable_mrls=0` | 无 |
| `uv_mode` | §5.20.5.5 | **0**（0≠CHROMA_MODE_COUNT-1，无 idx） | `TileUVModeCflNotAllowedCdf[0]` |
| → `UVMode=DC_PRED` | §5.20.5.5 | | |
| `palette`/`dip_mode` | §6.19.4 | 工具关 | 无 |
| `read_block_tx_size` | §5.20.7.25 | `TxMode=LARGEST` → `Max_Tx_Size_Rect[64×64]=TX_64X64` | 无 |
| `coeffs(0, TX_64X64)` | §6.19.7.23 | **`all_zero=1`** | `TileTxbSkipCdf[0][txSzCtx_64][0]` |
| `coeffs(1, TX_32X32)` | §6.19.7.23 | **`all_zero=1`** | `TileTxbSkipCdf[0][txSzCtx_32][6]` |
| `coeffs(2, TX_32X32)` | §6.19.7.23 | **`all_zero=1`** | `TileVTxbSkipCdf[0]` |

**符号数**：`do_split`(1) + `y_mode_set`(1) + `y_mode_index`(1) + `uv_mode`(1) + `all_zero`(3) = **7**。

**ctx 推导**（务必逐条对上，否则 CDF 选错行）：
- `y_mode_index` ctx（§8.3.2）：`(get_joint_mode(0)>=5) + (get_joint_mode(1)>=5)`。`(0,0)` 邻居不可用 → 均 DC_PRED(0) → **ctx=0**。
- `uv_mode` ctx：`is_directional_mode(YMode=DC_PRED)` = 0 → **ctx=0**。
- `all_zero`（plane 0）：`bw==w && bh==h`（64==64）→ **ctx=0**；`is_inter||fsc_mode=0`；`txSzCtx=(Tx_Size_Sqr[txSz]+Tx_Size_Sqr_Up[txSz]+1)>>1`。
- `all_zero`（plane 1）：`plane>0` 分支 `ctx=(above!=0)+(left!=0)=0`，`+6` → **ctx=6**。
- `all_zero`（plane 2）：plane==2 分支，`bw*bh==w*h`、`EobU=0` → **ctx=0**；CDF 用 `TileVTxbSkipCdf`。

`txSzCtx`（附件 `tx_size_sqr.h`/`tx_size_sqr_up.h`，TX enum：在 plane 0 用 TX_64X64，在 plane1/2 用 TX_32X32）：
- TX_64X64(4)：`Tx_Size_Sqr=TX_64X64(4)`、`Tx_Size_Sqr_Up=TX_64X64(4)` → `txSzCtx=(4+4+1)>>1=4`。
- TX_32X32(3)：`(3+3+1)>>1=3`。

> E3 实现时以 C 头文件枚举值为准（tx enum 0..TX_SIZES_ALL-1）；上表为核对用，务必与 `max_tx_size_rect.h`/`tx_size_sqr*.h` 的实际下标一致。

---

## 4. 默认熵状态（§8.2）

### 4.1 init_symbol(sz)（§8.2.2）

```
numBits       = Min(sz*8, 15)
buf           = f(numBits)
paddedBuf     = buf << (15 - numBits)
SymbolValue   = ((1<<15) - 1) ^ paddedBuf
SymbolRange   = 1<<15
SymbolMaxBits = 8*sz - 15
```
调用前**必然字节对齐**（帧头/分区是整字节）。`init_symbol` 时把默认 CDF 拷贝为 `Tile*Cdf`（§8.2.2 列表）。

**所需的 Tile CDF（最小集）**：
- `TileYModeSetCdf` ← `Default_Y_Mode_Set_Cdf`
- `TileYModeIndexCdf` ← `Default_Y_Mode_Index_Cdf`
- `TileUVModeCflNotAllowedCdf` ← `Default_Uv_Mode_Cfl_Not_Allowed_Cdf`
- `TileTxbSkipCdf` ← `Default_Txb_Skip_Cdf[idx]`
- `TileVTxbSkipCdf` ← `Default_V_Txb_Skip_Cdf[idx]`
- `TileDoSplitCdf` ← `Default_Do_Split_Cdf`

其中 `idx` 由 `base_q_idx` 决定（§6.17 `init_coeff_cdfs`）：`≤90→0`、`≤140→1`、`≤190→2`、`else→3`。

### 4.2 read_symbol(cdf)（§8.2.6，N 个符号）

```
N = size-1（cdf[N-1] 是常量速率，cdf[N] 是使用计数）
cur = SymbolRange; symbol = -1
do {
  symbol++
  prev = cur
  f = (symbol == N-1) ? 0 : (1<<15) - cdf[symbol]
  pp = ((f>>EC_PROB_SHIFT)<<4) + Prob_Inc[N-2][symbol]
  cur = (((SymbolRange>>8)*pp)>>7)<<3
} while (SymbolValue < cur)
newRange = prev - cur
newValue = SymbolValue - cur
bits = 15 - FloorLog2(newRange)
SymbolRange = newRange << bits
numBits = Clip3(0, bits, SymbolMaxBits)
newData = f(numBits)
SymbolValue = (newValue<<bits) | ((newData<<(bits-numBits)) ^ ((1<<bits)-1))
SymbolMaxBits -= bits
if (!disable_cdf_update) update_cdf(...)   // M1 置 disable_cdf_update=1 → 跳过
```

常量：`EC_PROB_SHIFT=7`；`Prob_Inc[7][8]` 见附件 `prob_inc.h`；`Para_Adjustment_List` 见 `para_adjustment_list.h`。

### 4.3 read_bool（§8.2.3）/ read_literal（§8.2.5）

`read_bool` 即 50/50 的 bypass 位；`read_literal(n)` 循环 n 次 read_bool。**M1 只在 `read_literal` 路径用到**（例如 `base_q_idx` 是 `f(n)`，属 frame header，不经熵编码）。

### 4.4 exit_symbol（§8.2.4，终止规则）

```
要求 SymbolMaxBits >= -14
trailingBitPosition = get_position() - Min(15, SymbolMaxBits+15)
advance by Max(0, SymbolMaxBits)
paddingEndPosition = get_position()（必为 8 的倍数）
要求 position=trailingBitPosition 的 bit == 1
要求 trailingBitPosition < x < paddingEndPosition 的 bit == 0
```
即 OBU 尾部是标准 `trailing_bits`（1 后跟 0）。这与 T003 `svt_av2_bit_writer_write_trailing_bits_align()` 语义一致（已对齐时写 `0x80`）。

**编码器必须保证**：`avm_od_ec_enc_done`（AVM `avm_dsp/entenc.c`）输出的尾字节与上述一致。采用 **AVM 编码器公式**（per-byte 输出 + carry，`enc->cnt=-9`、`rng=0x8000`、`done` 时 `s=10+c`、`m=0x3FFF`）可保证与 oracle 互通。

---

## 5. 编码器数学（E3 依据）

### 5.1 od_ec_prob_scale（AVM `prob.h` / 规范 §8.2.6）

```
rr = r >> 8
pp = (p >> EC_PROB_SHIFT) << 4
pp += av2_prob_inc_tbl[nsym-2][n]
return ((rr * pp >> (7 - EC_PROB_SHIFT - CDF_SHIFT + 1 + 6)) << 3)   // CDF_SHIFT=0 → >>7
```

注意 AVM 表 `av2_prob_inc_tbl` 与规范附件 `Prob_Inc` 数值**一致**。**关键**：AVM 用 `-1` 填充未用槽位，
且必须以 **signed int** 计算；最后一个符号（`n = nsym-1`）会读到该行的 `-1`，负偏移靠 unsigned
回绕得到正确边界（例如 2 符号表 row0 的 `{8, 0, -1, ...}`，`n=1` 读到 `0`，`n=2` 读到 `-1`）。
把 -1 改成 0 或用无符号计算都会产生与 oracle 不兼容的码流（T005 实测教训）。

### 5.2 decode（AVM `avm_od_ec_decode_cdf_q15_c`）与规范 read_symbol 等价

```
c = dif >> 48        // top 16
v = r; ret = -1
do { u=v; ret++; v = prob_scale(icdf[ret], r, ret, nsym); } while (c < v)
r = u - v; dif -= v<<48; normalize
```
`icdf[]` 与规范 `cdf[]` 为互补：`icdf[k] = (1<<15) - cdf[k]`，`icdf[N-1]=0` 对应规范 `symbol==N-1→f=0`。**二者等价**，故我的规范编码器一定能被 avmdec 解码。

### 5.3 编码（AVM `od_ec_encode_q15`，`od_ec_encode_cdf_q15`）

```
fl = (s>0) ? icdf[s-1] : 32768
fh = icdf[s]
u = prob_scale(fl, r, s-1, nsym)
v = prob_scale(fh, r, s,   nsym)
l += r - u
r  = u - v
normalize(l, r, 0)
```

`done`：`e = ((l + 0x3FFF) & ~0x3FFF) | 0x4000`；flush `(s + 7)>>3` 字节；再做 carry 传播。M1 直接用 AVM 公式实现（无 carry 也可，因为单 tile 少符号；但用标准 carry 更稳）。

---

## 6. 终止/一致性规则汇总

1. **OBU payload**：tile_group 的 payload 以熵编码字节开始，末尾是 `exit_symbol` 语义的 trailing bits。**不得**用 `svt_av2_obu_payload_finalize()`（那是无符号 payload 的 trailer）；tile group payload 自行以 entenc `done` 的输出结尾。
2. **frame header** 用普通 BitWriter（`f(n)`/`uvlc`），`byte_alignment()` 后即进入熵编码。
3. `is_first_tile_group=1` + `frame_header_present_flag`（由 `is_first_tile_group` 推断）→ 帧头就在该 OBU 内。
4. 单 tile：无 `tile_start_and_end_present_flag`、无 `tile_size` 字段。
5. `disable_cdf_update=1` → 熵状态恒为默认 CDF；仍然执行 exit_symbol 的 copy/avg（`SavedCdf`），但不影响本帧。
6. `SymbolMaxBits >= -14` 是硬约束，长 padding 不能超。

---

## 7. 依赖与阻塞（E1 收尾）

| 项 | 状态 |
|---|---|
| T003 writer/OBU（含 CLK） | DONE（contracts §5） |
| T004 sequence header | DONE |
| T001 app/API 输出接线 | **未交接**（E4 需要；E2/E3 可在 T005 内部单测闭环，不阻塞） |
| T002 runner / avmdec 调用 | 未交接（E4 需要） |
| 本文 §2 冻结配置 | 待 00 写入 contracts §6 |

**未解释字段**：无。§3 依赖表覆盖 sequence→frame→tile→block 每个产生 bit 的字段与每个熵符号，并给出条款与取值推导。

---

## 8. E2/E3 实施计划（小闭环）

1. **E2**：`frame_header.{h,c}` + `tile_writer.{h,c}`，产出 TD+SEQ+CLK 的 OBU 串（先不接熵编码，用固定 payload），单测核对帧头位串与 §3.3 表。
2. **E3**：`EntropyCoding/entropy_encoder.{h,c}`（AVM 等价 range encoder）+ 默认 CDF 表 `EntropyCoding/cdf_tables.{h,c}`（从规范附件逐值落表）+ `block_writer`（7 符号），产出完整 tile payload。
3. 闭环验证：T005 自带单测先用**规范解码器参考实现**回读符号；再交给 00/avmdec 做 E4。
4. 每步落盘、逐步报 `[T005][READY_FOR_INTEGRATION]`。

### 8.1 最小可用子集取舍（如 00 需要缩减）

本路径**已经是最小**：7 个符号、0 个 transform、0 个预测。若 E3 熵编码规模需要进一步压缩，唯一可省的是 `disable_cdf_update` 的 update 逻辑——已通过置 1 省掉。**没有其它可静默缩减的项**；任何“全零 payload / 空 tile”都不合法（§8.2.4 硬约束 + T005 任务书禁止）。

---

## 8bis. 已实现与验证（T005 E2–E4，2026-10-07）

本文件描述的设计已全部实现并通过只读 oracle 验证：

```
stream (21B):
01 08 0C 04 82 0A 55 FF F0 00 04 00 10 00 08 05 10 E6 C8 00 12
OBU: TD(type2,size1) + SEQ_HEADER(type1,size12) + CLOSED_LOOP_KEY(type4,size5)
tile payload (4B): E6 C8 00 12   -> 头 3B(E6 C8 00) + 熵码 1B(0x12)
avmdec -> 1 decoded frames, 64x64 4:2:0 全 128
```

### 8bis.1 实测确认的编码器事实（务必遵守）

1. `od_ec_prob_scale` 移位量 = **7**（§5.1 已修正）。
2. `av2_prob_inc_tbl` 的 `-1` 填充保留 + **signed** 计算（见 §5.1 注）。
3. tile payload **无反比特前缀**：AVM `avm_start_encode`/`avm_stop_encode` 把
   `avm_od_ec_enc_done()` 字节写到 offset 0；规范 `init_symbol(sz)` 的 raw bits 就是
   这段流前 `Min(sz*8,15)` 个 bit，不是独立字段。
4. CDF 数组表示：传 `nsyms` 个上界，省略冗余尾部 32768；encoder 对末符号取 `fh=0`。

### 8bis.2 实际使用的默认 CDF（base_q_idx=100 → COEFF_CDF_Q_CTXS=1）

| 符号 | 表/下标 | 边界值 |
|---|---|---|
| do_split | `Default_Do_Split_Cdf[0][12]` | 20492 |
| all_zero luma | `Default_Txb_Skip_Cdf[1][0][4][0]` | 23062 |
| all_zero U | `Default_Txb_Skip_Cdf[1][0][3][6]` | 4208 |
| all_zero V | `Default_V_Txb_Skip_Cdf[1][0]` | 1341 |
| y_mode_set | `Default_Y_Mode_Set_Cdf` | 28863, 31022, 31724 |
| y_mode_index | `Default_Y_Mode_Index_Cdf[0]` | 15175, 20075, 21728, 24098, 26405, 27655, 28860 |
| uv_mode | `Default_Uv_Mode_Cfl_Not_Allowed_Cdf[0]` | 9363, 20957, 22865, 24753, 26411, 27983, 30428 |

ctx：`do_split`=12（BLOCK_64X64, Partition_Size_Adjust=3, ctx1=ctx2=0）；`all_zero` luma=0、U=6、V=0；
`y_mode_index`=0；`uv_mode`=0；`txSzCtx` luma TX_64X64=4、chroma TX_32X32=3。

详见 `docs/task-reports/T005.md`。

---

## 9. 规范条款索引

- §4.11.2 `f(n)`、§4.11.3 `uvlc`、§4.11.6 `leb128`、§4.11.8 `ns`
- §5.2.2 `obu_header`、§5.2.4 `byte_alignment`、§5.2.6 `tile_group_obu`
- §5.4.1 `sequence_header_obu`、§5.18.6 `screen_content_params`、§5.18.7 `intrabc_params`
- §5.20 `quantization_params`/`segmentation_params`/`setup_qm_params`/`delta_q_params`/`deblocking_filter_params`/`read_tx_mode`/`frame_reference_mode`
- §5.20.5.5 `read_intra_y_mode`/`read_intra_uv_mode`、§5.20.7.25 `read_tx_size`、§5.20.7.26 `get_plane_residual_size`
- §6.17 `frame_header_info`、§6.18 `tile_group`、§6.19.3.1 `decode_partition`、§6.19.3.2 `read_partition`、§6.19.7.23 `coeffs`
- §8.2.2 `init_symbol`、§8.2.3 `read_bool`、§8.2.4 `exit_symbol`、§8.2.6 `read_symbol`
- Annex B.2 OBU 长度定界；§7.3.7 时间单元顺序
