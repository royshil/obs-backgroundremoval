# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string] $ObsExecutable,
    [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string] $SourceDir,
    [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string] $WorkDir,
    [Parameter(Mandatory)][ValidateScript({ Test-Path -LiteralPath $_ -PathType Container })]
    [string] $Artifacts
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class ScenarioNativeWindows
{
    private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr window, StringBuilder text, int maximumCount);

    public static IntPtr[] Find(uint processId, string title, bool contains)
    {
        var matches = new List<IntPtr>();
        EnumWindows((window, parameter) => {
            GetWindowThreadProcessId(window, out uint windowProcessId);
            if (windowProcessId != processId) return true;
            var text = new StringBuilder(1024);
            GetWindowText(window, text, text.Capacity);
            string actual = text.ToString();
            if ((contains && actual.Contains(title)) || (!contains && actual == title)) matches.Add(window);
            return true;
        }, IntPtr.Zero);
        return matches.ToArray();
    }
}
'@

$AutomationElement = [System.Windows.Automation.AutomationElement]
$TreeScope = [System.Windows.Automation.TreeScope]
$Condition = [System.Windows.Automation.Condition]
$ControlType = [System.Windows.Automation.ControlType]
$AboutTitle = 'About obs-backgroundremoval'
$Version = (Get-Content -LiteralPath (Join-Path $SourceDir 'VERSION') -Raw).Trim()
$PortableRoot = Split-Path (Split-Path (Split-Path $ObsExecutable))
$ObsConfig = Join-Path $PortableRoot 'config\obs-studio'
$PluginConfig = Join-Path $ObsConfig 'plugin_config\obs-backgroundremoval'
$UpdateConfig = Join-Path $PluginConfig 'update.ini'
$UpdateTemp = Join-Path $PluginConfig 'update.ini.tmp'
$TestResults = Join-Path $Artifacts 'test-results.txt'

function Report-Case {
    param([string] $Event, [string] $Name)
    $message = "TEST ${Event}: $Name"
    Write-Host $message
    Add-Content -LiteralPath $TestResults -Value $message -Encoding utf8
}

function Wait-Until {
    param(
        [Parameter(Mandatory)][scriptblock] $ConditionScript,
        [Parameter(Mandatory)][string] $Description,
        [int] $TimeoutSeconds = 30
    )
    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    do {
        try {
            $result = & $ConditionScript
            if ($result) { return $result }
        } catch [System.Windows.Automation.ElementNotAvailableException] {
            # Qt can rebuild its accessibility tree while a dialog changes.
        }
        Start-Sleep -Milliseconds 100
    } while ($stopwatch.Elapsed.TotalSeconds -lt $TimeoutSeconds)
    $diagnostics = foreach ($element in $AutomationElement::RootElement.FindAll(
            $TreeScope::Descendants, $Condition::TrueCondition)) {
        try {
            if ($element.Current.Name) {
                '{0}`t{1}`t{2}' -f $element.Current.ProcessId,
                    $element.Current.ControlType.ProgrammaticName, $element.Current.Name
            }
        } catch [System.Windows.Automation.ElementNotAvailableException] {}
    }
    $diagnostics | Set-Content -LiteralPath (Join-Path $Artifacts 'accessibility-tree.txt') -Encoding utf8
    throw "Timed out waiting for $Description."
}

function Find-Window {
    param([int] $ProcessId, [string] $Title, [switch] $Contains)
    $handles = [ScenarioNativeWindows]::Find($ProcessId, $Title, $Contains.IsPresent)
    if ($handles.Count -eq 0) { return $null }
    return $AutomationElement::FromHandle($handles[0])
}

function Find-Control {
    param(
        [Parameter(Mandatory)][System.Windows.Automation.AutomationElement] $Root,
        [Parameter(Mandatory)][string] $Name,
        [Parameter(Mandatory)][System.Windows.Automation.ControlType] $Type
    )
    $combined = [System.Windows.Automation.AndCondition]::new(
        [System.Windows.Automation.Condition[]]@(
            [System.Windows.Automation.PropertyCondition]::new($AutomationElement::NameProperty, $Name),
            [System.Windows.Automation.PropertyCondition]::new($AutomationElement::ControlTypeProperty, $Type)
        ))
    return $Root.FindFirst($TreeScope::Descendants, $combined)
}

function Invoke-Control {
    param([Parameter(Mandatory)][System.Windows.Automation.AutomationElement] $Element)
    $pattern = $Element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
    ([System.Windows.Automation.InvokePattern] $pattern).Invoke()
}

function Close-Window {
    param([Parameter(Mandatory)][System.Windows.Automation.AutomationElement] $Window)
    $pattern = $Window.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern)
    ([System.Windows.Automation.WindowPattern] $pattern).Close()
}

function Get-WindowText {
    param([Parameter(Mandatory)][System.Windows.Automation.AutomationElement] $Window)
    $parts = foreach ($element in $Window.FindAll($TreeScope::Descendants, $Condition::TrueCondition)) {
        $name = $element.Current.Name
        if ($name) { $name }
        $textPattern = $null
        if ($element.TryGetCurrentPattern([System.Windows.Automation.TextPattern]::Pattern, [ref] $textPattern)) {
            ([System.Windows.Automation.TextPattern] $textPattern).DocumentRange.GetText(-1)
        }
    }
    return $parts -join "`n"
}

function Get-Checkbox {
    param([System.Windows.Automation.AutomationElement] $Dialog)
    $checkboxes = $Dialog.FindAll($TreeScope::Descendants,
        [System.Windows.Automation.PropertyCondition]::new(
            $AutomationElement::ControlTypeProperty, $ControlType::CheckBox))
    if ($checkboxes.Count -ne 1) {
        throw "Expected one update-notification checkbox; found $($checkboxes.Count)."
    }
    return $checkboxes.Item(0)
}

function Test-Checked {
    param([System.Windows.Automation.AutomationElement] $Checkbox)
    $toggle = [System.Windows.Automation.TogglePattern] $Checkbox.GetCurrentPattern(
        [System.Windows.Automation.TogglePattern]::Pattern)
    return $toggle.Current.ToggleState -eq [System.Windows.Automation.ToggleState]::On
}

function Toggle-Checkbox {
    param([System.Windows.Automation.AutomationElement] $Checkbox)
    $toggle = [System.Windows.Automation.TogglePattern] $Checkbox.GetCurrentPattern(
        [System.Windows.Automation.TogglePattern]::Pattern)
    $toggle.Toggle()
}

function Write-UpdateConfig {
    param([Parameter(ValueFromRemainingArguments)][string[]] $Lines)
    New-Item -ItemType Directory -Force -Path $PluginConfig | Out-Null
    [IO.File]::WriteAllText($UpdateConfig, (($Lines -join "`n") + "`n"),
        [Text.UTF8Encoding]::new($false))
}

function Wait-ForUpdateConfig {
    param(
        [Parameter(Mandatory)][scriptblock] $ConditionScript,
        [Parameter(Mandatory)][string] $Description,
        [int] $TimeoutSeconds = 3
    )
    $lastContents = $null
    $lastError = $null
    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        try {
            $lastContents = Get-Content -LiteralPath $UpdateConfig -Raw -ErrorAction Stop
            $lastError = $null
            if (& $ConditionScript $lastContents) { return $lastContents }
        } catch [System.Management.Automation.ItemNotFoundException], [IO.IOException] {
            $lastError = $_.Exception.Message
        }
        Start-Sleep -Milliseconds 50
    }
    if ($lastError) {
        throw "Timed out waiting for update.ini to $Description. Last read error: $lastError"
    }
    throw "Timed out waiting for update.ini to $Description. Last contents: $lastContents"
}

function Assert-VersionAcknowledged {
    param([string] $Expected = $Version)
    Wait-ForUpdateConfig -Description "contain version=$Expected" -ConditionScript {
        param($contents)
        $contents -match "(?m)^version=$([regex]::Escape($Expected))`r?$"
    } | Out-Null
}

function Assert-Notifications {
    param([bool] $Enabled)
    $expected = if ($Enabled) { 'true' } else { 'false' }
    Wait-ForUpdateConfig -Description "contain check_for_updates=$expected" -ConditionScript {
        param($contents)
        $contents -match "(?m)^check_for_updates=$expected`r?$"
    } | Out-Null
}

function Get-AboutDialog {
    param([Diagnostics.Process] $Process)
    return Wait-Until -Description $AboutTitle -ConditionScript {
        Find-Window -ProcessId $Process.Id -Title $AboutTitle
    }
}

function Assert-AboutAbsent {
    param([Diagnostics.Process] $Process, [double] $Seconds = 2.0)
    Wait-Until -Description 'OBS Studio main window' -ConditionScript {
        Find-Window -ProcessId $Process.Id -Title 'OBS' -Contains
    } | Out-Null
    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.Elapsed.TotalSeconds -lt $Seconds) {
        if ($Process.HasExited) { throw 'OBS exited during absence verification.' }
        if (Find-Window -ProcessId $Process.Id -Title $AboutTitle) {
            throw "$AboutTitle was unexpectedly displayed."
        }
        Start-Sleep -Milliseconds 100
    }
}

function Stop-Obs {
    param([Diagnostics.Process] $Process)
    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
}

function Run-ObsPhase {
    param([string] $Name, [scriptblock] $Action)
    $process = $null
    try {
        $process = Start-Process -FilePath $ObsExecutable -WorkingDirectory (Split-Path $ObsExecutable) `
            -ArgumentList @('--portable', '--multi', '--disable-updater', '--disable-missing-files-check',
                '--disable-shutdown-check') -PassThru
        & $Action $process
    } finally {
        if ($process) { Stop-Obs $process }
    }

    $obsLog = Get-ChildItem -LiteralPath (Join-Path $ObsConfig 'logs') -Filter '*.txt' -File |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if (-not $obsLog) { throw "OBS did not write a log for phase $Name." }
    $phaseLog = Join-Path $Artifacts "obs-$Name.log"
    Copy-Item -LiteralPath $obsLog.FullName -Destination $phaseLog -Force
    if (-not (Select-String -LiteralPath $phaseLog -SimpleMatch '[obs-backgroundremoval] Plugin loaded successfully' -Quiet)) {
        throw "OBS did not load obs-backgroundremoval during phase $Name."
    }
    return $phaseLog
}

function Assert-Log {
    param([string] $Path, [string] $Expected)
    if (-not (Select-String -LiteralPath $Path -SimpleMatch $Expected -Quiet)) {
        throw "OBS log does not contain: $Expected"
    }
}

function Accept-About {
    param([Diagnostics.Process] $Process, [Nullable[bool]] $ExpectedNotifications)
    $dialog = Get-AboutDialog $Process
    $checkbox = Get-Checkbox $dialog
    if ($null -ne $ExpectedNotifications -and (Test-Checked $checkbox) -ne [bool]$ExpectedNotifications) {
        throw 'The update-notification checkbox did not preserve its expected state.'
    }
    Invoke-Control (Find-Control -Root $dialog -Name 'OK' -Type $ControlType::Button)
    Wait-Until -Description "$AboutTitle to close" -ConditionScript {
        -not (Find-Window -ProcessId $Process.Id -Title $AboutTitle)
    } | Out-Null
    Assert-VersionAcknowledged
}

function Repair-State {
    param([string] $Name, [byte[]] $Contents)
    New-Item -ItemType Directory -Force -Path $PluginConfig | Out-Null
    [IO.File]::WriteAllBytes($UpdateConfig, $Contents)
    Run-ObsPhase "$Name-repair" { param($process) Accept-About $process $null } | Out-Null
    Run-ObsPhase "$Name-repeat" { param($process) Assert-AboutAbsent $process } | Out-Null
}

New-Item -ItemType Directory -Force -Path $PluginConfig | Out-Null
Write-UpdateConfig '[update]' 'check_for_updates=true'

$cases = [ordered]@{}
$cases['ST-2000.1-ST-2000.4 initial actions'] = {
    Run-ObsPhase 'about-dialog-on-startup-initial' {
        param($process)
        $about = Get-AboutDialog $process
        $text = Get-WindowText $about
        foreach ($expected in @(
                'Portrait Background Removal / Virtual Green-screen and Low-Light Enhancement',
                "Version $Version", 'Copyright © 2021–2026 Roy Shilkrot')) {
            if (-not $text.Contains($expected)) { throw "About dialog is missing: $expected" }
        }
        $checkbox = Get-Checkbox $about
        if (-not (Test-Checked $checkbox)) { throw 'Notification checkbox should initially be checked.' }

        Invoke-Control (Find-Control -Root $about -Name 'Licenses' -Type $ControlType::Button)
        $licenses = Wait-Until -Description 'Licenses dialog' -ConditionScript {
            Find-Window -ProcessId $process.Id -Title 'Licenses'
        }
        if (-not (Get-WindowText $licenses).Contains('GNU General Public License')) {
            throw 'Licenses dialog does not contain the GNU General Public License.'
        }
        Invoke-Control (Find-Control -Root $licenses -Name 'Close' -Type $ControlType::Button)
        Wait-Until -Description 'Licenses dialog to close' -ConditionScript {
            -not (Find-Window -ProcessId $process.Id -Title 'Licenses')
        } | Out-Null

        Invoke-Control (Find-Control -Root $about -Name 'About Qt' -Type $ControlType::Button)
        $qt = Wait-Until -Description 'About Qt dialog' -ConditionScript {
            Find-Window -ProcessId $process.Id -Title 'About Qt'
        }
        if (-not (Get-WindowText $qt).Contains('Qt')) { throw 'About Qt contains no Qt information.' }
        $close = Find-Control -Root $qt -Name 'OK' -Type $ControlType::Button
        if (-not $close) { $close = Find-Control -Root $qt -Name 'Close' -Type $ControlType::Button }
        Invoke-Control $close
        Wait-Until -Description 'About Qt dialog to close' -ConditionScript {
            -not (Find-Window -ProcessId $process.Id -Title 'About Qt')
        } | Out-Null

        Toggle-Checkbox $checkbox
        Wait-Until -Description 'notification checkbox to clear' -ConditionScript {
            -not (Test-Checked $checkbox)
        } | Out-Null
        Invoke-Control (Find-Control -Root $about -Name 'OK' -Type $ControlType::Button)
        Wait-Until -Description "$AboutTitle to close" -ConditionScript {
            -not (Find-Window -ProcessId $process.Id -Title $AboutTitle)
        } | Out-Null
        Assert-VersionAcknowledged
        Assert-Notifications $false
    } | Out-Null
}
$cases['ST-2000.5 acknowledged version is absent'] = {
    Run-ObsPhase 'about-dialog-on-startup-repeat' { param($process) Assert-AboutAbsent $process } | Out-Null
}
$cases['ST-2000.6 preserve notification preference'] = {
    Write-UpdateConfig '[update]' 'version=0.0.0' 'check_for_updates=false'
    Run-ObsPhase 'about-dialog-on-startup-new-version' {
        param($process) Accept-About $process $false
    } | Out-Null
}
$cases['ST-2000.7 notification save failure'] = {
    Write-UpdateConfig '[update]' 'version=0.0.0' 'check_for_updates=false'
    $log = Run-ObsPhase 'about-dialog-on-startup-notification-save-failure' {
        param($process)
        $dialog = Get-AboutDialog $process
        $before = [IO.File]::ReadAllBytes($UpdateConfig)
        $lock = [IO.File]::Open($UpdateConfig, 'Open', 'Read', 'Read')
        try {
            Invoke-Control (Find-Control -Root $dialog -Name 'OK' -Type $ControlType::Button)
            Wait-Until -Description "$AboutTitle to close" -ConditionScript {
                -not (Find-Window -ProcessId $process.Id -Title $AboutTitle)
            } | Out-Null
        } finally { $lock.Dispose() }
        if (-not [Linq.Enumerable]::SequenceEqual([byte[]]$before, [byte[]][IO.File]::ReadAllBytes($UpdateConfig))) {
            throw 'update.ini changed despite the expected save failure.'
        }
    }
    Assert-Log $log '[obs-backgroundremoval] Failed to save the update notification setting to update.ini'
}
$cases['ST-2000.8 update state cannot be opened'] = {
    Remove-Item -LiteralPath $UpdateConfig -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $UpdateConfig | Out-Null
    try {
        foreach ($attempt in 1..2) {
            $log = Run-ObsPhase "about-dialog-on-startup-update-open-failure-$attempt" {
                param($process) Assert-AboutAbsent $process
            }
            Assert-Log $log 'Failed to open update config'
        }
    } finally { Remove-Item -LiteralPath $UpdateConfig -Recurse -Force }
}
$cases['ST-2000.9 current version cannot be recorded'] = {
    Write-UpdateConfig '[update]' 'version=0.0.0' 'check_for_updates=false'
    Set-ItemProperty -LiteralPath $UpdateConfig -Name IsReadOnly -Value $true
    try {
        foreach ($attempt in 1..2) {
            $log = Run-ObsPhase "about-dialog-on-startup-update-save-failure-$attempt" {
                param($process) Assert-AboutAbsent $process
            }
            Assert-Log $log 'Failed to save update config'
        }
    } finally { Set-ItemProperty -LiteralPath $UpdateConfig -Name IsReadOnly -Value $false }
    Wait-ForUpdateConfig -Description 'retain version=0.0.0' -ConditionScript {
        param($contents)
        $contents -match '(?m)^version=0\.0\.0\r?$'
    } | Out-Null
}
$cases['ST-2000.10 unwritable stale temporary file'] = {
    Write-UpdateConfig '[update]' 'version=0.0.0' 'check_for_updates=false'
    [IO.File]::WriteAllText($UpdateTemp, 'stale')
    $lock = [IO.File]::Open($UpdateTemp, 'Open', 'Read', 'None')
    try {
        foreach ($attempt in 1..2) {
            $log = Run-ObsPhase "about-dialog-on-startup-unwritable-temporary-file-$attempt" {
                param($process) Assert-AboutAbsent $process
            }
            Assert-Log $log 'Failed to save update config'
        }
    } finally { $lock.Dispose(); Remove-Item -LiteralPath $UpdateTemp -Force }
}
$cases['ST-2000.11-ST-2000.12 temporary directory and recovery'] = {
    Write-UpdateConfig '[update]' 'version=0.0.0' 'check_for_updates=false'
    New-Item -ItemType Directory -Path $UpdateTemp | Out-Null
    try {
        foreach ($attempt in 1..2) {
            $log = Run-ObsPhase "about-dialog-on-startup-temporary-directory-$attempt" {
                param($process) Assert-AboutAbsent $process
            }
            Assert-Log $log 'Failed to save update config'
        }
    } finally { Remove-Item -LiteralPath $UpdateTemp -Recurse -Force }
    Run-ObsPhase 'about-dialog-on-startup-storage-recovered' {
        param($process) Accept-About $process $false
    } | Out-Null
    Run-ObsPhase 'about-dialog-on-startup-storage-recovered-repeat' {
        param($process) Assert-AboutAbsent $process
    } | Out-Null
}
$cases['ST-2000.13 empty update state'] = {
    Repair-State 'about-dialog-on-startup-empty-update-state' ([byte[]]@())
}
$cases['ST-2000.14 missing update section'] = {
    Repair-State 'about-dialog-on-startup-missing-update-section' `
        ([Text.Encoding]::UTF8.GetBytes("[other]`nversion=0.0.0`n"))
}
$cases['ST-2000.15 missing version'] = {
    Repair-State 'about-dialog-on-startup-missing-version' `
        ([Text.Encoding]::UTF8.GetBytes("[update]`ncheck_for_updates=false`n"))
}
$cases['ST-2000.16 truncated update state'] = {
    Repair-State 'about-dialog-on-startup-truncated-update-state' `
        ([Text.Encoding]::UTF8.GetBytes("[update`nversion=0.0.0`n"))
}
$cases['ST-2000.17 invalid update state bytes'] = {
    Repair-State 'about-dialog-on-startup-invalid-update-state-bytes' ([byte[]](0xff, 0xfe, 0x00, 0x69))
}
$cases['ST-2000.18 dismiss without OK'] = {
    Write-UpdateConfig '[update]' 'version=0.0.0' 'check_for_updates=false'
    Run-ObsPhase 'about-dialog-on-startup-dismiss-without-ok' {
        param($process)
        Close-Window (Get-AboutDialog $process)
        Wait-Until -Description "$AboutTitle to close" -ConditionScript {
            -not (Find-Window -ProcessId $process.Id -Title $AboutTitle)
        } | Out-Null
        Assert-VersionAcknowledged
    } | Out-Null
    Run-ObsPhase 'about-dialog-on-startup-dismiss-without-ok-repeat' {
        param($process) Assert-AboutAbsent $process
    } | Out-Null
}
$cases['ST-2000.19 exit while dialog visible'] = {
    Write-UpdateConfig '[update]' 'version=0.0.0' 'check_for_updates=false'
    Run-ObsPhase 'about-dialog-on-startup-exit-while-dialog-visible' {
        param($process) Get-AboutDialog $process | Out-Null; Assert-VersionAcknowledged
    } | Out-Null
    Run-ObsPhase 'about-dialog-on-startup-exit-while-dialog-visible-repeat' {
        param($process) Assert-AboutAbsent $process
    } | Out-Null
}
$cases['ST-2000.20 downgrade is displayed once'] = {
    Write-UpdateConfig '[update]' 'version=9999.0.0' 'check_for_updates=false'
    Run-ObsPhase 'about-dialog-on-startup-downgrade' {
        param($process) Accept-About $process $false
    } | Out-Null
    Run-ObsPhase 'about-dialog-on-startup-downgrade-repeat' {
        param($process) Assert-AboutAbsent $process
    } | Out-Null
}

$failed = $false
foreach ($case in $cases.GetEnumerator()) {
    Report-Case 'START' $case.Key
    try {
        & $case.Value
        Report-Case 'RESULT' "$($case.Key): PASSED"
    } catch {
        Report-Case 'RESULT' "$($case.Key): FAILED"
        $_ | Out-String | Add-Content -LiteralPath (Join-Path $Artifacts 'failures.txt') -Encoding utf8
        $failed = $true
        break
    }
}

if ($failed) { throw 'Windows ST-2000 scenario failed. See the scenario artifacts for details.' }
Write-Host "Windows ST-2000 scenario passed ($($cases.Count) case groups)."
