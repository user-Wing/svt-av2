/*
 * SvtAv2EncApp - command line front end.
 *
 * Responsibilities:
 *   * read Y4M stream metadata and report it
 *   * drive the public encoder API through a full call sequence
 *
 * Encode path (M1, T013): the app is a pure client of the public API. For each
 * frame it fills a SvtAv2Picture view over the Y4M frame buffer, calls
 * svt_av2_enc_send_picture(), then takes the coded bytes with
 * svt_av2_enc_get_packet_ex() and writes them out. It does NOT call the internal
 * keyframe entry point and does not assemble OBUs (contract section 7: the CLI
 * and the library share one encode path, so a stream written by the app is
 * exactly what the library produces).
 *
 * Unsupported input exits 14; an encoder failure exits non-zero.
 *
 * The M1 encoder is ALL-INTRA/DC_PRED with zero residual: the input pixels are
 * accepted for API stability but not yet coded, so the reconstruction is uniform
 * mid-grey (128). This is honest documented behaviour, not a silent shortcut.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "svt_av2.h"
#include "y4m.h"

/* M4: single-threaded 8-bit420 / 10-bit420,444, DC prediction, fixed user QP. */
#define SVT_AV2_M1_DEFAULT_QP 100
#define SVT_AV2_APP_PACKET_INITIAL_BYTES 4096u
#define SVT_AV2_APP_EXIT_UNSUPPORTED_INPUT 8

#include <inttypes.h>

typedef struct AppOptions {
    const char* input_path;
    const char* output_path;
    const char* recon_path; /* NULL = do not export the reconstruction */
    int32_t     qp;
    int32_t     preset;
    int32_t     threads;
    int32_t     frames; /* -1 = all frames in the source */
    int32_t     deterministic;
    SvtAv2PredictionStructure prediction_structure;
    uint32_t intra_mode_selection;
    uint32_t partition_selection;
    uint32_t transform_selection;
    uint32_t angle_search;
    uint32_t mrl_search;
    uint32_t tree_refinement;
    uint32_t rd_lambda_model;
    uint32_t cfl_search;
    uint32_t mhccp_search;
    uint32_t intra_joint;
    uint32_t rd_mode;
    uint32_t coeff_optimization;
    uint32_t motion_optimization;
    uint32_t entropy_adaptation;
    uint32_t still_picture;
    SvtAv2ContentType content_type; /* NONE unless supplied explicitly */
    int         verbose;
} AppOptions;

static void app_options_set_defaults(AppOptions* options) {
    memset(options, 0, sizeof(*options));
    options->qp = SVT_AV2_M1_DEFAULT_QP;
    options->preset = SVT_AV2_PRESET_SLOW;
    options->threads = 1;
    options->frames = -1;
    options->deterministic = 1;
}

static void print_usage(const char* program) {
    printf("SvtAv2EncApp - SVT-AV2 reference front end\n"
           "\n"
           "Usage:\n"
           "  %s --input <file.y4m> [options]\n"
           "\n"
           "Options:\n"
           "  --input <path>        Y4M input file (required)\n"
           "  --output <path>       Encode to <path> (M4: 8-bit 420 / 10-bit 420,444).\n"
           "                        Omit to only report input metadata.\n"
           "  --frames <n>          Encode n frames: 0 = none.\n"
           "  --still-picture       Single-picture header; encode exactly one frame\n"
           "                        Default: all available frames.\n"
           "  --export-recon <path> Also write the encoder's own reconstruction\n"
           "                        (tight planar raw; 10-bit uses u16le, one frame after\n"
           "                        another) to <path>. Additive output: it does\n"
           "                        not change the encoded stream.\n"
           "  --pred-structure <all-intra|low-delay-p> Prediction structure (default: all-intra)\n"
           "  --intra-select <dc|adaptive|basic|directional|cfl>  extended single-picture Y/UV RD\n"
           "  --partition-select <fixed|adaptive|fine|mixed|rect|longrect|mixedrect|mixed8|thinrect|mixedthin|thin64>  fine=64/32/16; mixed adds 8 and quadrant splits; rect adds 16x8/8x16; longrect adds six larger rectangles; mixedrect adds mixed rectangular quadrants; mixed8 adds mixed small rectangles; thinrect adds uniform 8x32/32x8; mixedthin adds mixed thin quadrants; thin64 adds uniform 8x64/64x8 (still + basic/directional + RD)\n"
           "  --rd-mode <off|exhaustive|pruned|fast>  M6 bounded RD (default: off)\n"
           "  --tree-refine <off|rd|wide>  rd=best two trees; wide preserves seeds before long rectangles; thinrect/mixedthin also preserve pre-thin seeds with up to eight finalists; thin64 preserves those eight plus both new shapes (still + fine or later + RD)\n"
           "  --rd-lambda <original|avm-ai|avm-ai150>  Experimental intra RD lambda model (still + RD; default original)\n"
           "  --content-type <animation|real> Externally supplied content hint (default: none)\n"
           "                        Animation guard: compiled on + 10-bit 420 still + 1920x1080/3840x2160\n"
           "  --cfl-search <off|rd>  Also search classic explicit UV alpha (still + cfl + RD)\n"
           "  --mhccp-search <off|rd>  Also search causal MHCCP UV prediction (still + cfl + RD)\n"
           "  --intra-joint <off|rd>  Bounded Y mode/TX joint search (still + directional/cfl + RD + full TX; default off)\n"
           "  --mrl-search <off|rd>  Bounded square/rectangle Y multiple reference lines (still + directional/CfL + RD)\n"
           "  --angle-search <off|rd>  Bounded nonzero Y angles (still + directional/CfL + RD)\n"
           "  --transform-select <dct|primary|full>  Y 4/8/16 DCT/ADST subset; full also supports 8x16/16x8 (still + basic/directional + RD)\n"
           "  fast: still intra directional/cfl, partition <= mixed8, joint off\n"
           "  --coeff-opt <off|rd|rdoq>  rdoq=bounded level/EOB search (still + RD)\n"
           "  --motion-opt <off|rd|wide> Integer MV RD (requires RD, excludes coefficient RD)\n"
           "  --entropy-adapt <off|on> Frame-local CDF updates (requires RD off)\n"
           "  --qp <1..255>         CQP (10-bit actual qindex=QP+48) (default: 100)\n"
           "  --preset <0|4|8|12>   Speed preset placeholder (default: 4)\n"
           "  --threads <n>         Thread count; M4 only supports 1 (default: 1)\n"
           "  --deterministic       Enable deterministic mode (default)\n"
           "  --no-deterministic    Disable deterministic mode\n"
           "  --verbose             Print per frame progress\n"
           "  --version             Print build information and exit\n"
           "  --help                Print this message and exit\n"
           "\n"
           "With --output the app writes an AV2 elementary stream for the supported\n"
           "M4 formats: 8-bit 420, 10-bit 420 and 10-bit 444. All-Intra is the default.\n"
           "LOW_DELAY_P uses one previously reconstructed frame. --export-recon additionally writes the encoder's own\n"
           "reconstruction so a decoder run can be compared against it.\n",
           program);
}

static void print_version(void) {
    const SvtAv2EncoderInfo* info = svt_av2_enc_get_build_info();
    const SvtAv2BuildInfo*   build = svt_av2_get_build_info();
    printf("SvtAv2EncApp %s (milestone %s)\n", info->version_string, info->milestone);
    printf("  build type : %s\n", info->build_type);
    printf("  compiler   : %s\n", info->compiler);
    printf("  avm oracle : %s\n", (build->oracle_dir[0] != '\0') ? build->oracle_dir : "(not configured)");
}

static int parse_int32(const char* text, int32_t* out) {
    if (text == NULL || *text == '\0' || out == NULL) {
        return -1;
    }
    char*              end = NULL;
    const long int     value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < -2147483647L - 1L || value > 2147483647L) {
        return -1;
    }
    *out = (int32_t)value;
    return 0;
}

/* Returns 0 on success, 1 when the program should exit successfully (help /
 * version), -1 on argument error. */
static int app_parse_args(int argc, char** argv, AppOptions* options) {
    app_options_set_defaults(options);

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        const int   has_next = (i + 1) < argc;

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            print_usage(argv[0]);
            return 1;
        }
        if (strcmp(arg, "--version") == 0) {
            print_version();
            return 1;
        }
        if (strcmp(arg, "--verbose") == 0) {
            options->verbose = 1;
            continue;
        }
        if (strcmp(arg, "--deterministic") == 0) {
            options->deterministic = 1;
            continue;
        }
        if (strcmp(arg, "--no-deterministic") == 0) {
            options->deterministic = 0;
            continue;
        }

        const char* value = has_next ? argv[i + 1] : NULL;

        if (strcmp(arg, "--input") == 0 || strcmp(arg, "-i") == 0) {
            if (!has_next) {
                fprintf(stderr, "error: %s requires a value\n", arg);
                return -1;
            }
            options->input_path = value;
            ++i;
            continue;
        }
        if (strcmp(arg, "--output") == 0 || strcmp(arg, "-o") == 0) {
            if (!has_next) {
                fprintf(stderr, "error: %s requires a value\n", arg);
                return -1;
            }
            options->output_path = value;
            ++i;
            continue;
        }
        if (strcmp(arg, "--still-picture") == 0) {
            options->still_picture = 1u;
            continue;
        }
        if (strcmp(arg, "--frames") == 0) {
            if (!has_next || parse_int32(value, &options->frames) != 0 || options->frames < 0) {
                fprintf(stderr, "error: --frames requires a non-negative integer\n");
                return -1;
            }
            ++i;
            continue;
        }
        if (strcmp(arg, "--export-recon") == 0) {
            if (!has_next) {
                fprintf(stderr, "error: %s requires a value\n", arg);
                return -1;
            }
            options->recon_path = value;
            ++i;
            continue;
        }
        if (strcmp(arg, "--pred-structure") == 0) {
            if (!has_next || (strcmp(value, "all-intra") != 0 && strcmp(value, "low-delay-p") != 0)) {
                fprintf(stderr, "error: --pred-structure requires all-intra or low-delay-p\n");
                return -1;
            }
            options->prediction_structure = strcmp(value, "all-intra") == 0 ? SVT_AV2_PRED_ALL_INTRA : SVT_AV2_PRED_LOW_DELAY_P;
            ++i;
            continue;
        }
        if (strcmp(arg, "--rd-mode") == 0) {
            if (!has_next || (strcmp(value, "off") != 0 && strcmp(value, "exhaustive") != 0 && strcmp(value, "pruned") != 0 && strcmp(value, "fast") != 0)) {
                fprintf(stderr, "error: --rd-mode requires off, exhaustive, pruned or fast\n");
                return -1;
            }
            options->rd_mode = strcmp(value, "off") == 0 ? 0u : (strcmp(value, "exhaustive") == 0 ? 1u : strcmp(value, "pruned") == 0 ? 2u : 3u);
            ++i;
            continue;
        }
        if (strcmp(arg, "--coeff-opt") == 0) {
            if (!has_next || (strcmp(value, "off") != 0 && strcmp(value, "rd") != 0 && strcmp(value, "rdoq") != 0)) {
                fprintf(stderr, "error: --coeff-opt requires off, rd or rdoq\n");
                return -1;
            }
            options->coeff_optimization = strcmp(value, "rdoq") == 0 ? 2u : (uint32_t)(strcmp(value, "rd") == 0);
            ++i;
            continue;
        }
        if (strcmp(arg, "--entropy-adapt") == 0) {
            if (!has_next || (strcmp(value, "off") != 0 && strcmp(value, "on") != 0)) {
                fprintf(stderr, "error: --entropy-adapt requires off or on\n");
                return -1;
            }
            options->entropy_adaptation = strcmp(value, "on") == 0;
            ++i;
            continue;
        }
        if (strcmp(arg, "--motion-opt") == 0) {
            if (!has_next || (strcmp(value, "off") != 0 && strcmp(value, "rd") != 0 && strcmp(value, "wide") != 0)) {
                fprintf(stderr, "error: --motion-opt requires off, rd or wide\n");
                return -1;
            }
            options->motion_optimization = strcmp(value, "off") == 0 ? 0u : (strcmp(value, "rd") == 0 ? 1u : 2u);
            ++i;
            continue;
        }
        if (strcmp(arg, "--angle-search") == 0) {
            if (!has_next || (strcmp(value, "off") != 0 && strcmp(value, "rd") != 0)) {
                fprintf(stderr, "error: --angle-search requires off or rd\n");
                return -1;
            }
            options->angle_search = (uint32_t)(strcmp(value, "rd") == 0);
            ++i; continue;
        }
        if (strcmp(arg, "--tree-refine") == 0) {
            if (!has_next || (strcmp(value, "off") != 0 && strcmp(value, "rd") != 0 && strcmp(value, "wide") != 0)) {
                fprintf(stderr, "error: --tree-refine requires off, rd or wide\n");
                return -1;
            }
            options->tree_refinement = strcmp(value, "wide") == 0 ? 2u : (uint32_t)(strcmp(value, "rd") == 0);
            ++i; continue;
        }
        if (strcmp(arg, "--content-type") == 0) {
            if (!has_next || (strcmp(value,"animation")!=0 && strcmp(value,"real")!=0)) {
                fprintf(stderr,"error: --content-type requires animation or real\n");
                return -1;
            }
            options->content_type=strcmp(value,"animation")==0 ?
                SVT_AV2_CONTENT_ANIMATION : SVT_AV2_CONTENT_REAL;
            ++i;continue;
        }
        if (strcmp(arg, "--rd-lambda") == 0) {
            if (!has_next || (strcmp(value, "original") != 0 && strcmp(value, "avm-ai") != 0 && strcmp(value, "avm-ai150") != 0)) {
                fprintf(stderr, "error: --rd-lambda requires original, avm-ai or avm-ai150\n");
                return -1;
            }
            options->rd_lambda_model = strcmp(value, "avm-ai150") == 0 ? 2u : (uint32_t)(strcmp(value, "avm-ai") == 0);
            ++i; continue;
        }
        if (strcmp(arg, "--mrl-search") == 0) {
            if (!has_next || (strcmp(value, "off") != 0 && strcmp(value, "rd") != 0)) {
                fprintf(stderr, "error: --mrl-search requires off or rd\n");
                return -1;
            }
            options->mrl_search = (uint32_t)(strcmp(value, "rd") == 0);
            ++i; continue;
        }
        if (strcmp(arg, "--cfl-search") == 0) {
            if (!has_next || (strcmp(value, "off") != 0 && strcmp(value, "rd") != 0)) {
                fprintf(stderr, "error: --cfl-search requires off or rd\n");
                return -1;
            }
            options->cfl_search = (uint32_t)(strcmp(value, "rd") == 0);
            ++i; continue;
        }
        if (strcmp(arg, "--mhccp-search") == 0) {
            if (!has_next || (strcmp(value, "off") != 0 && strcmp(value, "rd") != 0)) {
                fprintf(stderr, "error: --mhccp-search requires off or rd\n");
                return -1;
            }
            options->mhccp_search = (uint32_t)(strcmp(value, "rd") == 0);
            ++i; continue;
        }
        if (strcmp(arg, "--intra-joint") == 0) {
            if (!has_next || (strcmp(value, "off") != 0 && strcmp(value, "rd") != 0)) {
                fprintf(stderr, "error: --intra-joint requires off or rd\n");
                return -1;
            }
            options->intra_joint = (uint32_t)(strcmp(value, "rd") == 0);
            ++i; continue;
        }
        if (strcmp(arg, "--transform-select") == 0) {
            if (!has_next || (strcmp(value, "dct") != 0 && strcmp(value, "primary") != 0 && strcmp(value, "full") != 0)) {
                fprintf(stderr, "error: --transform-select requires dct, primary or full\n");
                return -1;
            }
            options->transform_selection = strcmp(value, "full") == 0 ? 2u : (uint32_t)(strcmp(value, "primary") == 0);
            ++i; continue;
        }
        if (strcmp(arg, "--partition-select") == 0) {
            if (!has_next || (strcmp(value, "fixed") != 0 && strcmp(value, "adaptive") != 0 && strcmp(value, "fine") != 0 && strcmp(value, "mixed") != 0 && strcmp(value, "rect") != 0 && strcmp(value, "longrect") != 0 && strcmp(value, "mixedrect") != 0 && strcmp(value, "mixed8") != 0 && strcmp(value, "thinrect") != 0 && strcmp(value, "mixedthin") != 0 && strcmp(value, "thin64") != 0)) {
                fprintf(stderr, "error: --partition-select requires fixed, adaptive, fine, mixed, rect, longrect, mixedrect, mixed8, thinrect, mixedthin or thin64\n");
                return -1;
            }
            options->partition_selection = strcmp(value, "thin64") == 0 ? 10u : strcmp(value, "mixedthin") == 0 ? 9u : strcmp(value, "thinrect") == 0 ? 8u : strcmp(value, "mixed8") == 0 ? 7u : strcmp(value, "mixedrect") == 0 ? 6u : strcmp(value, "longrect") == 0 ? 5u : strcmp(value, "rect") == 0 ? 4u : strcmp(value, "mixed") == 0 ? 3u :
                strcmp(value, "fine") == 0 ? 2u : (uint32_t)(strcmp(value, "adaptive") == 0);
            ++i;
            continue;
        }
        if (strcmp(arg, "--intra-select") == 0) {
            if (!has_next || (strcmp(value, "dc") != 0 && strcmp(value, "adaptive") != 0 && strcmp(value, "basic") != 0 && strcmp(value, "directional") != 0 && strcmp(value, "cfl") != 0)) {
                fprintf(stderr, "error: --intra-select requires dc, adaptive, basic, directional or cfl\n");
                return -1;
            }
            options->intra_mode_selection = strcmp(value, "cfl") == 0 ? 4u : strcmp(value, "directional") == 0 ? 3u : strcmp(value, "basic") == 0 ? 2u : (uint32_t)(strcmp(value, "adaptive") == 0);
            ++i;
            continue;
        }
        if (strcmp(arg, "--qp") == 0) {
            if (!has_next || parse_int32(value, &options->qp) != 0 || options->qp < 1 || options->qp > 255) {
                fprintf(stderr, "error: --qp requires an integer in [1, 255]\n");
                return -1;
            }
            ++i;
            continue;
        }
        if (strcmp(arg, "--preset") == 0) {
            if (!has_next || parse_int32(value, &options->preset) != 0) {
                fprintf(stderr, "error: --preset requires an integer\n");
                return -1;
            }
            ++i;
            continue;
        }
        if (strcmp(arg, "--threads") == 0) {
            if (!has_next || parse_int32(value, &options->threads) != 0 || options->threads < 1) {
                fprintf(stderr, "error: --threads requires a positive integer\n");
                return -1;
            }
            ++i;
            continue;
        }

        fprintf(stderr, "error: unknown argument '%s'\n", arg);
        return -1;
    }

    if (options->input_path == NULL) {
        fprintf(stderr, "error: --input is required\n");
        return -1;
    }
    if (options->still_picture && (options->frames == 0 || options->frames > 1 ||
        options->prediction_structure != SVT_AV2_PRED_ALL_INTRA)) {
        fprintf(stderr, "error: --still-picture requires one frame and all-intra\n");
        return -1;
    }
    return 0;
}

static void app_print_y4m_info(const char* path, const SvtAv2Y4mInfo* info) {
    printf("input       : %s\n", path);
    printf("resolution  : %ux%u\n", info->width, info->height);
    printf("frame rate  : %u/%u (%.4f fps)\n",
           info->frame_rate_num,
           info->frame_rate_den,
           (double)info->frame_rate_num / (double)info->frame_rate_den);
    printf("chroma      : %s\n", svt_av2_chroma_format_name(info->chroma_format));
    printf("bit depth   : %u\n", (unsigned)info->bit_depth);
    printf("colorspace  : %s\n", (info->color_space[0] != '\0') ? info->color_space : "(default)");
    printf("interlace   : %s\n", info->interlaced ? "interlaced" : "progressive");

    size_t samples = 0;
    if (svt_av2_compute_frame_sample_count(info->width, info->height, info->chroma_format, &samples) ==
        SVT_AV2_ERROR_NONE) {
        const size_t bytes = samples * svt_av2_bytes_per_sample(info->bit_depth);
        printf("frame size  : %zu samples / %zu bytes (unpadded)\n", samples, bytes);
    }
}

/* --- M1 input profile ------------------------------------------------------ */

/* The M1 encoder path accepts exactly one profile: 64x64, 8-bit, 4:2:0,
 * progressive. Returns 0 when the input is supported, -1 otherwise and prints
 * the reason to stderr. */
static int app_check_m1_input(const SvtAv2Y4mInfo* info) {
    if (info->width % 8u != 0 || info->height % 8u != 0) {
        fprintf(stderr, "error: M4 requires dimensions divisible by 8\n");
        return -1;
    }
    if (info->bit_depth != SVT_AV2_BIT_DEPTH_8 && info->bit_depth != SVT_AV2_BIT_DEPTH_10) {
        fprintf(stderr, "error: M4 supports only 8-bit or 10-bit input, got %u-bit\n", (unsigned)info->bit_depth);
        return -1;
    }
    if (info->chroma_format != SVT_AV2_CHROMA_420 &&
        !(info->bit_depth == SVT_AV2_BIT_DEPTH_10 && info->chroma_format == SVT_AV2_CHROMA_444)) {
        fprintf(stderr,
                "error: M4 supports 8-bit 420, 10-bit 420 or 10-bit 444 input, got %s\n",
                svt_av2_chroma_format_name(info->chroma_format));
        return -1;
    }
    if (info->interlaced) {
        fprintf(stderr, "error: M4 supports only progressive input\n");
        return -1;
    }
    return 0;
}

/* --- frame iteration ------------------------------------------------------ */

/* Reads the next frame tag. Returns 1 when a frame tag was found, 0 at clean
 * end of stream, -1 on malformed input. The stream is left positioned at the
 * first payload byte. */
static int app_read_frame_tag(FILE* stream) {
    char   tag[8];
    size_t length = 0;

    while (length < sizeof(tag) - 1) {
        const int c = fgetc(stream);
        if (c == EOF) {
            return (length == 0) ? 0 : -1;
        }
        if (c == '\n') {
            break;
        }
        tag[length++] = (char)c;
    }
    tag[length] = '\0';

    if (length < 5 || memcmp(tag, "FRAME", 5) != 0) {
        return -1;
    }
    return 1;
}

/* Reads exactly `size` bytes of frame payload into buffer. Returns 0 on
 * success, -1 on truncated input. */
static int app_read_frame_payload(FILE* stream, uint8_t* buffer, size_t size) {
    if (size == 0) {
        return 0;
    }
    return (fread(buffer, 1, size, stream) == size) ? 0 : -1;
}

/* Fills a SvtAv2Picture view over tightly packed supported YUV data held in a
 * single Y-then-U-then-V buffer (the Y4M layout). planes[] aliases into
 * `buffer`; strides are the plane widths in bytes. */
static void app_fill_picture(SvtAv2Picture* picture, uint8_t* buffer,
                                     uint32_t width, uint32_t height, SvtAv2BitDepth bit_depth,
                                     SvtAv2ChromaFormat chroma_format) {
    const uint32_t bytes = bit_depth == SVT_AV2_BIT_DEPTH_10 ? 2u : 1u;
    const uint32_t shift = chroma_format == SVT_AV2_CHROMA_444 ? 0u : 1u;
    const uint32_t chroma_width = width >> shift;
    const uint32_t chroma_height = height >> shift;
    uint8_t* const luma = buffer;
    uint8_t* const cb = luma + (size_t)width * height * bytes;
    uint8_t* const cr = cb + (size_t)chroma_width * chroma_height * bytes;

    memset(picture, 0, sizeof(*picture));
    picture->planes[0] = luma;
    picture->planes[1] = cb;
    picture->planes[2] = cr;
    picture->strides[0] = width * bytes;
    picture->strides[1] = chroma_width * bytes;
    picture->strides[2] = chroma_width * bytes;
    picture->width = width;
    picture->height = height;
    picture->chroma_format = chroma_format;
    picture->bit_depth = bit_depth;
    picture->pts = 0;
}

/* --- main ----------------------------------------------------------------- */

int main(int argc, char** argv) {
    AppOptions options;
    const int  parse_result = app_parse_args(argc, argv, &options);
    if (parse_result == 1) {
        return 0;
    }
    if (parse_result != 0) {
        print_usage(argv[0]);
        return 2;
    }

    SvtAv2Y4mInfo y4m;
    SvtAv2Error   err = svt_av2_y4m_read_header_from_path(options.input_path, &y4m);
    if (err != SVT_AV2_ERROR_NONE) {
        fprintf(stderr,
                "error: cannot read Y4M header from '%s' (%s)\n",
                options.input_path,
                svt_av2_error_to_string(err));
        return 3;
    }

    app_print_y4m_info(options.input_path, &y4m);

    if (options.output_path == NULL) {
        printf("mode        : metadata only (no --output given)\n");
        return 0;
    }

    /* The M1 encoder path exists for exactly one profile; unsupported input is
     * rejected here rather than producing a stream that could be mistaken for a
     * successful encode. */
    if (app_check_m1_input(&y4m) != 0) {
        return SVT_AV2_APP_EXIT_UNSUPPORTED_INPUT;
    }

    SvtAv2Handle             handle = NULL;
    const SvtAv2EncoderInfo* info = NULL;
    err = svt_av2_enc_init_handle(&handle, &info);
    if (err != SVT_AV2_ERROR_NONE) {
        fprintf(stderr, "error: svt_av2_enc_init_handle failed (%s)\n", svt_av2_error_to_string(err));
        return 4;
    }

    const SvtAv2ChromaFormat chroma = y4m.chroma_format;
    const SvtAv2BitDepth     depth = y4m.bit_depth;
    const SvtAv2Preset       preset = (SvtAv2Preset)options.preset;
    const uint32_t           threads = (uint32_t)options.threads;
    const int32_t            deterministic = options.deterministic;

    /* Parameters must be set before svt_av2_enc_init(). */
    err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_WIDTH, &y4m.width);
    if (err == SVT_AV2_ERROR_NONE) {
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_HEIGHT, &y4m.height);
    }
    if (err == SVT_AV2_ERROR_NONE) {
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_CHROMA_FORMAT, &chroma);
    }
    if (err == SVT_AV2_ERROR_NONE) {
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_BIT_DEPTH, &depth);
    }
    if (err == SVT_AV2_ERROR_NONE) {
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_FRAME_RATE_NUM, &y4m.frame_rate_num);
    }
    if (err == SVT_AV2_ERROR_NONE) {
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_FRAME_RATE_DEN, &y4m.frame_rate_den);
    }
    if (err == SVT_AV2_ERROR_NONE) {
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_QP, &options.qp);
    }
    if (err == SVT_AV2_ERROR_NONE) {
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_PRESET, &preset);
    }
    if (err == SVT_AV2_ERROR_NONE) {
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_THREADS, &threads);
    }
    if (err == SVT_AV2_ERROR_NONE) {
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_DETERMINISTIC, &deterministic);
    }
    if (err == SVT_AV2_ERROR_NONE) {
        err = svt_av2_enc_set_parameter(handle, SVT_AV2_PARAM_OUTPUT_PATH, &options.output_path);
    }
    if (err != SVT_AV2_ERROR_NONE) {
        fprintf(stderr, "error: setting encoder parameters failed (%s)\n", svt_av2_error_to_string(err));
        svt_av2_enc_deinit(&handle);
        return 5;
    }

    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_PREDICTION_STRUCTURE, &options.prediction_structure);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_INTRA_MODE_SELECTION, &options.intra_mode_selection);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_PARTITION_SELECTION, &options.partition_selection);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_TRANSFORM_SELECTION, &options.transform_selection);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_ANGLE_SEARCH, &options.angle_search);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_MRL_SEARCH, &options.mrl_search);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_TREE_REFINEMENT, &options.tree_refinement);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_RD_LAMBDA_MODEL, &options.rd_lambda_model);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_CFL_SEARCH, &options.cfl_search);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_MHCCP_SEARCH, &options.mhccp_search);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_INTRA_JOINT, &options.intra_joint);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_RD_MODE, &options.rd_mode);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_COEFF_OPTIMIZATION, &options.coeff_optimization);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_MOTION_OPTIMIZATION, &options.motion_optimization);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_ENTROPY_ADAPTATION, &options.entropy_adaptation);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_set_parameter(handle,
        SVT_AV2_PARAM_STILL_PICTURE, &options.still_picture);
    /* Omission preserves the historical setter sequence and NONE default. */
    if (err == SVT_AV2_ERROR_NONE && options.content_type!=SVT_AV2_CONTENT_NONE)
        err = svt_av2_enc_set_parameter(handle,SVT_AV2_PARAM_CONTENT_TYPE,&options.content_type);
    if (err == SVT_AV2_ERROR_NONE) err = svt_av2_enc_init(handle);
    if (err != SVT_AV2_ERROR_NONE) {
        fprintf(stderr, "error: svt_av2_enc_init failed (%s)\n", svt_av2_error_to_string(err));
        svt_av2_enc_deinit(&handle);
        return 6;
    }

    printf("encoder     : %s (%s, %s)\n", info->version_string, info->milestone, info->build_type);
    printf("config      : qp=%d preset=%d threads=%u deterministic=%d\n",
           options.qp,
           options.preset,
           threads,
           deterministic);
    printf("output      : %s\n", options.output_path);

    FILE* input = fopen(options.input_path, "rb");
    if (input == NULL) {
        fprintf(stderr, "error: cannot open '%s'\n", options.input_path);
        svt_av2_enc_deinit(&handle);
        return 3;
    }

    /* Re-read the header so the stream is positioned on the first frame tag. */
    if (svt_av2_y4m_read_header(input, &y4m) != SVT_AV2_ERROR_NONE) {
        fprintf(stderr, "error: re-reading Y4M header failed\n");
        fclose(input);
        svt_av2_enc_deinit(&handle);
        return 3;
    }

    size_t frame_bytes = 0;
    if (svt_av2_compute_frame_sample_count(y4m.width, y4m.height, y4m.chroma_format, &frame_bytes) !=
        SVT_AV2_ERROR_NONE) {
        fprintf(stderr, "error: cannot compute frame size\n");
        fclose(input);
        svt_av2_enc_deinit(&handle);
        return 7;
    }
    frame_bytes *= svt_av2_bytes_per_sample(y4m.bit_depth);

    /* One input frame buffer, reused for every frame: the API copies the pixels
     * during send_picture(), so the buffer is free to be overwritten immediately
     * (contract section 1.2). */
    uint8_t* frame_buffer = (uint8_t*)malloc(frame_bytes);
    if (frame_buffer == NULL) {
        fprintf(stderr, "error: cannot allocate %zu byte frame buffer\n", frame_bytes);
        fclose(input);
        svt_av2_enc_deinit(&handle);
        return 7;
    }

    FILE* output = fopen(options.output_path, "wb");
    if (output == NULL) {
        fprintf(stderr, "error: cannot create '%s'\n", options.output_path);
        free(frame_buffer);
        fclose(input);
        svt_av2_enc_deinit(&handle);
        return 3;
    }

    /* --frames 0 emits nothing; positive values bound the loop; the default
     * drains the whole source. Each frame uses the public API and optional
     * reconstruction export before the next picture is submitted. */
    const int32_t frame_limit = options.frames; /* -1 = all */

    size_t   packet_capacity = SVT_AV2_APP_PACKET_INITIAL_BYTES;
    uint8_t* packet = (uint8_t*)malloc(packet_capacity);
    if (packet == NULL) {
        fprintf(stderr, "error: cannot allocate %zu byte packet buffer\n", packet_capacity);
        fclose(output);
        free(frame_buffer);
        fclose(input);
        svt_av2_enc_deinit(&handle);
        return 7;
    }

    uint32_t frames_written = 0;
    int      encode_failed = 0;
    int      exit_code = 0;
    const int32_t effective_limit = options.still_picture ? 1 : frame_limit;

    /* --- optional reconstruction export -------------------------------------
     *
     * The reconstruction is the encoder's own picture, not a decode of the
     * stream. It is obtained ONLY through the public accessor
     * svt_av2_enc_get_recon(), which in turn calls the project's single packing
     * primitive; the app never packs a frame itself. The FILE* below is the one
     * and only I/O point for this feature (contract section 7.2). */
    FILE*    recon_output = NULL;
    uint8_t* recon_buffer = NULL;
    size_t   recon_capacity = 0;
    if (options.recon_path != NULL) {
        /* `data` is a required, caller-owned buffer (svt_av2.h:199): the
         * accessor reports "too small" only once it has a buffer to measure
         * against. Start with the same generous capacity the packet path uses
         * and let the grow-and-retry branch below take over on OUT_OF_MEMORY. */
        recon_capacity = SVT_AV2_APP_PACKET_INITIAL_BYTES;
        recon_buffer = (uint8_t*)malloc(recon_capacity);
        if (recon_buffer == NULL) {
            fprintf(stderr, "error: cannot allocate %zu byte recon buffer\n", recon_capacity);
            free(packet);
            free(frame_buffer);
            fclose(input);
            fclose(output);
            svt_av2_enc_deinit(&handle);
            return 7;
        }

        recon_output = fopen(options.recon_path, "wb");
        if (recon_output == NULL) {
            fprintf(stderr, "error: cannot create '%s'\n", options.recon_path);
            free(recon_buffer);
            free(packet);
            free(frame_buffer);
            fclose(input);
            fclose(output);
            svt_av2_enc_deinit(&handle);
            return 3;
        }
    }

    for (;;) {
        if (effective_limit >= 0 && (int32_t)frames_written >= effective_limit) {
            break;
        }

        const int tag = app_read_frame_tag(input);
        if (tag == 0) {
            break; /* clean end of stream */
        }
        if (tag < 0) {
            fprintf(stderr, "error: malformed frame tag in '%s'\n", options.input_path);
            exit_code = 3;
            break;
        }
        if (app_read_frame_payload(input, frame_buffer, frame_bytes) != 0) {
            fprintf(stderr,
                    "error: truncated frame %u in '%s'\n",
                    frames_written,
                    options.input_path);
            exit_code = 3;
            break;
        }

        SvtAv2Picture picture;
        app_fill_picture(&picture, frame_buffer, y4m.width, y4m.height, y4m.bit_depth, y4m.chroma_format);
        picture.pts = (int32_t)frames_written;

        err = svt_av2_enc_send_picture(handle, &picture);
        if (err != SVT_AV2_ERROR_NONE) {
            fprintf(stderr,
                    "error: svt_av2_enc_send_picture failed for frame %u (%s)\n",
                    frames_written,
                    svt_av2_error_to_string(err));
            exit_code = 9;
            break;
        }

        SvtAv2Packet out;
        out.data = packet;
        out.capacity = packet_capacity;
        out.size = 0;
        out.pts = 0;
        err = svt_av2_enc_get_packet_ex(handle, &out);
        if (err == SVT_AV2_ERROR_OUT_OF_MEMORY) {
            /* The API reports the size it needs in `size`; grow and retry. This
             * is why the app never needs a hard-coded 4096-byte buffer. */
            const size_t needed = (out.size > 0) ? out.size : packet_capacity * 2u;
            uint8_t*     grown = (uint8_t*)realloc(packet, needed);
            if (grown == NULL) {
                fprintf(stderr, "error: cannot grow packet buffer to %zu bytes\n", needed);
                exit_code = 7;
                break;
            }
            packet = grown;
            packet_capacity = needed;

            out.data = packet;
            out.capacity = packet_capacity;
            out.size = 0;
            err = svt_av2_enc_get_packet_ex(handle, &out);
        }
        if (err != SVT_AV2_ERROR_NONE) {
            fprintf(stderr,
                    "error: svt_av2_enc_get_packet_ex failed for frame %u (%s)\n",
                    frames_written,
                    svt_av2_error_to_string(err));
            exit_code = 11;
            break;
        }
        if (out.size == 0) {
            fprintf(stderr, "error: encoder produced no packet for frame %u\n", frames_written);
            exit_code = 11;
            break;
        }

        if (fwrite(packet, 1, out.size, output) != out.size) {
            fprintf(stderr, "error: writing frame %u to '%s' failed\n", frames_written, options.output_path);
            exit_code = 12;
            break;
        }
        if (options.verbose) {
            printf("frame %u    : %zu bytes (pts %d)\n", frames_written, out.size, out.pts);
        }

        /* Additive output: the encoded stream above is unaffected by this
         * block. The recon is written only when the user asked for it, and a
         * failure here is a real failure (contract section 7: no fake
         * success). */
        if (recon_output != NULL) {
            SvtAv2ReconFrame recon;
            recon.data = recon_buffer;
            recon.capacity = recon_capacity;
            recon.byte_count = 0;
            recon.frame_index = 0;
            recon.visible_width = 0;
            recon.visible_height = 0;
            recon.chroma_width = 0;
            recon.chroma_height = 0;
            SvtAv2Error rerr = svt_av2_enc_get_recon(handle, &recon);
            if (rerr == SVT_AV2_ERROR_OUT_OF_MEMORY) {
                /* The accessor reports the required size in `byte_count`; grow
                 * and retry, exactly as the packet path does. */
                const size_t needed = (recon.byte_count > 0) ? recon.byte_count
                                                             : (recon_capacity > 0 ? recon_capacity * 2u : 1u);
                uint8_t* grown = (uint8_t*)realloc(recon_buffer, needed);
                if (grown == NULL) {
                    fprintf(stderr, "error: cannot grow recon buffer to %zu bytes\n", needed);
                    exit_code = 7;
                    break;
                }
                recon_buffer = grown;
                recon_capacity = needed;

                recon.data = recon_buffer;
                recon.capacity = recon_capacity;
                recon.byte_count = 0;
                rerr = svt_av2_enc_get_recon(handle, &recon);
            }
            if (rerr != SVT_AV2_ERROR_NONE) {
                fprintf(stderr,
                        "error: svt_av2_enc_get_recon failed for frame %u (%s)\n",
                        frames_written,
                        svt_av2_error_to_string(rerr));
                exit_code = 11;
                break;
            }
            if (recon.byte_count == 0 ||
                fwrite(recon_buffer, 1, recon.byte_count, recon_output) != recon.byte_count) {
                fprintf(stderr,
                        "error: writing reconstruction for frame %u to '%s' failed\n",
                        frames_written,
                        options.recon_path);
                exit_code = 12;
                break;
            }
            if (options.verbose) {
                printf("recon %u    : %zu bytes (%ux%u, frame_index %u)\n",
                       frames_written,
                       recon.byte_count,
                       recon.visible_width,
                       recon.visible_height,
                       recon.frame_index);
            }
        }
        if (options.rd_mode) {
            SvtAv2RdStats rd;
            const SvtAv2Error rd_error = svt_av2_enc_get_rd_stats(handle, &rd);
            if (rd_error != SVT_AV2_ERROR_NONE) {
                fprintf(stderr, "error: reading RD statistics failed (%s)\n", svt_av2_error_to_string(rd_error));
                exit_code = 11;
                break;
            }
            printf("rd frame=%u mode=%u candidates=%" PRIu64 " forward=%" PRIu64
                   " replay=%" PRIu64 " D=%" PRIu64 " Rq12=%" PRIu64 " J=%" PRIu64
                   " lambdaq8=%" PRIu64 " full=%" PRIu64 " partial=%" PRIu64
                   " winner=%" PRIu64 "\n", frames_written, options.rd_mode,
                   rd.candidate_count, rd.forward_calls, rd.replay_checks, rd.distortion,
                   rd.rate_q12, rd.cost, rd.lambda_q8, rd.full_superblocks,
                   rd.partial_superblocks, rd.winner_hash);
            if (options.coeff_optimization)
                printf("coeff frame=%u trials=%" PRIu64 " accepts=%" PRIu64 "\n",
                    frames_written, rd.coeff_trials, rd.coeff_accepts);
            if (options.motion_optimization)
                printf("motion frame=%u trials=%" PRIu64 " accepts=%" PRIu64 "\n",
                    frames_written, rd.motion_trials, rd.motion_accepts);
            if (options.motion_optimization == 2u)
                printf("wide frame=%u trials=%" PRIu64 " accepts=%" PRIu64 "\n",
                    frames_written, rd.wide_trials, rd.wide_accepts);
        }
        frames_written++;
    }

    /* Signal end of stream through the public API so the flush path is
     * exercised, then close. A failure to close must not be reported as a
     * successful encode (contract section 7: no fake success). */
    if (exit_code == 0 && !encode_failed) {
        const SvtAv2Error eos_err = svt_av2_enc_send_picture(handle, NULL);
        if (eos_err != SVT_AV2_ERROR_NONE) {
            fprintf(stderr, "error: EOS (send_picture(NULL)) failed (%s)\n", svt_av2_error_to_string(eos_err));
            exit_code = 9;
        }
    }

    free(packet);
    free(recon_buffer);
    free(frame_buffer);
    fclose(input);

    /* Closing the output is where a buffered write can still fail; a successful
     * fwrite loop is not proof that the file is complete. */
    if (fclose(output) != 0 && exit_code == 0) {
        fprintf(stderr, "error: closing '%s' failed\n", options.output_path);
        exit_code = 12;
    }
    if (recon_output != NULL) {
        /* Same rule for the additive reconstruction file. */
        if (fclose(recon_output) != 0 && exit_code == 0) {
            fprintf(stderr, "error: closing '%s' failed\n", options.recon_path);
            exit_code = 12;
        }
    }

    const SvtAv2Error deinit_err = svt_av2_enc_deinit(&handle);
    if (exit_code != 0) {
        return exit_code;
    }
    if (deinit_err != SVT_AV2_ERROR_NONE) {
        fprintf(stderr, "error: svt_av2_enc_deinit failed (%s)\n", svt_av2_error_to_string(deinit_err));
        return 13;
    }

    printf("mode        : AV2 elementary stream (Annex B)\n");
    printf("output      : %s\n", options.output_path);
    if (options.recon_path != NULL) {
        printf("recon       : %s (%u frame%s, tight planar raw; 10-bit uses u16le)\n",
               options.recon_path,
               frames_written,
               (frames_written == 1u) ? "" : "s");
    }
    printf("frames      : %u encoded\n", frames_written);
    printf("note        : %s, coded transform residuals\n",
           options.prediction_structure == SVT_AV2_PRED_LOW_DELAY_P
               ? "LOW_DELAY_P, single reconstructed reference" :
                 options.intra_mode_selection == 4u ? "SINGLE_PICTURE/directional Y + implicit CfL UV RD" :
                 options.intra_mode_selection == 3u ? "SINGLE_PICTURE/directional Y + basic UV RD" :
                 options.intra_mode_selection == 2u ? "SINGLE_PICTURE/basic Y/UV RD" : "ALL_INTRA/DC_PRED");
    return 0;
}
