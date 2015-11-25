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

#include <stdio.h>

#ifndef __ANDROID__
#include "sqlite3.h"
#endif

#include "sqlite_log.h"
#include "com_couchbase_lite_database_sqlite_SQLiteDatabase.h"

JNIEXPORT jboolean JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteDatabase_nativeSupportEncryption
(JNIEnv* env, jclass clazz) {
#ifdef __ANDROID__
// PROBLEM: Not every system's SQLite on Android is built with sqlite3_compileoption_used() function included.
// WORKAROUND: Use SQLITE_HAS_CODEC compile option to detect this.
// TODO: See if there is a standard way of doing this without querying the database.
#ifdef SQLITE_HAS_CODEC
    return true;
#else
    return false;
#endif
#else
    return sqlite3_compileoption_used("SQLITE_HAS_CODEC") != 0;
#endif
}
