/*
 * SVT-AV2 - OBU framing (T003).
 *
 * Scope: M1 minimal OBU header + Annex B length-delimited framing. Only the
 * OBU types required by the M1 single-frame path are enabled; every other type
 * is rejected with UNSUPPORTED so that an accidental unsupported stream fails
 * loudly instead of producing a half-valid bitstream.
 *
 * Spec references (AV2 v1.0.0):
 *   - 5.2.1 open_bitstream_unit(sz): OBU is [obu_header][payload]; the size
 *     value counts the header and payload but not the size field itself.
 *   - 5.2.2 obu_header(): obu_header_extension_flag f(1), obu_type f(5),
 *     obu_tlayer_id f(2) [, obu_mlayer_id f(3), obu_xlayer_id f(5)].
 *   - 5.2.3 trailing_bits(nbBits).
 *   - 6.2.1: obu_extension_flag must be 0 in conforming bitstreams.
 *   - Table 6.1: OBU type values.
 *   - Annex B.2 bitstream(): leb128() num_bytes_in_obu; open_bitstream_unit().
 *
 * Contract (frozen by T003; see docs/m0-m1-contracts.md section 5):
 *   - The bitstream is a sequence of Annex B length-delimited OBUs:
 *         leb128( num_bytes_in_obu ) | obu_header | obu_payload
 *     where num_bytes_in_obu = obu_header_size + obu_payload_size.
 *   - The OBU writer only adds the leb128 length field and the OBU header.
 *     The payload (including its own trailing bits) is built by the caller.
 *   - Extension headers are out of the M1 subset and rejected.
 */
#ifndef SVT_AV2_OBU_WRITER_H
#define SVT_AV2_OBU_WRITER_H

#include <stddef.h>
#include <stdint.h>

#include "bit_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OBU type values (AV2 v1.0.0 Table 6.1). */
typedef enum SvtAv2ObuType {
    SVT_AV2_OBU_RESERVED_0 = 0,
    SVT_AV2_OBU_SEQUENCE_HEADER = 1,
    SVT_AV2_OBU_TEMPORAL_DELIMITER = 2,
    SVT_AV2_OBU_MULTI_FRAME_HEADER = 3,
    SVT_AV2_OBU_CLOSED_LOOP_KEY = 4,
    SVT_AV2_OBU_OPEN_LOOP_KEY = 5,
    SVT_AV2_OBU_LEADING_TILE_GROUP = 6,
    SVT_AV2_OBU_REGULAR_TILE_GROUP = 7,
    SVT_AV2_OBU_METADATA_SHORT = 8,
    SVT_AV2_OBU_METADATA_GROUP = 9,
    SVT_AV2_OBU_SWITCH = 10,
    SVT_AV2_OBU_LEADING_SEF = 11,
    SVT_AV2_OBU_REGULAR_SEF = 12,
    SVT_AV2_OBU_LEADING_TIP = 13,
    SVT_AV2_OBU_REGULAR_TIP = 14,
    SVT_AV2_OBU_BUFFER_REMOVAL_TIMING = 15,
    SVT_AV2_OBU_LAYER_CONFIGURATION_RECORD = 16,
    SVT_AV2_OBU_ATLAS_SEGMENT = 17,
    SVT_AV2_OBU_OPERATING_POINT_SET = 18,
    SVT_AV2_OBU_BRIDGE_FRAME = 19,
    SVT_AV2_OBU_MSDO = 20,
    SVT_AV2_OBU_RAS_FRAME = 21,
    SVT_AV2_OBU_QUANTIZATION_MATRIX = 22,
    SVT_AV2_OBU_FILM_GRAIN = 23,
    SVT_AV2_OBU_CONTENT_INTERPRETATION = 24,
    SVT_AV2_OBU_PADDING = 25
} SvtAv2ObuType;

/* The M1 subset this writer accepts. Everything else returns UNSUPPORTED. */

/* True when type is in the M1 subset accepted by svt_av2_obu_write(). */
int svt_av2_obu_type_is_supported(SvtAv2ObuType type);

/* Number of bytes obu_header() uses for type/tlayer_id with no extension:
 * always 1 in the M1 subset (obu_header_extension_flag == 0). */
size_t svt_av2_obu_header_size(SvtAv2ObuType type, uint32_t tlayer_id);

/* Appends one OBU to bw: leb128(size) + header + payload. payload may be NULL
 * only when payload_size is 0. The caller owns bw's buffer.
 *
 * Errors: NULL pointer, unsupported type, tlayer_id > 3, ext_flag != 0, a
 * payload size whose total exceeds 2^32-1, or insufficient buffer space. */
int svt_av2_obu_write(SvtAv2BitWriter* bw,
                      SvtAv2ObuType    type,
                      uint32_t         tlayer_id,
                      const uint8_t*   payload,
                      size_t           payload_size);

/* Completes an OBU payload written with the BitWriter: appends the trailing
 * bits and finalizes. Used by T004/T005 before svt_av2_obu_write(). */
int svt_av2_obu_payload_finalize(SvtAv2BitWriter* bw);

#ifdef __cplusplus
}
#endif

#endif /* SVT_AV2_OBU_WRITER_H */
