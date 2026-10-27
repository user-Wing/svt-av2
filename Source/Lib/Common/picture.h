/*
 * SVT-AV2 picture and plane storage (T007).
 *
 * Scope: the 8-bit 4:2:0 plane storage used by source pictures and by the
 * encoder's internal reconstruction. This module owns allocation, padding,
 * copy-in and the tightly packed raw export. It deliberately contains no
 * prediction, transform or quantisation logic.
 *
 * Frozen contract: docs/t007-picture-block-geometry.md (and the -v2 addendum).
 * Spec references used by the geometry:
 *   - FFmpeg/YUV4MPEG 4:2:0 plane layout: chroma is (width>>1) x (height>>1).
 *   - avm\av2\decoder\decoder.c:97  mi_cols = ALIGN_POWER_OF_TWO(width, 3) >> 2
 *
 * Caller responsibility (do not try to detect it): the API only receives a
 * byte stride, so it can never validate the real length of an external buffer.
 * Everywhere the length cannot be proven, the caller guarantees that rows
 * [0, height) are readable. This module does not probe pointer capacity.
 */
#ifndef SVT_AV2_PICTURE_H
#define SVT_AV2_PICTURE_H

#include "common.h"

#include <stddef.h>
#include <stdint.h>

/* Maximum number of planes handled by this module (Y, U, V). */
#define SVT_AV2_PICTURE_MAX_PLANES 3

/* Internal frame storage. Distinct from the public SvtAv2Picture in
 * include/svt_av2.h, which is the caller's borrowed view of an input frame:
 * this one owns its buffers, carries the allocation geometry and is what the
 * reconstruction is written into.
 *
 * All planes live in one frame-level allocation; the plane pointers stay valid
 * until svt_av2_picture_free. */
typedef struct SvtAv2FrameBuffer {
    uint32_t bit_depth;
    SvtAv2ChromaFormat chroma_format;
    uint32_t visible_width;  /* luma samples, 8-bit: equals bytes per visible row */
    uint32_t visible_height; /* luma rows */
    uint32_t aligned_height[SVT_AV2_PICTURE_MAX_PLANES]; /* allocated rows per plane */
    uint32_t strides[SVT_AV2_PICTURE_MAX_PLANES];        /* bytes per row */
    uint32_t plane_count;                                /* 3 for this module */
    uint8_t* planes[SVT_AV2_PICTURE_MAX_PLANES];         /* NULL until allocated */
    uint8_t* buffer;                                     /* single frame allocation */
} SvtAv2FrameBuffer;

/* Plane indices. Kept explicit so no caller has to remember the order. */
#define SVT_AV2_PLANE_Y 0u
#define SVT_AV2_PLANE_U 1u
#define SVT_AV2_PLANE_V 2u

/* Read-only borrowed view of caller-owned planes.
 *
 * This exists so that a caller whose input is NOT an allocation (for example
 * the public SvtAv2Picture in include/svt_av2.h, which is only a set of
 * borrowed plane pointers) can be copied into an allocated SvtAv2FrameBuffer
 * without faking a buffer pointer to satisfy picture_is_allocated().
 *
 * It is deliberately a distinct type from SvtAv2FrameBuffer, not an
 * SvtAv2FrameBuffer with a NULL buffer:
 *   - it owns no memory and carries no allocation descriptor;
 *   - its plane pointers are const (the caller keeps ownership);
 *   - it is only meaningful as the `source` of svt_av2_picture_copy_from_view.
 *
 * Keeping it separate keeps the SvtAv2FrameBuffer invariant ("buffer != NULL
 * means allocated") exactly true for every function that takes one, so no
 * downstream code has to re-check whether a frame it was handed is real.
 *
 * Lifetime: a view is valid only for the duration of the call that consumes it
 * (contract 1.2). No pointer inside it is retained afterwards, so the borrowed
 * memory may be released as soon as that call returns and must not be modified
 * while the view is in use. */
typedef struct SvtAv2BorrowedView {
    uint32_t bit_depth;
    SvtAv2ChromaFormat chroma_format;
    const uint8_t* planes[SVT_AV2_PICTURE_MAX_PLANES]; /* caller-owned, read-only */
    uint32_t       strides[SVT_AV2_PICTURE_MAX_PLANES]; /* bytes per row */
    uint32_t       visible_width;                       /* luma samples, 4:2:0 */
    uint32_t       visible_height;                      /* luma rows */
} SvtAv2BorrowedView;

/* --- geometry helpers ------------------------------------------------------ */

/* Visible width/height of one plane. Planes 1..2 are chroma: (w>>1) x (h>>1)
 * for the only supported chroma format, 4:2:0. Returns 0 for an unusable
 * plane index. */
uint32_t svt_av2_picture_plane_width(const SvtAv2FrameBuffer* picture, uint32_t plane);
uint32_t svt_av2_picture_plane_height(const SvtAv2FrameBuffer* picture, uint32_t plane);

/* Read-only accessor. Returns NULL when the picture is not allocated or the
 * plane index is out of range. */
const uint8_t* svt_av2_picture_plane_const(const SvtAv2FrameBuffer* picture, uint32_t plane);

/* Writable accessor for the internal reconstruction picture. Returns NULL when
 * the picture is not allocated or the plane index is out of range. */
uint8_t* svt_av2_picture_plane(SvtAv2FrameBuffer* picture, uint32_t plane);

/* --- lifecycle ------------------------------------------------------------- */

/* Allocates luma `width` x `height`, 8-bit 4:2:0.
 *
 * `width` and `height` must be non-zero multiples of 8 (see the frozen
 * contract: the AV2 mi grid and the chroma reference blocks are both 8x8
 * granular, so any other size would produce chroma columns that cannot be
 * legally coded). Anything else returns SVT_AV2_ERROR_UNSUPPORTED; zero or a
 * NULL pointer returns SVT_AV2_ERROR_INVALID_PARAMETER. On failure `picture`
 * is left fully zeroed and safe to pass to svt_av2_picture_free().
 *
 * Ownership: the library owns every buffer; release with svt_av2_picture_free.
 * Alignment: the frame buffer comes from one malloc, so nothing beyond the
 * standard malloc alignment is promised.
 */
SvtAv2Error svt_av2_picture_alloc(SvtAv2FrameBuffer* picture, uint32_t width, uint32_t height);

/* Frees every buffer and zeroes the descriptor. Idempotent: calling it twice,
 * or on a zeroed descriptor, is not an error. */
SvtAv2Error svt_av2_picture_free(SvtAv2FrameBuffer* picture);

/* Fills the padding area of every plane with `pad_value`. Idempotent.
 *
 * Padding is the region outside the visible area but inside the allocation:
 * columns [visible_width, stride) of each row and rows
 * [visible_height_p, aligned_height) of each plane. Visible samples are never
 * touched.
 *
 * Padding is a memory-safety aid, NOT a substitute for the normative
 * unavailable-neighbour rules: it only guarantees that reading padding is
 * deterministic instead of reading uninitialised memory. Choosing the value to
 * use when a prediction neighbour is unavailable is T008's job. */
SvtAv2Error svt_av2_picture_pad(SvtAv2FrameBuffer* picture, uint16_t pad_value);

/* Copies `source` into `destination`, row by row, using each plane's own
 * visible width. Both pictures must be allocated with the same geometry.
 *
 * `source` planes are read for the duration of this call only; no source
 * pointer is retained afterwards (contract 1.2: consumed at call time).
 *
 * Fails without partial writes when: either picture is unallocated, the
 * geometries differ, a source plane pointer is NULL, or a source stride is
 * smaller than that plane's visible width (a stride shorter than the visible
 * width can never describe a readable row). */
SvtAv2Error svt_av2_picture_copy_in(SvtAv2FrameBuffer* destination, const SvtAv2FrameBuffer* source);

/* Builds a read-only borrowed view of caller-owned planes.
 *
 * Pure descriptor construction: it reads no samples, writes no samples and
 * allocates nothing. The geometry checks are the same as svt_av2_picture_alloc:
 * `width`/`height` must be non-zero multiples of 8, every one of the three
 * plane pointers must be non-NULL and every stride must be at least that
 * plane's visible width (a shorter stride cannot describe a readable row).
 *
 * Fails without leaving a partially built view: on any error `*out` is left
 * fully zeroed. A zeroed view is never a valid source and is rejected by
 * svt_av2_picture_copy_from_view.
 *
 * Ownership: the caller keeps every plane. The view borrows them; see the
 * lifetime note on SvtAv2BorrowedView. */
SvtAv2Error svt_av2_picture_view(SvtAv2BorrowedView* out,
                                 const uint8_t* const planes[SVT_AV2_PICTURE_MAX_PLANES],
                                 const uint32_t strides[SVT_AV2_PICTURE_MAX_PLANES],
                                 uint32_t width,
                                 uint32_t height);

/* Copies a read-only borrowed view into `destination`, row by row, using each
 * plane's own visible width. This is the borrowed-input counterpart of
 * svt_av2_picture_copy_in and shares its single implementation: the two differ
 * only in where the source planes come from.
 *
 * The checks are the same as svt_av2_picture_copy_in and fail without partial
 * writes: `destination` must be allocated, the view geometry must be non-zero
 * and equal to the destination's, every view plane pointer must be non-NULL and
 * every view stride must be at least that plane's visible width.
 *
 * The view is read for the duration of this call only; no pointer into it is
 * retained afterwards (contract 1.2: consumed at call time). */
SvtAv2Error svt_av2_picture_copy_from_view(SvtAv2FrameBuffer* destination,
                                           const SvtAv2BorrowedView* source);

/* Copies the visible samples of every plane into `out` as tightly packed
 * 8-bit 4:2:0 raw, in Y, U, V order, with all padding removed (contract 6).
 *
 * This is THE single export primitive, used for BOTH the source picture and the
 * reconstructed picture: T012/T013 must not write a second copy of it. It only
 * reads visible samples, so the same call works before encoding (source) and
 * after reconstruction (recon); the two differ solely in what the caller wrote
 * into the planes.
 *
 * The caller identifies the frame by KNOWING which frame it passed in: the
 * output carries no frame index and no visible dimensions of its own. The
 * visible geometry is available separately from the SvtAv2FrameBuffer fields
 * (visible_width / visible_height) and from svt_av2_picture_raw_size, which a
 * caller walking a sequence must record alongside the exported bytes so that
 * T014 can compare frames one by one.
 *
 * Required length = W*H + 2*(W/2)*(H/2) bytes. When `out_capacity` is smaller
 * the call fails with SVT_AV2_ERROR_INVALID_PARAMETER and `*out_size` is not
 * written. `out_size` may be NULL only when the caller does not need the
 * length (it is still validated against the capacity). */
SvtAv2Error svt_av2_picture_export_raw(const SvtAv2FrameBuffer* picture,
                                       uint8_t* out,
                                       size_t out_capacity,
                                       size_t* out_size);

/* Total tightly packed size of the visible samples, in bytes. */
SvtAv2Error svt_av2_picture_raw_size(const SvtAv2FrameBuffer* picture, size_t* out_size);

/* Byte-safe sample access; 10-bit planes contain unsigned little-endian words. */
static inline uint16_t svt_av2_sample_load(const uint8_t* sample, uint32_t bit_depth) {
    return bit_depth > 8 ? (uint16_t)((uint16_t)sample[0] | ((uint16_t)sample[1] << 8)) : sample[0];
}
static inline void svt_av2_sample_store(uint8_t* sample, uint32_t bit_depth, uint16_t value) {
    sample[0] = (uint8_t)value;
    if (bit_depth > 8) sample[1] = (uint8_t)(value >> 8);
}
SvtAv2Error svt_av2_picture_alloc_ex(SvtAv2FrameBuffer* picture, uint32_t width,
                                   uint32_t height, uint32_t bit_depth,
                                   SvtAv2ChromaFormat chroma_format);
SvtAv2Error svt_av2_picture_view_ex(SvtAv2BorrowedView* out,
                                  const uint8_t* const planes[3],
                                  const uint32_t strides[3], uint32_t width,
                                  uint32_t height, uint32_t bit_depth,
                                  SvtAv2ChromaFormat chroma_format);
#endif /* SVT_AV2_PICTURE_H */
