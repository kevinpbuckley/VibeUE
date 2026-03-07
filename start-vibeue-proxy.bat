@echo off
:: VibeUE MCP Proxy Launcher
:: Starts the proxy in the background (no console window).
:: Add this to Windows startup: Win+R -> shell:startup -> paste shortcut here.

set SCRIPT=%~dp0vibeue-proxy.py

:: Set your VibeUE MCP server bearer token here (leave blank for no auth)
set VIBEUE_BEARER_TOKEN=vibe_claude

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
