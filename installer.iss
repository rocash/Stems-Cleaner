[Setup]
AppName=Stem Cleaner Pro
AppVersion=1.0.0
AppPublisher=TuEstudio
DefaultDirName={autopf}\Stem Cleaner Pro
OutputBaseFilename=StemCleanerPro-Setup
Compression=lzma2
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin

[Files]
Source: "build\StemCleanerPro_artefacts\Release\VST3\Stem Cleaner Pro.vst3\*"; DestDir: "{commoncf64}\VST3\Stem Cleaner Pro.vst3\"; Flags: recursesubdirs createallsubdirs; Check: Is64BitInstallMode
Source: "build\StemCleanerPro_artefacts\Release\Standalone\Stem Cleaner Pro.exe"; DestDir: "{app}"; Flags: ignoreversion
