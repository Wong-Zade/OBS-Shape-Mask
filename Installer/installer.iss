#ifndef Version
#define Version "0.0.0"
#endif
#ifndef ProductName
#define ProductName "obs-shape-mask"
#endif
#ifndef StageDir
#define StageDir "stage"
#endif

[Setup]
AppId={{8E5A8B40-6C9A-4F4E-9A15-OBSSHAPEMASKID}}
AppName=OBS Shape Mask
AppVersion={#Version}
AppPublisher=Wong-Zade
DefaultDirName={autopf}\obs-studio
DisableProgramGroupPage=yes
OutputBaseFilename={#ProductName}-{#Version}-windows-x64-setup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=icon.ico
WizardImageFile=wizard_large.bmp
WizardSmallImageFile=wizard_small.bmp
; Inno Setup skips the Welcome page by default, which would hide the large banner
; until the very last page. Turn it on so the banner shows from the start.
DisableWelcomePage=no

[Files]
; The plugin DLL and ALL of its data files (effect + locale) -- fixing the
; earlier version of this installer, which only ever packaged the DLL.
Source: "{#StageDir}\obs-plugins\64bit\*"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "{#StageDir}\data\obs-plugins\{#ProductName}\*"; DestDir: "{app}\data\obs-plugins\{#ProductName}"; Flags: ignoreversion recursesubdirs createallsubdirs

; Files installed by [Files] above are removed automatically on uninstall,
; so only the plugin's data folder needs cleaning up explicitly.
[UninstallDelete]
Type: filesandordirs; Name: "{app}\data\obs-plugins\{#ProductName}"
