# couchbase-lite-java-native #

This is a shared native SQLite library used for Couchbase Lite [Android](https://github.com/couchbase/couchbase-lite-android)/[Java](https://github.com/couchbase/couchbase-lite-java). 

There are three SQLite configurations.

1. sqlite-system - No sqlite library provided, use system sqlite installed on the device or machine
2. sqlite        - bundled with a prebuilt sqlite library
3. sqlcipher     - bundled with a prebuilt sqlcipher library for encryption

## Get the code

```
$ git clone https://github.com/couchbase/couchbase-lite-java-native.git
```

## Support Platform
* Android
* Linux (x86, x86_64, amd64)
* Windows (x86, x86_64)
* OSX (x86, x86_64)

## How to build

The project is using [Gradle](http://www.gradle.org) to build and package the native binaries into a jar file (See Gradle [Building native binaries](http://www.gradle.org/docs/current/userguide/nativeBinaries.html) for more info). The packaged jar file will be located in build/libs folder.

```
$ cd <sqlite-system|sqlite|sqlcipher>
$ gradlew -Pspec=<android|java> clean
$ gradlew -Pspec=<android|java> build
```
### Android
* [Android NDK](http://developer.android.com/ndk/index.html)

 Make sure that you have the `ANDROID_NDK_HOME` variable defined. For example,
 ```
 #.bashrc:
 export ANDROID_NDK_HOME=~/Android/android-ndk-r10e
```

### Linux 
* To build the x86 binary on a 64-bit machine, you will need to setup a 64-bit toolchain as follows.

```
$ sudo apt-get install gcc-multilib
$ sudo apt-get install g++-multilib
```

### Windows
* Visual Studio 2013 or later is required. 
* Note: If you have problem with VS 2015, check [Build on Windows with Visual Studio 2015](https://github.com/couchbase/couchbase-lite-java-native/issues/23).

### OSX
* Command Line Tools for Xcode is required. You may download the Command Line Tools from the [Apple Developer](https://developer.apple.com/xcode/downloads) website.
