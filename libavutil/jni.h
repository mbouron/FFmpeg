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

#ifndef AVUTIL_JNI_H
#define AVUTIL_JNI_H

#include <jni.h>

/*
 * Register a java virtual machine that will be used to manage the JNI
 * environment.
 *
 * @param vm java virtual machine
 */
void av_jni_register_java_vm(JavaVM *vm);

/*
 * Get the registered java virtual machine.
 *
 * @return the java virtual machine, NULL if no java virtual machine has been
 * registered
 */
JavaVM *av_jni_get_java_vm(void);

#ifdef __ANDROID__
/*
 * Register an Android application context that will be used later on
 * to load application classes.
 *
 * @param env JNI environment
 * @param context application context
 * @return 0 on success, a negative number otherwise
 */
int av_jni_register_application_context(JNIEnv *env, jobject context);

/*
 * Get the registered android application context.
 *
 * @return the registered android application context
 */
jobject av_jni_get_application_context(void);

/*
 * Unregister the previously registered Android application context
 * and delete the global reference associated with it.
 *
 * @param env JNI environment
 * @return 0 on success, a negative number otherwise
 *
 */
int av_jni_unregister_application_context(JNIEnv *env);
#endif

#endif /* AVUTIL_JNI_H */
