# couchbase-lite-java-native #

This is a shared native SQLite library used for [Couchbase Lite Java](https://github.com/couchbase/couchbase-lite-java). 

## Get the code

```
$ git clone https://github.com/couchbase/couchbase-lite-java-native.git
```

## Support Platform
* Linux (x86, x86_64, amd64)
* Windows (x86, x86_64)
* OSX (x86, x86_64)

## How to build

The project is using [Gradle](http://www.gradle.org) to build and package the native binaries into a jar file (See Gradle [Building native binaries](http://www.gradle.org/docs/current/userguide/nativeBinaries.html) for more info). The packaged jar file will be located in build/libs folder.

```
$ gradlew clean
$ gradlew build
```

### Linux 
* To build the x86 binary on a 64-bit machine, you will need to setup a 64-bit toolchain as follows.

```
$ sudo apt-get install gcc-multilib
$ sudo apt-get install g++-multilib
```

### Windows
* Visual Studio 2013 or later is required. 

### OSX
* Command Line Tools for Xcode is required. You may download the Command Line Tools from the [Apple Developer](https://developer.apple.com/xcode/downloads) website.

