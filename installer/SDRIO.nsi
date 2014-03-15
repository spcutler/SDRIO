Name "SDRIO"

OutFile "SDRIOInstaller.exe"

!define BUILDTYPE "Release"

InstallDir $PROGRAMFILES\SDRIO

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\Scott Cutler\SDRIO" "Install_Dir"

; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------

; Pages

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------

; The stuff to install
Section "SDRIO (required)"

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Put file there
  File "..\${BUILDTYPE}\SDRIO_bladeRF.dll"
  File "..\${BUILDTYPE}\SDRIO_FUNcube.dll"
  File "..\${BUILDTYPE}\SDRIO_RTLSDR.dll"
  File "..\${BUILDTYPE}\SDRIO_Mirics.dll"
  
  File "..\3rdparty\libusb\MS32\dll\libusb-1.0.dll"
  File "..\3rdparty\pthreads\dll\pthreadVC2.dll"
  File "..\3rdparty\rtl-sdr\x32\pthreadVC2-w32.dll"

  File "..\SDRIO\sdrio_ext.h"
  
  File "..\LICENSE.txt"
  
  ; Write the installation path into the registry
  WriteRegStr HKLM "SOFTWARE\Scott Cutler\SDRIO" "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SDRIO" "DisplayName" "SDRIO"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SDRIO" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SDRIO" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SDRIO" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
  
SectionEnd

Section /o "bladeRF drivers (download)"

    StrCpy $1 "$TEMP\bladerf_win_installer.exe"
    NSISdl::download /TIMEOUT=30000 http://nuand.com/downloads/bladerf_win_installer.exe $1
    Pop $0
    
    StrCmp $0 success success
        SetDetailsView show
        DetailPrint "download failed: $1"
        Abort
    success:
        ExecWait '"$1"'
        Delete $1
        CopyFiles "$PROGRAMFILES\bladeRF\x86\bladeRF.dll" $INSTDIR
        CopyFiles "$PROGRAMFILES\bladeRF\hostedx40.rbf" $INSTDIR
        CopyFiles "$PROGRAMFILES\bladeRF\hostedx115.rbf" $INSTDIR
  
SectionEnd

Section /o "Zadig (download) for RTL-SDR and Mirics devices"

    File /r "zadig"

    StrCpy $1 "$TEMP\zadig_2.1.0.exe"
    NSISdl::download /TIMEOUT=30000 http://zadig.akeo.ie/downloads/zadig_2.1.0.exe $1
    Pop $0
    
    StrCmp $0 success success
        SetDetailsView show
        DetailPrint "download failed: $1"
        Abort
    success:
        ExecShell "open" "zadig\instructions.html"
        ExecWait '"$1"'
        Delete $1
  
SectionEnd

Section /o "MSVC 2010 runtime (download)"

    StrCpy $1 "$TEMP\vcredist_2010_x86.exe"
    NSISdl::download /TIMEOUT=30000 "http://download.microsoft.com/download/5/B/C/5BC5DBB3-652D-4DCE-B14A-475AB85EEF6E/vcredist_x86.exe" $1
    Pop $0
    
    StrCmp $0 success success
        SetDetailsView show
        DetailPrint "download failed: $1"
        Abort
    success:
        ExecWait '"$1" /passive /norestart'
        Delete $1

SectionEnd

Function .onInstSuccess
FunctionEnd

;--------------------------------
; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SDRIO"
  DeleteRegKey HKLM "SOFTWARE\Scott Cutler\SDRIO"

  ; Remove files and uninstaller
  Delete $INSTDIR\*.*

  ; Remove directories used
  RMDir "$SMPROGRAMS\SDRIO"
  RMDir "$INSTDIR"

SectionEnd
