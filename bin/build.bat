@REM SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
@REM
@REM SPDX-License-Identifier: Apache-2.0

@echo off

if not exist "buildspec.props" (
	echo ERROR: buildspec.props not found. The working directory MUST be the root of the project. & exit /b 1
)

setlocal

set "HOST_PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE%"
if defined PROCESSOR_ARCHITEW6432 set "HOST_PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITEW6432%"

set "VS_HOST_ARCH=x64"
if /i "%HOST_PROCESSOR_ARCHITECTURE%" == "ARM64" (
	set "VS_HOST_ARCH=arm64"
)

echo === Load buildspec.props ===

for /f "usebackq tokens=1,2 delims== eol=#" %%i in ("buildspec.props") do (
	echo %%i=%%j
	set "buildspec_%%i=%%j"
)

echo === Ensure Visual Studio toolchain ===

set "ORIGINAL_BUILD_PATH=%PATH%"
set "PATH="
set "Path=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%ORIGINAL_BUILD_PATH%"
set "ORIGINAL_BUILD_PATH="
set "VSWHERE_COMMAND=vswhere.exe"

for /f "delims=" %%i in ('%VSWHERE_COMMAND% -latest -version 18 -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VS_INSTALL_PATH=%%i"
if not exist "%VS_INSTALL_PATH%\Common7\Tools\VsDevCmd.bat" (
	echo ERROR: Visual Studio developer tools were not found at "%VS_INSTALL_PATH%". & endlocal & exit /b 1
)

set "VSLANG=1033"
set "DOTNET_CLI_UI_LANGUAGE=en-US"
call "%VS_INSTALL_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=%VS_HOST_ARCH% >nul
if errorlevel 1 (
	echo ERROR: Visual Studio developer environment could not be initialized. & endlocal & exit /b 1
)

for /f "delims=" %%i in ('%VSWHERE_COMMAND% -find "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"') do set "CMAKE_COMMAND=%%i"
if not defined CMAKE_COMMAND (
	echo ERROR: Visual Studio CMake was not found. & endlocal & exit /b 1
)

set "OBS_DEPS_PREFIX=%CD%\.deps\obs-deps"
set "OBS_DEPS_QT6_PREFIX=%CD%\.deps\obs-deps-qt6"

if not exist "%OBS_DEPS_PREFIX%" (
	echo ERROR: OBS dependencies were not found. Run bin\setup.bat first. & endlocal & exit /b 1
)
if not exist "%OBS_DEPS_QT6_PREFIX%" (
	echo ERROR: OBS Qt dependencies were not found. Run bin\setup.bat first. & endlocal & exit /b 1
)
echo == Build Plugin ==

set "CMAKE_PREFIX_PATH=%OBS_DEPS_PREFIX%;%OBS_DEPS_QT6_PREFIX%"
set "MSVC_COMPILER=%VCToolsInstallDir:\=/%bin/Hostx64/x64/cl.exe"

"%CMAKE_COMMAND%" --fresh -S . -B build -G "Visual Studio 18 2026" -A x64 -T host=%VS_HOST_ARCH% ^
	"-DCMAKE_PREFIX_PATH=%CMAKE_PREFIX_PATH%" ^
	"-DCMAKE_GENERATOR_INSTANCE=%VS_INSTALL_PATH%" ^
	"-DCMAKE_C_COMPILER=%MSVC_COMPILER%" ^
	"-DCMAKE_CXX_COMPILER=%MSVC_COMPILER%" ^
	"-DCMAKE_SYSTEM_VERSION=%buildspec_windows_sdk_version%" || (echo ERROR: plugin configuration failed. & endlocal & exit /b 1)

"%CMAKE_COMMAND%" --build build --config RelWithDebInfo --parallel || (echo ERROR: plugin build failed. & endlocal & exit /b 1)
"%CMAKE_COMMAND%" --install build --config RelWithDebInfo --prefix release || (echo ERROR: plugin installation failed. & endlocal & exit /b 1)

echo Copy %CD%\release\obs-backgroundremoval to %ProgramData%\obs-studio\plugins\obs-backgroundremoval for testing your build.

endlocal
