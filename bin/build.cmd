@REM SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
@REM
@REM SPDX-License-Identifier: Apache-2.0

@echo off

if not exist "VERSION" (
	echo ERROR: VERSION not found. The working directory MUST be the root of the project. & exit /b 1
)

setlocal

echo === Ensure Visual Studio toolchain ===

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

echo == Build Plugin ==

"%CMAKE_COMMAND%" --fresh -S . -B build -G "Visual Studio 18 2026" -A x64 ^
	"-DCMAKE_GENERATOR_INSTANCE=%VS_INSTALL_PATH%" ^
	"-DCMAKE_SYSTEM_VERSION=10.0.26100" || (echo ERROR: plugin configuration failed. & endlocal & exit /b 1)

"%CMAKE_COMMAND%" --build build --config RelWithDebInfo --parallel || (echo ERROR: plugin build failed. & endlocal & exit /b 1)
"%CMAKE_COMMAND%" --install build --config RelWithDebInfo --prefix release || (echo ERROR: plugin installation failed. & endlocal & exit /b 1)

echo Copy %CD%\release\obs-backgroundremoval to %ProgramData%\obs-studio\plugins\obs-backgroundremoval for testing your build.

endlocal
