/*
 * SVT-AV2 - OBU framing (T003). See obu_writer.h for the frozen contract.
 */
#include "obu_writer.h"

int svt_av2_obu_type_is_supported(SvtAv2ObuType type) {
    switch (type) {
    case SVT_AV2_OBU_SEQUENCE_HEADER:
    case SVT_AV2_OBU_TEMPORAL_DELIMITER:
    case SVT_AV2_OBU_CLOSED_LOOP_KEY:
    case SVT_AV2_OBU_REGULAR_TILE_GROUP:
    case SVT_AV2_OBU_REGULAR_SEF:
    case SVT_AV2_OBU_PADDING:
        return 1;
    default:
        return 0;
    }
}

size_t svt_av2_obu_header_size(SvtAv2ObuType type, uint32_t tlayer_id) {
    /* M1 always uses obu_header_extension_flag == 0 (spec 6.2.1), so the
     * header is a single byte regardless of the arguments. */
    (void)type;
    (void)tlayer_id;
    return 1u;
}

int svt_av2_obu_write(SvtAv2BitWriter* bw,
                      SvtAv2ObuType    type,
                      uint32_t         tlayer_id,
                      const uint8_t*   payload,
                      size_t           payload_size) {
    if (bw == NULL) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (payload == NULL && payload_size != 0u) {
        return SVT_AV2_BW_ERROR_NULL;
    }
    if (!svt_av2_obu_type_is_supported(type)) {
        return SVT_AV2_BW_ERROR_UNSUPPORTED;
    }
    if (tlayer_id > 3u) {
        return SVT_AV2_BW_ERROR_INVALID_ARG;
    }

    const size_t header_size = svt_av2_obu_header_size(type, tlayer_id);
    if (payload_size > (size_t)SVT_AV2_BIT_WRITER_MAX_LEB128_VALUE - header_size) {
        return SVT_AV2_BW_ERROR_RANGE;
    }
    const uint32_t num_bytes_in_obu = (uint32_t)(header_size + payload_size);

    /* Atomic write: verify the whole OBU fits before committing anything, so a
     * NO_SPACE failure leaves the writer byte-for-byte unchanged. */
    if ((bw->bit_count & 7u) != 0u) {
        return SVT_AV2_BW_ERROR_UNALIGNED;
    }
    const size_t total_bytes = svt_av2_leb128_size(num_bytes_in_obu) + header_size + payload_size;
    if (total_bytes > bw->capacity - (bw->bit_count >> 3)) {
        return SVT_AV2_BW_ERROR_NO_SPACE;
    }

    /* obu_header(): obu_header_extension_flag(1) = 0, obu_type(5),
     * obu_tlayer_id(2). MSB-first => ext flag is bit 7. */
    const uint8_t header_byte = (uint8_t)(((uint32_t)type << 2) | (tlayer_id & 3u));

    int status = svt_av2_bit_writer_write_leb128(bw, num_bytes_in_obu);
    if (status != SVT_AV2_BW_OK) {
        return status;
    }
    status = svt_av2_bit_writer_write_byte(bw, header_byte);
    if (status != SVT_AV2_BW_OK) {
        return status;
    }
    return svt_av2_bit_writer_write_bytes(bw, payload, payload_size);
}

int svt_av2_obu_payload_finalize(SvtAv2BitWriter* bw) {
    int status = svt_av2_bit_writer_write_trailing_bits_align(bw);
    if (status != SVT_AV2_BW_OK) {
        return status;
    }
    return svt_av2_bit_writer_finalize(bw);
}
