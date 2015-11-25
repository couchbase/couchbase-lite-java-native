#!/bin/bash

set -e

# Output directory
OUTPUT_DIR="`pwd`/libs/android"

# Clean output directory
rm -rf $OUTPUT_DIR
mkdir $OUTPUT_DIR

cd android

# Clean:
rm -rf libs
rm -rf obj

# Build static binaries:
ndk-build -B NDK_PROJECT_PATH=. NDK_APPLICATION_MK=./Application.mk APP_BUILD_SCRIPT=./Android-Static.mk 

# Copy binaries:
cd obj/local
find . -name "*.a" -exec rsync -R {} $OUTPUT_DIR \;
cd ../../

# Clean:
rm -rf libs
rm -rf obj

# Build shared binaries:
ndk-build -B NDK_PROJECT_PATH=. NDK_APPLICATION_MK=./Application.mk APP_BUILD_SCRIPT=./Android-Shared.mk 

# Copy binaries:
cp -r libs/* $OUTPUT_DIR

# Clean:
rm -rf libs
rm -rf obj

cd ..
