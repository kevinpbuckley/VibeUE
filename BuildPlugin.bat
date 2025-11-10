@echo off
REM VibeUE Plugin Build Script
REM Double-click this file to build the VibeUE plugin for any Unreal Engine project
REM Uses UAT BuildPlugin which doesn't require project-specific Editor targets

setlocal enabledelayedexpansion

echo.
echo ====================================
echo VibeUE Plugin Build Script
echo ====================================
echo.

REM Get plugin directory (where this script is located)
set "PLUGIN_DIR=%~dp0"
set "PLUGIN_DIR=%PLUGIN_DIR:~0,-1%"

REM Find .uplugin file
set "UPLUGIN_PATH="
for %%F in ("%PLUGIN_DIR%\*.uplugin") do (
    set "UPLUGIN_PATH=%%F"
    goto :uplugin_found
)

echo ERROR: Could not find .uplugin file in plugin directory.
echo Please ensure this script is in the VibeUE plugin root folder.
echo.
pause
exit /b 1

:uplugin_found
for %%F in ("%UPLUGIN_PATH%") do set "PLUGIN_NAME=%%~nF"
echo Found plugin: %PLUGIN_NAME%

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
    if exist "%%~P\Engine\Build\BatchFiles\RunUAT.bat" (
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

REM Set output directory
set "OUTPUT_DIR=%PLUGIN_DIR%\Packaged"

echo.
echo Building VibeUE plugin...
echo   Engine: %UE_PATH%
echo   Plugin: %PLUGIN_NAME%
echo   Output: %OUTPUT_DIR%
echo   Platforms: Win64
echo.

REM Build the plugin using UAT BuildPlugin
REM This works for any project and doesn't require project-specific Editor targets
"%UE_PATH%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%UPLUGIN_PATH%" -Package="%OUTPUT_DIR%" -CreateSubFolder -TargetPlatforms=Win64

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
