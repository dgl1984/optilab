@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\validate-jsfx.ps1" %*
exit /b %errorlevel%
