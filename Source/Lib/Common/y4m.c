/* SVT-AV2 Y4M stream header reader. */

#include "y4m.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define SVT_AV2_Y4M_MAGIC "YUV4MPEG2"

/* --- colour space table --------------------------------------------------- */

typedef struct SvtAv2Y4mColorSpace {
    const char*        token;
    SvtAv2ChromaFormat chroma_format;
    SvtAv2BitDepth     bit_depth;
} SvtAv2Y4mColorSpace;

static const SvtAv2Y4mColorSpace g_svt_av2_y4m_color_spaces[] = {
    {"420", SVT_AV2_CHROMA_420, SVT_AV2_BIT_DEPTH_8},
    {"420jpeg", SVT_AV2_CHROMA_420, SVT_AV2_BIT_DEPTH_8},
    {"420mpeg2", SVT_AV2_CHROMA_420, SVT_AV2_BIT_DEPTH_8},
    {"420paldv", SVT_AV2_CHROMA_420, SVT_AV2_BIT_DEPTH_8},
    {"420p10", SVT_AV2_CHROMA_420, SVT_AV2_BIT_DEPTH_10},
    {"420p12", SVT_AV2_CHROMA_420, SVT_AV2_BIT_DEPTH_12},
    {"422", SVT_AV2_CHROMA_422, SVT_AV2_BIT_DEPTH_8},
    {"422p10", SVT_AV2_CHROMA_422, SVT_AV2_BIT_DEPTH_10},
    {"422p12", SVT_AV2_CHROMA_422, SVT_AV2_BIT_DEPTH_12},
    {"444", SVT_AV2_CHROMA_444, SVT_AV2_BIT_DEPTH_8},
    {"444p10", SVT_AV2_CHROMA_444, SVT_AV2_BIT_DEPTH_10},
    {"444p12", SVT_AV2_CHROMA_444, SVT_AV2_BIT_DEPTH_12},
    {"mono", SVT_AV2_CHROMA_400, SVT_AV2_BIT_DEPTH_8},
    {"mono10", SVT_AV2_CHROMA_400, SVT_AV2_BIT_DEPTH_10},
    {"mono12", SVT_AV2_CHROMA_400, SVT_AV2_BIT_DEPTH_12},
};

SvtAv2Error svt_av2_y4m_parse_color_space(const char* token,
                                          SvtAv2ChromaFormat* chroma_format,
                                          SvtAv2BitDepth* bit_depth) {
    if (token == NULL || chroma_format == NULL || bit_depth == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    for (size_t i = 0; i < SVT_AV2_ARRAY_SIZE(g_svt_av2_y4m_color_spaces); ++i) {
        if (strcmp(token, g_svt_av2_y4m_color_spaces[i].token) == 0) {
            *chroma_format = g_svt_av2_y4m_color_spaces[i].chroma_format;
            *bit_depth = g_svt_av2_y4m_color_spaces[i].bit_depth;
            return SVT_AV2_ERROR_NONE;
        }
    }
    return SVT_AV2_ERROR_UNSUPPORTED;
}

/* --- header token parsing ------------------------------------------------- */

/* Parses an unsigned decimal integer token. Rejects empty input, non-digits and
 * overflow. Returns 0 on success. */
static int parse_uint32(const char* text, size_t length, uint32_t* out) {
    if (length == 0 || out == NULL) {
        return -1;
    }
    uint64_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        if (!isdigit((unsigned char)text[i])) {
            return -1;
        }
        value = value * 10u + (uint64_t)(text[i] - '0');
        if (value > 0xFFFFFFFFull) {
            return -1;
        }
    }
    *out = (uint32_t)value;
    return 0;
}

/* Parses "<num>:<den>". Returns 0 on success. */
static int parse_ratio(const char* text, size_t length, uint32_t* num, uint32_t* den) {
    size_t colon = (size_t)-1;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] == ':') {
            colon = i;
            break;
        }
    }
    if (colon == (size_t)-1) {
        return -1;
    }
    if (parse_uint32(text, colon, num) != 0) {
        return -1;
    }
    if (parse_uint32(text + colon + 1, length - colon - 1, den) != 0) {
        return -1;
    }
    return 0;
}

SvtAv2Error svt_av2_y4m_read_header(FILE* stream, SvtAv2Y4mInfo* info) {
    if (stream == NULL || info == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }

    memset(info, 0, sizeof(*info));

    char line[SVT_AV2_Y4M_MAX_HEADER_LENGTH];
    size_t length = 0;
    int    saw_newline = 0;

    /* Read the header byte by byte: a fixed-size fgets() would consume the
     * first frame payload bytes when the header is shorter than the buffer. */
    while (length < sizeof(line) - 1) {
        const int c = fgetc(stream);
        if (c == EOF) {
            return SVT_AV2_ERROR_IO;
        }
        if (c == '\n') {
            saw_newline = 1;
            break;
        }
        line[length++] = (char)c;
    }
    if (!saw_newline) {
        return SVT_AV2_ERROR_UNSUPPORTED; /* header too long or truncated */
    }
    line[length] = '\0';
    info->header_length = (uint32_t)(length + 1);

    const size_t magic_length = sizeof(SVT_AV2_Y4M_MAGIC) - 1;
    if (length <= magic_length || memcmp(line, SVT_AV2_Y4M_MAGIC, magic_length) != 0) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }

    int seen_width = 0;
    int seen_height = 0;
    int seen_frame_rate = 0;
    int seen_color_space = 0;
    int seen_interlace = 0;

    const char* cursor = line + magic_length;
    while (*cursor != '\0') {
        if (*cursor == ' ') {
            ++cursor;
            continue;
        }
        const char tag = *cursor++;
        const char* token_start = cursor;
        while (*cursor != '\0' && *cursor != ' ') {
            ++cursor;
        }
        const size_t token_length = (size_t)(cursor - token_start);
        if (token_length == 0) {
            /* Trailing or doubled space: tolerated, but a bare tag is not. */
            continue;
        }

        switch (tag) {
        case 'W':
            if (parse_uint32(token_start, token_length, &info->width) != 0) {
                return SVT_AV2_ERROR_UNSUPPORTED;
            }
            seen_width = 1;
            break;
        case 'H':
            if (parse_uint32(token_start, token_length, &info->height) != 0) {
                return SVT_AV2_ERROR_UNSUPPORTED;
            }
            seen_height = 1;
            break;
        case 'F':
            if (parse_ratio(token_start, token_length, &info->frame_rate_num, &info->frame_rate_den) != 0) {
                return SVT_AV2_ERROR_UNSUPPORTED;
            }
            seen_frame_rate = 1;
            break;
        case 'I':
            if (token_length != 1) {
                return SVT_AV2_ERROR_UNSUPPORTED;
            }
            if (token_start[0] == 'p') {
                info->interlaced = 0;
            } else if (token_start[0] == 't' || token_start[0] == 'b' || token_start[0] == 'm') {
                return SVT_AV2_ERROR_UNSUPPORTED; /* interlaced input not supported */
            } else {
                return SVT_AV2_ERROR_UNSUPPORTED;
            }
            seen_interlace = 1;
            break;
        case 'A': {
            uint32_t par_num = 0;
            uint32_t par_den = 0;
            if (parse_ratio(token_start, token_length, &par_num, &par_den) != 0) {
                return SVT_AV2_ERROR_UNSUPPORTED;
            }
            break;
        }
        case 'C': {
            if (token_length >= sizeof(info->color_space)) {
                return SVT_AV2_ERROR_UNSUPPORTED;
            }
            memcpy(info->color_space, token_start, token_length);
            info->color_space[token_length] = '\0';
            SvtAv2Error err = svt_av2_y4m_parse_color_space(info->color_space, &info->chroma_format, &info->bit_depth);
            if (err != SVT_AV2_ERROR_NONE) {
                return err;
            }
            seen_color_space = 1;
            break;
        }
        case 'X':
            /* Free-form comment: it runs to the end of the line, so stop
             * parsing tags entirely. Treating it as a single token would make
             * the following comment words look like unknown tags. */
            cursor = line + length;
            break;
        default:
            return SVT_AV2_ERROR_UNSUPPORTED;
        }
    }

    if (!seen_width || !seen_height) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }
    if (info->width == 0 || info->height == 0) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }
    if (!seen_frame_rate) {
        /* Missing frame rate defaults to 25/1, matching the informal spec. */
        info->frame_rate_num = 25;
        info->frame_rate_den = 1;
    }
    if (info->frame_rate_num == 0 || info->frame_rate_den == 0) {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }
    if (!seen_color_space) {
        /* The informal spec defaults a missing C token to 4:2:0 8-bit. */
        info->chroma_format = SVT_AV2_CHROMA_420;
        info->bit_depth = SVT_AV2_BIT_DEPTH_8;
        memcpy(info->color_space, "420", 3);
        info->color_space[3] = '\0';
    }
    (void)seen_interlace;

    /* Verify that the next token really is a frame tag so that callers can rely
     * on the stream position. A header-only file (no frames) is valid. */
    int first = fgetc(stream);
    if (first == EOF) {
        return SVT_AV2_ERROR_NONE;
    }
    if (first != 'F') {
        return SVT_AV2_ERROR_UNSUPPORTED;
    }
    if (fseek(stream, -1, SEEK_CUR) != 0) {
        return SVT_AV2_ERROR_IO;
    }
    return SVT_AV2_ERROR_NONE;
}

SvtAv2Error svt_av2_y4m_read_header_from_path(const char* path, SvtAv2Y4mInfo* info) {
    if (path == NULL || info == NULL) {
        return SVT_AV2_ERROR_INVALID_PARAMETER;
    }
    FILE* stream = fopen(path, "rb");
    if (stream == NULL) {
        return SVT_AV2_ERROR_IO;
    }
    const SvtAv2Error err = svt_av2_y4m_read_header(stream, info);
    fclose(stream);
    return err;
}
