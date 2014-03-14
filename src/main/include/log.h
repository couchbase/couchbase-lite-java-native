/*
 * log.h
 *
 *  Created on: Nov 23, 2013
 *      Author: Wayne Carter
 */

#include <jni.h>

#ifndef LOG_H_
#define LOG_H_
#ifdef __cplusplus
extern "C" {
#endif

void log_w(JNIEnv * env, const char * fmt, ...);

void log_e(JNIEnv * env, const char * fmt, ...);

#ifdef __cplusplus
}
#endif
#endif
