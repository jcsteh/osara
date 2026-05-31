; OSARA: Open Source Accessibility for the REAPER Application
; NSIS installer script
; Copyright 2016-2023 NV Access Limited, James Teh
; License: GNU General Public License version 2.0

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"
!include "WinVer.nsh"

SetCompressor /SOLID LZMA
!ifndef NSIS_UNICODE
	Unicode true
!endif
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

!define MUI_PAGE_CUSTOMFUNCTION_PRE preLicenseCheck
!insertmacro MUI_PAGE_LICENSE "..\copying.txt"
Page custom portablePage portablePageLeave
!define MUI_PAGE_CUSTOMFUNCTION_PRE directoryPagePre
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_PAGE_CUSTOMFUNCTION_SHOW finishPageShow
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

Var dialog
var standardRadio
var portableRadio
var portable
Var keymapReplaced
!define OSARA_LANG_ENGLISH 1033

!macro OSARA_LANG_STRING NAME TEXT
	; The NSIS preprocessor strips quotes from the macro argument value, so
	; LangString must add them here to keep spaces in the translated text.
	LangString ${NAME} ${OSARA_LANG_ENGLISH} "${TEXT}"
!macroend

; Translators: Displayed if the installer starts while REAPER is still running.
!insertmacro OSARA_LANG_STRING osaraReaperRunning "OSARA cannot be installed while REAPER is running. Please close REAPER then run this installer again."
; Translators: Prompt on the custom installer page where the user chooses between a standard or portable REAPER install.
!insertmacro OSARA_LANG_STRING osaraInstallTypePrompt "Install into a standard or portable installation of REAPER?"
; Translators: Option text on the custom installer page. Keep the ampersand so Windows can expose a shortcut key.
!insertmacro OSARA_LANG_STRING osaraStandardInstall "&Standard installation"
; Translators: Option text on the custom installer page. Keep the ampersand so Windows can expose a shortcut key.
!insertmacro OSARA_LANG_STRING osaraPortableInstall "&Portable installation"
; Translators: Prompt asking whether the existing REAPER key map should be replaced with the OSARA key map. Keep the $\r$\n sequences as line breaks.
!insertmacro OSARA_LANG_STRING osaraReplaceKeyMapPrompt "Do you want to replace the existing key map with the OSARA key map?$\r$\nNew users are advised to answer Yes, which will completely replace your key map with a clean copy of the OSARA key map including all latest assignments.$\r$\nAnswering No will install OSARA without modifying your key map, which may be preferable for experienced users who have prior alterations that they'd like to preserve."
; Translators: Shown on the finish page after replacing the user's key map. Keep $INSTDIR unchanged because it is replaced with the install path.
!insertmacro OSARA_LANG_STRING osaraFinishKeyMapReplaced "OSARA is installed with its latest key map. A safety backup of your prior key map has been placed in $INSTDIR\KeyMaps\OSARAReplacedBackup.ReaperKeyMap"
; Translators: Shown on the finish page when the existing key map is preserved.
!insertmacro OSARA_LANG_STRING osaraFinishKeyMapPreserved "OSARA has been installed with your current key map preserved."

!include "..\build\installerStrings.nsh"

Function preLicenseCheck
	FindWindow $0 "REAPERwnd"
	${If} $0 <> 0
		MessageBox MB_OK|MB_ICONEXCLAMATION \
			"$(osaraReaperRunning)"
		Quit
	${EndIf}
FunctionEnd

Function portablePage
	nsDialogs::Create 1018
	Pop $Dialog
	${If} $Dialog == error
		Abort
	${EndIf}
	${NSD_CreateLabel} 0% 0% 100% 10% "$(osaraInstallTypePrompt)"
	Pop $0
	${NSD_CreateRadioButton} 10% 20% 50% 10% "$(osaraStandardInstall)"
	Pop $standardRadio
	${NSD_CreateRadioButton} 10% 40% 50% 10% "$(osaraPortableInstall)"
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
		; Only copy ARM64EC on Windows 10 or later.
		; Older versions of Windows, at least Windows 7, will show an error when REAPER starts up.
		${If} ${AtLeastWin10}
			File "..\build\arm64\reaper_osara_arm64ec.dll"
		${Else}
			Delete "$INSTDIR\UserPlugins\reaper_osara_arm64ec.dll"
		${EndIf}
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
	StrCpy $keymapReplaced 0
	MessageBox MB_YESNO|MB_ICONQUESTION \
		"$(osaraReplaceKeyMapPrompt)" \
		/SD IDNO IDNO dontReplaceKeyMap
	; If we reach here, the user chose yes.
	StrCpy $keymapReplaced 1
	SetOutPath "$INSTDIR"
	delete "$INSTDIR\KeyMaps\OSARAReplacedBackup.ReaperKeyMap"
	Rename "reaper-kb.ini" "KeyMaps\OSARAReplacedBackup.ReaperKeyMap"
	File "..\config\windows\reaper-kb.ini"
	dontReplaceKeyMap:
SectionEnd

Function finishPageShow
	${If} $keymapReplaced = 1
		StrCpy $0 "$(osaraFinishKeyMapReplaced)"
	${Else}
		StrCpy $0 "$(osaraFinishKeyMapPreserved)"
	${EndIf}
	SendMessage $mui.FinishPage.Text ${WM_SETTEXT} 0 "STR:$0"
FunctionEnd

Section "Uninstall"
	Delete "$INSTDIR\..\UserPlugins\reaper_osara32.dll"
	Delete "$INSTDIR\..\UserPlugins\reaper_osara64.dll"
	Delete "$INSTDIR\..\UserPlugins\reaper_osara_arm64ec.dll"
	Delete "$INSTDIR\..\KeyMaps\OSARA.ReaperKeyMap"
	Delete "$INSTDIR\uninstall.exe"
	RMDir "$INSTDIR"
	DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\OSARA"
SectionEnd
