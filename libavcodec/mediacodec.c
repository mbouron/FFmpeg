/*
 * Android MediaCodec public API functions
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

#include "config.h"

#include "libavcodec/avcodec.h"
#include "libavutil/mem.h"

#include "libavcodec/mediacodec.h"

AVMediaCodecContext *av_mediacodec_alloc_context(void)
{
    return av_mallocz(sizeof(AVMediaCodecContext));
}

int av_mediacodec_default_init(AVCodecContext *avctx, AVMediaCodecContext *ctx)
{
    avctx->hwaccel_context = ctx ? ctx : av_mediacodec_alloc_context();
    if (!avctx->hwaccel_context)
        return AVERROR(ENOMEM);

    return 0;
}

void av_mediacodec_default_free(AVCodecContext *avctx)
{
    AVMediaCodecContext *ctx = avctx->hwaccel_context;

    if (ctx && ctx->surface) {
        ANativeWindow_release(ctx->surface);
    }

    av_freep(&avctx->hwaccel_context);
}
