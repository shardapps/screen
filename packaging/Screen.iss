; Conventional per-user setup. MSIX remains a separate Store-validation artifact.
#ifndef AppBinary
  #error AppBinary must point to a built Screen.exe
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef TargetArchitecture
  #error TargetArchitecture must be x64 or arm64
#endif

[Setup]
AppId={{A373290A-4074-4C94-8C9E-F8844D3EE424}
AppName=Screen by Shardapps
AppVersion={#AppVersion}
AppPublisher=Shardapps
AppPublisherURL=https://github.com/shardapps/screen
AppSupportURL=https://github.com/shardapps/screen/issues
DefaultDirName={localappdata}\Programs\Shardapps\Screen
DefaultGroupName=Shardapps
DisableProgramGroupPage=no
PrivilegesRequired=lowest
MinVersion=10.0.22000
#if TargetArchitecture == "arm64"
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
#else
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#endif
AppMutex=Local\Shardapps.Screen.Instance
SetupMutex=Local\Shardapps.Screen.Setup
CloseApplications=no
RestartApplications=no
UninstallDisplayIcon={app}\Screen.exe
SetupIconFile=..\resources\Screen.ico
LicenseFile=..\LICENSE
OutputBaseFilename=Screen-Setup-{#AppVersion}-{#TargetArchitecture}
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
VersionInfoDescription=Screen by Shardapps Setup
VersionInfoVersion={#AppVersion}.0
SignedUninstaller=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#AppBinary}"; DestDir: "{app}"; DestName: "Screen.exe"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\*.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Screen by Shardapps"; Filename: "{app}\Screen.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Screen by Shardapps"; Filename: "{app}\Screen.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\Screen.exe"; Description: "Open Screen"; Flags: nowait postinstall skipifsilent
