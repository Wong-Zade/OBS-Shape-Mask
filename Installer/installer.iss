#ifndef Version
#define Version "0.0.0"
#endif

[Setup]
AppId={{8E5A8B40-6C9A-4F4E-9A15-OBSSHAPEMASKID}}
AppName=OBS Shape Mask
AppVersion={#Version}
AppPublisher=Wong-Zade
DefaultDirName={autopf}\obs-studio
DisableProgramGroupPage=yes
OutputDir=..\installer-output
OutputBaseFilename=OBS-Shape-Mask-Setup-{#Version}
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=icon.ico
WizardImageFile=wizard_large.bmp
WizardSmallImageFile=wizard_small.bmp
WizardImageStretch=no

[Files]
; The plugin DLL and ALL of its data files (effect + locale) -- fixing the
; earlier version of this installer, which only ever packaged the DLL.
Source: "stage\obs-plugins\64bit\*"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "stage\data\obs-plugins\obs-shape-mask\*"; DestDir: "{app}\data\obs-plugins\obs-shape-mask"; Flags: ignoreversion recursesubdirs createallsubdirs

; Files installed by [Files] above are removed automatically on uninstall,
; so only the plugin's data folder needs cleaning up explicitly.
[UninstallDelete]
Type: filesandordirs; Name: "{app}\data\obs-plugins\obs-shape-mask"
