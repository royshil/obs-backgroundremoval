@REM SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
@REM
@REM SPDX-License-Identifier: Apache-2.0

@echo off

if not exist "buildspec.props" (
	echo ERROR: buildspec.props not found. The working directory MUST be the root of the project. & exit /b 1
)

setlocal

set "VCPKG_BINARY_SOURCES=clear;files,%CD%\.vcpkg_archives,readwrite"
set "VCPKG_FORCE_DOWNLOADED_BINARIES=1"
set "VCPKG_ROOT=%CD%\vendor\vcpkg"

set "HOST_PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE%"
if defined PROCESSOR_ARCHITEW6432 set "HOST_PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITEW6432%"

set "VS_HOST_ARCH=amd64"
set "VCPKG_HOST_TRIPLET=x64-windows"
if /i "%HOST_PROCESSOR_ARCHITECTURE%" == "ARM64" (
	set "VS_HOST_ARCH=arm64"
	set "VCPKG_HOST_TRIPLET=arm64-windows"
)

echo === Load buildspec.props ===

for /f "usebackq tokens=1,2 delims== eol=#" %%i in ("buildspec.props") do (
	echo %%i=%%j
	set "buildspec_%%i=%%j"
)

echo === Ensure vcpkg and its toolchain ===

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
	echo ERROR: vcpkg was not found. Run bin\setup.bat first. & endlocal & exit /b 1
)

for /f "delims=" %%i in ('""%VCPKG_ROOT%\vcpkg.exe" fetch python3_with_venv"') do set "PYTHON_COMMAND=%%i"
if not defined PYTHON_COMMAND (
	echo ERROR: Python 3 was not found. & endlocal & exit /b 1
)
for %%i in ("%PYTHON_COMMAND%") do set "PYTHON_DIR=%%~dpi"
set "PATH=%PYTHON_DIR%;%PATH%"

for /f "delims=" %%i in ('""%VCPKG_ROOT%\vcpkg.exe" fetch vswhere"') do set "VSWHERE_COMMAND=%%i"
if not defined VSWHERE_COMMAND (
	echo ERROR: Vswhere was not found. & endlocal & exit /b 1
)

for /f "delims=" %%i in ('""%VSWHERE_COMMAND%" -latest -version 18 -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath"') do set "VS_INSTALL_PATH=%%i"
if not defined VS_INSTALL_PATH (
	echo ERROR: Visual Studio 18 with the MSVC x64/x86 build tools was not found. & endlocal & exit /b 1
)

set "VSDEVCMD=%VS_INSTALL_PATH%\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
	echo ERROR: VsDevCmd.bat not found at "%VSDEVCMD%". & endlocal & exit /b 1
)

call "%VSDEVCMD%" -arch=amd64 -host_arch=%VS_HOST_ARCH% || (echo ERROR: Visual Studio developer environment setup failed. & endlocal & exit /b 1)
set "VCPKG_COMMAND=%VCPKG_ROOT%\vcpkg.exe"
if not exist "%VCPKG_COMMAND%" (
	echo ERROR: Visual Studio vcpkg was not found. & endlocal & exit /b 1
)
set "VCPKG_ROOT=%CD%\vendor\vcpkg"

for /f "delims=" %%i in ('""%VSWHERE_COMMAND%" -find "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe""') do set "CMAKE_COMMAND=%%i"
if not defined CMAKE_COMMAND (
	echo ERROR: Visual Studio CMake was not found. & endlocal & exit /b 1
)

for /f "delims=" %%i in ('""%VCPKG_ROOT%\vcpkg.exe" fetch ninja"') do set "NINJA_COMMAND=%%i"
if not defined NINJA_COMMAND (
	echo ERROR: Ninja was not found. & endlocal & exit /b 1
)

for %%i in ("%NINJA_COMMAND%") do set "NINJA_DIR=%%~dpi"
set "PATH=%NINJA_DIR%;%PATH%"

set "OBS_DEPS_PREFIX=%CD%\.deps\obs-deps"
set "OBS_DEPS_QT6_PREFIX=%CD%\.deps\obs-deps-qt6"
set "CCACHE_COMMAND=%CD%\.deps\ccache\ccache-%buildspec_ccache_windows_version%-windows-x86_64\ccache.exe"

if not exist "%OBS_DEPS_PREFIX%" (
	echo ERROR: OBS dependencies were not found. Run bin\setup.bat first. & endlocal & exit /b 1
)
if not exist "%OBS_DEPS_QT6_PREFIX%" (
	echo ERROR: OBS Qt dependencies were not found. Run bin\setup.bat first. & endlocal & exit /b 1
)
if not exist "%CCACHE_COMMAND%" (
	echo ERROR: ccache was not found. Run bin\setup.bat first. & endlocal & exit /b 1
)

echo == Install vcpkg dependencies ==

if exist "vcpkg_installed\x64-windows-static-md-obs\lib\cpuinfo.lib" goto vcpkg_install_done
"%VCPKG_COMMAND%" install --vcpkg-root "%VCPKG_ROOT%" --triplet x64-windows-static-md-obs --host-triplet "%VCPKG_HOST_TRIPLET%" || (echo ERROR: vcpkg dependency installation failed. & endlocal & exit /b 1)
:vcpkg_install_done

echo == Install vcpkg_ort dependencies ==

if exist "vcpkg_ort_installed\x64-windows-static-md-obs-ort\lib\onnx.lib" goto vcpkg_ort_install_done
"%VCPKG_COMMAND%" install --vcpkg-root "%VCPKG_ROOT%" --triplet x64-windows-static-md-obs-ort --host-triplet "%VCPKG_HOST_TRIPLET%" --overlay-triplets "%CD%\vcpkg-triplets" --x-install-root vcpkg_ort_installed --x-manifest-root vendor\onnxruntime\cmake || (echo ERROR: ONNX Runtime vcpkg dependency installation failed. & endlocal & exit /b 1)
:vcpkg_ort_install_done

echo == Build OBS sources ==

if exist "build_obs\CMakeCache.txt" goto obs_config_done
"%CMAKE_COMMAND%" -S vendor\obs-studio -B build_obs -G "Visual Studio 18 2026" -A x64 ^
	"-DCMAKE_BUILD_TYPE=Release" ^
	"-DCMAKE_INSTALL_PREFIX=%CD%\obs_installed" ^
	"-DCMAKE_PREFIX_PATH=%OBS_DEPS_PREFIX%;%OBS_DEPS_QT6_PREFIX%" ^
	"-DCMAKE_SYSTEM_VERSION=%buildspec_windows_sdk_version%" ^
	"-DENABLE_FRONTEND=OFF" ^
	"-DENABLE_PLUGINS=OFF" ^
	"-DOBS_CMAKE_VERSION=3.0.0" ^
	"-DOBS_VERSION_OVERRIDE=%buildspec_obs_studio_git_tag%" || (echo ERROR: OBS Studio configuration failed. & endlocal & exit /b 1)
:obs_config_done

"%CMAKE_COMMAND%" --build build_obs --target obs-frontend-api --config Release --parallel || (echo ERROR: OBS Studio build failed. & endlocal & exit /b 1)
"%CMAKE_COMMAND%" --install build_obs --component Development --config Release --prefix obs_installed || (echo ERROR: OBS Studio installation failed. & endlocal & exit /b 1)

echo == Build ONNX Runtime ==

set "CCACHE_DIR=%CD%\.ccache_ort"
set "CCACHE_SLOPPINESS=include_file_mtime,time_macros"
set "CCACHE_BASEDIR=%CD%"

if exist "build_ort\CMakeCache.txt" goto ort_config_done
"%PYTHON_COMMAND%" -m pip install -r requirements-build.txt || (echo ERROR: Python build dependency installation failed. & endlocal & exit /b 1)
"%PYTHON_COMMAND%" vendor\onnxruntime\tools\ci_build\reduce_op_kernels.py "%buildspec_onnxruntime_reduced_ops_config%" --cmake_build_dir build_ort --is_extended_minimal_build_or_higher || (echo ERROR: ONNX Runtime operator reduction failed. & endlocal & exit /b 1)

"%CMAKE_COMMAND%" -S vendor\onnxruntime\cmake -B build_ort -G Ninja --compile-no-warning-as-error ^
	"-DCMAKE_BUILD_TYPE=Release" ^
	"-DCMAKE_C_COMPILER_LAUNCHER=%CCACHE_COMMAND%" ^
	"-DCMAKE_CXX_COMPILER_LAUNCHER=%CCACHE_COMMAND%" ^
	"-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON" ^
	"-DCMAKE_POLICY_VERSION_MINIMUM=3.5" ^
	"-DCMAKE_SYSTEM_NAME=Windows" ^
	"-DCMAKE_SYSTEM_PROCESSOR=AMD64" ^
	"-DCMAKE_SYSTEM_VERSION=%buildspec_windows_sdk_version%" ^
	"-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
	"-DPython_EXECUTABLE=%PYTHON_COMMAND%" ^
	"-DPython3_EXECUTABLE=%PYTHON_COMMAND%" ^
	"-DVCPKG_INSTALLED_DIR=%CD%\vcpkg_ort_installed" ^
	"-DVCPKG_MANIFEST_INSTALL=OFF" ^
	"-DVCPKG_OVERLAY_TRIPLETS=%CD%\vcpkg-triplets" ^
	"-DVCPKG_TARGET_TRIPLET=x64-windows-static-md-obs-ort" ^
	"-Donnxruntime_BUILD_SHARED_LIB=ON" ^
	"-Donnxruntime_BUILD_UNIT_TESTS=OFF" ^
	"-Donnxruntime_DISABLE_RTTI=OFF" ^
	"-Donnxruntime_REDUCED_OPS_BUILD=ON" ^
	"-Donnxruntime_RUN_ONNX_TESTS=OFF" ^
	"-Donnxruntime_USE_VCPKG=ON" ^
	"-Donnxruntime_USE_WEBGPU=ON" || (echo ERROR: ONNX Runtime configuration failed. & endlocal & exit /b 1)
:ort_config_done

if /i "%HOST_PROCESSOR_ARCHITECTURE%" == "ARM64" (
	echo == Patch Dawn DXC cross-compilation support ==
        git apply --check --directory=build_ort/_deps/dawn-src cmake/patches/dawn-dxc-cross-compile-source-dir.patch >nul 2>&1
        if errorlevel 1 (
                git apply --reverse --check --directory=build_ort/_deps/dawn-src cmake/patches/dawn-dxc-cross-compile-source-dir.patch >nul 2>&1
                if errorlevel 1 (
                        echo ERROR: Dawn DXC cross-compilation patch does not apply cleanly. & endlocal & exit /b 1
                )
                echo Dawn DXC cross-compilation patch is already applied.
	) else (
		git apply --directory=build_ort/_deps/dawn-src cmake/patches/dawn-dxc-cross-compile-source-dir.patch || (echo ERROR: Dawn DXC cross-compilation patch failed. & endlocal & exit /b 1)
		if exist "build_ort\NATIVE" rmdir /s /q "build_ort\NATIVE"
		mkdir "build_ort\NATIVE" || (echo ERROR: Failed to create the Dawn DXC native build directory. & endlocal & exit /b 1)
		"%CMAKE_COMMAND%" -S vendor\onnxruntime\cmake -B build_ort --compile-no-warning-as-error || (echo ERROR: ONNX Runtime reconfiguration after patching Dawn DXC failed. & endlocal & exit /b 1)
	)
)

"%CMAKE_COMMAND%" --build build_ort --config Release --parallel || (echo ERROR: ONNX Runtime build failed. & endlocal & exit /b 1)
"%CMAKE_COMMAND%" --install build_ort --config Release --prefix ort_installed || (echo ERROR: ONNX Runtime installation failed. & endlocal & exit /b 1)

set "CCACHE_DIR="
set "CCACHE_SLOPPINESS="
set "CCACHE_BASEDIR="

echo == Build Plugin ==

set "CMAKE_PREFIX_PATH=%OBS_DEPS_PREFIX%;%OBS_DEPS_QT6_PREFIX%;%CD%\obs_installed;%CD%\vcpkg_ort_installed\x64-windows-static-md-obs-ort;%CD%\ort_installed;%CD%\vcpkg_installed\x64-windows-static-md-obs"

if exist "build\CMakeCache.txt" goto plugin_config_done
"%CMAKE_COMMAND%" -S . -B build -G Ninja ^
	"-DCMAKE_BUILD_TYPE=RelWithDebInfo" ^
	"-DCMAKE_PREFIX_PATH=%CMAKE_PREFIX_PATH%" ^
	"-DCMAKE_SYSTEM_VERSION=%buildspec_windows_sdk_version%" || (echo ERROR: plugin configuration failed. & endlocal & exit /b 1)
:plugin_config_done

"%CMAKE_COMMAND%" --build build --config RelWithDebInfo --parallel || (echo ERROR: plugin build failed. & endlocal & exit /b 1)
"%CMAKE_COMMAND%" --install build --config RelWithDebInfo --prefix release || (echo ERROR: plugin installation failed. & endlocal & exit /b 1)

copy /y "ort_installed\bin\onnxruntime.dll" "release\obs-backgroundremoval\bin\64bit\onnxruntime.dll" >nul || (echo ERROR: Failed to install onnxruntime.dll. & endlocal & exit /b 1)
copy /y "build_ort\dxcompiler.dll" "release\obs-backgroundremoval\bin\64bit\dxcompiler.dll" >nul || (echo ERROR: Failed to install dxcompiler.dll. & endlocal & exit /b 1)
copy /y "build_ort\dxil.dll" "release\obs-backgroundremoval\bin\64bit\dxil.dll" >nul || (echo ERROR: Failed to install dxil.dll. & endlocal & exit /b 1)

echo Copy %CD%\release\obs-backgroundremoval to %ProgramData%\obs-studio\plugins\obs-backgroundremoval for testing your build.

endlocal
