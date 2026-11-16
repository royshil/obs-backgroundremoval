@REM SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
@REM
@REM SPDX-License-Identifier: Apache-2.0

@echo off
setlocal EnableDelayedExpansion

if not exist "buildspec.props" (
	echo ERROR: buildspec.props not found. The working directory MUST be the root of the project. & exit /b 1
)

set "PIPX_RUN=py -m pipx run --python 3.14"

set "CMAKE_FILES="
for /f "delims=" %%f in ('git ls-files ":/CMakeLists.txt" ":/cmake/*.cmake" ":/src/*CMakeLists.txt" ":/tests/*CMakeLists.txt"') do set "CMAKE_FILES=!CMAKE_FILES! "%%f""

set "SOURCES="
for /f "delims=" %%f in ('git ls-files ":/src/*.cpp" ":/src/*.hpp" ":/src/*.c" ":/src/*.h" ":/src/*.m" ":/src/*.mm" ":/tests/*.cpp" ":/tests/*.hpp" ":/tests/*.c" ":/tests/*.h" ":/tests/*.m" ":/tests/*.mm"') do set "SOURCES=!SOURCES! "%%f""

if "%~1"=="" goto check
if /i "%~1"=="check" goto check
if /i "%~1"=="fix" goto fix
echo Usage: %~nx0 [check^|fix] 1>&2
exit /b 2

:check
%PIPX_RUN% "reuse[charset-normalizer]==6.3.0" lint || exit /b 1
%PIPX_RUN% "gersemi==0.29.2" !CMAKE_FILES! --check || exit /b 1
%PIPX_RUN% "clang-format==21.1.8" !SOURCES! --dry-run --Werror || exit /b 1
exit /b 0

:fix
%PIPX_RUN% "gersemi==0.29.2" !CMAKE_FILES! -i || exit /b 1
%PIPX_RUN% "clang-format==21.1.8" !SOURCES! -i || exit /b 1
exit /b 0
