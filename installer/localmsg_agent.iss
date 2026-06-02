; =============================================================================
; LocalMsg Agent Installer
; Installs localmsg.exe + localmsg-cli.exe for a named AI agent.
;
; Wizard pages (in order):
;   1. Welcome
;   2. Agent selection  — pick claude / codex / gemini / opencode / custom
;   3. Scope selection  — Global (all projects) | Local project only
;   4. Project dir      — (shown only when Local is chosen) browse for project root
;   5. Confirm / Install
; =============================================================================

#define MyAppName    "LocalMsg Agent"
#define MyAppVersion "1.0"
#define MyAppPublisher "Ecode Team"
#define MyAppURL     "https://github.com/user/Ecode"

[Setup]
AppId={{B2D4F7A3-9C1E-4D8B-A6F2-C3E5D7B9A0F1}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

DefaultDirName={autopf}\Ecode
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes

OutputDir=..\bin
OutputBaseFilename=LocalMsgAgentSetup
Compression=lzma
SolidCompression=yes
WizardStyle=modern

UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\LocalMsg.exe

[Languages]
Name: "english";  MessagesFile: "compiler:Default.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

; =============================================================================
; [Code] — All custom wizard logic
; =============================================================================
[Code]

{ ── Shared state ──────────────────────────────────────────────────────────── }
var
  { Page handles }
  AgentPage:   TWizardPage;        { page 2: agent selection }
  ScopePage:   TWizardPage;        { page 3: global vs local }
  ProjectPage: TInputDirWizardPage;{ page 4: project directory }

  { Controls on AgentPage }
  AgentCombo:  TNewComboBox;
  AgentCustomEdit: TNewEdit;
  AgentCustomLabel: TNewStaticText;

  { Controls on ScopePage }
  RadioGlobal: TNewRadioButton;
  RadioLocal:  TNewRadioButton;

  { Resolved values (set in ShouldInstallOn / GetXxx helpers) }
  FAgentName:   String;
  FScopeGlobal: Boolean;
  FProjectDir:  String;

{ ── Helper: resolve agent name from combo + optional custom edit ─────────── }
function GetAgentName(Param: String): String;
var
  Sel: String;
begin
  Sel := AgentCombo.Text;
  if Sel = 'Custom...' then
    Result := Trim(AgentCustomEdit.Text)
  else
    Result := Sel;
  if Result = '' then Result := 'claude';
  FAgentName := Result;
end;

{ ── Helper: is this a global install? ────────────────────────────────────── }
function IsGlobal: Boolean;
begin
  Result := RadioGlobal.Checked;
  FScopeGlobal := Result;
end;

{ ── Helper: skill destination ────────────────────────────────────────────── }
function SkillDestDir(Param: String): String;
begin
  if IsGlobal then
    { Global: ~/.claude/skills/ }
    Result := ExpandConstant('{userdocs}') + '\..\' +
              '.claude\skills'              { %USERPROFILE%\.claude\skills }
  else begin
    FProjectDir := ProjectPage.Values[0];
    Result := FProjectDir + '\.claude\skills';
  end;
end;

function ExeDestDir(Param: String): String;
begin
  if IsGlobal then
    Result := ExpandConstant('{app}')
  else begin
    FProjectDir := ProjectPage.Values[0];
    Result := FProjectDir + '\.localmsg';
  end;
end;

{ ── AgentCombo change handler — must be defined before CreateAgentPage ──── }
procedure AgentComboChange(Sender: TObject);
var
  IsCustom: Boolean;
begin
  IsCustom := AgentCombo.Text = 'Custom...';
  AgentCustomEdit.Visible  := IsCustom;
  AgentCustomLabel.Visible := IsCustom;
end;

{ ── Build AgentPage ─────────────────────────────────────────────────────── }
procedure CreateAgentPage;
var
  Lbl: TNewStaticText;
begin
  AgentPage := CreateCustomPage(
    wpWelcome,
    'Agent Identity',
    'Select which AI agent this installation is for');

  Lbl := TNewStaticText.Create(AgentPage);
  Lbl.Caption := 'Agent:';
  Lbl.Left    := 0;
  Lbl.Top     := 8;
  Lbl.Parent  := AgentPage.Surface;

  AgentCombo := TNewComboBox.Create(AgentPage);
  AgentCombo.Left   := 60;
  AgentCombo.Top    := 4;
  AgentCombo.Width  := 200;
  AgentCombo.Style  := csDropDownList;
  AgentCombo.Parent := AgentPage.Surface;
  AgentCombo.Items.Add('claude');
  AgentCombo.Items.Add('codex');
  AgentCombo.Items.Add('gemini');
  AgentCombo.Items.Add('opencode');
  AgentCombo.Items.Add('Custom...');
  AgentCombo.ItemIndex := 0;

  AgentCustomLabel := TNewStaticText.Create(AgentPage);
  AgentCustomLabel.Caption := 'Custom name:';
  AgentCustomLabel.Left    := 0;
  AgentCustomLabel.Top     := 40;
  AgentCustomLabel.Visible := False;
  AgentCustomLabel.Parent  := AgentPage.Surface;

  AgentCustomEdit := TNewEdit.Create(AgentPage);
  AgentCustomEdit.Left    := 90;
  AgentCustomEdit.Top     := 36;
  AgentCustomEdit.Width   := 170;
  AgentCustomEdit.Visible := False;
  AgentCustomEdit.Parent  := AgentPage.Surface;

  AgentCombo.OnChange := @AgentComboChange;
end;

{ ── Build ScopePage ─────────────────────────────────────────────────────── }
procedure CreateScopePage;
var
  Lbl: TNewStaticText;
begin
  ScopePage := CreateCustomPage(
    AgentPage.ID,
    'Installation Scope',
    'Choose where to install the messaging skill');

  Lbl := TNewStaticText.Create(ScopePage);
  Lbl.Caption :=
    'Global  — available in all projects; adds executables to PATH and' + #13#10 +
    '          installs ask-agent skill to ~/.claude/skills/' + #13#10 + #13#10 +
    'Local   — scoped to one project; executables go to <project>\.localmsg\' + #13#10 +
    '          and skill goes to <project>\.claude\skills\';
  Lbl.Left   := 0;
  Lbl.Top    := 0;
  Lbl.Width  := ScopePage.SurfaceWidth;
  Lbl.Height := 80;
  Lbl.Parent := ScopePage.Surface;

  RadioGlobal := TNewRadioButton.Create(ScopePage);
  RadioGlobal.Caption := '&Global  (all projects, adds to PATH)';
  RadioGlobal.Left    := 0;
  RadioGlobal.Top     := 90;
  RadioGlobal.Width   := ScopePage.SurfaceWidth;
  RadioGlobal.Checked := True;
  RadioGlobal.Parent  := ScopePage.Surface;

  RadioLocal := TNewRadioButton.Create(ScopePage);
  RadioLocal.Caption := '&Local project only  (choose project folder next)';
  RadioLocal.Left    := 0;
  RadioLocal.Top     := 114;
  RadioLocal.Width   := ScopePage.SurfaceWidth;
  RadioLocal.Parent  := ScopePage.Surface;
end;

{ ── Build ProjectPage ───────────────────────────────────────────────────── }
procedure CreateProjectPage;
begin
  ProjectPage := CreateInputDirPage(
    ScopePage.ID,
    'Project Directory',
    'Select the project root where the skill will be installed locally',
    'The skill file will go to <project>\.claude\skills\ask-agent.md' + #13#10 +
    'The executables will go to <project>\.localmsg\',
    False, '');
  ProjectPage.Add('Project root:');
  ProjectPage.Values[0] := GetCurrentDir;
end;

{ ── InitializeWizard ────────────────────────────────────────────────────── }
procedure InitializeWizard;
begin
  CreateAgentPage;
  CreateScopePage;
  CreateProjectPage;
end;

{ ── Skip ProjectPage when Global is selected ────────────────────────────── }
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = ProjectPage.ID then
    Result := RadioGlobal.Checked;
end;

{ ── Validate pages before proceeding ───────────────────────────────────── }
function NextButtonClick(CurPageID: Integer): Boolean;
var
  Name: String;
begin
  Result := True;

  if CurPageID = AgentPage.ID then begin
    Name := GetAgentName('');
    if Name = '' then begin
      MsgBox('Please enter a custom agent name.', mbError, MB_OK);
      Result := False;
    end;
  end;

  if CurPageID = ProjectPage.ID then begin
    if Trim(ProjectPage.Values[0]) = '' then begin
      MsgBox('Please select a project directory.', mbError, MB_OK);
      Result := False;
    end else if not DirExists(ProjectPage.Values[0]) then begin
      MsgBox('The selected directory does not exist.', mbError, MB_OK);
      Result := False;
    end;
  end;
end;

{ ── Post-install: write .agent-identity files ─────────────────────────── }
{ Creates agent identity files next to executables and in the project tree. }
procedure WriteAgentIdentityFiles;
var
  AgentName, GlobalIdFile: String;
  Lines: TArrayOfString;
begin
  AgentName := GetAgentName('');
  SetArrayLength(Lines, 1);
  Lines[0] := AgentName;

  { Global: write next to the executables }
  GlobalIdFile := ExpandConstant('{app}') + '\.agent-identity';
  SaveStringsToFile(GlobalIdFile, Lines, False);

  { Local: write into <project>\.agent\<name>\ }
  if not IsGlobal then begin
    FProjectDir := ProjectPage.Values[0];
    ForceDirectories(FProjectDir + '\.agent\' + AgentName);
    SaveStringsToFile(
      FProjectDir + '\.agent\' + AgentName + '\.agent-identity',
      Lines, False);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    WriteAgentIdentityFiles;
end;

[Files]
Source: "..\bin\Release\plugins\LocalMsg.exe"; DestDir: "{code:ExeDestDir}"; Flags: ignoreversion
Source: "..\bin\Release\plugins\localmsg-cli.exe"; DestDir: "{code:ExeDestDir}"; Flags: ignoreversion
Source: "..\Application\LocalMsg\.claude\skills\ask-agent.md"; DestDir: "{code:SkillDestDir}"; Flags: ignoreversion
Source: "..\Application\LocalMsg\SKILL.md"; DestDir: "{code:ExeDestDir}"; Flags: ignoreversion

; =============================================================================
; [Registry] — global only: add to PATH (no LOCALMSG_AGENT env var)
; =============================================================================
[Registry]
; Add exe dir to system PATH (global install, all users)
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: expandsz; ValueName: "PATH"; ValueData: "{app};{olddata}"; Check: IsGlobal; Flags: preservestringtype uninsdeletekeyifempty

; =============================================================================
; [Icons] — Start Menu entry (global only)
; =============================================================================
[Icons]
Name: "{userprograms}\LocalMsg Agent ({code:GetAgentName})"; Filename: "{app}\LocalMsg.exe"; Parameters: "--agent {code:GetAgentName}"; Check: IsGlobal

; =============================================================================
; [Run] — post-install steps
; =============================================================================
[Run]
Filename: "{code:ExeDestDir}\localmsg-cli.exe"; Parameters: "--login {code:GetAgentName}"; Flags: runhidden waituntilterminated; StatusMsg: "Registering agent {code:GetAgentName}..."
Filename: "{code:ExeDestDir}\SKILL.md"; Description: "Open usage guide (SKILL.md)"; Flags: postinstall shellexec skipifsilent unchecked

[UninstallRun]
Filename: "{app}\localmsg-cli.exe"; Parameters: "--logout {code:GetAgentName}"; Flags: runhidden waituntilterminated; RunOnceId: "LogoutAgent"
