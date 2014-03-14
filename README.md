# couchbase-lite-java-native #

This is a shared native SQLite library used for [Couchbase Lite Java](https://github.com/couchbase/couchbase-lite-java). 

## Get the code

```
$ git clone https://github.com/couchbase/couchbase-lite-java-native.git
```

## Support Platform
Currently the library has been compiled and tested on OSX 10.9 x86_64 and Windows 7 x86 (Using G++ distributed with [MinGW](http://www.mingw.org/)). 

## How to build or package

The project is using [Gradle](http://www.gradle.org) to build and package the native binaries into a jar file (See Gradle [Building native binaries](http://www.gradle.org/docs/current/userguide/nativeBinaries.html) for more info). The packaged jar file will be located in build/libs folder.

```
$ gradlew clean
$ gradlew build
```

## Troubleshooting
* Current MinGW does't include dflcn library. You will get the pre-built shared dlfcn-win32 binary for MinGW from this [link](https://code.google.com/p/dlfcn-win32/). Just simply extract and put all the files into your installed MinGW folder.



