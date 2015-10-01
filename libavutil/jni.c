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
#include <pthread.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

JavaVM *java_vm = NULL;

jobject application_context = NULL;

jobject application_class_loader = NULL;
jmethodID find_class_id = NULL;

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

#ifdef __ANDROID__
int av_jni_register_application_context(JNIEnv *env, jobject context)
{
    int ret = 0;

    jclass application_context_class;
    jmethodID get_class_loader_id;

    jclass application_class_loader_class;

    pthread_mutex_lock(&lock);

    if (application_context && application_class_loader) {
        pthread_mutex_unlock(&lock);

        av_log(NULL, AV_LOG_INFO, "The application context has already been registered\n");
        return ret;
    }

    application_context_class = (*env)->GetObjectClass(env, context);
    if ((ret = avpriv_jni_exception_check(env, 1, NULL)) < 0) {
        goto done;
    }

    get_class_loader_id = (*env)->GetMethodID(env, application_context_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if ((ret = avpriv_jni_exception_check(env, 1, NULL)) < 0) {
        goto done;
    }

    application_context = (*env)->NewGlobalRef(env, context);
    application_class_loader = (*env)->CallObjectMethod(env, application_context, get_class_loader_id);
    if ((ret = avpriv_jni_exception_check(env, 1, NULL)) < 0) {
        goto done;
    }

    application_class_loader = (*env)->NewGlobalRef(env, application_class_loader);
    if ((ret = avpriv_jni_exception_check(env, 1, NULL)) < 0) {
        goto done;
    }

    application_class_loader_class = (*env)->GetObjectClass(env, application_class_loader);
    if ((ret = avpriv_jni_exception_check(env, 1, NULL)) < 0) {
        goto done;
    }

    find_class_id = (*env)->GetMethodID(env, application_class_loader_class, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if ((ret = avpriv_jni_exception_check(env, 1, NULL)) < 0) {
        goto done;
    }

done:
    if (application_context_class) {
        (*env)->DeleteLocalRef(env, application_context_class);
    }

    if (application_class_loader_class) {
        (*env)->DeleteLocalRef(env, application_class_loader_class);
    }

    if (ret != 0) {

        if (application_context) {
            (*env)->DeleteGlobalRef(env, application_context);
            application_context = NULL;
        }

        if (application_class_loader) {
            (*env)->DeleteGlobalRef(env, application_class_loader);
            application_context = NULL;
        }
    }

    pthread_mutex_unlock(&lock);

    return ret;
}

jobject av_jni_get_application_context(void)
{
    return application_context;
}

int av_jni_unregister_application_context(JNIEnv *env)
{
    pthread_mutex_lock(&lock);

    if (application_context) {
        (*env)->DeleteGlobalRef(env, application_context);
        application_context = NULL;
    }

    if (application_class_loader) {
        (*env)->DeleteGlobalRef(env, application_class_loader);
        application_class_loader = NULL;
    }

    pthread_mutex_unlock(&lock);

    return 0;
}

#endif
