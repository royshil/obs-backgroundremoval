@REM SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
@REM
@REM SPDX-License-Identifier: Apache-2.0

@echo off

if not exist "buildspec.props" (
	echo ERROR: buildspec.props not found. The working directory MUST be the root of the project. & exit /b 1
)

setlocal

if not defined PLUGIN_CONFIG (
	set "PLUGIN_CONFIG=RelWithDebInfo"
)

echo == Load buildspec.props ==

for /f "usebackq tokens=1,2 delims== eol=#" %%i in ("buildspec.props") do (
	echo %%i=%%j
	set "buildspec_%%i=%%j"
)

echo == Ensure vcpkg and its toolchain ==

if not defined VCPKG_ROOT (
	set "VCPKG_ROOT=%CD%\vcpkg"
)

if not defined VCPKG_BINARY_SOURCES (
	set "VCPKG_BINARY_SOURCES=clear;files,%CD%\.vcpkg_archives,readwrite;http,https://vcpkg-obs.kaito.tokyo/{name}/{version}/{sha},read"
)

"%VCPKG_ROOT%\vcpkg.exe" fetch cmake  || (echo ERROR: vcpkg fetch powershell-core failed. & endlocal & exit /b 1)
"%VCPKG_ROOT%\vcpkg.exe" fetch powershell-core  || (echo ERROR: vcpkg fetch powershell-core failed. & endlocal & exit /b 1)

for /f "delims=" %%i in ('"%VCPKG_ROOT%\vcpkg.exe" fetch cmake') do set "CMAKE_DIR=%%~dpi"
if not exist "%CMAKE_DIR%" (
	echo ERROR: cmake not found. & endlocal & exit /b 1
)

for /f "delims=" %%i in ('"%VCPKG_ROOT%\vcpkg.exe" fetch powershell-core') do set "PWSH_DIR=%%~dpi"
if not exist "%PWSH_DIR%" (
	echo ERROR: pwsh not found. & endlocal & exit /b 1
)

set "PATH=%CMAKE_DIR%;%PWSH_DIR%;%PATH%"

echo == Install vcpkg dependencies ==

"%VCPKG_ROOT%\vcpkg.exe" install --triplet=x64-windows-static-md-obs --x-install-root=vcpkg_installed

echo == Build OBS sources ==

pwsh -NoProfile -Command "Import-Module ./scripts/BuildOBS.psm1 && Invoke-ObsConfigure" || (echo ERROR: Invoke-ObsConfigure failed. & endlocal & exit /b 1)
pwsh -NoProfile -Command "Import-Module ./scripts/BuildOBS.psm1 && Invoke-ObsBuild" || (echo ERROR: Invoke-ObsBuild failed. & endlocal & exit /b 1)
pwsh -NoProfile -Command "Import-Module ./scripts/BuildOBS.psm1 && Install-Obs" || (echo ERROR: Install-Obs failed. & endlocal & exit /b 1)

echo == Build ONNX Runtime ==

set "CCACHE_DIR=%CD%\.ccache_ort"
set "CCACHE_SLOPPINESS=include_file_mtime,time_macros"
set "CCACHE_BASEDIR=%CD%"

pwsh -NoProfile -Command "Import-Module ./scripts/BuildOnnxRuntime.psm1 && Invoke-OrtBuildPy -Command update" || (echo ERROR: Invoke-ObsConfigure failed. & endlocal & exit /b 1)
pwsh -NoProfile -Command "Import-Module ./scripts/BuildOnnxRuntime.psm1 && Invoke-OrtBuildPy -Command build" || (echo ERROR: Invoke-ObsBuild failed. & endlocal & exit /b 1)
pwsh -NoProfile -Command "Import-Module ./scripts/BuildOnnxRuntime.psm1 && Install-Ort" || (echo ERROR: Install-Obs failed. & endlocal & exit /b 1)

set "CCACHE_DIR="
set "CCACHE_SLOPPINESS="
set "CCACHE_BASEDIR="

echo == Build Plugin ==

cmake --preset windows -DCMAKE_BUILD_TYPE="%PLUGIN_CONFIG%" || (echo ERROR: cmake configure failed. & endlocal & exit /b 1)
cmake --build --preset windows --config "%PLUGIN_CONFIG%" || (echo ERROR: cmake configure failed. & endlocal & exit /b 1)
cmake --install build --config "%PLUGIN_CONFIG%" --prefix release || (echo ERROR: cmake install failed. & endlocal & exit /b 1)

echo Copy %CD%\release\obs-backgroundremoval to %ProgramData%\obs-studio\plugins\obs-backgroundremoval for testing your build.

endlocal
