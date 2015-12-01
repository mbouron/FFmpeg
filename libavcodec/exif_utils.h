/*
 * Exif metadata utils
 * Copyright (c) 2015 Matthieu Bouron <matthieu.bouron stupeflix.com>
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

/**
 * @file
 * Exif metadata parser
 */

#ifndef AVCODEC_EXIF_UTILS_H
#define AVCODEC_EXIF_UTILS_H

#include <stdint.h>

#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>

/*
 * Extract the Exif metadata from a buffer.
 *
 * @param data Exif buffer to parse
 * @param size Exif buffer size
 * @param metadata destination metadata address
 * @param log_ctx context used for logging, can be NULL
 * @return number of bytes read, < 0 in case of error
 */
int av_exif_parse(const uint8_t *data, int size, AVDictionary **metadata, void *log_ctx);

#endif /* AVCODEC_EXIF_UTILS_H */
