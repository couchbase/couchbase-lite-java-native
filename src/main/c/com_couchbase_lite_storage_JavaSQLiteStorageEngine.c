/*
 * com_couchbase_lite_storage_JavaSQLiteStorageEngine.c
 *
 *  Created on: Nov 22, 2013
 *      Author: Wayne Carter
 */

#include <stdbool.h>
#include <string.h>

#include "com_couchbase_lite_storage_JavaSQLiteStorageEngine.h"
#include "com_couchbase_lite_storage_JavaSQLiteStorageEngine_StatementCursor.h"

#include "log.h"

#include "sqlite3.h"

#include "sqlite_json_collator.h"
#include "sqlite_rev_collator.h"

static jclass JavaSQLiteStorageEngineClass;
static jclass JavaSQLiteStorageEngine_StatementCursorClass;

// Type Classes
static jclass StringClass;
static jclass IntegerClass;
static jclass LongClass;
static jclass BooleanClass;
static jclass ByteArrayClass;

static void _throwException(JNIEnv *env, const char * fmt, ...)
{
    static jmethodID throwExceptionMethod = 0;
    if (!throwExceptionMethod) {
    	throwExceptionMethod = (*env)->GetStaticMethodID(env, JavaSQLiteStorageEngineClass, "throwSQLException", "(Ljava/lang/String;)V");
    }

    va_list ap;
	va_start(ap, fmt);

	char msg[256];
	vsnprintf(msg, sizeof(msg), fmt, ap);

	va_end(ap);

    (*env)->CallStaticVoidMethod(env, JavaSQLiteStorageEngineClass, throwExceptionMethod, (*env)->NewStringUTF(env, msg));
}

static void * _fromPointer(jlong value)
{
    jvalue ret;
    ret.j = value;
    return (void *) ret.l;
}

static jlong _toPointer(void * value)
{
	jvalue ret;
    ret.l = value;
    return ret.j;
}

static bool _setBindArgs(JNIEnv * env, sqlite3_stmt * stmt, jobjectArray bindArgs)
{
	// Nothing to do if we don't have any bind args.
	if (!bindArgs) {
		return true;
	}

	// Value Methods
	static jmethodID intValueMethod = 0;
	static jmethodID longValueMethod = 0;
	static jmethodID booleanValueMethod = 0;

	if (!intValueMethod) {
		intValueMethod = (*env)->GetMethodID(env, IntegerClass, "intValue", "()I");
	}

	if (!longValueMethod) {
		longValueMethod = (*env)->GetMethodID(env, LongClass, "longValue", "()J");
	}

	if (!booleanValueMethod) {
		booleanValueMethod = (*env)->GetMethodID(env, BooleanClass, "booleanValue", "()Z");
	}

	int length = (*env)->GetArrayLength(env, bindArgs);
	int i;
	for(i=0; i<length; i++)
	{
		 jobject bindArg = (*env)->GetObjectArrayElement(env, bindArgs, i);
		 if (!bindArg) {
			 sqlite3_bind_null(stmt, i+1);
		 } else {
		     jclass bindArgClass = (*env)->GetObjectClass(env, bindArg);
			 if((*env)->IsSameObject(env, bindArgClass, StringClass) == JNI_TRUE)
			 {
				 const char * chars = (*env)->GetStringUTFChars(env, bindArg, 0);
				 int status = sqlite3_bind_text(stmt, i+1, chars, -1, SQLITE_TRANSIENT);
				 (*env)->ReleaseStringUTFChars(env, bindArg, chars);

				 if (status != SQLITE_OK) {
					 log_e(env, "BindArgs: Error (%d) binding string arg: %d", status, i+1);
					 _throwException(env, "BindArgs: Error (%d) binding string arg: %d", status, i+1);

					 return false;
				 }
			 } else if((*env)->IsSameObject(env, bindArgClass, IntegerClass) == JNI_TRUE) {
				 jint value = (*env)->CallIntMethod(env, bindArg, intValueMethod);

				 int status = sqlite3_bind_int(stmt, i+1, value);

				 if (status != SQLITE_OK) {
					 log_e(env, "BindArgs: Error (%d) binding int arg: %d", status, i+1);
					 _throwException(env, "BindArgs: Error (%d) binding int arg: %d", status, i+1);

					 return false;
				 }
			 } else if((*env)->IsSameObject(env, bindArgClass, LongClass) == JNI_TRUE) {
				 jlong value = (*env)->CallLongMethod(env, bindArg, longValueMethod);
				 int status = sqlite3_bind_int64(stmt, i+1, value);

				 if (status != SQLITE_OK) {
					 log_e(env, "BindArgs: Error (%d) binding long arg: %d", status, i+1);
					 _throwException(env, "BindArgs: Error (%d) binding long arg: %d", status, i+1);

					 return false;
				 }
			 } else if((*env)->IsSameObject(env, bindArgClass, BooleanClass) == JNI_TRUE) {
				 jboolean value = (*env)->CallBooleanMethod(env, bindArg, booleanValueMethod);

				 int status = sqlite3_bind_int(stmt, i+1, (value == true ? 1 : 0));

				 if (status != SQLITE_OK) {
					 log_e(env, "BindArgs: Error (%d) binding boolean arg: %d", status, i+1);
					 _throwException(env, "BindArgs: Error (%d) binding boolean arg: %d", status, i+1);

					 return false;
				 }
			 } else if((*env)->IsSameObject(env, bindArgClass, ByteArrayClass) == JNI_TRUE) {
				 jbyteArray byteArray = (jbyteArray)bindArg;
				 jsize length = (*env)->GetArrayLength(env, byteArray);
				 void * bytes = (*env)->GetPrimitiveArrayCritical(env, byteArray, 0);
				 int status = sqlite3_bind_blob(stmt, i+1, bytes, length, SQLITE_TRANSIENT);
				 (*env)->ReleasePrimitiveArrayCritical(env, byteArray, bytes, JNI_ABORT);

				 if (status != SQLITE_OK) {
					 log_e(env, "BindArgs: Error (%d) binding blob arg: %d", status, i+1);
					 _throwException(env, "BindArgs: Error (%d) binding blob arg: %d", status, i+1);

					 return false;
				 }
			 } else {
				 log_e(env, "BindArgs: Error binding arg %d.  Unsupported type", i+1);
			 }
		 }
	}

	return true;
}

static sqlite3_stmt * _createStatementWithCString(JNIEnv * env, jobject this, sqlite3 * db, const char * sql, jobjectArray bindArgs, bool throwException)
{
	if(!db) {
		log_e(env, "CreateStatement: Database not open");
		if (throwException) {
			_throwException(env, "CreateStatement: Database not open");
		}
		return NULL;
	}

	sqlite3_stmt * stmt;
	int status = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	if (status != SQLITE_OK) {
		log_e(env, "Error (%d) preparing statement: %s", status, sql);
		if (throwException) _throwException(env, "CreateStatement: Error (%d) preparing statement: %s", status, sql);

		return NULL;
	}

	if (!_setBindArgs(env, stmt, bindArgs)) {
		log_e(env, "Error binding args");
		if (throwException) _throwException(env, "CreateStatement: Error binding args");

		return NULL;
	}

	return stmt;
}

static sqlite3_stmt * _createStatement(JNIEnv * env, jobject this, sqlite3 * db, jstring sql, jobjectArray bindArgs, bool throwException)
{
	const char * sqlStr = (*env)->GetStringUTFChars(env, sql, 0);
	sqlite3_stmt * stmt = _createStatementWithCString(env, this, db, sqlStr, bindArgs, throwException);
	(*env)->ReleaseStringUTFChars(env, sql, sqlStr);

	return stmt;
}

/* ******
 * INIT *
 ********/

JavaVM *cached_jvm;
jclass collator_clazz;
jmethodID unicode_compare_method;

JNIEnv *getEnv() 
{
	JNIEnv *env;
	(*cached_jvm)->GetEnv(cached_jvm, (void **)&env, JNI_VERSION_1_2);
	return env;
}

// HACK : Call back to Java to do Unicode string comparison
int unicode_string_compare(const char *str1, const char *str2) 
{
    JNIEnv *env = getEnv();

    jstring jstr1 = (*env)->NewStringUTF(env, str1);
	jstring jstr2 = (*env)->NewStringUTF(env, str2);

	int result = (*env)->CallStaticIntMethod(env, collator_clazz, unicode_compare_method, jstr1, jstr2);
	(*env)->DeleteLocalRef(env, jstr1);
	(*env)->DeleteLocalRef(env, jstr2);

	return result;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM * jvm, void * reserved) 
{
	// Try to get a reference to the current environment.
	JNIEnv * env;
	if (JNI_OK != (*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_2)) {
		return JNI_ERR;
	}

	// Try to find the Storage Engine class.
	jclass clazz = (*env)->FindClass(env, "com/couchbase/lite/storage/JavaSQLiteStorageEngine");
	if (clazz == NULL) return JNI_ERR;
	// Cache a weak global ref.  This allows the C class to be unloaded.
	JavaSQLiteStorageEngineClass = (jclass)((*env)->NewGlobalRef(env, clazz));
	if (JavaSQLiteStorageEngineClass == NULL) return JNI_ERR;

	// Try to find the Storage Engine Statement Cursor class.
	clazz = (*env)->FindClass(env, "com/couchbase/lite/storage/JavaSQLiteStorageEngine$StatementCursor");
	if (clazz == NULL) return JNI_ERR;
	// Cache a weak global ref.  This allows the C class to be unloaded.
	JavaSQLiteStorageEngine_StatementCursorClass = (jclass)((*env)->NewGlobalRef(env, clazz));
	if (JavaSQLiteStorageEngine_StatementCursorClass == NULL) return JNI_ERR;

	// Type Classes
	StringClass = (jclass)(*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/String"));
	IntegerClass = (jclass)(*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Integer"));
	LongClass = (jclass)(*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Long"));
	BooleanClass = (jclass)(*env)->NewGlobalRef(env, (*env)->FindClass(env, "java/lang/Boolean"));
	ByteArrayClass = (jclass)(*env)->NewGlobalRef(env, (*env)->FindClass(env, "[B"));

	// Unicode String Compare
	cached_jvm = jvm;
	clazz = (*env)->FindClass(env, "com/couchbase/lite/util/JsonCollator");
	if (clazz == NULL) {
        return JNI_ERR;
    }

    collator_clazz = (jclass)((*env)->NewGlobalRef(env, clazz));
    if (collator_clazz == NULL) {
        return JNI_ERR;
    }

    unicode_compare_method = (*env)->GetStaticMethodID(env, clazz, "compareStringsUnicode", "(Ljava/lang/String;Ljava/lang/String;)I");
    if (unicode_compare_method == NULL) {
        return JNI_ERR;
    }

	return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM * jvm, void * reserved) 
{
	// Try to get a reference to the current environment.
	JNIEnv * env;
	if (JNI_OK != (*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_2)) return;

	// Remove cached weak global refs to the classes.
	(*env)->DeleteWeakGlobalRef(env, JavaSQLiteStorageEngineClass);
	(*env)->DeleteWeakGlobalRef(env, JavaSQLiteStorageEngine_StatementCursorClass);

	return;
}

/******************
 * STORAGE ENGINE *
 ******************/

JNIEXPORT jlong JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1open
  (JNIEnv * env, jobject this, jstring path)
{
    sqlite3 * db;

	const char * pathStr = (*env)->GetStringUTFChars(env, path, 0);
	int status = sqlite3_open(pathStr, &db);
	(*env)->ReleaseStringUTFChars(env, path, pathStr);
	if (status != SQLITE_OK) {
		log_e(env, "Open: Error (%d) opening database", status);

		status = sqlite3_close(db);
		if (status != SQLITE_OK) {
			log_e(env, "Open: Error (%d) closing database", status);
		}

		return false;
	}

	sqlite_json_collator_init(db, &unicode_string_compare);
	sqlite_rev_collator_init(db);

	return _toPointer(db);
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1getVersion
	(JNIEnv * env, jobject this, jlong handle)
{
	const char * sql = "PRAGMA user_version";
	sqlite3_stmt * stmt = _createStatementWithCString(env, this, _fromPointer(handle), sql, NULL, false);
	if (!stmt) {
		return 0;
	}

	int status = sqlite3_step(stmt);
	if (status != SQLITE_ROW && status != SQLITE_DONE) {
		log_e(env, "GetVersion: Error (%d) stepping statement: %s", status, sql);

		return 0;
	}

	int version = sqlite3_column_int(stmt, 0);

	status = sqlite3_finalize(stmt);
	if (status != SQLITE_OK) {
		log_w(env, "GetVersion: Error (%d) finalizing statement: %s", status, sql);
	}

	return version;
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1setVersion
	(JNIEnv * env, jobject this, jlong handle, jint version)
{
	char sql[50];

	sqlite3 * db = _fromPointer(handle);

	if(!db) {
		log_e(env, "SetVersion: Database not open");

		return;
	}

	sprintf(sql, "PRAGMA user_version = %ld", (long)version);
	
	char * error;
	int status = sqlite3_exec(db, sql, 0, 0, &error);
	if (status != SQLITE_OK) {
		log_e(env, "SetVersion: Error (%d) executing SQL: %s", status, error);

		sqlite3_free(error);

		return;
	}
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1beginTransaction
	(JNIEnv * env, jobject this, jlong handle)
{
	sqlite3 * db = _fromPointer(handle);

	if(!db) {
		log_e(env, "BeginTransaction: Database not open");

		return;
	}

	const char * sql = "BEGIN";
	char * error;
	int status = sqlite3_exec(db, sql, 0, 0, &error);
	if (status != SQLITE_OK) {
		log_e(env, "BeginTransaction: Error (%d) executing SQL: %s", status, error);

		_throwException(env, "Execute: Error (%d) executing SQL: %s", status, error);

		sqlite3_free(error);

		return;
	}
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1commit
	(JNIEnv * env, jobject this, jlong handle)
{
	sqlite3 * db = _fromPointer(handle);

	if(!db) {
		log_e(env, "Commit: Database not open");

		return;
	}

	const char * sql = "COMMIT";
	char * error;
	int status = sqlite3_exec(db, sql, 0, 0, &error);
	if (status != SQLITE_OK) {
		log_e(env, "Commit: Error (%d) executing SQL: %s", status, error);

		sqlite3_free(error);

		return;
	}
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1rollback
	(JNIEnv * env, jobject this, jlong handle)
{
	sqlite3 * db = _fromPointer(handle);

	if(!db) {
		log_e(env, "Rollback: Database not open");

		return;
	}

	const char * sql = "ROLLBACK";
	char * error;
	int status = sqlite3_exec(db, sql, 0, 0, &error);
	if (status != SQLITE_OK) {
		log_e(env, "Rollback: Error (%d) executing SQL: %s", status, error);

		sqlite3_free(error);

		return;
	}
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1execute__JLjava_lang_String_2
	(JNIEnv * env, jobject this, jlong handle, jstring sql)
{
	sqlite3 * db = _fromPointer(handle);

	if(!db) {
		log_e(env, "Execute: Database not open");
		_throwException(env, "Execute: Database not open");

		return;
	}

	char * error;
	const char * sqlStr = (*env)->GetStringUTFChars(env, sql, 0);
	int status = sqlite3_exec(db, sqlStr, 0, 0, &error);
	(*env)->ReleaseStringUTFChars(env, sql, sqlStr);
	if (status != SQLITE_OK) {
		log_e(env, "Execute: Error (%d) executing SQL: %s", status, error);
		_throwException(env, "Execute: Error (%d) executing SQL: %s", status, error);

		sqlite3_free(error);

		return;
	}
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1execute__JLjava_lang_String_2_3Ljava_lang_Object_2
	(JNIEnv * env, jobject this, jlong handle, jstring sql, jobjectArray bindArgs)
{
	sqlite3 * db = _fromPointer(handle);

    sqlite3_stmt * stmt = _createStatement(env, this, db, sql, bindArgs, true);
	if (!stmt) {
		return;
	}

	int status = sqlite3_step(stmt);
	if (status != SQLITE_ROW && status != SQLITE_DONE) {
		const char * sqlStr = (*env)->GetStringUTFChars(env, sql, 0);
		log_e(env, "Execute: Error (%d) stepping statement: %s", status, sqlStr);
		_throwException(env, "Execute: Error (%d) stepping statement: %s", status, sqlStr);
		(*env)->ReleaseStringUTFChars(env, sql, sqlStr);

		return;
	}

	status = sqlite3_finalize(stmt);
	if (status != SQLITE_OK) {
		const char * sqlStr = (*env)->GetStringUTFChars(env, sql, 0);
		log_w(env, "Execute: Error (%d) finalizing statement: %s", status, sqlStr);
		(*env)->ReleaseStringUTFChars(env, sql, sqlStr);
	}
}

JNIEXPORT jobject JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1query
	(JNIEnv * env, jobject this, jlong handle, jstring sql, jobjectArray bindArgs)
{
	sqlite3 * db = _fromPointer(handle);

    sqlite3_stmt * stmt = _createStatement(env, this, db, sql, bindArgs, false);

	static jmethodID statementCursorCtor = 0;
	if (!statementCursorCtor) {
		statementCursorCtor = (*env)->GetMethodID(env, JavaSQLiteStorageEngine_StatementCursorClass, "<init>", "(J)V");
	}
	
	jobject cursor = (*env)->NewObject(env, JavaSQLiteStorageEngine_StatementCursorClass, statementCursorCtor, _toPointer(stmt));

	return cursor;
}

JNIEXPORT jlong JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1insert
	(JNIEnv * env, jobject this, jlong handle, jstring sql, jobjectArray bindArgs)
{
	sqlite3 * db = _fromPointer(handle);

    sqlite3_stmt * stmt = _createStatement(env, this, db, sql, bindArgs, false);
	if (!stmt) {
		return 0;
	}
	
	int status = sqlite3_step(stmt);
	if (status != SQLITE_ROW && status != SQLITE_DONE) {
		const char * sqlStr = (*env)->GetStringUTFChars(env, sql, 0);
		log_e(env, "Insert: Error (%d) stepping statement, SQL: %s", status, sqlStr);
		(*env)->ReleaseStringUTFChars(env, sql, sqlStr);

		return 0;
	}

	sqlite_int64 rowId = sqlite3_last_insert_rowid(db);

	status = sqlite3_finalize(stmt);
	if (status != SQLITE_OK) {
		const char * sqlStr = (*env)->GetStringUTFChars(env, sql, 0);
		log_w(env, "Insert: Error (%d) finalizing statement: %s", status, sqlStr);
		(*env)->ReleaseStringUTFChars(env, sql, sqlStr);
	}

	return rowId;
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1update
	(JNIEnv * env, jobject this, jlong handle, jstring sql, jobjectArray bindArgs)
{
    sqlite3 * db = _fromPointer(handle);

	sqlite3_stmt * stmt = _createStatement(env, this, db, sql, bindArgs, false);
	if (!stmt) {
		return 0;
	}

	int status = sqlite3_step(stmt);
	if (status != SQLITE_ROW && status != SQLITE_DONE) {
		const char * sqlStr = (*env)->GetStringUTFChars(env, sql, 0);
		log_e(env, "Update: Error (%d) stepping statement: %s", status, sqlStr);
		(*env)->ReleaseStringUTFChars(env, sql, sqlStr);

		return 0;
	}

	int changeCount = sqlite3_changes(db);

	status = sqlite3_finalize(stmt);
	if (status != SQLITE_OK) {
		const char * sqlStr = (*env)->GetStringUTFChars(env, sql, 0);
		log_w(env, "Update: Error (%d) finalizing statement: %s", status, sqlStr);
		(*env)->ReleaseStringUTFChars(env, sql, sqlStr);
	}

	return changeCount;
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1delete
	(JNIEnv * env, jobject this, jlong handle, jstring sql, jobjectArray bindArgs)
{
    sqlite3 * db = _fromPointer(handle);

	sqlite3_stmt * stmt = _createStatement(env, this, db, sql, bindArgs, false);
	if (!stmt) {
		return 0;
	}

	int status = sqlite3_step(stmt);
	if (status != SQLITE_ROW && status != SQLITE_DONE) {
		const char * sqlStr = (*env)->GetStringUTFChars(env, sql, 0);
		log_e(env, "Delete: Error (%d) stepping statement: %s", status, sqlStr);
		(*env)->ReleaseStringUTFChars(env, sql, sqlStr);

		return 0;
	}

	int changeCount = sqlite3_changes(db);

	status = sqlite3_finalize(stmt);
	if (status != SQLITE_OK) {
		const char * sqlStr = (*env)->GetStringUTFChars(env, sql, 0);
		log_w(env, "Delete: Error (%d) finalizing statement: %s", status, sqlStr);
		(*env)->ReleaseStringUTFChars(env, sql, sqlStr);
	}

	return changeCount;
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine__1close
	(JNIEnv * env, jobject this, jlong handle)
{
	sqlite3 * db = _fromPointer(handle);

	int status = sqlite3_close(db);
	if (status != SQLITE_OK) {
		log_e(env, "Close: Error (%d) closing database", status);
	}
}

/**********
 * CURSOR *
 **********/

JNIEXPORT jboolean JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine_00024StatementCursor__1next
  (JNIEnv * env, jobject this, jlong handle)
{
	sqlite3_stmt * stmt = _fromPointer(handle);

	int status = sqlite3_step(stmt);

	if (status == SQLITE_ROW) {
		return true;
	} else if (status == SQLITE_DONE) {
		return false;
	} else {
		log_e(env, "Cursor.Step: Error (%d) stepping statement: %s", status, sqlite3_sql(stmt));

		return false;
	}
}

JNIEXPORT jstring JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine_00024StatementCursor__1getString
	(JNIEnv * env, jobject this, jlong handle, jint columnIndex)
{
	sqlite3_stmt * stmt = _fromPointer(handle);
	const char * value = (const char *)sqlite3_column_text(stmt, columnIndex);

	return (*env)->NewStringUTF(env, value);
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine_00024StatementCursor__1getInt
	(JNIEnv * env, jobject this, jlong handle, jint columnIndex)
{
	sqlite3_stmt * stmt = _fromPointer(handle);
	int value = sqlite3_column_int(stmt, columnIndex);

	return value;
}

JNIEXPORT jlong JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine_00024StatementCursor__1getLong
	(JNIEnv * env, jobject this, jlong handle, jint columnIndex)
{
	sqlite3_stmt * stmt = _fromPointer(handle);
	sqlite_int64 value = sqlite3_column_int64(stmt, columnIndex);

	return value;
}

JNIEXPORT jbyteArray JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine_00024StatementCursor__1getBlob
	(JNIEnv * env, jobject this, jlong handle, jint columnIndex)
{
	sqlite3_stmt * stmt = _fromPointer(handle);

	jsize length;
	const void *blob = sqlite3_column_blob(stmt, columnIndex);
	if (!blob) return NULL;

	length = sqlite3_column_bytes(stmt, columnIndex);
	jbyteArray jBytes = (*env)->NewByteArray(env, length);
	jbyte * bytes = (*env)->GetPrimitiveArrayCritical(env, jBytes, 0);
	memcpy(bytes, blob, length);
	(*env)->ReleasePrimitiveArrayCritical(env, jBytes, bytes, 0);

	return jBytes;
}

JNIEXPORT void JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine_00024StatementCursor__1close
	(JNIEnv * env, jobject this, jlong handle)
{
	sqlite3_stmt * stmt = _fromPointer(handle);

	int status = sqlite3_finalize(stmt);

	if (status != SQLITE_OK) {
		log_w(env, "Cursor.Close: Error (%d) finalizing statement: %s", status, sqlite3_sql(stmt));
	}
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine_nativeTestCollateJson
  (JNIEnv * env, jclass clazz, jint mode, jstring string1, jstring string2)
{
	const char* cstring1 = (*env)->GetStringUTFChars(env, string1, 0);
	const char* cstring2 = (*env)->GetStringUTFChars(env, string2, 0);

	sqlite_json_collator_setUnicodeStringCompare(&unicode_string_compare);
	int result = sqlite_json_collator_test((void *)mode, cstring1, cstring2);
	
	(*env)->ReleaseStringUTFChars(env, string1, cstring1);
	(*env)->ReleaseStringUTFChars(env, string2, cstring2);
	
	return result;
}

JNIEXPORT jint JNICALL Java_com_couchbase_lite_storage_JavaSQLiteStorageEngine_nativeTestCollateRevIds
  (JNIEnv * env, jclass clazz, jstring string1, jstring string2) 
{
	const char* cstring1 = (*env)->GetStringUTFChars(env, string1, 0);
	const char* cstring2 = (*env)->GetStringUTFChars(env, string2, 0);

	int result = sqlite_rev_collator_test(cstring1, cstring2);

	(*env)->ReleaseStringUTFChars(env, string1, cstring1);
	(*env)->ReleaseStringUTFChars(env, string2, cstring2);

	return result;
}
