!include "LogicLib.nsh"
!include "Include\nsProcess.nsh"

!define VERSION "1.2.1"

Name "BuildMonitor"
InstallDir "$PROGRAMFILES64\BuildMonitor"
InstallDirRegKey HKLM "Software\BuildMonitor" "Install_Dir"
RequestExecutionLevel admin
LicenseData "..\..\LICENSE"
OutFile "BuildMonitor - ${VERSION}.exe"

Page license
Page directory
Page components
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "BuildMonitor"
	${nsProcess::FindProcess} "BuildMonitor.exe" $R0
	${If} $R0 == 0
	    DetailPrint "Build Monitor is running. Closing it down"
	    ${nsProcess::CloseProcess} "BuildMonitor.exe" $R0
	    DetailPrint "Waiting for Build Monitor to close"
	    Sleep 2000
	${Else}
	    DetailPrint "Build Monitor was not found to be running"
	${EndIf}

	${nsProcess::Unload}

	SectionIn RO
	SetOutPath $INSTDIR
	File /r Files\*
	WriteRegStr HKLM "Software\BuildMonitor" "Install_Dir" "$INSTDIR"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitor" "DisplayName" "BuildMonitor"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitor" "UninstallString" '"$INSTDIR\uninstall.exe"'
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitor" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitor" "NoRepair" 1
	WriteUninstaller "uninstall.exe"
SectionEnd

Section "Start Menu Shortcuts"
	CreateDirectory "$SMPROGRAMS\BuildMonitor"
	CreateShortcut "$SMPROGRAMS\BuildMonitor\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
	CreateShortcut "$SMPROGRAMS\BuildMonitor\BuildMonitor.lnk" "$INSTDIR\BuildMonitor.exe" "" "$INSTDIR\BuildMonitor.exe" 0
SectionEnd


Section "Uninstall"
	${nsProcess::FindProcess} "BuildMonitor.exe" $R0
	${If} $R0 == 0
	    DetailPrint "Build Monitor is running. Closing it down"
	    ${nsProcess::CloseProcess} "BuildMonitor.exe" $R0
	    DetailPrint "Waiting for Build Monitor to close"
	    Sleep 2000
	${Else}
	    DetailPrint "Build Monitor was not found to be running"
	${EndIf}

	${nsProcess::Unload}

	${nsProcess::Unload}
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\BuildMonitor"
	DeleteRegKey HKLM "Software\BuildMonitor"

	Delete "$INSTDIR\*.*"
	RMDir "$INSTDIR"

	Delete "$SMPROGRAMS\BuildMonitor\*.*"
	RMDir "$SMPROGRAMS\BuildMonitor"
SectionEnd
