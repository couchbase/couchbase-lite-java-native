#!/bin/bash

set -e

# Output directory:
OUTPUT_DIR="`pwd`/libs/ios"

# Clean output directory:
rm -rf $OUTPUT_DIR
mkdir $OUTPUT_DIR

# Goto project directory:
cd src/xcode/libsqlite

# Build device binary:
rm -rf build
xcodebuild -scheme ios-static -configuration Release -derivedDataPath build
#cp build/Build/Products/Release/libsqlite3.a $OUTPUT_DIR

# Build simualtor binary:
xcodebuild -scheme ios-static -sdk iphonesimulator -configuration Release -derivedDataPath build
#cp build/Build/Products/Release/libsqlite3.dylib $OUTPUT_DIR

# Build fat binary
lipo -create build/Build/Products/Release-iphoneos/libsqlite3.a build/Build/Products/Release-iphonesimulator/libsqlite3.a -o $OUTPUT_DIR/libsqlite3.a

# Clean
rm -rf build

# Finished:
cd ../../../
