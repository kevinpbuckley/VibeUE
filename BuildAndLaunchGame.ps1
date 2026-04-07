# Build and Launch an Unreal Engine project in Development Mode without Debugger
# Can live anywhere inside the project tree (e.g. Plugins/VibeUE).
# Walks UP from its own directory until it finds a .uproject file.

param(
    [string]$Mode = "Development",
    [switch]$Clean,
    [switch]$SkipBuild
)

# ============================================================================
# Walk up the directory tree to find the .uproject
# ============================================================================
$searchDir = $PSScriptRoot
$uprojectFile = $null

while ($searchDir) {
    $found = Get-ChildItem -Path $searchDir -Filter "*.uproject" -File -ErrorAction SilentlyContinue
    if ($found) {
        if ($found.Count -gt 1) {
            Write-Host "WARNING: Multiple .uproject files found in $searchDir, using first: $($found[0].Name)" -ForegroundColor Yellow
        }
        $uprojectFile = $found[0]
        break
    }
    $parent = Split-Path $searchDir -Parent
    if ($parent -eq $searchDir) { break }   # reached filesystem root
    $searchDir = $parent
}

if (-not $uprojectFile) {
    Write-Host "ERROR: No .uproject file found walking up from $PSScriptRoot" -ForegroundColor Red
    exit 1
}

$projectPath = $uprojectFile.FullName
$projectName = $uprojectFile.BaseName
$projectRoot = $uprojectFile.DirectoryName

# ============================================================================
# Auto-discover Unreal Engine install from EngineAssociation in .uproject
# ============================================================================
$uprojectJson = Get-Content $projectPath -Raw | ConvertFrom-Json
$engineAssociation = $uprojectJson.EngineAssociation

# First try: look up custom/source builds from registry (HKCU)
$enginePath = $null
try {
    $customBuilds = Get-ItemProperty "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue
    if ($customBuilds -and $customBuilds.$engineAssociation) {
        $enginePath = $customBuilds.$engineAssociation
    }
} catch {}

# Second try: standard launcher installs (HKLM)
if (-not $enginePath) {
    try {
        $regPath = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$engineAssociation"
        $regEntry = Get-ItemProperty $regPath -ErrorAction SilentlyContinue
        if ($regEntry -and $regEntry.InstalledDirectory) {
            $enginePath = $regEntry.InstalledDirectory
        }
    } catch {}
}

# Third try: well-known path pattern (e.g. UE_5.7)
if (-not $enginePath) {
    $candidates = @(
        "E:\Program Files\Epic Games\UE_$engineAssociation",
        "C:\Program Files\Epic Games\UE_$engineAssociation",
        "D:\Program Files\Epic Games\UE_$engineAssociation"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $enginePath = $c; break }
    }
}

if (-not $enginePath) {
    Write-Host "ERROR: Could not locate Unreal Engine '$engineAssociation'. Set the path manually or ensure it is registered." -ForegroundColor Red
    exit 1
}

$buildBat  = Join-Path $enginePath "Engine\Build\BatchFiles\Build.bat"
$runUatBat = Join-Path $enginePath "Engine\Build\BatchFiles\RunUAT.bat"
$editorExe = Join-Path $enginePath "Engine\Binaries\Win64\UnrealEditor.exe"
$pluginDescriptor = Get-ChildItem -Path $PSScriptRoot -Filter "*.uplugin" -File -ErrorAction SilentlyContinue | Select-Object -First 1
$projectEditorTarget = Join-Path $projectRoot "Source\${projectName}Editor.Target.cs"
$hasProjectEditorTarget = Test-Path $projectEditorTarget

Write-Host "=== $projectName Build and Launch Script ===" -ForegroundColor Cyan
Write-Host "Script  : $PSScriptRoot" -ForegroundColor Gray
Write-Host "Project : $projectPath" -ForegroundColor Yellow
Write-Host "Engine  : $enginePath" -ForegroundColor Yellow
Write-Host "Mode    : $Mode" -ForegroundColor Yellow

function Wait-ForAutomationTool {
    param(
        [int]$TimeoutSeconds = 120
    )

    $elapsed = 0
    while ($true) {
        $automationToolProcesses = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
            ($_.Name -ieq "dotnet.exe" -or $_.Name -ieq "AutomationTool.exe") -and
            $_.CommandLine -and
            $_.CommandLine -like "*AutomationTool*"
        }

        if (-not $automationToolProcesses) {
            return $true
        }

        if ($elapsed -eq 0) {
            Write-Host "Waiting for an existing AutomationTool process to finish..." -ForegroundColor Yellow
        }

        if ($elapsed -ge $TimeoutSeconds) {
            return $false
        }

        Start-Sleep 1
        $elapsed++
    }
}

# Stop any running Unreal Engine processes gracefully
$unrealProcesses = Get-Process | Where-Object { 
    $_.ProcessName -like "UnrealEditor*" -or 
    $_.ProcessName -like "CrashReportClient*" -or 
    $_.ProcessName -eq "UnrealTraceServer" -or
    ($_.ProcessName -eq "crashpad_handler" -and $_.MainModule.FileName -like "*Epic Games*")
}

if ($unrealProcesses) {
    Write-Host "Requesting Unreal Engine processes to close gracefully..." -ForegroundColor Yellow
    $unrealProcesses | ForEach-Object {
        if ($_.MainWindowHandle -ne 0) {
            $_.CloseMainWindow() | Out-Null
        }
    }
    
    # Wait up to 15 seconds for graceful shutdown
    $timeout = 15
    $elapsed = 0
    $remaining = Get-Process | Where-Object { 
        $_.ProcessName -like "UnrealEditor*" -or 
        $_.ProcessName -like "CrashReportClient*" -or 
        $_.ProcessName -eq "UnrealTraceServer" -or
        ($_.ProcessName -eq "crashpad_handler" -and $_.MainModule.FileName -like "*Epic Games*")
    }
    
    while ($remaining -and ($elapsed -lt $timeout)) {
        Start-Sleep 1
        $elapsed++
        Write-Host "  Waiting for processes to close... ($elapsed/$timeout seconds)" -ForegroundColor Gray
        $remaining = Get-Process | Where-Object { 
            $_.ProcessName -like "UnrealEditor*" -or 
            $_.ProcessName -like "CrashReportClient*" -or 
            $_.ProcessName -eq "UnrealTraceServer" -or
            ($_.ProcessName -eq "crashpad_handler" -and $_.MainModule.FileName -like "*Epic Games*")
        }
    }
    
    # Force kill if still running
    $remaining = Get-Process | Where-Object { 
        $_.ProcessName -like "UnrealEditor*" -or 
        $_.ProcessName -like "CrashReportClient*" -or 
        $_.ProcessName -eq "UnrealTraceServer" -or
        ($_.ProcessName -eq "crashpad_handler" -and $_.MainModule.FileName -like "*Epic Games*")
    }
    
    if ($remaining) {
        Write-Host "Processes didn't close gracefully, forcing shutdown..." -ForegroundColor Yellow
        $remaining | ForEach-Object {
            Write-Host "  Killing $($_.ProcessName) (PID: $($_.Id)) and child processes..." -ForegroundColor Gray
            taskkill /F /PID $_.Id /T 2>$null
        }
        Start-Sleep 3
        
        $stillRunning = Get-Process | Where-Object { $_.ProcessName -like "UnrealEditor*" }
        if ($stillRunning) {
            Write-Host "  Some processes still running, using taskkill by name..." -ForegroundColor Yellow
            taskkill /F /IM UnrealEditor.exe /T 2>$null
            Start-Sleep 2
        }
        Write-Host "All Unreal Engine processes terminated!" -ForegroundColor Green
    } else {
        Write-Host "All processes closed gracefully!" -ForegroundColor Green
    }
} else {
    Write-Host "No running Unreal Engine processes found." -ForegroundColor Gray
}

# Clean build if requested (relative to project root)
if ($Clean) {
    Write-Host "Cleaning intermediate and binary files..." -ForegroundColor Yellow
    $cleanPaths = @(
        (Join-Path $projectRoot "Intermediate"),
        (Join-Path $projectRoot "Binaries")
    )
    foreach ($p in $cleanPaths) {
        if (Test-Path $p) { Remove-Item $p -Recurse -Force }
    }
    
    # Clean plugin binaries too
    Get-ChildItem (Join-Path $projectRoot "Plugins") -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        foreach ($sub in @("Binaries","Intermediate")) {
            $subPath = Join-Path $_.FullName $sub
            if (Test-Path $subPath) { Remove-Item $subPath -Recurse -Force }
        }
    }
}

# Build the project
if (-not $SkipBuild) {
    if ($pluginDescriptor -and -not $hasProjectEditorTarget) {
        $packageRoot = Join-Path $PSScriptRoot "Packaged"
        $packageDir = Join-Path $packageRoot (Get-Date -Format "yyyyMMdd-HHmmss")
        $packagedBinaries = Join-Path $packageDir "Binaries\Win64"
        $pluginBinaries = Join-Path $PSScriptRoot "Binaries\Win64"

        Write-Host "Blueprint-only project detected. Building plugin standalone for launch..." -ForegroundColor Yellow

        if (-not (Wait-ForAutomationTool)) {
            Write-Host "Timed out waiting for another AutomationTool process to finish. Close the other build or wait for it to complete, then rerun the script." -ForegroundColor Red
            exit 1
        }

        New-Item -ItemType Directory -Path $packageDir -Force | Out-Null

        & $runUatBat BuildPlugin "-Plugin=$($pluginDescriptor.FullName)" "-Package=$packageDir" -CreateSubFolder -TargetPlatforms=Win64

        if ($LASTEXITCODE -ne 0) {
            Write-Host "Plugin build failed! Exit code: $LASTEXITCODE" -ForegroundColor Red
            exit 1
        }

        if (-not (Test-Path $packagedBinaries)) {
            Write-Host "Plugin build completed but no packaged binaries were found at $packagedBinaries" -ForegroundColor Red
            exit 1
        }

        New-Item -ItemType Directory -Path $pluginBinaries -Force | Out-Null
        Copy-Item (Join-Path $packagedBinaries "*") $pluginBinaries -Force

        if (Test-Path $packageRoot) {
            Get-ChildItem $packageRoot -Directory -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending |
                Select-Object -Skip 2 |
                ForEach-Object {
                    Remove-Item $_.FullName -Recurse -Force -ErrorAction SilentlyContinue
                }
        }

        Write-Host "Plugin build completed successfully!" -ForegroundColor Green
    } else {
        Write-Host "Building $projectName in $Mode mode..." -ForegroundColor Yellow
        
        & $buildBat "${projectName}Editor" Win64 $Mode $projectPath -waitmutex
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Build failed! Exit code: $LASTEXITCODE" -ForegroundColor Red
            exit 1
        }
        
        Write-Host "Build completed successfully!" -ForegroundColor Green
    }
} else {
    Write-Host "Skipping build..." -ForegroundColor Yellow
}

# Clear logs folder (relative to project root)
Write-Host "Clearing all files in logs folder..." -ForegroundColor Yellow
$logsPath = Join-Path $projectRoot "Saved\Logs"
if (Test-Path $logsPath) {
    $logsCleared = (Get-ChildItem $logsPath -File -ErrorAction SilentlyContinue).Count
    Get-ChildItem $logsPath -File -ErrorAction SilentlyContinue | ForEach-Object {
        Remove-Item $_.FullName -Force -ErrorAction SilentlyContinue
    }
    if ($logsCleared -gt 0) {
        Write-Host "Cleared $logsCleared file(s) from logs folder!" -ForegroundColor Green
    } else {
        Write-Host "No files found in logs folder." -ForegroundColor Gray
    }
} else {
    Write-Host "Logs folder not found." -ForegroundColor Gray
}

# Clear agent conversation files (relative to project root)
Write-Host "Clearing agent conversations..." -ForegroundColor Yellow
$agentConversationsPath = Join-Path $projectRoot "Saved\Logs\AgentConversations"
if (Test-Path $agentConversationsPath) {
    $conversationsCleared = (Get-ChildItem $agentConversationsPath -File -ErrorAction SilentlyContinue).Count
    Get-ChildItem $agentConversationsPath -File -ErrorAction SilentlyContinue | ForEach-Object {
        Remove-Item $_.FullName -Force -ErrorAction SilentlyContinue
    }
    if ($conversationsCleared -gt 0) {
        Write-Host "Cleared $conversationsCleared agent conversation file(s)!" -ForegroundColor Green
    } else {
        Write-Host "No agent conversation files found." -ForegroundColor Gray
    }
} else {
    Write-Host "AgentConversations folder not found." -ForegroundColor Gray
}

# Launch Unreal Editor
Write-Host "Launching Unreal Editor..." -ForegroundColor Yellow

Start-Process -FilePath $editorExe -ArgumentList $projectPath

Write-Host "=== Launch Complete ===" -ForegroundColor Green
Write-Host "Unreal Editor is starting with $projectName" -ForegroundColor Green
Write-Host "No debugger attached - MCP tools should work without breakpoints!" -ForegroundColor Green
