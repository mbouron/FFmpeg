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

#include "config.h"
#include "jni.h"
#include "jni_internal.h"
#include "log.h"

#include <jni.h>
#include <stdlib.h>

extern JavaVM *java_vm;

JNIEnv *avpriv_jni_attach_env(int *attached, void *log_ctx)
{
    int ret = 0;
    JNIEnv *env = NULL;

    *attached = 0;

    if (java_vm == NULL) {
        av_log(log_ctx, AV_LOG_ERROR, "No java virtual machine has been registered\n");
        return NULL;
    }

    ret = (*java_vm)->GetEnv(java_vm, (void **)&env, JNI_VERSION_1_6);
    switch(ret) {
    case JNI_EDETACHED:
        if ((*java_vm)->AttachCurrentThread(java_vm, &env, NULL) != 0) {
            av_log(log_ctx, AV_LOG_ERROR, "Failed to attach the JNI environment to the current thread\n");
            env = NULL;
        } else {
            *attached = 1;
        }
        break;
    case JNI_OK:
        break;
    case JNI_EVERSION:
        av_log(log_ctx, AV_LOG_ERROR, "The specified JNI version is not supported\n");
        break;
    default:
        av_log(log_ctx, AV_LOG_ERROR, "Failed to get the JNI environment attached to this thread");
        break;
    }

    return env;
}

int avpriv_jni_detach_env(void *log_ctx)
{
    if (java_vm == NULL) {
        av_log(log_ctx, AV_LOG_ERROR, "No java virtual machine has been registered\n");
        return AVERROR(EINVAL);
    }

    return (*java_vm)->DetachCurrentThread(java_vm);
}
