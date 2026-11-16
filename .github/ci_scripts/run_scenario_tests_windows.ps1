# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# Runs the ST-2000 scenarios (tests/Scenarios/2000-about-dialog-on-startup.feature)
# on a disposable Windows machine. It installs the plugin ZIP into C:\ProgramData
# as users do, uses the machine's own OBS profile, and does not clean up after
# itself. Pass -ObsExe, -ObsConfig and -ObsArgs to run against another OBS.

param(
    [string] $PluginZip,
    [Parameter(Mandatory)][string] $Artifacts,
    [string] $ObsExe = 'C:\Program Files\obs-studio\bin\64bit\obs64.exe',
    [string] $ObsConfig = (Join-Path $env:APPDATA 'obs-studio'),
    [string[]] $ObsArgs = @()
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
$UIA = [Windows.Automation.AutomationElement]
$Scope = [Windows.Automation.TreeScope]
$ControlType = [Windows.Automation.ControlType]

$Repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Version = (Get-Content -LiteralPath (Join-Path $Repo 'VERSION') -Raw).Trim()
$State = Join-Path $ObsConfig 'plugin_config\obs-backgroundremoval'
$Ini = Join-Path $State 'update.ini'
$About = 'About obs-backgroundremoval'
$Notify = 'Notify me about new versions on its Filter Properties screen'

# --- Setup -------------------------------------------------------------------

New-Item -ItemType Directory -Force -Path $Artifacts | Out-Null
if ($PluginZip) {
    $extracted = Join-Path ([IO.Path]::GetTempPath()) 'scenario-plugin-zip'
    Expand-Archive -LiteralPath $PluginZip -DestinationPath $extracted -Force
    Copy-Item -LiteralPath (Join-Path $extracted 'obs-studio') -Destination $env:ProgramData -Recurse -Force
}
$profileDir = Join-Path $ObsConfig 'basic\profiles\obs-backgroundremoval-dev-720p'
New-Item -ItemType Directory -Force -Path $profileDir, (Join-Path $ObsConfig 'basic\scenes') | Out-Null
Copy-Item (Join-Path $Repo 'tests\Scenarios\0010-profile-720p-basic.ini') (Join-Path $profileDir 'basic.ini')
Copy-Item (Join-Path $Repo 'tests\Scenarios\0011-720p-user.ini') (Join-Path $ObsConfig 'user.ini')

# --- Helpers -----------------------------------------------------------------

function Wait-For([string] $What, [scriptblock] $Test, [int] $Seconds = 30) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        try {
            if ($result = & $Test) { return $result }
        } catch [Windows.Automation.ElementNotAvailableException] {
            # Qt can rebuild its accessibility tree while a dialog changes.
        } catch [IO.IOException] {
            # OBS may be replacing update.ini.
        }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for $What."
}

# Replaces the plugin state with $Bytes as update.ini.
function Reset-State([byte[]] $Bytes) {
    Remove-Item -LiteralPath $State -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $State | Out-Null
    [IO.File]::WriteAllBytes($Ini, $Bytes)
}

function Ini([string[]] $Lines) { , [Text.Encoding]::UTF8.GetBytes(($Lines -join "`n") + "`n") }

function Find($Root, [string] $Name, $Type) {
    $Root.FindFirst($Scope::Descendants, [Windows.Automation.AndCondition]::new(
            [Windows.Automation.PropertyCondition]::new($UIA::NameProperty, $Name),
            [Windows.Automation.PropertyCondition]::new($UIA::ControlTypeProperty, $Type)))
}

function Wait-Window($Main, [string] $Name) { Wait-For $Name { Find $Main $Name $ControlType::Window } }

function Wait-Closed($Main, [string] $Name) { Wait-For "$Name to close" { -not (Find $Main $Name $ControlType::Window) } | Out-Null }

function Click($Root, [string] $Name) {
    $button = Find $Root $Name $ControlType::Button
    if (-not $button) { throw "Button not found: $Name" }
    $button.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
}

function Get-Text($Window) {
    $parts = foreach ($element in $Window.FindAll($Scope::Descendants, [Windows.Automation.Condition]::TrueCondition)) {
        $element.Current.Name
        $pattern = $null
        if ($element.TryGetCurrentPattern([Windows.Automation.TextPattern]::Pattern, [ref] $pattern)) {
            $pattern.DocumentRange.GetText(-1)
        }
    }
    $parts -join "`n"
}

function Assert-Contains([string] $Text, [string] $Expected, [string] $Where) {
    if (-not $Text.Contains($Expected)) { throw "$Where does not contain: $Expected" }
}

function Get-Checkbox($Dialog) {
    $checkbox = Find $Dialog $Notify $ControlType::CheckBox
    if (-not $checkbox) { throw 'The update-notification checkbox was not found.' }
    $checkbox.GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern)
}

function Test-Checked($Toggle) { $Toggle.Current.ToggleState -eq [Windows.Automation.ToggleState]::On }

function Assert-Ini([string] $Line) {
    Wait-For "update.ini to contain $Line" {
        ([IO.File]::ReadAllText($Ini) -split '\r?\n') -contains $Line
    } -Seconds 3 | Out-Null
}

# Logs that the current OBS run created. File times cannot tell this: OBS deletes
# its oldest log before creating a new one, and NTFS name tunneling can give the
# new file the deleted file's creation time.
function Get-RunLogs {
    @(Get-ChildItem -LiteralPath (Join-Path $ObsConfig 'logs') -Filter '*.txt' -ErrorAction SilentlyContinue |
            Where-Object { $script:LogsBefore -notcontains $_.Name })
}

# Starts OBS, runs $Body with its main window, kills OBS, and returns the log of this run.
function Invoke-Obs([string] $Name, [scriptblock] $Body) {
    Write-Host "--- OBS run: $Name"
    $script:LogsBefore = @(Get-ChildItem -LiteralPath (Join-Path $ObsConfig 'logs') -Filter '*.txt' -ErrorAction SilentlyContinue |
            ForEach-Object Name)  # .Name on an empty array fails under strict mode
    $obs = Start-Process -FilePath $ObsExe -WorkingDirectory (Split-Path $ObsExe) -PassThru -ArgumentList (
        @('--multi', '--disable-updater', '--disable-missing-files-check', '--disable-shutdown-check') + $ObsArgs)
    try {
        $main = Wait-For 'OBS main window' {
            $obs.Refresh()
            if ($obs.HasExited) { throw 'OBS exited unexpectedly.' }
            if ($obs.MainWindowHandle -ne [IntPtr]::Zero) { $UIA::FromHandle($obs.MainWindowHandle) }
        }
        & $Body $main | Out-Null
    } finally {
        if (-not $obs.HasExited) { $obs.Kill(); $obs.WaitForExit() }
    }
    # A log that existed before this run would hide a crash, so require exactly one new log.
    $logs = @(Get-RunLogs)
    if ($logs.Count -ne 1) { throw "$Name wrote $($logs.Count) new OBS logs instead of one." }
    Copy-Item -LiteralPath $logs[0].FullName -Destination (Join-Path $Artifacts "obs-$Name.log")
    $text = Get-Content -LiteralPath $logs[0].FullName -Raw
    Assert-Contains $text '[obs-backgroundremoval] Plugin loaded successfully' "OBS log of $Name"
    $text
}

# The dialog is shown on OBS_FRONTEND_EVENT_FINISHED_LOADING, which comes after
# "Startup complete" in the log, so wait for that line and then watch a while.
function Assert-NoAbout($Main) {
    Wait-For 'OBS startup to complete' {
        $logs = @(Get-RunLogs)
        $logs.Count -eq 1 -and (Select-String -LiteralPath $logs[0].FullName -SimpleMatch 'Startup complete' -Quiet)
    } | Out-Null
    $deadline = (Get-Date).AddSeconds(3)
    while ((Get-Date) -lt $deadline) {
        if (Find $Main $About $ControlType::Window) { throw "$About was unexpectedly displayed." }
        Start-Sleep -Milliseconds 100
    }
}

function Accept-About($Main, $ExpectChecked = $null) {
    $dialog = Wait-Window $Main $About
    if ($null -ne $ExpectChecked -and (Test-Checked (Get-Checkbox $dialog)) -ne $ExpectChecked) {
        throw 'The update-notification checkbox did not keep its saved state.'
    }
    Click $dialog 'OK'
    Wait-Closed $Main $About
    Assert-Ini "version=$Version"
}

function Scenario([string] $Name, [scriptblock] $Body) {
    Write-Host "::group::$Name"
    try {
        & $Body
    } catch {
        Write-Host '::endgroup::'
        Write-Host "::error::$Name failed: $_"
        throw
    }
    Write-Host '::endgroup::'
}

# Runs a table row: OBS starts twice from the row's state with the row's fault in place.
# Shown rows expect the dialog on the first start only; other rows never expect it.
function Invoke-Row([hashtable] $Row) {
    Reset-State $Row.Ini
    $lock = $null
    switch ($Row['Fault']) {
        'ini-is-directory' { Remove-Item -LiteralPath $Ini; New-Item -ItemType Directory -Path $Ini | Out-Null }
        'ini-read-only' { Set-ItemProperty -LiteralPath $Ini -Name IsReadOnly -Value $true }
        'temp-locked' { [IO.File]::WriteAllText("$Ini.tmp", 'stale'); $lock = [IO.File]::Open("$Ini.tmp", 'Open', 'Read', 'None') }
        'temp-is-directory' { New-Item -ItemType Directory -Path "$Ini.tmp" | Out-Null }
    }
    try {
        $tag = $Row.Name -replace '\W+', '-'
        foreach ($attempt in 1, 2) {
            $expectShown = $Row.Shown -and $attempt -eq 1
            $log = Invoke-Obs "$tag-$attempt" {
                param($main)
                if ($expectShown) { Accept-About $main $Row['Checked'] } else { Assert-NoAbout $main }
            }
            if ($Row['Log']) { Assert-Contains $log $Row.Log "OBS log of $tag-$attempt" }
        }
    } finally {
        if ($lock) { $lock.Dispose() }
    }
    if ($Row['IniAfter']) { Assert-Ini $Row.IniAfter }
}

# --- Scenarios ---------------------------------------------------------------

$NewVersion = Ini '[update]', 'version=0.0.0', 'check_for_updates=false'

Scenario 'ST-2000.1-5 About dialog, Licenses, About Qt, disable notifications, not shown again' {
    Reset-State (Ini '[update]', 'check_for_updates=true')
    Invoke-Obs 'initial' {
        param($main)
        $dialog = Wait-Window $main $About
        $text = Get-Text $dialog
        foreach ($expected in 'Portrait Background Removal / Virtual Green-screen and Low-Light Enhancement',
            "Version $Version", 'Copyright © 2021–2026 Roy Shilkrot', '© 2023–2026 Kaito Udagawa',
            'The plugin never downloads or installs updates.') {
            Assert-Contains $text $expected $About
        }
        $checkbox = Get-Checkbox $dialog
        if (-not (Test-Checked $checkbox)) { throw 'The update-notification checkbox should start checked.' }

        Click $dialog 'Licenses'
        $licenses = Wait-Window $main 'Licenses'
        $licensesText = Get-Text $licenses
        Assert-Contains $licensesText 'GNU General Public License' 'Licenses'
        Assert-Contains $licensesText 'Apache License' 'Licenses'
        Click $licenses 'Close'
        Wait-Closed $main 'Licenses'

        Click $dialog 'About Qt'
        $qt = Wait-Window $main 'About Qt'
        Assert-Contains (Get-Text $qt) 'Qt' 'About Qt'
        Click $qt 'OK'
        Wait-Closed $main 'About Qt'

        $checkbox.Toggle()
        Wait-For 'the checkbox to clear' { -not (Test-Checked $checkbox) } | Out-Null
        Click $dialog 'OK'
        Wait-Closed $main $About
        Assert-Ini "version=$Version"
        Assert-Ini 'check_for_updates=false'
    } | Out-Null
    Invoke-Obs 'acknowledged' { param($main) Assert-NoAbout $main } | Out-Null
}

Scenario 'ST-2000.7 Notification preference cannot be saved' {
    Reset-State $NewVersion
    $log = Invoke-Obs 'notification-save-failure' {
        param($main)
        $dialog = Wait-Window $main $About
        $before = [IO.File]::ReadAllBytes($Ini)
        $lock = [IO.File]::Open($Ini, 'Open', 'Read', 'Read')
        try {
            Click $dialog 'OK'
            Wait-Closed $main $About
        } finally { $lock.Dispose() }
        if (-not [Linq.Enumerable]::SequenceEqual($before, [IO.File]::ReadAllBytes($Ini))) {
            throw 'update.ini changed although saving should have failed.'
        }
    }
    Assert-Contains $log '[obs-backgroundremoval] Failed to save the update notification setting to update.ini' 'OBS log'
}

$rows = @(
    @{ Name = 'ST-2000.6 Notification preference is kept for a new version'; Ini = $NewVersion; Shown = $true; Checked = $false }
    @{ Name = 'ST-2000.8 Update state cannot be opened'; Ini = $NewVersion; Fault = 'ini-is-directory'; Shown = $false; Log = 'Failed to open update config' }
    @{ Name = 'ST-2000.9 Current version cannot be recorded'; Ini = $NewVersion; Fault = 'ini-read-only'; Shown = $false; Log = 'Failed to save update config'; IniAfter = 'version=0.0.0' }
    @{ Name = 'ST-2000.10 Stale temporary file is not writable'; Ini = $NewVersion; Fault = 'temp-locked'; Shown = $false; Log = 'Failed to save update config' }
    @{ Name = 'ST-2000.11 Stale temporary directory'; Ini = $NewVersion; Fault = 'temp-is-directory'; Shown = $false; Log = 'Failed to save update config' }
    @{ Name = 'ST-2000.12 Recovered storage'; Ini = $NewVersion; Shown = $true; Checked = $false }
    @{ Name = 'ST-2000.13 Empty update state'; Ini = [byte[]]@(); Shown = $true }
    @{ Name = 'ST-2000.14 No update section'; Ini = (Ini '[other]', 'version=0.0.0'); Shown = $true }
    @{ Name = 'ST-2000.15 No recorded version'; Ini = (Ini '[update]', 'check_for_updates=false'); Shown = $true }
    @{ Name = 'ST-2000.16 Truncated update state'; Ini = (Ini '[update', 'version=0.0.0'); Shown = $true }
    @{ Name = 'ST-2000.17 Invalid bytes in update state'; Ini = [byte[]](0xff, 0xfe, 0x00, 0x69); Shown = $true }
    @{ Name = 'ST-2000.20 Downgrade is shown once'; Ini = (Ini '[update]', 'version=9999.0.0', 'check_for_updates=false'); Shown = $true; Checked = $false }
)
foreach ($row in $rows) { Scenario $row.Name { Invoke-Row $row } }

Scenario 'ST-2000.18 Dismiss without OK' {
    Reset-State $NewVersion
    Invoke-Obs 'dismiss-without-ok' {
        param($main)
        (Wait-Window $main $About).GetCurrentPattern([Windows.Automation.WindowPattern]::Pattern).Close()
        Wait-Closed $main $About
        Assert-Ini "version=$Version"
    } | Out-Null
    Invoke-Obs 'dismiss-without-ok-repeat' { param($main) Assert-NoAbout $main } | Out-Null
}

Scenario 'ST-2000.19 OBS exits while the dialog is shown' {
    Reset-State $NewVersion
    Invoke-Obs 'exit-while-shown' { param($main) Wait-Window $main $About | Out-Null; Assert-Ini "version=$Version" } | Out-Null
    Invoke-Obs 'exit-while-shown-repeat' { param($main) Assert-NoAbout $main } | Out-Null
}

Write-Host 'All ST-2000 scenarios passed.'
