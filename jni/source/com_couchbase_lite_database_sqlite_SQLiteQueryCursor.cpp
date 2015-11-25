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

#include "sqlite3.h"

#include "com_couchbase_lite_database_sqlite_SQLiteQueryCursor.h"

#include "sqlite_common.h"

JNIEXPORT jboolean JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteQueryCursor_nativeMoveToNext
(JNIEnv* env, jclass clazz, jlong statementPtr) {
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    int err = sqlite3_step(statement);
    if (err == SQLITE_ROW) {
        return true;
    } else if (err == SQLITE_DONE) {
        return false;
    } else {
        throw_sqlite3_exception(env, sqlite3_db_handle(statement), NULL);
    }
    return false;
}

JNIEXPORT jstring JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteQueryCursor_nativeGetString
(JNIEnv* env, jclass clazz, jlong statementPtr, jint columnIndex) {
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    if (sqlite3_column_type(statement, columnIndex) == SQLITE_NULL) {
        return NULL;
    }

    const char* value = (const char*)sqlite3_column_text(statement, columnIndex);
    if (!value) {
        return NULL;
    }
    return env->NewStringUTF(value);
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteQueryCursor_nativeGetInt
(JNIEnv* env, jclass clazz, jlong statementPtr, jint columnIndex) {
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    return sqlite3_column_int(statement, columnIndex);
}

JNIEXPORT jlong JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteQueryCursor_nativeGetLong
(JNIEnv* env, jclass clazz, jlong statementPtr, jint columnIndex) {
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    return sqlite3_column_int64(statement, columnIndex);
}

JNIEXPORT jdouble JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteQueryCursor_nativeGetDouble
(JNIEnv* env, jclass clazz, jlong statementPtr, jint columnIndex) {
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    return sqlite3_column_double(statement, columnIndex);
}

JNIEXPORT jbyteArray JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteQueryCursor_nativeGetBlob
(JNIEnv* env, jclass clazz, jlong statementPtr, jint columnIndex) {
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    if (sqlite3_column_type(statement, columnIndex) == SQLITE_NULL) {
        return NULL;
    }

    const void* value = sqlite3_column_blob(statement, columnIndex);
    int size = sqlite3_column_bytes(statement, columnIndex);
    jbyteArray byteArray = env->NewByteArray(size);
    if (!byteArray) {
        env->ExceptionClear();
        throw_sqlite3_exception(env, "Native could not create new byte[]");
        return NULL;
    }
    env->SetByteArrayRegion(byteArray, 0, size, static_cast<const jbyte*>(value));
    return byteArray;
}

JNIEXPORT jboolean JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteQueryCursor_nativeIsNull
(JNIEnv* env, jclass clazz, jlong statementPtr, jint columnIndex) {
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    return sqlite3_column_type(statement, columnIndex) == SQLITE_NULL;
}
