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
#include <stdlib.h>
#include <string>
#include <cstring>

#include "sqlite3.h"

#include "com_couchbase_lite_database_sqlite_SQLiteConnection.h"
#include "sqlite_connection.h"
#include "sqlite_common.h"

/* Busy timeout in milliseconds.
 * If another connection (possibly in another process) has the database locked for
 * longer than this amount of time then SQLite will generate a SQLITE_BUSY error.
 * The SQLITE_BUSY error is then raised as a SQLiteDatabaseLockedException.
 *
 * In ordinary usage, busy timeouts are quite rare.  Most databases only ever
 * have a single open connection at a time unless they are using WAL.  When using
 * WAL, a timeout could occur if one connection is busy performing an auto-checkpoint
 * operation.  The busy timeout needs to be long enough to tolerate slow I/O write
 * operations but not so long as to cause the application to hang indefinitely if
 * there is a problem acquiring a database lock.
 */
static const int BUSY_TIMEOUT_MS = 2500;

// Called each time a statement begins execution, when tracing is enabled.
static void sqliteTraceCallback(void *data, const char *sql) {
    SQLiteConnection* connection = static_cast<SQLiteConnection*>(data);
    LOGV(SQLITE_TRACE_TAG, "%s: \"%s\"\n", connection->label, sql);
}

// Called each time a statement finishes execution, when profiling is enabled.
static void sqliteProfileCallback(void *data, const char *sql, sqlite3_uint64 tm) {
    SQLiteConnection* connection = static_cast<SQLiteConnection*>(data);
    LOGV(SQLITE_PROFILE_TAG, "%s: \"%s\" took %0.3f ms\n", connection->label, sql, tm * 0.000001f);
}

// Called after each SQLite VM instruction when cancelation is enabled.
static int sqliteProgressHandlerCallback(void* data) {
    SQLiteConnection* connection = static_cast<SQLiteConnection*>(data);
    return connection->canceled;
}

static bool startsWith(const char *str, const char *prefix) {
    if (str == NULL || prefix == NULL)
        return false;
    size_t strSize = strlen(str);
    size_t preSize = strlen(prefix);
    return preSize > 0 && strSize >= preSize && strncmp(str, prefix, preSize) == 0;
}

static char* lowercase(char* str) {
    for(unsigned int i = 0; str[i]; i++)
        str[i] = tolower(str[i]);
    return str;
}

JNIEXPORT jlong JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeOpen
(JNIEnv* env, jclass clazz, jstring pathStr, jint openFlags, jstring labelStr, jboolean enableTrace, jboolean enableProfile) {
    int sqliteFlags;
    if (openFlags & SQLiteConnection::CREATE_IF_NECESSARY) {
        sqliteFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    } else if (openFlags & SQLiteConnection::OPEN_READONLY) {
        sqliteFlags = SQLITE_OPEN_READONLY;
    } else {
        sqliteFlags = SQLITE_OPEN_READWRITE;
    }

    const char* pathCStr = env->GetStringUTFChars(pathStr, NULL);
    std::string path(pathCStr);
    env->ReleaseStringUTFChars(pathStr, pathCStr);

    const char* labelCStr = env->GetStringUTFChars(labelStr, NULL);
    std::string label(labelCStr);
    env->ReleaseStringUTFChars(labelStr, labelCStr);
    
    sqlite3* db;
    int err = sqlite3_open_v2(path.c_str(), &db, sqliteFlags, NULL);
    if (err != SQLITE_OK) {
        LOGE(SQLITE_LOG_TAG, "sqlite3_open_v2 failed PATH: %s", path.c_str());
        throw_sqlite3_exception_errcode(env, err, "Could not open database");
        return 0;
    }
    
    // Check that the database is really read/write when that is what we asked for.
    if ((sqliteFlags & SQLITE_OPEN_READWRITE) && sqlite3_db_readonly(db, NULL)) {
        throw_sqlite3_exception(env, db, "Could not open the database in read/write mode.");
        sqlite3_close(db);
        return 0;
    }
    
    // Set the default busy handler to retry automatically before returning SQLITE_BUSY.
    err = sqlite3_busy_timeout(db, BUSY_TIMEOUT_MS);
    if (err != SQLITE_OK) {
        throw_sqlite3_exception(env, db, "Could not set busy timeout");
        sqlite3_close(db);
        return 0;
    }

    // Create wrapper object.
    SQLiteConnection* connection = new SQLiteConnection(db, openFlags, path.c_str(), label.c_str());
    
    // Enable tracing and profiling if requested.
    if (enableTrace) {
        sqlite3_trace(db, &sqliteTraceCallback, connection);
    }
    if (enableProfile) {
        sqlite3_profile(db, &sqliteProfileCallback, connection);
    }

    LOGV(SQLITE_LOG_TAG, "SQLITE VERSION %s", sqlite3_libversion());
    LOGV(SQLITE_LOG_TAG, "Opened connection %p with label '%s'", db, label.c_str());
    return reinterpret_cast<jlong>(connection);
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeClose
(JNIEnv* env, jclass clazz, jlong connectionPtr) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    
    if (connection) {
        LOGV(SQLITE_LOG_TAG, "Closing connection %p", connection->db);

        // Close database:
        int err = sqlite3_close(connection->db);
        if (err != SQLITE_OK) {
            // This can happen if sub-objects aren't closed first.  Make sure the caller knows.
            LOGE(SQLITE_LOG_TAG, "sqlite3_close(%p) failed: %d", connection->db, err);
            throw_sqlite3_exception(env, connection->db, "Count not close db.");
            return;
        }
        
        delete connection;
    }
}

JNIEXPORT jlong JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativePrepareStatement
(JNIEnv* env, jclass clazz, jlong connectionPtr, jstring sqlString) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    
    jsize sqlLength = env->GetStringLength(sqlString);
    const jchar* sql = env->GetStringCritical(sqlString, NULL);
    sqlite3_stmt* statement;
    int err = sqlite3_prepare16_v2(connection->db,
                                   sql, sqlLength * sizeof(jchar), &statement, NULL);
    env->ReleaseStringCritical(sqlString, sql);
    
    if (err != SQLITE_OK) {
        // Error messages like 'near ")": syntax error' are not
        // always helpful enough, so construct an error string that
        // includes the query itself.
        const char *query = env->GetStringUTFChars(sqlString, NULL);
        char *message = (char*) malloc(strlen(query) + 50);
        if (message) {
            strcpy(message, ", while compiling: "); // less than 50 chars
            strcat(message, query);
        }
        env->ReleaseStringUTFChars(sqlString, query);
        throw_sqlite3_exception(env, connection->db, message);
        free(message);
        return 0;
    }

    //LOGV(SQLITE_LOG_TAG,"Prepared statement %p on connection %p", statement, connection->db);
    return reinterpret_cast<jlong>(statement);
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeFinalizeStatement
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    // We ignore the result of sqlite3_finalize because it is really telling us about
    // whether any errors occurred while executing the statement.  The statement itself
    // is always finalized regardless.
    //LOGV(SQLITE_LOG_TAG, "Finalized statement %p on connection %p", statement, connection->db);
    sqlite3_finalize(statement);
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeGetParameterCount
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr) {
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    return sqlite3_bind_parameter_count(statement);
}

JNIEXPORT jboolean JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeIsReadOnly
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr) {
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    return sqlite3_stmt_readonly(statement) != 0;
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeGetColumnCount
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr) {
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    return sqlite3_column_count(statement);
}

JNIEXPORT jstring JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeGetColumnName
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr, jint index) {
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    const jchar* name = static_cast<const jchar*>(sqlite3_column_name16(statement, index));
    if (name) {
        size_t length = 0;
        while (name[length]) {
            length += 1;
        }
        return env->NewString(name, (int)length);
    }
    return NULL;
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeBindNull
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr, jint index) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    int err = sqlite3_bind_null(statement, index);
    if (err != SQLITE_OK) {
        throw_sqlite3_exception(env, connection->db, NULL);
    }
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeBindLong
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr, jint index, jlong value) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    int err = sqlite3_bind_int64(statement, index, value);
    if (err != SQLITE_OK) {
        throw_sqlite3_exception(env, connection->db, NULL);
    }
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeBindDouble
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr, jint index, jdouble value) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    int err = sqlite3_bind_double(statement, index, value);
    if (err != SQLITE_OK) {
        throw_sqlite3_exception(env, connection->db, NULL);
    }
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeBindString
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr, jint index, jstring valueString) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    jsize valueLength = env->GetStringLength(valueString);
    const jchar* value = env->GetStringCritical(valueString, NULL);
    int err = sqlite3_bind_text16(statement, index, value, valueLength * sizeof(jchar),
                                  SQLITE_TRANSIENT);
    env->ReleaseStringCritical(valueString, value);
    if (err != SQLITE_OK) {
        throw_sqlite3_exception(env, connection->db, NULL);
    }
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeBindBlob
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr, jint index, jbyteArray valueArray) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    jsize valueLength = env->GetArrayLength(valueArray);
    jbyte* value = static_cast<jbyte*>(env->GetPrimitiveArrayCritical(valueArray, NULL));
    int err = sqlite3_bind_blob(statement, index, value, valueLength, SQLITE_TRANSIENT);
    env->ReleasePrimitiveArrayCritical(valueArray, value, JNI_ABORT);
    if (err != SQLITE_OK) {
        throw_sqlite3_exception(env, connection->db, NULL);
    }}

JNIEXPORT void JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeResetStatementAndClearBindings
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    int err = sqlite3_reset(statement);
    if (err == SQLITE_OK) {
        err = sqlite3_clear_bindings(statement);
    }
    if (err != SQLITE_OK) {
        throw_sqlite3_exception(env, connection->db, NULL);
    }
}

static int executeNonQuery(JNIEnv* env, SQLiteConnection* connection, sqlite3_stmt* statement) {
    int err = sqlite3_step(statement);
    if (err == SQLITE_ROW) {
        const char *sql = sqlite3_sql(statement);
        if (sql) {
            // Allows PRAGMA and SELECT sqlcipher_export statement:
            if (startsWith(lowercase(strdup(sql)), "pragma") ||
                startsWith(lowercase(strdup(sql)), "select sqlcipher_export")) {
                err = SQLITE_OK;
            }
        }
        if (err != SQLITE_OK) {
            throw_sqlite3_exception(env,
                "Queries can be performed using SQLiteDatabase query or rawQuery methods only.");
        }
    } else if (err != SQLITE_DONE) {
        throw_sqlite3_exception(env, connection->db);
    }
    return err;
}

static int executeOneRowQuery(JNIEnv* env, SQLiteConnection* connection, sqlite3_stmt* statement) {
    int err = sqlite3_step(statement);
    if (err != SQLITE_ROW) {
        throw_sqlite3_exception(env, connection->db);
    }
    return err;
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeExecute
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    executeNonQuery(env, connection, statement);
}

JNIEXPORT jlong JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeExecuteForLong
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    int err = executeOneRowQuery(env, connection, statement);
    if (err == SQLITE_ROW && sqlite3_column_count(statement) >= 1) {
        return sqlite3_column_int64(statement, 0);
    }
    return -1;
}

JNIEXPORT jstring JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeExecuteForString
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    int err = executeOneRowQuery(env, connection, statement);
    if (err == SQLITE_ROW && sqlite3_column_count(statement) >= 1) {
        const jchar* text = static_cast<const jchar*>(sqlite3_column_text16(statement, 0));
        if (text) {
            size_t length = sqlite3_column_bytes16(statement, 0) / sizeof(jchar);
            return env->NewString(text, (int)length);
        }
    }
    return NULL;
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeExecuteForChangedRowCount
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    int err = executeNonQuery(env, connection, statement);
    return err == SQLITE_DONE ? sqlite3_changes(connection->db) : -1;
}

JNIEXPORT jlong JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeExecuteForLastInsertedRowId
(JNIEnv* env, jclass clazz, jlong connectionPtr, jlong statementPtr) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    sqlite3_stmt* statement = reinterpret_cast<sqlite3_stmt*>(statementPtr);
    
    int err = executeNonQuery(env, connection, statement);
    return err == SQLITE_DONE && sqlite3_changes(connection->db) > 0
    ? sqlite3_last_insert_rowid(connection->db) : -1;
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeGetDbLookaside
(JNIEnv* env, jclass clazz, jlong connectionPtr) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    
    int cur = -1;
    int unused;
    sqlite3_db_status(connection->db, SQLITE_DBSTATUS_LOOKASIDE_USED, &cur, &unused, 0);
    return cur;
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeCancel
(JNIEnv* env, jclass clazz, jlong connectionPtr) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    connection->canceled = true;
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_database_sqlite_SQLiteConnection_nativeResetCancel
(JNIEnv* env, jclass clazz, jlong connectionPtr, jboolean cancelable) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    connection->canceled = false;
    
    if (cancelable) {
        sqlite3_progress_handler(connection->db, 4, sqliteProgressHandlerCallback,
                                 connection);
    } else {
        sqlite3_progress_handler(connection->db, 0, NULL, NULL);
    }
}
