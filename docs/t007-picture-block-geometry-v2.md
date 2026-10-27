# T007 相对路径契约（picture / block geometry）v2

**这是唯一权威版本。**`docs\t007-picture-block-geometry.md` §4/§5.2/§5.3 中涉及块坐标的散文表述已被本文件取代
（原文按规范风格写作，把块坐标当作**块内相对坐标** `[0, w)` 处理；本模块实际接口返回**图像坐标系相对平面原点的坐标**）。
冻结后两处将合并，以本文件为准。

冻结日期：2026-10-07。只影响 T007 模块内部口径：`svt_av2_block_neighbors` 的入参与 `SvtAv2BlockView.x/y` 的含义。
对 T008/T012 而言坐标语义不变（仍是"该平面像素、相对平面原点"），只是**写清楚了**，并给了 T008 需要的邻居查询原语。

## 1. 唯一坐标系

- `SvtAv2BlockView.x` / `.y`：块左上角在**该平面内**的像素坐标，相对平面原点（可见区左上角）。
- 所有几何函数（`svt_av2_partition_traverse` 的回调、`svt_av2_block_neighbors`）一律使用这个坐标系。
- **不做包内相对坐标**：不出现 `[0, w)` 口径的 `x_rel`、也不出现"块内坐标 + 转换函数"这套双口径。
  理由：一个整数坐标体系足以表达 AVM 的半开区间 `[x, x+w)` 采样约定，且避免 T008 在两种口径间来回转换出错。

## 2. 二维半开区间约定（与 AVM 一致）

- 块覆盖 `col ∈ [x, x+w)`、`row ∈ [y, y+h)`。
- **chroma 采样一律 `x_c = x >> 1`、`y_c = y >> 1`**（截断除法，不向上取整）。
  依据：`avm\av2\common\blockd.h:1466-1473`（`mi_to_pixel_loc`）、`avm\av2\decoder\decodemv.c:174`。

## 3. 边界判定（T008 直接使用）

```c
/* 左/上邻居是否存在：块的上/左边缘是否贴着图像边界 */
int has_left   = (blk->x > 0);
int has_above  = (blk->y > 0);
```

**不需要 chroma 专用字段**。理由是 chroma 边界与 luma 边界恒等：

- 上一块 chroma 行的右端 chroma 列 = `(x >> 1) - 1`，其 luma 覆盖 `[x-2, x)`；本块 chroma 左端 = `x >> 1`，其 luma 覆盖 `[x, x+2)`。
  "两段相等" ⇔ `(x-2) >> 1 == x >> 1` ⇔ **`x` 是奇数** ⇔ `x` 不是偶数（4:2:0 下 `x` 必为偶数）。
- `x > 0` ⇒ `x >= 2`（偶数）⇒ `(x >> 1) - 1 < x >> 1` ⇒ 两段不相等 ⇒ chroma 上确有一条真实的前一列。
- `x == 0` ⇒ 本块 chroma 左端为 0 ⇒ 图像左边界，无前一块。

T008 若选择跨块引用共享 chroma 边界（AVM 的 chroma reference 机制），**必须**改为查询本模块的
`svt_av2_chroma_ref_view()`（§5），而不是依赖"自己的块"。两者不是同一个图。

## 4. `svt_av2_block_neighbors`（T008 的邻居查询原语）

给定 `pic`、`plane`、块视图（图像坐标），返回**可用性 + 取值**：

```c
typedef struct SvtAv2BlockNeighbors {
    uint32_t above[2];  /* {sample at (x-1, y-1), sample at (x, y-1)}；无效时取值未定义 */
    uint32_t left[2];   /* {sample at (x-1, y-1), sample at (x-1, y)} */
    int      above_valid;
    int      left_valid;
} SvtAv2BlockNeighbors;
```

返回码：

| 情况 | `above_valid` / `left_valid` | 返回 |
|---|---|---|
| x>0 且 y>0（两个邻居都在） | 1 / 1 | `SVT_AV2_ERROR_NONE` |
| x==0 且 y>0（只有上） | 1 / 0 | `SVT_AV2_ERROR_NONE` |
| x>0 且 y==0（只有左） | 0 / 1 | `SVT_AV2_ERROR_NONE` |
| x==0 且 y==0（都没有） | 0 / 0 | `SVT_AV2_ERROR_NONE` |

**语义边界（必须写进 T008 的实现）**：

- 本函数**只负责读像素与报可用性**，返回的是**已重建样本**（它读 `pic`，也就是内部 recon）。
  它**不**实现任何规范的不可用邻居替代值（DC 类模式在被 reference 样本缺失时的取值），那是 T008 按规范做的事。
- 因此"全部不可用"不是错误，而是一个合法的输入状态：`valid` 全 0，`above[]`/`left[]` 数值未定义（实现里保持为 0，但不作为契约）。
- 若调用方对 `valid == 0` 的项读取数值，属于调用方越权；T007 不保证其内容。

## 5. chroma reference 与 partition 遍历的块图（几何层，无语法）

### 5.1 为什么必须分开

AVM 在 `SHARED_PART` 下，色度**不是**逐 luma 块采样，而是走 chroma reference 图：

- `set_chroma_ref_info`（`avm\av2\common\blockd.h:1418-1463`）在父块有 `chroma_ref_offset` 时，
  只有 `is_sub_partition_chroma_ref(...)` 为真的子块才 `is_chroma_ref = 1`，其余子块共享**父块**的 `bsize_base` 与 chroma 基准坐标。

所以"每个 luma 块配一个 `w/2 × h/2` 色度块"是**错的**（本模块 v1 文档 §4.3 也已指出）。本模块需要：

1. **primitives**：`ss_size_lookup`（`avm\av2\common\common_data.h:709-743`，"The Subsampled_Size table in the spec"）→ 由 luma 块形状直接得到色度块形状，对**所有**合法 luma 形状都成立，与 partition 无关。
2. **chroma reference 解析**（本文档只到几何层）：把任意 luma 块映射到它的 chroma 基准块。

本轮冻结的组合恰好让两者重合（luma 块边长 ≥ 8、且都是正方形或 2:1 矩形，`have_nz_chroma_ref_offset` 为假），
即 chroma ref == 自身。**但接口不依赖这一点**，`svt_av2_chroma_ref_view()` 按父子关系计算，将来加细分不用改调用方。

### 5.2 `svt_av2_chroma_ref_view(frame, blk, &out)`

```text
chroma_eff_w = visible_width  >> 1
chroma_eff_h = visible_height >> 1

/* 基准块 = 块自身或其祖先；本轮组合下恒为自身 */
out->x = in.x >> 1
out->y = in.y >> 1
out->w = chroma_view_width_from_luma(in.w)    /* ss_size_lookup[..][1][1] 的块宽，见 §4.3 */
out->h = chroma_view_height_from_luma(in.h)
/* 夹到色度有效区（右/下边界 SB） */
if (out->x >= chroma_eff_w || out->y >= chroma_eff_h) → 该块无 chroma 覆盖，返回 SVT_AV2_ERROR_UNSUPPORTED
out->w = min(out->w, chroma_eff_w - out->x)
out->h = min(out->h, chroma_eff_h - out->y)
```

**判决记录（2026-10-07）**：本模块 v1 文档 §4.3 写的 `w_c = w/2`、`h_c = h/2` 是**错的**，现修正为上表的表查值。
若 luma 块被分到 4×4（本轮不发生），色度侧退化为共享父块，由 §5.1 的 chroma reference 规则处理。

## 6. partition 遍历的 implied 分支（T007 裁决，写在几何层）

`read_partition`（`avm\av2\decoder\decodeframe.c:1728-1758`）的三条 early-return 都不写符号：

```c
const int plane = xd->tree_type == CHROMA_PART;                    /* 1738 */
PARTITION_TYPE derived = av2_get_normative_forced_partition_type(...);/* 1741-1743 */
if (derived != PARTITION_INVALID && partition_allowed[derived])
    return derived;                                                /* 1750-1752 无 read_symbol */
derived = only_allowed_partition(partition_allowed);
if (derived != PARTITION_INVALID)
    return derived;                                                /* 1755-1757 无 read_symbol */
```

`av2_get_normative_forced_partition_type`（`av2_common_int.h:4608-4638`）对边界块（`has_rows`/`has_cols` 不同时为真）
走 `is_partition_implied_at_boundary`，返回 `VERT`（只缺列）或 `HORZ`（缺行）。

**由此确定：边界 implied 拆分在合法流中不写 `do_split` / `do_square_split` / 其他 partition 符号。**

因此 `svt_av2_partition_traverse` 对边界块直接以 §5.2 的几何**递归切分**（D5.3 实现的 `recurse_square_at_boundary`），
**不调用 `visit`、不产生任何语法符号**；`svt_av2_partition_implied_at_boundary` 只用于让 T011/T015 在
"需要写符号的地方"判定哪些块免写。

> 与 `svt_av2_block_neighbors` 的一致性：边界块切分后每个子块仍然"没有任何可见前驱"，
> 因此它们的 `above_valid`/`left_valid` 也应当为 0。遍历中调用 `visit` 的块才代表语法块。

**该结论依据的是 AVM decoder 源码，不是规范 PDF 原文**（本机无 PDF 文本提取工具，见报告"未验证项"）。
需要 T011/T015 用独立码流复核。

> TODO（本轮不做，记录在案）：当 partition 细分落到 4×4 luma 且 chroma 共享父块时，
> 实现 §5.1 的 `set_chroma_ref_info` 父子偏移表（`get_chroma_ref_offsets`，`av2_common_int.h:4640-4686`）。
> 本轮 luma 块不小于 8×8，不触发。

## 7. 上游存在的真实缺陷（T007 报告，不在 T007 归属内）

1. **`Source\Lib\Common\y4m.c:237` 越界读**：`memcpy(info->color_space, "420", 4)` 读的是长度 4 的字符串字面量
   `"420"`（含 NUL，共 4 字节）——这一处本身正好 4 字节，**不是**越界；真正的问题是它把
   `color_space` 声明为 `char[16]` 却只初始化到 4 字节，其余 12 字节保留 `memset` 的 0，**无缺陷**。
   此处更正：**该条为误判，撤销**，不报给 00 交 T013。
2. **`y4m.c` 只解析头部、不读帧负载**（设计如此，属 T013 的输入循环）。T007 的 `copy_in` 需要调用方提供
   已定位到帧负载的 plane 指针；**不要**让 T007 依赖 y4m 的帧读取能力。
