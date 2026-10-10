# Project Ambrose by Imjustchico
# The Windows setup program package.py builds: installs the prebuilt servers, tools and private MariaDB for the current user with no administrator, asks which Wizard101 install to read with the one the client tool finds already filled in, adds Start menu, desktop and sign-in shortcuts and an entry in Installed apps, and uninstalls exactly what it installed, keeping the database and settings unless asked.
Unicode true
!include MUI2.nsh
!include nsDialogs.nsh
!include LogicLib.nsh
!include FileFunc.nsh
!include x64.nsh

!define PRODUCT "Project Ambrose"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProjectAmbrose"
!define SETTINGS_KEY "Software\ProjectAmbrose"
!define POWERSHELL "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe"
!define SCRIPT '-NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\ambrose.ps1"'

Name "${PRODUCT}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\Project Ambrose"
InstallDirRegKey HKCU "${SETTINGS_KEY}" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "${PRODUCT} ${VERSION}"
VIProductVersion "${VERSION_NUMERIC}"
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "${PRODUCT} setup"
VIAddVersionKey "LegalCopyright" "Project Ambrose contributors"

Var GameDir
Var GameField
Var RemoveData
Var RemoveDataBox

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TEXT "This installs the Project Ambrose servers on this computer, ready to run, with the database they keep your accounts and wizards in.$\r$\n$\r$\nNothing is compiled and nothing else needs installing. Ambrose never downloads Wizard101: it reads the game's data from your own copy when it first starts.$\r$\n$\r$\nClick Next to continue."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}\LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
Page custom GamePage GamePageLeave
!define MUI_FINISHPAGE_TEXT "Project Ambrose is installed.$\r$\n$\r$\nStart it from the Start menu with Start Ambrose. The first start reads your Wizard101 install, which can take a while on a slow disk; later starts are quick."
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Start the Ambrose servers now"
!define MUI_FINISHPAGE_RUN_FUNCTION StartNow
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
UninstPage custom un.DataPage un.DataPageLeave
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP|MB_OK "Project Ambrose needs 64-bit Windows."
    Abort
  ${EndIf}
FunctionEnd

Function FirstField
  Exch $0
  Push $1
  Push $2
  StrCpy $1 0
  next:
    StrCpy $2 $0 1 $1
    StrCmp $2 "" done
    StrCmp $2 "$\t" done
    StrCmp $2 "$\r" done
    StrCmp $2 "$\n" done
    IntOp $1 $1 + 1
    Goto next
  done:
  StrCpy $0 $0 $1
  Pop $2
  Pop $1
  Exch $0
FunctionEnd

Section "Ambrose servers" SecServers
  SectionIn RO
  ${If} ${FileExists} "$INSTDIR\ambrose.ps1"
    DetailPrint "Stopping the Ambrose servers this setup replaces"
    nsExec::ExecToLog '"${POWERSHELL}" ${SCRIPT} stop'
    Pop $0
  ${EndIf}
  SetOutPath "$INSTDIR"
  File /r "${STAGE}\*.*"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "${SETTINGS_KEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "Project Ambrose"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "URLInfoAbout" "https://github.com/Justchicoo/Project-Ambrose"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "EstimatedSize" "$0"
  CreateDirectory "$SMPROGRAMS\${PRODUCT}"
  CreateShortcut "$SMPROGRAMS\${PRODUCT}\Start Ambrose.lnk" "${POWERSHELL}" '${SCRIPT} start' "$INSTDIR\bin\supervisor.exe" 0
  CreateShortcut "$SMPROGRAMS\${PRODUCT}\Stop Ambrose.lnk" "${POWERSHELL}" '${SCRIPT} stop' "$INSTDIR\bin\supervisor.exe" 0
  CreateShortcut "$SMPROGRAMS\${PRODUCT}\Ambrose folder.lnk" "$INSTDIR"
  CreateShortcut "$SMPROGRAMS\${PRODUCT}\Uninstall Ambrose.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section "Desktop shortcut" SecDesktop
  CreateShortcut "$DESKTOP\Start Ambrose.lnk" "${POWERSHELL}" '${SCRIPT} start' "$INSTDIR\bin\supervisor.exe" 0
SectionEnd

Section /o "Start Ambrose when I sign in" SecSignIn
  CreateShortcut "$SMSTARTUP\Start Ambrose.lnk" "${POWERSHELL}" '-WindowStyle Hidden ${SCRIPT} start -Background' "$INSTDIR\bin\supervisor.exe" 0 SW_SHOWMINIMIZED
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecServers} "The login, game and patch servers, the supervisor that runs them, the tools they use and a private MariaDB that only this computer can reach."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "A Start Ambrose shortcut on the desktop."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSignIn} "Start the servers in the background whenever you sign in to Windows."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function GamePage
  !insertmacro MUI_HEADER_TEXT "Your Wizard101 install" "Ambrose reads the game's data from your own copy of Wizard101."
  nsDialogs::Create 1018
  Pop $0
  ${If} $GameDir == ""
    nsExec::ExecToStack '"$INSTDIR\bin\client.exe" installs'
    Pop $1
    Pop $2
    ${If} $1 == 0
      Push $2
      Call FirstField
      Pop $GameDir
    ${EndIf}
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 30u "This is the folder that holds Wizard101's Data and Bin folders. The one found on this computer is filled in; change it if you play from another copy."
  Pop $0
  ${NSD_CreateDirRequest} 0 36u 76% 13u "$GameDir"
  Pop $GameField
  ${NSD_CreateBrowseButton} 78% 35u 22% 15u "Browse..."
  Pop $0
  ${NSD_OnClick} $0 BrowseGame
  ${NSD_CreateLabel} 0 60u 100% 30u "Leave the box empty to let Ambrose look for Wizard101 each time it starts, for example if you install the game later."
  Pop $0
  nsDialogs::Show
FunctionEnd

Function BrowseGame
  ${NSD_GetText} $GameField $0
  nsDialogs::SelectFolderDialog "Choose the folder that holds Wizard101's Data and Bin folders" "$0"
  Pop $0
  ${If} $0 != error
    ${NSD_SetText} $GameField $0
  ${EndIf}
FunctionEnd

Function GamePageLeave
  ${NSD_GetText} $GameField $GameDir
  StrCpy $0 $GameDir 1 -1
  ${If} $0 == "\"
    StrCpy $GameDir $GameDir -1
  ${EndIf}
  ${If} $GameDir != ""
    nsExec::ExecToStack '"${POWERSHELL}" ${SCRIPT} client "$GameDir"'
    Pop $0
    Pop $1
    ${If} $0 != 0
      MessageBox MB_ICONEXCLAMATION|MB_OK "$GameDir does not hold Wizard101. Choose the folder that holds its Data and Bin folders, or leave the box empty so Ambrose looks for it when it starts."
      Abort
    ${EndIf}
  ${EndIf}
FunctionEnd

Function StartNow
  Exec '"${POWERSHELL}" ${SCRIPT} start'
FunctionEnd

Function un.onInit
  ${GetParameters} $0
  ClearErrors
  ${GetOptions} $0 "/REMOVEDATA" $1
  ${IfNot} ${Errors}
    StrCpy $RemoveData ${BST_CHECKED}
  ${EndIf}
FunctionEnd

Function un.DataPage
  !insertmacro MUI_HEADER_TEXT "Your accounts and wizards" "Choose whether to keep them."
  nsDialogs::Create 1018
  Pop $0
  ${NSD_CreateLabel} 0 0 100% 30u "Your accounts, wizards and settings stay on this computer unless you tick the box, so installing Ambrose again picks up where you left off."
  Pop $0
  ${NSD_CreateCheckbox} 0 40u 100% 12u "Also delete my accounts, wizards and settings"
  Pop $RemoveDataBox
  nsDialogs::Show
FunctionEnd

Function un.DataPageLeave
  ${NSD_GetState} $RemoveDataBox $RemoveData
FunctionEnd

Section "Uninstall"
  nsExec::ExecToLog '"${POWERSHELL}" ${SCRIPT} stop'
  Pop $0
  RMDir /r "$INSTDIR\bin\logs"
  ${If} $RemoveData == ${BST_CHECKED}
    RMDir /r "$INSTDIR\bin\conf.d"
    Delete "$INSTDIR\bin\*.conf"
    RMDir /r "$LOCALAPPDATA\ProjectAmbrose\database"
  ${EndIf}
  !include "${UNINSTALL_LIST}"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${PRODUCT}\Start Ambrose.lnk"
  Delete "$SMPROGRAMS\${PRODUCT}\Stop Ambrose.lnk"
  Delete "$SMPROGRAMS\${PRODUCT}\Ambrose folder.lnk"
  Delete "$SMPROGRAMS\${PRODUCT}\Uninstall Ambrose.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT}"
  Delete "$DESKTOP\Start Ambrose.lnk"
  Delete "$SMSTARTUP\Start Ambrose.lnk"
  DeleteRegKey HKCU "${UNINSTALL_KEY}"
  DeleteRegKey HKCU "${SETTINGS_KEY}"
SectionEnd
