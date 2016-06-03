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
//  Ported from https://github.com/couchbase/couchbase-lite-ios/blob/master/Source/CBLCollateJSON.m
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "sqlite_connection.h"
#include "sqlite_log.h"
#include "com_couchbase_lite_storage_SQLiteJsonCollator.h"

#ifdef USE_ICU4C_UNICODE_COMPARE
#include <unicode/coll.h>
#endif

// Default collation rules, including Unicode collation for strings
#define sqlite_json_colator_Unicode ((void*)0)
// "Raw" collation rules (which order scalar types differently, beware)
#define sqlite_json_colator_Raw ((void*)1)
// ASCII mode, which is like Unicode except that strings are compared as binary UTF-8.
#define sqlite_json_colator_ASCII ((void*)2)

#ifdef USE_ICU4C_UNICODE_COMPARE
#define DEFAULT_COLLATOR_LOCALE "en_US"
#endif

/**
 * <CollatorContext>
 */

class CollatorContext {
public:
    CollatorContext(void* r, void* c);
    ~CollatorContext();
    void* getRule();
    void* getCollator();
private:
    void* rule;
    void* collator;
};


CollatorContext::CollatorContext(void* r, void* c) {
    rule = r;
    collator = c;
}

CollatorContext::~CollatorContext() {
#ifdef USE_ICU4C_UNICODE_COMPARE
    if (collator) {
        Collator *c = (Collator*)collator;
        delete c;
    }
#endif
}

void* CollatorContext::getRule() {
    return rule;
}

void* CollatorContext::getCollator() {
    return collator;
}

/**
 * </CollatorContext>
 */

static int java_unicode_string_compare(const char *str1, const char *str2);

/**
 * Linux uint8_t is not defined.
 * use unsigned char instead of unit8_t
 */

static int cmp(int n1, int n2) {
    int diff = n1 - n2;
    return diff > 0 ? 1 : (diff < 0 ? -1 : 0);
}

static int dcmp(double n1, double n2) {
    double diff = n1 - n2;
    return diff > 0.0 ? 1 : (diff < 0.0 ? -1 : 0);
}

// Maps an ASCII character to its relative priority in the Unicode collation sequence.
static unsigned char kCharPriority[128];
// Same thing but case-insensitive.
static unsigned char kCharPriorityCaseInsensitive[128];

static void initializeCharPriorityMap(void) {
    static const char* const kInverseMap = "\t\n\r `^_-,;:!?.'\"()[]{}@*/\\&#%+<=>|~$0123456789aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ";
    unsigned char priority = 1;
    unsigned int i;
    for (i = 0; i < strlen(kInverseMap); i++)
        kCharPriority[(unsigned char)kInverseMap[i]] = priority++;
    
    // This table gives lowercase letters the same priority as uppercase:
    memcpy(kCharPriorityCaseInsensitive, kCharPriority, sizeof(kCharPriority));
    unsigned char c;
    for (c = 'a'; c <= 'z'; c++)
        kCharPriorityCaseInsensitive[c] = kCharPriority[toupper(c)];
}

// Types of values, ordered according to Couch collation order.
typedef enum {
    kEndArray,
    kEndObject,
    kComma,
    kColon,
    kNull,
    kFalse,
    kTrue,
    kNumber,
    kString,
    kArray,
    kObject,
    kIllegal
} ValueType;

// "Raw" ordering is: 0:number, 1:false, 2:null, 3:true, 4:object, 5:array, 6:string
// (according to view_collation_raw.js)
static int kRawOrderOfValueType[] = { -4, -3, -2, -1, 2, 1, 3, 0, 6, 5, 4, 7 };

static ValueType valueTypeOf(char c) {
    switch (c) {
        case 'n':
            return kNull;
        case 'f':
            return kFalse;
        case 't':
            return kTrue;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '-':
            return kNumber;
        case '"':
            return kString;
        case ']':
            return kEndArray;
        case '}':
            return kEndObject;
        case ',':
            return kComma;
        case ':':
            return kColon;
        case '[':
            return kArray;
        case '{':
            return kObject;
        default:
            // TODO: How/should we log unexpected characters?
            return kIllegal;
    }
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

static int digitToInt(char ch) {
    if(!isXDigit(ch)) {
        return 0;
    }
    
    if(ch > 'a') {
        return 10 + ch - 'a';
    } else if(ch > 'A') {
        return 10 + ch - 'A';
    } else {
        return ch - '0';
    }
}

static char convertEscape(const char **in) {
    char c = *++(*in);
    switch (c) {
        case 'u': {
            // \u is a Unicode escape; 4 hex digits follow.
            const char* digits = *in + 1;
            *in += 4;
            int uc = (digitToInt(digits[0]) << 12) | (digitToInt(digits[1]) << 8) |
                     (digitToInt(digits[2]) <<  4) | (digitToInt(digits[3]));
            
            if (uc > 127)
                return 0xFF; // This function doesn't support non-ASCII characters
            
            return (char)uc;
        }
        case 'b':   return '\b';
        case 'n':   return '\n';
        case 'r':   return '\r';
        case 't':   return '\t';
        default:    return c;
    }
}

static int compareStringsASCII(const char** in1, const char** in2) {
    const char* str1 = *in1, *str2 = *in2;
    while (true) {
        char c1 = *++str1;
        char c2 = *++str2;
        
        // If one string ends, the other is greater; if both end, they're equal:
        if (c1 == '"') {
            if (c2 == '"')
                break;
            else
                return -1;
        } else if (c2 == '"')
            return 1;
        
        // Un-escape the next character after a backslash:
        if (c1 == '\\')
            c1 = convertEscape(&str1);
        if (c2 == '\\')
            c2 = convertEscape(&str2);
        
        // Compare the next characters:
        int s = cmp(c1, c2);
        if (s)
            return s;
    }
    
    // Strings are equal, so update the positions:
    *in1 = str1 + 1;
    *in2 = str2 + 1;
    return 0;
}

// Unicode collation, but fails (returns -2) if non-ASCII characters are found.
// Basic rule is to compare case-insensitively, but if the strings compare equal, let the one that's
// higher case-sensitively win (where uppercase is _greater_ than lowercase, unlike in ASCII.)
static int compareStringsUnicodeFast(const char** in1, const char** in2) {
    const char* str1 = *in1, *str2 = *in2;
    int resultIfEqual = 0;
    while(true) {
        char c1 = *++str1;
        char c2 = *++str2;
        
        // If one string ends, the other is greater; if both end, they're equal:
        if (c1 == '"') {
            if (c2 == '"')
                break;
            else
                return -1;
        } else if (c2 == '"')
            return 1;
        
        // Handle escape sequences:
        if (c1 == '\\')
            c1 = convertEscape(&str1);
        if (c2 == '\\')
            c2 = convertEscape(&str2);
        
        if ((c1 & 0x80) || (c2 & 0x80))
            return -2; // fail: I only handle ASCII
        
        // Compare the next characters, according to case-insensitive Unicode character priority:
        int s = cmp(kCharPriorityCaseInsensitive[(unsigned char)c1],
                    kCharPriorityCaseInsensitive[(unsigned char)c2]);
        if (s)
            return s;
        
        // Remember case-sensitive result too
        if (resultIfEqual == 0 && c1 != c2)
            resultIfEqual = cmp(kCharPriority[(unsigned char)c1], kCharPriority[(unsigned char)c2]);
    }
    
    if (resultIfEqual)
        return resultIfEqual;
    
    // Strings are equal, so update the positions:
    *in1 = str1 + 1;
    *in2 = str2 + 1;
    return 0;
}


static const char* createStringFromJSON(const char** in) {
    // Scan the JSON string to find its end and whether it contains escapes:
    const char* start = ++*in;
    unsigned escapes = 0;
    const char* str;
    for (str = start; *str != '"'; ++str) {
        if (*str == '\\') {
            ++str;
            if (*str == 'u') {
                escapes += 5;  // \uxxxx adds 5 bytes
                str += 4;
            } else
                escapes += 1;
        }
    }
    *in = str + 1;
    size_t length = str - start;
    
    char* buf = NULL;
    length -= escapes;
    buf = (char*) malloc(length + 1);
    char* dst = buf;
    char c;
    for (str = start; (c = *str) != '"'; ++str) {
        if (c == '\\')
            c = convertEscape(&str);
        *dst++ = c;
    }
    *dst++ = 0; //null terminate
    
    return (const char *)buf;
}

static int compareBinary(const char *str1, const char *str2) {
    if (str1 == NULL && str2 == NULL)
        return 0;
    else if (str1 != NULL && str2 == NULL)
        return 1;
    else if (str1 == NULL && str2 != NULL)
        return -1;
    else
        return strcmp(str1, str2);
}

static int compareStringsUnicode(const void *context, const char **in1, const char **in2) {
    int result = compareStringsUnicodeFast(in1, in2);
    if (result > -2)
        return result;

    const char *str1 = createStringFromJSON(in1);
    const char *str2 = createStringFromJSON(in2);

#ifdef USE_ICU4C_UNICODE_COMPARE
    CollatorContext* cc = (CollatorContext*)context;
    void* c = cc->getCollator();
    if (c) {
        Collator* collator = (Collator*)c;
        result = (int)(collator->compare(str1, str2));
    } else{
        result = compareBinary(str1, str2);
    }
#else
    // Fast compare failed, so resort to using java.text.Collator
    // HACK : calling back to Java to do unicode string compare.
    result = java_unicode_string_compare(str1, str2);
#endif

    if (str1 != NULL) {
        ::free((void *) str1);
        str1 = NULL;
    }
    if (str2 != NULL) {
        ::free((void *) str2);
        str2 = NULL;
    }

    return result;
}

static double readNumber(const char* start, const char* end, char** endOfNumber) {
    // First copy the string into a zero-terminated buffer so we can safely call strtod:
    size_t len = end - start;
    char buf[50];
    char* str = (len < sizeof(buf)) ? buf : (char*) malloc(len + 1);
    if (!str) {
        return 0.0;
    }
    memcpy(str, start, len);
    str[len] = '\0';
    
    char* endInStr;
    double result = strtod(str, &endInStr);
    *endOfNumber = (char*)start + (endInStr - str);
    if (str != buf) {
        free(str);
    }
    return result;
}

// SQLite collation function for JSON-formatted strings.
// The "context" parameter should be one of the three collation mode constants below.
// WARNING: This function *only* works on valid JSON with no whitespace.
// If called on non-JSON strings it is quite likely to crash!
static int collateJSON(void *context, int len1, const void * chars1, int len2, const void * chars2) {
    static bool charPriorityMapInitialized = false;
    if(!charPriorityMapInitialized){
        initializeCharPriorityMap();
        charPriorityMapInitialized = true;
    }
    
    CollatorContext *cc = (CollatorContext*)context;
    if (cc == NULL) {
        return 0;
    }
    
    void* rule = cc->getRule();
    int depth = 0;
    
    const char* str1 = (const char*) chars1;
    const char* str2 = (const char*) chars2;
    do {
        // Get the types of the next token in each string:
        ValueType type1 = valueTypeOf(*str1);
        ValueType type2 = valueTypeOf(*str2);
        
        if (type1 != type2) {
            // If types don't match, stop and return their relative ordering:
            if (rule != sqlite_json_colator_Raw) {
                return cmp(type1, type2);
            } else {
                return cmp(kRawOrderOfValueType[type1], kRawOrderOfValueType[type2]);
            }
        } else
            // If types match, compare the actual token values:
            switch (type1) {
                case kNull:
                case kTrue:
                    str1 += 4;
                    str2 += 4;
                    break;
                case kFalse:
                    str1 += 5;
                    str2 += 5;
                    break;
                case kNumber: {
                    char* next1, *next2;
                    int diff;
                    if (depth == 0) {
                        diff = dcmp( readNumber(str1, str1 + len1, &next1),
                                    readNumber(str2, str2 + len2, &next2) );
                    } else {
                        diff = dcmp(strtod(str1, &next1), strtod(str2, &next2));
                    }
                    if (diff) {
                        return diff; // Numbers don't match
                    }
                    str1 = next1;
                    str2 = next2;
                    break;
                }
                case kString: {
                    int diff;
                    
                    if (rule == sqlite_json_colator_Unicode) {
                        diff = compareStringsUnicode(context, &str1, &str2);
                    } else {
                        diff = compareStringsASCII(&str1, &str2);
                    }
                    
                    if (diff) {
                        // Strings don't match
                        return diff;
                    }
                    
                    break;
                }
                case kArray:
                case kObject:
                    ++str1;
                    ++str2;
                    ++depth;
                    break;
                case kEndArray:
                case kEndObject:
                    ++str1;
                    ++str2;
                    --depth;
                    break;
                case kComma:
                case kColon:
                    ++str1;
                    ++str2;
                    break;
                case kIllegal:
                    return 0;
            }
    } while (depth > 0); // Keep going as long as we're inside an array or object.
    
    return 0;
}

static void collator_dtor(void* context) {
#ifdef USE_ICU4C_UNICODE_COMPARE
    CollatorContext *cc = (CollatorContext*)context;
    if (cc)
        delete cc;
#endif
}

#ifdef USE_ICU4C_UNICODE_COMPARE
static Collator* createCollator(const char* locale) {
    const char* localeStr = locale;
    if (localeStr == NULL)
        localeStr = DEFAULT_COLLATOR_LOCALE;
    
    UErrorCode status = U_ZERO_ERROR;
    Collator* collator = Collator::createInstance(localeStr, status);
    if (!U_SUCCESS(status)) {
        LOGE(SQLITE_COLLATOR_TAG, "Failed to create ICU Collator (locale=%s, status=%d)",
             localeStr, status);
        localeStr = DEFAULT_COLLATOR_LOCALE;
        LOGV(SQLITE_COLLATOR_TAG, "Create ICU Collator with locale= %s instead", localeStr);
        collator = Collator::createInstance(localeStr, status);
        if(!U_SUCCESS(status)) {
            LOGE(SQLITE_COLLATOR_TAG, "Failed to create ICU Collator (locale=%s, status=%d)",
                 localeStr, status);
        }
    }
    return collator;
}
#endif

static void registerCollator(sqlite3* db, const char* locale, const char* icuDataPath) {
#ifdef USE_ICU4C_UNICODE_COMPARE
    const char* localeStr = locale;
    if (localeStr == NULL)
        localeStr = DEFAULT_COLLATOR_LOCALE;

#ifdef ANDROID
    Collator* collator = NULL;
    if (icuDataPath != NULL) {
        // NOTE: Dictionary file is NOT bundled with library. It is separated dictionary file.
        // Set data path:
        setenv("CBL_ICU_PREFIX", icuDataPath, 1);
        // Create ICU Collator:
        collator = createCollator(locale);
    } else {
        LOGE(SQLITE_COLLATOR_TAG, "Failed to create ICU Collator, No ICU Data Path specified\n");
    }
#else // Java
    // NOTE: Dictionary file is bundled with library
    // Create ICU Collator:
    Collator* collator = createCollator(locale);
#endif

    CollatorContext* context = NULL;
    context = new CollatorContext(sqlite_json_colator_Unicode, collator);
    sqlite3_create_collation_v2(db, "JSON", SQLITE_UTF8, context, collateJSON, (void(*)(void*))collator_dtor);
    
    context = new CollatorContext(sqlite_json_colator_Raw, NULL);
    sqlite3_create_collation_v2(db, "JSON_RAW", SQLITE_UTF8, context, collateJSON, (void(*)(void*))collator_dtor);
    
    context = new CollatorContext(sqlite_json_colator_ASCII, NULL);
    sqlite3_create_collation_v2(db, "JSON_ASCII", SQLITE_UTF8, context, collateJSON, (void(*)(void*))collator_dtor);
#else
    CollatorContext* context = NULL;
    context = new CollatorContext(sqlite_json_colator_Unicode, NULL);
    sqlite3_create_collation_v2(db, "JSON", SQLITE_UTF8, context, collateJSON, (void(*)(void*))collator_dtor);
    
    context = new CollatorContext(sqlite_json_colator_Raw, NULL);
    sqlite3_create_collation_v2(db, "JSON_RAW", SQLITE_UTF8, context, collateJSON, (void(*)(void*))collator_dtor);
    
    context = new CollatorContext(sqlite_json_colator_ASCII, NULL);
    sqlite3_create_collation_v2(db, "JSON_ASCII", SQLITE_UTF8, context, collateJSON, (void(*)(void*))collator_dtor);
#endif
}

#ifndef USE_ICU4C_UNICODE_COMPARE
JavaVM *cachedJvm;
static jclass sqliteJsonCollatorClazz;
static jmethodID javaUnicodeCompareMethod;

static int java_unicode_string_compare(const char *str1, const char *str2) {
    JNIEnv* env;
    if (cachedJvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE(SQLITE_COLLATOR_TAG, "Cannot get JNI Environment from the cache JVM.");
        return 0;
    }
    
    jstring jstr1 = env->NewStringUTF(str1);
    jstring jstr2 = env->NewStringUTF(str2);
    
    int result = env->CallStaticIntMethod(sqliteJsonCollatorClazz, javaUnicodeCompareMethod, jstr1, jstr2);
    
    env->DeleteLocalRef(jstr1);
    env->DeleteLocalRef(jstr2);
    
    return result;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM * jvm, void * reserved) {
    JNIEnv* env;
    if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    
    // Unicode String Compare
    cachedJvm = jvm;
    jclass localClazz = env->FindClass("com/couchbase/lite/storage/SQLiteJsonCollator");
    if (localClazz == NULL) {
        return JNI_ERR;
    }
    
    sqliteJsonCollatorClazz = reinterpret_cast<jclass>(env->NewGlobalRef(localClazz));
    if (sqliteJsonCollatorClazz == NULL) {
        return JNI_ERR;
    }
    
    javaUnicodeCompareMethod = env->GetStaticMethodID(localClazz,
                                                      "compareStringsUnicode",
                                                      "(Ljava/lang/String;Ljava/lang/String;)I");
    if (javaUnicodeCompareMethod == NULL) {
        return JNI_ERR;
    }
    
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM * jvm, void * reserved) {
    JNIEnv * env;
    if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return;
    }
    env->DeleteGlobalRef(sqliteJsonCollatorClazz);
    return;
}
#endif


JNIEXPORT void JNICALL Java_com_couchbase_lite_storage_SQLiteJsonCollator_nativeRegister
(JNIEnv* env, jclass clazz, jlong connectionPtr, jstring locale, jstring icuDataPath) {
    SQLiteConnection* connection = reinterpret_cast<SQLiteConnection*>(connectionPtr);
    const char* localeStr = NULL;
    if (locale != NULL)
        localeStr = env->GetStringUTFChars(locale, NULL);
    
    const char* icuDataPathStr = NULL;
    if (icuDataPath != NULL)
        icuDataPathStr = env->GetStringUTFChars(icuDataPath, NULL);
    
    registerCollator(connection->db, localeStr, icuDataPathStr);
    
    if (locale != NULL)
        env->ReleaseStringUTFChars(locale, localeStr);
    if (icuDataPath != NULL)
        env->ReleaseStringUTFChars(icuDataPath, icuDataPathStr);
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_storage_SQLiteJsonCollator_nativeTestCollate
(JNIEnv* env, jclass clazz, jint rule, jint len1, jstring string1, jint len2, jstring string2) {
    const char* cstring1 = env->GetStringUTFChars(string1, NULL);
    const char* cstring2 = env->GetStringUTFChars(string2, NULL);
    
    void* r = NULL;
    if (rule == 0)
        r = sqlite_json_colator_Unicode;
    else if (rule == 1)
        r = sqlite_json_colator_Raw;
    else if (rule == 2)
        r = sqlite_json_colator_ASCII;
    else
        r = sqlite_json_colator_Unicode;
    
#ifdef USE_ICU4C_UNICODE_COMPARE
    Collator* c = createCollator(NULL);
    CollatorContext* cc = new CollatorContext(r, c);
#else
    CollatorContext* cc = new CollatorContext(r, NULL);
#endif
    int result = collateJSON(cc, (int)len1, cstring1, (int)len2, cstring2);
    
    env->ReleaseStringUTFChars(string1, cstring1);
    env->ReleaseStringUTFChars(string2, cstring2);
    
    delete cc;
    
    return result;
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_storage_SQLiteJsonCollator_nativeTestCollateWithLocale
(JNIEnv* env, jclass clazz, jint rule, jstring locale, jint len1, jstring string1, jint len2, jstring string2) {
    const char* clocale = env->GetStringUTFChars(locale, NULL);
    const char* cstring1 = env->GetStringUTFChars(string1, NULL);
    const char* cstring2 = env->GetStringUTFChars(string2, NULL);
    
    void* r = NULL;
    if (rule == 0)
        r = sqlite_json_colator_Unicode;
    else if (rule == 1)
        r = sqlite_json_colator_Raw;
    else if (rule == 2)
        r = sqlite_json_colator_ASCII;
    else
        r = sqlite_json_colator_Unicode;

#ifdef USE_ICU4C_UNICODE_COMPARE
    Collator* c = createCollator(clocale);
    CollatorContext* cc = new CollatorContext(r, c);
#else
    CollatorContext* cc = new CollatorContext(r, NULL);
#endif
    int result = collateJSON(cc, (int)len1, cstring1, (int)len2, cstring2);
    
    env->ReleaseStringUTFChars(locale, clocale);
    env->ReleaseStringUTFChars(string1, cstring1);
    env->ReleaseStringUTFChars(string2, cstring2);
    
    delete cc;
    
    return result;
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_storage_SQLiteJsonCollator_nativeTestDigitToInt
(JNIEnv* env, jclass clazz, jint digit) {
    int result = digitToInt(digit);
    return result;
}

JNIEXPORT jchar JNICALL Java_com_couchbase_lite_storage_SQLiteJsonCollator_nativeTestEscape
(JNIEnv* env, jclass clazz, jstring string) {
    const char* cstring = env->GetStringUTFChars(string, NULL);
    const char* nucstring = cstring;
    char result = convertEscape(&nucstring);
    env->ReleaseStringUTFChars(string, cstring);
    return result;
}
