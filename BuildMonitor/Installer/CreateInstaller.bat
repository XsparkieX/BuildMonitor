@echo off

setlocal

call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
MSBuild.exe "%~dp0..\BuildMonitor.vcxproj" /nologo /t:Rebuild /p:Configuration=Release
if %errorlevel% neq 0 (
	echo Project failed to compile.
	pause
	exit /b 1
)

rd /S /Q "%~dp0\Files\"
mkdir "%~dp0\Files\"
copy "%~dp0\..\release\BuildMonitor.exe" "%~dp0\Files\"
"C:\Qt\5.9\msvc2017_64\bin\windeployqt.exe" "%~dp0\Files\BuildMonitor.exe"
if %errorlevel% neq 0 (
	echo Failed to generate dependencies.
	pause
	exit /b 1
)

cd /d %~dp0%
"C:\Program Files (x86)\NSIS\makensis.exe" Installer.nsi
if %errorlevel% neq 0 (
	echo Failed creating installer. Have you installed the plugin?
	pause
	exit /b 1
)

pause
