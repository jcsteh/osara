; OSARA: Open Source Accessibility for the REAPER Application
; NSIS installer script
; Author: James Teh <jamie@jantrid.net>
; Copyright 2016 NV Access Limited
; License: GNU General Public License version 2.0

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

SetCompressor /SOLID LZMA
Unicode true
Name "OSARA"
Caption "OSARA ${VERSION} Setup"
OutFile "${OUTFILE}"
VIProductVersion "0.0.0.0" ;Needs to be here so other version info shows up
VIAddVersionKey "ProductName" "OSARA"
VIAddVersionKey "LegalCopyright" "${COPYRIGHT}"
VIAddVersionKey "FileDescription" "OSARA installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
InstallDir "$APPDATA\REAPER\"

RequestExecutionLevel user
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_LICENSE "..\copying.txt"
Page custom portablePage portablePageLeave
!define MUI_PAGE_CUSTOMFUNCTION_PRE directoryPagePre
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Var dialog
var standardRadio
var portableRadio
var portable

Function portablePage
	nsDialogs::Create 1018
	Pop $Dialog
	${If} $Dialog == error
		Abort
	${EndIf}
	${NSD_CreateLabel} 0% 0% 100% 10% "Install into a standard or portable installation of REAPER?"
	Pop $0
	${NSD_CreateRadioButton} 10% 20% 50% 10% "&Standard installation"
	Pop $standardRadio
	${NSD_CreateRadioButton} 10% 40% 50% 10% "&Portable installation"
	Pop $portableRadio
	${If} $portable = ${BST_CHECKED}
		${NSD_Check} $portableRadio
		${NSD_Uncheck} $standardRadio
		${NSD_SetFocus} $portableRadio
	${Else}
		${NSD_Check} $standardRadio
		${NSD_Uncheck} $portableRadio
		${NSD_SetFocus} $standardRadio
	${EndIf}
	nsDialogs::Show
FunctionEnd

Function portablePageLeave
	${NSD_GetState} $portableRadio $portable
	${Unless} $portable = ${BST_CHECKED}
		StrCpy $INSTDIR "$APPDATA\REAPER\"
	${EndIf}
FunctionEnd

Function directoryPagePre
	${Unless} $portable = ${BST_CHECKED}
		Abort
	${EndIf}
FunctionEnd

Section "OSARA plug-in" SecPlugin
	SectionIn RO
	SetOutPath "$INSTDIR\UserPlugins"
	File "..\build\x86\reaper_osara32.dll"
	; Installing the 64 bit dll on a 32 bit system causes an error when REAPER starts up.
	; However, it's fine on a 64 bit system even with 32 bit REAPER.
	${If} ${RunningX64}
		File "..\build\x86_64\reaper_osara64.dll"
	${EndIf}
	SetOutPath "$INSTDIR\KeyMaps"
	File /oname=OSARA.ReaperKeyMap "..\config\windows\reaper-kb.ini"
	CreateDirectory "$INSTDIR\osara\locale"
	SetOutPath "$INSTDIR\osara\locale"
	File "..\locale\*.po"
	${Unless} $portable = ${BST_CHECKED}
		WriteUninstaller "$INSTDIR\osara\uninstall.exe"
		WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OSARA" "DisplayName" "OSARA"
		WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OSARA" "DisplayVersion" "${VERSION}"
		WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OSARA" "Publisher" "${PUBLISHER}"
		WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OSARA" "UninstallString" "$\"$INSTDIR\osara\uninstall.exe$\""
	${EndIf}
SectionEnd

Section "Replace existing key map with OSARA key map" SecKeyMap
	SetOutPath "$INSTDIR"
	Rename "reaper-kb.ini" "KeyMaps\OSARAReplacedBackup.ReaperKeyMap"
	File "..\config\windows\reaper-kb.ini"
SectionEnd

Section "Uninstall"
	Delete "$INSTDIR\..\UserPlugins\reaper_osara32.dll"
	Delete "$INSTDIR\..\UserPlugins\reaper_osara64.dll"
	Delete "$INSTDIR\..\KeyMaps\OSARA.ReaperKeyMap"
	Delete "$INSTDIR\uninstall.exe"
	RMDir "$INSTDIR"
	DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OSARA"
SectionEnd
