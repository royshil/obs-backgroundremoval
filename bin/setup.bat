@REM SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
@REM
@REM SPDX-License-Identifier: Apache-2.0

@echo off

if not exist "buildspec.props" (
    echo ERROR: buildspec.props not found. The working directory MUST be the root of the project. & exit /b 1
)

setlocal

echo == Load buildspec.props ==

for /f "usebackq tokens=1,2 delims== eol=#" %%i in ("buildspec.props") do (
	echo %%i=%%j
	set "buildspec_%%i=%%j"
)

echo == Ensure vcpkg and its toolchains ==

if not defined VCPKG_ROOT (
	set "VCPKG_ROOT=%CD%\vcpkg"
)

if /i "%VCPKG_ROOT%" == "%CD%\vcpkg" (
	if not exist "%VCPKG_ROOT%" (
		git clone --filter blob:none --branch "%buildspec_vcpkg_default_git_tag%" https://github.com/microsoft/vcpkg.git "%VCPKG_ROOT%" || (echo ERROR: vcpkg git clone failed. & endlocal & exit /b 1)
	)

	for /f "delims=" %%i in ('git -C vcpkg rev-parse HEAD') do (
		if /i not "%%i" == "%buildspec_vcpkg_default_git_commit%" (
			echo ERROR: vcpkg commit hash mismatch. & endlocal & exit /b 1
		)
	)

	if not exist "%VCPKG_ROOT%\vcpkg.exe" (
		call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" || (echo ERROR: vcpkg bootstrap failed. & endlocal & exit /b 1)
	)
)

"%VCPKG_ROOT%\vcpkg.exe" fetch powershell-core || (echo ERROR: vcpkg fetch powershell-core failed. & endlocal & exit /b 1)

for /f "delims=" %%i in ('"%VCPKG_ROOT%\vcpkg.exe" fetch powershell-core') do set "PWSH_DIR=%%~dpi"

if not exist "%PWSH_DIR%" (
	echo ERROR: pwsh not found. & endlocal & exit /b 1
)

set "PATH=%PWSH_DIR%;%PATH%"

echo == Set up OBS sources and dependencies ==

if not exist "obs-studio" (
	git clone --filter blob:none --branch "%buildspec_obs_studio_git_tag%" https://github.com/obsproject/obs-studio.git || (echo ERROR: obs-studio git clone failed. & endlocal & exit /b 1)
)

git -C obs-studio fetch origin "%buildspec_obs_studio_git_tag%" || (echo ERROR: obs-studio git fetch failed. & endlocal & exit /b 1)
git -C obs-studio checkout --force FETCH_HEAD || (echo ERROR: obs-studio git checkout failed. & endlocal & exit /b 1)
git -C obs-studio clean -fdx || (echo ERROR: obs-studio git clean failed. & endlocal & exit /b 1)
git -C obs-studio submodule update --init --recursive --filter=blob:none || (echo ERROR: obs-studio git submodule update failed. & endlocal & exit /b 1)

for /f "delims=" %%i in ('git -C obs-studio rev-parse HEAD') do (
	if /i not "%%i" == "%buildspec_obs_studio_git_commit%" (
		echo ERROR: obs-studio commit hash mismatch. & endlocal & exit /b 1
	)
)

if not exist "%PLUGIN_BUILD_DIR%\.deps\obs-deps" (
	pwsh -NoProfile -Command "Import-Module ./scripts/BuildOBS.psm1; Initialize-ObsDeps -Component prebuilt" || (echo ERROR: Initialize-ObsDeps prebuilt failed. & endlocal & exit /b 1)
)

if not exist "%PLUGIN_BUILD_DIR%\.deps\obs-deps-qt6" (
	pwsh -NoProfile -Command "Import-Module ./scripts/BuildOBS.psm1; Initialize-ObsDeps -Component qt6" || (echo ERROR: Initialize-ObsDeps qt6 failed. & endlocal & exit /b 1)
)

echo == Set up ONNX Runtime source ==

if not exist "onnxruntime" (
	git clone --filter blob:none --branch "%buildspec_onnxruntime_git_tag%" https://github.com/microsoft/onnxruntime.git || (echo ERROR: onnxruntime git clone failed. & endlocal & exit /b 1)
)

git -C onnxruntime fetch origin "%buildspec_onnxruntime_git_tag%" || (echo ERROR: onnxruntime git fetch failed. & endlocal & exit /b 1)
git -C onnxruntime checkout --force FETCH_HEAD || (echo ERROR: onnxruntime git checkout failed. & endlocal & exit /b 1)
git -C onnxruntime clean -fdx  || (echo ERROR: onnxruntime git clean failed. & endlocal & exit /b 1)
git -C onnxruntime submodule update --init --recursive --filter=blob:none || (echo ERROR: onnxruntime git submodule update failed. & endlocal & exit /b 1)

for /f "delims=" %%i in ('git -C onnxruntime rev-parse HEAD') do (
	if not "%%i" == "%buildspec_onnxruntime_git_commit%" (
		echo ERROR: onnxruntime commit hash mismatch. & endlocal & exit /b 1
	)
)

pwsh -NoProfile -Command "Import-Module ./scripts/BuildOnnxRuntime.psm1; Update-OrtSourceWithPatches" || (echo ERROR: Update-OrtTreeWithPatches failed. & endlocal & exit /b 1)

echo == Set up python ==

if not exist ".venv" (
	python -m venv .venv || (echo ERROR: python venv creation failed. & endlocal & exit /b 1)
)

.\.venv\Scripts\pip.exe install -r requirements-build.txt || (echo ERROR: pip install failed. & endlocal & exit /b 1)

endlocal
