@echo off
@rem Running cmake on Windows example for VS2017

cls

@SETLOCAL
@SET CMAKE_EXEC="C:/Program Files/CMake/bin/cmake.exe"
@IF EXIST %CMAKE_EXEC% GOTO START
@GOTO ERROR

:START
mkdir build
cd build

%CMAKE_EXEC% ^
-G"Visual Studio 15 Win64" ../work

cd ..
@GOTO EOF

@:ERROR
@echo ERROR: CMake not found.

:EOF

pause

