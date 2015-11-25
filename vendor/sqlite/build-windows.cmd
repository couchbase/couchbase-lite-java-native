@echo off

:: Output Directory
set OUTPUT_DIR="%CD%\libs\windows"

:: Clean output directory
if EXIST %OUTPUT_DIR% (
	rmdir /S /Q %OUTPUT_DIR%	
)
mkdir %OUTPUT_DIR%

:: Clean build directory
if EXIST build (
	rmdir /S /Q build
)

:: build
call gradlew.bat build

:: Copy binaries
cd build\libs
call jar xf sqlite.jar libs
Xcopy /E /I libs\windows\* %OUTPUT_DIR%

:: Finished
cd ..\..\

:: Clean build directory
if EXIST build (
	rmdir /S /Q build
)
