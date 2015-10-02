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

/*
 * Convert a jstring to its utf characters equivalent.
 *
 * @param env JNI environment
 * @param string java string to convert
 * @param log_ctx context used for logging, can be NULL
 * @return a pointer to an array of unicode caracters on success, NULL
 * otherwise
 */
char *avpriv_jni_jstring_to_utf_chars(JNIEnv *env, jstring string, void *log_ctx);

/*
 * Convert utf chars to its jstring equivalent.
 *
 * @param env JNI environment
 * @param utf_chars a pointer to an array of unicode caracters
 * @param log_ctx context used for logging, can be NULL
 * @return a java string object on success, NULL otherwise
 */
jstring avpriv_jni_utf_chars_to_jstring(JNIEnv *env, const char *utf_chars, void *log_ctx);

/*
 * Extract the error summary from a jthrowable in the form of "className: errorMesage"
 *
 * @param env JNI environment
 * @param exception exception to get the summary from
 * @param error address pointing to the error, the value is updated if a
 * summary can be extracted
 * @param log_ctx context used for logging, can be NULL
 * @return 0 on success, < 0 otherwise
 */
int avpriv_jni_exception_get_summary(JNIEnv *env, jthrowable exception, char **error, void *log_ctx);

/*
 * Check if an exception has occurred,log it using av_log and clear it.
 *
 * @param env JNI environment
 * @param log value used to enable logging if an exception has occured,
 * 0 disables logging, != 0 enables logging
 * @param log_ctx context used for logging, can be NULL
 */
int avpriv_jni_exception_check(JNIEnv *env, int log, void *log_ctx);

/*
 * Jni field type.
 */
enum FFJniFieldType {

    FF_JNI_CLASS,
    FF_JNI_FIELD,
    FF_JNI_STATIC_FIELD,
    FF_JNI_METHOD,
    FF_JNI_STATIC_METHOD

} MemberType;

/*
 * Jni field describing a class, a field or a method to be retrived using
 * the avpriv_jni_init_jfields method.
 */
struct FFJniField {

    const char *name;
    const char *method;
    const char *signature;
    enum FFJniFieldType type;
    int offset;
    int mandatory;

};

/*
 * Retreive class references, field ids and method ids to an arbitrary structure.
 *
 * @param env JNI environment
 * @param jfields a pointer to an arbitrary structure where the different
 * fields are declared and where the FFJNIField mapping table offsets are
 * pointing to
 * @param jfields_mapping null terminated array of FFJNIFields describing
 * the class/field/method to be retrived
 * @param global, wheter or not to make the classes reference global, it is
 * the caller responsability to properly release global references.
 * @param log_ctx context used for logging, can be NULL
 * @return 0 on success, < 0 otherwise
 */
int avpriv_jni_init_jfields(JNIEnv *env, void *jfields, const struct FFJniField *jfields_mapping, int global, void *log_ctx);

/*
 * Delete class references, field ids and method ids of an arbitrary structure.
 *
 * @param env JNI environment
 * @param jfields a pointer to an arbitrary structure where the different
 * fields are declared and where the FFJNIField mapping table offsets are
 * pointing to
 * @param jfields_mapping null terminated array of FFJNIFields describing
 * the class/field/method to be deleted
 * @param global, wheter or not the classes reference are global
 * @param log_ctx context used for logging, can be NULL
 * @return 0 on success, < 0 otherwise
 */
int avpriv_jni_reset_jfields(JNIEnv *env, void *jfields, const struct FFJniField *jfields_mapping, int global, void *log_ctx);

#endif /* AVUTIL_JNI_INTERNAL_H */
