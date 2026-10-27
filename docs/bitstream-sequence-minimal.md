# 最小 Sequence Header（M1）— 规范字段表

- 归属：T004（`Source/Lib/Bitstream/sequence_header.*`、对应单测、本文件）。
- 规范真值：**AV2 Bitstream & Decoding Process Specification v1.0.0**（Final Deliverable 2026-05-28）。
  本地副本 `docs/spec/AV2_Spec_v1.0.0_20260528_38f28e7.pdf`，
  SHA256 `E9916F091E4E83446AAD6B4601641C5B292E569C144C4163B26A4497573B533F`。
- AVM 仅作交叉验证；AVM 与规范冲突时以规范为准。AVM 符号位置基于只读源码 `avm/`（HEAD）。
- 状态：**已实现并单测通过**（`sequence_header.*` 接入 T003 的 `bit_writer`/`obu_writer`；CTest `test_sequence_header` 74 checks / 0 failures）。本文件为规范字段表 + 已核对测试向量。单独序列头不构成“完整码流 decoder 通过”。
- 最小编码配置（尺寸/profile/level/工具开关/qindex）由 T004 与 T005 共同冻结，写入 `docs/m0-m1-contracts.md` §6。

> 页码为规范 PDF 打印页码（“Page N of 1169”）。本文件所有“取值”均为 **T004 提案**，标注 `[待 T005]` 的须与 T005 共同确认，未定项不得猜。

---

## 1. 目标配置（提案）

| 项 | 取值 | 依据 |
|---|---|---|
| 位深 | 8-bit，`bit_depth_idc = 1` | Table 6.3（idc 0→10bit，1→8bit；>1 reserved）。注意不是 API 枚举值 8 |
| 色度 | 4:2:0，`chroma_format_idc = 0` | Table 6.2 |
| profile | `seq_profile_idc = 0`（Main_420_10_IP0） | Table A.1：允许 400/420，bit_depth_idc 0/1 |
| 帧结构 | 单帧 / All-Intra / 单线程，`single_picture_header_flag = 1` | §5.4.1：省略时序/显示模型/LCR/tlayer/mlayer 等 |
| 固定尺寸 | `max_frame_width/height`；无裁剪 | §5.4.1；`seq_cropping_window_present_flag = 0` |
| qindex | `base_q_idx ∈ [0, 255]`（8-bit 的 `MaxQ = MAXQ_8_BITS = 255`） | Table 6.3；帧级 `base_q_idx`（§5.20/§6.x）。**不是 AV1 的 [0,63]** |
| level | 覆盖目标分辨率的最小 `seq_level_idx` | Table A.8（见 §4） |

`single_picture_header_flag = 1` 的后果（规范明示）：`seq_lcr_id = 0`、`still_picture = 1`、`max_tlayer_id = 0`、`max_mlayer_id = 0`、`SeqMaxMlayerCnt = 1`、`monotonic_output_order_flag = 1`，且 inter 段大部分工具被推断为 0。

---

## 2. OBU 封装（Annex B + §5.2）

| 层 | 规范名称 | 条款 | 编码 | 取值 | 依赖 | AVM 读取位置 | 实现 |
|---|---|---|---|---|---|---|---|
| framing | `num_bytes_in_obu` | Annex B.2 | `leb128()` | = OBU header 字节数 + payload 字节数 | 长度字段在 OBU 之前 | `avm_read_obu_header_and_size` → `read_obu_size`（`av2/common/obu_util.c:21`） | 待 T003 |
| header | `obu_header_extension_flag` | §5.2.2 | `f(1)` | 0（M1 不需要 mlayer/xlayer） | — | `obu_util.c:47` | 待 T003 |
| header | `obu_type` | §5.2.2 / Table 6.1 | `f(5)` | 1 = `OBU_SEQUENCE_HEADER` | — | `obu_util.c:48` | 待 T003 |
| header | `obu_tlayer_id` | §5.2.2 | `f(2)` | 0 | — | `obu_util.c:50` | 待 T003 |
| wrapper | `obu_extension_flag` | §5.2.1 | `f(1)` | 0（`OBU_SEQUENCE_HEADER` 属 `is_extensible_obu()`） | OBU payload 末尾 | `obu.c:702`（AVM 名 `seq_extension_present_flag`） | 见 §5 待澄清 |
| wrapper | `trailing_bits` | §5.2.3 / §6.2.3 | 1 后补 0 至字节对齐 | — | payload 末尾 | `av2_check_trailing_bits`（`obu.c:717`） | 待 T003 |

> AVM 编码端把 `obu_extension_flag` 写作 `seq_extension_present_flag` 并断言为 0（`av2/encoder/bitstream.c:6042`），语义与规范 §5.2.1 一致，非冲突。

`leb128()`（§4.11.6）：每字节低 7 位，最高位=1 表示继续；canonical；最多 8 字节；返回值 ≤ 2^32-1。

---

## 3. `sequence_header_obu()` 字段表（§5.4.1）

顺序即写入顺序；“条件”列说明在当前固定配置下是否出现。

| # | 规范名称 | 条款 | 条件 | 位宽/编码 | 取值 | 依赖/备注 | AVM 读取 | 实现 |
|---|---|---|---|---|---|---|---|---|
| 1 | `seq_header_id` | §5.4.1 | 总是 | `uvlc()` | 0 | < `MAX_SEQ_NUM`(=16) | `obu.c:541` | 待 T003 |
| 2 | `seq_profile_idc` | §5.4.1 / A.2 | 总是 | `f(5)` | 0 | Table A.1 | `obu.c:552` | 待 T003 |
| 3 | `single_picture_header_flag` | §5.4.1 | 总是 | `f(1)` | 1 | 见 §1 | `obu.c:558` | 待 T003 |
| 4 | `seq_level_idx` | §5.4.1 / A.4 | 总是 | `f(5)` | 见 §4 | `is_valid_seq_level_idx` | `obu.c:559` | 待 T003 |
| 5 | `seq_tier` | §5.4.1 | 仅当 `seq_level_idx>3 && !single_picture` | `f(1)` | 不出现（推断 0） | — | `obu.c:563` | 待 T003 |
| 6 | `chroma_format_idc` | §5.4.1 / Table 6.2 | 总是 | `uvlc()` | 0 | 420 | `obu.c:568` | 待 T003 |
| 7 | `bit_depth_idc` | §5.4.1 / Table 6.3 | 总是 | `uvlc()` | 1 | 8-bit | `obu.c:568` | 待 T003 |
| 8 | `seq_lcr_id` | §5.4.1 | 仅当 `!single_picture` | `f(3)` | 不出现（推断 0） | — | `obu.c:577` | 待 T003 |
| 9 | `still_picture` | §5.4.1 | 仅当 `!single_picture` | `f(1)` | 不出现（推断 1） | — | `obu.c:583` | 待 T003 |
| 10 | `max_tlayer_id` | §5.4.1 | 仅当 `!single_picture` | `f(2)` | 不出现（推断 0） | — | `obu.c:584` | 待 T003 |
| 11 | `max_mlayer_id` | §5.4.1 | 仅当 `!single_picture` | `f(3)` | 不出现（推断 0） | — | `obu.c:585` | 待 T003 |
| 12 | `seq_max_mlayer_cnt_minus_1` | §5.4.1 | 仅当 `max_mlayer_id>0` | `f(n)` | 不出现 | — | `obu.c:588` | 待 T003 |
| 13 | `monotonic_output_order_flag` | §5.4.1 | 仅当 `!single_picture` | `f(1)` | 不出现（推断 1） | — | `obu.c:599` | 待 T003 |
| 14 | `frame_width_bits_minus_1` | §5.4.1 | 总是 | `f(4)` | `ceil_log2(W)-1` | 决定 #16 位宽 | `obu.c:608` | 待 T003 |
| 15 | `frame_height_bits_minus_1` | §5.4.1 | 总是 | `f(4)` | `ceil_log2(H)-1` | 决定 #17 位宽 | `obu.c:609` | 待 T003 |
| 16 | `max_frame_width_minus_1` | §5.4.1 | 总是 | `f(frame_width_bits_minus_1+1)` | W-1 | 固定尺寸 `[待 T005]` | `obu.c:610` | 待 T003 |
| 17 | `max_frame_height_minus_1` | §5.4.1 | 总是 | `f(frame_height_bits_minus_1+1)` | H-1 | 固定尺寸 `[待 T005]` | `obu.c:611` | 待 T003 |
| 18 | `seq_cropping_window_present_flag` | §5.4.1 | 总是 | `f(1)` | 0 | 无裁剪 | `obu.c:618` | 待 T003 |
| 19–22 | `seq_cropping_win_{left,right,top,bottom}_offset` | §5.4.1 | 仅当 #18=1 | `uvlc()`×4 | 不出现（推断 0） | — | `obu.c:618` | 待 T003 |
| 23 | `seq_initial_display_delay_present_flag` | §5.4.1 | 仅当 `!single_picture` | `f(1)` | 不出现 | — | `obu.c:625` | 待 T003 |
| 24 | `seq_initial_display_delay_minus_1` | §5.4.1 | 仅当 #23=1 | `f(4)` | 不出现 | — | `obu.c:630` | 待 T003 |
| 25 | `decoder_model_info_present_flag` | §5.4.1 | 仅当 `!single_picture` | `f(1)` | 不出现（推断 0） | — | `obu.c:622` | 待 T003 |
| 26+ | `num_units_in_decoding_tick` 等 | §5.4.1/§5.4.13 | 仅当 #25=1 | — | 不出现 | — | `obu.c:632` | 待 T003 |
| — | `TLayerDependencyMap`/`MLayerDependencyMap` | §5.4.1 | 派生 | — | 由 `max_tlayer_id=0`/`max_mlayer_id=0` 派生 | — | `obu.c:670` | — |
| 27 | `mlayer_dependency_present_flag` | §5.4.1 | 仅当 `max_mlayer_id>0` | `f(1)` | 不出现 | — | `obu.c:675` | 待 T003 |
| 28 | `tlayer_dependency_present_flag` | §5.4.1 | 仅当 `max_tlayer_id>0` | `f(1)` | 不出现 | — | `obu.c:684` | 待 T003 |
| 29 | `sequence_partition_config()` | §5.4.3 | 总是 | 子结构 | 见 §3.1 | — | `obu.c`→`decodeframe.c:5975` | 待 T003 |
| 30 | `sequence_segment_config()` | §5.4.4 | 总是 | 子结构 | 见 §3.2 | — | `decodeframe.c:6300` | 待 T003 |
| 31 | `sequence_intra_config()` | §5.4.5 | 总是 | 子结构 | 见 §3.3 | — | `decodeframe.c:5994` | 待 T003 |
| 32 | `sequence_inter_config()` | §5.4.6 | 总是 | 子结构 | 见 §3.4 | — | `decodeframe.c:6005` | 待 T003 |
| 33 | `sequence_scc_config()` | §5.4.7 | 总是 | 子结构 | 推断，无位 | — | `decodeframe.c:6146` | 待 T003 |
| 34 | `sequence_transform_quant_entropy_config()` | §5.4.8 | 总是 | 子结构 | 见 §3.5 | — | `decodeframe.c:6220` | 待 T003 |
| 35 | `sequence_filter_config()` | §5.4.10 | 总是 | 子结构 | 见 §3.6 | — | `decodeframe.c:6171` | 待 T003 |
| 36 | `sequence_tile_config()` | §5.4.2 | 总是 | 子结构 | 见 §3.7 | — | `decodeframe.c:5931` | 待 T003 |
| 37 | `film_grain_params_present` | §5.4.1 | 总是 | `f(1)` | 0 | — | `obu.c:699` | 待 T003 |
| 38 | `obu_extension_flag`（wrapper） | §5.2.1 | 总是（extensible） | `f(1)` | 0 | 见 §2 | `obu.c:702` | 待 T003 |
| 39 | `trailing_bits` | §5.2.3/§6.2.3 | 总是 | 见 §2 | — | — | `obu.c:717` | 待 T003 |

### 3.1 `sequence_partition_config()`（§5.4.3）

| 规范名称 | 条件 | 位宽 | 取值 | AVM | 备注 |
|---|---|---|---|---|---|
| `use_256x256_superblock` | 总是 | `f(1)` | 0 `[待 T005]` | `bitstream.c:4512` / `decodeframe.c:5977` | 0 且 `use_128x128=0` → SB=64×64 |
| `use_128x128_superblock` | 仅当 `!use_256x256` | `f(1)` | 0 `[待 T005]` | `bitstream.c:4516` | SB 尺寸影响 T005 块划分 |
| `enable_sdp` | 仅当非 Monochrome | `f(1)` | 0 | `bitstream.c:4521` | — |
| `enable_extended_sdp` | 仅当 `enable_sdp && !single_picture` | `f(1)` | 不出现 | `bitstream.c:4523` | — |
| `enable_ext_partitions` | 总是 | `f(1)` | 0 | `bitstream.c:4529` | — |
| `enable_uneven_4way_partitions` | 仅当 `enable_ext_partitions` | `f(1)` | 不出现 | `bitstream.c:4531` | — |
| `reduce_pb_aspect_ratio` | 总是 | `f(1)` | 0 → MaxPbAspectRatio=8 | `bitstream.c:4532` | 0 时不写 `max_pb_aspect_ratio_log2_minus_1` |
| `max_pb_aspect_ratio_log2_minus_1` | 仅当 `reduce_pb_aspect_ratio` | `f(1)` | 不出现 | `bitstream.c:4534` | — |

### 3.2 `sequence_segment_config()`（§5.4.4）

| 规范名称 | 条件 | 位宽 | 取值 | AVM |
|---|---|---|---|---|
| `enable_ext_seg` | 总是 | `f(1)` | 0 → MaxSegments=8 | `bitstream.c:4846` / `decodeframe.c:6300` |
| `seq_seg_info_present_flag` | 总是 | `f(1)` | 0 | `bitstream.c:4850` |
| `seq_allow_seg_info_change` + `seg_info()` | 仅当 `seq_seg_info_present_flag` | — | 不出现 | `bitstream.c:4851` |

### 3.3 `sequence_intra_config()`（§5.4.5）

| 规范名称 | 条件 | 位宽 | 取值 | AVM |
|---|---|---|---|---|
| `enable_dip` | 总是 | `f(1)` | 0 | `bitstream.c:4540` |
| `enable_intra_edge_filter` | 总是 | `f(1)` | 0 | `bitstream.c:4541` |
| `enable_mrls` | 总是 | `f(1)` | 0 | `bitstream.c:4542` |
| `enable_cfl_intra` | 总是 | `f(1)` | 0 | `bitstream.c:4543` |
| `cfl_ds_filter_index` | 仅当非 Monochrome | `f(2)` | 0 | `bitstream.c:4545` |
| `enable_mhccp` | 总是 | `f(1)` | 0 | `bitstream.c:4546` |
| `enable_ibp` | 总是 | `f(1)` | 0 | `bitstream.c:4547` |

### 3.4 `sequence_inter_config()`（§5.4.6，`single_picture=1` 分支）

以下被推断，不写入：`seq_enabled_motion_modes[*]=0`、`enable_six_param_warp_delta=0`、`enable_masked_compound=0`、`enable_ref_frame_mvs=0`、`reduced_ref_frame_mvs_mode=0`、`OrderHintBits=0`、`enable_opfl_refine=REFINE_NONE`、`enable_mv_traj=0`、`enable_imp_msk_bld=0`、`NumRefFrames=2`、`long_term_frame_id_bits=0`。

实际写入：

| 规范名称 | 条件 | 位宽 | 取值 | AVM |
|---|---|---|---|---|
| `enable_refmvbank` | 总是 | `f(1)` | 0 | `decodeframe.c:6043` |
| `disable_drl_reorder` | 总是 | `f(1)` | 1 → DrlReorder=DISABLED | `decodeframe.c:6044` |
| `constrain_drl_reorder` | 仅当 `!disable_drl_reorder` | `f(1)` | 不出现 | `decodeframe.c:6048` |
| `seq_max_bvp_drl_bits_minus_1` | 总是 | `ns(MAX_REF_BV_STACK_SIZE-1=3)` | 0 | `decodeframe.c:6073`（`primitive_quniform`≡`ns`） |
| `allow_frame_max_bvp_drl_bits` | 总是 | `f(1)` | 0 | `decodeframe.c:6077` |
| `enable_bawp` | 总是 | `f(1)` | 0 | `decodeframe.c:6096` |

### 3.5 `sequence_transform_quant_entropy_config()`（§5.4.8）

| 规范名称 | 条件 | 位宽 | 取值 | AVM |
|---|---|---|---|---|
| `enable_fsc` | 总是 | `f(1)` | 0 | `bitstream.c:4768` |
| `enable_idtx_intra` | 仅当 `!enable_fsc` | `f(1)` | 0 | `bitstream.c:4770` |
| `enable_intra_ist` | 总是 | `f(1)` | 0 | `bitstream.c:4772` |
| `enable_inter_ist` | 总是 | `f(1)` | 0 | `bitstream.c:4773` |
| `enable_chroma_dctonly` | 仅当非 Monochrome | `f(1)` | 0 | `bitstream.c:4775` |
| `enable_inter_ddt` | 仅当 `!single_picture` | `f(1)` | 不出现 | `bitstream.c:4776` |
| `reduced_tx_part_set` | 总是 | `f(1)` | 0 | `bitstream.c:4781` |
| `enable_cctx` | 仅当非 Monochrome | `f(1)` | 0 | `bitstream.c:4782` |
| `enable_tcq` | 总是 | `f(1)` | 0 | `bitstream.c:4784` |
| `choose_tcq_per_frame` | 仅当 `enable_tcq && !single_picture` | `f(1)` | 不出现 | `bitstream.c:4790` |
| `enable_parity_hiding` | 仅当 `enable_tcq==0` 或 `enable_tcq>=TCQ_8ST_FR` | `f(1)` | 0 | `bitstream.c:4796` |
| `enable_avg_cdf`/`avg_cdf_type` | 仅当 `!single_picture` | — | 不出现（推断 1/1） | `bitstream.c:4799` |
| `separate_uv_delta_q` | 仅当非 Monochrome | `f(1)` | 0 | `bitstream.c:4811` |
| `equal_ac_dc_q` | 总是 | `f(1)` | 1 | `bitstream.c:4814` |
| `base_y_dc_delta_q` + `y_dc_delta_q_enabled` | 仅当 `!equal_ac_dc_q` | — | 不出现 | `bitstream.c:4815` |
| `base_uv_dc_delta_q` + `uv_dc_delta_q_enabled` | 仅当 `!equal_ac_dc_q` | — | 不出现 | `bitstream.c:4826` |
| `base_uv_ac_delta_q` + `uv_ac_delta_q_enabled` | 仅当非 Monochrome | `f(5)`+`f(1)` | **总是写入**（即使 `equal_ac_dc_q=1`） | `bitstream.c:4836` |

> ⚠ `equal_ac_dc_q=1` 时仍必须写 `base_uv_ac_delta_q f(5)` 与 `uv_ac_delta_q_enabled f(1)`（规范 §5.4.8 在 `!Monochrome` 分支内、`equal_ac_dc_q` 条件之外）。取 `base_uv_ac_delta_q=0`（实际值 `DELTA_DCQUANT_MIN + 0`）、`uv_ac_delta_q_enabled=0`。

### 3.6 `sequence_filter_config()`（§5.4.10）

| 规范名称 | 条件 | 位宽 | 取值 | AVM |
|---|---|---|---|---|
| `disable_loopfilters_across_tiles` | 总是 | `f(1)` | 0 | `bitstream.c:4723` |
| `enable_cdef` | 总是 | `f(1)` | 0 | `bitstream.c:4724` |
| `enable_gdf` | 总是 | `f(1)` | 0 | `bitstream.c:4725` |
| `gdf_unit_matches_sb_size` | 仅当 `enable_gdf && SB==64×64` | `f(1)` | 不出现 | `bitstream.c:4726` |
| `enable_restoration` | 总是 | `f(1)` | 0 | `bitstream.c:4730` |
| `lr_tools_disable[0][*]` | 仅当 `enable_restoration` | `f(1)`×(RESTORE_SWITCHABLE_TYPES-1) | 不出现 | `bitstream.c:4732` |
| `lr_tools_uv_present` 等 | 仅当 `enable_restoration` | — | 不出现 | `bitstream.c:4735` |
| `enable_ccso` | 总是 | `f(1)` | 0 | `bitstream.c:4747` |
| `ccso_unit_matches_sb_size` | 仅当 `enable_ccso` | `f(1)` | 不出现 | `bitstream.c:4748` |
| `cdef_on_skip_txfm_always_on`/`_disabled` | 仅当 `!single_picture` | — | 不出现 | `bitstream.c:4752` |
| `df_par_bits_minus_2` | 总是 | `f(2)` | 0 | `bitstream.c:4763` |

### 3.7 `sequence_tile_config()`（§5.4.2）

| 规范名称 | 条件 | 位宽 | 取值 | AVM |
|---|---|---|---|---|
| `seq_tile_info_present_flag` | 总是 | `f(1)` | 0（默认单 tile） `[待 T005]` | `bitstream.c:4479` / `decodeframe.c:5933` |
| `allow_tile_info_change` + `tile_params()` | 仅当 `seq_tile_info_present_flag` | — | 不出现 | `bitstream.c:4481` |

---

## 4. `seq_level_idx` 取值（Table A.8）

`seq_level_idx` 为 5 bit，0..21 有效，31=`Maximum parameters`，其余保留。约束：`MaxPicSize`（样本数）与 `MaxHSize/MaxVSize`（每边像素）。

| 分辨率 | 样本数 | 建议 `seq_level_idx` | 规范 Level | 依据 |
|---|---|---|---|---|
| 176×144 | 25344 | 0 | 2.0 | MaxPicSize 147456，MaxH/V 640 |
| 640×360 | 230400 | 1 | 2.1 | MaxPicSize 278784，MaxH/V 880 |
| 1280×720 | 921600 | 3 | 3.1 | MaxPicSize 1065024，MaxH/V 1720 |
| 1920×1080 | 2073600 | 4 | 4.0 | MaxPicSize 2359296，MaxH/V 2560 |

`seq_tier` 仅在 `seq_level_idx>3 && !single_picture` 出现；本配置 `single_picture=1` → 不出现。

---

## 5. 与 T003 的接口/职责边界（待确认）

`OBU_SEQUENCE_HEADER` 属 `is_extensible_obu()`（§5.2.1），其 payload 末尾由 wrapper 写 `obu_extension_flag f(1)` 与 `trailing_bits`。职责二选一，须与 T003 冻结：

- **方案 A（建议）**：T004 产出完整 payload 字节串 = `sequence_header_obu()` 字段 + `obu_extension_flag(0)` + `trailing_bits`；T003 的 `obu_writer` 只做 `header + leb128(size=header+payload) + payload`。
- 方案 B：T004 只产出 `sequence_header_obu()` 字段；T003 的 `obu_writer` 负责追加 `obu_extension_flag` 与 `trailing_bits`。

无论哪种，`leb128 size = header(1) + payload`（§5.2.1 `obuPayloadSize = sz - 1 - obu_header_extension_flag`）。

T004 需要的 `bit_writer` 能力：MSB-first `f(n)`（n=1..32）、`uvlc()`、`ns(n)`、`byte_alignment()`、`trailing_bits()`、查询字节数/错误返回。已向 T003 发对齐请求（抄送 00）。

---

## 6. 已推导测试向量（已由实现 + 独立参考模型核对）

配置：`profile=0, single_picture=1, level=0, 420, 8-bit, W=176, H=144, SB=64×64, seq_tile_info_present=0, 全工具关闭, equal_ac_dc_q=1`。

`sequence_header_obu()` 语法位（含 `obu_extension_flag`）共 88 bit，恰为 11 字节（未加 trailing）：

```
82 0A 77 AF 8F 00 00 40 01 00 00
```

经 `svt_av2_obu_payload_finalize()` 追加 trailing bits（已字节对齐 → 整字节 `0x80`）后，payload 12 字节：

```
82 0A 77 AF 8F 00 00 40 01 00 00 80
```

完整 OBU（header `0x04` = `0|00001|00`，leb128 size `0x0D` = 1 header + 12 payload）：

```
0D 04 82 0A 77 AF 8F 00 00 40 01 00 00 80
```

字节来源（位序，MSB-first）：`1`(uvlc id=0) `00000`(profile) `1`(single) `00000`(level) `1`(uvlc chroma=0) `010`(uvlc bitdepth=1) `0111`(W bits-1) `0111`(H bits-1) `10101111`(W-1) `10001111`(H-1)，其后依次为 §3 各标志位与 `base_uv_ac_delta_q f(5)`+`uv_ac_delta_q_enabled f(1)`，末尾 `obu_extension_flag=0`；恰 88 bit 对齐，trailing 补整字节 `0x80`。

> 该向量已同时由 C 实现与一份独立 Python 参考模型（按规范语法另写）逐字节比对通过，见 `tests/unit/test_sequence_header.c`。`[待 T005]` 项（SB 尺寸、tile flag）一旦变更，向量随之更新。**单独序列头不构成完整解码通过。**

---

## 7. 未定项（禁止猜测）

1. 固定分辨率、SB 尺寸、`seq_tile_info_present_flag` —— 与 T005 共同冻结。
2. writer/OBU 接口签名与 §5 职责划分 —— T003 冻结。
3. `seq_level_idx` 与最终尺寸的对应 —— 待尺寸确定后按 Table A.8 定。
4. `base_q_idx` 的具体固定值 —— T005 帧语法确认。

## 8. 验证计划（接口冻结后）

- 独立解析器/字段 trace 核对 writer 输出（不得只断言“返回成功”）。
- 覆盖：`seq_header_id`/`chroma_format_idc`/`bit_depth_idc` 的 `uvlc` 0/1/边界；尺寸位宽边界（如 W=1、W=2^k、W=2^k+1、`UINT32` 上限拒绝）；不支持的色度/位深/profile 明确失败；`ns(3)` 0..2；`equal_ac_dc_q` 两种分支的字段出现/不出现；`trailing_bits` 对齐与非对齐。
- 长度：OBU 声明长度 == header + payload 实际字节数。
- 单独序列头**不能**证明完整解码成功；最终 decoder 验收由 T005 完整流完成。
