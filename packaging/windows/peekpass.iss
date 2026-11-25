; Compile the deployment folder produced by windeployqt, not the build folder.
; Required ISCC defines: AppVersion, StageDir, InstallerOutputDir.
#ifndef AppVersion
  #error AppVersion must be supplied with /DAppVersion.
#endif
#ifndef StageDir
  #error StageDir must be supplied with /DStageDir.
#endif
#ifndef InstallerOutputDir
  #error InstallerOutputDir must be supplied with /DInstallerOutputDir.
#endif
#ifndef AppIdentity
  #define AppIdentity "{{E186D102-C702-4E70-916E-994E4E84217E}"
#endif
#ifndef StartupValueName
  #define StartupValueName "peekpass"
#endif
#ifndef OutputBase
  #define OutputBase "peekpass-" + AppVersion + "-win64-setup"
#endif

[Setup]
AppId={#AppIdentity}
AppName=peekpass
AppVersion={#AppVersion}
AppPublisher=peekpass
AppPublisherURL=https://github.com/j0n9m1n1/peekpass
AppSupportURL=https://github.com/j0n9m1n1/peekpass/issues
DefaultDirName={localappdata}\Programs\peekpass
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
DisableProgramGroupPage=yes
LicenseFile={#StageDir}\LICENSE
OutputDir={#InstallerOutputDir}
OutputBaseFilename={#OutputBase}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
#if FileExists("..\..\src\icons\peekpass.ico")
SetupIconFile=..\..\src\icons\peekpass.ico
#endif
UninstallDisplayIcon={app}\peekpass.exe
CloseApplications=force
RestartApplications=no

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{userprograms}\peekpass"; Filename: "{app}\peekpass.exe"
Name: "{userdesktop}\peekpass"; Filename: "{app}\peekpass.exe"; Tasks: desktopicon

[Registry]
; Startup remains opt-in through the app. Remove its entry on uninstall.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "{#StartupValueName}"; Flags: uninsdeletevalue

[Run]
Filename: "{app}\peekpass.exe"; Description: "Launch peekpass"; Flags: nowait postinstall

[Code]
procedure StopRunningPeekpass(const ExePath: string);
var
  ResultCode: Integer;
begin
  if (ExePath <> '') and FileExists(ExePath) then
    Exec(ExePath, '--quit', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

  { Fallback for older versions without --quit or hung tray instances }
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM peekpass.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Sleep(250);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  StopRunningPeekpass(ExpandConstant('{app}\peekpass.exe'));
  Result := '';
end;

function InitializeUninstall(): Boolean;
begin
  StopRunningPeekpass(ExpandConstant('{app}\peekpass.exe'));
  Result := True;
end;
