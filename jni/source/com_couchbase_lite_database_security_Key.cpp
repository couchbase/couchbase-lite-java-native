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

#include "com_couchbase_lite_database_security_Key.h"

#if !defined (CBL_KEY_CRYPTO_CC) \
 && !defined (CBL_KEY_CRYPTO_OPENSSL)
#define CBL_KEY_CRYPTO_OPENSSL
#endif

#if defined (CBL_KEY_CRYPTO_CC)

#import <CommonCrypto/CommonCrypto.h>

JNIEXPORT jbyteArray JNICALL Java_com_couchbase_lite_database_security_Key_nativeDerivePBKDF2SHA256Key
  (JNIEnv *env, jclass clazz, jstring password, jbyteArray salt, jint rounds) {
    if (password == NULL || salt == NULL)
      return NULL;

    // Password:
    const char* passwordCStr = env->GetStringUTFChars(password, NULL);
    int passwordSize = (int)env->GetStringLength (password);

    // Salt:
    int saltSize = env->GetArrayLength (salt);
    unsigned char* saltBytes = new unsigned char[saltSize];
    env->GetByteArrayRegion (salt, 0, saltSize, reinterpret_cast<jbyte*>(saltBytes));

    // PBKDF2-SHA256
    int outputSize = 32; //256 bit
    unsigned char* output = new unsigned char[outputSize * 2];
    int status = CCKeyDerivationPBKDF(kCCPBKDF2,
                                      passwordCStr, passwordSize,
                                      saltBytes, saltSize,
                                      kCCPRFHmacAlgSHA256, rounds,
                                      output, outputSize);

    // Release memory:
    env->ReleaseStringUTFChars(password, passwordCStr);
    delete[] saltBytes;

    // Return null if not success:
    if (status)
      return NULL;

    // Result:
    jbyteArray result = env->NewByteArray(outputSize);
    env->SetByteArrayRegion(result, 0, outputSize, (jbyte*)output);

    // Release memory:
    delete[] output;
    
    return result;
}

#elif defined (CBL_KEY_CRYPTO_OPENSSL)

#include "openssl/evp.h"
#include "openssl/sha.h"

JNIEXPORT jbyteArray JNICALL Java_com_couchbase_lite_database_security_Key_nativeDerivePBKDF2SHA256Key
  (JNIEnv *env, jclass clazz, jstring password, jbyteArray salt, jint rounds) {
    if (password == NULL || salt == NULL)
      return NULL;

    // Password:
    const char* passwordCStr = env->GetStringUTFChars(password, NULL);
    int passwordSize = (int)env->GetStringLength (password);

    // Salt:
    int saltSize = env->GetArrayLength (salt);
    unsigned char* saltBytes = new unsigned char[saltSize];
    env->GetByteArrayRegion (salt, 0, saltSize, reinterpret_cast<jbyte*>(saltBytes));

    // PBKDF2-SHA256
    int outputSize = 32; //256 bit
    unsigned char* output = new unsigned char[outputSize * 2];
    int status = PKCS5_PBKDF2_HMAC(passwordCStr, passwordSize, saltBytes, saltSize,
                                   (int)rounds, EVP_sha256(), outputSize, output);
    // Release memory:
    env->ReleaseStringUTFChars(password, passwordCStr);
    delete[] saltBytes;

    // Return null if not success:
    if (status == 0)
      return NULL;

    // Result:
    jbyteArray result = env->NewByteArray(outputSize);
    env->SetByteArrayRegion(result, 0, outputSize, (jbyte*)output);

    // Release memory:
    delete[] output;
    
    return result;
}
#else
#error "NO DEFAULT CRYPTO PROVIDER DEFINED"
#endif

