#!/bin/bash

set -e

# Output directory:
OUTPUT_DIR="`pwd`/libs/osx"

# Clean output directory:
rm -rf $OUTPUT_DIR
mkdir $OUTPUT_DIR

# Goto project directory:
cd src/xcode/libsqlite

# Build static binary:
rm -rf build
xcodebuild -scheme osx-static -configuration Release -derivedDataPath build
cp build/Build/Products/Release/libsqlite3.a $OUTPUT_DIR

# Build dynamic binary:
rm -rf build
xcodebuild -scheme osx-dynamic -configuration Release -derivedDataPath build
cp build/Build/Products/Release/libsqlite3.dylib $OUTPUT_DIR

# Clean
rm -rf build

# Finished:
cd ../../../
