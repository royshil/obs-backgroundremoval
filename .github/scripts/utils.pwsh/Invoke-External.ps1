# SPDX-FileCopyrightText: 2018-2026 OBS Project and its contributors
# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

function Invoke-External {
    <#
        .SYNOPSIS
            Invokes a non-PowerShell command.
        .DESCRIPTION
            Runs a non-PowerShell command, and captures its return code.
            Throws an exception if the command returns non-zero.
        .EXAMPLE
            Invoke-External 7z x $MyArchive
    #>

    if ( $args.Count -eq 0 ) {
        throw 'Invoke-External called without arguments.'
    }

    if ( ! ( Test-Path function:Log-Information ) ) {
        . $PSScriptRoot/Logger.ps1
    }

    $Command = $args[0]
    $CommandArgs = @()

    if ( $args.Count -gt 1) {
        $CommandArgs = $args[1..($args.Count - 1)]
    }

    $_EAP = $ErrorActionPreference
    $ErrorActionPreference = "Continue"

    Log-Debug "Invoke-External: ${Command} ${CommandArgs}"

    & $command $commandArgs
    $Result = $LASTEXITCODE

    $ErrorActionPreference = $_EAP

    if ( $Result -ne 0 ) {
        throw "${Command} ${CommandArgs} exited with non-zero code ${Result}."
    }
}
