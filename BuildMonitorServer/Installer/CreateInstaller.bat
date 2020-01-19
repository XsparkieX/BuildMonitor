@echo off

setlocal

pushd "%~dp0"

set PATH=C:\Qt\Tools\mingw730_64\bin;%PATH%
rmdir /S /Q Build
mkdir Build
pushd Build
C:\Qt\5.13.0\mingw73_64\bin\qmake.exe -o Makefile ..\..\BuildMonitorServer.pro -spec win32-g++ CONFIG+=release CONFIG+=qml_release
mingw32-make -j4 -f Makefile.Release
if %errorlevel% neq 0 (
	echo Project failed to compile.
	pause
	exit /b 1
)
popd

rmdir /S /Q "Files"
mkdir "Files"
copy "Build\release\BuildMonitorServer.exe" "Files\"
"C:\Qt\5.13.0\mingw73_64\bin\windeployqt.exe" "Files\BuildMonitorServer.exe"
if %errorlevel% neq 0 (
	echo Failed to generate dependencies.
	pause
	exit /b 1
)

"C:\Program Files (x86)\NSIS\makensis.exe" Installer.nsi
if %errorlevel% neq 0 (
	echo Failed creating installer. Have you installed the plugin?
	pause
	exit /b 1
)

popd

pause
