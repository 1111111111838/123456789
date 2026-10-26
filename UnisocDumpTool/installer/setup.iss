; ============================================================================
;  Unisoc Dump Tool - Inno Setup installer
;
;  Builds a Windows 10/11 installer that:
;    - installs the application + the sample INI,
;    - optionally installs a Unisoc download-mode driver package IF a signed
;      package is present under ..\drivers\ (see docs/DRIVERS.md),
;    - requests elevation only when needed, via the standard UAC prompt,
;    - never disables Driver Signature Enforcement or any security mechanism,
;    - lets the user continue even if the driver step is skipped/fails.
;
;  Build:  iscc installer\setup.iss
;  (expects the Release exe at ..\build\Release\UnisocDumpTool.exe by default)
; ============================================================================

#define AppName "Unisoc Dump Tool"
#define AppVersion "1.0.0"
#define AppExe "UnisocDumpTool.exe"

; Adjust if your build output differs.
#ifndef BuildDir
  #define BuildDir "..\build\Release"
#endif

[Setup]
AppId={{B2E4B7B0-6B9A-4E7E-9E2C-UNISOCDUMP0001}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Unisoc Dump Tool contributors
DefaultDirName={autopf}\UnisocDumpTool
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
OutputBaseFilename=UnisocDumpTool-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Admin is needed only for the driver step; if you ship without drivers you may
; set this to "lowest" and drop the [Run] driver line.
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
; Hebrew UI is available if the Languages\Hebrew.isl is present in your Inno install.
; Name: "he"; MessagesFile: "compiler:Languages\Hebrew.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"
Name: "installdriver"; Description: "Install Unisoc download-mode driver (if bundled and signed)"; GroupDescription: "Drivers:"; Flags: checkedonce

[Files]
Source: "{#BuildDir}\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\UnisocDumpTool.sample.ini"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion isreadme
Source: "..\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs
; Driver package (optional). Place signed .inf/.cat/.sys under ..\drivers\.
; "skipifsourcedoesntexist" keeps the installer valid when no driver is bundled.
Source: "..\drivers\*"; DestDir: "{app}\drivers"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
; Install every .inf found in {app}\drivers via Windows' own pnputil. pnputil
; installs only packages Windows trusts (signed); it will refuse untrusted ones
; and the result is surfaced, not bypassed. Runs only if the user kept the task
; AND at least one .inf was shipped.
Filename: "{sys}\pnputil.exe"; Parameters: "/add-driver ""{app}\drivers\*.inf"" /subdirs /install"; \
  StatusMsg: "Installing Unisoc download-mode driver..."; \
  Flags: runhidden waituntilterminated; \
  Check: DriversPresentAndWanted
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Best-effort driver removal is left to Windows' Device Manager / pnputil, which
; only removes a shared driver when it is unused. We do not force removal here.

[Code]
function DriversDirHasInf(): Boolean;
var
  FindRec: TFindRec;
begin
  Result := False;
  if FindFirst(ExpandConstant('{app}\drivers\*.inf'), FindRec) then
  begin
    try
      Result := True;
    finally
      FindClose(FindRec);
    end;
  end;
end;

function DriversPresentAndWanted(): Boolean;
begin
  Result := WizardIsTaskSelected('installdriver') and DriversDirHasInf();
  if WizardIsTaskSelected('installdriver') and (not DriversDirHasInf()) then
    MsgBox('No signed driver package was bundled with this installer.' + #13#10 +
           'The application will still run; install the Unisoc download-mode ' +
           'driver yourself if your device is not detected. See docs\DRIVERS.md.',
           mbInformation, MB_OK);
end;
