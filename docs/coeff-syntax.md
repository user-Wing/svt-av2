# T011 coefficient syntax clause table (frozen for M2 round 1)

Date: 2026-10-07. Owner: T011. Consumers: T012 (supplies qcoeff), T013 (assembles the
stream), T014/T016 (verification).

This file is the **clause-by-clause** justification for every symbol T011 writes below
the transform-block boundary. Each row names the syntax element, the spec clause that
defines it, the CDF selection clause, and the condition under which it is present.
Anything not justified here is **not written**.

Sources (read-only oracles; no file in `avm\` was modified):

* `docs\spec\AV2_Spec_v1.0.0_20260528_38f28e7.pdf`
  * 5.20.7.27 Coefficients syntax (p.224–230)
  * 5.20.7.28 Read quantized coefficient syntax (`read_quant`, p.231)
  * 6.19.7.23 Coefficients semantics (p.369–371)
  * 8.3.2 Cdf selection process (p.596–605)
* `avm\av2\decoder\decodetxb.c` (decode side, `av2_read_coeffs_txb`, `decode_eob`)
* `avm\av2\encoder\encodetxb.c` (encode side, `av2_write_coeffs_txb`)
* `avm\av2\common\txb_common.h` (context helpers, `get_lf_limits`, padded index)
* `avm\av2\common\entropy_inits_coeffs.h` (default CDF tables, identical to the spec
  attachment "Default_*_Cdf" arrays and to `av2_reset_cdf_symbol_counters`)

## 0. Scope frozen for this round

From `docs\m1-m2-contracts.md` §0 plus the T011 task file:

| item | value |
|---|---|
| bit depth / chroma | 8-bit, 4:2:0 |
| QP | `base_q_idx = 100` only (`COEFF_CDF_Q_CTXS` index 1) |
| frame type | All-Intra (`is_inter = 0`) |
| CDF update | `disable_cdf_update = 1` (contexts still exist, see §6) |
| transform | **DCT_DCT only** (TX_CLASS_2D) |
| tools disabled | TCQ (`allow_tcq=0`), parity hiding (`allow_parity_hiding=0`), FSC/IDTX skip coding (`enable_fsc=0`), IST/secondary tx, CCTX, qmatrix |
| partition | top-level `PARTITION_NONE` |
| luma tx_size | **TX_64X64** (`tx_mode_select = 0` -> `TX_MODE_LARGEST`; `Source\Lib\Bitstream\frame_header.c:237-240`) |
| tx partition | **not present** under `TX_MODE_LARGEST`: `read_tx_partition()` (and its `do_partition` symbol) is reached only when `tx_mode == TX_MODE_SELECT` (`avm\av2\decoder\decodeframe.c:1456-1463`); the `TX_MODE_LARGEST` branch takes `tx_size_from_tx_mode()` at `:1462` and reads nothing. Contrast: under `TX_MODE_SELECT`, `BLOCK_64X64` + `TX_64X64` *does* allow both splits (`avm\av2\common\av2_common_int.h:5193-5214`, `coding_block_disallows_tx_partitioning` is false for `bsize < BLOCK_64X128`), so `do_partition` would be read. T011 writes **no** tx-partition symbol. |
| chroma tx_size | **TX_32X32** |
| coded coeff count | **1024** for both: `av2_get_max_eob()` = `min(w,32)*min(h,32)` (`avm\av2\common\blockd.h:3477-3490`; TX_64X64 is the `1024` special case, **not** `tx_size_2d` = 4096) |
| coded scan | TX_64X64 reuses `default_scan_32x32`; TX_32X32 too |

`disable_cdf_update = 1` means **the CDF values never adapt**. It does **not** mean only
one context exists: every symbol below still selects its CDF from the full context
addressing scheme. T011 must therefore implement the real `ctx/lfCtx/hfCtx/uvCtx/eobCtx`
derivation, not reuse M1's single `ctx12/ctx0/ctx6` triple.

Correction history (kept so the error cannot be reintroduced): the first `tx_size` ruling
(2026-10-07) stated `max_eob(TX_64X64) = 4096`. 00 retracted that in two follow-up
corrections after T009's AVM citation: `av2_get_max_eob(TX_64X64) = 1024`, i.e. the coded
grid is the clamped 32x32 area, **not** `tx_size_2d`. All of T011 uses 1024; there is no
`4096` anywhere in `coeff_syntax.c`/`.h` or in this document.

## 1. Order of the block-level symbols

For one transform block, `coeffs(plane, startX, startY, txSz)` (5.20.7.27) writes, in
order:

1. `all_zero` (S) — always, unless the whole block was already skipped by the
   `tx_skip`/`all_zero` path. If `all_zero == 1`, **nothing else follows** for the block
   and `eob = 0`.
2. `eob_pt_*` (S) and, when the token is the escape value, `eob_pt_*_extra` (L) —
   5.20.7.27 p.225.
3. `eob_extra` (S) plus `eob_extra_bit` (L) repeats — p.225–226.
4. `transform_type(...)` — **luma only** (plane 0); chroma inherits the luma type
   (6.19.7.23 NOTE, p.369). With `DCT_DCT` frozen and `TxTypes` pre-set, no
   `intra_tx_type` symbol is written for luma at this round's configuration
   (`av2_read_tx_type` is a separate module; see §7 "open items").
5. Coefficient **magnitudes**: scan order from `eob-1` down to `0`
   (`read_coeffs_reverse_2d`), each with a base symbol (`coeff_base_eob` for the last
   position, `coeff_base` for the rest) and, when the base saturates, `coeff_br`.
6. Coefficient **signs**: a second pass from `eob-1` down to `0`, each non-zero
   coefficient getting `dc_sign` (DC), `dc_sign_horz_vert` (first row/col of a 1D class,
   not applicable for DCT_DCT) or `sign_bit` (L(1)).
7. High-range extension `read_quant()` — L(1)/L(n) bits, not an entropy symbol.

This round writes **no** `coeff_base_bob`/`coeff_base_idtx`/`coeff_br_idtx`/`idtx_sign`
(FSC disabled) and no parity-hiding `coeff_base_ph` symbol.

## 2. `all_zero` — presence and context

| field | value |
|---|---|
| syntax | 5.20.7.27 p.224 `all_zero S()` |
| semantics | 6.19.7.23 p.370: `all_zero = 1` ⇒ all coefficients zero |
| CDF (plane 0/1) | `TileTxbSkipCdf[is_inter \|\| fsc_mode][txSzCtx][ctx]` (8.3.2 p.599) |
| CDF (plane 2) | `TileVTxbSkipCdf[ctx]` |
| symbol count | 2 |

Context derivation (8.3.2 p.598–599, `all_zero`), plane 0:

```
w4 = Tx_Width[txSz] >> 2 ; h4 = Tx_Height[txSz] >> 2
maxX4 = MiCols ; maxY4 = MiRows
top = 0 ; left = 0
for k in [0,w4): if (x4+k < maxX4) top  |= AboveLevelContext[0][x4+k]
for k in [0,h4): if (y4+k < maxY4) left |= LeftLevelContext[0][y4+k]
top = Min(top,4) ; left = Min(left,4)
if (enable_fsc && fsc_mode)      ctx = TXB_SKIP_CONTEXTS - 1
else if (bw == w && bh == h)     ctx = 0        // block size == tx size
else                             ctx = (top + left + 3) >> 1
```

Plane 1/2 (chroma):

```
above = OR of AboveLevelContext[p][x4+i] | AboveDcContext[p][x4+i] for i in [0,w4), x4+i<maxX4
left  = OR of LeftLevelContext[p][y4+i]  | LeftDcContext[p][y4+i]  for i in [0,h4), y4+i<maxY4
ctx = (above != 0) + (left != 0)
plane == 2: if (bw*bh > w*h) ctx += 3 ; if (EobU != 0) ctx += 6
plane == 1: ctx += 6
```

`AboveLevelContext`/`LeftLevelContext` are the `culLevel` values written back at the end
of `coeffs()` (5.20.7.27 p.230), one entry per 4×4 unit; `AboveDcContext`/`LeftDcContext`
carry the 2-bit DC-sign category (0 none, 1 negative, 2 positive). The neighbour
entries beyond the visible picture (`x4+k >= maxX4` / `y4+k >= maxY4`) are **treated as
zero and not read** — this is the spec's clause, not an optimisation.

## 3. EOB / last position

| field | value |
|---|---|
| syntax | 5.20.7.27 p.225 `eob_pt_16/32/64/128/256/512/1024 S()` |
| eob token | `eobMultisize = Min(Tx_Width_Log2[txSz],5) + Min(Tx_Height_Log2[txSz],5) - 4` |
| eob ctx | `eobCtx = (plane > 0) ? 2 : is_inter` (p.224) — 0 luma-intra, 2 chroma |
| CDF | `TileEobPt16Cdf[eobCtx]` … `TileEobPt1024Cdf[eobCtx]` (8.3.2 p.599) |
| extra | for `eobMultisize >= 4` and token == 7: `eob_pt_*_extra L(1)` (256) or `L(2)` (512/1024) |
| eob_extra | `S()` with `TileEobExtraCdf` when `eobPt >= 3` |
| eob_extra_bit | `L(1)` repeated `for i = eobPt-4 downto 0` |

Computation (p.225–226), reproduced exactly because the encoder must invert it:

```
eob = (eobPt < 2) ? eobPt : ((1 << (eobPt-2)) + 1)
if (eobPt >= 3) {
    if (eob_extra) eob += 1 << (eobPt-3)
    for (i = eobPt-4; i >= 0; i--) if (eob_extra_bit[i]) eob += 1 << i
}
```

`avm\av2\common\txb_common.c:399-401` gives the inverse table pair used for the encoder
side and as a cross-check of the above:

```
av2_eob_group_start[12] = {0,1,2,3,5,9,17,33,65,129,257,513}
av2_eob_offset_bits[12] = {0,0,0,1,2,3,4,5,6,7,8,9}
```

Consistency requirement (assert-like check in the encoder, not an error path): for a
given `eob`, the reconstructed value from the token/extra bits must equal `eob`.

**Chroma transform sizes.** §0 of the contract disallows assuming AV1 4×4 and disallows
M1's 64×64 maximum. The frozen round-1 configuration uses the same square transform size
for luma and chroma on the same residual block, so `eobMultisize` can differ between
planes 0 and 1/2 only if the chroma tx size differs. T011 reads `txSz` as a parameter and
derives `eobMultisize` from it, so any square size 4×4..64×64 works without a code change.

## 4. Coefficient magnitude: base level

`coeff_base_eob` (last non-zero position, `c == eob-1`):

| plane / region | CDF | nsyms | symbol |
|---|---|---|---|
| plane>0, `isLf` | `TileCoeffBaseLfEobUvCdf[ctx]` | `LF_BASE_SYMBOLS-1 = 5` | `Min(level,5)-1` |
| plane>0, !`isLf` | `TileCoeffBaseEobUvCdf[ctx]` | 3 | `Min(level,3)-1` |
| plane 0, `isLf` | `TileCoeffBaseLfEobCdf[txSzCtx][ctx]` | 5 | `Min(level,5)-1` |
| plane 0, !`isLf` | `TileCoeffBaseEobCdf[txSzCtx][ctx]` | 3 | `Min(level,3)-1` |

`coeff_base` (all other scanned positions `c < eob-1`):

| plane / region | CDF | nsyms | symbol |
|---|---|---|---|
| plane>0, `isLf` | `TileCoeffBaseLfUvCdf[uvCtx]` | `LF_BASE_SYMBOLS = 6` | `Min(level,5)` |
| plane>0, !`isLf` | `TileCoeffBaseUvCdf[uvCtx]` | 4 | `Min(level,3)` |
| plane 0, `isLf` | `TileCoeffBaseLfCdf[txSzCtx][lfCtx][(tcqState>>1)&1]` | 6 | `Min(level,5)` |
| plane 0, !`isLf` | `TileCoeffBaseCdf[txSzCtx][hfCtx][(tcqState>>1)&1]` | 4 | `Min(level,3)` |

With `allow_tcq = 0`, `tcqState = 0` always, so the last dimension is always index 0
(`(0>>1)&1 = 0`). The dimension is still present in the interface so that enabling TCQ
later does not change the table shape.

`get_lf_limits(row, col, txClass, plane)` (5.20.7.27 p.230) with `txClass = TX_CLASS_2D`:

```
isLf = (plane == 0) ? ((row + col) < 4) : ((row + col) < 1)
```

**Context for `coeff_base_eob`** (8.3.2 p.601):

```
bwl = Tx_Width_Log2[Adjusted_Tx_Size[txSz]]
height = Tx_Height[Adjusted_Tx_Size[txSz]]
if (c == 0)                        ctx = SIG_COEF_CONTEXTS_EOB - 4   // 0
else if (c <= (height<<bwl)/8)     ctx = SIG_COEF_CONTEXTS_EOB - 3   // 1
else if (c <= (height<<bwl)/4)     ctx = SIG_COEF_CONTEXTS_EOB - 2   // 2
else                               ctx = SIG_COEF_CONTEXTS_EOB - 1   // 3
```

**Context for `coeff_base`** (8.3.2 p.600–601):

```
mag = 0
num = SIG_REF_DIFF_OFFSET_NUM = 5                 // luma
if (plane > 0) num = (txClass == TX_CLASS_2D) ? 3 : 2
for idx in [0,num):
    refRow = row + Sig_Ref_Diff_Offset[txClass][idx][0]
    refCol = col + Sig_Ref_Diff_Offset[txClass][idx][1]
    magLimit = (isLf && (txClass==2D || idx<2) && !(isHidden && c==0)) ? 5 : 3
    if (refRow < height && refCol < width)
        mag += Min(Level[refRow][refCol], magLimit)
ctx = (mag + 1) >> 1
```

`isHidden` is 0 in this round (parity hiding disabled), so the `magLimit` condition
reduces to `isLf && (txClass==2D || idx<2)`.

`Sig_Ref_Diff_Offset` (spec p.615–616, verbatim):

```
Sig_Ref_Diff_Offset[ 3 ][ SIG_REF_DIFF_OFFSET_NUM ][ 2 ] = {
  { { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 2 }, { 2, 0 } },   // TX_CLASS_2D
  { { 0, 1 }, { 1, 0 }, { 0, 2 }, { 0, 3 }, { 0, 4 } },   // TX_CLASS_HORIZ
  { { 0, 1 }, { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 0 } }    // TX_CLASS_VERT
}
```

So the 2D luma neighbour template is **5** coefficients: right, below, below-right,
right-by-2 and below-by-2 — exactly `{0,1},{1,0},{1,1},{0,2},{2,0}`. This matches
`get_nz_mag()` in `avm\av2\common\txb_common.h:380-403`, whose 2D branch reads
`levels[1]`, `levels[(1<<bwl)+TX_PAD_HOR]`, `levels[(1<<bwl)+TX_PAD_HOR+1]`,
`levels[2]` and `levels[(2<<bwl)+2*TX_PAD_HOR]` — the same five offsets through the
padded index. Chroma 2D uses only the first three, matching
`get_nz_mag_chroma()` (`txb_common.h:339-350`). The `ctx = (mag+1)>>1` fold and the
`lfCtx`/`hfCtx`/`uvCtx` remapping below are likewise identical to
`get_nz_map_ctx_from_stats*` in the same header.

Cross-check status: the offset table and the AVM helper agree; this is **not** an
unverified transcription. The remaining `.5` uncertainty is that `mag` is accumulated in
the spec's order (`{0,1}` first, then `{1,0}`, `{1,1}`, `{0,2}`, `{2,0}`) and AVM's
`clip_max3`/`clip_max5` are lookup tables of `Min(v,3)`/`Min(v,5)`, i.e. the same
saturating min. T011's test pins both the per-neighbour order and the saturation.

**lf/hf/uv context index** (8.3.2 p.600–601):

```
plane > 0:
    ctx2 = Min(ctx,3)
    txClass == 2D: uvCtx = (plane==1) ? ctx2 : ctx2 + 4
    else:          uvCtx = ctx2 + LF_SIG_COEF_CONTEXTS_2D_UV   // 8
plane == 0 && isLf:
    txClass == 2D: c==0 -> lfCtx = Min(ctx,8)
                   row+col < 2 -> lfCtx = Min(ctx,6) + 9
                   else        -> lfCtx = Min(ctx,4) + 16
    else:          (1D only; not used this round)
plane == 0 && !isLf:
    ctx2 = Min(ctx,4)
    txClass == 2D: row+col < 6 -> hfCtx = ctx2
                   row+col < 8 -> hfCtx = ctx2 + 5
                   else        -> hfCtx = ctx2 + 10
    else:          hfCtx = ctx2 + 15
```

## 5. Coefficient magnitude: high range

`coeff_br` is present when the base level saturated (`level > baseLevels`, where
`baseLevels = isLf ? LF_NUM_BASE_LEVELS : NUM_BASE_LEVELS`), except for the
`isLf && plane>0` case: for LF chroma the base level saturates at
`LF_NUM_BASE_LEVELS + 1` and **no** `coeff_br` follows (5.20.7.27 p.228
`&& !(isLf && plane > 0)`, matching `decodetxb.c:743-760`).

`read_low_range` (decodetxb.c:151-160) reads up to `COEFF_BASE_RANGE = 3` with
`BR_CDF_SIZE = 4` symbols, breaking early on a symbol `< 3`. `write_low_range` in
`avm\av2\encoder\encodetxb.c` is the encoder inverse.

CDF (8.3.2 p.603, `coeff_br`):

| plane / region | CDF |
|---|---|
| plane>0 | `TileCoeffBrUvCdf[ctx]`, `ctx = Min(mag,3)` |
| plane 0, `isLf` | `TileCoeffBrLfCdf[mag + 7]` |
| plane 0, !`isLf` | `TileCoeffBrCdf[mag]` |

`mag` (p.603):

```
num = (txClass != 2D && plane > 0) ? 2 : 3
for idx in [0,num):
    refRow = row + Mag_Ref_Offset_With_Tx_Class[txClass][idx][0]
    refCol = col + Mag_Ref_Offset_With_Tx_Class[txClass][idx][1]
    if (refRow < txh && refCol < txw)
        mag += Min(Level[refRow][refCol], MAX_BASE_BR_RANGE - 1)   // 7
mag = Min((mag + 1) >> 1, 6)
```

`Mag_Ref_Offset_With_Tx_Class` (p.603) for 2D is `{{0,1},{1,0},{1,1}}`. For plane 0,
`pos == 0` with a non-2D class uses `ctx = mag + 7`; for this round (2D only) that branch
is not reachable.

`MAX_BASE_BR_RANGE = COEFF_BASE_RANGE + NUM_BASE_LEVELS + 1 = 3 + 2 + 1 = 6`; the spec
clamps neighbour magnitudes to `MAX_BASE_BR_RANGE - 1 = 5` (`AVMMIN(...,7)` in the
decoder corresponds to `MAX_VAL_BR_CTX`; the spec value 7 is what T011 follows).

**Level semantics.** `Level[row][col]` is the *absolute* level used for context; the
signed value is carried separately as `QuantSign`/the sign pass. The decoder stores
`levels[get_padded_idx(pos,bwl)] = level` for every scanned position including zeros, and
the neighbour reads go through the padded index so that `row-1`/`col-1` clamp to the
padding instead of reading garbage. T011 reproduces this with its own padded level buffer
(`bwl`, `TX_PAD_HOR`, `TX_PAD_TOP`), not a raw raster read.

## 6. Sign

Second pass, `c` from `eob-1` down to `0`, only for `level != 0`:

| case | syntax | CDF |
|---|---|---|
| `row==0 && col==0 && plane==0` (DC of luma) | `dc_sign S()` | `TileDcSignCdf[plane_type][isHidden][ctx]` |
| `txClass==HORIZ && col==0 && plane==0` | `dc_sign_horz_vert S()` | `TileDcSignCdf[plane_type][isHidden][0]` |
| `txClass==VERT && row==0 && plane==0` | `dc_sign_horz_vert S()` | `TileDcSignCdf[plane_type][isHidden][0]` |
| otherwise (any AC, and every chroma coefficient) | `sign_bit L(1)` | raw literal |

For DCT_DCT (`TX_CLASS_2D`) only the first and last rows above are reachable.

`dc_sign` context (8.3.2 p.602):

```
maxX4 = MiCols ; maxY4 = MiRows ; dcSign = 0
for k in [0,w4): if (x4+k<maxX4) { sign = AboveDcContext[p][x4+k]
                                   if (sign==1) dcSign--; else if (sign==2) dcSign++ }
for k in [0,h4): if (y4+k<maxY4) { sign = LeftDcContext[p][y4+k]
                                   if (sign==1) dcSign--; else if (sign==2) dcSign++ }
ctx = (dcSign < 0) ? 1 : (dcSign > 0) ? 2 : 0
```

`isHidden` is 0 this round, so the CDF is `TileDcSignCdf[ptype][0][ctx]`.

`dcCategory` (what is stored back into `AboveDcContext`/`LeftDcContext`) is set when
`pos == 0 && quant > 0`: `dcCategory = sign ? 1 : 2` (5.20.7.27 p.227/229). Zero
coefficients leave the category at 0.

`culLevel` for the level context is `Min(4, culLevel + quant)` accumulated over all
non-zero coefficients (p.227/229), then written back into
`AboveLevelContext`/`LeftLevelContext`.

## 7. Reconstruction of the written symbols from qcoeff

The encoder is the exact inverse of the decode order:

```
1. all_zero = svt_av2_quant_all_zero(qcoeff, count)
2. if all_zero: write all_zero=1 and stop. eob = 0.
3. eob = 1 + index of the last non-zero coefficient in SCAN order
   (scan[c] = raster position of scan index c)
4. write all_zero=0, then the eob token / extra bits / eob_extra bits
5. for c = eob-1 downto 1: write coeff_base or coeff_base_eob (+coeff_br)
6. if eob >= 1: write the coeff_base/_eob symbol for c = 0
7. for c = eob-1 downto 0: write the sign of each non-zero coefficient
```

Note step 5/6 split: the decoder's magnitude pass is
`read_coeffs_reverse_2d(1, eob-2)` followed by the `c=0` block, i.e. the position `c=0`
is handled last even though it is the *lowest* scan index. The encoder must match that
order because the range coder is order sensitive.

## 8. Scan order

`scan = get_scan(txSz, txClass)` (5.20.7.40 Get scan function, p.234; `avm\av2\common\scan.c`).
For `DCT_DCT` / `TX_CLASS_2D` these are the standard AV2 diagonal (zig-zag) scans. T011
does **not** re-derive the scan from memory layout: `qcoeff` is delivered in raster order
by T010, and T011 maps raster→scan with the per-size scan table.

The scan tables T011 uses are transcribed from the spec's `get_scan` definition
(and equal to `avm\av2\common\scan.c`'s `av2_default_scan_orders`). Only
`DCT_DCT`/2D entries are needed this round.

## 9. Reset and lifecycle

* A **tile** begins with a fresh symbol decoder (`init_symbol`, 8.2.2) and a fresh
  `above`/`left` level/DC context array per plane. The range coder itself is T005's and
  is not re-implemented (contract §5).
* Per-frame CDF state: with `disable_cdf_update = 1` the CDFs written into the frame are
  the defaults; T013/T015 own the frame reset. T011 exposes a `reset` entry point so a
  new tile starts from a defined state rather than reusing the previous frame's
  neighbours.
* `AboveLevelContext`/`LeftLevelContext`/`AboveDcContext`/`LeftDcContext` are **not**
  reset at a transform-block boundary; they accumulate across blocks in the frame within
  the same plane. They are reset per tile-group/tile.

## 10. Explicitly out of scope this round (not "missing evidence", just not done)

These are legal AV2 syntax elements that this round's frozen configuration does not
reach. They are listed so no reader mistakes their absence for a completed feature:

* `transform_type` / `intra_tx_type` for luma (DCT_DCT is implied by the frozen
  configuration; the full `get_tx_set` machinery is not implemented).
* `cctx_type`, `sec_tx_type` / IST.
* FSC/IDTX skip coding (`coeff_base_bob`, `coeff_base_idtx`, `coeff_br_idtx`,
  `idtx_sign`).
* TCQ (`useTcq`), parity hiding (`coeff_base_ph`), hidden-coefficient level arithmetic.
* Inter-coded coefficients (`is_inter = 1`) and the inter `pred_mode_ctx = 1` skip CDF.

## 11. Raw `L(n)` fields: the bypass interface and its verification

`coeffs()` (5.20.7.27) and `read_quant()` (5.20.7.28) interleave **entropy symbols** with
**raw literal fields**:

| field | clause | bits | when |
|---|---|---|---|
| `eob_pt_*_extra` | p.225 | `L(1)` (eobMultisize 4) / `L(2)` (5, 6) | `eobMultisize >= 4` and the token reads the escape value 7 |
| `eob_extra_bit[i]` | p.226 | `L(1)` x `(eobPt-3)` | `eobPt >= 4` |
| `sign_bit` | p.229 | `L(1)` | **every** non-zero coefficient except the luma DC (`c==0 && plane==0`, which uses `dc_sign`) |
| `read_quant()` `q_length_bit` / `golomb_length_bit` / `coeff_rem` | p.231 | `L(1)` / `L(1)` / `L(length)` | every level `>= maxLevel - allowTcq`, i.e. every level `>= maxLevel` |

The M1 range coder originally exposed only `svt_av2_entropy_encode_symbol()`, so
`svt_av2_coeff_write_block()` could not emit a block containing a raw field.

**Resolved (2026-10-07, 00's ruling).** 00 approved an **additive** extension of
`entropy_encoder.{h,c}`:

- `svt_av2_entropy_encode_literal(enc, value, n_bits)` (n_bits 0..16, MSB first),
- `svt_av2_entropy_encode_bit(enc, bit)`,
- `ent_normalize()` gains an `n_bypass` parameter (static; existing call sites pass 0).

Both new functions are pure additions: no exported signature changed and the M1 frozen
vector `E6 C8 00 12` is byte-identical (gate 1). `svt_av2_coeff_literals_supported()` now
returns 1 and all four raw fields above are emitted. The arithmetic is AVM's
`od_ec_encode_literal_bypass` (`avm/avm_dsp/entenc.c:223`), independently verified
bit-exactly — see §11.2.

Remaining refusal: only `read_quant()`'s **run-length-driven** field widths. Those are
resolved differently — see §11.1 point 4 and §11.3.

### 11.1 The raw reads are locally forward, so no backward capability is needed

The concern is that a "forward-only" raw read breaks when a *later* field lives at a
**smaller** stream offset. Traced clause by clause, the coefficient path is in fact safe:

1. **Before the magnitude passes**, `eob_pt_*_extra` (p.225) and the `eob_extra`/`eob_extra_bit`
   loop (p.226) are read in full. Nothing after them is read backwards past them, except the
   sign pass step 3 below.
2. **Magnitude passes** (p.227-228). `c` runs `eob-1 .. 1` for
   `read_coeffs_reverse_2d`, then a separate block reads `c == 0`, which is raster index
   `scan[0]`, the **smallest** non-zero raster index in the block. A raw read in the
   `c == 0` block therefore cannot be preceded-and-followed inside the same pass by a larger
   raster index: the pass strictly descends.
3. **Sign pass** (p.228-229). `c` runs `eob-1 .. 0`. A raw `sign_bit` at `scan[c]` is
   followed inside the same pass only by smaller `c`, hence by raster indices allowed to be
   larger or smaller. However, `sign_bit` values are **independent** of the later fields:
   no branch, symbol selection, or context in `read_quant()` (or in the remaining signs)
   depends on a preceding `sign_bit`.
4. **`read_quant()`** (p.231) is safe **in the encoder direction** even though the width of
   `coeff_rem` depends on the preceding `q_length_bit` / `golomb_length_bit` runs: the encoder
   already knows `x = level - maxLevel` and derives `m` from its own running `hrLevelAvg`, so
   it can compute those widths instead of reading them. The "not forward-safe" property only
   constrains a *decoder* that does not know the level in advance; it does not constrain us.
   `write_high_range()` implements this.

Net: the required capability is a **bypass/raw-bit** entry point
(`od_ec_encode_literal_bypass` equivalent), not a backward seek.

### 11.2 Independent verification of the bypass path (00 gate 3)

`artifacts/T011/gate3_bypass_diff.c` (+ `run_gate3.cmd`, one-command reproduction) compiles
AVM's **unmodified** `avm_dsp/{entenc.c,entdec.c,entcode.c}` and drives both coders over the
same call sequence:

- encodes the same mixed symbol + literal op list, and asserts the payloads are
  **byte-identical** to `od_ec_encode_literal_bypass` / `od_ec_encode_bool_bypass` /
  `avm_od_ec_encode_cdf_q15`;
- decodes the svt payload with AVM's `od_ec_decode_literal_bypass` /
  `avm_od_ec_decode_cdf_q15_c` and asserts every value returns.

118 checks, 0 failures, Release and Debug. Cases include all literal widths 1..16, literals
0..255 as 8-bit, an alternating 1/9-bit run (exercising `low`'s live low bits), and literal
sequences interleaved with symbols at the byte-boundary.

**Sensitivity was demonstrated**: corrupting the bypass path (`l += rng * value` → `l += 0`)
makes the harness exit non-zero with 59/59 failures, so a passing result is not vacuous.

### 11.3 Cross-check gaps in this area (explicitly "missing", not claimed)

Rather than claim full end-to-end coverage, these are recorded as **missing**:

- **No `avmdec` round-trip of a complete coefficient block.** The independent check covers
  the *entropy-coder primitive*, not the coefficient syntax on top of it. Driving AVM's
  `av2_read_coeffs_txb` requires its full `AV2_COMMON` / `DecoderCodingBlock` / `MACROBLOCKD`
  closure, which is outside this round's cost budget. 00 approved delivering with this marked
  missing.
- **`svt_av2_coeff_write_block()` is not verified by an independent decoder.** Its positive
  tests assert symbol order, contexts, and that encoding succeeds; they do not prove a
  conforming decoder reconstructs the same `qcoeff`. Only the scan vectors and the
  `eob_*`/`base+br`/`read_quant` value formulas are cross-checked against AVM line-by-line.
- **`read_quant()` width logic** is cross-checked by reading (`write_hr` vs `write_adaptive_hr`
  / `write_truncated_rice` / `write_exp_golomb`), not by execution.

