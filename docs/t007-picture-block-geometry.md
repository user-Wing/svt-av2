# T007：picture / plane 存储与块几何（冻结提案）

日期：2026-10-07。属主：T007。状态：**DRAFT → 待 00 记账入 `docs/m1-m2-contracts.md` §1**。
本文件只冻结 **T007 与 T008/T012/T013 之间的边界**；模块内部 helper 命名与实现细节不在冻结范围。

路径相对 `svt-av2\`。只读依据：`avm\`（AVM 源码）与 `docs\spec\AV2_Spec_v1.0.0_20260528_38f28e7.pdf`。
每条几何规则都标注可复核的 AVM 文件:行号；**没有**标注来源的规则一律视为未冻结，不得被下游依赖。

---

## 0. 冻结配置（沿用 contracts §0，不扩大）

8-bit、4:2:0、单线程、All-Intra、`base_q_idx = 100`、全滤波关闭。
目标尺寸 64×64 → 176×144 → 1280×720 → 1920×1080。

**尺寸有效性（T007 冻结）**：可见宽度与可见高度都必须是 **8 的倍数**。理由见 §2.1：AV2 的 mi 网格与 chroma 参考块都以 8×8 为粒度，非 8 倍数的尺寸会让"右/下不完整块"在 chroma 侧产生无法合法编码的 2/4 像素列/行。
4:0:0、10-bit、12-bit、宽或高为 0、尺寸溢出：**返回明确负值，不做隐式转换、不静默 crop**（contracts §1.4）。

---

## 1. owner / lifetime / mutability / alignment / stride / 边界（任务关键要求表）

| 属性 | 源图（source picture） | 重建图（recon picture） |
|---|---|---|
| 内存所有权 | **调用方**（`SvtAv2Picture.planes[]` 指向调用方缓冲）或 T007 分配的内部拷贝 | **库**（T007 分配，`svt_av2_picture_alloc`） |
| 生命周期 | 仅在 `send_picture` 调用期内有效 | 至下一帧被覆盖或 `deinit`（contracts §1.2） |
| 可变性 | **只读**。编码器不得写源图 | 可写；只允许按 contracts §6 的链写入 `source→prediction→residual→coeff→qcoeff→dqcoeff→inverse residual→clipped recon` |
| 可区分性 | 源与 recon **必须是两块不同缓冲** | 同左；禁止把 source 直接当 recon（contracts §9） |
| 对齐 | 不要求（外部指针只按 byte 步进访问） | 帧级缓冲由单个 `malloc` 得到；**不承诺**超出 `malloc` 基本对齐的保证 |
| stride 单位 | **字节**/行（`uint32_t`） | 字节/行，由 T007 计算（见 §3） |
| 边界 | 调用方保证 `row∈[0,height)`、`col∈[0,stride)` 可读 | 同左，另外 `row∈[0,aligned_height)`、`col∈[0,stride)` 可读写 |
| 平面数 | 3（Y,U,V）；4:2:0 时 planes[1..2] 按 `width>>1 × height>>1` 解释 | 同左 |
| 失败方式 | 空指针 / `stride < 可见宽度` / 宽高为 0 / 溢出 / 非 8 倍数 → 明确负值，**不部分写入** | 同左 |
| per-block malloc | **禁止**（contracts §6 末、任务书关键要求） | 同左 |

**调用者责任（必须写入接口注释，不得虚构自动检测）**：接口只接收 `const uint8_t*` 与 `stride`，**无法**验证外部缓冲的真实长度。凡不能证明长度处，`row` 的可读范围由调用方保证为 `[0, height)`；T007 不做指针容量探测，也不把 `stride >= width` 当作缓冲长度足够的证明。

---

## 2. 几何模型

### 2.1 mi 网格（4×4 单元）

```text
mi_width  = align_pow2(visible_width , 3) >> 2      /* 见 avm\av2\decoder\decoder.c:97 */
mi_height = align_pow2(visible_height, 3) >> 2      /* 见 avm\av2\decoder\inspection.c:31-34 */
mi_cols_in_frame = mi_width
mi_rows_in_frame = mi_height
```

AVM 原文：`mi_params->mi_cols = ALIGN_POWER_OF_TWO(frame_width, 3) >> MI_SIZE_LOG2`（`avm\av2\decoder\decoder.c:97`、`avm\av2\decoder\inspection.c:31-34`、`avm\av2\encoder\encoder_utils.h:56`）。`MI_SIZE_LOG2 = 2`（`avm\av2\common\enums.h:164`）。

因为可见宽高是 8 的倍数，`mi_width = visible_width >> 2`、`mi_height = visible_height >> 2`，mi 网格与可见区域**精确重合**：mi 内的像素范围 `[mi*4, mi*4+4)` 永远落在图像内，"不完整 mi"不存在。

> 这一步是**简化**，不是规范强制：若将来允许非 8 倍数尺寸，mi 网格会超出可见区域，必须改用 §4 的有效尺寸裁剪。当前冻结版本不需要该逻辑。

### 2.2 superblock（SB）网格

```text
sb_size_log2 ∈ {6, 7, 8}        /* 64 / 128 / 256；见 avm\av2\decoder\decodeframe.c:4028-4029 */
mib_size_log2 = sb_size_log2 - 2                 /* avm\av2\common\enums.h:171 */
sb_cols = align_pow2(mi_width , mib_size_log2) >> mib_size_log2   /* tile_common.c:52-54 */
sb_rows = align_pow2(mi_height, mib_size_log2) >> mib_size_log2
```

依据：`avm\av2\common\tile_common.c:52-57`（`mi_cols = ALIGN_POWER_OF_TWO(tiles->mi_cols, mib_size_log2)`，`sb_cols = mi_cols >> mib_size_log2`），同文件 `84-87` 再算一次同样的对齐。

**关键**：对齐用的是 **帧的** mi_cols/mi_rows，**不是** mi_cols 向上取的 2 的幂。因此

- 1920×1080，64×64 SB → `mi = 480×270`，`sb = 8×5`（对 128 SB 则是 `align(480,5)=512`→`16×?`，`270→align(.,5)=288`→`9`，即 `16×9`）。
- 帧的 mi 尺寸**不是** SB 的整数倍时，最右/最下的 SB **部分落在图像外**，该 SB 仍然存在于网格里（`sb_cols/sb_rows` 由向上对齐得到）。
- 该 SB 的 partition 由 decoder 的 `is_partition_implied_at_boundary` 强制决定：`has_rows = (mi_row + hbs_h) < mi_rows`、`has_cols = (mi_col + hbs_w) < mi_cols`，二者不同时成立就进入 forced 分支（`avm\av2\common\av2_common_int.h:4535-4593`）。
- 直接后果：**不能**把帧宽高向上取整到 SB 再按整块遍历，否则会读写可见区域以外的行/列——正是任务书§关键要求与 contracts §1.1 禁止的越界。

### 2.3 冻结的 partition 组合

| 项 | 冻结值 | 依据/说明 |
|---|---|---|
| SB 大小 | **64×64**（`sb_size_log2 = 6`） | 64×64 → 1 个 SB；176×144 → 3×3；1280×720 → 20×12（全部整除）。最小合法值（`MIN_SB_SIZE_LOG2 = 6`，`enums.h:161`），几何最简 |
| 更大 SB（128/256） | **本模块不声明已支持** | decoder 允许（`decodeframe.c:4028-4029`），但 AVM 在 intra 帧会把 256 降为 128（`av2_common_int.h:5285-5295`），再对边界 SB 用显式 split 符号降到 64（`decodeframe.c:1818`、`1885`）。该语法不在本模块范围内 |
| 顶层 partition | `PARTITION_NONE` | 即每个 SB 一个编码块 |
| 更细 partition | **不在本轮** | `traverse` 已按 SB 边界预计算 implied partition，将来加细分时**不得**对边界 SB 调用 `traverse`（§5.3） |

### 2.4 编码块（coding block）几何

```text
x0 = sb_col * 64, y0 = sb_row * 64          /* 像素，左上角 */
visible_w = min(64, visible_width  - x0)    /* >0 guaranteed by *validated* sb_cols */
visible_h = min(64, visible_height - y0)
```

`visible_w/h` 是**该块在各自平面上的有效样本范围**。只有落在 `[x0, x0+visible_w) × [y0, y0+visible_h)` 的样本才属于图像内容；其余属于 §3 的 padding，**数值不属于契约**，任何模块不得依赖。

---

## 3. 内存布局、stride 与 padding

### 3.1 布局

**帧级单块分配**：一次 `malloc` 覆盖三个平面（Y、U、V 依次排列），每帧一次、每平面一次分配，`free` 一次。**不引入对象池、不 per-block malloc**（任务书关键要求）。

```text
stride = align_pow2(plane_visible_width, 5)          /* 32 字节对齐 */
aligned_width  = stride                              /* 见下方说明 */
aligned_height = align_pow2(plane_visible_height, 5) /* 32 行对齐（下 padding） */
plane_bytes    = stride * aligned_height
```

- 地址公式全项目统一：`plane[row * stride + col]`（contracts §1.1）。
- 4:2:0：chroma 可见宽高 = `visible_width >> 1`、`visible_height >> 1`（可见宽高是 8 的倍数，故为整数，且 chroma 尺寸也是 4 的倍数）。
- stride 与对齐值（32）是本模块的**实现选择**，不是规范要求：规范只要求可寻址，不要求特定对齐。若将来需要 64 或 128 对齐，改这一个常量即可，不影响下游。
- 分配量计算必须在**乘法之前**检查 `stride != 0 && aligned_height <= (SIZE_MAX - header) / stride`（contracts §1.4：溢出检查必须在乘法前做）。

### 3.2 三个"宽高"必须分开

| 名词 | 含义 | 谁能依赖 |
|---|---|---|
| 可见宽高 `visible_width/height` | 属于图像的样本范围 | **契约**：所有模块可依赖 |
| stride（字节/行） | 一行占用的字节数，`>= visible_width` | **契约**：所有取样都必须用它 |
| 分配宽高 `aligned_width/aligned_height` | 缓冲实际容量（行数） | **实现细节**：只有 T007 的分配/释放可依赖 |
| 有效宽高 `visible_w/visible_h`（块级） | 某块在某平面上的样本数 | **契约**：T008/T012 写块时用它避免越界 |

`aligned_width` 直接等于 `stride`（因为 stride 已对齐）——保留这个概念是为了 10-bit（M3，`stride` 单位为字节而样本数为 `stride/2`）时口径不变。

### 3.3 padding 与哨兵值

- 每行 `[visible_width, stride)` 是行尾 padding；每平面 `[visible_height, aligned_height)` 是行 padding。
- 填充值：**`pad_value`**，默认 **0**，由调用方在分配时指定（`svt_av2_picture_pad(pic, pad_value)` 重新填充）。
- 用途：让"读到未初始化内存"变成可检测的确定性现象。测试用 `pad_value = 0xA5` 之类的哨兵，在 copy-in/padding **之后**断言 padding 区间未被写坏。
- **不得**用扩边值代替规范的不可用邻居规则（contracts §1.3）：padding 值**不是**预测邻居的替代值。邻居是否可用、取什么替代值由 T008 按规范决定；T007 只保证"padding 区域可读且内容确定"。

### 3.4 copy-in（源图 → 内部源图）

`svt_av2_picture_copy_in(dst, src)`：按 `row` 逐行 `memcpy(visible_width)`，**不做** stride 猜测：
- `dst` 必须是 T007 分配的 picture；
- `src->strides[p] >= src->width_p`，否则返回 `SVT_AV2_ERROR_INVALID_PARAMETER`（contracts §1.4：`stride < visible_width` 是失败条件）；
- `src->planes[p]` 为空 → 失败；
- 复制后不保留任何 `src` 指针（contracts §1.2：调用期消费）。
- 复制后 **padding 不变**（调用方需自行在 copy-in 后调用 `svt_av2_picture_pad` 若要求哨兵可见，或者按 §3.5 的顺序调用）。

### 3.5 推荐的帧生命周期（冻结顺序）

```text
1. svt_av2_picture_alloc(&recon, &cfg)        /* 库拥有；任何失败均不部分写入 */
2. svt_av2_picture_pad(recon, pad_value)      /* 哨兵/填充；仅调试与测试需要 */
3. svt_av2_picture_copy_in(recon, src)        /* 源 → 内部源图（调用期消费） */
   ... 编码：预测/残差/变换/量化/重建 写入 recon ...
4. svt_av2_picture_export_planes(recon, buf, cap, &written)   /* 去 padding 的紧密 raw */
5. svt_av2_picture_free(&recon)               /* 幂等：repeat free 不崩溃、不重复释放 */
```

`export_planes` 输出布局与 `tools\compare_recon.py`、contracts §6 一致：
`Y[W*H] ‖ U[(W/2)*(H/2)] ‖ V[(W/2)*(H/2)]`，**紧密排列、按行去掉 padding**；总长 `W*H + 2*(W/2)*(H/2)`。容量不足返回明确负值（`SVT_AV2_ERROR_INVALID_PARAMETER`），`written` 不写出错误长度。

> 注意：contracts §6 把 recon 导出的**最终形态**列为待 00 冻结（debug 文件 / API / CLI 三选一）。本模块只提供"picture → 紧密 raw 缓冲区"这一个原语，**不**自行决定落盘形式；最终形态由 00 与 T012/T013/T014 定。

---

## 4. 块视图与有效尺寸

```c
typedef struct SvtAv2BlockView {
    uint32_t x;          /* 可见坐标（该平面像素），必为偶数（4:2:0 色度对齐） */
    uint32_t y;
    uint32_t w;          /* 该平面上的有效宽度，>=1 */
    uint32_t h;          /* 该平面上的有效高度，>=1 */
    uint8_t* base;       /* 指向 picture 平面缓冲的 (y*stride + x) 处；可为 NULL（只算几何） */
} SvtAv2BlockView;
```

规则：

1. **单一 x/y 同时用于 luma 与 chroma**：色度坐标是**截断**除法，`x_c = x >> 1`，`y_c = y >> 1`。
   依据：AVM 的 `mi_to_pixel_loc`（`avm\av2\common\blockd.h:1466-1473`）与 `ccso_nhfb`（`avm\av2\decoder\decodemv.c:174`）都是 `mi_col >> subsampling_x` 后整块左移，没有向上取整。
2. **块尺寸必须保证色度坐标不塌缩**：`w >= 8`、`h >= 8`（luma 口径）。`w = 4` 时 `x` 与 `x+4` 会映射到同一个色度列，块在色度上退化为零宽。
3. **色度块尺寸与 luma 块尺寸无关**（不是 `w/2`）：对半宽/半高的块，色度上要用**不同**的块形状，由 AVM/Spec 的 `ss_size_lookup` 表给出（`avm\av2\common\common_data.h:709-743`，注释 "The Subsampled_Size table in the spec"。4:2:0 → `[bsize][1][1]` 列）。
   冻结的本轮组合：

   | luma 块 | 4:2:0 色度块 | 色度有效尺寸（全在图像内时） |
   |---|---|---|
   | 64×64 | 32×32 | 32×32 |
   | 32×32 | 16×16 | 16×16 |
   | 16×16 | 8×8 | 8×8 |
   | 8×8 | 4×4 | 4×4 |
   | 64×32 | 32×16 | 32×16 |

4. **色度最大编码单元 8×8（luma 口径）**：decoder 在 `tree_type == CHROMA_PART && bsize == BLOCK_8X8` 时强制 `PARTITION_NONE`（`avm\av2\common\av2_common_int.h:4620-4622`），且 plane block size 必须能由 `get_plane_block_size` 得到合法值（`av2_common_int.h:4525-4526`；`ss_size_lookup[BLOCK_4X4][1][1] = BLOCK_4X4` 合法，但 `BLOCK_4X4` 已无更细划分）。
5. **有效尺寸按平面各自计算**：
   ```text
   chroma 有效区 = align8(visible_width) >> 1 × align8(visible_height) >> 1
   block w_c = min(chroma_eff_w , (x >> 1) + w/2) - (x >> 1)      /* 夹到图像右/下边 */
   ```
   因为可见宽高是 8 的倍数，`align8(visible_width) == visible_width`，"尾部不足 8 的列不参与色度"这一情形在本轮**不出现**（176/144/1920/1080 全部是 8 的倍数）。
   > 更正记录：`docs\m1-m2-contracts.md` 讨论中出现的"62×62、右侧 2 列不参与色度"表述，其前提（非 8 倍数尺寸）在本轮冻结配置下不成立。T007 不按该前提实现，也不引入为它服务的 `chroma_eff_width` 特殊分支。

---

## 5. partition 遍历接口（供 T011/T012/T013 使用）

### 5.1 遍历单位 = SB，不是整帧

```text
for sb_row in [0, sb_rows):
  for sb_col in [0, sb_cols):
     block = block_from_sb(frame, sb_col, sb_row)       /* §2.4 */
     implied = partition_implied_at_boundary(block)     /* §5.2 */
     if (implied != PARTITION_NONE)  traverse_none(block)   /* 见 §5.3 */
     else                            traverse(block)
```

- `sb_cols/sb_rows` 由 §2.2 的向上对齐得出，保证**覆盖全部可见像素、不遗漏**（"全图覆盖"）。
- 每个 `block` 的坐标/尺寸互不重叠，且 `x + w <= visible_width`、`y + h <= visible_height`（"不重叠、不越界"）。
- 每帧的遍历只做几何，不产生任何语法符号。

### 5.2 边界 SB 的 implied partition（冻结规则）

照 `avm\av2\common\av2_common_int.h:4535-4593` 直译，正方形块情形：

```text
hbs = N/8  （N 为块边长像素数，hbs 为半个块的 mi 数）
has_rows = (mi_row + hbs) < mi_rows_in_frame
has_cols = (mi_col + hbs) < mi_cols_in_frame
if (has_rows && has_cols) → 不是边界块，无 implied
else if ( has_rows && !has_cols) → implied = PARTITION_VERT   /* 只缺列 */
else                             → implied = PARTITION_HORZ   /* 缺行（或行列都缺） */
```

`assert(!has_rows || !has_cols)` 在 AVM 里成立，因为区块一定是正方形（`hbs_w == hbs_h`）。

### 5.3 必须在 T011 之前定的那一件事（待确认）

**本模块的立场**：顶层块一律 `PARTITION_NONE`，边界 SB 的 implied 拆分**不产生任何 fuzz/split 语法符号**，因此 **T011 在 tile 起始处不需要写 partition 符号**。

理由（可复核）：`read_partition` 在这些路径上直接 `return` 而没有 `avm_read_symbol`——
`av2_common_int.h:4136`（`derived_partition != PARTITION_INVALID && partition_allowed[derived_partition]` → 直接返回）、`4155`（`only_allowed_partition` 唯一可行 → 直接返回）。
唯一会写 `do_split` 符号的是 `decodeframe.c:1779`，它只在 `is_do_split_implied(...)` 为假、即多个 partition 都合法时才到达。
**反证 T011 侧的隐患**：若隐含边界拆分真的在合法流里写符号，那 `partition_forced_by_boundary` 这个标志（`decodeframe.c:1964-1967` 置位，`1818`/`1885` 消费）就没有存在意义。

**但这是我从 AVM 源码得出的结论，不是规范原文**，且 `read_partition` 上方的 `(void)has_rows; (void)has_cols;`（`decodeframe.c:1736-1737`）恰好说明这两个参数在当前 decoder 里**没有被用于语法判定**——我没有独立复现该结论的合法码流。
**已于 2026-10-07 由规范正文关闭（原 U2）**：AV2 v1.0.0 §5.20.3.2 `read_partition` 在取得 `(implied, partition)` 后立即 `if (implied && allowed[partition]) return partition;`；`do_split`、`do_square_split`、`rect_type`、`do_ext_partition` 全部位于其后的 `bru_mode != BRU_ACTIVE` 分支内，隐含时不会被读取。故边界隐含划分**不写符号**，与 §5.1 的既有实现一致，几何部分不变。证据：`artifacts\T007\spec-partition-evidence.txt`。

### 5.4 chroma 坐标正确性

- 色度块坐标 = luma `x>>1`、`y>>1`（§4.1），**不是**取整到 8（取整到 8 会让色度块错位，见 AVM `mi_to_pixel_loc`）。
- 色度块尺寸取 §4.3 的表，**不是** `w/2 × h/2`（半宽/半高块上两者不同）。
- 边界处色度块同样按 §4.5 夹到色度有效区。

---

## 6. 失败方式（全部返回明确负值，不部分写入）

| 情形 | 结果 |
|---|---|
| 空指针（picture、out、参数指针） | `SVT_AV2_ERROR_INVALID_PARAMETER` |
| 宽或高为 0 | `SVT_AV2_ERROR_INVALID_PARAMETER` |
| 宽或高不是 8 的倍数 | `SVT_AV2_ERROR_UNSUPPORTED` |
| 4:0:0 / 4:2:2 / 4:4:4（本轮只支持 4:2:0） | `SVT_AV2_ERROR_UNSUPPORTED` |
| 10-bit / 12-bit（本轮只支持 8-bit） | `SVT_AV2_ERROR_UNSUPPORTED` |
| `stride < visible_width`（copy-in / 外部视图） | `SVT_AV2_ERROR_INVALID_PARAMETER` |
| 分配量溢出（乘法前检查） | `SVT_AV2_ERROR_INVALID_PARAMETER` |
| `malloc` 失败 | `SVT_AV2_ERROR_OUT_OF_MEMORY`（picture 保持全零/未分配，可安全 `free`） |
| 导出容量不足 | `SVT_AV2_ERROR_INVALID_PARAMETER`，`*written` 不写 |
| 重复 `svt_av2_picture_free` | `SVT_AV2_ERROR_NONE`（幂等，不崩溃、不二次释放） |
| 重复 `svt_av2_picture_pad` | `SVT_AV2_ERROR_NONE`（幂等，可重复调用） |
| 对未分配（全零）picture 调 `pad`/`copy_in`/`export` | `SVT_AV2_ERROR_INVALID_PARAMETER` |

---

## 7. 交给下游的接口清单

| 使用者 | 用到的接口 | 用途 |
|---|---|---|
| **T008** | `svt_av2_picture_plane(pic, plane)`、`visible_width/height`、`stride`、`svt_av2_block_neighbors(...)`（§5/§4） | 取邻居；邻居**必须**来自内部 recon，不得来自未编码 source（contracts §2.2）。邻居**可用性**归 T008，本模块只提供坐标与 padding 可读范围 |
| **T012** | `alloc/free/copy_in/pad`、`SvtAv2BlockView.base/x/y/w/h`、`export_planes` | 读写重建样本、导出紧密 recon |
| **T013** | `copy_in`（调用期消费）、`visible/aligned` 语义、失败码 | 同步复制输入、按实际负载管理容量（contracts §7） |
| **T011/T015** | §5 遍历几何 + §2.2 SB 网格 | tile/partition 边界的合法规则确认 |

---

## 8. 未决项（禁止猜测，需 00 裁定）

- **U1**（原 contracts §11）：partition 组合的**编码合法性**（哪些 partition 可在 syntax 中出现）归 T011 + T015。本模块只提供几何，不发明 syntax。
- ~~**U2**（本文档 §5.3）~~ **已关闭（2026-10-07）**：已从规范正文解决。AV2 v1.0.0 §5.20.3.2 `read_partition` 在 `(implied, partition) = partition_implied(...)` 之后直接 `if (implied && allowed[partition]) return partition;`，所有描述符（`do_split` / `do_square_split` / `rect_type` / `do_ext_partition`）都在其后的 `bru_mode != BRU_ACTIVE` 分支内，隐含时**不会**被读取。结论：边界隐含划分不产生语法符号，与 T007 实现一致。证据落盘 `artifacts\T007\spec-partition-evidence.txt`。
- **U3**（contracts §11）：recon 导出的最终形态（debug 文件 / API / CLI）。本模块只提供紧密 raw 缓冲区原语。
- **U4**：10-bit / 4:2:2 / 4:4:4 的 stride 与块几何（M3 及以后）。
- **U5**：128/256 SB 的边界显式拆分语法（本模块不声明支持）。
