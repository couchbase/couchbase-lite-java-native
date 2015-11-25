LOCAL_PATH := $(call my-dir)

### libicuuc static library ###
include $(CLEAR_VARS)
ICUUC_PATH := ../../../../vendor/icu4c-android
LOCAL_MODULE := libicuuc
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/$(ICUUC_PATH)/include/common
LOCAL_SRC_FILES := $(ICUUC_PATH)/libs/$(TARGET_ARCH_ABI)/libicuuc_static.a
include $(PREBUILT_STATIC_LIBRARY)

### libicui18n static library ###
include $(CLEAR_VARS)
ICUI18N_PATH := ../../../../vendor/icu4c-android
LOCAL_MODULE := libicui18n
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/$(ICUI18N_PATH)/include/i18n
LOCAL_SRC_FILES := $(ICUI18N_PATH)/libs/$(TARGET_ARCH_ABI)/libicui18n_static.a
include $(PREBUILT_STATIC_LIBRARY)

### libsqlite3 shared library ###
include $(CLEAR_VARS)
LIBSQLITE_PATH := ../../../../vendor/sqlite/libs
LOCAL_MODULE := libsqlite3
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/$(LIBSQLITE_PATH)/../src/headers
LOCAL_SRC_FILES := $(LIBSQLITE_PATH)/android/$(TARGET_ARCH_ABI)/libsqlite3.so
include $(PREBUILT_SHARED_LIBRARY)

### Build CBLJavaNativeSQLite ###
include $(CLEAR_VARS)
LOCAL_MODULE := CBLJavaNativeSQLite
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../../jni/headers
LOCAL_SRC_FILES := ../../../../jni/source/com_couchbase_lite_database_sqlite_SQLiteDatabase.cpp \
                   ../../../../jni/source/com_couchbase_lite_database_sqlite_SQLiteConnection.cpp \
                   ../../../../jni/source/com_couchbase_lite_database_sqlite_SQLiteQueryCursor.cpp \
                   ../../../../jni/source/com_couchbase_lite_storage_SQLiteJsonCollator.cpp \
                   ../../../../jni/source/com_couchbase_lite_storage_SQLiteRevCollator.cpp \
                   ../../../../jni/source/sqlite_common.cpp
LOCAL_CPPFLAGS := -DANDROID_LOG
LOCAL_CPPFLAGS += -DUSE_ICU4C_UNICODE_COMPARE
LOCAL_CPPFLAGS += -DUCONFIG_ONLY_COLLATION=1
LOCAL_CPPFLAGS += -DUCONFIG_NO_LEGACY_CONVERSION=1
LOCAL_STATIC_LIBRARIES := libicui18n libicuuc
LOCAL_SHARED_LIBRARIES := libsqlite3
LOCAL_LDLIBS := -llog -ldl
include $(BUILD_SHARED_LIBRARY)
