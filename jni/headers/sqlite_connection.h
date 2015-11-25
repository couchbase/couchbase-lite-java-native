/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (c) 2015 Couchbase, Inc. All rights reserved.
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

#ifndef _CBL_DATABASE_SQLITE_CONNECTION_H
#define _CBL_DATABASE_SQLITE_CONNECTION_H

#include "sqlite3.h"

struct SQLiteConnection {
    // Open flags.
    // Must be kept in sync with the constants defined in SQLiteDatabase.java.
    enum {
        OPEN_READWRITE          = 0x00000000,
        OPEN_READONLY           = 0x00000001,
        OPEN_READ_MASK          = 0x00000001,
        NO_LOCALIZED_COLLATORS  = 0x00000010,
        CREATE_IF_NECESSARY     = 0x10000000,
    };
    
    sqlite3* const db;
    const int openFlags;
    const char* path;
    const char* label;
    
    volatile bool canceled;
    
    SQLiteConnection(sqlite3* db, int openFlags, const char* path, const char* label) :
    db(db), openFlags(openFlags), path(path), label(label), canceled(false) { }
};

#endif // _CBL_DATABASE_SQLITE_CONNECTION_H
