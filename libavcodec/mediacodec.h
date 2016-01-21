/*
 * Android MediaCodec public API
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

#ifndef AVCODEC_MEDIACODEC_H
#define AVCODEC_MEDIACODEC_H

#include <android/native_window.h>

#include "libavcodec/avcodec.h"

/**
 * This structure holds a reference to a native window that will
 * be used as output by the decoder.
 *
 * The native window reference is *owned* by the structure and will be
 * released when av_mediacodec_default_free() is called.
 */
typedef struct AVMediaCodecContext {

    /**
     * Native window reference created by the user.
     * The reference is created by the user using ANativeWindow_acquire()
     * and will be released when it is not used anymore by the decoder and
     * its output frames.
     */
    ANativeWindow *surface;

} AVMediaCodecContext;

/**
 * Allocate and initialize a MediaCodec context.
 *
 * When decoding with MediaCodec is finished, the caller must free the
 * MediaCodec context with av_mediacodec_default_free.
 */
AVMediaCodecContext *av_mediacodec_alloc_context(void);


/**
 * Convenience function that sets up the MediaContext context.
 */
int av_mediacodec_default_init(AVCodecContext *avctx, AVMediaCodecContext *ctx);

/**
 * This function must be called to free the MediaCodec context initialized with
 * av_mediacodec_default_init().
 */
void av_mediacodec_default_free(AVCodecContext *avctx);

#endif /* AVCODEC_MEDIACODEC_H */
