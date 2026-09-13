; IRONICS installer (Inno Setup 6). Driven by Tools/package_release.ps1 via /D defines:
;   ISCC.exe /DMyVersion=0.1.4-beta /DMyVersionNumeric=0.1.4.0 ^
;            /DMyStage="C:\Dev\Bag_Man\Saved\StagedBuilds\Windows" ^
;            /DMyOutDir="D:\BagMan\releases\win64\0.1.4-beta" Tools\installer\IRONICS.iss
;
; Wraps the cooked Shipping stage into a signed-capable, uninstall-clean installer. The stage's top-level
; IRONICS.exe is the launcher the shortcuts target. PDBs + build manifests are excluded (they never ship).
; No admin required (PrivilegesRequired=lowest -> per-user install, no UAC) — a beta player double-clicks and plays.

#ifndef MyVersion
  #define MyVersion "0.0.0-dev"
#endif
#ifndef MyVersionNumeric
  #define MyVersionNumeric "0.0.0.0"
#endif
#ifndef MyStage
  #define MyStage "..\..\Saved\StagedBuilds\Windows"
#endif
#ifndef MyOutDir
  #define MyOutDir "."
#endif

[Setup]
; A STABLE AppId (never change it) — it is how upgrades replace and the uninstaller finds a prior install.
AppId={{7B1E2C4A-9F3D-4E6B-8A1C-IRONICS0BAGMAN}}
AppName=IRONICS
AppVersion={#MyVersion}
AppVerName=IRONICS {#MyVersion}
VersionInfoVersion={#MyVersionNumeric}
AppPublisher=C12 AI Gaming
AppPublisherURL=https://ironics.org
DefaultDirName={autopf}\IRONICS
DefaultGroupName=IRONICS
DisableProgramGroupPage=yes
UninstallDisplayName=IRONICS {#MyVersion}
UninstallDisplayIcon={app}\IRONICS.exe
OutputDir={#MyOutDir}
OutputBaseFilename=IRONICS-{#MyVersion}-Setup
; No admin: {autopf} resolves to the per-user Programs dir under `lowest`, so no UAC prompt for a beta.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
; The payload is already Oodle-compressed IoStore (.ucas ~3.3 GB); LZMA2 non-solid keeps compile sane and
; handles the >2 GB file (Inno 6 supports it). Solid would re-stream the whole 3.3 GB for ~0 gain.
Compression=lzma2/normal
SolidCompression=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; The whole cooked stage -> {app}. Exclude debug symbols and the cook's build manifests (not shipped).
Source: "{#MyStage}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion; \
  Excludes: "*.pdb,Manifest_DebugFiles_Win64.txt,Manifest_NonUFSFiles_Win64.txt,Manifest_UFSFiles_Win64.txt"

[Icons]
Name: "{group}\IRONICS"; Filename: "{app}\IRONICS.exe"; WorkingDir: "{app}"
Name: "{group}\Uninstall IRONICS"; Filename: "{uninstallexe}"
Name: "{autodesktop}\IRONICS"; Filename: "{app}\IRONICS.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\IRONICS.exe"; Description: "{cm:LaunchProgram,IRONICS}"; Flags: nowait postinstall skipifsilent
