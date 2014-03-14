/*
 * sqlite_json_collator.h
 *
 *  Created on: Nov 23, 2013
 *      Author: wcarter
 */

#include <sqlite3.h>

#ifndef SQLITE_JSON_COLLATOR_H_
#define SQLITE_JSON_COLLATOR_H_
#ifdef __cplusplus
extern "C" {
#endif

void sqlite_json_collator_init(sqlite3 * db);

#ifdef __cplusplus
}
#endif
#endif
