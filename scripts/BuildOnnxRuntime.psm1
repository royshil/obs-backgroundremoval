# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# file: scripts/BuildOnnxRuntime.psm1
# description: Helper module to build the ONNX Runtime.
# author: Kaito Udagawa <umireon@kaito.tokyo>
# version: 1.2.0
# date: 2026-06-17

function Update-OrtSourceWithPatches {
    [CmdletBinding()]
    param(
        [string]$RootDir = $PWD,
        [string]$PluginBuildDir = $env:PLUGIN_BUILD_DIR ?? $RootDir,
        [string]$OrtSourceDir = (Join-Path $PluginBuildDir 'onnxruntime'),
        [string]$OrtPatchesDir = (Join-Path $RootDir 'scripts' 'ort_patches'),
        [string]$OrtVersion = $env:PLUGIN_ORT_VERSION
    )
    process {
        Set-StrictMode -Version Latest; $ErrorActionPreference = 'Stop'; $PSNativeCommandUseErrorActionPreference = $true; $ProgressPreference = 'SilentlyContinue'

        if (-not $OrtVersion) {
            $buildspec = Get-Content -LiteralPath (Join-Path $RootDir 'buildspec.props') -Raw | ConvertFrom-StringData
            $OrtVersion = $buildspec.onnxruntime_git_tag
        }

        if ($OrtVersion -eq 'v1.23.2') {
            $patches = @(
                Join-Path $OrtPatchesDir '0000-27960.patch'
                Join-Path $OrtPatchesDir '0001-27981.patch'
                Join-Path $OrtPatchesDir '0002-27982.patch'
            )
            git -C $OrtSourceDir apply $patches
        }
    }
}

function Get-OrtToolchain {
    [CmdletBinding()]
    param(
        [string]$RootDir = $PWD,
        [string]$PluginBuildDir = $env:PLUGIN_BUILD_DIR ?? $RootDir,
        [string]$VcpkgRoot = $env:VCPKG_ROOT ?? (Join-Path $PluginBuildDir 'vcpkg'),
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [string]$OrtToolchainDir = (Join-Path $PluginBuildDir '.deps' 'ort_toolchain')
    )
    process {
        Set-StrictMode -Version Latest; $ErrorActionPreference = 'Stop'; $PSNativeCommandUseErrorActionPreference = $true; $ProgressPreference = 'SilentlyContinue'

        $vcpkgExe = Join-Path $VcpkgRoot 'vcpkg'

        $vcpkgToolsJsonPath = Join-Path $VcpkgRoot 'scripts' 'vcpkg-tools.json'
        $vcpkgToolsJson = Get-Content -LiteralPath $vcpkgToolsJsonPath | Out-String | ConvertFrom-Json -AsHashtable
        $vcpkgTools = $vcpkgToolsJson.tools | Group-Object { "$($_['arch'] ?? 'unknown')_$($_['os'])_$($_['name'])" } -AsHashTable -AsString

        function Install-VcpkgTool {
            param(
                [Parameter(Mandatory = $true)]
                [hashtable]$Tool
            )
            process {
                New-Item $OrtToolchainDir -ItemType 'Directory' -Force

                $outfile = Join-Path $OrtToolchainDir (Split-Path $Tool.url -Leaf)
                $outdir = Join-Path $OrtToolchainDir $Tool.name

                if (!(Test-Path $outfile)) {
                    Invoke-WebRequest -Uri $Tool.url -OutFile $outfile
                }

                $fileHash = Get-FileHash -LiteralPath $outfile -Algorithm SHA512
                if ($fileHash.Hash -ine $Tool.sha512) {
                    throw "Checksum verification failed: $Name expected=$($Tool.sha512) actual=$($fileHash.Hash)"
                }

                if (!(Test-Path -LiteralPath $outdir)) {
                    if ((Split-Path $Tool.url -Extension) -imatch '^\.(zip|tar\.gz)$') {
                        Expand-Archive -LiteralPath $outfile -Destination $outdir -Force
                    }
                    else {
                        throw 'Tool artifact in not supported format'
                    }
                }

                Join-Path $outdir $Tool.executable
            }

        }

        if ($IsWindows) {
            if ($Name -eq 'cmake') {
                Install-VcpkgTool -Tool $vcpkgTools.amd64_windows_cmake[0]
            }
            elseif ($Name -eq 'ninja') {
                Install-VcpkgTool -Tool $vcpkgTools.x64_windows_ninja[0]
            }
            elseif ($Name -eq 'vswhere') {
                & $vcpkgExe fetch vswhere | Select-Object -Last 1
            }
            else {
                throw "Unsupported tool name: $Name"
            }
        }
        elseif ($IsMacOS) {
            if ($Name -eq 'cmake') {
                & $vcpkgExe fetch cmake | Select-Object -Last 1
            }
            elseif ($Name -eq 'ninja') {
                & $vcpkgExe fetch ninja | Select-Object -Last 1
            }
            else {
                throw "Unsupported tool name: $Name"
            }
        }
        else {
            throw 'Unsupported platform'
        }
    }
}

function Invoke-OrtBuildPy {
    [CmdletBinding()]
    param(
        [string]$RootDir = $PWD,
        [string]$PluginBuildDir = $env:PLUGIN_BUILD_DIR ?? $PWD,
        [string]$VcpkgRoot = $env:VCPKG_ROOT ? $env:VCPKG_ROOT : (Join-Path $PluginBuildDir 'vcpkg'),
        [Parameter(Mandatory = $true)]
        [string]$Command = $null,
        [string]$Arch = $null,
        [string]$Config = 'Release',
        [string]$ReducedOpsConfigPath = (Join-Path $RootDir 'src' 'required_operators_and_types.with_runtime_opt.config'),
        [string]$PythonExe = $env:PYTHON,
        [string]$VsVersionRange = '[17,]',
        [string]$OsxDeploymentTarget = $null,
        [string]$WindowsSdkVersion = $env:PLUGIN_WINDOWS_SDK_VERSION
    )
    process {
        Set-StrictMode -Version Latest; $ErrorActionPreference = 'Stop'; $PSNativeCommandUseErrorActionPreference = $true; $ProgressPreference = 'SilentlyContinue'

        $buildPyPath = Join-Path $RootDir 'onnxruntime' 'tools' 'ci_build' 'build.py'

        $buildPyArgs = @(
            '--config', $Config,
            '--parallel',
            '--compile_no_warning_as_error',
            '--disable_rtti',
            '--skip_submodule_sync',
            '--skip_tests',
            '--use_vcpkg'
        )

        $buildPyCMakeExtraDefines = @(
            'CMAKE_POLICY_VERSION_MINIMUM=3.5',
            'onnxruntime_BUILD_UNIT_TESTS=OFF'
        )

        if (Test-Path $ReducedOpsConfigPath -PathType Leaf) {
            $buildPyArgs += @(
                '--include_ops_by_config', $ReducedOpsConfigPath,
                '--enable_reduced_operator_type_support'
            )
        }

        if ($env:ORT_CCACHE_DIR) {
            $env:CCACHE_DIR = $env:ORT_CCACHE_DIR
        }

        if ($env:CCACHE_DIR) {
            $buildPyArgs += '--use_cache'
        }

        if ($IsWindows) {
            if (-not $WindowsSdkVersion) {
                $cmakePresets = Get-Content -LiteralPath (Join-Path $RootDir 'CMakePresets.json') -Raw | ConvertFrom-Json
                $windowsPreset = $cmakePresets.configurePresets | Where-Object { $_.name -eq 'windows' }
                $WindowsSdkVersion = $windowsPreset.cacheVariables.CMAKE_SYSTEM_VERSION
            }

            $ortBuildDir = Join-Path $PluginBuildDir 'build_ort'

            $buildPyArgs += @(
                '--build_dir', $ortBuildDir,
                '--cmake_generator', 'Ninja',
                '--windows_sdk_version', $WindowsSdkVersion
            )

            if ($env:CCACHE_DIR) {
                $buildPyArgs += '--use_cache'
            }

            if (-not $PythonExe) {
                $PythonExe = Join-Path $PluginBuildDir '.venv' 'Scripts' 'python.exe'
            }
        }
        elseif ($IsMacOS) {
            if (-not $Arch) {
                throw 'Arch not provided'
            }

            if (-not $OsxDeploymentTarget) {
                $cmakePresets = Get-Content -LiteralPath (Join-Path $RootDir 'CMakePresets.json') -Raw | ConvertFrom-Json
                $macOSPreset = $cmakePresets.configurePresets | Where-Object { $_.name -eq 'macos' }
                $OsxDeploymentTarget = $macOSPreset.cacheVariables.CMAKE_OSX_DEPLOYMENT_TARGET
            }

            $ortBuildDir = Join-Path $PluginBuildDir "build_ort_$Arch"

            $buildPyArgs += @(
                '--build_dir', $ortBuildDir,
                '--cmake_generator', 'Ninja',
                '--apple_deploy_target', $OsxDeploymentTarget,
                '--osx_arch', $Arch,
                '--use_coreml'
            )

            $buildPyCMakeExtraDefines += "CMAKE_OSX_DEPLOYMENT_TARGET=$OsxDeploymentTarget"

            if (-not $PythonExe) {
                $PythonExe = Join-Path $PluginBuildDir '.venv' 'bin' 'python3'
            }
        }
        else {
            throw "Unsupported platform: $($PSVersionTable.OS)"
        }

        if ($Command -eq 'update') {
            $buildPyArgs += '--update'
        }
        elseif ($Command -eq 'build') {
            $buildPyArgs += '--build'
        }
        else {
            throw "Unsupported command: $Command"
        }

        if ($IsWindows) {
            $vsInstallationPath = vswhere -version "$VsVersionRange" -property installationPath

            . (Join-Path $vsInstallationPath 'Common7' 'Tools' 'Launch-VsDevShell.ps1') -Arch amd64

            $buildPyLauncherCode = @(
              'import platform, runpy, sys',
              'from pathlib import Path',
              'platform.machine = lambda: "AMD64"',
              'sys.argv = sys.argv[1:]',
              'sys.path.insert(0, str(Path(sys.argv[0]).resolve().parent))',
              'runpy.run_path(sys.argv[0], run_name="__main__")'
            )

            & $PythonExe -c ($buildPyLauncherCode -join [Environment]::NewLine) $buildPyPath $buildPyArgs --cmake_extra_defines $buildPyCMakeExtraDefines
        }
        else {
            & $PythonExe $buildPyPath $buildPyArgs --cmake_extra_defines $buildPyCMakeExtraDefines
        }

        if ($LASTEXITCODE -ne 0) {
            throw "build.py failed with exit code $LASTEXITCODE (command: $Command)"
        }
    }
}
function Install-Ort {
    [CmdletBinding()]
    param(
        [string]$RootDir = $PWD,
        [string]$PluginBuildDir = $PWD,
        [string]$VcpkgRoot = $env:VCPKG_ROOT ? $env:VCPKG_ROOT : (Join-Path $PluginBuildDir 'vcpkg'),
        [string]$Config = 'Release',
        [string]$OsxDeploymentTarget = $null
    )
    process {
        if ($IsWindows) {
            $ortBuildDir = Join-Path $PluginBuildDir 'build_ort' $Config
            $ortInstalledDir = Join-Path $PluginBuildDir 'ort_installed'
            cmake --install $ortBuildDir --config $Config --prefix $ortInstalledDir
        }
        elseif ($IsMacOS) {
            if (-not $OsxDeploymentTarget) {
                $cmakePresets = Get-Content -LiteralPath (Join-Path $RootDir 'CMakePresets.json') -Raw | ConvertFrom-Json
                $macOSPreset = $cmakePresets.configurePresets | Where-Object { $_.name -eq 'macos' }
                $OsxDeploymentTarget = $macOSPreset.cacheVariables.CMAKE_OSX_DEPLOYMENT_TARGET
            }

            $ortBuildDirs = @{
                arm64  = Join-Path $PluginBuildDir 'build_ort_arm64' $Config
                x86_64 = Join-Path $PluginBuildDir 'build_ort_x86_64' $Config
            }
            $ortPrefixDirs = @{
                universal = Join-Path $PluginBuildDir 'ort_installed'
                arm64     = Join-Path $PluginBuildDir 'ort_arm64_installed'
                x86_64    = Join-Path $PluginBuildDir 'ort_x86_64_installed'
            }

            cmake --install $ortBuildDirs.arm64 --config $Config --prefix $ortPrefixDirs.universal
            cmake --install $ortBuildDirs.arm64 --config $Config --prefix $ortPrefixDirs.arm64
            cmake --install $ortBuildDirs.x86_64 --config $Config --prefix $ortPrefixDirs.x86_64

            $ortLibs = Get-ChildItem -Path (Join-Path $ortPrefixDirs.universal 'lib' 'libonnxruntime_*.a')
            $ortLibs += Get-ChildItem -Path (Join-Path $ortPrefixDirs.universal 'lib' 'libcoreml_proto.a')

            foreach ($lib in $ortLibs) {
                $basename = Split-Path $lib.Name -Leaf
                $components = @(
                    Join-Path $ortPrefixDirs.arm64 'lib' $basename
                    Join-Path $ortPrefixDirs.x86_64 'lib' $basename
                )
                lipo -create $components -output $lib
            }

            $dummyO = Join-Path $ortPrefixDirs.x86_64 'dummy.o'
            $dummyA = Join-Path $ortPrefixDirs.x86_64 'dummy.a'

            'void __attribute__((visibility("hidden"))) __dummy__(){}' | clang -x c -arch x86_64 -c -o $dummyO -mmacosx-version-min="$OsxDeploymentTarget" -

            libtool -static -o $dummyA $dummyO

            $kleidiaiArm64 = Join-Path $ortPrefixDirs.arm64 'lib' 'libkleidiai.a'
            $kleidiaiUniversal = Join-Path $ortPrefixDirs.universal 'lib' 'libkleidiai.a'
            lipo -create $kleidiaiArm64 $dummyA -output $kleidiaiUniversal
        }
    }
}
