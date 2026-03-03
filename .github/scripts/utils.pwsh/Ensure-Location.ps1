# SPDX-FileCopyrightText: 2018-2026 OBS Project and its contributors
# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

function Ensure-Location {
    <#
        .SYNOPSIS
            Ensures current location to be set to specified directory.
        .DESCRIPTION
            If specified directory exists, switch to it. Otherwise create it,
            then switch.
        .EXAMPLE
            Ensure-Location "My-Directory"
            Ensure-Location -Path "Path-To-My-Directory"
    #>

    param(
        [Parameter(Mandatory)]
        [string] $Path
    )

    if ( ! ( Test-Path $Path ) ) {
        $_Params = @{
            ItemType = "Directory"
            Path = ${Path}
            ErrorAction = "SilentlyContinue"
        }

        New-Item @_Params | Set-Location
    } else {
        Set-Location -Path ${Path}
    }
}
