@echo off
REM Build script for Windows executable
REM Usage: build_windows.bat

setlocal enabledelayedexpansion

echo ======================================
echo PicoSignals Config Editor - Windows Build
echo ======================================
echo.

REM Check if PyInstaller is installed
where pyinstaller >nul 2>nul
if errorlevel 1 (
    echo Error: PyInstaller is not installed
    echo Install it with: pip install pyinstaller
    exit /b 1
)

REM Create build directories
if not exist "dist" mkdir dist
if not exist "build" mkdir build

REM Build the Windows executable
echo Building Windows executable...
pyinstaller config_editor_windows.spec

REM Check if build was successful
if exist "dist\config_editor\config_editor.exe" (
    echo.
    echo ✓ Build successful!
    echo.
    echo Output location:
    echo   dist\config_editor\config_editor.exe
    echo.
    echo To run the app:
    echo   dist\config_editor\config_editor.exe
) else (
    echo ✗ Build failed
    exit /b 1
)

endlocal
