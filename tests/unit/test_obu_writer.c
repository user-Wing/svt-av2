/*
 * T003 unit tests: OBU framing.
 *
 * Expected byte sequences are derived from AV2 v1.0.0 5.2.1/5.2.2/Annex B:
 *   leb128(num_bytes_in_obu) | obu_header(1 byte, ext flag 0) | payload
 * with num_bytes_in_obu = header_size + payload_size. The reader below is an
 * independent, minimal parser used to prove that the declared length matches
 * the bytes actually written.
 */
#include "obu_writer.h"

#include "test_util.h"

#include <stdint.h>
#include <string.h>

/* --- independent reader --------------------------------------------------- */

typedef struct {
    const uint8_t* data;
    size_t         size;
    size_t         pos;
} Reader;

static int reader_leb128(Reader* r, uint32_t* value, size_t* num_bytes) {
    uint32_t result = 0;
    size_t   count = 0;
    for (size_t i = 0; i < 8u && r->pos < r->size; ++i) {
        const uint8_t byte = r->data[r->pos++];
        result |= (uint32_t)(byte & 0x7Fu) << (i * 7u);
        ++count;
        if ((byte & 0x80u) == 0u) {
            *value = result;
            *num_bytes = count;
            return 0;
        }
    }
    return -1;
}

typedef struct {
    uint32_t       num_bytes_in_obu;
    size_t         size_field_size;
    SvtAv2ObuType  type;
    uint32_t       tlayer_id;
    const uint8_t* payload;
    size_t         payload_size;
} ParsedObu;

static int parse_obu(const uint8_t* data, size_t size, ParsedObu* out) {
    Reader r = { data, size, 0 };
    if (reader_leb128(&r, &out->num_bytes_in_obu, &out->size_field_size) != 0) {
        return -1;
    }
    if (r.pos + out->num_bytes_in_obu != size) {
        return -1; /* declared length must account for the whole OBU */
    }
    const uint8_t header = r.data[r.pos++];
    if ((header & 0x80u) != 0u) {
        return -1; /* obu_header_extension_flag must be 0 in the M1 subset */
    }
    out->type = (SvtAv2ObuType)((header >> 2) & 0x1Fu);
    out->tlayer_id = header & 0x03u;
    out->payload = r.data + r.pos;
    out->payload_size = out->num_bytes_in_obu - 1u;
    return 0;
}

/* --- tests ---------------------------------------------------------------- */

static void test_sequence_header_obu(void) {
    uint8_t          buffer[16];
    SvtAv2BitWriter  bw;
    const uint8_t    payload[] = { 0xAA, 0xBB };
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    SVT_TEST_CHECK_EQ_INT(
        svt_av2_obu_write(&bw, SVT_AV2_OBU_SEQUENCE_HEADER, 0u, payload, sizeof(payload)),
        SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), 4);

    const uint8_t expected[] = { 0x03, 0x04, 0xAA, 0xBB };
    SVT_TEST_CHECK(memcmp(buffer, expected, sizeof(expected)) == 0);

    ParsedObu parsed;
    SVT_TEST_CHECK_EQ_INT(parse_obu(buffer, 4, &parsed), 0);
    SVT_TEST_CHECK_EQ_INT(parsed.type, SVT_AV2_OBU_SEQUENCE_HEADER);
    SVT_TEST_CHECK_EQ_INT(parsed.tlayer_id, 0);
    SVT_TEST_CHECK_EQ_INT(parsed.num_bytes_in_obu, 3);
    SVT_TEST_CHECK_EQ_INT(parsed.payload_size, 2);
    SVT_TEST_CHECK(memcmp(parsed.payload, payload, 2) == 0);
}

static void test_temporal_delimiter_header_only(void) {
    uint8_t         buffer[8];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    SVT_TEST_CHECK_EQ_INT(
        svt_av2_obu_write(&bw, SVT_AV2_OBU_TEMPORAL_DELIMITER, 1u, NULL, 0u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), 2);

    const uint8_t expected[] = { 0x01, 0x09 };
    SVT_TEST_CHECK(memcmp(buffer, expected, sizeof(expected)) == 0);

    ParsedObu parsed;
    SVT_TEST_CHECK_EQ_INT(parse_obu(buffer, 2, &parsed), 0);
    SVT_TEST_CHECK_EQ_INT(parsed.type, SVT_AV2_OBU_TEMPORAL_DELIMITER);
    SVT_TEST_CHECK_EQ_INT(parsed.tlayer_id, 1);
    SVT_TEST_CHECK_EQ_INT(parsed.payload_size, 0);
}

static void test_tile_group_large_size(void) {
    uint8_t         buffer[256];
    uint8_t         payload[128];
    SvtAv2BitWriter bw;
    for (size_t i = 0; i < sizeof(payload); ++i) {
        payload[i] = (uint8_t)i;
    }
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    SVT_TEST_CHECK_EQ_INT(
        svt_av2_obu_write(&bw, SVT_AV2_OBU_REGULAR_TILE_GROUP, 2u, payload, sizeof(payload)),
        SVT_AV2_BW_OK);

    /* 2-byte leb128 (129) + 1-byte header + 128 payload = 131. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), 131);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0x81);
    SVT_TEST_CHECK_EQ_INT(buffer[1], 0x01);
    SVT_TEST_CHECK_EQ_INT(buffer[2], (int)((7u << 2) | 2u));

    ParsedObu parsed;
    SVT_TEST_CHECK_EQ_INT(parse_obu(buffer, 131, &parsed), 0);
    SVT_TEST_CHECK_EQ_INT(parsed.num_bytes_in_obu, 129);
    SVT_TEST_CHECK_EQ_INT(parsed.type, SVT_AV2_OBU_REGULAR_TILE_GROUP);
    SVT_TEST_CHECK_EQ_INT(parsed.payload_size, 128);
    SVT_TEST_CHECK(memcmp(parsed.payload, payload, sizeof(payload)) == 0);
}

static void test_padding_and_type_set(void) {
    uint8_t         buffer[8];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_type_is_supported(SVT_AV2_OBU_PADDING), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_type_is_supported(SVT_AV2_OBU_REGULAR_SEF), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_type_is_supported(SVT_AV2_OBU_CLOSED_LOOP_KEY), 1);
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_type_is_supported(SVT_AV2_OBU_FILM_GRAIN), 0);
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_type_is_supported(SVT_AV2_OBU_RESERVED_0), 0);

    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_write(&bw, SVT_AV2_OBU_PADDING, 0u, NULL, 0u), SVT_AV2_BW_OK);
    const uint8_t expected[] = { 0x01, 0x64 }; /* (25 << 2) = 0x64 */
    SVT_TEST_CHECK(memcmp(buffer, expected, sizeof(expected)) == 0);
}

static void test_rejections(void) {
    uint8_t         buffer[8];
    SvtAv2BitWriter bw;
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    /* Unsupported OBU type. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_write(&bw, SVT_AV2_OBU_FILM_GRAIN, 0u, NULL, 0u),
                          SVT_AV2_BW_ERROR_UNSUPPORTED);

    /* tlayer_id out of range. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_write(&bw, SVT_AV2_OBU_TEMPORAL_DELIMITER, 4u, NULL, 0u),
                          SVT_AV2_BW_ERROR_INVALID_ARG);

    /* Non-null size with null payload. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_write(&bw, SVT_AV2_OBU_SEQUENCE_HEADER, 0u, NULL, 2u),
                          SVT_AV2_BW_ERROR_NULL);

    /* Null writer. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_write(NULL, SVT_AV2_OBU_SEQUENCE_HEADER, 0u, NULL, 0u),
                          SVT_AV2_BW_ERROR_NULL);

    /* Insufficient capacity: 1-byte leb128 + header + 2 payload needs 4. */
    SvtAv2BitWriter small;
    uint8_t         small_buffer[2];
    svt_av2_bit_writer_init(&small, small_buffer, sizeof(small_buffer));
    const uint8_t payload[] = { 0x00, 0x00 };
    SVT_TEST_CHECK_EQ_INT(
        svt_av2_obu_write(&small, SVT_AV2_OBU_SEQUENCE_HEADER, 0u, payload, sizeof(payload)),
        SVT_AV2_BW_ERROR_NO_SPACE);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bits_written(&small), 0);
}

static void test_closed_loop_key_obu(void) {
    /* M1 single-frame path: first frame must be OBU_CLOSED_LOOP_KEY (type 4).
     * The CLK branch was contributed by 00 during integration; this test pins
     * its header byte and leb128 size prefix. */
    uint8_t         buffer[16];
    SvtAv2BitWriter bw;
    const uint8_t   payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    svt_av2_bit_writer_init(&bw, buffer, sizeof(buffer));

    SVT_TEST_CHECK_EQ_INT(
        svt_av2_obu_write(&bw, SVT_AV2_OBU_CLOSED_LOOP_KEY, 0u, payload, sizeof(payload)),
        SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_bytes_written(&bw), 6);
    SVT_TEST_CHECK_EQ_INT(buffer[0], 0x05); /* leb128(1 + 4) */
    SVT_TEST_CHECK_EQ_INT(buffer[1], 0x10); /* (4 << 2) | 0 = closed loop key, tlayer 0 */

    const uint8_t expected[] = { 0x05, 0x10, 0xDE, 0xAD, 0xBE, 0xEF };
    SVT_TEST_CHECK(memcmp(buffer, expected, sizeof(expected)) == 0);

    ParsedObu parsed;
    SVT_TEST_CHECK_EQ_INT(parse_obu(buffer, 6, &parsed), 0);
    SVT_TEST_CHECK_EQ_INT(parsed.type, SVT_AV2_OBU_CLOSED_LOOP_KEY);
    SVT_TEST_CHECK_EQ_INT(parsed.tlayer_id, 0);
    SVT_TEST_CHECK_EQ_INT(parsed.num_bytes_in_obu, 5);
    SVT_TEST_CHECK_EQ_INT(parsed.payload_size, 4);
    SVT_TEST_CHECK(memcmp(parsed.payload, payload, sizeof(payload)) == 0);
}

static void test_payload_finalize(void) {
    uint8_t         payload_buffer[8];
    SvtAv2BitWriter payload_writer;
    svt_av2_bit_writer_init(&payload_writer, payload_buffer, sizeof(payload_buffer));

    /* 3 payload bits 111 -> trailing 1 then 4 zeros: 1111 0000 = 0xF0. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_bit_writer_write_bits(&payload_writer, 0x7u, 3u), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_payload_finalize(&payload_writer), SVT_AV2_BW_OK);
    SVT_TEST_CHECK_EQ_INT(payload_buffer[0], 0xF0);

    uint8_t         stream[16];
    SvtAv2BitWriter stream_writer;
    svt_av2_bit_writer_init(&stream_writer, stream, sizeof(stream));
    SVT_TEST_CHECK_EQ_INT(svt_av2_obu_write(&stream_writer,
                                            SVT_AV2_OBU_SEQUENCE_HEADER,
                                            0u,
                                            payload_buffer,
                                            svt_av2_bit_writer_bytes_written(&payload_writer)),
                          SVT_AV2_BW_OK);

    const uint8_t expected[] = { 0x02, 0x04, 0xF0 };
    SVT_TEST_CHECK(memcmp(stream, expected, sizeof(expected)) == 0);

    ParsedObu parsed;
    SVT_TEST_CHECK_EQ_INT(parse_obu(stream, 3, &parsed), 0);
    SVT_TEST_CHECK_EQ_INT(parsed.payload_size, 1);
    SVT_TEST_CHECK_EQ_INT(parsed.payload[0], 0xF0);
}

int main(void) {
    test_sequence_header_obu();
    test_temporal_delimiter_header_only();
    test_tile_group_large_size();
    test_padding_and_type_set();
    test_rejections();
    test_closed_loop_key_obu();
    test_payload_finalize();

    SVT_TEST_REPORT("test_obu_writer");
}
