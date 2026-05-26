Unicode true
ManifestSupportedOS all
RequestExecutionLevel admin
SetCompressor /SOLID lzma

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

!ifndef VERSION
  !define VERSION "v0.0.0-dev"
!endif

!ifndef SOURCE_DIR
  !error "SOURCE_DIR must point to the staged CoreVideo install folder"
!endif

!ifndef OUT_FILE
  !define OUT_FILE "CoreVideo-Setup-${VERSION}.exe"
!endif

!ifndef FILE_LIST
  !define FILE_LIST "corevideo-uninstall-files.nsh"
!endif

Name "CoreVideo for OBS"
OutFile "${OUT_FILE}"
InstallDir "$PROGRAMFILES64\obs-studio"
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" "InstallLocation"
BrandingText "CoreVideo ${VERSION}"

!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "CoreVideo requires 64-bit Windows and 64-bit OBS Studio."
    Abort
  ${EndIf}

  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" "InstallLocation"
  ${If} $0 != ""
    StrCpy $INSTDIR $0
  ${EndIf}
FunctionEnd

Function .onVerifyInstDir
  IfFileExists "$INSTDIR\bin\64bit\obs64.exe" done
  IfFileExists "$INSTDIR\obs64.exe" done
  Abort
done:
FunctionEnd

Function CheckObsClosed
  nsExec::ExecToStack 'tasklist /FI "IMAGENAME eq obs64.exe" /NH'
  Pop $0
  Pop $1
  ${If} $1 != ""
  ${AndIf} $1 != "INFO: No tasks are running which match the specified criteria."
  ${AndIf} $1 != "No tasks are running which match the specified criteria."
    MessageBox MB_ICONSTOP "OBS Studio is currently running. Close OBS before installing CoreVideo."
    Abort
  ${EndIf}
FunctionEnd

Section "CoreVideo for OBS" SecCoreVideo
  Call CheckObsClosed
  SetOutPath "$INSTDIR"
  File /r "${SOURCE_DIR}\*.*"

  WriteUninstaller "$INSTDIR\Uninstall-CoreVideo.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CoreVideo for OBS" "DisplayName" "CoreVideo for OBS"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CoreVideo for OBS" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CoreVideo for OBS" "Publisher" "CoreVideo"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CoreVideo for OBS" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CoreVideo for OBS" "UninstallString" "$INSTDIR\Uninstall-CoreVideo.exe"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CoreVideo for OBS" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CoreVideo for OBS" "NoRepair" 1
SectionEnd

Section "Uninstall"
  Call un.CheckObsClosed
  !include "${FILE_LIST}"
  Delete "$INSTDIR\Uninstall-CoreVideo.exe"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CoreVideo for OBS"
SectionEnd

Function un.CheckObsClosed
  nsExec::ExecToStack 'tasklist /FI "IMAGENAME eq obs64.exe" /NH'
  Pop $0
  Pop $1
  ${If} $1 != ""
  ${AndIf} $1 != "INFO: No tasks are running which match the specified criteria."
  ${AndIf} $1 != "No tasks are running which match the specified criteria."
    MessageBox MB_ICONSTOP "OBS Studio is currently running. Close OBS before uninstalling CoreVideo."
    Abort
  ${EndIf}
FunctionEnd
