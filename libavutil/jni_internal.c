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

#include "bprint.h"
#include "config.h"
#include "jni.h"
#include "jni_internal.h"
#include "log.h"

#include <jni.h>
#include <stdlib.h>

extern JavaVM *java_vm;

extern jobject application_context;

extern jobject application_class_loader;
extern jmethodID find_class_id;

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

char *avpriv_jni_jstring_to_utf_chars(JNIEnv *env, jstring string, void *log_ctx)
{
    char *ret = NULL;
    const char *utf_chars = NULL;

    jboolean copy = 0;

    if (!string) {
        return NULL;
    }

    utf_chars = (*env)->GetStringUTFChars(env, string, &copy);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        av_log(log_ctx, AV_LOG_ERROR, "String.getStringUTFChars() threw an exception\n");
        return NULL;
    }

    ret = av_strdup(utf_chars);

    (*env)->ReleaseStringUTFChars(env, string, utf_chars);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        av_log(log_ctx, AV_LOG_ERROR, "String.releaseStringUTFChars() threw an exception\n");
        return NULL;;
    }

    return ret;
}

jstring avpriv_jni_utf_chars_to_jstring(JNIEnv *env, const char *utf_chars, void *log_ctx)
{
    jstring ret;

    ret = (*env)->NewStringUTF(env, utf_chars);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        av_log(log_ctx, AV_LOG_ERROR, "NewStringUTF() threw an exception\n");
        return NULL;
    }

    return ret;
}

int avpriv_jni_exception_get_summary(JNIEnv *env, jthrowable exception, char **error, void *log_ctx)
{
    int ret = 0;

    AVBPrint bp;

    char *name = NULL;
    char *message = NULL;

    jclass class_class = NULL;
    jmethodID get_name_id = NULL;

    jclass exception_class = NULL;
    jmethodID get_message_id = NULL;

    jstring string;

    av_bprint_init(&bp, 0, AV_BPRINT_SIZE_AUTOMATIC);

    exception_class = (*env)->GetObjectClass(env, exception);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        av_log(log_ctx, AV_LOG_ERROR, "Could not find Throwable class\n");
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    class_class = (*env)->GetObjectClass(env, exception_class);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        av_log(log_ctx, AV_LOG_ERROR, "Could not find Throwable class's class\n");
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    get_name_id = (*env)->GetMethodID(env, class_class, "getName", "()Ljava/lang/String;");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        av_log(log_ctx, AV_LOG_ERROR, "Could not find method Class.getName()\n");
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    string = (*env)->CallObjectMethod(env, exception_class, get_name_id);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        av_log(log_ctx, AV_LOG_ERROR, "Class.getName() threw an exception\n");
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    if (string) {
        name = avpriv_jni_jstring_to_utf_chars(env, string, log_ctx);
        (*env)->DeleteLocalRef(env, string);
        string = NULL;
    }

    get_message_id = (*env)->GetMethodID(env, exception_class, "getMessage", "()Ljava/lang/String;");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        av_log(log_ctx, AV_LOG_ERROR, "Could not find method java/lang/Throwable.getMessage()\n");
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    string = (*env)->CallObjectMethod(env, exception, get_message_id);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        av_log(log_ctx, AV_LOG_ERROR, "Throwable.getMessage() threw an exception\n");
        ret = AVERROR_EXTERNAL;
        goto done;
    }

    if (string) {
        message = avpriv_jni_jstring_to_utf_chars(env, string, log_ctx);
        (*env)->DeleteLocalRef(env, string);
        string = NULL;
    }

    if (name && message) {
        av_bprintf(&bp, "%s: %s", name, message);
    } else if (name && !message) {
        av_bprintf(&bp, "%s occured", name);
    } else if (!name && message) {
        av_bprintf(&bp, "Exception: %s", message);
    } else {
        av_log(log_ctx, AV_LOG_WARNING, "Could not retreive exception name and message\n");
        av_bprintf(&bp, "Exception occured");
    }

    ret = av_bprint_finalize(&bp, error);
done:

    av_free(name);
    av_free(message);

    if (class_class) {
        (*env)->DeleteLocalRef(env, class_class);
    }

    if (exception_class) {
        (*env)->DeleteLocalRef(env, exception_class);
    }

    if (string) {
        (*env)->DeleteLocalRef(env, string);
    }

    return ret;
}

int avpriv_jni_exception_check(JNIEnv *env, int log, void *log_ctx)
{
    int ret;

    jthrowable exception;

    char *message = NULL;

    if (!(*(env))->ExceptionCheck((env))) {
        return 0;
    }

    if (!log) {
        (*(env))->ExceptionClear((env));
        return AVERROR_EXTERNAL;
    }

    exception = (*env)->ExceptionOccurred(env);
    (*(env))->ExceptionClear((env));

    if ((ret = avpriv_jni_exception_get_summary(env, exception, &message, log_ctx)) < 0) {
        (*env)->DeleteLocalRef(env, exception);
        return ret;
    }

    (*env)->DeleteLocalRef(env, exception);

    av_log(log_ctx, AV_LOG_ERROR, "%s\n", message);
    av_free(message);

    return AVERROR_EXTERNAL;
}

int avpriv_jni_init_jfields(JNIEnv *env, void *jfields, const struct FFJniField *jfields_mapping, int global, void *log_ctx)
{
    int i, ret = 0;
    jclass last_clazz = NULL;

    for (i = 0; jfields_mapping[i].name; i++) {
        int mandatory = jfields_mapping[i].mandatory;
        enum FFJniFieldType type = jfields_mapping[i].type;

        if (type == FF_JNI_CLASS || type == FF_JNI_APPLICATION_CLASS) {
            jclass clazz;

            last_clazz = NULL;

            if  (application_class_loader && type == FF_JNI_APPLICATION_CLASS) {

                jobject tmp = avpriv_jni_utf_chars_to_jstring(env, jfields_mapping[i].name, log_ctx);
                if (!tmp) {
                    ret = AVERROR_EXTERNAL;
                    goto done;
                }

                clazz = (*env)->CallObjectMethod(env, application_class_loader, find_class_id, tmp);
                if ((ret = avpriv_jni_exception_check(env, 1, log_ctx)) < 0 && mandatory) {
                    goto done;
                }

                (*env)->DeleteLocalRef(env, tmp);

            } else {
                clazz = (*env)->FindClass(env, jfields_mapping[i].name);
                if ((ret = avpriv_jni_exception_check(env, mandatory, log_ctx)) < 0 && mandatory) {
                    goto done;
                }
            }

            last_clazz = *(jclass*)((uint8_t*)jfields + jfields_mapping[i].offset) =
                    global ? (*env)->NewGlobalRef(env, clazz) : clazz;
        } else {

            if (!last_clazz) {
                ret = AVERROR_EXTERNAL;
                break;
            }

            switch(type) {
            case (FF_JNI_FIELD): {
                jfieldID field_id = (*env)->GetFieldID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = avpriv_jni_exception_check(env, mandatory, log_ctx)) < 0 && mandatory) {
                    goto done;
                }

                *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = field_id;
                break;
            }
            case (FF_JNI_STATIC_FIELD): {
                jfieldID field_id = (*env)->GetStaticFieldID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = avpriv_jni_exception_check(env, mandatory, log_ctx)) < 0 && mandatory) {
                    goto done;
                }

                *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = field_id;
                break;
            }
            case (FF_JNI_METHOD): {
                jmethodID method_id = (*env)->GetMethodID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = avpriv_jni_exception_check(env, mandatory, log_ctx)) < 0 && mandatory) {
                    goto done;
                }

                *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = method_id;
                break;
            }
            case (FF_JNI_STATIC_METHOD): {
                jmethodID method_id = (*env)->GetStaticMethodID(env, last_clazz, jfields_mapping[i].method, jfields_mapping[i].signature);
                if ((ret = avpriv_jni_exception_check(env, mandatory, log_ctx)) < 0 && mandatory) {
                    goto done;
                }

                *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = method_id;
                break;
            }
            default:
                av_log(log_ctx, AV_LOG_ERROR, "Unknown JNI field type\n");
                ret = AVERROR(EINVAL);
                goto done;
            }
        }
    }

done:
    if (ret < 0) {
        /* reset jfields in case of failure so it does not leak references */
        avpriv_jni_reset_jfields(env, jfields, jfields_mapping, global, log_ctx);
    }

    return ret;
}

int avpriv_jni_reset_jfields(JNIEnv *env, void *jfields, const struct FFJniField *jfields_mapping, int global, void *log_ctx)
{
    int i;

    for (i = 0; jfields_mapping[i].name; i++) {
        enum FFJniFieldType type = jfields_mapping[i].type;

        switch(type) {
        case (FF_JNI_CLASS): {
            jclass clazz = *(jclass*)((uint8_t*)jfields + jfields_mapping[i].offset);
            if (!clazz)
                continue;

            if (global) {
                (*env)->DeleteGlobalRef(env, clazz);
            } else {
                (*env)->DeleteLocalRef(env, clazz);
            }

            *(jclass*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case (FF_JNI_FIELD): {
            *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case (FF_JNI_STATIC_FIELD): {
            *(jfieldID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case (FF_JNI_METHOD): {
            *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        case (FF_JNI_STATIC_METHOD): {
            *(jmethodID*)((uint8_t*)jfields + jfields_mapping[i].offset) = NULL;
            break;
        }
        default:
            av_log(log_ctx, AV_LOG_ERROR, "Unknown JNI field type\n");
        }
    }

    return 0;
}

jclass avpriv_jni_find_application_class(JNIEnv *env, const char *name, void *log_ctx)
{
    jobject ret;
    jobject tmp;

    if (!application_class_loader || !find_class_id) {
        av_log(log_ctx, AV_LOG_ERROR, "No application class loader has been registered\n");
        return NULL;
    }

    tmp = avpriv_jni_utf_chars_to_jstring(env, name, log_ctx);
    if (!tmp) {
        return NULL;
    }

    ret = (*env)->CallObjectMethod(env, application_class_loader, find_class_id, tmp);
    if (avpriv_jni_exception_check(env, 1, log_ctx) < 0) {
        ret = NULL;
    }

    (*env)->DeleteLocalRef(env, tmp);

    return ret;
}
