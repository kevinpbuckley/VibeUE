@echo off
REM VibeUE Plugin Build Script
REM Universal build script that works with any Unreal Engine project
REM 
REM Usage Scenarios:
REM   1. Place in project's Plugins/VibeUE/ folder - builds for that project
REM   2. Standalone execution - builds as generic plugin
REM   3. Multiple .uproject files - prompts user to choose
REM
REM Uses project-specific build when possible for maximum compatibility

setlocal enabledelayedexpansion

echo.
echo ====================================
echo VibeUE Universal Plugin Build Script
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

REM Search for Unreal Engine installation using registry (standard method)
echo Searching for Unreal Engine installation...
set "UE_PATH="

REM First, check common UE 5.7 paths (prioritize before registry to find latest version)
echo Checking for Unreal Engine 5.7 installations...
for %%P in (
    "%ProgramFiles%\Epic Games\UE_5.7"
    "%ProgramFiles(x86)%\Epic Games\UE_5.7"
    "E:\Program Files\Epic Games\UE_5.7"
    "C:\Program Files\Epic Games\UE_5.7"
    "D:\Program Files\Epic Games\UE_5.7"
    "F:\Program Files\Epic Games\UE_5.7"
    "G:\Program Files\Epic Games\UE_5.7"
    "H:\Program Files\Epic Games\UE_5.7"
) do (
    if exist "%%~P\Engine\Build\BatchFiles\RunUAT.bat" (
        set "UE_PATH=%%~P"
        echo Found UE 5.7 at: !UE_PATH!
        goto :ue_found
    )
)

REM Try to find UE versions from registry
echo Checking Windows Registry for Epic Games Unreal Engine installations...
for %%V in (5.7 5.6 5.5 5.4 5.3) do (
    reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%%V" /v InstalledDirectory >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        for /f "skip=2 tokens=3*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%%V" /v InstalledDirectory') do (
            set "CANDIDATE_PATH=%%a %%b"
            if exist "!CANDIDATE_PATH!\Engine\Build\BatchFiles\RunUAT.bat" (
                set "UE_PATH=!CANDIDATE_PATH!"
                echo Found UE %%V via registry: !UE_PATH!
                goto :ue_found
            )
        )
    )
)

REM Fallback: Try common installation paths (if registry method fails)
echo Registry search completed, trying common installation paths...
REM Note: If UE 5.7 is not found, ensure it's properly installed via Epic Games Launcher
REM or check that the installation is registered in the Windows Registry
for %%P in (
    "%ProgramFiles%\Epic Games\UE_5.7"
    "%ProgramFiles(x86)%\Epic Games\UE_5.7"
    "E:\Program Files\Epic Games\UE_5.7"
    "C:\Program Files\Epic Games\UE_5.7"
    "D:\Program Files\Epic Games\UE_5.7"
    "F:\Program Files\Epic Games\UE_5.7"
    "G:\Program Files\Epic Games\UE_5.7"
    "H:\Program Files\Epic Games\UE_5.7"
    "E:\Program Files\Epic Games\UE_5.6"
    "%ProgramFiles%\Epic Games\UE_5.6"
    "%ProgramFiles%\Epic Games\UE_5.5"
    "%ProgramFiles%\Epic Games\UE_5.4"
) do (
    if exist "%%~P\Engine\Build\BatchFiles\RunUAT.bat" (
        set "UE_PATH=%%~P"
        echo Found at fallback path: !UE_PATH!
        goto :ue_found
    )
)

echo ERROR: Could not find Unreal Engine installation.
echo Please ensure Unreal Engine 5.4+ is installed via Epic Games Launcher.
echo.
echo Checked:
echo   - Windows Registry (HKLM\SOFTWARE\EpicGames\Unreal Engine)
echo   - Common installation paths
echo.
pause
exit /b 1

:ue_found
for %%F in ("%UE_PATH%") do set "UE_VERSION=%%~nxF"
echo Using Unreal Engine: %UE_VERSION%
echo Location: %UE_PATH%

REM Set output directory
set "OUTPUT_DIR=%PLUGIN_DIR%\Packaged"

echo.
echo Building VibeUE plugin...
echo   Engine: %UE_PATH%
echo   Plugin: %PLUGIN_NAME%
echo   Output: %OUTPUT_DIR%
echo   Platforms: Win64
echo.

REM Find .uproject files to build against (look in current and parent directories)
set "PROJECT_FILE="
set "SEARCH_DIR=%PLUGIN_DIR%"
set "SEARCH_LEVEL=0"
set "FOUND_PROJECTS="
set "PROJECT_COUNT=0"

echo Searching for .uproject files...
echo Starting from: %SEARCH_DIR%

:find_project
echo Checking: %SEARCH_DIR%
for %%F in ("%SEARCH_DIR%\*.uproject") do (
    set /a PROJECT_COUNT+=1
    set "FOUND_PROJECTS=!FOUND_PROJECTS!%%F|"
    echo Found project !PROJECT_COUNT!: %%~nxF
)

REM If we found projects at this level, decide what to do
if %PROJECT_COUNT% GTR 0 (
    if %PROJECT_COUNT% EQU 1 (
        REM Single project found - use it
        for %%F in ("%SEARCH_DIR%\*.uproject") do set "PROJECT_FILE=%%F"
        goto :project_found
    ) else (
        REM Multiple projects found - let user choose
        goto :choose_project
    )
)

REM Move up one directory and continue searching
for %%P in ("%SEARCH_DIR%\..") do (
    set "SEARCH_DIR=%%~fP"
)

REM Check if we've gone too far up (stop at root or if no more parent dirs)
if "%SEARCH_DIR:~-1%"=="\" if "%SEARCH_DIR:~0,-1%"=="%SEARCH_DIR:~0,2%" goto :no_project_found
if not exist "%SEARCH_DIR%" goto :no_project_found

REM Continue searching (limit to 5 levels up)
set /a "SEARCH_LEVEL+=1"
if %SEARCH_LEVEL% LSS 5 goto :find_project

:choose_project
echo.
echo Multiple Unreal projects found:
set "PROJECT_ARRAY="
set "IDX=0"
for %%P in (%FOUND_PROJECTS:~0,-1%) do (
    set /a IDX+=1
    set "PROJECT_ARRAY[!IDX!]=%%P"
    for %%F in ("%%P") do echo   !IDX!. %%~nxF
)
echo   %PROJECT_COUNT%. Build as standalone plugin (may cause compatibility issues)
echo.
set /p "CHOICE=Enter your choice (1-%PROJECT_COUNT%): "

if "%CHOICE%"=="%PROJECT_COUNT%" goto :no_project_found
if %CHOICE% LSS 1 goto :invalid_choice
if %CHOICE% GTR %PROJECT_COUNT% goto :invalid_choice

call set "PROJECT_FILE=%%PROJECT_ARRAY[%CHOICE%]%%"
goto :project_found

:invalid_choice
echo Invalid choice. Building as standalone plugin...
goto :no_project_found

:no_project_found
echo.
echo ====================================
echo Building Standalone Plugin
echo ====================================
echo.
echo No Unreal project found or standalone build selected.
echo Building as generic plugin using UAT BuildPlugin...
echo.
echo NOTE: Standalone builds may cause "Missing Modules" errors in some projects.
echo For best compatibility, place this script in: YourProject\Plugins\VibeUE\
echo.

REM Build the plugin using UAT BuildPlugin as fallback
echo Running: "%UE_PATH%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin
"%UE_PATH%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="%UPLUGIN_PATH%" -Package="%OUTPUT_DIR%" -CreateSubFolder -TargetPlatforms=Win64
goto :build_complete

:project_found
for %%F in ("%PROJECT_FILE%") do (
    set "PROJECT_NAME=%%~nF"
    set "PROJECT_DIR=%%~dpF"
    set "PROJECT_DIR=!PROJECT_DIR:~0,-1!"
)
echo.
echo Selected project: %PROJECT_NAME%
echo Project location: %PROJECT_DIR%
echo Building plugin in project context for maximum compatibility...
echo.

REM Build the project with the plugin (this ensures compatibility)
echo Running: "%UE_PATH%\Engine\Build\BatchFiles\Build.bat" %PROJECT_NAME%Editor Win64 Development "%PROJECT_FILE%" -waitmutex
"%UE_PATH%\Engine\Build\BatchFiles\Build.bat" %PROJECT_NAME%Editor Win64 Development "%PROJECT_FILE%" -waitmutex

:build_complete

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ====================================
    echo Build Successful!
    echo ====================================
    echo.
    echo VibeUE plugin compiled successfully.
    
    REM Check if binaries exist in plugin directory
    if exist "%PLUGIN_DIR%\Binaries\Win64\UnrealEditor-VibeUE.dll" (
        echo Found: %PLUGIN_DIR%\Binaries\Win64\UnrealEditor-VibeUE.dll
        for %%A in ("%PLUGIN_DIR%\Binaries\Win64\UnrealEditor-VibeUE.dll") do echo Size: %%~zA bytes
        echo.
        echo You can now launch Unreal Engine without the "Missing Modules" error.
    ) else if exist "%OUTPUT_DIR%\Binaries\Win64\*" (
        echo Copying binaries from packaged output...
        if not exist "%PLUGIN_DIR%\Binaries\Win64" (
            mkdir "%PLUGIN_DIR%\Binaries\Win64"
        )
        copy "%OUTPUT_DIR%\Binaries\Win64\*" "%PLUGIN_DIR%\Binaries\Win64\" >nul
        echo Copied binaries to: %PLUGIN_DIR%\Binaries\Win64\
        echo You can now launch Unreal Engine without the "Missing Modules" error.
    ) else (
        echo WARNING: Plugin built but no binaries found.
        echo You may still see "Missing Modules" errors.
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
    echo   - Plugin source code errors
    echo.
    echo If the project hasn't been built before, try:
    echo   1. Build the main project first
    echo   2. Then run this script
    echo.
)

pause
