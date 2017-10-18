@echo off

setlocal

set installer_framework=C:\Qt\Tools\QtInstallerFramework\2.0\bin
set zipexec="C:\Program Files\7-Zip\7z.exe"

call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"

MSBuild.exe "%~dp0..\BuildMonitor.vcxproj" /nologo /p:Configuration=Release
if %errorlevel% neq 0 (
	echo Project failed to compile.
	pause
	exit /b 1
)

rd /S /Q "%~dp0\Packages\buildmonitor\data\"
mkdir "%~dp0\Packages\buildmonitor\data\"
copy "%~dp0..\x64\Release\BuildMonitor.exe" "%~dp0\Packages\buildmonitor\data\"
"C:\Qt\5.9\msvc2017_64\bin\windeployqt.exe" "%~dp0\Packages\buildmonitor\data\BuildMonitor.exe"
if %errorlevel% neq 0 (
	echo Failed to generate dependencies.
	pause
	exit /b 1
)

cd /d "%~dp0\Packages\buildmonitor\data\"
%zipexec% a BuildMonitor.7z *
if %errorlevel% neq 0 (
	echo Failed creating 7z archive.
	pause
	exit /b 1
)
move BuildMonitor.7z ..
cd /d ..

set data_path="%~dp0\Packages\buildmonitor\data\"
rd /S /Q %data_path% 
mkdir %data_path%

move BuildMonitor.7z %data_path%

cd /d %~dp0%

%installer_framework%\binarycreator.exe --offline-only -c Config\Config.xml -p Packages BuildMonitorInstaller.exe
if %errorlevel% neq 0 (
	echo Failed creating installer.
	pause
	exit /b 1
)

