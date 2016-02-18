; OSARA: Open Source Accessibility for the REAPER Application
; NSIS installer script
; Author: James Teh <jamie@nvaccess.org>
; Copyright 2016 NV Access Limited
; License: GNU General Public License version 2.0

!include "MUI2.nsh"
!include "x64.nsh"
SetCompressor /SOLID LZMA
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
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "OSARA plug-in" SecPlugin
	SectionIn RO
	SetOutPath "$INSTDIR\UserPlugins"
	File "..\build\x86\reaper_osara32.dll"
	${If} ${RunningX64}
		File "..\build\x86_64\reaper_osara64.dll"
	${EndIf}
	CreateDirectory "$INSTDIR\osara"
	WriteUninstaller "$INSTDIR\osara\uninstall.exe"
	WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OSARA" "DisplayName" "OSARA"
	WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OSARA" "DisplayVersion" "${VERSION}"
	WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OSARA" "Publisher" "${PUBLISHER}"
	WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OSARA" "UninstallString" "$\"$INSTDIR\osara\uninstall.exe$\""
SectionEnd

Section "OSARA key map (replaces existing key map)" SecKeyMap
	SetOutPath "$INSTDIR"
	File "..\config\reaper-kb.ini"
SectionEnd

Section "Uninstall"
	Delete "$INSTDIR\..\UserPlugins\reaper_osara32.dll"
	Delete "$INSTDIR\..\UserPlugins\reaper_osara64.dll"
	Delete "$INSTDIR\uninstall.exe"
	RMDir "$INSTDIR"
	DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OSARA"
SectionEnd
