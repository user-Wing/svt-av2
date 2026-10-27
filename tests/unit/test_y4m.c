/*
 * T001 unit tests: Y4M header parsing.
 *
 * Covers the normal cases (geometry, frame rate, chroma format, bit depth,
 * defaults) and the rejection cases required by the task book: missing fields,
 * illegal numbers, truncated / oversized headers, illegal dimensions and
 * integer overflow. Unsupported bit depths / chroma formats must be rejected
 * explicitly, never silently downgraded to 4:2:0.
 */
#include "y4m.h"

#include "test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Writes "content" to an anonymous temp stream rewound to byte 0. */
static FILE* make_stream(const char* content) {
    FILE* stream = tmpfile();
    if (stream == NULL) {
        fprintf(stderr, "FAIL: tmpfile() returned NULL\n");
        return NULL;
    }
    const size_t length = strlen(content);
    if (length > 0 && fwrite(content, 1, length, stream) != length) {
        fclose(stream);
        return NULL;
    }
    rewind(stream);
    return stream;
}

static SvtAv2Error parse(const char* content, SvtAv2Y4mInfo* info) {
    FILE* stream = make_stream(content);
    if (stream == NULL) {
        return SVT_AV2_ERROR_INTERNAL;
    }
    const SvtAv2Error err = svt_av2_y4m_read_header(stream, info);
    fclose(stream);
    return err;
}

static void test_valid_headers(void) {
    SvtAv2Y4mInfo info;

    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W352 H288 F25:1 Ip A1:1 C420\n", &info), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(info.width, 352);
    SVT_TEST_CHECK_EQ_INT(info.height, 288);
    SVT_TEST_CHECK_EQ_INT(info.frame_rate_num, 25);
    SVT_TEST_CHECK_EQ_INT(info.frame_rate_den, 1);
    SVT_TEST_CHECK_EQ_INT(info.chroma_format, SVT_AV2_CHROMA_420);
    SVT_TEST_CHECK_EQ_INT(info.bit_depth, SVT_AV2_BIT_DEPTH_8);
    SVT_TEST_CHECK_EQ_INT(info.interlaced, 0);
    SVT_TEST_CHECK_EQ_STR(info.color_space, "420");

    /* Header only, no frame: still a valid header. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W16 H16 F30:1 C420\n", &info), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(info.width, 16);
    SVT_TEST_CHECK_EQ_INT(info.height, 16);

    /* A frame tag after the header is accepted and left in the stream. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 C420\nFRAME\n", &info), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(info.width, 8);

    /* Comment token is ignored. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 C420 Xcreated by test\n", &info), SVT_AV2_ERROR_NONE);

    /* 4:2:0 10-bit is recognised (not silently downgraded). */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 C420p10\n", &info), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(info.chroma_format, SVT_AV2_CHROMA_420);
    SVT_TEST_CHECK_EQ_INT(info.bit_depth, SVT_AV2_BIT_DEPTH_10);

    /* 4:4:4 and 4:2:2 are recognised and preserved. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 C444\n", &info), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(info.chroma_format, SVT_AV2_CHROMA_444);
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 C422\n", &info), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(info.chroma_format, SVT_AV2_CHROMA_422);
}

static void test_defaults(void) {
    SvtAv2Y4mInfo info;

    /* Missing F defaults to 25/1; missing C defaults to 8-bit 4:2:0. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W640 H480\n", &info), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(info.frame_rate_num, 25);
    SVT_TEST_CHECK_EQ_INT(info.frame_rate_den, 1);
    SVT_TEST_CHECK_EQ_INT(info.chroma_format, SVT_AV2_CHROMA_420);
    SVT_TEST_CHECK_EQ_INT(info.bit_depth, SVT_AV2_BIT_DEPTH_8);

    /* Interlace tag "p" is accepted and recorded as progressive. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W640 H480 Ip\n", &info), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(info.interlaced, 0);
}

static void test_rejections(void) {
    SvtAv2Y4mInfo info;

    /* Bad magic. */
    SVT_TEST_CHECK_EQ_INT(parse("NOTY4M W8 H8 C420\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    /* Empty input. */
    SVT_TEST_CHECK_EQ_INT(parse("", &info), SVT_AV2_ERROR_IO);
    /* Missing width / height. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 H8 C420\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 C420\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    /* Zero dimensions. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W0 H8 C420\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H0 C420\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    /* Non-numeric / negative dimensions. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 Wabc H8 C420\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W-8 H8 C420\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    /* 32-bit overflow in a dimension. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W4294967296 H8 C420\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    /* Malformed frame rate. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 F25\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 F0:1\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 F25:0\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    /* Unknown / unsupported colour space. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 C420p16\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 Cfoo\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    /* Unknown tag. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 Z9\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    /* Interlaced input is rejected. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 It\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 Ib\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
}

static void test_truncated_and_oversized(void) {
    SvtAv2Y4mInfo info;

    /* Header without a terminating newline: EOF while reading. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 C420", &info), SVT_AV2_ERROR_IO);

    /* A header longer than the internal buffer must be rejected, not overflow. */
    {
        const size_t big = SVT_AV2_Y4M_MAX_HEADER_LENGTH + 512;
        char*        buffer = (char*)malloc(big + 1);
        SVT_TEST_CHECK(buffer != NULL);
        if (buffer != NULL) {
            memcpy(buffer, "YUV4MPEG2 W8 H8 C420 X", 22);
            size_t written = 22;
            while (written < big) {
                buffer[written++] = 'A';
            }
            buffer[written] = '\0';
            SVT_TEST_CHECK_EQ_INT(parse(buffer, &info), SVT_AV2_ERROR_UNSUPPORTED);
            free(buffer);
        }
    }

    /* A frame tag that is not "FRAME" after a valid header is rejected. */
    SVT_TEST_CHECK_EQ_INT(parse("YUV4MPEG2 W8 H8 C420\nXXXXX\n", &info), SVT_AV2_ERROR_UNSUPPORTED);
}

static void test_color_space_mapping(void) {
    SvtAv2ChromaFormat chroma = SVT_AV2_CHROMA_400;
    SvtAv2BitDepth     depth = SVT_AV2_BIT_DEPTH_8;

    SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_parse_color_space("420", &chroma, &depth), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(chroma, SVT_AV2_CHROMA_420);
    SVT_TEST_CHECK_EQ_INT(depth, SVT_AV2_BIT_DEPTH_8);

    SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_parse_color_space("420p12", &chroma, &depth), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(depth, SVT_AV2_BIT_DEPTH_12);

    SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_parse_color_space("mono", &chroma, &depth), SVT_AV2_ERROR_NONE);
    SVT_TEST_CHECK_EQ_INT(chroma, SVT_AV2_CHROMA_400);

    SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_parse_color_space("nope", &chroma, &depth), SVT_AV2_ERROR_UNSUPPORTED);

    /* NULL arguments. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_parse_color_space(NULL, &chroma, &depth),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_parse_color_space("420", NULL, &depth),
                          SVT_AV2_ERROR_INVALID_PARAMETER);
}

static void test_read_from_path(void) {
    SvtAv2Y4mInfo info;
    const char*   path = "svt_av2_test_y4m_tmp.y4m";

    FILE* stream = fopen(path, "wb");
    SVT_TEST_CHECK(stream != NULL);
    if (stream != NULL) {
        const char* content = "YUV4MPEG2 W320 H240 F24:1 C420\n";
        SVT_TEST_CHECK(fwrite(content, 1, strlen(content), stream) == strlen(content));
        fclose(stream);

        SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_read_header_from_path(path, &info), SVT_AV2_ERROR_NONE);
        SVT_TEST_CHECK_EQ_INT(info.width, 320);
        SVT_TEST_CHECK_EQ_INT(info.height, 240);
        SVT_TEST_CHECK_EQ_INT(info.frame_rate_num, 24);
        remove(path);
    }

    /* Missing file. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_read_header_from_path("does_not_exist.y4m", &info), SVT_AV2_ERROR_IO);
    /* NULL arguments. */
    SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_read_header_from_path(NULL, &info), SVT_AV2_ERROR_INVALID_PARAMETER);
    SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_read_header_from_path("x.y4m", NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
}

static void test_read_header_null_args(void) {
    SvtAv2Y4mInfo info;
    SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_read_header(NULL, &info), SVT_AV2_ERROR_INVALID_PARAMETER);
    FILE* stream = make_stream("YUV4MPEG2 W8 H8 C420\n");
    if (stream != NULL) {
        SVT_TEST_CHECK_EQ_INT(svt_av2_y4m_read_header(stream, NULL), SVT_AV2_ERROR_INVALID_PARAMETER);
        fclose(stream);
    }
}

int main(void) {
    test_valid_headers();
    test_defaults();
    test_rejections();
    test_truncated_and_oversized();
    test_color_space_mapping();
    test_read_from_path();
    test_read_header_null_args();

    SVT_TEST_REPORT("test_y4m");
}
