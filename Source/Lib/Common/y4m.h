/*
 * SVT-AV2 Y4M container reader (metadata only in M0).
 *
 * M0 requirement: "空 encoder app 能读 Y4M 元信息". The reader therefore parses
 * the stream header and exposes geometry, frame rate, chroma format and bit
 * depth. It deliberately does not decode frame payloads yet.
 *
 * Supported header layout (per the YUV4MPEG2 informal specification):
 *
 *   YUV4MPEG2 W<width> H<height> F<num>:<den> I<interlace> A<par>:<den>
 *             C<colorspace> [X<comment>]\n
 *
 * Only progressive ("Ip") input is accepted. The following colour spaces are
 * recognised, everything else is rejected:
 *
 *   C420, C420jpeg, C420mpeg2, C420paldv   -> 8-bit 4:2:0
 *   C420p10, C420p12                       -> high bit depth 4:2:0
 *   C444, C444p10, C444p12                 -> 4:4:4
 *   C422, C422p10, C422p12                 -> 4:2:2
 *   Cmono, Cmono10, Cmono12                -> 4:0:0
 */
#ifndef SVT_AV2_Y4M_H
#define SVT_AV2_Y4M_H

#include <stdio.h>

#include "common.h"

#define SVT_AV2_Y4M_MAX_HEADER_LENGTH 1024

typedef struct SvtAv2Y4mInfo {
    uint32_t           width;
    uint32_t           height;
    uint32_t           frame_rate_num;
    uint32_t           frame_rate_den;
    SvtAv2ChromaFormat chroma_format;
    SvtAv2BitDepth     bit_depth;
    int                interlaced; /* always 0 in M0: interlaced input is rejected */
    uint32_t           header_length;
    char               color_space[16]; /* raw C<...> token, empty when absent */
} SvtAv2Y4mInfo;

/* Parses the YUV4MPEG2 header from an open stream positioned at byte 0.
 * On success the stream is positioned on the first frame tag ("FRAME"). */
SvtAv2Error svt_av2_y4m_read_header(FILE* stream, SvtAv2Y4mInfo* info);

/* Convenience wrapper that opens the file, parses the header and closes it. */
SvtAv2Error svt_av2_y4m_read_header_from_path(const char* path, SvtAv2Y4mInfo* info);

/* Maps a Y4M colour space token to chroma format / bit depth.
 * Returns SVT_AV2_ERROR_UNSUPPORTED for unknown tokens. */
SvtAv2Error svt_av2_y4m_parse_color_space(const char* token,
                                          SvtAv2ChromaFormat* chroma_format,
                                          SvtAv2BitDepth* bit_depth);

#endif /* SVT_AV2_Y4M_H */
