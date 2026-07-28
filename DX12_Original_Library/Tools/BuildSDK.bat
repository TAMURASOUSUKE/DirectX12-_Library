@echo off
setlocal

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0BuildSDK.ps1"

set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    echo.
    echo [ERROR] BuildSDK.ps1 failed. ExitCode=%RESULT%
    pause
    exit /b %RESULT%
)

echo.
echo [SUCCESS] BuildSDK.ps1 completed.
pause
exit /b 0