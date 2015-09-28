/*
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

#ifndef AVUTIL_JNI_INTERNAL_H
#define AVUTIL_JNI_INTERNAL_H

#include <jni.h>

/*
 * Attach a JNI environment to the current thread.
 *
 * @param attached pointer to an integer that will be set to 1 if the
 * environment has been attached to the current thread or 0 if it is
 * already attached.
 * @param log_ctx context used for logging, can be NULL
 * @return the JNI environment on success, NULL otherwise
 */
JNIEnv *avpriv_jni_attach_env(int *attached, void *log_ctx);

/*
 * Detach the JNI environment from the current thread.
 *
 * @param log_ctx context used for logging, can be NULL
 * @return 0 on success, < 0 otherwise
 */
int avpriv_jni_detach_env(void *log_ctx);

#endif /* AVUTIL_JNI_INTERNAL_H */
