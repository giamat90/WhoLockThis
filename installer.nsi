; ============================================================================
;  WhoLockThis - NSIS Installer Script
;  Find out who's locking your files — and show them the door.
; ============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "EnVar.nsh"

; ---------------------------------------------------------------------------
;  General
; ---------------------------------------------------------------------------
Name "WhoLockThis"
OutFile "WhoLock_Setup.exe"
InstallDir "$PROGRAMFILES\WhoLockThis"
InstallDirRegKey HKLM "Software\WhoLockThis" "InstallDir"
RequestExecutionLevel admin
Unicode True

; ---------------------------------------------------------------------------
;  Version info embedded in the installer .exe
; ---------------------------------------------------------------------------
!define VERSION "1.0.0"
!define PUBLISHER "TheStandardGuy"
!define WEBSITE "https://github.com/GiaMat90/WhoLockThis"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "WhoLockThis"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "CompanyName" "${PUBLISHER}"
VIAddVersionKey "FileDescription" "WhoLockThis Installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "${PUBLISHER}"

; ---------------------------------------------------------------------------
;  Modern UI configuration
; ---------------------------------------------------------------------------
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Installer pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Language
!insertmacro MUI_LANGUAGE "English"

; ---------------------------------------------------------------------------
;  Installer Section
; ---------------------------------------------------------------------------
Section "WhoLockThis (required)" SecMain
    SectionIn RO

    ; --- Copy files ---
    SetOutPath "$INSTDIR"
    File "build\wholock.exe"
    File "README.md"
    File "LICENSE"

    ; --- Save install directory ---
    WriteRegStr HKLM "Software\WhoLockThis" "InstallDir" "$INSTDIR"

    ; --- Context menu: right-click on a FILE ---
    WriteRegStr HKCR "*\shell\WhoLockThis" "" "WhoLockThis - Who's locking this?"
    WriteRegStr HKCR "*\shell\WhoLockThis" "Icon" "$INSTDIR\wholock.exe,0"
    WriteRegStr HKCR "*\shell\WhoLockThis\command" "" '"$INSTDIR\wholock.exe" "%1"'

    ; --- Context menu: right-click on a FOLDER ---
    WriteRegStr HKCR "Directory\shell\WhoLockThis" "" "WhoLockThis - Who's locking this?"
    WriteRegStr HKCR "Directory\shell\WhoLockThis" "Icon" "$INSTDIR\wholock.exe,0"
    WriteRegStr HKCR "Directory\shell\WhoLockThis\command" "" '"$INSTDIR\wholock.exe" "%1"'

    ; --- Context menu: right-click on folder BACKGROUND ---
    WriteRegStr HKCR "Directory\Background\shell\WhoLockThis" "" "WhoLockThis - Who's locking this?"
    WriteRegStr HKCR "Directory\Background\shell\WhoLockThis" "Icon" "$INSTDIR\wholock.exe,0"
    WriteRegStr HKCR "Directory\Background\shell\WhoLockThis\command" "" '"$INSTDIR\wholock.exe" "%V"'

    ; --- Add to system PATH ---
    EnVar::SetHKLM
    EnVar::AddValue "Path" "$INSTDIR"

    ; Notify the system that environment variables changed
    SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

    ; --- Add/Remove Programs entry ---
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis" \
        "DisplayName" "WhoLockThis"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis" \
        "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis" \
        "Publisher" "${PUBLISHER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis" \
        "URLInfoAbout" "${WEBSITE}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis" \
        "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis" \
        "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis" \
        "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis" \
        "DisplayIcon" "$INSTDIR\wholock.exe,0"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis" \
        "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis" \
        "NoRepair" 1

    ; Compute installed size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis" \
        "EstimatedSize" $0

    ; --- Create uninstaller ---
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

; ---------------------------------------------------------------------------
;  Start Menu Shortcuts (optional)
; ---------------------------------------------------------------------------
Section "Start Menu Shortcut" SecStartMenu
    CreateDirectory "$SMPROGRAMS\WhoLockThis"
    CreateShortCut "$SMPROGRAMS\WhoLockThis\Uninstall WhoLockThis.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

; ---------------------------------------------------------------------------
;  Uninstaller Section
; ---------------------------------------------------------------------------
Section "Uninstall"
    ; --- Remove files ---
    Delete "$INSTDIR\wholock.exe"
    Delete "$INSTDIR\README.md"
    Delete "$INSTDIR\LICENSE"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"

    ; --- Remove context menu entries ---
    DeleteRegKey HKCR "*\shell\WhoLockThis"
    DeleteRegKey HKCR "Directory\shell\WhoLockThis"
    DeleteRegKey HKCR "Directory\Background\shell\WhoLockThis"

    ; --- Remove from system PATH ---
    EnVar::SetHKLM
    EnVar::DeleteValue "Path" "$INSTDIR"
    SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

    ; --- Remove registry keys ---
    DeleteRegKey HKLM "Software\WhoLockThis"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\WhoLockThis"

    ; --- Remove Start Menu shortcuts ---
    Delete "$SMPROGRAMS\WhoLockThis\Uninstall WhoLockThis.lnk"
    RMDir "$SMPROGRAMS\WhoLockThis"
SectionEnd

