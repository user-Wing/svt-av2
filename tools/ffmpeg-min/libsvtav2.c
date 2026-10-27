/* Minimal synchronous SVT-AV2 M4 encoder wrapper. LGPL-2.1-or-later. */
#include <svt_av2.h>
#include "libavutil/opt.h"
#include "avcodec.h"
#include "codec_internal.h"
#include "encode.h"

typedef struct SvtAv2Context {
    const AVClass *class;
    SvtAv2Handle handle;
    int qp;
    int prediction;
    uint64_t frames;
} SvtAv2Context;

static int svt_error(AVCodecContext *avctx, SvtAv2Error err)
{
    if (err == SVT_AV2_ERROR_NONE)
        return 0;
    av_log(avctx, AV_LOG_ERROR, "SVT-AV2: %s\n", svt_av2_error_to_string(err));
    return err == SVT_AV2_ERROR_OUT_OF_MEMORY ? AVERROR(ENOMEM) : AVERROR_EXTERNAL;
}

static av_cold int svt_init(AVCodecContext *avctx)
{
    SvtAv2Context *s = avctx->priv_data;
    AVRational rate = avctx->framerate;
    int ret;
    const char *output_label = "ffmpeg-packet-output";
    int values[SVT_AV2_PARAM_COUNT] = {0};
    const SvtAv2Parameter params[] = {
        SVT_AV2_PARAM_WIDTH, SVT_AV2_PARAM_HEIGHT, SVT_AV2_PARAM_CHROMA_FORMAT,
        SVT_AV2_PARAM_BIT_DEPTH, SVT_AV2_PARAM_FRAME_RATE_NUM,
        SVT_AV2_PARAM_FRAME_RATE_DEN, SVT_AV2_PARAM_QP, SVT_AV2_PARAM_THREADS,
        SVT_AV2_PARAM_PREDICTION_STRUCTURE
    };
    if (avctx->bit_rate || avctx->max_b_frames || avctx->thread_count > 1 ||
        (avctx->gop_size != -1 && avctx->gop_size != 1)) {
        av_log(avctx, AV_LOG_ERROR,
               "M4 requires bitrate=0, no B frames, threads=1 and g=-1 or 1.\n");
        return AVERROR(EINVAL);
    }
    if (avctx->gop_size == 1 && s->prediction != SVT_AV2_PRED_ALL_INTRA)
        return AVERROR(EINVAL);
    if (rate.num <= 0 || rate.den <= 0)
        rate = av_inv_q(avctx->time_base);
    values[SVT_AV2_PARAM_WIDTH] = avctx->width;
    values[SVT_AV2_PARAM_HEIGHT] = avctx->height;
    values[SVT_AV2_PARAM_CHROMA_FORMAT] = avctx->pix_fmt == AV_PIX_FMT_YUV444P10LE ?
                                        SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
    values[SVT_AV2_PARAM_BIT_DEPTH] = avctx->pix_fmt == AV_PIX_FMT_YUV420P ? 8 : 10;
    values[SVT_AV2_PARAM_FRAME_RATE_NUM] = rate.num;
    values[SVT_AV2_PARAM_FRAME_RATE_DEN] = rate.den;
    values[SVT_AV2_PARAM_QP] = s->qp;
    values[SVT_AV2_PARAM_THREADS] = 1;
    values[SVT_AV2_PARAM_PREDICTION_STRUCTURE] = s->prediction;
    if ((ret = svt_error(avctx, svt_av2_enc_init_handle(&s->handle, NULL))) < 0)
        return ret;
    for (int i = 0; i < FF_ARRAY_ELEMS(params); i++) {
        if ((ret = svt_error(avctx, svt_av2_enc_set_parameter(
                s->handle, params[i], &values[params[i]]))) < 0)
            return ret;
    }
    /* M4 requires a nonempty output label; coded bytes use the packet API. */
    if ((ret = svt_error(avctx, svt_av2_enc_set_parameter(
            s->handle, SVT_AV2_PARAM_OUTPUT_PATH, &output_label))) < 0)
        return ret;
    avctx->has_b_frames = 0;
    return svt_error(avctx, svt_av2_enc_init(s->handle));
}

static int svt_encode(AVCodecContext *avctx, AVPacket *pkt,
                      const AVFrame *frame, int *got_packet)
{
    SvtAv2Context *s = avctx->priv_data;
    SvtAv2Picture pic = {0};
    uint8_t dummy;
    SvtAv2Packet out = { .data = &dummy, .capacity = 1 };
    SvtAv2Error err;
    int ret;
    *got_packet = 0;
    if (!frame)
        return svt_error(avctx, svt_av2_enc_send_picture(s->handle, NULL));
    if (s->frames > INT32_MAX)
        return AVERROR(EINVAL);
    pic.width = frame->width;
    pic.height = frame->height;
    pic.chroma_format = frame->format == AV_PIX_FMT_YUV444P10LE ?
                        SVT_AV2_CHROMA_444 : SVT_AV2_CHROMA_420;
    pic.bit_depth = frame->format == AV_PIX_FMT_YUV420P ? 8 : 10;
    pic.pts = s->frames;
    for (int i = 0; i < 3; i++) {
        if (frame->linesize[i] <= 0)
            return AVERROR(EINVAL);
        pic.planes[i] = frame->data[i];
        pic.strides[i] = frame->linesize[i];
    }
    if ((ret = svt_error(avctx, svt_av2_enc_send_picture(s->handle, &pic))) < 0)
        return ret;
    err = svt_av2_enc_get_packet_ex(s->handle, &out);
    if (err != SVT_AV2_ERROR_OUT_OF_MEMORY && err != SVT_AV2_ERROR_NONE)
        return svt_error(avctx, err);
    if (!out.size || out.size > INT_MAX)
        return AVERROR_INVALIDDATA;
    if ((ret = ff_get_encode_buffer(avctx, pkt, out.size, 0)) < 0)
        return ret;
    if (err == SVT_AV2_ERROR_OUT_OF_MEMORY) {
        out.data = pkt->data;
        out.capacity = pkt->size;
        if ((ret = svt_error(avctx, svt_av2_enc_get_packet_ex(s->handle, &out))) < 0)
            return ret;
    } else {
        pkt->data[0] = dummy;
    }
    pkt->pts = pkt->dts = frame->pts;
    pkt->duration = frame->duration;
    if (!s->frames || s->prediction == SVT_AV2_PRED_ALL_INTRA)
        pkt->flags |= AV_PKT_FLAG_KEY;
    s->frames++;
    *got_packet = 1;
    return 0;
}

static av_cold int svt_close(AVCodecContext *avctx)
{
    SvtAv2Context *s = avctx->priv_data;
    if (s->handle)
        svt_av2_enc_deinit(&s->handle);
    return 0;
}

#define VE (AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM)
static const AVOption options[] = {
    { "qp", "M4 fixed user QP", offsetof(SvtAv2Context, qp),
      AV_OPT_TYPE_INT, {.i64 = 100}, 100, 100, VE },
    { "pred-structure", "M4 prediction structure", offsetof(SvtAv2Context, prediction),
      AV_OPT_TYPE_INT, {.i64 = 0}, 0, 1, VE, .unit = "prediction" },
    { "all-intra", "Every frame is a keyframe", 0, AV_OPT_TYPE_CONST,
      {.i64 = 0}, 0, 0, VE, .unit = "prediction" },
    { "low-delay-p", "First keyframe followed by single-reference P frames", 0,
      AV_OPT_TYPE_CONST, {.i64 = 1}, 0, 0, VE, .unit = "prediction" },
    { NULL }
};
static const AVClass svt_class = {
    .class_name = "libsvtav2", .item_name = av_default_item_name,
    .option = options, .version = LIBAVUTIL_VERSION_INT,
};
static const FFCodecDefault defaults[] = {
    { "b", "0" }, { "g", "-1" }, { "threads", "1" }, { NULL }
};
const FFCodec ff_libsvtav2_encoder = {
    .p.name = "libsvtav2", CODEC_LONG_NAME("SVT-AV2 M4 scalar encoder"),
    .p.type = AVMEDIA_TYPE_VIDEO, .p.id = AV_CODEC_ID_AV2,
    .priv_data_size = sizeof(SvtAv2Context), .init = svt_init,
    FF_CODEC_ENCODE_CB(svt_encode), .close = svt_close,
    /* Keep the requested thread count visible to M4 validation. */
    .caps_internal = FF_CODEC_CAP_INIT_CLEANUP | FF_CODEC_CAP_AUTO_THREADS,
    CODEC_PIXFMTS(AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV420P10LE, AV_PIX_FMT_YUV444P10LE),
    .p.priv_class = &svt_class, .defaults = defaults, .p.wrapper_name = "libsvtav2",
};
