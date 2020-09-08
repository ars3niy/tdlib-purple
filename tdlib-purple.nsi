; Script based on the telegram-purple NSI files

SetCompressor /SOLID /FINAL lzma

; todo: SetBrandingImage
; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "tdlib-purple"
!define PRPL_INSTALL_TARGET "libtelegram-tdlib.dll"
!define PRODUCT_VERSION "${PLUGIN_VERSION}"
!define PRODUCT_PUBLISHER "The ${PRODUCT_NAME} team"
!define PRODUCT_WEB_SITE "https://github.com/ars3niy/tdlib-purple"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!insertmacro MUI_PAGE_LICENSE "LICENSE"
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Run Pidgin"
!define MUI_FINISHPAGE_RUN_FUNCTION "RunPidgin"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
;!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${BUILD_DIR}\${PRODUCT_NAME}-${PRODUCT_VERSION}.exe"

Var "PidginDir"

ShowInstDetails show
ShowUnInstDetails show

Section "MainSection" SEC01
    ;Check for pidgin installation
    Call GetPidginInstPath
    
    SetOverwrite try
    
	SetOutPath "$PidginDir\pixmaps\pidgin"
	File "/oname=protocols\16\telegram.png" "data\telegram16.png"
	File "/oname=protocols\22\telegram.png" "data\telegram22.png"
	File "/oname=protocols\48\telegram.png" "data\telegram48.png"

    SetOverwrite try
	copy:
		ClearErrors
		Delete "$PidginDir\plugins\${PRPL_INSTALL_TARGET}"
		IfErrors dllbusy
		SetOutPath "$PidginDir\plugins"
	    File "/oname=${PRPL_INSTALL_TARGET}" "${BUILD_DIR}\${PRPL_LIBRARY}"
		Goto after_copy
	dllbusy:
		MessageBox MB_RETRYCANCEL "${PRPL_INSTALL_TARGET} is busy. Please close Pidgin (including tray icon) and try again" IDCANCEL cancel
		Goto copy
	cancel:
		Abort "Installation of ${PRODUCT_NAME} aborted"
	after_copy:
	
	;SetOutPath "$PidginDir\locale"
	;File /nonfatal "/oname=變\LC_MESSAGES\telegram-purple.mo" "po\變.mo"
	
SectionEnd

Function GetPidginInstPath
  Push $0
  ReadRegStr $0 HKLM "Software\pidgin" ""
	IfFileExists "$0\pidgin.exe" cont
	ReadRegStr $0 HKCU "Software\pidgin" ""
	IfFileExists "$0\pidgin.exe" cont
		MessageBox MB_OK|MB_ICONINFORMATION "Failed to find Pidgin installation."
		Abort "Failed to find Pidgin installation. Please install Pidgin first."
  cont:
	StrCpy $PidginDir $0
FunctionEnd

Function RunPidgin
	ExecShell "" "$PidginDir\pidgin.exe"
FunctionEnd

