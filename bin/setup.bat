@REM SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
@REM
@REM SPDX-License-Identifier: Apache-2.0

@echo off

if not exist "buildspec.props" (
    echo ERROR: buildspec.props not found. The working directory MUST be the root of the project. & exit /b 1
)

setlocal

echo == Initialize submodules ==

git submodule update --init --filter=blob:none vendor/obsproject/obs-studio vendor/curl/curl || (echo ERROR: submodule initialization failed. & endlocal & exit /b 1)

echo == Ensure Visual Studio toolchain ==

set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
where /q vswhere.exe || (echo ERROR: Vswhere was not found. & endlocal & exit /b 1)

for /f "delims=" %%i in ('vswhere.exe -latest -version "[18,19)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VS_INSTALL_PATH=%%i"
if not defined VS_INSTALL_PATH (
	echo ERROR: Visual Studio 18 with the MSVC x64/x86 build tools was not found. & endlocal & exit /b 1
)

set "CMAKE_COMMAND=%VS_INSTALL_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE_COMMAND%" (
	echo ERROR: Visual Studio CMake was not found. & endlocal & exit /b 1
)

echo === Set up OBS dependencies ===

"%CMAKE_COMMAND%" -P scripts\download-deps.cmake || (echo ERROR: download-deps.cmake failed. & endlocal & exit /b 1)

endlocal
