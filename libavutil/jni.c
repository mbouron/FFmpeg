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
#include "log.h"

#include <jni.h>
#include <stdlib.h>
#include <pthread.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

JavaVM *java_vm = NULL;

void av_jni_register_java_vm(JavaVM *vm)
{
    pthread_mutex_lock(&lock);
    if (java_vm) {
        av_log(NULL, AV_LOG_INFO, "The Java VM has already been registered\n");
        goto done;
    }

    java_vm = vm;
done:
    pthread_mutex_unlock(&lock);
}

JavaVM *av_jni_get_java_vm(void)
{
    JavaVM *vm;

    pthread_mutex_lock(&lock);
    vm = java_vm;
    pthread_mutex_unlock(&lock);

    return vm;
}
