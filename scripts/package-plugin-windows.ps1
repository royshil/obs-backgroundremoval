param (
    [string]$Target = "x64",
    [string]$Configuration = "RelWithDebInfo",
    [string]$OutputDir = "release"
)

$ErrorActionPreference = 'Stop'

# 1. Locate and read buildspec.json
$BuildDir = "build_$Target"
$ScriptDir = $PSScriptRoot
$RootDir = Split-Path -Parent $ScriptDir
$BuildSpecFile = Join-Path $RootDir "buildspec.json"

if (-not (Test-Path $BuildSpecFile)) {
    Write-Error "buildspec.json not found at $BuildSpecFile"
    exit 1
}

try {
    Write-Host "Reading buildspec.json..."
    $Spec = Get-Content $BuildSpecFile -Raw | ConvertFrom-Json
    $PluginName = $Spec.name
    $PluginVersion = $Spec.version
}
catch {
    Write-Error "Failed to parse buildspec.json: $_"
    exit 1
}

if (-not $PluginName -or -not $PluginVersion) {
    Write-Error "Could not find 'name' or 'version' in buildspec.json"
    exit 1
}

Write-Host "Packaging Plugin: $PluginName ($PluginVersion)"
Write-Host "Output Directory: $OutputDir"

# 2. Prepare output directory
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Generate a unique temporary directory path
$TempBase = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid().ToString())
$InstallDir = Join-Path $TempBase "package"
$SymbolsDir = Join-Path $TempBase "symbols"

Write-Host "Using temporary working directory: $TempBase"

try {
    # 3. Run installation
    Write-Host "Installing to temporary directory..."
    # Ensure the parent temp directory exists
    if (-not (Test-Path $TempBase)) { New-Item -ItemType Directory -Path $TempBase -Force | Out-Null }

    cmake --install $BuildDir --config $Configuration --prefix $InstallDir
    if ($LASTEXITCODE -ne 0) { throw "CMake install failed" }

    # 4. Separate symbol files (PDBs)
    Write-Host "Separating PDB files..."
    New-Item -ItemType Directory -Path $SymbolsDir -Force | Out-Null

    Get-ChildItem -Path $InstallDir -Filter "*.pdb" -Recurse | ForEach-Object {
        $RelativePath = $_.DirectoryName.Substring($InstallDir.Length).TrimStart('\', '/')
        $DestDir = Join-Path $SymbolsDir $RelativePath

        if (-not (Test-Path $DestDir)) {
            New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
        }

        Move-Item -Path $_.FullName -Destination $DestDir
    }

    # 5. Create Zip archives (from temp dir to OutputDir)
    Write-Host "Creating Archives..."

    $MainZip = Join-Path $OutputDir "$PluginName-$PluginVersion-windows-x64.zip"
    if (Test-Path $MainZip) { Remove-Item $MainZip -Force }

    # Copy scripts/windows files to a temp root for zipping
    $TempRoot = Join-Path $TempBase "ziproot"
    New-Item -ItemType Directory -Path $TempRoot -Force | Out-Null

    # Copy plugin install files (preserve structure)
    Copy-Item -Path "$InstallDir\*" -Destination $TempRoot -Recurse -Force

    # Copy scripts/windows files to root of zip
    $ScriptsWin = Join-Path $ScriptDir "windows"
    if (Test-Path $ScriptsWin) {
        Get-ChildItem -Path $ScriptsWin -File | ForEach-Object {
            Copy-Item $_.FullName -Destination $TempRoot -Force
        }
    }

    Compress-Archive -Path "$TempRoot/*" -DestinationPath $MainZip -Force

    $PdbZip = Join-Path $OutputDir "$PluginName-$PluginVersion-windows-x64-pdb.zip"
    if (Test-Path $PdbZip) { Remove-Item $PdbZip -Force }
    Compress-Archive -Path "$SymbolsDir/*" -DestinationPath $PdbZip -Force

    Write-Host "Packaging complete."
    Write-Host "  Binary: $MainZip"
    Write-Host "  Symbols: $PdbZip"

}
catch {
    Write-Error "Packaging failed: $_"
    exit 1
}
finally {
    # 6. Cleanup
    # Remove the temporary directory regardless of success or failure
    if (Test-Path $TempBase) {
        Write-Host "Cleaning up temporary directory..."
        Remove-Item -Path $TempBase -Recurse -Force
    }
}
