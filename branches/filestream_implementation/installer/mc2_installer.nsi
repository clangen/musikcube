SetCompressor /SOLID lzma

!include "MUI.nsh"


!define TRUE 1
!define FALSE 0

!define MUI_BGCOLOR "FFFFFF"

!define PROJECT_NAME "musikCube 2"
!define SUB_NAME "developers milestone 2"
!define INSTALLER_NAME "mC2dm2"
!define INSTALL_DIR "musikCube 2"

;----------------------------------------------------------------
OutFile ".\${INSTALLER_NAME} Setup.exe"

Name "${PROJECT_NAME} ${SUB_NAME}"

ShowInstDetails show	;show/hide

!include "LanguageStrings.nsh"
!insertmacro MUI_LANGUAGE "English"

InstallDir "$PROGRAMFILES\${INSTALL_DIR}"
InstallDirRegKey HKCU "Software\${INSTALL_DIR}" ""

!insertmacro MUI_PAGE_LICENSE "..\LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
;!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES


Section "mC2installation" main

	SectionIn RO

	SetShellVarContext current

	SetOverwrite on

	SetOutPath "$INSTDIR"
	File /r "..\bin\release\mC2.exe"
	File /r "..\bin\release\musikServer.exe"
	File /r "..\LICENSE.txt"

	SetOutPath "$INSTDIR\plugins"
	File /r "..\bin\release\plugins\*.dll"

	SetAutoClose false


	CreateDirectory "$SMPROGRAMS\${PROJECT_NAME} ${SUB_NAME}"
	CreateShortCut "$SMPROGRAMS\${PROJECT_NAME} ${SUB_NAME}\${PROJECT_NAME}.lnk" "$INSTDIR\mC2.exe"
	CreateShortCut "$SMPROGRAMS\${PROJECT_NAME} ${SUB_NAME}\musikServer.lnk" "$INSTDIR\musikServer.exe"

SectionEnd

Section -Post
	WriteRegStr HKCU "Software\${INSTALL_DIR}" "" $INSTDIR
	WriteUninstaller "$INSTDIR\uninst.exe"
	CreateDirectory "$SMPROGRAMS\${PROJECT_NAME} ${SUB_NAME}"
  	CreateShortCut "$SMPROGRAMS\${PROJECT_NAME} ${SUB_NAME}\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section Uninstall
	SetShellVarContext current
	RMDir /r "$INSTDIR"
	RMDir /r "$SMPROGRAMS\${PROJECT_NAME} ${SUB_NAME}"
SectionEnd

Function .onInit
FunctionEnd

