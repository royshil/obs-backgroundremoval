[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
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

    foreach ( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path        = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
        )
    }

    Remove-Item @RemoveArgs

    Log-Group "Archiving ${ProductName}..."

    $SourceDir = "${ProjectRoot}/release/${Configuration}/${ProductName}"
    $StagingDir = "${ProjectRoot}/release/staging-${Target}"

    if ( Test-Path -Path $StagingDir ) {
        Remove-Item -Path $StagingDir -Recurse -Force
    }
    New-Item -Path $StagingDir -ItemType Directory | Out-Null

    # Create OBS plugin structure
    $ObsPluginDir = New-Item -Path "$StagingDir/obs-plugins/64bit" -ItemType Directory -Force
    $DataPluginDir = New-Item -Path "$StagingDir/data/obs-plugins/$ProductName" -ItemType Directory -Force

    # Copy plugin binaries
    Copy-Item -Path "$SourceDir/bin/64bit/*" -Destination $ObsPluginDir -Recurse

    # Copy data files
    Copy-Item -Path "$SourceDir/data/*" -Destination $DataPluginDir -Recurse

    $CompressArgs = @{
        Path            = (Get-ChildItem -Path $StagingDir).FullName
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose         = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs

    Remove-Item -Path $StagingDir -Recurse -Force

    Log-Group
}

Package
