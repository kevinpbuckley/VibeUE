@echo off
REM VibeUE Plugin Build Script
REM Double-click this file to build the VibeUE plugin and resolve "Missing Modules" errors

setlocal enabledelayedexpansion

echo.
echo ====================================
echo VibeUE Plugin Build Script
echo ====================================
echo.

REM Get plugin directory (where this script is located)
set "PLUGIN_DIR=%~dp0"
set "PLUGIN_DIR=%PLUGIN_DIR:~0,-1%"

REM Search for Unreal Engine installation
echo Searching for Unreal Engine installation...
set "UE_PATH="

REM Try common installation paths
for %%P in (
    "E:\Program Files\Epic Games\UE_5.6"
    "C:\Program Files\Epic Games\UE_5.6"
    "D:\Program Files\Epic Games\UE_5.6"
    "%ProgramFiles%\Epic Games\UE_5.6"
    "%ProgramFiles(x86)%\Epic Games\UE_5.6"
) do (
    if exist "%%~P\Engine\Build\BatchFiles\Build.bat" (
        set "UE_PATH=%%~P"
        goto :ue_found
    )
)

echo ERROR: Could not find Unreal Engine 5.6 installation.
echo Please install Unreal Engine 5.6 or edit this script to specify the path.
echo.
pause
exit /b 1

:ue_found
echo Found: %UE_PATH%

REM Search for .uproject file
echo Searching for .uproject file...
set "PROJECT_PATH="

REM Check current directory and parent directories
set "SEARCH_DIR=%PLUGIN_DIR%"
for /L %%i in (1,1,5) do (
    for %%F in ("!SEARCH_DIR!\*.uproject") do (
        set "PROJECT_PATH=%%F"
        goto :project_found
    )
    for %%D in ("!SEARCH_DIR!") do set "SEARCH_DIR=%%~dpD"
    set "SEARCH_DIR=!SEARCH_DIR:~0,-1!"
)

echo ERROR: Could not find .uproject file.
echo Please ensure this plugin is in a Plugins folder of an Unreal project.
echo.
pause
exit /b 1

:project_found
echo Found: %PROJECT_PATH%

REM Extract project name
for %%F in ("%PROJECT_PATH%") do set "PROJECT_NAME=%%~nF"

echo.
echo Building VibeUE plugin...
echo   Engine: %UE_PATH%
echo   Project: %PROJECT_NAME%
echo   Configuration: Development
echo.

REM Build the project
"%UE_PATH%\Engine\Build\BatchFiles\Build.bat" %PROJECT_NAME%Editor Win64 Development "%PROJECT_PATH%" -waitmutex

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ====================================
    echo Build Successful!
    echo ====================================
    echo.
    echo VibeUE plugin compiled successfully.
    echo You can now launch Unreal Engine without the "Missing Modules" error.
    echo.
    
    REM Show what was built
    if exist "%PLUGIN_DIR%\Binaries\Win64\UnrealEditor-VibeUE.dll" (
        echo Built: %PLUGIN_DIR%\Binaries\Win64\UnrealEditor-VibeUE.dll
        for %%A in ("%PLUGIN_DIR%\Binaries\Win64\UnrealEditor-VibeUE.dll") do echo Size: %%~zA bytes
    )
    echo.
) else (
    echo.
    echo ====================================
    echo Build Failed!
    echo ====================================
    echo.
    echo Please check the error messages above.
    echo.
    echo Common issues:
    echo   - Missing Visual Studio or build tools
    echo   - Incorrect Unreal Engine version
    echo   - Project file corruption
    echo.
)

pause
