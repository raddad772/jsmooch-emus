@echo off

if "%VisualStudioVersion%"=="" (
	echo Please run in Visual Studio Native Tools Command Prompt or run vcvarsall.bat to ensure cmake is on the path
	EXIT /B 1
)

if %VisualStudioVersion% == 15.0 (
	set CMAKE_GENERATOR="Visual Studio 15 2017"
) else if %VisualStudioVersion% == 16.0 (
	set CMAKE_GENERATOR="Visual Studio 16 2019"
) else if %VisualStudioVersion% == 17.0 (
	set CMAKE_GENERATOR="Visual Studio 17 2022"
) else (
	echo Unsupported Visual Studio version
	EXIT /B 1
)

REM Set the current directory to the location of the batch script, using the %0 parameter
REM This allows the script to be called from anywhere
pushd "%~dp0"

cmake -B ..\build -S .. -G %CMAKE_GENERATOR% -A x64 || EXIT /B 1

popd

EXIT /B
