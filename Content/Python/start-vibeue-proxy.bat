@echo off
:: VibeUE MCP Proxy Launcher
:: Starts the proxy in the background (no console window).
:: Kills any existing proxy instance before starting a new one.
:: Add this to Windows startup: Win+R -> shell:startup -> paste shortcut here.

set SCRIPT=%~dp0vibeue-proxy.py

:: Kill any existing proxy process listening on port 8089
powershell -NoProfile -Command "Get-NetTCPConnection -LocalPort 8089 -State Listen -ErrorAction SilentlyContinue | ForEach-Object { Write-Host ('Stopping existing proxy (PID ' + $_.OwningProcess + ')...'); Stop-Process -Id $_.OwningProcess -Force -ErrorAction SilentlyContinue }"

:: Check Python is available
where pythonw >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    start "" /B pythonw "%SCRIPT%"
    echo VibeUE proxy started on port 8089
) else (
    where python >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        start "" /B /MIN python "%SCRIPT%"
        echo VibeUE proxy started on port 8089
    ) else (
        echo ERROR: Python not found. Install Python 3 and ensure it is on PATH.
        pause
    )
)
