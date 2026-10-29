@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0BUILD_WINDOWS.ps1"
exit /b %errorlevel%
