#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif

#define MyAppName "PA System"
#define MyAppPublisher "NDT PA"
#define MyAppExeName "PA.exe"

[Setup]
AppId={{69E9FB38-FA37-4ED0-B911-ED7BF1552A91}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\PA
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\installer-output
OutputBaseFilename=PA-Setup-{#MyAppVersion}-x64
SetupIconFile=..\resources\logo.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加快捷方式："

[Files]
Source: "..\dist\PA\*"; DestDir: "{app}"; Excludes: "params\*"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\PA\params\*"; DestDir: "{app}\params"; Flags: ignoreversion recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall skipifsourcedoesntexist

[Dirs]
Name: "{app}\params"; Flags: uninsneveruninstall
Name: "{app}\data"; Flags: uninsneveruninstall
Name: "{app}\logs"; Flags: uninsneveruninstall
Name: "{app}\screenshots"; Flags: uninsneveruninstall

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "运行 {#MyAppName}"; Flags: nowait postinstall skipifsilent
