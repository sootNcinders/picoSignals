@echo off
REM Convenience script to build the Config Editor for Windows from the root picoSignals directory
REM Usage: build_config_editor_windows.bat

pushd "%~dp0ConfigEditor"

if not exist "build_windows.bat" (
    echo Error: ConfigEditor\build_windows.bat not found
    popd
    exit /b 1
)

call build_windows.bat
popd
