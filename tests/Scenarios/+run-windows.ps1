# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

[CmdletBinding()]
param(
    [string] $ObsExecutable = $env:GUI_SCENARIO_OBS_EXECUTABLE,
    [string] $PluginRoot = $env:GUI_SCENARIO_PLUGIN_ROOT
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ($env:OS -ne 'Windows_NT') {
    throw 'This test must run natively on Windows.'
}

$scenarioDir = $PSScriptRoot
$sourceDir = (Resolve-Path (Join-Path $scenarioDir '..\..')).Path

if (-not $ObsExecutable) {
    $candidates = @(
        (Join-Path $env:ProgramFiles 'obs-studio\bin\64bit\obs64.exe'),
        $(if (${env:ProgramFiles(x86)}) {
            Join-Path ${env:ProgramFiles(x86)} 'obs-studio\bin\64bit\obs64.exe'
        })
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }
    $ObsExecutable = $candidates | Select-Object -First 1
}
if (-not $ObsExecutable -or -not (Test-Path -LiteralPath $ObsExecutable -PathType Leaf)) {
    throw 'obs64.exe was not found. Pass -ObsExecutable or set GUI_SCENARIO_OBS_EXECUTABLE.'
}

if (-not $PluginRoot) {
    $PluginRoot = Join-Path $sourceDir 'release\obs-backgroundremoval'
}
if (-not (Test-Path -LiteralPath (Join-Path $PluginRoot 'bin\64bit\obs-backgroundremoval.dll') -PathType Leaf)) {
    throw 'The built Windows plugin was not found. Run bin\build.bat or pass -PluginRoot.'
}

$artifactsRoot = Join-Path $sourceDir 'artifacts\scenario-tests\windows'
$runName = '{0}-run-{1}' -f [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'), ([guid]::NewGuid().ToString('N'))
$artifacts = Join-Path $artifactsRoot $runName
$workDir = Join-Path ([IO.Path]::GetTempPath()) "obs-backgroundremoval-$runName"
$portableRoot = Join-Path $workDir 'obs-portable'
$obsConfig = Join-Path $portableRoot 'config\obs-studio'
$profileDir = Join-Path $obsConfig 'basic\profiles\obs-backgroundremoval-dev-720p'
$scenesDir = Join-Path $obsConfig 'basic\scenes'
$testPluginRoot = Join-Path $workDir 'test-plugin'
$portablePluginBin = Join-Path $testPluginRoot 'bin\64bit'
$portablePluginDataRoot = Join-Path $testPluginRoot 'data\obs-plugins'
$portablePluginData = Join-Path $portablePluginDataRoot 'obs-backgroundremoval'
$installedObsRoot = Split-Path (Split-Path (Split-Path (Resolve-Path -LiteralPath $ObsExecutable).Path))
$portableObsExecutable = Join-Path $portableRoot 'bin\64bit\obs64.exe'

$portableDataRoot = Join-Path $portableRoot 'data'
New-Item -ItemType Directory -Force -Path $artifacts, $profileDir, $scenesDir, $portableDataRoot, `
    $portablePluginBin, $portablePluginData | Out-Null
New-Item -ItemType Junction -Path (Join-Path $portableRoot 'bin') `
    -Target (Join-Path $installedObsRoot 'bin') | Out-Null
New-Item -ItemType Junction -Path (Join-Path $portableRoot 'obs-plugins') `
    -Target (Join-Path $installedObsRoot 'obs-plugins') | Out-Null
foreach ($dataDirectory in @('libobs', 'obs-scripting', 'obs-studio')) {
    New-Item -ItemType Junction -Path (Join-Path $portableRoot "data\$dataDirectory") `
        -Target (Join-Path $installedObsRoot "data\$dataDirectory") | Out-Null
}
Copy-Item -LiteralPath (Join-Path $PluginRoot 'bin\64bit\obs-backgroundremoval.dll') `
    -Destination $portablePluginBin -Force
Get-ChildItem -LiteralPath (Join-Path $PluginRoot 'data') |
    Copy-Item -Destination $portablePluginData -Recurse -Force
Copy-Item -LiteralPath (Join-Path $sourceDir 'tests\Scenarios\0010-profile-720p-basic.ini') `
    -Destination (Join-Path $profileDir 'basic.ini')
Copy-Item -LiteralPath (Join-Path $sourceDir 'tests\Scenarios\0011-720p-user.ini') `
    -Destination (Join-Path $obsConfig 'user.ini')

$resultPath = Join-Path $artifacts 'result.txt'
$status = 1
$previousPluginsPath = $env:OBS_PLUGINS_PATH
$previousPluginsDataPath = $env:OBS_PLUGINS_DATA_PATH
try {
    $env:OBS_PLUGINS_PATH = $portablePluginBin
    $env:OBS_PLUGINS_DATA_PATH = $portablePluginDataRoot
    & (Join-Path $scenarioDir '2000_windows-about-dialog.test.ps1') `
        -ObsExecutable $portableObsExecutable `
        -SourceDir $sourceDir `
        -WorkDir $workDir `
        -Artifacts $artifacts
    $status = 0
} catch {
    $_ | Out-String | Set-Content -LiteralPath (Join-Path $artifacts 'exception.txt') -Encoding utf8
    throw
} finally {
    $env:OBS_PLUGINS_PATH = $previousPluginsPath
    $env:OBS_PLUGINS_DATA_PATH = $previousPluginsDataPath
    "exit_code=$status" | Set-Content -LiteralPath $resultPath -Encoding ascii
    if (Test-Path -LiteralPath $obsConfig) {
        $savedConfig = Join-Path $artifacts 'config'
        New-Item -ItemType Directory -Force -Path $savedConfig | Out-Null
        foreach ($relativePath in @('plugin_config', 'logs', 'user.ini')) {
            $sourcePath = Join-Path $obsConfig $relativePath
            if (Test-Path -LiteralPath $sourcePath) {
                Copy-Item -LiteralPath $sourcePath -Destination $savedConfig -Recurse -Force
            }
        }
    }
    Remove-Item -LiteralPath $workDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "Windows scenario artifacts: $artifacts"
}
