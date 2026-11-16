# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# Runs the ST-2000 scenarios (tests/Scenarios/2000-about-dialog-on-startup.feature)
# on a GitHub Actions Windows runner. It installs the plugin ZIP into
# C:\ProgramData as users do and uses the runner's own OBS profile.

param(
    [Parameter(Mandatory)][string] $PluginZip,
    [Parameter(Mandatory)][string] $Artifacts
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ObsExecutable = 'C:\Program Files\obs-studio\bin\64bit\obs64.exe'
$ObsConfig = Join-Path $env:APPDATA 'obs-studio'
$PluginConfig = Join-Path $ObsConfig 'plugin_config\obs-backgroundremoval'
$UpdateConfig = Join-Path $PluginConfig 'update.ini'
$UpdateTemp = "$UpdateConfig.tmp"
$Version = (Get-Content -LiteralPath (Join-Path $env:GITHUB_WORKSPACE 'VERSION') -Raw).Trim()
$AboutTitle = 'About obs-backgroundremoval'

Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class TopLevelWindows
{
    private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr window, StringBuilder text, int maximumCount);

    public static IntPtr Find(uint processId, string title, bool contains)
    {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, parameter) => {
            GetWindowThreadProcessId(window, out uint windowProcessId);
            if (windowProcessId != processId) return true;
            var text = new StringBuilder(1024);
            GetWindowText(window, text, text.Capacity);
            string actual = text.ToString();
            if (contains ? actual.Contains(title) : actual == title) { found = window; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@
$UIA = [System.Windows.Automation.AutomationElement]
$Scope = [System.Windows.Automation.TreeScope]
$ControlType = [System.Windows.Automation.ControlType]

# --- Setup -------------------------------------------------------------------

New-Item -ItemType Directory -Force -Path $Artifacts | Out-Null
$Extracted = Join-Path $env:RUNNER_TEMP 'scenario-plugin-zip'
Expand-Archive -LiteralPath $PluginZip -DestinationPath $Extracted -Force
Copy-Item -LiteralPath (Join-Path $Extracted 'obs-studio') -Destination $env:ProgramData -Recurse -Force

$ProfileDir = Join-Path $ObsConfig 'basic\profiles\obs-backgroundremoval-dev-720p'
New-Item -ItemType Directory -Force -Path $ProfileDir, (Join-Path $ObsConfig 'basic\scenes'), $PluginConfig | Out-Null
Copy-Item (Join-Path $env:GITHUB_WORKSPACE 'tests\Scenarios\0010-profile-720p-basic.ini') (Join-Path $ProfileDir 'basic.ini')
Copy-Item (Join-Path $env:GITHUB_WORKSPACE 'tests\Scenarios\0011-720p-user.ini') (Join-Path $ObsConfig 'user.ini')

# --- Helpers -----------------------------------------------------------------

function Wait-Until([string] $Description, [scriptblock] $Condition, [int] $TimeoutSeconds = 30) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        try {
            $result = & $Condition
            if ($result) { return $result }
        } catch [System.Windows.Automation.ElementNotAvailableException] {
            # Qt can rebuild its accessibility tree while a dialog changes.
        }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for $Description."
}

function Find-Window([Diagnostics.Process] $Process, [string] $Title, [switch] $Contains) {
    $handle = [TopLevelWindows]::Find($Process.Id, $Title, $Contains.IsPresent)
    if ($handle -eq [IntPtr]::Zero) { return $null }
    return $UIA::FromHandle($handle)
}

function Wait-Window([Diagnostics.Process] $Process, [string] $Title) {
    Wait-Until $Title { Find-Window $Process $Title }
}

function Wait-WindowClosed([Diagnostics.Process] $Process, [string] $Title) {
    Wait-Until "$Title to close" { -not (Find-Window $Process $Title) } | Out-Null
}

function Find-Control($Root, [string] $Name, $Type) {
    $Root.FindFirst($Scope::Descendants, [System.Windows.Automation.AndCondition]::new(
            [System.Windows.Automation.PropertyCondition]::new($UIA::NameProperty, $Name),
            [System.Windows.Automation.PropertyCondition]::new($UIA::ControlTypeProperty, $Type)))
}

function Click($Root, [string] $Name) {
    $button = Find-Control $Root $Name $ControlType::Button
    if (-not $button) { throw "Button not found: $Name" }
    $button.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
}

function Get-Text($Window) {
    $parts = foreach ($element in $Window.FindAll($Scope::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) {
        $element.Current.Name
        $pattern = $null
        if ($element.TryGetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern, [ref] $pattern)) {
            $pattern.DocumentRange.GetText(-1)
        }
    }
    return $parts -join "`n"
}

function Assert-Contains([string] $Text, [string] $Expected, [string] $Where) {
    if (-not $Text.Contains($Expected)) { throw "$Where does not contain: $Expected" }
}

function Get-Checkbox($Dialog) {
    $checkbox = Find-Control $Dialog 'Notify me about new versions on its Filter Properties screen' $ControlType::CheckBox
    if (-not $checkbox) { throw 'The update-notification checkbox was not found.' }
    return $checkbox.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
}

function Test-Checked($Toggle) {
    $Toggle.Current.ToggleState -eq [System.Windows.Automation.ToggleState]::On
}

function Set-UpdateState([string[]] $Lines) {
    [IO.File]::WriteAllText($UpdateConfig, (($Lines -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))
}

function Assert-UpdateState([string] $Pattern) {
    Wait-Until "update.ini to match $Pattern" {
        try {
            [IO.File]::ReadAllText($UpdateConfig) -match "(?m)^$Pattern\r?$"
        } catch [IO.IOException] {
            # OBS may be replacing or still writing update.ini; try again.
            $false
        }
    } -TimeoutSeconds 3 | Out-Null
}

function Assert-Acknowledged([string] $Expected = $Version) {
    Assert-UpdateState "version=$([regex]::Escape($Expected))"
}

# Starts OBS, runs $Action with the process, stops OBS, and returns the OBS log text.
function Invoke-Obs([string] $Name, [scriptblock] $Action) {
    Write-Host "--- OBS run: $Name"
    $process = Start-Process -FilePath $ObsExecutable -WorkingDirectory (Split-Path $ObsExecutable) -PassThru `
        -ArgumentList '--multi', '--disable-updater', '--disable-missing-files-check', '--disable-shutdown-check'
    try {
        & $Action $process | Out-Null
    } finally {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
    }
    $log = Get-ChildItem -LiteralPath (Join-Path $ObsConfig 'logs') -Filter '*.txt' |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $Artifacts "obs-$Name.log")
    $text = Get-Content -LiteralPath $log.FullName -Raw
    Assert-Contains $text '[obs-backgroundremoval] Plugin loaded successfully' "OBS log of $Name"
    return $text
}

function Assert-AboutAbsent([Diagnostics.Process] $Process) {
    Wait-Until 'OBS main window' { Find-Window $Process 'OBS' -Contains } | Out-Null
    $deadline = [DateTime]::UtcNow.AddSeconds(2)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($Process.HasExited) { throw 'OBS exited unexpectedly.' }
        if (Find-Window $Process $AboutTitle) { throw "$AboutTitle was unexpectedly displayed." }
        Start-Sleep -Milliseconds 100
    }
}

function Accept-About([Diagnostics.Process] $Process, $ExpectChecked = $null) {
    $about = Wait-Window $Process $AboutTitle
    if ($null -ne $ExpectChecked -and (Test-Checked (Get-Checkbox $about)) -ne $ExpectChecked) {
        throw 'The update-notification checkbox did not keep its saved state.'
    }
    Click $about 'OK'
    Wait-WindowClosed $Process $AboutTitle
    Assert-Acknowledged
}

# Runs OBS twice: the About dialog is shown and accepted once, then not shown again.
function Assert-ShownOnce([string] $Name, $ExpectChecked = $null) {
    Invoke-Obs $Name { param($p) Accept-About $p $ExpectChecked } | Out-Null
    Invoke-Obs "$Name-repeat" { param($p) Assert-AboutAbsent $p } | Out-Null
}

# Runs OBS twice while saving update.ini fails: the About dialog never appears.
function Assert-NeverShown([string] $Name, [string] $ExpectedLog) {
    foreach ($attempt in 1, 2) {
        $log = Invoke-Obs "$Name-$attempt" { param($p) Assert-AboutAbsent $p }
        Assert-Contains $log $ExpectedLog "OBS log of $Name-$attempt"
    }
}

function Scenario([string] $Name, [scriptblock] $Body) {
    Write-Host "::group::$Name"
    try {
        & $Body
    } catch {
        Write-Host "::endgroup::"
        Write-Host "::error::$Name failed: $_"
        throw
    }
    Write-Host "::endgroup::"
}

# --- Scenarios ---------------------------------------------------------------

try {
    Set-UpdateState '[update]', 'check_for_updates=true'

    Scenario 'ST-2000.1-4 About dialog, Licenses, About Qt, disable notifications' {
        Invoke-Obs 'initial' {
            param($p)
            $about = Wait-Window $p $AboutTitle
            $text = Get-Text $about
            foreach ($expected in 'Portrait Background Removal / Virtual Green-screen and Low-Light Enhancement',
                "Version $Version", 'Copyright © 2021–2026 Roy Shilkrot', '© 2023–2026 Kaito Udagawa',
                'The plugin never downloads or installs updates.') {
                Assert-Contains $text $expected $AboutTitle
            }
            $checkbox = Get-Checkbox $about
            if (-not (Test-Checked $checkbox)) { throw 'The update-notification checkbox should start checked.' }

            Click $about 'Licenses'
            $licenses = Wait-Window $p 'Licenses'
            $licensesText = Get-Text $licenses
            Assert-Contains $licensesText 'GNU General Public License' 'Licenses'
            Assert-Contains $licensesText 'Apache License' 'Licenses'
            Click $licenses 'Close'
            Wait-WindowClosed $p 'Licenses'

            Click $about 'About Qt'
            $qt = Wait-Window $p 'About Qt'
            Assert-Contains (Get-Text $qt) 'Qt' 'About Qt'
            Click $qt 'OK'
            Wait-WindowClosed $p 'About Qt'

            $checkbox.Toggle()
            Wait-Until 'the checkbox to clear' { -not (Test-Checked $checkbox) } | Out-Null
            Click $about 'OK'
            Wait-WindowClosed $p $AboutTitle
            Assert-Acknowledged
            Assert-UpdateState 'check_for_updates=false'
        } | Out-Null
    }

    Scenario 'ST-2000.5 Acknowledged version is not shown again' {
        Invoke-Obs 'acknowledged' { param($p) Assert-AboutAbsent $p } | Out-Null
    }

    Scenario 'ST-2000.6 Notification preference is kept for a new version' {
        Set-UpdateState '[update]', 'version=0.0.0', 'check_for_updates=false'
        Invoke-Obs 'new-version' { param($p) Accept-About $p $false } | Out-Null
    }

    Scenario 'ST-2000.7 Notification preference cannot be saved' {
        Set-UpdateState '[update]', 'version=0.0.0', 'check_for_updates=false'
        $log = Invoke-Obs 'notification-save-failure' {
            param($p)
            $about = Wait-Window $p $AboutTitle
            $before = [IO.File]::ReadAllBytes($UpdateConfig)
            $lock = [IO.File]::Open($UpdateConfig, 'Open', 'Read', 'Read')
            try {
                Click $about 'OK'
                Wait-WindowClosed $p $AboutTitle
            } finally { $lock.Dispose() }
            if (-not [Linq.Enumerable]::SequenceEqual([byte[]]$before, [byte[]][IO.File]::ReadAllBytes($UpdateConfig))) {
                throw 'update.ini changed although saving should have failed.'
            }
        }
        Assert-Contains $log '[obs-backgroundremoval] Failed to save the update notification setting to update.ini' 'OBS log'
    }

    Scenario 'ST-2000.8 Update state cannot be opened' {
        Remove-Item -LiteralPath $UpdateConfig -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Path $UpdateConfig | Out-Null
        try { Assert-NeverShown 'update-open-failure' 'Failed to open update config' }
        finally { Remove-Item -LiteralPath $UpdateConfig -Recurse -Force }
    }

    Scenario 'ST-2000.9 Current version cannot be recorded' {
        Set-UpdateState '[update]', 'version=0.0.0', 'check_for_updates=false'
        Set-ItemProperty -LiteralPath $UpdateConfig -Name IsReadOnly -Value $true
        try { Assert-NeverShown 'update-save-failure' 'Failed to save update config' }
        finally { Set-ItemProperty -LiteralPath $UpdateConfig -Name IsReadOnly -Value $false }
        Assert-Acknowledged '0.0.0'
    }

    Scenario 'ST-2000.10 Stale temporary file is not writable' {
        Set-UpdateState '[update]', 'version=0.0.0', 'check_for_updates=false'
        [IO.File]::WriteAllText($UpdateTemp, 'stale')
        $lock = [IO.File]::Open($UpdateTemp, 'Open', 'Read', 'None')
        try { Assert-NeverShown 'unwritable-temporary-file' 'Failed to save update config' }
        finally { $lock.Dispose(); Remove-Item -LiteralPath $UpdateTemp -Force }
    }

    Scenario 'ST-2000.11-12 Stale temporary directory, then recovery' {
        Set-UpdateState '[update]', 'version=0.0.0', 'check_for_updates=false'
        New-Item -ItemType Directory -Path $UpdateTemp | Out-Null
        try { Assert-NeverShown 'temporary-directory' 'Failed to save update config' }
        finally { Remove-Item -LiteralPath $UpdateTemp -Recurse -Force }
        Assert-ShownOnce 'storage-recovered' $false
    }

    $brokenStates = [ordered]@{
        'ST-2000.13 Empty update state'          = [byte[]]@()
        'ST-2000.14 No update section'           = [Text.Encoding]::UTF8.GetBytes("[other]`nversion=0.0.0`n")
        'ST-2000.15 No recorded version'         = [Text.Encoding]::UTF8.GetBytes("[update]`ncheck_for_updates=false`n")
        'ST-2000.16 Truncated update state'      = [Text.Encoding]::UTF8.GetBytes("[update`nversion=0.0.0`n")
        'ST-2000.17 Invalid bytes in update state' = [byte[]](0xff, 0xfe, 0x00, 0x69)
    }
    foreach ($state in $brokenStates.GetEnumerator()) {
        Scenario $state.Key {
            [IO.File]::WriteAllBytes($UpdateConfig, $state.Value)
            Assert-ShownOnce ($state.Key -replace '\W+', '-')
        }
    }

    Scenario 'ST-2000.18 Dismiss without OK' {
        Set-UpdateState '[update]', 'version=0.0.0', 'check_for_updates=false'
        Invoke-Obs 'dismiss-without-ok' {
            param($p)
            $about = Wait-Window $p $AboutTitle
            $about.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern).Close()
            Wait-WindowClosed $p $AboutTitle
            Assert-Acknowledged
        } | Out-Null
        Invoke-Obs 'dismiss-without-ok-repeat' { param($p) Assert-AboutAbsent $p } | Out-Null
    }

    Scenario 'ST-2000.19 OBS exits while the dialog is shown' {
        Set-UpdateState '[update]', 'version=0.0.0', 'check_for_updates=false'
        Invoke-Obs 'exit-while-shown' { param($p) Wait-Window $p $AboutTitle | Out-Null; Assert-Acknowledged } | Out-Null
        Invoke-Obs 'exit-while-shown-repeat' { param($p) Assert-AboutAbsent $p } | Out-Null
    }

    Scenario 'ST-2000.20 Downgrade is shown once' {
        Set-UpdateState '[update]', 'version=9999.0.0', 'check_for_updates=false'
        Assert-ShownOnce 'downgrade' $false
    }

    Write-Host 'All ST-2000 scenarios passed.'
} finally {
    foreach ($path in 'logs', 'plugin_config', 'user.ini') {
        $source = Join-Path $ObsConfig $path
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $Artifacts "config-$path") -Recurse -Force
        }
    }
}
