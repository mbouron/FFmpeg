/*
 * Android MediaCodec decoder
 *
 * Copyright (c) 2015-2016 Matthieu Bouron <matthieu.bouron stupeflix.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <string.h>
#include <sys/types.h>

#include <android/native_window.h>

#include "libavutil/atomic.h"
#include "libavutil/common.h"
#include "libavutil/mem.h"
#include "libavutil/log.h"
#include "libavutil/pixfmt.h"
#include "libavutil/time.h"
#include "libavutil/timestamp.h"
#include "libavutil/jni.h"
#include "libavutil/jni_internal.h"

#include "avcodec.h"
#include "internal.h"
#include "mediacodec.h"
#include "mediacodecdec.h"

#include "mediacodec_wrapper.h"

enum {
    COLOR_FormatYUV420Planar                              = 0x13,
    COLOR_FormatYUV420SemiPlanar                          = 0x15,
    COLOR_FormatYCbYCr                                    = 0x19,
    COLOR_FormatAndroidOpaque                             = 0x7F000789,
    COLOR_QCOM_FormatYUV420SemiPlanar                     = 0x7fa30c00,
    COLOR_QCOM_FormatYUV420SemiPlanar32m                  = 0x7fa30c04,
    COLOR_QCOM_FormatYUV420PackedSemiPlanar64x32Tile2m8ka = 0x7fa30c03,
    COLOR_TI_FormatYUV420PackedSemiPlanar                 = 0x7f000100,
    COLOR_TI_FormatYUV420PackedSemiPlanarInterlaced       = 0x7f000001,
};

static const struct {

    int color_format;
    enum AVPixelFormat pix_fmt;

} color_formats[] = {

    { COLOR_FormatYUV420Planar,                              AV_PIX_FMT_YUV420P },
    { COLOR_FormatYUV420SemiPlanar,                          AV_PIX_FMT_NV12    },
    { COLOR_QCOM_FormatYUV420SemiPlanar,                     AV_PIX_FMT_NV12    },
    { COLOR_QCOM_FormatYUV420SemiPlanar32m,                  AV_PIX_FMT_NV12    },
    { COLOR_QCOM_FormatYUV420PackedSemiPlanar64x32Tile2m8ka, AV_PIX_FMT_NV12    },
    { COLOR_TI_FormatYUV420PackedSemiPlanar,                 AV_PIX_FMT_NV12    },
    { COLOR_TI_FormatYUV420PackedSemiPlanarInterlaced,       AV_PIX_FMT_NV12    },
    { 0 }
};

static enum AVPixelFormat mcdec_map_color_format(AVCodecContext *avctx, MediaCodecDecContext *s, int color_format)
{
    int i;
    enum AVPixelFormat ret = AV_PIX_FMT_NONE;

    if (s->surface)
        return AV_PIX_FMT_MEDIACODEC;

    if (!strcmp(s->codec_name, "OMX.k3.video.decoder.avc") && color_format == COLOR_FormatYCbYCr) {
        s->color_format = color_format = COLOR_TI_FormatYUV420PackedSemiPlanar;
    }

    for (i = 0; i < FF_ARRAY_ELEMS(color_formats); i++) {
        if (color_formats[i].color_format == color_format) {
            return color_formats[i].pix_fmt;
        }
    }

    av_log(avctx, AV_LOG_ERROR, "Output color format 0x%x (value=%d) is not supported\n",
        color_format, color_format);

    return ret;
}

static MediaCodecRef *mediacodec_ref_create(FFAMediaCodec *codec)
{
    MediaCodecRef *ref;

    ref = av_mallocz(sizeof(*ref));
    if (!ref)
        goto fail;

    ref->refcount = av_mallocz(sizeof(*ref->refcount));
    if (!ref->refcount)
        goto fail;

    ref->codec = codec;
    *ref->refcount = 1;

    return ref;

fail:
    if (ref)
        av_free(ref->refcount);

    av_free(ref);

    return NULL;
}

static MediaCodecRef *mediacodec_ref_ref(MediaCodecRef *ref)
{
    MediaCodecRef *ret;

    ret = av_mallocz(sizeof(*ret));
    if (!ret) {
        return ret;
    }

    *ret = *ref;
    avpriv_atomic_int_add_and_fetch(ret->refcount, 1);

    return ret;
}

static void mediacodec_ref_unref(MediaCodecRef **ref)
{
    if (!ref || !*ref)
        return;

    if (!avpriv_atomic_int_add_and_fetch((*ref)->refcount, -1)) {
        int status;

        status = ff_AMediaCodec_flush((*ref)->codec);
        if (status < 0) {
            av_log(NULL, AV_LOG_ERROR,
                "Failed to flush MediaCodec %p", (*ref)->codec);
        }

        status = ff_AMediaCodec_stop((*ref)->codec);
        if (status < 0) {
            av_log(NULL, AV_LOG_ERROR,
                "Failed to stop MediaCodec %p", (*ref)->codec);
        }

        status = ff_AMediaCodec_delete((*ref)->codec);
        if (status < 0) {
            av_log(NULL, AV_LOG_ERROR,
                "Failed to delete MediaCodec %p", (*ref)->codec);
        }

        av_free((*ref)->refcount);
    }

    av_freep(ref);
}

static void mediacodec_buffer_release(void *opaque, uint8_t *data)
{
    MediaCodecBuffer *buffer = opaque;
    MediaCodecRef *codec_ref = buffer->codec_ref;
    int status;

    status = ff_AMediaCodec_releaseOutputBuffer(codec_ref->codec,
        buffer->index, buffer->surface != NULL);
    if (status < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to release output buffer\n");
    }

    mediacodec_ref_unref(&codec_ref);
    if (buffer->surface)
        ANativeWindow_release(buffer->surface);
    av_free(buffer);
}

#define QCOM_TILE_WIDTH 64
#define QCOM_TILE_HEIGHT 32
#define QCOM_TILE_SIZE (QCOM_TILE_WIDTH * QCOM_TILE_HEIGHT)
#define QCOM_TILE_GROUP_SIZE (4 * QCOM_TILE_SIZE)

static size_t qcom_tile_pos(size_t x, size_t y, size_t w, size_t h)
{
  size_t flim = x + (y & ~1) * w;

  if (y & 1) {
    flim += (x & ~3) + 2;
  } else if ((h & 1) == 0 || y != (h - 1)) {
    flim += (x + 2) & ~3;
  }

  return flim;
}

static int mediacodec_buffer_create(AVCodecContext *avctx,
                                    MediaCodecDecContext *s,
                                    uint8_t *data,
                                    size_t size,
                                    ssize_t index,
                                    FFAMediaCodecBufferInfo *info,
                                    AVFrame *frame)
{
    int ret = 0;
    int status = 0;
    MediaCodecBuffer *buffer = NULL;
    MediaCodecRef *codec_ref = s->codec_ref;

    frame->width = avctx->width;
    frame->height = avctx->height;
    frame->format = avctx->pix_fmt;
    frame->pkt_pts = info->presentationTimeUs;

    if (s->surface) {
        buffer = av_mallocz(sizeof(MediaCodecBuffer));
        if (!buffer) {
            av_log(avctx, AV_LOG_ERROR, "Could not allocate memory");
            return AVERROR(ENOMEM);
        }

        buffer->index = index;
        buffer->data = data;
        buffer->size = size;
        buffer->info = *info;
        buffer->codec_ref = mediacodec_ref_ref(codec_ref);
        buffer->surface = s->surface;
        if (buffer->surface) {
            ANativeWindow_acquire(buffer->surface);
        }

        frame->buf[0] = av_buffer_create(NULL,
                                        0,
                                        mediacodec_buffer_release,
                                        buffer,
                                        AV_BUFFER_FLAG_READONLY);

        if (!frame->buf[0]) {
            mediacodec_buffer_release(buffer, NULL);
            return AVERROR(ENOMEM);
        }

        return 0;
    }

    /* MediaCodec buffers needs unfortunately to be copied to our own
     * refcounted buffers because the flush command invalidates all input
     * and output buffers.
     */
    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0) {
        av_log(avctx, AV_LOG_ERROR, "Could not allocate buffer\n");
        goto done;
    }

    av_log(avctx, AV_LOG_DEBUG,
            "Frame: width=%d stride=%d height=%d slice-height=%d "
            "crop-top=%d crop-bottom=%d crop-left=%d crop-right=%d encoder=%s\n"
            "destination linesizes=%d,%d,%d" ,
            avctx->width, s->stride, avctx->height, s->slice_height,
            s->crop_top, s->crop_bottom, s->crop_left, s->crop_right, s->codec_name,
            frame->linesize[0], frame->linesize[1], frame->linesize[2]);

    switch (s->color_format) {
    case COLOR_FormatYUV420Planar: {
        int i;
        uint8_t *src = NULL;

        for (i = 0; i < 3; i++) {
            int stride = s->stride;
            int height;

            src = data + info->offset;
            if (i == 0) {
                height = avctx->height;

                src += s->crop_top * s->stride;
                src += s->crop_left;
            } else {
                height = avctx->height / 2;
                stride = (s->stride + 1) / 2;

                src += s->slice_height * s->stride;

                if (i == 2) {
                    src += ((s->slice_height + 1) / 2) * stride;
                }

                src += s->crop_top * stride;
                src += (s->crop_left / 2);
            }

            if (frame->linesize[i] == stride) {
                memcpy(frame->data[i], src, height * stride);
            } else {
                int j, width;
                uint8_t *dst = frame->data[i];

                if (i == 0) {
                    width = avctx->width;
                } else if (i == 1) {
                    width = FFMIN(frame->linesize[i], FFALIGN(avctx->width, 2));
                }

                for (j = 0; j < height; j++) {
                    memcpy(dst, src, width);
                    src += stride;
                    dst += frame->linesize[i];
                }
            }
        }

        break;
    }
    case COLOR_FormatYUV420SemiPlanar:
    case COLOR_QCOM_FormatYUV420SemiPlanar:
    case COLOR_QCOM_FormatYUV420SemiPlanar32m: {
        int i;
        uint8_t *src = NULL;

        for (i = 0; i < 2; i++) {
            int height;

            src = data + info->offset;
            if (i == 0) {
                height = avctx->height;

                src += s->crop_top * s->stride;
                src += s->crop_left;
            } else if (i == 1) {
                height = avctx->height / 2;

                src += s->slice_height * s->stride;
                src += s->crop_top * s->stride;
                src += s->crop_left;
            }

            if (frame->linesize[i] == s->stride) {
                memcpy(frame->data[i], src, height * s->stride);
            } else {
                int j, width;
                uint8_t *dst = frame->data[i];

                if (i == 0) {
                    width = avctx->width;
                } else if (i == 1) {
                    width = FFMIN(frame->linesize[i], FFALIGN(avctx->width, 2));
                }

                for (j = 0; j < height; j++) {
                    memcpy(dst, src, width);
                    src += s->stride;
                    dst += frame->linesize[i];
                }
            }
        }

        break;
    }
    case COLOR_TI_FormatYUV420PackedSemiPlanar:
    case COLOR_TI_FormatYUV420PackedSemiPlanarInterlaced: {
        int i;
        uint8_t *src = NULL;

        for (i = 0; i < 2; i++) {
            int height;

            src = data + info->offset;
            if (i == 0) {
                height = avctx->height;
            } else if (i == 1) {
                height = avctx->height / 2;

                src += (s->slice_height - s->crop_top / 2) * s->stride;

                src += s->crop_top * s->stride;
                src += s->crop_left;
            }

            if (frame->linesize[i] == s->stride) {
                memcpy(frame->data[i], src, height * s->stride);
            } else {
                int j, width;
                uint8_t *dst = frame->data[i];

                if (i == 0) {
                    width = avctx->width;
                } else if (i == 1) {
                    width = FFMIN(frame->linesize[i], FFALIGN(avctx->width, 2));
                }

                for (j = 0; j < height; j++) {
                    memcpy(dst, src, width);
                    src += s->stride;
                    dst += frame->linesize[i];
                }
            }
        }
    }
    case COLOR_QCOM_FormatYUV420PackedSemiPlanar64x32Tile2m8ka: {

        size_t width = frame->width;
        size_t linesize = frame->linesize[0];
        size_t height = frame->height;

        const size_t tile_w = (width - 1) / QCOM_TILE_WIDTH + 1;
        const size_t tile_w_align = (tile_w + 1) & ~1;
        const size_t tile_h_luma = (height - 1) / QCOM_TILE_HEIGHT + 1;
        const size_t tile_h_chroma = (height / 2 - 1) / QCOM_TILE_HEIGHT + 1;

        size_t luma_size = tile_w_align * tile_h_luma * QCOM_TILE_SIZE;
        if((luma_size % QCOM_TILE_GROUP_SIZE) != 0)
            luma_size = (((luma_size - 1) / QCOM_TILE_GROUP_SIZE) + 1) * QCOM_TILE_GROUP_SIZE;

        /* The following code is borrowed from VLC and Gstreamer */
        for(size_t y = 0; y < tile_h_luma; y++) {
            size_t row_width = width;
            for(size_t x = 0; x < tile_w; x++) {
                size_t tile_width = row_width;
                size_t tile_height = height;
                /* dest luma memory index for this tile */
                size_t luma_idx = y * QCOM_TILE_HEIGHT * linesize + x * QCOM_TILE_WIDTH;
                /* dest chroma memory index for this tile */
                /* XXX: remove divisions */
                size_t chroma_idx = (luma_idx / linesize) * linesize / 2 + (luma_idx % linesize);

                /* luma source pointer for this tile */
                const uint8_t *src_luma  = data
                    + qcom_tile_pos(x, y,tile_w_align, tile_h_luma) * QCOM_TILE_SIZE;

                /* chroma source pointer for this tile */
                const uint8_t *src_chroma = data + luma_size
                    + qcom_tile_pos(x, y/2, tile_w_align, tile_h_chroma) * QCOM_TILE_SIZE;
                if (y & 1)
                    src_chroma += QCOM_TILE_SIZE/2;

                /* account for right columns */
                if (tile_width > QCOM_TILE_WIDTH)
                    tile_width = QCOM_TILE_WIDTH;

                /* account for bottom rows */
                if (tile_height > QCOM_TILE_HEIGHT)
                    tile_height = QCOM_TILE_HEIGHT;

                tile_height /= 2;
                while (tile_height--) {
                    memcpy(frame->data[0] + luma_idx, src_luma, tile_width);
                    src_luma += QCOM_TILE_WIDTH;
                    luma_idx += linesize;

                    memcpy(frame->data[0] + luma_idx, src_luma, tile_width);
                    src_luma += QCOM_TILE_WIDTH;
                    luma_idx += linesize;

                    memcpy(frame->data[1] + chroma_idx, src_chroma, tile_width);
                    src_chroma += QCOM_TILE_WIDTH;
                    chroma_idx += linesize;
                }
                row_width -= QCOM_TILE_WIDTH;
            }
            height -= QCOM_TILE_HEIGHT;
        }
        break;
    }
    default:
        av_log(avctx, AV_LOG_ERROR, "Unsupported color format 0x%x (value=%d)\n",
            s->color_format, s->color_format);
        ret = AVERROR(EINVAL);
        goto done;
    }

    ret = 0;
done:
    status = ff_AMediaCodec_releaseOutputBuffer(codec_ref->codec, index, 0);
    if (status < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to release output buffer\n");
        ret = AVERROR_EXTERNAL;
    }

    return ret;
}

static int mediacodec_dec_parse_format(AVCodecContext *avctx, MediaCodecDecContext *s)
{
    int32_t value = 0;
    char *format = NULL;

    if (!s->format) {
        av_log(avctx, AV_LOG_ERROR, "Output MediaFormat is not set\n");
        return AVERROR(EINVAL);
    }

    format = ff_AMediaFormat_toString(s->format);
    if (!format) {
        return AVERROR_EXTERNAL;
    }
    av_log(avctx, AV_LOG_DEBUG, "Parsing MediaFormat %s\n", format);
    av_freep(&format);

    /* Mandatory fields */
    if (!ff_AMediaFormat_getInt32(s->format, "width", &value)) {
        format = ff_AMediaFormat_toString(s->format);
        av_log(avctx, AV_LOG_ERROR, "Could not get %s from format %s\n", "width", format);
        av_freep(&format);
        return AVERROR_EXTERNAL;
    }
    s->width = value;

    if (!ff_AMediaFormat_getInt32(s->format, "height", &value)) {
        format = ff_AMediaFormat_toString(s->format);
        av_log(avctx, AV_LOG_ERROR, "Could not get %s from format %s\n", "height", format);
        av_freep(&format);
        return AVERROR_EXTERNAL;
    }
    s->height = value;

    if (!ff_AMediaFormat_getInt32(s->format, "stride", &value)) {
        format = ff_AMediaFormat_toString(s->format);
        av_log(avctx, AV_LOG_ERROR, "Could not get %s from format %s\n", "stride", format);
        av_freep(&format);
        return AVERROR_EXTERNAL;
    }
    s->stride = value >= 0 ? value : s->width;

    if (!ff_AMediaFormat_getInt32(s->format, "slice-height", &value)) {
        format = ff_AMediaFormat_toString(s->format);
        av_log(avctx, AV_LOG_ERROR, "Could not get %s from format %s\n", "slice-height", format);
        av_freep(&format);
        return AVERROR_EXTERNAL;
    }
    if (value > 0) {
        s->slice_height = value;
    } else {
        s->slice_height = s->height;
    }

    if (strstr(s->codec_name, "OMX.Nvidia.")) {
        s->slice_height = FFALIGN(s->height, 16);
    } else if (strstr(s->codec_name, "OMX.SEC.avc.dec")) {
        s->slice_height = avctx->height;
        s->stride = avctx->width;
    }

    if (!ff_AMediaFormat_getInt32(s->format, "color-format", &value)) {
        format = ff_AMediaFormat_toString(s->format);
        av_log(avctx, AV_LOG_ERROR, "Could not get %s from format %s\n", "color-format", format);
        av_freep(&format);
        return AVERROR_EXTERNAL;
    }
    s->color_format = value;

    s->pix_fmt = avctx->pix_fmt = mcdec_map_color_format(avctx, s, value);
    if (avctx->pix_fmt == AV_PIX_FMT_NONE && !s->surface) {
        av_log(avctx, AV_LOG_ERROR, "Output color format is not supported\n");
        return AVERROR(EINVAL);
    }

    /* Optional fields */
    if (ff_AMediaFormat_getInt32(s->format, "crop-top", &value))
        s->crop_top = value;

    if (ff_AMediaFormat_getInt32(s->format, "crop-bottom", &value))
        s->crop_bottom = value;

    if (ff_AMediaFormat_getInt32(s->format, "crop-left", &value))
        s->crop_left = value;

    if (ff_AMediaFormat_getInt32(s->format, "crop-right", &value))
        s->crop_right = value;

    av_log(avctx, AV_LOG_INFO,
        "Output crop parameters top=%d bottom=%d left=%d right=%d\n",
        s->crop_top, s->crop_bottom, s->crop_left, s->crop_right);

    return 0;
}

int ff_mediacodec_dec_init(AVCodecContext *avctx, MediaCodecDecContext *s,
                           const char *mime, FFAMediaFormat *format)
{
    int ret = 0;
    FFAMediaCodec *codec;
    int status;
    enum AVPixelFormat pix_fmt;
    enum AVPixelFormat pix_fmts[3] = {
        AV_PIX_FMT_MEDIACODEC,
        AV_PIX_FMT_NONE,
        AV_PIX_FMT_NONE,
    };

    s->first_buffer_at = av_gettime();

    pix_fmt = ff_get_format(avctx, pix_fmts);
    if (pix_fmt == AV_PIX_FMT_MEDIACODEC) {
        AVMediaCodecContext *user_ctx = avctx->hwaccel_context;

        if (user_ctx && user_ctx->surface) {
            s->surface = user_ctx->surface;
            ANativeWindow_acquire(s->surface);
        }
    }

    s->codec_name = ff_AMediaCodecList_getCodecNameByType(mime, avctx->width, avctx->height);
    if (!s->codec_name) {
        ret = AVERROR_EXTERNAL;
        goto fail;
    }

    av_log(avctx, AV_LOG_DEBUG, "Found decoder %s\n", s->codec_name);
    codec = ff_AMediaCodec_createCodecByName(s->codec_name);
    if (!codec) {
        av_log(avctx, AV_LOG_ERROR, "Failed to create media decoder for type %s and name %s\n", mime, s->codec_name);
        ret = AVERROR_EXTERNAL;
        goto fail;
    }

    s->codec_ref = mediacodec_ref_create(codec);
    if (!s->codec_ref) {
        ret = AVERROR(ENOMEM);
        goto fail;
    }

    status = ff_AMediaCodec_configure(codec, format, NULL, NULL, 0);
    if (status < 0) {
        char *desc = ff_AMediaFormat_toString(format);
        av_log(avctx, AV_LOG_ERROR,
            "Failed to configure codec (status = %d) with format %s\n",
            status, desc);
        av_freep(&desc);

        ret = AVERROR_EXTERNAL;
        goto fail;
    }

    status = ff_AMediaCodec_start(codec);
    if (status < 0) {
        char *desc = ff_AMediaFormat_toString(format);
        av_log(avctx, AV_LOG_ERROR,
            "Failed to start codec (status = %d) with format %s\n",
            status, desc);
        av_freep(&desc);
        ret = AVERROR_EXTERNAL;
        goto fail;
    }

    s->format = ff_AMediaCodec_getOutputFormat(codec);
    if (s->format) {
        if ((ret = mediacodec_dec_parse_format(avctx, s)) < 0) {
            av_log(avctx, AV_LOG_ERROR,
                "Failed to configure context\n");
            goto fail;
        }
    }

    av_log(avctx, AV_LOG_DEBUG, "MediaCodec %p started successfully\n", codec);

    return 0;

fail:
    av_log(avctx, AV_LOG_ERROR, "MediaCodec %p failed to start\n", codec);
    ff_mediacodec_dec_close(avctx, s);
    return ret;
}

int ff_mediacodec_dec_decode(AVCodecContext *avctx, MediaCodecDecContext *s,
                             AVFrame *frame, int *got_frame,
                             AVPacket *pkt)
{
    int ret;
    int offset = 0;
    uint8_t *data;
    ssize_t index;
    size_t size;
    uint32_t flags;
    FFAMediaCodecBufferInfo info = { 0 };

    int status;

    int64_t input_dequeue_timeout_us = 8333;
    int64_t output_dequeue_timeout_us = 8333;

    FFAMediaCodec *codec = s->codec_ref->codec;

    while (offset < pkt->size) {
        int size;

        index = ff_AMediaCodec_dequeueInputBuffer(codec, input_dequeue_timeout_us);
        if (ff_AMediaCodec_infoTryAgainLater(codec, index)) {
            break;
        }

        if (index < 0) {
            av_log(avctx, AV_LOG_ERROR, "Failed to dequeue input buffer (status=%zd)\n", index);
            return AVERROR_EXTERNAL;
        }

        data = ff_AMediaCodec_getInputBuffer(codec, index, &size);
        if (!data) {
            av_log(avctx, AV_LOG_ERROR, "Failed to get input buffer\n");
            return AVERROR_EXTERNAL;
        }

        size = FFMIN(pkt->size - offset, size);

        memcpy(data, pkt->data + offset, size);
        offset += size;

        status = ff_AMediaCodec_queueInputBuffer(codec, index, 0, size, pkt->pts, flags);
        if (status < 0) {
            av_log(avctx, AV_LOG_ERROR, "Failed to queue input buffer (status = %d)\n", status);
            return AVERROR_EXTERNAL;
        }

        s->queued_buffer_nb++;
        if (s->queued_buffer_nb > s->queued_buffer_max)
            s->queued_buffer_max = s->queued_buffer_nb;
    }

    index = ff_AMediaCodec_dequeueOutputBuffer(codec, &info, s->dequeued_buffer_nb == 0 ? 0 : output_dequeue_timeout_us);
    if (index >= 0) {
        int ret;

        if (!s->first_buffer++) {
            av_log(avctx, AV_LOG_DEBUG, "Got first buffer after %fms\n", (av_gettime() - s->first_buffer_at) / 1000);
        }

        av_log(avctx, AV_LOG_DEBUG, "Got output buffer %zd"
                " offset=%" PRIi32 " size=%" PRIi32 " ts=%" PRIi64
                " flags=%" PRIu32 "\n", index, info.offset, info.size,
                info.presentationTimeUs, info.flags);

        if (s->surface) {
            data = NULL;
            size = 0;
        } else {
            data = ff_AMediaCodec_getOutputBuffer(codec, index, &size);
            if (!data) {
                av_log(avctx, AV_LOG_ERROR, "Failed to get output buffer\n");
                return AVERROR_EXTERNAL;
            }
        }

        if ((ret = mediacodec_buffer_create(avctx, s, data, size, index, &info, frame)) < 0) {
            return ret;
        }

        *got_frame = 1;
        s->queued_buffer_nb--;
        s->dequeued_buffer_nb++;

    } else if (ff_AMediaCodec_infoOutputFormatChanged(codec, index)) {
        char *format = NULL;

        if (s->format) {
            status = ff_AMediaFormat_delete(s->format);
            if (status < 0) {
                av_log(avctx, AV_LOG_ERROR, "Failed to delete MediaFormat %p\n", s->format);
            }
        }

        s->format = ff_AMediaCodec_getOutputFormat(codec);
        if (!s->format) {
            av_log(avctx, AV_LOG_ERROR, "Failed to get output format\n");
            return AVERROR_EXTERNAL;
        }

        format = ff_AMediaFormat_toString(s->format);
        if (!format) {
            return AVERROR_EXTERNAL;
        }
        av_log(avctx, AV_LOG_INFO, "Output MediaFormat changed to %s\n", format);
        av_freep(&format);

        if ((ret = mediacodec_dec_parse_format(avctx, s)) < 0) {
            return ret;
        }

    } else if (ff_AMediaCodec_infoOutputBuffersChanged(codec, index)) {
        ff_AMediaCodec_cleanOutputBuffers(codec);
    } else if (ff_AMediaCodec_infoTryAgainLater(codec, index)) {
        av_log(avctx, AV_LOG_DEBUG, "No output buffer available, try again later\n");
    } else {
        av_log(avctx, AV_LOG_ERROR, "Failed to dequeue output buffer (status=%zd)\n", index);
        return AVERROR_EXTERNAL;
    }

    return offset;
}

int ff_mediacodec_dec_flush(AVCodecContext *avctx, MediaCodecDecContext *s)
{
    FFAMediaCodec *codec = s->codec_ref->codec;
    int status;

    s->queued_buffer_nb = 0;
    s->dequeued_buffer_nb = 0;

    status = ff_AMediaCodec_flush(codec);
    if (status < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to flush MediaCodec %p", codec);
        return AVERROR_EXTERNAL;
    }

    s->first_buffer = 0;
    s->first_buffer_at = av_gettime();

    return 0;
}

int ff_mediacodec_dec_close(AVCodecContext *avctx, MediaCodecDecContext *s)
{
    mediacodec_ref_unref(&s->codec_ref);

    if (s->format) {
        ff_AMediaFormat_delete(s->format);
        s->format = NULL;
    }

    if (s->surface) {
        ANativeWindow_release(s->surface);
        s->surface = NULL;
    }

    return 0;
}
