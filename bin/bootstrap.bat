@REM SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
@REM
@REM SPDX-License-Identifier: Apache-2.0

@echo off

for /f "delims=" %%i in ('where python 2^>nul') do (    
    if /i "%%i" == "%USERPROFILE%\AppData\Local\Microsoft\WindowsApps\python.exe" (
        REM App execution alias (Microsoft Store) - ignore and continue searching
    ) else (
        set "PYTHON_EXE=%%i"
    )
)

"%PYTHON_EXE%" --version

if %ERRORLEVEL% equ 0 (
    echo Python found at %PYTHON_EXE%.
) else (
    echo Python not found. Installing Python 3.13 using winget...
    winget install Python.Python.3.13 || (echo ERROR: winget install Python.Python.3.13 failed. & endlocal & exit /b 1)
)

echo Bootstrap complete. Please reboot the computer if any Python was newly installed.

endlocal
