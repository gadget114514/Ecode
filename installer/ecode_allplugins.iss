; Inno Setup script for Ecode with all plugins
#define MyAppName "Ecode"
#define MyAppVersion "1.0"
#define MyAppPublisher "Ecode Team"
#define MyAppURL "https://github.com/user/Ecode"
#define MyAppExeName "ecode.exe"

[Setup]
AppId={{E5A6C3A1-7D4F-4B7E-9F9A-B8B8C8D8E8F9}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
OutputDir=..\bin
OutputBaseFilename=EcodeAllPluginsSetup
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Main application
Source: "..\bin\Release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\doc\*"; DestDir: "{app}\doc"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\scripts\*"; DestDir: "{app}\scripts"; Flags: ignoreversion recursesubdirs createallsubdirs

; Plugins (root level for backward compatibility)
Source: "..\bin\Release\Dired.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\Release\CSVEditor.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\Release\FastFileSearch.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\Release\FastFD.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\bin\Release\Terminal.exe"; DestDir: "{app}"; Flags: ignoreversion
#ifexist "..\bin\Release\JYEditor.exe"
Source: "..\bin\Release\JYEditor.exe"; DestDir: "{app}"; Flags: ignoreversion
#endif

; Plugins (plugins subdirectory)
Source: "..\bin\Release\Dired.exe"; DestDir: "{app}\plugins"; DestName: "Dired.exe"; Flags: ignoreversion
Source: "..\bin\Release\CSVEditor.exe"; DestDir: "{app}\plugins"; DestName: "CSVEditor.exe"; Flags: ignoreversion
Source: "..\bin\Release\FastFileSearch.exe"; DestDir: "{app}\plugins"; DestName: "FastFileSearch.exe"; Flags: ignoreversion
Source: "..\bin\Release\FastFD.exe"; DestDir: "{app}\plugins"; DestName: "FastFD.exe"; Flags: ignoreversion
Source: "..\bin\Release\Terminal.exe"; DestDir: "{app}\plugins"; DestName: "Terminal.exe"; Flags: ignoreversion
#ifexist "..\bin\Release\JYEditor.exe"
Source: "..\bin\Release\JYEditor.exe"; DestDir: "{app}\plugins"; DestName: "JYEditor.exe"; Flags: ignoreversion
#endif

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
