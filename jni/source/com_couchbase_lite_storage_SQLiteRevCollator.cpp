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
//  Ported from https://github.com/couchbase/couchbase-lite-ios/blob/master/Source/CBL_Revision.m
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "sqlite_connection.h"
#include "com_couchbase_lite_storage_SQLiteRevCollator.h"

#ifdef _MSC_VER
#define INLINE __forceinline /* use __forceinline (VC++ specific) */
#else
#define INLINE inline        /* use standard inline */
#endif

static INLINE int sgn(int n) {
    return n > 0 ? 1 : (n < 0 ? -1 : 0);
}

static int getMin(int a, int b) {
    return a < b ? a : b;
}

static int defaultCollate(const char* str1, int len1, const char* str2, int len2) {
    int result = memcmp(str1, str2, getMin(len1, len2));
    return sgn(result ? result : (len1 - len2));
}

static int isXDigit(char ch) {
    if (ch >= '0' && ch <= '9')
        return 1;
    if (ch >= 'a' && ch <= 'f')
        return 1;
    if (ch >= 'A' && ch <= 'F')
        return 1;
    
    return 0;
}

static int digitToInt(int c) {
    if(!isXDigit(c)) {
        return 0;
    }
    if(c > 'a') {
        return 10 + c - 'a';
    } else if(c > 'A') {
        return 10 + c - 'A';
    } else {
        return c - '0';
    }
}

static int parseDigits(const char* str, const char* end) {
    int result = 0;
    for (; str < end; ++str) {
        if (!isdigit(*str))
            return 0;
        result = 10*result + digitToInt(*str);
    }
    return result;
}

/* 
 * A proper revision ID consists of a generation number, a hyphen, and an arbitrary suffix.
 * Compare the generation numbers numerically, and then the suffixes lexicographically.
 * If either string isn't a proper rev ID, fall back to lexicographic comparison. 
 */
static int collateRevIDs(void *context, int len1, const void * r1, int len2, const void * r2) {
    const char* rev1 = (const char*)r1;
    const char* rev2 = (const char*)r2;
    const char* dash1 = (const char*)memchr(rev1, '-', len1);
    const char* dash2 = (const char*)memchr(rev2, '-', len2);
    if ((dash1==rev1+1 && dash2==rev2+1)
        || dash1 > rev1+8 || dash2 > rev2+8
        || dash1==NULL || dash2==NULL) {
        // Single-digit generation #s, or improper rev IDs; just compare as plain text:
        return defaultCollate(rev1,len1, rev2,len2);
    }
    // Parse generation numbers. If either is invalid, revert to default collation:
    int gen1 = parseDigits(rev1, dash1);
    int gen2 = parseDigits(rev2, dash2);
    if (!gen1 || !gen2)
        return defaultCollate(rev1,len1, rev2,len2);
    
    // Compare generation numbers; if they match, compare suffixes:
    int result = sgn(gen1 - gen2);
    return result ? result : defaultCollate(dash1+1, len1-(int)(dash1+1-rev1),
                                            dash2+1, len2-(int)(dash2+1-rev2));
}

static void registerCollator(sqlite3 * db) {
    sqlite3_create_collation(db, "REVID", SQLITE_UTF8, NULL, collateRevIDs);
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_storage_SQLiteRevCollator_nativeRegister
(JNIEnv* env, jclass clazz, jlong connectionPtr) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    registerCollator(connection->db);
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_storage_SQLiteRevCollator_nativeTestCollate
(JNIEnv* env, jclass clazz, jstring string1, jstring string2) {
    const char* cstring1 = env->GetStringUTFChars(string1, NULL);
    const char* cstring2 = env->GetStringUTFChars(string2, NULL);
    
    int result = collateRevIDs(NULL,
                               (int)strlen(cstring1), cstring1,
                               (int)strlen(cstring2), cstring2);
    
    env->ReleaseStringUTFChars(string1, cstring1);
    env->ReleaseStringUTFChars(string2, cstring2);
    
    return result;
}
