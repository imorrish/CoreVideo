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

!macro RequireSourceFile RELPATH
  !if /FileExists "${SOURCE_DIR}\${RELPATH}"
  !else
    !error "SOURCE_DIR is missing required CoreVideo runtime file: ${RELPATH}"
  !endif
!macroend

!insertmacro RequireSourceFile "obs-plugins\64bit\obs-zoom-plugin.dll"
!insertmacro RequireSourceFile "obs-plugins\64bit\CoreVideoOAuthCallback.exe"
!insertmacro RequireSourceFile "obs-plugins\64bit\Qt6Core.dll"
!insertmacro RequireSourceFile "obs-plugins\64bit\Qt6Gui.dll"
!insertmacro RequireSourceFile "obs-plugins\64bit\Qt6Network.dll"
!insertmacro RequireSourceFile "obs-plugins\64bit\Qt6Widgets.dll"
!insertmacro RequireSourceFile "obs-plugins\64bit\plugins\platforms\qwindows.dll"
!insertmacro RequireSourceFile "obs-plugins\64bit\plugins\tls\qcertonlybackend.dll"
!insertmacro RequireSourceFile "obs-plugins\64bit\plugins\tls\qschannelbackend.dll"
!insertmacro RequireSourceFile "obs-plugins\64bit\zoom-runtime\ZoomObsEngine.exe"
!insertmacro RequireSourceFile "obs-plugins\64bit\zoom-runtime\sdk.dll"

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
  nsExec::ExecToStack 'cmd /C tasklist /FI "IMAGENAME eq obs64.exe" /NH ^| findstr /I /R "^obs64\.exe" >NUL'
  Pop $0
  Pop $1
  ${If} $0 == 0
    MessageBox MB_ICONSTOP "OBS Studio is currently running. Close OBS before installing CoreVideo."
    Abort
  ${EndIf}
FunctionEnd

!macro VerifyInstalledFile RELPATH
  IfFileExists "$INSTDIR\${RELPATH}" +3
  MessageBox MB_ICONSTOP "CoreVideo installation is incomplete. Missing: ${RELPATH}"
  Abort
!macroend

Function VerifyInstalledRuntime
  !insertmacro VerifyInstalledFile "obs-plugins\64bit\obs-zoom-plugin.dll"
  !insertmacro VerifyInstalledFile "obs-plugins\64bit\CoreVideoOAuthCallback.exe"
  !insertmacro VerifyInstalledFile "obs-plugins\64bit\Qt6Core.dll"
  !insertmacro VerifyInstalledFile "obs-plugins\64bit\Qt6Gui.dll"
  !insertmacro VerifyInstalledFile "obs-plugins\64bit\Qt6Network.dll"
  !insertmacro VerifyInstalledFile "obs-plugins\64bit\Qt6Widgets.dll"
  !insertmacro VerifyInstalledFile "obs-plugins\64bit\plugins\platforms\qwindows.dll"
  !insertmacro VerifyInstalledFile "obs-plugins\64bit\plugins\tls\qcertonlybackend.dll"
  !insertmacro VerifyInstalledFile "obs-plugins\64bit\plugins\tls\qschannelbackend.dll"
  !insertmacro VerifyInstalledFile "obs-plugins\64bit\zoom-runtime\ZoomObsEngine.exe"
  !insertmacro VerifyInstalledFile "obs-plugins\64bit\zoom-runtime\sdk.dll"
FunctionEnd

Section "CoreVideo for OBS" SecCoreVideo
  Call CheckObsClosed
  SetOutPath "$INSTDIR"
  File /r "${SOURCE_DIR}\*.*"
  Call VerifyInstalledRuntime

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
  nsExec::ExecToStack 'cmd /C tasklist /FI "IMAGENAME eq obs64.exe" /NH ^| findstr /I /R "^obs64\.exe" >NUL'
  Pop $0
  Pop $1
  ${If} $0 == 0
    MessageBox MB_ICONSTOP "OBS Studio is currently running. Close OBS before uninstalling CoreVideo."
    Abort
  ${EndIf}
FunctionEnd
