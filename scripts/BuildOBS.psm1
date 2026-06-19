# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# file: scripts/BuildOBS.psm1
# description: Helper module to build the OBS library.
# author: Kaito Udagawa <umireon@kaito.tokyo>
# version: 1.0.0
# date: 2026-06-02

function Initialize-ObsDeps {
    [CmdletBinding()]
    param(
        [string]$RootDir = $PWD,
        [string]$PluginBuildDir = $env:PLUGIN_BUILD_DIR ?? $RootDir,
        [Parameter(Mandatory = $true)]
        [string]$Component
    )
    process {
        Set-StrictMode -Version Latest; $ErrorActionPreference = 'Stop'; $PSNativeCommandUseErrorActionPreference = $true; $ProgressPreference = 'SilentlyContinue'

        $buildspec = Get-Content -LiteralPath (Join-Path $RootDir 'buildspec.props') -Raw | ConvertFrom-StringData

        if ($IsWindows) {
            $url = $buildspec["${Component}_windows_x64_url"]
            $sha256 = $buildspec["${Component}_windows_x64_sha256"]
        }
        elseif ($IsMacOS) {
            $url = $buildspec["${Component}_macos_url"]
            $sha256 = $buildspec["${Component}_macos_sha256"]
        }
        else {
            throw 'Unsupported platform'
        }

        $depsDir = New-Item (Join-Path $PluginBuildDir '.deps') -ItemType Directory -Force
        $outDir = Join-Path $depsDir "obs-deps-$Component"
        $outfile = Join-Path $depsDir (Split-Path $url -Leaf)

        if (!(Test-Path -LiteralPath $outfile)) {
            Invoke-WebRequest -Uri $url -OutFile $outfile
        }

        $fileHash = Get-FileHash -LiteralPath $outfile -Algorithm SHA256
        if ($fileHash.Hash -ine $sha256) {
            throw 'Checksum verification failed'
        }

        if (!(Test-Path -LiteralPath $outdir)) {
            if ($IsWindows) {
                Expand-Archive -LiteralPath $outfile -Destination $outdir -Force
            } elseif ($IsMacOS) {
                $outDir = New-Item $outDir -ItemType Directory -Force
                tar -xf "$outfile" -C $outdir
            } else {
                throw 'Unsupported platform'
            }
        }
    }
}

function Invoke-ObsConfigure {
    [CmdletBinding()]
    param(
        [string]$RootDir = $PWD,
        [string]$PluginBuildDir = $env:PLUGIN_BUILD_DIR ?? $RootDir,
        [string]$Generator = $null,
        [string]$VsVersionRange = '[17,)'
    )
    process {
        Set-StrictMode -Version Latest; $ErrorActionPreference = 'Stop'; $PSNativeCommandUseErrorActionPreference = $true; $ProgressPreference = 'SilentlyContinue'

        $buildspec = Get-Content -LiteralPath (Join-Path $RootDir 'buildspec.props') -Raw | ConvertFrom-StringData
        $cmakePresets = Get-Content -LiteralPath (Join-Path $RootDir 'CMakePresets.json') -Raw | ConvertFrom-Json

        $obsSourceDir = Join-Path $PluginBuildDir 'obs-studio'
        $obsBuildDir = Join-Path $PluginBuildDir 'build_obs'
        $obsPrefixDir = Join-Path $PluginBuildDir 'obs_installed'
        $obsPrebuiltDir = Join-Path $PluginBuildDir '.deps' 'obs-deps-prebuilt'
        $obsQt6Dir = Join-Path $PluginBuildDir '.deps' 'obs-deps-qt6'

        $cmakeConfigureArgs = @(
            '-S', $obsSourceDir,
            '-B', $obsBuildDir,
            "-DCMAKE_INSTALL_PREFIX=$obsPrefixDir",
            "-DCMAKE_PREFIX_PATH=$obsPrebuiltDir;$obsQt6Dir",
            "-DOBS_CMAKE_VERSION=3.0.0",
            "-DENABLE_PLUGINS=OFF",
            "-DENABLE_FRONTEND=OFF",
            "-DOBS_VERSION_OVERRIDE=$($buildspec['obs_studio_git_tag'])"
        )

        if ($IsWindows) {
            $windowsPreset = $cmakePresets.configurePresets | Where-Object { $_.name -eq 'windows' }

            $arch = $windowsPreset.architecture
            $windowsSdkVersion = $windowsPreset.cacheVariables.CMAKE_SYSTEM_VERSION

            $cmakeConfigureArgs += @(
                '-A', $arch,
                "-DCMAKE_SYSTEM_VERSION=$windowsSdkVersion"
            )

            if ($Generator) {
                $cmakeConfigureArgs += @('-G', $Generator)
            }
        }
        elseif ($IsMacOS) {
            $macOSPreset = $cmakePresets.configurePresets | Where-Object { $_.name -eq 'macos' }

            $cmakeConfigureArgs += @(
                '-G', 'Xcode',
                "-DCMAKE_OSX_ARCHITECTURES=$($macOSPreset.cacheVariables.CMAKE_OSX_ARCHITECTURES)",
                "-DCMAKE_OSX_DEPLOYMENT_TARGET=$($macOSPreset.cacheVariables.CMAKE_OSX_DEPLOYMENT_TARGET)"
            )
        }
        else {
            throw 'Unsupported platform'
        }

        cmake $cmakeConfigureArgs
    }
}

function Invoke-ObsBuild {
    [CmdletBinding()]
    param(
        [string]$RootDir = $PWD,
        [string]$PluginBuildDir = $env:PLUGIN_BUILD_DIR ?? $RootDir,
        [string]$Configuration = 'Release'
    )
    process {
        Set-StrictMode -Version Latest; $ErrorActionPreference = 'Stop'; $PSNativeCommandUseErrorActionPreference = $true; $ProgressPreference = 'SilentlyContinue'

        $obsBuildDir = Join-Path $PluginBuildDir "build_obs"

        cmake --build $obsBuildDir --target obs-frontend-api --config $Configuration --parallel
    }
}

function Install-Obs {
    [CmdletBinding()]
    param(
        [string]$RootDir = $PWD,
        [string]$PluginBuildDir = $env:PLUGIN_BUILD_DIR ?? $RootDir,
        [string]$Configuration = 'Release'
    )
    process {
        Set-StrictMode -Version Latest; $ErrorActionPreference = 'Stop'; $PSNativeCommandUseErrorActionPreference = $true; $ProgressPreference = 'SilentlyContinue'

        $obsBuildDir = Join-Path $PluginBuildDir 'build_obs'
        $obsPrefixDir = Join-Path $PluginBuildDir 'obs_installed'

        cmake --install $obsBuildDir --component Development --config $Configuration --prefix $obsPrefixDir
    }
}
