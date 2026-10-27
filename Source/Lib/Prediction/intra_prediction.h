/*
 * SVT-AV2 minimal scalar intra prediction (T008).
 *
 * Scope: the AV2 intra prediction modes this encoder actually signals, plus the
 * normative unavailable-neighbour substitution needed to build their reference
 * samples. Only DC_PRED is implemented (frozen contract 0: "prediction: DC_PRED;
 * additional modes only when the closed loop genuinely needs them").
 *
 * Spec truth: AV2 v1.0.0
 *   - § 7.13.2.1   General intra prediction process (inputs, AboveRow/LeftCol)
 *   - § 7.13.2.10  DC intra prediction process
 *   - § 4.8        Clip1 / Round2 / FloorLog2 (approx_divide support)
 * AVM cross-check (read-only reference, not copied):
 *   - avm\avm_dsp\intrapred.c:175-236        dc_128/dc_left/dc_top/dc predictors
 *   - avm\av2\common\reconintra.c:1150-1262  unavailable-neighbour substitution
 *   - avm\av2\common\warped_motion.h:81-95   resolve_divisor_32
 *
 * Freeze prefix: this interface is consumed by T012 (reconstruction) and T009.
 * Changing a signature or a value here needs 00's agreement.
 */
#ifndef SVT_AV2_INTRA_PREDICTION_H
#define SVT_AV2_INTRA_PREDICTION_H

#include "common.h"
#include "intra_mode_map.h"
#include "picture.h"

#include <stddef.h>
#include <stdint.h>

/* Maximum transform dimension this module predicts. AV2 §7.13.2.1 caps the
 * predicted region at the transform block; 64 is the largest transform side in
 * the frozen configuration. */
#define SVT_AV2_MAX_TX_SIZE 64u

/* Number of reference samples this module needs per direction. The DC process
 * only reads w samples of AboveRow and h samples of LeftCol (§7.13.2.10), and
 * w,h <= SVT_AV2_MAX_TX_SIZE. */
#define SVT_AV2_MAX_REF_SAMPLES SVT_AV2_MAX_TX_SIZE

/* Intra prediction modes. The numeric values match the AV2 spec's
 * PREDICTION_MODE ordering so that a future syntax writer can reuse them; only
 * DC_PRED is produced by this module today. */
typedef enum SvtAv2IntraMode {
    SVT_AV2_INTRA_DC_PRED = 0,
    SVT_AV2_INTRA_MODE_COUNT
} SvtAv2IntraMode;

/* Reference samples of one prediction region, already substituted.
 *
 * `above` holds AboveRow[0 .. w-1] and `left` holds LeftCol[0 .. h-1] as used by
 * the DC process (§7.13.2.10 reads exactly those indices). The corner sample
 * AboveRow[-1] is deliberately NOT stored: the DC process never reads it, and
 * §7.13.2.10's haveLeft/haveAbove branches are decided by the caller.
 *
 * Both arrays are filled by svt_av2_intra_reference_build, so a predictor never
 * reads outside the visible reconstruction. */
typedef struct SvtAv2IntraReference {
    uint32_t bit_depth;
    uint16_t above[SVT_AV2_MAX_REF_SAMPLES];
    uint16_t left[SVT_AV2_MAX_REF_SAMPLES];
    uint32_t width;  /* number of valid entries in `above` (w) */
    uint32_t height; /* number of valid entries in `left` (h) */
} SvtAv2IntraReference;

/* A prediction region in image coordinates, matching T007's convention
 * (docs/t007-picture-block-geometry-v2.md §1): x/y are plane-relative pixel
 * coordinates of the top-left sample, and the region covers col in [x, x+w),
 * row in [y, y+h).
 *
 * This is deliberately the same shape as SvtAv2BlockView but with the transform
 * size made explicit: §7.13.2.1 invokes the intra process per transform block,
 * so a coding block with a smaller transform is predicted in several calls. */
typedef struct SvtAv2IntraBlock {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t plane; /* SVT_AV2_PLANE_Y / _U / _V */
} SvtAv2IntraBlock;

/* --- reference sample construction ----------------------------------------- */

/* Builds AboveRow/LeftCol for `block` from `recon`, applying the normative
 * unavailable-neighbour substitution.
 *
 * `recon` MUST be the internal reconstruction, never the uncoded source
 * (contract 2.2). Samples are read from the visible area only; the function
 * never touches padding, so its output cannot depend on uninitialised memory.
 *
 * Substitution, per AV2 §7.13.2.1 (the AboveRow/LeftCol construction preceding
 * the mode dispatch) and cross-checked against
 * avm\av2\common\reconintra.c:1197-1236:
 *   - haveLeft == 1 && haveAbove == 1: read the real neighbours.
 *   - haveLeft == 1 && haveAbove == 0: every AboveRow sample = the left column's
 *     first sample (the sample at (x-1, y)).
 *   - haveLeft == 0 && haveAbove == 1: every LeftCol sample = the above row's
 *     first sample (the sample at (x, y-1)).
 *   - haveLeft == 0 && haveAbove == 0: all reference samples = 1 << (BitDepth-1),
 *     i.e. 128 for 8-bit.
 *
 * Availability is decided from the frame boundary alone:
 *   haveLeft  = (block->x > 0)
 *   haveAbove = (block->y > 0)
 * which is exactly what T007's svt_av2_block_neighbors reports, and is valid for
 * chroma as well because the chroma boundary is identical to the luma boundary
 * (t007-picture-block-geometry-v2.md §3).
 *
 * Returns SVT_AV2_ERROR_INVALID_PARAMETER for a NULL pointer, a zero size, a
 * size above SVT_AV2_MAX_TX_SIZE, an out-of-range plane, or a region that does
 * not lie inside the visible area. On failure `ref` is left zeroed. */
SvtAv2Error svt_av2_intra_reference_build(const SvtAv2FrameBuffer* recon,
                                          const SvtAv2IntraBlock* block,
                                          SvtAv2IntraReference* ref);

/* Same, but takes the two availability flags explicitly instead of deriving them
 * from the frame boundary.
 *
 * T012 needs this for non-frame-boundary unavailability: an AV2 superblock
 * boundary, a tile boundary or a not-yet-reconstructed neighbour also makes a
 * reference sample unavailable even though x>0 / y>0 inside the picture. The
 * substitution rules are identical to the ones above; only the source of the two
 * flags differs. When a flag is 0 the corresponding picture samples are not read
 * at all. */
SvtAv2Error svt_av2_intra_reference_build_flags(const SvtAv2FrameBuffer* recon,
                                                const SvtAv2IntraBlock* block,
                                                int have_left,
                                                int have_above,
                                                SvtAv2IntraReference* ref);

/* --- prediction -------------------------------------------------------------- */

/* Runs the DC prediction process (AV2 §7.13.2.10) over `block`, writing the
 * predicted samples into `dst` at `dst_stride` bytes per row.
 *
 * `ref` must have been produced for the same region by one of the builders
 * above; `ref->width`/`ref->height` must equal `block->w`/`block->h`.
 *
 * The three averaged branches come straight from §7.13.2.10:
 *   haveLeft && haveAbove : avg      = Clip1( approx_divide(sum, w + h) )
 *   haveLeft && !haveAbove: leftAvg  = Round2(sum_left,  log2H)
 *   !haveLeft && haveAbove: aboveAvg = Round2(sum_above, log2W)
 *   neither               : 1 << (BitDepth - 1)
 * haveLeft/haveAbove are not passed separately: the builder records them in the
 * reference by the sizes it filled, and a substituted reference is by
 * construction the one the spec's branch would have produced, so the three
 * branches coincide with what the builder filled. To keep the branch selection
 * explicit and auditable, use svt_av2_intra_dc_predict_flags when the flags are
 * known independently.
 *
 * Returns SVT_AV2_ERROR_INVALID_PARAMETER on any inconsistency (NULL, zero size,
 * size above the maximum, stride smaller than the visible width, ref sizes
 * disagreeing with the block). On failure nothing is written. */
SvtAv2Error svt_av2_intra_dc_predict(const SvtAv2IntraReference* ref,
                                     const SvtAv2IntraBlock* block,
                                     uint8_t* dst,
                                     uint32_t dst_stride);

/* Explicit-flag variant of svt_av2_intra_dc_predict. `ref` must have been built
 * with the same flags. This is the entry point T012 should use, because the
 * availability that matters at reconstruction time includes tile and superblock
 * boundaries that the picture alone cannot express. */
SvtAv2Error svt_av2_intra_dc_predict_flags(const SvtAv2IntraReference* ref,
                                           const SvtAv2IntraBlock* block,
                                           int have_left,
                                           int have_above,
                                           uint8_t* dst,
                                           uint32_t dst_stride);

/* M5 G1 scalar-only axis prediction. This is not a wire-mode identifier and
 * is not selected by the encoder until the AV2 mode syntax is verified.
 * V copies each above sample down its column; H copies each left sample
 * across its row. The caller supplies already substituted reference samples.
 * Invalid geometry/depth/stride/axis is rejected before touching dst. */
typedef enum SvtAv2IntraAxis {
    SVT_AV2_INTRA_AXIS_VERTICAL = 0,
    SVT_AV2_INTRA_AXIS_HORIZONTAL = 1
} SvtAv2IntraAxis;

SvtAv2Error svt_av2_intra_axis_predict(const SvtAv2IntraReference* ref,
                                      const SvtAv2IntraBlock* block,
                                      SvtAv2IntraAxis axis,
                                      uint8_t* dst,
                                      uint32_t dst_stride);

/* I1 internal mode IDs preserve M5 DC/V/H and RD's reserved inter ID 3. */
enum {
    SVT_AV2_INTRA_SMOOTH = 4,
    SVT_AV2_INTRA_SMOOTH_V = 5,
    SVT_AV2_INTRA_SMOOTH_H = 6,
    SVT_AV2_INTRA_PAETH = 7,
    SVT_AV2_INTRA_D45 = 8,
    SVT_AV2_INTRA_D67 = 9,
    SVT_AV2_INTRA_D113 = 10,
    SVT_AV2_INTRA_D135 = 11,
    SVT_AV2_INTRA_D157 = 12,
    SVT_AV2_INTRA_D203 = 13,
    SVT_AV2_INTRA_CFL_IMPLICIT = 14
};
/* The caller supplies availability of the extra TR/BL endpoint from its decoded
 * block map. Ordinary top/left availability comes from the frame boundary.
 * Supports DC/V/H and the four I1 modes in raster reconstruction order. */
SvtAv2Error svt_av2_intra_basic_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t mode, int top_right, int bottom_left,
    uint8_t* dst, uint32_t stride);

/* Square directional modes 8..61: fixed AVM IDIF for Y, two-tap interpolation
 * for inherited UV angles. TR/BL are contiguous decoded sample counts beyond
 * the ordinary edge, each at most the side size; edge filter/MRLS/IBP are off. */
SvtAv2Error svt_av2_intra_directional_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t mode, uint32_t top_right,
    uint32_t bottom_left, uint8_t* dst, uint32_t stride);

/* AV2 classic CfL, shared coding/transform block; signed alpha_q3 is -8..8.
 * Current co-located Y and causal U/V neighbours must already be reconstructed.
 * Block coordinates are chroma samples. */
SvtAv2Error svt_av2_intra_cfl_explicit_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t sb_size, uint8_t* dst,
    uint32_t stride, int32_t alpha_q3);

/* AV2 implicit CfL, filter index zero; the same reconstruction prerequisites
 * apply. alpha_q8 may be NULL. */
SvtAv2Error svt_av2_intra_cfl_implicit_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t sb_size, uint8_t* dst,
    uint32_t stride, int32_t* alpha_q8);

/* Fixed-v1 filter0 MHCCP. Dimensions in allowed() are luma coding dimensions.
 * The caller supplies full causal TR-by-width / BL-by-height counts in UV pixels.
 * Current co-located clipped Y and causal UV boundaries must be reconstructed. */
int svt_av2_intra_mhccp_allowed(uint32_t width, uint32_t height, uint32_t chroma_format);
uint32_t svt_av2_intra_mhccp_size_group(uint32_t width, uint32_t height);
SvtAv2Error svt_av2_intra_mhccp_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t direction, uint32_t top_right,
    uint32_t bottom_left, uint8_t* dst, uint32_t stride, int32_t* parameters);

/* --- residual ----------------------------------------------------------------- */

/* Computes residual = source - prediction over `block`, as int16_t
 * (contract 2.3: range -255..+255, explicitly signed, never a truncated
 * uint8_t subtraction).
 *
 * Output layout (BLOCK-LOCAL, and this is the only correct reading):
 * `residual` holds the block's own w x h samples starting at index 0, so the
 * sample for block offset (i, j) is written to `residual[i * residual_stride + j]`
 * with i in [0, h) and j in [0, w). The buffer is NOT indexed in plane
 * coordinates: block->x and block->y select where to READ from the two pictures,
 * but never displace the output. Consequently residual_stride must be >= block->w,
 * only rows [0, h) are written, and only columns [0, w) of those rows. This is the
 * layout the transform consumes next (T009), which is why it is block-local
 * rather than plane-sized.
 *
 * `residual_stride` is in SAMPLES (int16_t units), not bytes, because the
 * residual is an intermediate buffer this project owns rather than a
 * byte-addressed picture plane; this is the one place the project-wide
 * "stride in bytes" rule (contract 1.1) does not apply.
 *
 * The stride is checked against the BLOCK width, not the plane width: unlike the
 * picture planes, this buffer only ever holds one block.
 *
 * Samples outside the visible area are read from neither image: only
 * col in [x, x+w) and row in [y, y+h) are touched, so a block at the right or
 * bottom edge never reads past the visible area.
 *
 * Returns SVT_AV2_ERROR_INVALID_PARAMETER for NULL pointers, zero size,
 * out-of-range plane, a region outside the visible area, a stride smaller than
 * block->w, or two pictures whose plane geometry disagrees. On failure nothing is
 * written. */
SvtAv2Error svt_av2_residual_compute(const SvtAv2FrameBuffer* source,
                                     const SvtAv2FrameBuffer* prediction,
                                     const SvtAv2IntraBlock* block,
                                     int16_t* residual,
                                     uint32_t residual_stride);

/* --- introspection ------------------------------------------------------------ */

/* Human-readable name of a prediction mode, for logs and test output. Returns
 * "INVALID" for an unknown value. */
const char* svt_av2_intra_mode_name(SvtAv2IntraMode mode);

SvtAv2Error svt_av2_intra_mrls_predict(const SvtAv2FrameBuffer* recon,
    const SvtAv2IntraBlock* block, uint32_t mode, uint32_t mrl,
    uint32_t top_right, uint32_t bottom_left, uint8_t* dst, uint32_t stride);

#endif /* SVT_AV2_INTRA_PREDICTION_H */
