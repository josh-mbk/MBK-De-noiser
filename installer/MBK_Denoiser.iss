; Installeur Windows - MBK De-noiser (VST3)
; Compilation : iscc /DAppVersion=1.0.0 installer\MBK_Denoiser.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#define AppName      "MBK De-noiser"
#define AppPublisher "MBK"
#define AppURL       "https://github.com/josh-mbk/MBK-De-noiser"
#ifndef VstSource
  #define VstSource "..\build\MBKDenoiser_artefacts\Release\VST3\MBK De-noiser.vst3"
#endif

[Setup]
AppId={{0D691D05-FF8B-434A-8831-1428F4B761AE}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
LicenseFile=license.txt
OutputDir=..\dist
OutputBaseFilename=MBK-De-noiser-{#AppVersion}-Windows-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName={#AppName} (VST3)
VersionInfoVersion=1.0.0.0
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} VST3 Setup
; SetupIconFile=icon.ico   <- decommentez quand vous avez une icone

[Languages]
Name: "french";  MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#VstSource}\*"; DestDir: "{commoncf64}\VST3\MBK De-noiser.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\MBK De-noiser.vst3"
