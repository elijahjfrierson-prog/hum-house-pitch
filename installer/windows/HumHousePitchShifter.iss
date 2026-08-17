; Inno Setup script for HumHouse Pitch Shifter.
; Produces HumHouse-Pitch-Shifter-Windows-Setup.exe, which shows the EULA, then
; installs the VST3 into C:\Program Files\Common Files\VST3\ and the Standalone
; into Program Files. No manual copying, no extra steps.
;
; Run from the repository root on Windows after a Release build:
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\windows\HumHousePitchShifter.iss

#define MyAppName "HumHouse Pitch Shifter"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "HumHouse"
#define MyAppURL "https://github.com/elijahjfrierson-prog/hum-house-pitch"
#define MyAppExeName "HumHouse Pitch Shifter.exe"

[Setup]
AppId={{6C2F41A8-9D53-4B7E-A1F4-HUMHOUSEPITCH}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\HumHouse\HumHouse Pitch Shifter
DefaultGroupName=HumHouse
DisableProgramGroupPage=yes
; The EULA pane: the wizard will not continue until the user accepts.
LicenseFile=..\LICENSE.txt
OutputDir=..\..\
OutputBaseFilename=HumHouse-Pitch-Shifter-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}

; An install always replaces whatever was there before: the AppId makes Inno
; upgrade the previous entry in place, and these deletions clear plug-in copies
; an older setup may have left in a different location, so a host never sees two
; competing bundles with the same plug-in ID.
[InstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\HumHouse Pitch Shifter.vst3"
Type: filesandordirs; Name: "{commoncf32}\VST3\HumHouse Pitch Shifter.vst3"
Type: filesandordirs; Name: "{localappdata}\Programs\Common\VST3\HumHouse Pitch Shifter.vst3"
Type: files; Name: "{app}\{#MyAppExeName}"

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut for HumHouse Pitch Shifter (Standalone)"; GroupDescription: "Additional shortcuts:"

[Files]
; Standalone .exe -> Program Files\HumHouse\HumHouse Pitch Shifter\
Source: "..\..\build\HumHousePitch_artefacts\Release\Standalone\HumHouse Pitch Shifter.exe"; \
  DestDir: "{app}"; Flags: ignoreversion

; VST3 bundle -> C:\Program Files\Common Files\VST3\HumHouse Pitch Shifter.vst3\
; A VST3 is a *folder*, so recurse the whole bundle.
Source: "..\..\build\HumHousePitch_artefacts\Release\VST3\HumHouse Pitch Shifter.vst3\*"; \
  DestDir: "{commoncf64}\VST3\HumHouse Pitch Shifter.vst3"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

; EULA kept in the install dir for reference.
Source: "..\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\HumHouse Pitch Shifter"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall HumHouse Pitch Shifter"; Filename: "{uninstallexe}"
Name: "{commondesktop}\HumHouse Pitch Shifter"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch HumHouse Pitch Shifter Standalone"; \
  Flags: nowait postinstall skipifsilent unchecked

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nThe VST3 plug-in goes to the standard location (Common Files\VST3) so FL Studio, Ableton, Reaper, Cubase, Studio One and Bitwig find it on the next plug-in scan.%n%nThe Standalone app is installed too, so you can play guitar through it without a DAW.%n%nClose your DAW before continuing.
