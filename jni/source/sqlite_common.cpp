/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>
#include "sqlite_common.h"

//namespace android {

/* throw a SQLiteException with a message appropriate for the error in handle */
void throw_sqlite3_exception(JNIEnv* env, sqlite3* handle) {
    throw_sqlite3_exception(env, handle, NULL);
}

/* throw a SQLiteException with the given message */
void throw_sqlite3_exception(JNIEnv* env, const char* message) {
    throw_sqlite3_exception(env, NULL, message);
}

/* throw a SQLiteException with a message appropriate for the error in handle
   concatenated with the given message
 */
void throw_sqlite3_exception(JNIEnv* env, sqlite3* handle, const char* message) {
    if (handle) {
        // get the error code and message from the SQLite connection
        // the error message may contain more information than the error code
        // because it is based on the extended error code rather than the simplified
        // error code that SQLite normally returns.
        throw_sqlite3_exception(env, sqlite3_extended_errcode(handle),
                sqlite3_errmsg(handle), message);
    } else {
        // we use SQLITE_OK so that a generic SQLiteException is thrown;
        // any code not specified in the switch statement below would do.
        throw_sqlite3_exception(env, SQLITE_OK, "unknown error", message);
    }
}

/* throw a SQLiteException for a given error code
 * should only be used when the database connection is not available because the
 * error information will not be quite as rich */
void throw_sqlite3_exception_errcode(JNIEnv* env, int errcode, const char* message) {
    throw_sqlite3_exception(env, errcode, "unknown error", message);
}

/* throw a SQLiteException for a given error code, sqlite3message, and
   user message
 */

void throw_sqlite3_exception(JNIEnv* env, int errcode,
                             const char* sqlite3Message, const char* message) {
    const char* exceptionClass;
    switch (errcode & 0xff) { /* mask off extended error code */
        case SQLITE_IOERR:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteDiskIOException";
            break;
        case SQLITE_CORRUPT:
        case SQLITE_NOTADB: // treat "unsupported file format" error as corruption also
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteDatabaseCorruptException";
            break;
        case SQLITE_CONSTRAINT:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteConstraintException";
            break;
        case SQLITE_ABORT:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteAbortException";
            break;
        case SQLITE_DONE:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteDoneException";
            sqlite3Message = NULL; // SQLite error message is irrelevant in this case
            break;
        case SQLITE_FULL:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteFullException";
            break;
        case SQLITE_MISUSE:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteMisuseException";
            break;
        case SQLITE_PERM:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteAccessPermException";
            break;
        case SQLITE_BUSY:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteDatabaseLockedException";
            break;
        case SQLITE_LOCKED:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteTableLockedException";
            break;
        case SQLITE_READONLY:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteReadOnlyDatabaseException";
            break;
        case SQLITE_CANTOPEN:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteCantOpenDatabaseException";
            break;
        case SQLITE_TOOBIG:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteBlobTooBigException";
            break;
        case SQLITE_RANGE:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteBindOrColumnIndexOutOfRangeException";
            break;
        case SQLITE_NOMEM:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteOutOfMemoryException";
            break;
        case SQLITE_MISMATCH:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteDatatypeMismatchException";
            break;
        case SQLITE_INTERRUPT:
            exceptionClass = "com/couchbase/lite/database/OperationCanceledException";
            break;
        default:
            exceptionClass = "com/couchbase/lite/database/sqlite/exception/SQLiteException";
            break;
    }

    if (sqlite3Message) {
        char errCodeCStr[16];
        sprintf(errCodeCStr, "%d", errcode);

        std::string fullMessage(sqlite3Message);
        fullMessage.append(" (code ");
        fullMessage.append(errCodeCStr);
        fullMessage.append(")");

        if (message) {
            fullMessage.append(": ");
            fullMessage.append(message);
        }
        jniThrowException(env, exceptionClass, fullMessage.c_str());
    } else {
        jniThrowException(env, exceptionClass, message);
    }
}

void jniThrowException(JNIEnv* env, const char* className, const char* msg) {
    jclass cls = env->FindClass(className);
    env->ThrowNew(cls, msg);
}

// } // namespace android
