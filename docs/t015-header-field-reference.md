# T015 字段依据表：sequence / frame header 尺寸、level 与多帧语法

- 归属：T015（`Source\Lib\Bitstream\sequence_header.*`、`frame_header.*`、对应 header 测试源、本文件）。
- 规范真值：**AV2 Bitstream & Decoding Process Specification v1.0.0**（Final Deliverable 2026-05-28），本地副本 `docs\spec\AV2_Spec_v1.0.0_20260528_38f28e7.pdf`，SHA256 `E9916F091E4E83446AAD6B4601641C5B292E569C144C4163B26A4497573B533F`。**AVM 仅作交叉验证**（只读 `avm\`），冲突以规范为准。
- 页码为规范 PDF 打印页码（"Page N of 1169"）。
- 依据生成脚本：`svt-av2\artifacts\T015\derive_header_fields.py`、`extract_spec_clauses.py`；抽取结果 `spec-seq-pages.txt`、`spec-frame-header.txt`、`spec-frame-header2.txt`、`spec-key-pages.txt`。
- 术语：SB = superblock（本阶段冻结 **64×64**）；`sbCols`/`sbRows` 是**帧的** SB 网格；`TileCols`/`TileRows` 是 **tile** 网格。

> **本文件不宣称"多尺寸/多帧已支持"**。它给出规范依据与逐尺寸推导，供 H2/H3 实现与 T011/T013 接线；完整流验收（H4）依赖 T011/T012/T013。

### 修订记录

- **2026-10-07 r4（00 `[CORRECTION]` 落表）**：修正 §6.5 的 coeff count。此前误写 "luma 4096 / chroma 1024"；经 00 指出并本轮用 AVM 源码独立复核（`scan.c:1799-1807`/`:1887-1894` `TX_64X64` 复用 `default_scan_32x32`；`blockd.h:3477-3490` `av2_get_max_eob(TX_64X64)=1024`），改为 **luma 1024 / chroma 1024（均用 32×32 scan 序）**，并补闭式 `count = min(w,32)*min(h,32)`。**仅文档修正，未改代码**。
- **2026-10-07 r3（00 `[RULING]` 落表）**：新增 **§6.5 `tx_size` 冻结推导**。00 裁定 `tx_mode_select=0` → `TX_MODE_LARGEST` → 顶层 64×64 `PARTITION_NONE` 推导 luma `TX_64X64`/chroma `TX_32X32`。§6.2 第 21 行已交叉引用 §6.5。同时修正 00 `[RULING]` 中的行号笔误：`tx_mode_select` 实写位在 `frame_header.c:245-248`（`:237-240` 是 `deblocking_filter_params` 的两个 0 位）。
- **2026-10-07 r2（本轮结论）**：`sequence_inter_config()` 多帧字段顺序按 AVM `decodeframe.c:6005-6143` 的**实际读序**重排（此前 `enable_refmvbank`/`enable_drl_reorder` 被放在函数开头，顺序错误；已移到 `order_hint_bits_minus_1` 之后，见 §3.3）。新增 §3.4 说明 `ns(n)` 的真实位宽（`ns(5,0)`=2 位、`ns(3,0)`=1 位，即 `FloorLog2` 而非 `CeilLog2`），确认 T003 `bit_writer.c` 的 `write_ns()` **实现正确、无需改动**。多帧序列头实写位数已由独立参考模型 `artifacts/T015/ref_model_headers.py` 与 writer 逐字节核对（`artifacts/T015/probe_writer_vs_model.py`），四个尺寸全部一致。

### 已验证的多帧序列头字节向量（独立参考模型 == writer）

| 尺寸 | level | payload 位 | OBU payload 字节 | 字节（含 trailing_bits） |
|---|---|---|---|---|
| 64×64 | 0 | 138 | 18 | `80 0A 00 55 7F FC 00 00 00 08 00 00 00 C0 0D 00 00 20` |
| 176×144 | 0 | 142 | 18 | `80 0A 00 5D EB E3 C0 00 00 00 80 00 00 0C 00 D0 00 02` |
| 1280×720 | 3 | 147 | 19 | `80 3A 00 6A 67 FD 9E 00 00 00 04 00 00 00 60 06 80 00 10` |
| 1920×1080 | 4 | 149 | 19 | `80 45 00 35 5D FE 1B 80 00 00 01 00 00 00 18 01 A0 00 04` |

帧头 176×144（`base_q_idx=100`）：26 位，前缀字节 `CC 19 00`（第 4 字节仅含 padding）。

---

## 0. 现状核对（改动前）

`sequence_header.*` / `frame_header.*` 已存在于 M1 构建中（`test_sequence_header`、`test_frame_header` 均 Passed），本轮为**核对并扩展**。

M1 冻结值（2026-10-07 契约 §0/§8）：

| 项 | M1 值 | 出处 |
|---|---|---|
| `single_picture_header_flag` | **1** | `sequence_header.c:88`（硬编码写 1） |
| `seq_level_idx` | **0**（level 2.0） | `svt_av2_sequence_header_config_default()`，`sequence_header.c:37` |
| SB 尺寸 | **64×64** | `use_256x256=0`、`use_128x128=0`，`sequence_header.c:40-41` |
| `seq_tile_info_present_flag` | 0（单 tile，不写额外位） | `sequence_header.c:42`、`:267` |
| `frame_width_bits_minus_1` | 按实际尺寸推导 | `sequence_header.c:69-70` |
| base_q_idx | 100 | `frame_header.c:16` |
| `tile_info()` | 只写 `uniform_tile_spacing_flag=1` 一位 | `frame_header.c:58` |

**M1 特化部分（必须扩展的三处）**：

1. `sequence_header.c:88` 硬编码 `single_picture_header_flag = 1` → 多帧序列需要 0 分支的完整字段集（§3）。
2. `frame_header.c:58` 只写 `uniform_tile_spacing_flag` 之后直接跳到 `quantization_params` → **只有当 `sbCols==sbRows==1` 才合法**；176×144 及以上必须按 §5 写 `tile_cols_log2` / `tile_rows_log2` 循环位。
3. `svt_av2_sequence_header_config_default()` 固定 `seq_level_idx = 0` → 1280×720 / 1920×1080 在该 level 下**超限**（§4）。

---

## 1. 尺寸位宽推导（契约 §8 要求）

规范（§5.4.1，p.55-56；AVM `obu.c:608-611`）：

```
frame_width_bits_minus_1        f(4)
frame_height_bits_minus_1       f(4)
n = frame_width_bits_minus_1 + 1
max_frame_width_minus_1         f(n)
n = frame_height_bits_minus_1 + 1
max_frame_height_minus_1        f(n)
```

规范语义（§6.4.x，AVM `obu.c:610-611` 一致）：`max_frame_width = read(n) + 1`。
因此写值必须是：
- `frame_width_bits_minus_1 = ceil_log2(W) - 1`，其中 `ceil_log2(W)` 是 **W-1 的表示所需的最小位数**（至少 1）；
- `max_frame_width_minus_1 = W - 1`，字段宽度 `f(frame_width_bits_minus_1 + 1)`。

实现核对：`sequence_header.c:14-20` 的 `seq_bits_for_value()` 对 `max_frame_width - 1` 求最小位数，`:69-70` 取 `-1` 作为 f(4) 值，`:107-118` 写位宽与值。**与规范一致**。

### 1.1 目标尺寸实测值（`derive_header_fields.py` 输出）

| 尺寸 W×H | `frame_width_bits_minus_1` | f(n) 宽度 | `max_frame_width_minus_1` | `frame_height_bits_minus_1` | f(n) 宽度 | `max_frame_height_minus_1` |
|---|---|---|---|---|---|---|
| 64×64 | 5 | 6 | 63 | 5 | 6 | 63 |
| 176×144 | 7 | 8 | 175 | 7 | 8 | 143 |
| 1280×720 | 10 | 11 | 1279 | 9 | 10 | 719 |
| 1920×1080 | 10 | 11 | 1919 | 10 | 11 | 1079 |

边界（必须与现有实现一致，不得放宽）：
- `W=1` → `frame_width_bits_minus_1 = 0`、字段 `f(1)`、值 0。合法。
- `W=65536` → `frame_width_bits_minus_1 = 15`、字段 `f(16)`。**`f(4)` 能表示的最大 bits-1 是 15**，即 `W ≤ 65536` 是可编码上限。
- `W > 65536` → 需要 `bits-1 ≥ 16`，`f(4)` 无法表示 → **必须返回 `SVT_AV2_BW_ERROR_RANGE`**（现有 `sequence_header.c:71-73` 已如此）。

**结论**：尺寸位宽字段**已经是按真实尺寸推导**，不是 M1 特化；本轮只需为多尺寸补测试向量并保留原期望（§6）。

---

## 2. SB 几何与 `sbCols`/`sbRows`

规范 `tile_params()`（§5.18.7.3，p.123）与 `tile_info()`（§5.18.7.2，p.122）：

```
miCols = 2 * ( ( frameWidth  + 7 ) >> 3 )     // = ceil(W/4) in MI units (MI=4px)
miRows = 2 * ( ( frameHeight + 7 ) >> 3 )
sb4x4  = Num_4x4_Blocks_Wide[ sbSize ]
sbShift = Mi_Width_Log2[ sbSize ]             // 64x64 -> 4
sbCols = ( miCols + sb4x4 - 1 ) >> sbShift
sbRows = ( miRows + sb4x4 - 1 ) >> sbShift
```

AVM 等价（`decodeframe.c:4227-4230` → `tile_common.c:43-57`，用 `ALIGN_POWER_OF_TWO`）。

固定 64×64 SB 时的实际网格：

| 尺寸 | miCols×miRows | sbCols×sbRows | 帧对齐覆盖 (px) | 右/下不完整 SB |
|---|---|---|---|---|
| 64×64 | 16×16 | 1×1 | 64×64 | 无 |
| 176×144 | 44×36 | **3×3** | 192×192 | 有（右 176..191、下 144..191） |
| 1280×720 | 320×180 | **20×12** | 1280×768 | 有（下 720..767） |
| 1920×1080 | 480×270 | **30×17** | 1920×1088 | 有（下 1080..1087） |

> 1280/64 = 20 整除 → 宽度无边缘；720/64 = 11.25 → 12 个 SB → 底部 48 行不完整。
> 1080/64 = 16.875 → 17 个 SB → 底部 56 行不完整。
> 176/64 = 2.75 → 3；144/64 = 2.25 → 3。

**这直接决定 `tile_params()` 的位数**（§5），也决定 T007/T011 的边缘块规则（右/下不完整 SB 的 partition 符号——契约 §5 的 00 裁定：顶层 `PARTITION_NONE` 时**不写** partition 符号）。

---

## 3. `sequence_header_obu()` 字段依据

### 3.1 与 M1 相同、`single_picture_header_flag` 无关的字段

| # | 字段 | 条款/页 | 位宽 | M1 取值 | 依据 |
|---|---|---|---|---|---|
| 1 | `seq_header_id` | §5.4.1 p.55 | `uvlc()` | 0 | < MAX_SEQ_NUM(=16)，AVM `obu.c:541-545` |
| 2 | `seq_profile_idc` | §5.4.1 p.55 / A.1 | `f(5)` | 0 | Main_420_10_IP0；允 400/420、idc 0/1 |
| 3 | `single_picture_header_flag` | §5.4.1 p.55 | `f(1)` | **1（M1）** / 0（多帧） | 见 §3.2/§3.3 |
| 4 | `seq_level_idx` | §5.4.1 p.55 / Table A.8 | `f(5)` | 0 | **多尺寸必须重算**，见 §4 |
| 5 | `seq_tier` | §5.4.1 p.55 | `f(1)` | 不出现 | 仅当 `seq_level_idx ≥ SEQ_LEVEL_4_0(4) && !single_picture_header_flag`（AVM `obu.c:563-567`）→ 多帧 720p/1080p 需重新判定 |
| 6 | `chroma_format_idc` | §5.4.1 / Table 6.2 | `uvlc()` | 0 | 4:2:0 |
| 7 | `bit_depth_idc` | §5.4.1 / Table 6.3 | `uvlc()` | 1 | 8-bit（**不是 API 枚举 8**） |
| 14 | `frame_width_bits_minus_1` | §5.4.1 p.56 | `f(4)` | 见 §1 | 按真实尺寸 |
| 15 | `frame_height_bits_minus_1` | §5.4.1 p.56 | `f(4)` | 见 §1 | 按真实尺寸 |
| 16 | `max_frame_width_minus_1` | §5.4.1 p.56 | `f(4)+1` | W-1 | 按真实尺寸 |
| 17 | `max_frame_height_minus_1` | §5.4.1 p.56 | `f(4)+1` | H-1 | 按真实尺寸 |
| 18 | `seq_cropping_window_present_flag` | §5.4.1 p.56 | `f(1)` | 0 | **不得用 crop 凑合法流**（契约 §8） |
| 19-22 | `seq_cropping_win_*_offset` | §5.4.1 p.56 | `uvlc()`×4 | 不出现 | 仅当 18=1 |
| 37 | `film_grain_params_present` | §5.4.1 p.58 | `f(1)` | 0 | — |
| 38 | `obu_extension_flag` | §5.2.1 | `f(1)` | 0 | AVM 名 `seq_extension_present_flag`，`obu.c:702` |
| 39 | `trailing_bits` | §6.2.3 | — | — | `svt_av2_obu_payload_finalize()`（T003） |

### 3.2 `single_picture_header_flag = 1`（M1 冻结，p.55-57）

推断、**不写位**：`seq_lcr_id = 0`、`still_picture = 1`、`max_tlayer_id = 0`、`max_mlayer_id = 0`、`SeqMaxMlayerCnt = 1`、`monotonic_output_order_flag = 1`、`decoder_model_info_present_flag = 0`。
AVM：`obu.c:569-575`（赋值）、`obu.c:621-623`（decoder model）。规范 p.56 第 50-51 行明确 `if (single_picture_header_flag) { decoder_model_info_present_flag = 0 } else {...}`。

### 3.3 `single_picture_header_flag = 0`（多帧，必须新增的字段）

规范 p.55-57；AVM `obu.c:576-600`、`obu.c:624-666`：

| 字段 | 条款/页 | 位宽 | 本阶段建议取值 | 依据/备注 |
|---|---|---|---|---|
| `seq_lcr_id` | p.56 | `f(3)` | 0 | ≤ MAX_NUM_SEQ_LCR_ID；AVM `obu.c:577-582` |
| `still_picture` | p.56 | `f(1)` | **0** | 多帧必须为 0；AVM `obu.c:583` |
| `max_tlayer_id` | p.56 | `f(2)` | 0 | `TLAYER_BITS=2`；AVM `obu.c:584` |
| `max_mlayer_id` | p.56 | `f(3)` | 0 | `MLAYER_BITS=3`；AVM `obu.c:585` |
| `seq_max_mlayer_cnt_minus_1` | p.56 | `f(n)`, n=CeilLog2(max_mlayer_id+1) | 不出现 | 仅当 `max_mlayer_id > 0`；AVM `obu.c:586-598` |
| `monotonic_output_order_flag` | p.56 | `f(1)` | 1 | 取 1 → 帧头 `implicit_output_frame` 不写位（p.98 第 62-63 行） |
| `seq_initial_display_delay_present_flag` | p.56 | `f(1)` | 0 | AVM `obu.c:625-627` |
| `seq_initial_display_delay_minus_1` | p.56 | `f(4)` | 不出现 | 仅当上一项=1 |
| `decoder_model_info_present_flag` | p.56 | `f(1)` | 0 | 取 0 → 其后 6+ 字段全部不出现 |
| `mlayer_dependency_present_flag` | p.57 | `f(1)` | 不出现 | 仅当 `max_mlayer_id > 0` |
| `tlayer_dependency_present_flag` | p.57 | `f(1)` | 不出现 | 仅当 `max_tlayer_id > 0` |

`sequence_partition_config()`（§5.4.3 p.59，AVM `decodeframe.c:5975-5992`）：
`enable_extended_sdp` 仅当 `enable_sdp && !single_picture_header_flag`；`enable_uneven_4way_partitions` 仅当 `enable_ext_partitions`。M1 全 0 → 多帧仍为 0 位（因为 `enable_sdp=0`、`enable_ext_partitions=0`）。

**⚠ `sequence_inter_config()` 在多帧分支写入大量字段**（§5.4.6 p.61-63，AVM `decodeframe.c:6005-6143`）。这是 M1→M2 多帧最大的字段集变化。**PDF 文本层不含该函数体，字段顺序以 AVM 解码器实际读序为准**（已用 `decodeframe.c:6005-6143` 逐行核对）：

| # | 字段 | 位宽 | 建议值 | 依据（AVM `decodeframe.c`） |
|---|---|---|---|---|
| 1 | `seq_enabled_motion_modes[mode]`，mode=INTERINTRA(1)..MOTION_MODES-1(5) | `f(1)` ×**5** | 0 | `:6018-6028`；`INTERINTRA=1`、`MOTION_MODES=6`（`enums.h`） |
| 2 | `seq_frame_motion_modes_present_flag` | `f(1)` | **不写**（motionModeEnabled=0） | `:6029-6030` |
| 3 | `enable_six_param_warp_delta` | `f(1)` | **不写**（warpDeltaEnabled=0） | `:6031-6032` |
| 4 | `enable_masked_compound` | `f(1)` | 0 | `:6034` |
| 5 | `enable_ref_frame_mvs` | `f(1)` | 0 | `:6035` |
| 6 | `reduced_ref_frame_mvs_mode` | `f(1)` | **不写**（仅当上一项=1） | `:6036-6038` |
| 7 | `order_hint_bits_minus_1` | **`f(4)`** | 0；多条 inter 工具解引用 `OrderHintBits`（p.98 `order_hint f(OrderHintBits)`） | `:6040-6041` |
| 8 | `enable_refmvbank` | `f(1)` | 0 | `:6043` |
| 9 | `enable_drl_reorder` | `f(1)`，读到 1 即 DISABLED；读到 0 再读 1 位决定 CONSTRAINT/ALWAYS | **1**（=DRL_REORDER_DISABLED） | `:6044-6048` |
| 10 | `explicit_ref_frame_map` | `f(1)` | 0 | `:6058` |
| 11 | `explicit_num_ref_frames` | `f(1)` | 0 → `ref_frames = 8` | `:6059-6064` |
| 12 | `num_ref_frames_minus_1` | `f(4)` | **不写** | 仅当上一项=1 |
| 13 | `long_term_frame_id_bits` | `f(3)` | 0 | `:6066` |
| 14 | `seq_max_drl_bits_minus_1` | `ns(MAX_MAX_DRL_BITS-MIN_MAX_DRL_BITS+1)=ns(5)` | 0 → **2 位**（见 §3.4） | `:6067-6070`；`MIN_MAX_DRL_BITS=1`、`MAX_MAX_DRL_BITS=5` |
| 15 | `allow_frame_max_drl_bits` | `f(1)` | 0 | `:6071` |
| 16 | `seq_max_bvp_drl_bits_minus_1` | `ns(MAX_MAX_IBC_DRL_BITS-MIN_MAX_IBC_DRL_BITS+1)=ns(3)` | 0 → **1 位** | `:6073-6076`；`3 = MAX_REF_BV_STACK_SIZE(4)-1` |
| 17 | `allow_frame_max_bvp_drl_bits` | `f(1)` | 0 | `:6077` |
| 18 | `num_same_ref_compound` | `f(2)` | 0 | `:6079-6080` |
| 19 | `enable_tip` | `f(1)` | 0 | `:6082-6088` |
| 20 | `enable_tip_hole_fill` | `f(1)` | **不写**（仅当 enable_tip） | `:6089-6093` |
| 21 | `enable_mv_traj` | `f(1)` | 0 | `:6094-6095` |
| 22 | `enable_bawp` | `f(1)` | 0 | `:6096`（**无 single_picture 条件，多帧也写**） |
| 23 | `enable_cwp` | `f(1)` | 0 | `:6097-6098` |
| 24 | `enable_imp_msk_bld` | `f(1)` | 0 | `:6099-6100` |
| 25 | `enable_lf_sub_pu` | `f(1)` | 0 | `:6101-6102` |
| 26 | `enable_tip_explicit_qp` | `f(1)` | **不写**（仅当 `enable_tip==1 && enable_lf_sub_pu`） | `:6103-6107` |
| 27 | `enable_opfl_refine` | `f(2)` | 0（REFINE_NONE） | `:6112` |
| 28 | `enable_refinemv` | `f(1)` | 0 | `:6113` |
| 29 | `enable_tip_refinemv` | `f(1)` | **不写**（仅当 `enable_tip && (opfl\|\|refinemv)`） | `:6116-6120` |
| 30 | `enable_bru` | `f(1)` | **0** | `:6128`；BRU 会改变 partition 语义（契约 §5） |
| 31 | `enable_adaptive_mvd` | `f(1)` | 0 | `:6129` |
| 32 | `enable_mvd_sign_derive` | `f(1)` | 0 | `:6130` |
| 33 | `enable_flex_mvres` | `f(1)` | 0 | `:6131` |
| 34 | `enable_global_motion` | `f(1)` | 0 | `:6136` |
| 35 | `enable_short_refresh_frame_flags` | `f(1)` | **0** | `:6142`；取 1 会改变帧头 `refresh_frame_flags` 位宽 |

本阶段取值下实写 28 个字段、共 **40 位**（N 为 5 个 motion-mode 标志）。注意 `enable_drl_reorder` 语义：写 **1** 才得到 DISABLED；`enable_refmvbank` 与它位于 `order_hint_bits_minus_1` **之后**（`decodeframe.c:6043-6048`），不是函数开头。

### 3.4 `ns(n)` 的位宽（关键：不是 CeilLog2(n)）

规范 §4.11.8（p.48）定义：

```
ns( n ) {
  w = FloorLog2(n) + 1
  m = (1 << w) - n
  v            f(w - 1)
  if ( v < m ) return v
  extra_bit    f(1)
  return (v << 1) - m + extra_bit
}
```

即值 < m 时只用 **w-1** 位，否则用 w 位。规范 p.48 Table 4.1 给出 `ns(5)`：值 0/1/2 → `00`/`01`/`10`（**2 位**），值 3/4 → `110`/`111`（3 位）。因此：

- `seq_max_drl_bits_minus_1 = ns(5, 0)` → **2 位**（不是 3）；
- `seq_max_bvp_drl_bits_minus_1 = ns(3, 0)` → **1 位**（不是 2）。

这与 AVM `avm_rb_read_primitive_quniform()`（`avm\avm_dsp\bitreader_buffer.c:88-95`）完全一致：`l = get_msb(n)+1`，先读 `l-1` 位，`v < m` 时结束。T003 的 `svt_av2_bit_writer_write_ns()`（`bit_writer.c:118-134`）实现正确，**不需要改动**。

`sequence_scc_config()`（§5.4.7 p.63-64，AVM `decodeframe.c:6146-6169`）：多帧写入
`seq_choose_screen_content_tools f(1)`、可选 `seq_force_screen_content_tools f(1)`、可选 `seq_choose_integer_mv f(1)` / `seq_force_integer_mv f(1)`。
**本阶段建议**：`seq_choose_screen_content_tools = 1` → `seq_force_screen_content_tools = SELECT(2)` → `seq_choose_integer_mv = 1` → `seq_force_integer_mv = SELECT(2)`，共 2 位。这与 M1 推断的 `force_screen_content_tools=2 / force_integer_mv=2` 语义等价（`decodeframe.c:6148-6151`），从而帧头 `screen_content_params()` / `intrabc_params()` 的位序与 M1 一致。

`sequence_transform_quant_entropy_config()`（§5.4.8 p.64-65，AVM `decodeframe.c:6220-6298`）多帧差异：
- `enable_inter_ddt f(1)` = 0（仅 `!single_picture_header_flag`）；
- `enable_tcq f(1)` 后 `if (enable_tcq && !single_picture_header_flag) choose_tcq_per_frame f(1)`；
- `enable_avg_cdf f(1)` / `avg_cdf_type f(1)` 显式写（M1 推断为 1/1）。**取 `enable_avg_cdf=1, avg_cdf_type=1`** 才能让 `tile_info()` 省略 `context_update_tile_id`（p.123 第 18-22 行），与本阶段"单/少量 tile 也能保持最小位"一致。
- 其余（`enable_fsc`/`enable_idtx_intra`/`enable_intra_ist`/`enable_inter_ist`/`enable_chroma_dctonly`/`reduced_tx_part_set`/`enable_cctx`/`enable_parity_hiding`/`separate_uv_delta_q`/`equal_ac_dc_q`/`base_uv_ac_delta_q`/`uv_ac_delta_q_enabled`）与 M1 同。

`sequence_filter_config()`（§5.4.10 p.66-68，AVM `decodeframe.c:6171-6218`）多帧差异：
- `cdef_on_skip_txfm_always_on f(1)` / 可选 `cdef_on_skip_txfm_disabled f(1)`（M1 推断 ADAPTIVE）。
  取 `always_on=0, disabled=0` → `CDEF_ON_SKIP_TXFM_ADAPTIVE`，与 M1 语义一致，共 2 位。
- 其余（`disable_loopfilters_across_tiles`/`enable_cdef`/`enable_gdf`/`enable_restoration`/`enable_ccso`/`df_par_bits_minus_2`）与 M1 同。

`sequence_tile_config()`（§5.4.2 p.59，AVM `decodeframe.c:6310-6322`）：
`seq_tile_info_present_flag f(1)`。**本阶段保持 0**（单 tile 由帧头 `tile_params()` 推导；tile 几何完全确定，无需序列级预置）。取 1 会额外写 `allow_tile_info_change` + `tile_params()`，并改变帧头复用语义（p.122 `reuse_tile_info`）——**不在本阶段最小闭环内**。

---

## 4. `seq_level_idx` 对目标尺寸的合法取值（契约 §8 明确要求重新核对）

Table A.8（p.1105 起），AVM 逐值一致（`avm\av2\common\level.c:36-260`）。约束同时满足三条：`MaxPicSize`（样本数）、`MaxHSize`、`MaxVSize`：

| LevelIdx | Level | MaxPicSize | MaxH/V | 覆盖尺寸（最小合法） |
|---|---|---|---|---|
| 0 | 2.0 | 147456 | 640 | 64×64, **176×144** |
| 1 | 2.1 | 278784 | 880 | (可选更大) |
| 2 | 3.0 | 665856 | 1360 | |
| 3 | 3.1 | 1065024 | 1720 | **1280×720** |
| 4 | 4.0 | 2359296 | 2560 | **1920×1080** |

**实测推导**（`derive_header_fields.py`）：

| 尺寸 | 样本数 | 最小合法 level | level 名称 | 校验 |
|---|---|---|---|---|
| 64×64 | 4096 | **0** | 2.0 | 4096 ≤ 147456 且 64 ≤ 640 |
| 176×144 | 25344 | **0** | 2.0 | 25344 ≤ 147456 且 176/144 ≤ 640 |
| 1280×720 | 921600 | **3** | 3.1 | 921600 > 665856(level 2) → 必须 level 3；921600 ≤ 1065024 且 1280 ≤ 1720 |
| 1920×1080 | 2073600 | **4** | 4.0 | 2073600 > 1065024(level 3) → 必须 level 4；2073600 ≤ 2359296 且 1920/1080 ≤ 2560 |

关键点：
- **M1 的 `seq_level_idx = 0` 对 1280×720 / 1920×1080 非法。** 720p 需要 level ≥ 3，1080p 需要 level ≥ 4。
- 176×144 保持 level 0 **合法**（与 M1 不冲突），但 `seq_level_idx=0` 与"所有尺寸"不能等同。
- `seq_tier`：仅当 `seq_level_idx ≥ 4 && !single_picture_header_flag` 才写（AVM `obu.c:563-567`）。即 **多帧 1080p（level 4）会多出 1 位 `seq_tier`**；单帧（M1）任何 level 都不写。
- AVM 的 level 校验函数 `check_level_constraints`（`level.c:1290+`）在**编码端**；解码端 `av2_validate_frame_level_conformance`（`decodeframe.c:3904-3953`）**只在 `conf_win_enabled_flag` 为真时检查**——本轮无 crop，故解码端不会因 level 与尺寸不匹配而拒绝。**但不能因此省掉正确的 level**：规范 §A.4 是比特流一致性要求，且 T014 会用 decoder 元信息核对。

---

## 5. 帧头 `tile_info()`：多 SB 必须写 tile 循环位（契约 §5 的核心风险）

规范 `tile_params()`（§5.18.7.3 p.123-125）：

```
if ( seq_level_idx != 31 ) {
  maxTileWidthSb = ( Tile_Width_Scaling_Factor[seq_tier][seq_level_idx] * MAX_TILE_WIDTH ) >> (sbShift + 4)
  maxTileAreaSb  = ( Tile_Area_Scaling_Factor[seq_tier][seq_level_idx] * MAX_TILE_AREA ) >> ( 2 * (sbShift + 2) + 2 )
} else { maxTileWidthSb = sbCols; maxTileAreaSb = sbCols * sbRows }
minLog2TileCols = tile_log2(maxTileWidthSb, sbCols)
maxLog2TileCols = tile_log2(1, Min(sbCols, MAX_TILE_COLS))
maxLog2TileRows = tile_log2(1, Min(sbRows, MAX_TILE_ROWS))
minLog2Tiles    = Max( minLog2TileCols, tile_log2(maxTileAreaSb, sbRows * sbCols) )
uniform_tile_spacing_flag  f(1)
if ( uniform_tile_spacing_flag ) {
  tileColsLog2 = minLog2TileCols
  while ( tileColsLog2 < maxLog2TileCols ) {
    increment_tile_cols_log2  f(1)     // 0 即停止
    if ( increment_tile_cols_log2 == 1 ) tileColsLog2 += 1
  }
  ...
  tileRowsLog2 = Min( minLog2Tiles - tileColsLog2, maxLog2TileRows )
  while ( tileRowsLog2 < maxLog2TileRows ) {
    increment_tile_rows_log2  f(1)
    if ( increment_tile_rows_log2 == 1 ) tileRowsLog2 += 1
  }
}
```

AVM 同构（`decodeframe.c:4154-4218`，`tile_common.c:23-70`；`tile_log2` 定义于 `tile_common.h:99-104`）。
常量：`MAX_TILE_WIDTH = 4096`、`MAX_TILE_AREA = 4096*2304`（`tile_common.h:60-61`）；`MAX_TILE_COLS = MAX_TILE_ROWS = 64`（`enums.h:193-194`）；缩放因子 `tile_common.h:65-83`。

**本阶段（64×64 SB、单 tile、`uniform_tile_spacing_flag=1`）实测位数**（`derive_header_fields.py`）：

| 尺寸 | sbCols×sbRows | minLog2TileCols | maxLog2TileCols | maxLog2TileRows | tile_col 位 | tile_row 位 | `tile_params()` 总位数 |
|---|---|---|---|---|---|---|---|
| 64×64 | 1×1 | 0 | 0 | 0 | 0 | 0 | **1**（仅 `uniform_tile_spacing_flag`） |
| 176×144 | 3×3 | 0 | 2 | 2 | 2 | 2 | **5** |
| 1280×720 | 20×12 | 0 | 5 | 4 | 5 | 4 | **10** |
| 1920×1080 | 30×17 | 0 | 5 | 5 | 5 | 5 | **11** |

（在全为 0 的停止位上，写 `maxLog2 - minLog2` 个 0；`minLog2Tiles` 在这些尺寸下均为 0，故 `tileRowsLog2` 起始值 = 0。）
注：`tile_log2(blk,target) = 最小 k 使 blk<<k ≥ target`；`1280→sbCols=20`，`tile_log2(1,20)=5`（2^5=32≥20），`tile_log2(1,12)=4`（16≥12）。

**结论**：`frame_header.c:58` 现在的写法只在 64×64 正确。176×144 及以上的帧头**必须**在 `uniform_tile_spacing_flag` 之后补写 `increment_tile_cols_log2` / `increment_tile_rows_log2` 停止位（全 0）。否则 decoder 会把后续 `base_q_idx` 位误当 tile 位。

`tile_info()` 尾部的 `context_update_tile_id` / `tile_size_bytes_minus_1` 只在 `TileCols*TileRows > 1` 时出现（p.123 第 18-22 行；AVM `decodeframe.c:4254-4268`）。本阶段单 tile → 均不写。若将来选择多 tile，`enable_avg_cdf && avg_cdf_type` 为 1 时可省 `context_update_tile_id`。

**单 tile 对 level 的合法性**（契约 §5/§8 关注的"边缘依赖"）：level 0 时 `maxTileAreaSb = (4 * 4096*2304) >> (2*(6+2)+2) = 576` SB、`maxTileWidthSb = (4*4096) >> (6+4) = 64` SB；176×144 帧为 3×3=9 SB ≤ 576 且 3 ≤ 64 → **单 tile 合法**。720p/1080p 的 `maxTileWidthSb` 同样为 32（level 0..4 tier0，`Tile_Width_Scaling_Factor`=4），远大于 sbCols → 单 tile 合法。

---

## 6. 多帧（H3）：`single_picture_header_flag`、帧头复用与 temporal unit

### 6.1 取值与后果

| 配置 | `single_picture_header_flag` | 说明 |
|---|---|---|
| M1（保留） | **1** | 每序列 1 帧；`still_picture=1`；AVM `decodeframe.c:8006-8021` 强制 `obu_type == OBU_CLOSED_LOOP_KEY`，否则 `"Still pictures must be coded as closed loop keyframes"`（`:8012-8016`） |
| M2 多帧 | **0** | `still_picture=0`；`max_tlayer_id=0`、`max_mlayer_id=0` 仍是本阶段最小配置 |

### 6.2 多帧帧头新增字段（规范 p.96-110；AVM `decodeframe.c:7896-8608`）

`frame_header_info()` 在 `cur_mfh_id == 0` 时（本阶段固定，不用 MFH OBU）：

| # | 字段 | 条款/页 | 位宽 | M1 | **多帧** |
|---|---|---|---|---|---|
| 1 | `cur_mfh_id` | p.95 | `uvlc()` | 0 → `"1"` | 0 → `"1"`（不变） |
| 2 | `seq_header_id_in_frame_header` | p.95 | `uvlc()` | 0 → `"1"` | 0 → `"1"`（仅 `cur_mfh_id==0` 时出现，AVM `decodeframe.c:7810-7833`） |
| 3 | `ShowExistingFrame` | p.96 | 派生 | 0 | 0（非 SEF） |
| 4 | `FrameType` | p.96-98 | `f(1)` 或派生 | KEY（派生） | `OBU_CLOSED_LOOP_KEY` → KEY（派生，不写位） |
| 5 | `immediate_output_frame` | p.98 | `f(1)` | 派生 1 | **写 1 位**（`IsBridge || OLK` 时才派生 0；AVM `:8100-8102`）→ 取 1 |
| 6 | `implicit_output_frame` | p.98 | `f(1)` | 派生 0 | `immediate_output_frame=1` → 派生 0（不写位；AVM `:8115-8125`） |
| 7 | `frame_size_override_flag` | p.98 | `f(1)` | 派生 0 | **写 1 位**（AVM `:8167`）→ 取 0 表示用 `max_frame_width/height` |
| 8 | `order_hint` | p.98 | `f(OrderHintBits)` | OrderHintBits=0 → 0 位 | `OrderHintBits = order_hint_bits_minus_1 + 1`。**取 `order_hint_bits_minus_1=0` 最小** |
| 9 | `primary_ref_frame` | p.99 | 派生 | PRIMARY_REF_NONE | FrameIsIntra → PRIMARY_REF_NONE（不写位） |
| 10 | `refresh_frame_flags` | p.99 | 派生/`f(NumRefFrames)` | 派生 allFrames | `OBU_CLOSED_LOOP_KEY && max_mlayer_id==0` → **派生 allFrames，0 位**（p.99 第 135-136 行；AVM `:8279-8281`） |
| 11 | `setup_frame_size` | p.98/p.106 | 见 §6.3 | 0 位 | `frame_size_override_flag=0` → 取 MFH/序列尺寸，**0 位**（AVM `decodeframe.c:3994-4011`） |
| 12 | `render_size` | — | `f(1)`+可选 | 0 | 见 §6.4 |
| 13 | `disable_cdf_update` | p.106 | `f(1)` | 1 | 1 |
| 14 | `tile_info()` | p.106 | 见 §5 | 1 位 | 固定尺寸下位数与尺寸相同（**不是** sequence 级 tile） |
| 15 | `quantization_params()` | p.108/118 | `f(8)` | 8 | 8（`BitDepth==8`） |
| 16 | `segmentation_params()` | — | `f(1)` | 0 | 0 |
| 17 | `setup_qm_params()` | — | `f(1)` | 0 | 0 |
| 18 | `delta_q_params()` | — | `f(1)` | 0 | 0 |
| 19 | `deblocking_filter_params()` | — | 2 位 | 0,0 | 0,0 |
| 20 | gdf/cdef/lr/ccso | — | 0 位 | — | 全 0 位（序列开关关） |
| 21 | `read_tx_mode()` | §5.20.5（p.109） | `f(1)` | 0（TX_MODE_LARGEST） | 0；由 0 推导 luma `TX_64X64`/chroma `TX_32X32`，见 **§6.5** |
| 22 | `frame_reference_mode()`/`skip_mode_params()`/`allow_bawp`/`allow_warpmv_mode` | — | 0 位 | — | FrameIsIntra → 0 位 |
| 23 | `reduced_tx_set` | p.110 | `f(2)` | 0 | 0 |
| 24 | `global_motion_params()`/`film_grain_config()` | — | 0 位 | — | 0 位（FrameIsIntra / film_grain=0） |

**多帧与 M1 的帧头差异（仅两处，均与尺寸无关）**：
1. 多 1 位 `immediate_output_frame`（取 1）；
2. 多 1 位 `frame_size_override_flag`（取 0）+ 可能的 `order_hint` 位（取 `order_hint_bits_minus_1=0` 时为 0 位）+ `render_size` 相关位。

### 6.3 `setup_frame_size()` / `render_size`（帧头内的尺寸覆写）

规范 p.98/p.106 的 `frame_size()` 与 AVM `decodeframe.c:3974-4012`：
```
if ( frame_size_override_flag ) { av2_read_frame_size(num_bits_width, num_bits_height) }  // 2 个 f(n)
else { 用 mfh_frame_size_present_flag ? MFH 尺寸 : 序列 max_frame_* }                     // 0 位
```
`av2_read_frame_size()` = `width_minus_1 f(num_bits_width)` + `height_minus_1 f(num_bits_height)`（`decodeframe.c:9346-9350`）。
**本阶段取 `frame_size_override_flag = 0`** → 帧尺寸完全由序列头 `max_frame_width/height` 决定，**0 位**，且 `max_frame_*` 必须等于真实尺寸（契约 §8：按真实尺寸推导）。这样也避免"帧头改尺寸"掩盖 crop。

`render_size`：**AV2 帧头里不存在 `render_size` 语法元素——`render_size` 永远是派生的，没有写入位。** 独立依据（两代 oracle 一致）：

| oracle | 证据 | 结论 |
|---|---|---|
| AVM（AV2） | `decodeframe.c:3703-3737` `setup_render_size()`：函数体只做 `cm->render_width = ...; cm->render_height = ...;`，**从不调用 `avm_rb_read_*`**；参数 `rb` 被显式标注 `(void)rb;`（`:3709`）。调用点 `:4019`（`setup_frame_size` 内）、`:4101` | 0 位 |
| AVM（AV2） | `decodeframe.c:3974-4012` `setup_frame_size()`：整个 `frame_size()` 只读 `frame_size_override_flag` 分支下的 2 个 `av2_read_frame_size()` 字段（`:3995`），随后 `:4019` 调用无位 `setup_render_size()` | 0 位 |
| libaom（AV1，同构参照） | `av1\decoder\decodeframe.c:1920-1926` `setup_render_size()` = `if (aom_rb_read_bit(rb)) read_frame_size(...)` —— AV1 **有** 1 位 `render_and_frame_size_different`；但 AV2 的 `setup_render_size`（上栏）把这个读位删掉了 | AV2 = 0 位 |
| libaom（AV1，同构参照） | 把 AV1 的 `render_and_frame_size_different` 置 0 时：解码器读位 0（`:1924`）且**不调用** `read_frame_size`；编码器 `write_render_size()`（`av1\encoder\bitstream.c:2333-2341`）写 1 位 `scaling_active`=0 且写 0 字节 —— **没有 `render_and_frame_size_different` 位可抑制这 1 位** | AV1 该 1 位不可省；AV2 无此位 |

**结论（H3 阻塞项 8.1 解除）**：`SvtAv2FrameHeaderConfig` **不需要** `render_size` / `frame_size_override` 配置字段；多帧帧头的长度与 M1 路径相比只差 §6.2 列出的 `immediate_output_frame` / `frame_size_override_flag` / `order_hint` / `seq_tier` 位，**不含任何 render_size 位**。此前字段表 §8.1 标为"缺失"的依据已补齐。

### 6.4 temporal unit 边界（不得默认"拼接多个 21B still-picture 流"）

规范 §7.3.7（p.相 关："temporal unit"）与 AVM `decodeframe.c:8012-8016`：
- `single_picture_header_flag = 1` 时**每个 CLK VCL OBU 自身构成一次 still-picture 解码**，`reset_frame_buffers` 被调用（`decodeframe.c:8019`）。把 N 个这样的流直接拼接**不构成** N 帧视频序列（无跨帧状态、每帧重置参考、每帧要求 CLK）。**契约 §8 明确禁止**如此实现多帧。
- `single_picture_header_flag = 0` 时：TU 以 `OBU_TEMPORAL_DELIMITER` 起（置 `FirstPictureInTU=1`、`SeenFrameHeader=0`）；CVS 首帧必须是 CLK/OLK/RAS（`decodeframe.c:7606-7608`）。多帧序列 = **1 个 TD → 1 个 SEQUENCE_HEADER → 每帧 `TD? + CLOSED_LOOP_KEY`**，帧间共享序列级状态。
- 每帧一个 temporal delimiter 是最简且规范允许的（M1 已验证 TD→SEQ→CLK 可解码）。

**→ 报告为 H3 待实现/待验收项；本节只给依据，不宣称已支持。**

### 6.5 `tx_size` 冻结推导（00 `[RULING]` 2026-10-07；决定 T011 的 scan 尺寸）

本条为 **00 裁定的已冻结推导**，写进本表以固定 M2 帧头不写 `tx_size` 语法这一事实，并让 T011 的 scan 尺寸（**luma 1024 / chroma 1024**）有共同依据。

| 环节 | 依据 | 结论 |
|---|---|---|
| 帧头 `tx_mode_select` 硬编码 | 规范 §5.20.5 `read_tx_mode`；本仓库 `frame_header.c:245-248` 写 1 位 0 | `tx_mode_select = 0` |
| `tx_mode_select=0` → `TX_MODE` | AVM `common_data.h:703-707` `tx_mode_to_biggest_tx_size[]` 首项 `TX_4X4`=ONLY_4X4、次项 `TX_64X64`=TX_MODE_LARGEST；0 位映射到 `TX_MODE_LARGEST` | `TX_MODE_LARGEST` |
| `TX_MODE_LARGEST` → 顶层 `tx_size` | AVM `blockd.h:2580-2589` `tx_size_from_tx_mode(bsize, tx_mode)`：`largest_tx_size = TX_64X64`；`max_rect_tx_size = max_txsize_rect_lookup[BLOCK_64X64] = TX_64X64`（`common_data.h:432-433`）；`txsize_sqr_map[TX_64X64] <= TX_64X64` → 返回 `max_rect_tx_size` | 顶层 64×64 `PARTITION_NONE` → luma **`TX_64X64`** |
| chroma `tx_size` | 4:2:0 下 chroma 为 luma 一半边长（`TX_64X64` → **`TX_32X32`**）；此点为 00 `[RULING]` 明示的冻结结论 | chroma **`TX_32X32`** |

**冻结结论**：M2 帧头**不写 `tx_size` 语法**（`tx_mode_select` 保持 0）。顶层 64×64 `PARTITION_NONE` 下推导为 **luma `TX_64X64` / chroma `TX_32X32`**。

**coeff count / scan（00 `[CORRECTION]` 2026-10-07 修正）**：`TX_64X64` 的 scan **不是** 64×64=4096，而是 **32×32=1024** —— AVM 对 tx64 高频半边强制置零，**直接复用 tx32 的 scan 序**：

| 环节 | 依据（file:line） | 结论 |
|---|---|---|
| luma `TX_64X64` 的 scan 序 | AVM `scan.c:1799-1807` `av2_default_scan_orders[TX_SIZES]` 的 `TX_64X64` 项（`:1804-1806`，含注释 "Half of the coefficients of tx64 at higher frequencies are set to zeros. So tx32's scan order is used."） | `default_scan_32x32`（**1024 项**） |
| luma `TX_64X64` 全 tx_type 的 scan 序 | AVM `scan.c:1887-1894` `av2_scan_orders[TX_SIZES_ALL][TX_TYPES]` 的 `TX_64X64` 段（`:1888-1890` 同注释） | 全部 `default_scan_32x32`（1024 项） |
| 最大 coeff 数（闭式） | AVM `blockd.h:3477-3490` `av2_get_max_eob()`：`TX_64X64`/`TX_64X32`/`TX_32X64` 提前 `return 1024`（`:3478-3479`） | luma `TX_64X64` = **1024** |
| chroma `TX_32X32` | `tx_size_2d[TX_32X32]` = 32×32 | chroma = **1024** |

**修正后的冻结数值**：**luma 1024 / chroma 1024（两者均用 `default_scan_32x32`）**；闭式 `count = min(w,32) * min(h,32) = av2_get_max_eob(tx_size)`。（此前写 "luma 4096" 系把 64×64 的 `w*h` 直觉当成了 coeff 数；`read_tx_size` 见规范 §5.20.7.25，`get_plane_residual_size` 见 §5.20.7.26。）

> 注 1：00 `[RULING]` 引用的 `frame_header.c:237-240` 实为 `deblocking_filter_params` 的两个 0 位；本表按实际代码记为 `frame_header.c:245-248`（`read_tx_mode` 的 1 位 0）。此差异 00 已接受并同步更正。
> 注 2：本条为**文档层**修正，未改任何代码；帧头语法位与 M1 冻结向量均不受影响。

---

## 7. 与 T011 / T013 / T007 的接口要点

| 对象 | 交付 | 状态 |
|---|---|---|
| T011 | 影响 block/tile syntax 的开关与尺寸规则：`enable_sdp=0`、`enable_ext_partitions=0`、SB=64×64、`enable_bru=0`、单 tile；**多 SB 时 frame header 会先消耗 tile 循环位**，T011 的 tile payload 起点必须与 frame header 实际写入字节对齐（`byte_alignment()` 后） | **已答复并确认**（T011 维持不依赖；见 §7.1） |
| T011 | 右/下不完整 SB 的 partition 符号：契约 §5 的 00 裁定（顶层 `PARTITION_NONE` 时边界 SB **不写** `do_split`）；T015 只给语法位，不实现 tile 内部 | 依契约，无需 T015 改动 |
| T013 | 配置接口形态：`SvtAv2SequenceHeaderConfig`（尺寸/level/SB/tile flag）+ 新的多帧字段；`SvtAv2FrameHeaderConfig`（尺寸/base_q_idx/首帧 vs 后续帧） | **已答复并确认**（T013 接受"调用点覆写 3 字段、不新增 API"；接线范围 00 已下达；见 §7.1） |
| T007 | 可见尺寸、编码几何、边缘有效范围：本文件 §2 给出 `sbCols×sbRows` 与不完整 SB 位置；T015 不改尺寸 | 已给事实 |
| 00 | `single_picture_header_flag` 多帧取值、level 取值、SB/tile 规则登记入契约 §8/§11 | 报 READY 时附 |

### 7.1 多帧构件的调用边界（00 S4 问询；2026-10-07 核实）

- **序列级**：调 1 次 `svt_av2_sequence_header_write_multi()`（`sequence_header.h:137`）→ 1 个 `sequence_header_obu`（`single_picture_header_flag=0`）。
- **帧级**：每帧调 1 次 `svt_av2_frame_header_write()`，且 `fcfg.multi_frame = 1`（`frame_header.h:92`；`frame_header.c:174-186` 写 2 位）。
- **谁决定"帧数 > 1"**：T013（`Codec\codec.c` / `keyframe.c` 属主）。**T015 模块内无帧数概念**：`sequence_header.c`/`frame_header.c` 检索 `frame_count|num_frames|n_frames|frame_idx|SVT_AV2_MAX` 命中 **0**，多帧 header **不写"总帧数"**（帧数由 TU/OBU 序列隐含）。`SVT_AV2_MAX_ENCODABLE_FRAMES`（`Codec\codec.h:137`）归 T013，T015 **无需配套改**。
- **重要区分（避免误读）**：`keyframe.c:86` 确实在生产路径调 `svt_av2_sequence_header_config_default()`，但走的是**单图** `svt_av2_sequence_header_write()`（`single_picture_header_flag=1`），**不是** `write_multi()`。生产路径当前**未**调用 `write_multi()` / `min_level_for_size()`（调用者仅单测与 T015 scratch 探针）。
- 状态：**header 侧已就绪，等 T013 接线**（含帧数/循环/PTS/EOS，全部在 T013）。

---

## 8. 未定项 / 缺失的独立依据（禁止猜测）

1. **`render_size` 位**：**已解除（2026-10-07，H2 期间补齐）。** 见 §6.3：AV2 帧头无 `render_size` 语法元素，纯派生（AVM `decodeframe.c:3703-3737` 全函数无读位，`:3709` 显式 `(void)rb;`）。与 libaom AV1 的 `render_and_frame_size_different`（`av1\decoder\decodeframe.c:1924`）对照确认 AV2 已删除该位。**帧头长度不受 render_size 影响。**
2. **`order_hint_bits_minus_1` 具体取值上限**：本阶段建议 0（OrderHintBits=1）→ 0 位 `order_hint`。但 `order_hint` 的**编码端**写入条件未在 oracle 上实测。
3. **多帧序列的 `enable_avg_cdf`/`avg_cdf_type` 取值对 `context_update_tile_id` 的影响**：已按规范推导，但未在真实流上验证。
4. **`seq_choose_screen_content_tools` 等 SCC 位的多帧最小取值**：按 AVM 语义推导；未实测。
5. 上述均不阻塞 H1/H2（尺寸/level/tile 循环位）；**H3 必须实测后登记**。

---

## 9. 依据索引（file:line）

规范（本地 PDF，SHA256 `E9916F09...33F`）：
- §5.4.1 `sequence_header_obu()`：p.55-58
- §5.4.2 `sequence_tile_config()`：p.59
- §5.4.3 `sequence_partition_config()`：p.59
- §5.4.4 `sequence_segment_config()`：p.60
- §5.4.5 `sequence_intra_config()`：p.60
- §5.4.6 `sequence_inter_config()`：p.60-63
- §5.4.7 `sequence_scc_config()`：p.63-64
- §5.4.8 `sequence_transform_quant_entropy_config()`：p.64-65
- §5.4.10 `sequence_filter_config()`：p.66-68
- §5.18.1 `frame_header()` / §5.18.2 `frame_header_info()`：p.93-110
- §5.18.6.1 `quantization_params()`：p.118
- §5.18.7.2 `tile_info()`：p.122-123
- §5.18.7.3 `tile_params()`：p.123-125
- §6.2.3 `trailing_bits`；§7.3.7 temporal unit
- Table A.8 值 for level：p.1105 起

AVM（只读）：
- `avm\av2\decoder\obu.c:541-545` seq_header_id；`:552-556` profile；`:558` single_picture；`:559-567` level+tier；`:568` chroma/bitdepth；`:569-600` single_picture vs 多帧字段；`:608-616` 尺寸位；`:618-619` crop；`:621-667` display/decoder model；`:699-702` film_grain/ext flag；`:717` trailing bits
- `avm\av2\decoder\decodeframe.c:4026-4043` `setup_seq_sb_size`；`:4154-4218` `read_tile_params`；`:4221-4268` `read_tile_info`；`:3974-4024` `setup_frame_size`（`:3994-4011` override/序列尺寸分支，`:4019` 调用 `setup_render_size`）；`:3703-3737` `setup_render_size`（**无读位**，`:3709` `(void)rb`）；`:6310-6322` `av2_read_sequence_header`；`:5975-5992` partition flags；`:5994-6004` intra flags；`:6005-6143` inter flags；`:6146-6169` scc flags；`:6171-6218` filter flags；`:6220-6298` tqentropy flags；`:7763-7797` `handle_zero_cur_mfh_id`；`:7799-7833` mfh/seq header id；`:7896-8130` frame header head；`:8133-8205` frame_size_override/order_hint；`:8279-8300` refresh_frame_flags；`:8452-8460` KEY path；`:8900-8946`；`:9122`/`:9286` `read_tile_info` 调用；`:9346-9350` `av2_read_frame_size`
- `avm\av2\common\level.c:36-114` `av2_level_defs`；`:1290-1400` `check_level_constraints`
- `avm\av2\common\tile_common.c:23-70` `get_max_tile_width`/`av2_get_tile_limits`；`avm\av2\common\tile_common.h:60-83` 常量与缩放因子，`:99-104` `tile_log2`
- `avm\av2\common\enums.h:193-194` `MAX_TILE_COLS/ROWS`；`:952-979` `AV2_LEVEL`/`LEVEL_BITS`
- `tx_size` 冻结推导（§6.5）：`avm\av2\common\common_data.h:703-707`（`tx_mode_to_biggest_tx_size`，`TX_MODE_LARGEST`→`TX_64X64`）、`:432-433`（`max_txsize_rect_lookup[BLOCK_64X64]=TX_64X64`）、`avm\av2\common\blockd.h:2580-2589`（`tx_size_from_tx_mode`）；本仓库 `svt-av2\Source\Lib\Bitstream\frame_header.c:245-248`（写 1 位 `tx_mode_select=0`）
- `tx_size` 的 coeff/scan（§6.5 修正）：`avm\av2\common\scan.c:1799-1807`（`av2_default_scan_orders[TX_SIZES]` 的 `TX_64X64`=`default_scan_32x32`，注释 `:1804-1806`）、`:1887-1894`（`av2_scan_orders` 的 `TX_64X64` 段，注释 `:1888-1890`）、`avm\av2\common\blockd.h:3477-3490`（`av2_get_max_eob`，`TX_64X64` 提前 `return 1024`，`:3478-3479`）

跨代参照（只读 `C:\Private\ffmpeg-native\deps-full\libaom\`，仅用于确认 AV2 相对 AV1 的**删除/保留**，不作为 AV2 真值）：
- `av1\decoder\decodeframe.c:1920-1926` AV1 `setup_render_size` 含 `render_and_frame_size_different` 读位；`:2046-2049` `setup_frame_size` 调用序
- `av1\encoder\bitstream.c:2333-2341` `write_render_size`；`:2379-2380` `write_frame_size` 调用序；`:2977-3016` `frame_size_override_flag` 与 `order_hint` 写入；`:3157-3163` 非 intra 路径的 `frame_size_with_refs` 分支

本仓库既有实现：
- `Source\Lib\Bitstream\sequence_header.c:69-73` 尺寸位与上限；`:88` single_picture 硬编码；`:107-118` 尺寸写入；`:37` level 默认 0
- `Source\Lib\Bitstream\frame_header.c:58` 单 tile 的 `uniform_tile_spacing_flag`；`:64` base_q_idx f(8)
