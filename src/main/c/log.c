/*
 * log.c
 *
 *  Created on: Nov 23, 2013
 *      Author: Wayne Carter
 */

#include "log.h"
#include <string.h>

#define LOG_TAG "StorageEngine"
#define LOG_BUF_SIZE 1024

#define LOG_LEVEL_W 1
#define LOG_LEVEL_E 2

static void _log(JNIEnv * env, int logLevel, const char * fmt, va_list ap)
{
	static jclass LogClass;
	if (!LogClass) {
		jclass clazz = (*env)->FindClass(env, "com/couchbase/lite/util/Log");

		if (clazz != NULL) {
			LogClass = (jclass)((*env)->NewGlobalRef(env, clazz));
		} else {
			return;
		}
	}

	static jstring logTagString;
	if (!logTagString) logTagString = (*env)->NewStringUTF(env, LOG_TAG);

	char buf[LOG_BUF_SIZE];
	vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
	jstring msg = (*env)->NewStringUTF(env, buf);

	if (logLevel == LOG_LEVEL_W) {
		static jmethodID wMethod;
		if (!wMethod) wMethod = (*env)->GetStaticMethodID(env, LogClass, "w", "(Ljava/lang/String;Ljava/lang/String;)V");

		(*env)->CallStaticVoidMethod(env, LogClass, wMethod, logTagString, msg);
	} else if (logLevel == LOG_LEVEL_E) {
		static jmethodID eMethod;
		if (!eMethod) eMethod = (*env)->GetStaticMethodID(env, LogClass, "e", "(Ljava/lang/String;Ljava/lang/String;)V");

		(*env)->CallStaticVoidMethod(env, LogClass, eMethod, logTagString, msg);
	}

	(*env)->DeleteLocalRef(env, msg);
}

void log_w(JNIEnv * env, const char * fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_log(env, LOG_LEVEL_W, fmt, ap);
	va_end(ap);
}

void log_e(JNIEnv * env, const char * fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_log(env, LOG_LEVEL_E, fmt, ap);
	va_end(ap);
}
