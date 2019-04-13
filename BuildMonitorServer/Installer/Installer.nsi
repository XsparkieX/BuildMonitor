!include "LogicLib.nsh"
!include "Include\nsProcess.nsh"

!define VERSION "1.3.1"

Name "BuildMonitorServer"
InstallDir "$PROGRAMFILES64\BuildMonitorServer"
InstallDirRegKey HKLM "Software\BuildMonitorServer" "Install_Dir"
RequestExecutionLevel admin
LicenseData "..\..\LICENSE"
OutFile "BuildMonitorServer - ${VERSION}.exe"
Icon "..\BuildMonitor.ico"
UninstallIcon "..\BuildMonitor.ico"

Page license
Page directory
Page components
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

VIAddVersionKey "ProductName" "Build Monitor Server"
VIAddVersionKey "FileDescription" "This application adds support for volunteer to fix functionality."
VIAddVersionKey "ProductVersion" ${Version}
VIAddVersionKey "FileVersion" ${Version}

VIProductVersion ${Version}.0
VIFileVersion ${Version}.0

Section "BuildMonitor"
	${nsProcess::FindProcess} "BuildMonitorServer.exe" $R0
	${If} $R0 == 0
	    DetailPrint "Build Monitor Server is running. Closing it down"
	    ${nsProcess::CloseProcess} "BuildMonitorServer.exe" $R0
	    DetailPrint "Waiting for Build Monitor Server to close"
	    Sleep 2000
	${Else}
	    DetailPrint "Build Monitor Server was not found to be running"
	${EndIf}

	${nsProcess::Unload}

	SectionIn RO
	SetOutPath $INSTDIR
	File /r Files\*
	WriteRegStr HKLM "Software\BuildMonitorServer" "Install_Dir" "$INSTDIR"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitorServer" "DisplayName" "BuildMonitorServer"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitorServer" "UninstallString" '"$INSTDIR\uninstall.exe"'
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitorServer" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitorServer" "NoRepair" 1
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitorServer" "DisplayIcon" '"$INSTDIR\uninstall.exe"'
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitorServer" "DisplayVersion" "${Version}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitorServer" "Publisher" "XsparkieX"
	WriteUninstaller "uninstall.exe"
SectionEnd

Section "Start Menu Shortcuts"
	CreateDirectory "$SMPROGRAMS\BuildMonitorServer"
	CreateShortcut "$SMPROGRAMS\BuildMonitorServer\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
	CreateShortcut "$SMPROGRAMS\BuildMonitorServer\BuildMonitorServer.lnk" "$INSTDIR\BuildMonitorServer.exe" "" "$INSTDIR\BuildMonitorServer.exe" 0
SectionEnd


Section "Uninstall"
	${nsProcess::FindProcess} "BuildMonitorServer.exe" $R0
	${If} $R0 == 0
	    DetailPrint "Build Monitor Server is running. Closing it down"
	    ${nsProcess::CloseProcess} "BuildMonitorServer.exe" $R0
	    DetailPrint "Waiting for Build Monitor Serverto close"
	    Sleep 2000
	${Else}
	    DetailPrint "Build Monitor Server was not found to be running"
	${EndIf}

	${nsProcess::Unload}

	${nsProcess::Unload}
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitorServer"
	DeleteRegKey HKLM "Software\BuildMonitorServer"

	Delete "$INSTDIR\*.*"
	RMDir "$INSTDIR"

	Delete "$SMPROGRAMS\BuildMonitorServer\*.*"
	RMDir "$SMPROGRAMS\BuildMonitorServer"
SectionEnd
