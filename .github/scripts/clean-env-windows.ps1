# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# file: .github/scripts/clean-env-windows.ps1
# author: Kaito Udagawa <umireon@kaito.tokyo>
# version: 1.0.0
# date: 2026-03-31

$CleanEnvWindowsAllowList = @(
  # System
  "ALLUSERSPROFILE",
  "APPDATA",
  "CI",
  "CommonProgramFiles",
  "CommonProgramFiles(x86)",
  "CommonProgramW6432",
  "COMPUTERNAME",
  "ComSpec",
  "HOMEDRIVE",
  "HOMEPATH",
  "ImageOS",
  "ImageVersion",
  "LOCALAPPDATA",
  "NUMBER_OF_PROCESSORS",
  "OS",
  "Path",
  "PATHEXT",
  "PROCESSOR_ARCHITECTURE",
  "PROCESSOR_IDENTIFIER",
  "PROCESSOR_LEVEL",
  "PROCESSOR_REVISION",
  "ProgramData",
  "ProgramFiles",
  "ProgramFiles(x86)",
  "ProgramW6432",
  "PSModulePath",
  "SystemDrive",
  "SystemRoot",
  "TEMP",
  "TMP",
  "USERDOMAIN_ROAMINGPROFILE",
  "USERDOMAIN",
  "USERNAME",
  "USERPROFILE",
  "windir",

  # Workflow-specific
  "VCPKG_BINARY_SOURCES",
  "VCPKG_ROOT",
  "VCPKG_TARGET_TRIPLET"
)
$CleanEnvWindowsAllowList += ($env:KEEP_ENV_VARS -split ';').Where({$_})
Get-ChildItem env: | ForEach-Object {
  if ($CleanEnvWindowsAllowList -notcontains $_.Name -and $_.Name -notlike "ACTIONS_*" -and $_.Name -notlike "CCACHE_*" -and $_.Name -notlike "GIT_*" -and $_.Name -notlike "GITHUB_*" -and $_.Name -notlike "RUNNER_*") {
    Remove-Item $_.PSPath -Force
  }
}
Remove-Variable CleanEnvWindowsAllowList -Force
