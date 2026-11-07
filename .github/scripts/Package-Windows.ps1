[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo',
    [switch] $Package
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
        )
    }

    Remove-Item @RemoveArgs

    Log-Group "Archiving ${ProductName}..."
    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release/${Configuration}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    Log-Group

    if ($Package) {
        $PluginInstallPath = Join-Path "${ProjectRoot}/release/${Configuration}" $ProductName
        $PluginBinaryPath = Join-Path $PluginInstallPath 'bin/64bit'
        $PluginDataPath = Join-Path $PluginInstallPath 'data'

        if (!(Test-Path -Path $PluginBinaryPath -PathType Container)) {
            throw "Plugin binaries were not found at ${PluginBinaryPath}."
        }
        if (!(Test-Path -Path $PluginDataPath -PathType Container)) {
            throw "Plugin data files were not found at ${PluginDataPath}."
        }

        $StageRoot = Join-Path "${ProjectRoot}/Installer/stage" $Configuration
        if (Test-Path -Path $StageRoot) {
            Remove-Item -Path $StageRoot -Recurse -Force
        }

        $StageBinaryPath = Join-Path $StageRoot 'obs-plugins/64bit'
        $StageDataPath = Join-Path $StageRoot "data/obs-plugins/${ProductName}"
        New-Item -ItemType Directory -Path $StageBinaryPath, $StageDataPath -Force | Out-Null
        Copy-Item -Path (Join-Path $PluginBinaryPath '*') -Destination $StageBinaryPath -Recurse -Force
        Copy-Item -Path (Join-Path $PluginDataPath '*') -Destination $StageDataPath -Recurse -Force

        $IsccCommand = Get-Command 'ISCC.exe' -ErrorAction SilentlyContinue
        $IsccPath = if ($IsccCommand) { $IsccCommand.Source } else { $null }
        if (!$IsccPath) {
            $IsccCandidates = @(
                (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6/ISCC.exe'),
                (Join-Path $env:ProgramFiles 'Inno Setup 6/ISCC.exe')
            )
            $IsccPath = $IsccCandidates | Where-Object { Test-Path -Path $_ -PathType Leaf } | Select-Object -First 1
        }
        if (!$IsccPath) {
            throw 'Inno Setup 6 compiler (ISCC.exe) was not found. Install Inno Setup 6 to create the Windows installer.'
        }

        $InstallerScript = Join-Path $ProjectRoot 'Installer/installer.iss'
        $InstallerArgs = @(
            "/DVersion=${ProductVersion}",
            "/DProductName=${ProductName}",
            "/DStageDir=stage\${Configuration}",
            "/O${ProjectRoot}/release",
            $InstallerScript
        )

        Log-Group "Creating Windows installer..."
        Invoke-External $IsccPath @InstallerArgs
        Log-Group
    }
}

Package
