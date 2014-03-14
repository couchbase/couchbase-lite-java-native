//
//  sqlite_json_collator.c
//  CouchbaseLite
//
//  Created by Wayne Carter
//  Copyright (c) 2011 Couchbase, Inc. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
//  http://wiki.apache.org/couchdb/View_collation#Collation_Specification

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <ctype.h>
#include <string.h>

#include "sqlite_json_collator.h"

// Default collation rules, including Unicode collation for strings
#define sqlite_json_colator_Unicode ((void*)0)
// "Raw" collation rules (which order scalar types differently, beware)
#define sqlite_json_colator_Raw ((void*)1)
// ASCII mode, which is like Unicode except that strings are compared as binary UTF-8.
#define sqlite_json_colator_ASCII ((void*)2)

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

static int cmp(int n1, int n2) {
	int diff = n1 - n2;
	return diff > 0 ? 1 : (diff < 0 ? -1 : 0);
}

static int dcmp(double n1, double n2) {
	double diff = n1 - n2;
	return diff > 0.0 ? 1 : (diff < 0.0 ? -1 : 0);
}

static ValueType valueTypeOf(char c) {
	switch (c) {
	case 'n':
		return kNull;
	case 'f':
		return kFalse;
	case 't':
		return kTrue;
	case '0' ... '9':
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

            if (uc > 127) {
            	// TODO: How/should we log unexpected characters?
        	}

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

static char* createStringFromJSON(const char** in) {
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
	start = buf;

	return buf;
}

static int compareStringsUnicode(const char** in1, const char** in2) {
	const char* str1 = createStringFromJSON(in1);
	const char* str2 = createStringFromJSON(in2);

	// TODO: This is wrong but at least allows this to run.  We need to actually
	// do a locale specific Unicode compare.
	return compareStringsASCII(&str1, &str2);
}

// SQLite collation function for JSON-formatted strings.
// The "context" parameter should be one of the three collation mode constants below.
// WARNING: This function *only* works on valid JSON with no whitespace.
// If called on non-JSON strings it is quite likely to crash!
int collateJSON(void *context, int len1, const void * chars1, int len2, const void * chars2) {
	const char* str1 = (const char*) chars1;
	const char* str2 = (const char*) chars2;
	int depth = 0;

	do {
		// Get the types of the next token in each string:
		ValueType type1 = valueTypeOf(*str1);
		ValueType type2 = valueTypeOf(*str2);

		if (type1 != type2) {
			// If types don't match, stop and return their relative ordering:
			if (context != sqlite_json_colator_Raw) {
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
				int diff = dcmp(strtod(str1, &next1), strtod(str2, &next2));
				if (diff) {
					// Numbers don't match
					return diff;
				}

				str1 = next1;
				str2 = next2;
				break;
			}
			case kString: {
				int diff;

				if (context == sqlite_json_colator_Unicode) {
					diff = compareStringsUnicode(&str1, &str2);
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

// Init method.

void sqlite_json_collator_init(sqlite3 * db)
{
	sqlite3_create_collation(db, "JSON", SQLITE_UTF8, sqlite_json_colator_Unicode, collateJSON);
	sqlite3_create_collation(db, "JSON_RAW", SQLITE_UTF8, sqlite_json_colator_Raw, collateJSON);
	sqlite3_create_collation(db, "JSON_ASCII", SQLITE_UTF8, sqlite_json_colator_ASCII, collateJSON);
}
