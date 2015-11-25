//
//  Copyright (c) 2015 Couchbase, Inc. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//

#ifndef SQLITE_LOG_H
#define SQLITE_LOG_H

#ifdef DEBUG
static bool shouldLog = false;
#else
static bool shouldLog = false;
#endif

#define SQLITE_LOG_TAG "SQLiteLog"
#define SQLITE_TRACE_TAG "SQLiteStatements"
#define SQLITE_PROFILE_TAG "SQLiteTime"
#define SQLITE_COLLATOR_TAG "SQLiteCollator"

#ifdef __ANDROID__
#include <android/log.h>
#define LOGD(LOG_TAG,...) if (shouldLog) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGV(LOG_TAG,...) if (shouldLog) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGE(LOG_TAG,...) if (shouldLog) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGD(LOG_TAG,...) if (shouldLog) fprintf(stderr, __VA_ARGS__)
#define LOGV(LOG_TAG,...) if (shouldLog) fprintf(stderr, __VA_ARGS__)
#define LOGE(LOG_TAG,...) if (shouldLog) fprintf(stderr, __VA_ARGS__)
#endif

#endif //SQLITE_LOG_H
