; HI-Spec — Windows VST3 installer (Inno Setup 6)

#define MyAppName       "HI-Spec"
#define MyAppPublisher  "HI"
#define MyAppVersion    "0.1.0"
#define MyAppURL        "https://github.com/rossxo-m/HI-Spec"

[Setup]
AppId={{a8e3d7c5-2b94-4f6a-8c1e-5d9b7a3e2f48}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commoncf64}\VST3\{#MyAppName}.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputDir=output
OutputBaseFilename=HI-Spec-{#MyAppVersion}-windows
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\..\build\HISpec_artefacts\Release\VST3\HI-Spec.vst3\*"; \
    DestDir: "{commoncf64}\VST3\HI-Spec.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
Filename: "{cmd}"; Parameters: "/c echo Installation complete. Reload your DAW to see HI-Spec."; Flags: runhidden
