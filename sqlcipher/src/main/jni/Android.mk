LOCAL_PATH := $(call my-dir)

### libsqlcipher shared library ###
include $(CLEAR_VARS)
LIBSQLCIPHER_PATH := ../../../../vendor/sqlcipher/libs
LOCAL_MODULE := libsqlcipher
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/$(LIBSQLCIPHER_PATH)/include
LOCAL_SRC_FILES := $(LIBSQLCIPHER_PATH)/android/$(TARGET_ARCH_ABI)/libsqlcipher.so
include $(PREBUILT_SHARED_LIBRARY)

### libicui18n static library ###
include $(CLEAR_VARS)
ICUI18N_PATH := ../../../../vendor/icu4c-android
LOCAL_MODULE := libicui18n
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/$(ICUI18N_PATH)/include/i18n
LOCAL_SRC_FILES := $(ICUI18N_PATH)/libs/android/$(TARGET_ARCH_ABI)/libicui18n_static.a
include $(PREBUILT_STATIC_LIBRARY)

### libicuuc static library ###
include $(CLEAR_VARS)
ICUUC_PATH := ../../../../vendor/icu4c-android
LOCAL_MODULE := libicuuc
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/$(ICUUC_PATH)/include/common
LOCAL_SRC_FILES := $(ICUUC_PATH)/libs/android/$(TARGET_ARCH_ABI)/libicuuc_static.a
include $(PREBUILT_STATIC_LIBRARY)

### Build cbljavasqlcipher ###
include $(CLEAR_VARS)
LOCAL_MODULE := cbljavasqlcipher
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../../jni/headers
LOCAL_SRC_FILES := ../../../../jni/source/com_couchbase_lite_internal_database_sqlite_SQLiteDatabase.cpp \
                   ../../../../jni/source/com_couchbase_lite_internal_database_sqlite_SQLiteConnection.cpp \
                   ../../../../jni/source/com_couchbase_lite_internal_database_sqlite_SQLiteQueryCursor.cpp \
                   ../../../../jni/source/com_couchbase_lite_storage_SQLiteJsonCollator.cpp \
                   ../../../../jni/source/com_couchbase_lite_storage_SQLiteRevCollator.cpp \
                   ../../../../jni/source/sqlite_common.cpp
LOCAL_CPPFLAGS := -DANDROID_LOG
LOCAL_CPPFLAGS += -DUSE_ICU4C_UNICODE_COMPARE
LOCAL_CPPFLAGS += -DUCONFIG_ONLY_COLLATION=1
LOCAL_CPPFLAGS += -DUCONFIG_NO_LEGACY_CONVERSION=1
LOCAL_CPPFLAGS += -DSQLITE_HAS_CODEC
LOCAL_STATIC_LIBRARIES := libicui18n libicuuc
LOCAL_SHARED_LIBRARIES := libsqlcipher
LOCAL_LDLIBS := -llog -ldl
include $(BUILD_SHARED_LIBRARY)

### Build cbljavakey ###
include $(CLEAR_VARS)
LOCAL_MODULE := cbljavakey
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../../jni/headers
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../vendor/sqlcipher/vendor/couchbase-lite-libcrypto/libs/include
LOCAL_SRC_FILES := ../../../../jni/source/com_couchbase_lite_internal_database_security_Key.cpp
LOCAL_CPPFLAGS := -DANDROID_LOG
LOCAL_SHARED_LIBRARIES := libsqlcipher
LOCAL_LDLIBS := -llog -ldl
include $(BUILD_SHARED_LIBRARY)