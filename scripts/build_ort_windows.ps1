# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# file: scripts/build_ort_windows.ps1
# description: Helper script to build ONNX Runtime for Windows.
# author: Kaito Udagawa <umireon@kaito.tokyo>
# version: 1.1.1
# date: 2026-04-04

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $true

$PYTHON = if ($env:PYTHON) { $env:PYTHON } else { 'python' }

$ROOT_DIR = Split-Path -Path $PSScriptRoot -Parent
$ORT_SRC_DIR = "$ROOT_DIR/.deps_vendor/onnxruntime"
$BUILD_PY = "$ORT_SRC_DIR/tools/ci_build/build.py"
$REDUCED_OPS_CONFIG = "$ROOT_DIR/src/required_operators_and_types.with_runtime_opt.config"
$ORT_X64_BUILD_DIR = "$ROOT_DIR/.deps_vendor/ort_x64"
$ORT_X64_PREFIX = "$ROOT_DIR/.deps_vendor/ort_x64-prefix"
$CMAKE_MASQUERADE_BIN_DIR = "$ROOT_DIR/.deps_vendor/cmake_masquerade_bin"
$CMAKE_MASQUERADE_CL_EXE = "$CMAKE_MASQUERADE_BIN_DIR/cl.exe"

$ORT_COMPONENTS = @(
  'onnxruntime_session',
  'onnxruntime_optimizer',
  'onnxruntime_providers',
  'onnxruntime_lora',
  'onnxruntime_framework',
  'onnxruntime_graph',
  'onnxruntime_util',
  'onnxruntime_mlas',
  'onnxruntime_common',
  'onnxruntime_flatbuffers'
)

$BUILD_PY_ARGS = @(
  '--config', 'Release',
  '--parallel',
  '--compile_no_warning_as_error',
  '--disable_rtti',
  '--skip_submodule_sync',
  '--skip_tests',
  '--use_vcpkg'
)

if (Test-Path $REDUCED_OPS_CONFIG -PathType Leaf) {
  $BUILD_PY_ARGS += @(
    '--include_ops_by_config', $REDUCED_OPS_CONFIG,
    '--enable_reduced_operator_type_support'
  )
}

$BUILD_PY_CMAKE_EXTRA_DEFINES = @(
  "CMAKE_POLICY_VERSION_MINIMUM=3.5",
  "onnxruntime_BUILD_UNIT_TESTS=OFF"
)

function setup_ccache() {
  if (Test-Path $CMAKE_MASQUERADE_CL_EXE -PathType Leaf) {
    Write-Host 'Masquerading cl.exe already exists. Skipping setup.'
  }
  else {
    New-Item -ItemType Directory -Path $CMAKE_MASQUERADE_BIN_DIR -Force

    $ccacheCommand = Get-Command ccache.exe -ErrorAction SilentlyContinue
    if (-not $ccacheCommand) {
      Write-Error 'ERROR: ccache.exe was not found.'
      exit 1
    }

    Copy-Item -Path $ccacheCommand.Source -Destination $CMAKE_MASQUERADE_CL_EXE -Force
  }
}

function run_build_py() {
  param(
    [string]$arch,
    [string]$command
  )

  if (!(Test-Path -Path $ORT_SRC_DIR -PathType Container)) {
    Write-Error 'ERROR: ONNX Runtime tree is not found.'
    exit 1
  }

  $commandlineArgs = @("$BUILD_PY")
  $commandlineArgs += $BUILD_PY_ARGS
  $commandlineArgs += @('--targets')
  $commandlineArgs += $ORT_COMPONENTS
  $commandlineArgs += @('--cmake_extra_defines')
  $commandlineArgs += $BUILD_PY_CMAKE_EXTRA_DEFINES
  if ($env:CCACHE_DIR) {
    $commandlineArgs += @("CMAKE_VS_GLOBALS=UseMultiToolTask=true;EnforceProcessCountAcrossBuilds=true;TrackFileAccess=false;CLToolExe=cl.exe;CLToolPath=$CMAKE_MASQUERADE_BIN_DIR")
    $commandlineArgs += @('--use_cache')
  }

  switch ($arch) {
    'x64' { $commandlineArgs += @('--build_dir', $ORT_X64_BUILD_DIR) }
    default {
      Write-Error "ERROR: Invalid arch $arch"
      exit 1
    }
  }

  switch ($command) {
    'update' { $commandlineArgs += '--update' }
    'build' { $commandlineArgs += '--build' }
    default {
      Write-Error "ERROR: Invalid command $command."
      exit 1
    }
  }

  & $PYTHON @commandlineArgs
}

function install_ort() {
  Remove-Item -Path $ORT_X64_PREFIX -Recurse -Force -ErrorAction SilentlyContinue
  cmake --install "${ORT_X64_BUILD_DIR}/Release" --config Release --prefix "$ORT_X64_PREFIX"
}

if ($args.Count -eq 0) {
  run_build_py x64 update
  run_build_py x64 build
  install_ort
}
else {
  $command = $args[0]
  $commandArgs = $args | Select-Object -Skip 1
  & $command @commandArgs
}
