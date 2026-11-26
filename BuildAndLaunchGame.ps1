# Build and Launch an Unreal Engine project in Development Mode without Debugger
# Can live anywhere inside the project tree (e.g. Plugins/VibeUE).
# Walks UP from its own directory until it finds a .uproject file.
#
# Closing the running editor (issue #697): each editor of this project is asked over MCP whether it has unsaved
# packages; a clean editor is told to quit and exits on its own (no kill), an editor with unsaved work stops the
# script with the list (exit 5) unless -Force. Old logs are moved to Saved/Logs/Previous, not deleted.
#
# Exit codes: 0 done (ready, when waiting), 1 build or setup failure, 2 -WaitForReady timed out, 3 the editor exited
# before it was ready, 4 invalid -Map, 5 refused to close a running editor (unsaved work, or it would not quit),
# 6 -WaitForIdle timed out.

param(
    [string]$Mode = "Development",
    [switch]$Clean,
    [switch]$SkipBuild,
    # Force a full recompile of the VibeUE plugin under strict settings.
    # Strict compilation (warnings-as-errors / deprecation-as-error) is enforced
    # module-wide via VibeUE.Build.cs (bWarningsAsErrors = true), so every build is
    # strict. Incremental builds, however, only recompile changed files — so a stale
    # object file can hide a freshly-deprecated engine API. -StrictRebuild wipes the
    # plugin's Binaries/Intermediate so ALL plugin files are recompiled and rechecked.
    [switch]$StrictRebuild,
    # Block until the launched editor writes its readiness signal
    # (Saved/VibeUE/Signals/editor-<pid>-true.json, written once VibeUE's toolsets are
    # registered), so agents can chain the next MCP call without watching the file
    # themselves. Exit codes: 0 ready, 2 timed out, 3 editor exited before ready,
    # 4 invalid -Map (rejected before building; see the -Map note below).
    [switch]$WaitForReady,
    # Seconds WITHOUT A SIGN OF LIFE before -WaitForReady gives up (issue #697). The wait goes on while the editor
    # visibly works: its CPU time grows, its log grows, or its health file is refreshed. A cold start after a reboot
    # (shader and DDC caches empty) can take many minutes and is not a failure.
    [int]$ReadyTimeoutSec = 180,
    # Hard cap on the whole -WaitForReady wait, however alive the editor looks.
    [int]$ReadyMaxSec = 3600,
    # After ready, also wait until the editor is idle (issue #697): no slow task, no modal dialog, no shader jobs and
    # no assets compiling (textures, meshes, distance fields, Lumen cards), read from the health file. Implies
    # -WaitForReady. Exit 6 when -IdleTimeoutSec passes first.
    [switch]$WaitForIdle,
    [int]$IdleTimeoutSec = 3600,
    # Optional explicit engine root for non-registered/source installations.
    [string]$UnrealEnginePath = "",
    # Map to open on launch (e.g. /Game/Maps/TrainingPool). Without this the editor opens the
    # project's default map, which after a mid-task relaunch is usually the WRONG level — world
    # edits then land on the default map (issue #554). The loaded map is also published in the
    # readiness signal JSON as "currentMap" so agents can verify before editing.
    [string]$Map = "",
    # Close EVERY Unreal process on the machine before building (the pre-2026-11-17 behaviour).
    # By default only editors whose command line names this project are closed.
    [switch]$AllUnrealProcesses,
    # Close the running editor even when it has unsaved packages (they are lost) or does not answer over MCP
    # (it is killed when it does not exit).
    [switch]$Force,
    # MCP port for the launched editor: passed as -ModelContextProtocolPort=<Port>, and checked against the port the
    # editor reports. A second editor on the machine needs its own port (both default to 8000).
    [int]$Port = 0,
    # Seconds to wait for a running editor to finish a slow task before asking it to quit, and then to exit.
    [int]$QuitTimeoutSec = 120,
    # Mute the launched editor's Windows audio session(s). The engine still renders audio (in-engine recording and
    # au.DumpActiveSounds keep working); only the speakers stay silent. A hidden watcher keeps it muted for the
    # editor's lifetime. Windows remembers the mute per executable: the next launch WITHOUT -Muted through this
    # script un-mutes it again (see the README).
    [switch]$Muted,
    # Leave Saved/Logs as it is (by default its files move to Saved/Logs/Previous/<timestamp>).
    [switch]$KeepLogs,
    # How many generations of old logs to keep in Saved/Logs/Previous.
    [int]$LogGenerations = 5,
    # Internal: run as the hidden audio-session watcher for an editor process. Not for direct use.
    [int]$AudioWatchPid = 0,
    [ValidateSet("mute", "unmute")][string]$AudioWatchMode = "mute",
    [string]$AudioWatchLog = ""
)

# ============================================================================
# Windows audio sessions (Core Audio, through C#: works in Windows PowerShell 5.1 with nothing installed)
# ============================================================================
# A process's audio session is muted with ISimpleAudioVolume::SetMute on each render endpoint. That is applied after
# the engine's own mixer, so the engine keeps rendering (recording a submix, au.DumpActiveSounds still work).
$AudioSessionSource = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace VibeUELaunch
{
    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    class MMDeviceEnumeratorCom { }

    [ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMMDeviceEnumerator
    {
        [PreserveSig] int EnumAudioEndpoints(int dataFlow, int stateMask, out IMMDeviceCollection devices);
    }

    [ComImport, Guid("0BD7A1BE-7A1A-44DB-8397-CC5392387B5E"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMMDeviceCollection
    {
        [PreserveSig] int GetCount(out int count);
        [PreserveSig] int Item(int index, out IMMDevice device);
    }

    [ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IMMDevice
    {
        [PreserveSig] int Activate(ref Guid iid, int clsCtx, IntPtr activationParams, [MarshalAs(UnmanagedType.IUnknown)] out object iface);
    }

    [ComImport, Guid("77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioSessionManager2
    {
        [PreserveSig] int GetAudioSessionControl(IntPtr sessionGuid, int flags, out IntPtr control);
        [PreserveSig] int GetSimpleAudioVolume(IntPtr sessionGuid, int flags, out IntPtr volume);
        [PreserveSig] int GetSessionEnumerator(out IAudioSessionEnumerator sessions);
    }

    [ComImport, Guid("E2F5BB11-0570-40CA-ACDD-3AA01277DEE8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioSessionEnumerator
    {
        [PreserveSig] int GetCount(out int count);
        [PreserveSig] int GetSession(int index, out IAudioSessionControl2 session);
    }

    [ComImport, Guid("BFB7FF88-7239-4FC9-8FA2-07C950BE9C6D"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IAudioSessionControl2
    {
        [PreserveSig] int GetState(out int state);
        [PreserveSig] int GetDisplayName(out IntPtr name);
        [PreserveSig] int SetDisplayName(IntPtr name, IntPtr context);
        [PreserveSig] int GetIconPath(out IntPtr path);
        [PreserveSig] int SetIconPath(IntPtr path, IntPtr context);
        [PreserveSig] int GetGroupingParam(out Guid grouping);
        [PreserveSig] int SetGroupingParam(IntPtr grouping, IntPtr context);
        [PreserveSig] int RegisterAudioSessionNotification(IntPtr client);
        [PreserveSig] int UnregisterAudioSessionNotification(IntPtr client);
        [PreserveSig] int GetSessionIdentifier(out IntPtr id);
        [PreserveSig] int GetSessionInstanceIdentifier(out IntPtr id);
        [PreserveSig] int GetProcessId(out uint pid);
        [PreserveSig] int IsSystemSoundsSession();
        [PreserveSig] int SetDuckingPreference([MarshalAs(UnmanagedType.Bool)] bool optOut);
    }

    [ComImport, Guid("87CE5498-68D6-44E5-9215-6DA47EF883D8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface ISimpleAudioVolume
    {
        [PreserveSig] int SetMasterVolume(float level, ref Guid context);
        [PreserveSig] int GetMasterVolume(out float level);
        [PreserveSig] int SetMute([MarshalAs(UnmanagedType.Bool)] bool mute, ref Guid context);
        [PreserveSig] int GetMute([MarshalAs(UnmanagedType.Bool)] out bool mute);
    }

    public static class AudioSessions
    {
        // For every session of process `pid` on every active render endpoint: when `apply` is true, set the mute
        // state to `mute`; always read it back. Returns one entry per session, "<device index>:<muted true|false>"
        // (empty: the process has no audio session yet).
        public static string[] MuteProcess(uint pid, bool mute, bool apply)
        {
            List<string> result = new List<string>();
            IMMDeviceEnumerator devices = (IMMDeviceEnumerator)(new MMDeviceEnumeratorCom());
            IMMDeviceCollection collection = null;
            try
            {
                // eRender = 0, DEVICE_STATE_ACTIVE = 1
                if (devices.EnumAudioEndpoints(0, 1, out collection) != 0 || collection == null) { return result.ToArray(); }
                int deviceCount;
                collection.GetCount(out deviceCount);
                for (int d = 0; d < deviceCount; d++)
                {
                    IMMDevice device = null;
                    object managerObject = null;
                    IAudioSessionEnumerator sessions = null;
                    try
                    {
                        if (collection.Item(d, out device) != 0 || device == null) { continue; }
                        Guid iid = typeof(IAudioSessionManager2).GUID;
                        // CLSCTX_ALL = 23
                        if (device.Activate(ref iid, 23, IntPtr.Zero, out managerObject) != 0 || managerObject == null) { continue; }
                        IAudioSessionManager2 manager = (IAudioSessionManager2)managerObject;
                        if (manager.GetSessionEnumerator(out sessions) != 0 || sessions == null) { continue; }
                        int sessionCount;
                        sessions.GetCount(out sessionCount);
                        for (int s = 0; s < sessionCount; s++)
                        {
                            IAudioSessionControl2 session = null;
                            try
                            {
                                if (sessions.GetSession(s, out session) != 0 || session == null) { continue; }
                                uint sessionPid;
                                if (session.GetProcessId(out sessionPid) != 0 || sessionPid != pid) { continue; }
                                ISimpleAudioVolume volume = (ISimpleAudioVolume)session;
                                bool current;
                                volume.GetMute(out current);
                                if (apply && current != mute)
                                {
                                    Guid context = Guid.Empty;
                                    volume.SetMute(mute, ref context);
                                    volume.GetMute(out current);
                                }
                                result.Add(d + ":" + (current ? "true" : "false"));
                            }
                            finally
                            {
                                if (session != null) { Marshal.FinalReleaseComObject(session); }
                            }
                        }
                    }
                    finally
                    {
                        if (sessions != null) { Marshal.FinalReleaseComObject(sessions); }
                        if (managerObject != null) { Marshal.FinalReleaseComObject(managerObject); }
                        if (device != null) { Marshal.FinalReleaseComObject(device); }
                    }
                }
            }
            finally
            {
                if (collection != null) { Marshal.FinalReleaseComObject(collection); }
                Marshal.FinalReleaseComObject(devices);
            }
            return result.ToArray();
        }
    }
}
'@

# Windows remembers an audio session's mute per executable, so an editor muted by -Muted makes the next editor started
# from the same UnrealEditor.exe (any project, the launcher too) start muted. This file lists the executables this
# script muted; a later launch without -Muted un-mutes the new editor's session and removes the entry.
$MutedExeListPath = Join-Path $env:LOCALAPPDATA "VibeUE\muted-editor-executables.txt"

function Update-MutedExeList([string]$ExePath, [bool]$Add) {
    $dir = Split-Path $MutedExeListPath -Parent
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $entries = @()
    if (Test-Path $MutedExeListPath) { $entries = @(Get-Content -LiteralPath $MutedExeListPath | Where-Object { $_ -and $_ -ne $ExePath }) }
    if ($Add) { $entries += $ExePath }
    if ($entries.Count -gt 0) { Set-Content -LiteralPath $MutedExeListPath -Value $entries -Encoding UTF8 }
    elseif (Test-Path $MutedExeListPath) { Remove-Item -LiteralPath $MutedExeListPath -Force -ErrorAction SilentlyContinue }
}

# The hidden watcher: this same script, started with -AudioWatchPid. "mute" keeps every session of that process muted
# until it exits (a session appears once the engine opens its audio device, a few seconds into startup, and a new one
# after an audio device change). "unmute" waits for the first session, un-mutes it once and exits.
if ($AudioWatchPid -gt 0) {
    (Get-Process -Id $PID).PriorityClass = 'Idle'
    function Write-WatchLog([string]$Text) {
        if ($AudioWatchLog) { Add-Content -LiteralPath $AudioWatchLog -Value ("{0} {1}" -f (Get-Date -Format s), $Text) }
    }
    $env:LIB = $null   # a stale LIB (Visual Studio developer shells) makes Add-Type's compiler fail
    try { Add-Type -TypeDefinition $AudioSessionSource -ErrorAction Stop }
    catch { Write-WatchLog "audio watcher: could not compile the Core Audio helper: $($_.Exception.Message)"; exit 1 }

    $target = Get-Process -Id $AudioWatchPid -ErrorAction SilentlyContinue
    if (-not $target) { Write-WatchLog "audio watcher: process $AudioWatchPid is not running"; exit 1 }
    $exePath = $null
    try { $exePath = $target.MainModule.FileName } catch {}
    Write-WatchLog "audio watcher ($AudioWatchMode) started for pid $AudioWatchPid ($exePath)"
    $wantMute = ($AudioWatchMode -eq "mute")
    $deadline = (Get-Date).AddMinutes(15)
    $lastReport = ""
    $seenFirst = $false
    while (-not $target.HasExited) {
        try {
            if (-not $seenFirst) {
                # The state Windows gave the new session (its remembered state for this executable), before any change.
                $before = @([VibeUELaunch.AudioSessions]::MuteProcess([uint32]$AudioWatchPid, $wantMute, $false))
                if ($before.Count -gt 0) {
                    Write-WatchLog "pid $AudioWatchPid first session(s) as Windows created them (device:muted) $($before -join ',')"
                    $seenFirst = $true
                }
            }
            $states = @([VibeUELaunch.AudioSessions]::MuteProcess([uint32]$AudioWatchPid, $wantMute, $true))
            $report = ($states -join ",")
            if ($states.Count -gt 0 -and $report -ne $lastReport) {
                Write-WatchLog "pid $AudioWatchPid sessions (device:muted) $report"
                $lastReport = $report
            }
            if (-not $wantMute -and $states.Count -gt 0) {
                if ($exePath) { Update-MutedExeList $exePath $false }
                Write-WatchLog "audio watcher: un-muted; done"
                exit 0
            }
        } catch {
            Write-WatchLog "audio watcher: $($_.Exception.Message)"
        }
        if (-not $wantMute -and (Get-Date) -gt $deadline) {
            Write-WatchLog "audio watcher: no audio session appeared within 15 minutes; giving up"
            exit 1
        }
        Start-Sleep -Milliseconds 1000
        $target.Refresh()
    }
    Write-WatchLog "audio watcher: pid $AudioWatchPid exited"
    exit 0
}

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

if ($WaitForIdle) { $WaitForReady = $true }
if ($Port -lt 0 -or $Port -gt 65535) {
    Write-Host "ERROR: -Port $Port is not a valid TCP port." -ForegroundColor Red
    exit 1
}

# ============================================================================
# Validate -Map BEFORE building (issue #554 follow-up)
# ============================================================================
# A mount-point path like /Game/Maps/L_Foo is what the editor expects. When this script is invoked
# from Git Bash, MSYS path conversion rewrites a leading-slash argument into a Windows path
# (e.g. /Game/Maps/L_Foo -> C:/Program Files/Git/Game/Maps/L_Foo), and the editor then silently
# opens the WRONG level. Reject anything that is not a clean mount-point path (must start with '/',
# must not contain ':' or '\') up front, so the mistake fails loudly instead of after a full build.
if ($Map) {
    if ($Map -notmatch '^/' -or $Map -match '[:\\]') {
        Write-Host "ERROR: -Map '$Map' is not a valid Unreal package path." -ForegroundColor Red
        Write-Host "       Expected a mount-point path such as /Game/Maps/L_Foo or /Engine/Maps/Foo" -ForegroundColor Red
        Write-Host "       (starts with '/', no ':' or '\\')." -ForegroundColor Red
        Write-Host "       Likely cause: MSYS path conversion when run from Git Bash rewrote the leading" -ForegroundColor Red
        Write-Host "       slash into a Windows path. Prefix the command with MSYS_NO_PATHCONV=1, or run" -ForegroundColor Red
        Write-Host "       this script from PowerShell instead." -ForegroundColor Red
        exit 4
    }
}

# Resolve the editor target from the C# target declaration. The .uproject file
# name is not required to match the module/target name (for example this
# project is ue-gas-learn but its target is GAS_LearnEditor). A Blueprint-only project has no
# target of its own: its plugins build through the engine's UnrealEditor target with -Project=
# (the same rule as BuildAndLaunchGame.sh).
$editorTarget = $null
$targetArgs = @($projectPath)
$targetFiles = @(Get-ChildItem -Path (Join-Path $projectRoot "Source") -Filter "*Editor.Target.cs" -File -ErrorAction SilentlyContinue)
foreach ($targetFile in $targetFiles) {
    $targetText = Get-Content -LiteralPath $targetFile.FullName -Raw
    $targetMatch = [regex]::Match($targetText, 'class\s+([A-Za-z0-9_]+)EditorTarget\s*:\s*TargetRules')
    if ($targetMatch.Success) {
        $editorTarget = "{0}Editor" -f $targetMatch.Groups[1].Value
        break
    }
}
if (-not $editorTarget) {
    if ($targetFiles.Count -eq 0) {
        $editorTarget = "UnrealEditor"
        $targetArgs = @("-Project=$projectPath")
    } else {
        $editorTarget = "${projectName}Editor"
    }
}

# ============================================================================
# Auto-discover Unreal Engine install from EngineAssociation in .uproject
# ============================================================================
$uprojectJson = Get-Content $projectPath -Raw | ConvertFrom-Json
$engineAssociation = $uprojectJson.EngineAssociation

# First try: look up custom/source builds from registry (HKCU)
$enginePath = $null
if ($UnrealEnginePath) {
    $candidateEnginePath = $UnrealEnginePath.Trim().Trim('"')
    if (Test-Path (Join-Path $candidateEnginePath "Engine\Build\BatchFiles\Build.bat")) {
        $enginePath = $candidateEnginePath
    } else {
        Write-Host "ERROR: Invalid Unreal engine path: $candidateEnginePath" -ForegroundColor Red
        exit 1
    }
}
try {
    $customBuilds = Get-ItemProperty "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue
    if (-not $enginePath -and $customBuilds -and $customBuilds.$engineAssociation) {
        $enginePath = $customBuilds.$engineAssociation
    }
} catch {}

# Second try: the Epic launcher's own install list, which is what the editor resolves
# EngineAssociation from. The HKLM key below can go stale: after 5.8.4 was installed to a new
# folder it still named the old 5.8.3 install, so the script built against the wrong engine.
if (-not $enginePath) {
    try {
        $launcherDat = Join-Path $env:ProgramData "Epic\UnrealEngineLauncher\LauncherInstalled.dat"
        $launcherInstall = (Get-Content $launcherDat -Raw | ConvertFrom-Json).InstallationList |
            Where-Object { $_.AppName -eq "UE_$engineAssociation" } | Select-Object -First 1
        if ($launcherInstall -and (Test-Path (Join-Path $launcherInstall.InstallLocation "Engine\Build\BatchFiles\Build.bat"))) {
            $enginePath = $launcherInstall.InstallLocation
        }
    } catch {}
}

# Third try: standard launcher installs (HKLM)
if (-not $enginePath) {
    try {
        $regPath = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$engineAssociation"
        $regEntry = Get-ItemProperty $regPath -ErrorAction SilentlyContinue
        if ($regEntry -and $regEntry.InstalledDirectory) {
            $enginePath = $regEntry.InstalledDirectory
        }
    } catch {}
}

# Fourth try: well-known path pattern (e.g. UE_5.7)
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

# Fifth try: ask for a manual path rather than just giving up (the Epic launcher
# doesn't always register a drive, e.g. an engine installed on a non-default drive).
if (-not $enginePath) {
    Write-Host "Could not auto-detect Unreal Engine '$engineAssociation' from the Epic launcher, the registry or common install paths." -ForegroundColor Yellow
    # Non-interactive session (CI, agent-run builds): fail fast instead of prompting.
    if ([Console]::IsInputRedirected -or ([Environment]::GetCommandLineArgs() -match '^-NonI')) {
        Write-Host "ERROR: Non-interactive session - cannot prompt for a path. Set the engine path manually or register the install." -ForegroundColor Red
        exit 1
    }
    while (-not $enginePath) {
        $manualPath = Read-Host "Enter the full path to your Unreal Engine install (e.g. D:\Program Files\Epic Games\UE_$engineAssociation), or leave blank to abort"
        if ([string]::IsNullOrWhiteSpace($manualPath)) {
            Write-Host "ERROR: No Unreal Engine path provided. Aborting." -ForegroundColor Red
            exit 1
        }
        $manualPath = $manualPath.Trim().Trim('"')
        if (Test-Path (Join-Path $manualPath "Engine\Build\BatchFiles\Build.bat")) {
            $enginePath = $manualPath
        } else {
            Write-Host "That path doesn't look like an Unreal Engine install (expected Engine\Build\BatchFiles\Build.bat under it)." -ForegroundColor Red
        }
    }
}

$buildBat  = Join-Path $enginePath "Engine\Build\BatchFiles\Build.bat"
$editorExe = Join-Path $enginePath "Engine\Binaries\Win64\UnrealEditor.exe"
$buildManifestPath = Join-Path $projectRoot "Saved\VibeUE\last-build.json"
$quotedTargetArgs = ($targetArgs | ForEach-Object { '"' + $_ + '"' }) -join ' '

function Write-BuildManifest([string]$Status, [Nullable[int]]$ExitCode, [string]$Diagnostic = "") {
    $manifestDir = Split-Path $buildManifestPath -Parent
    New-Item -ItemType Directory -Path $manifestDir -Force | Out-Null
    $payload = [ordered]@{
        schema = "vibeue.build.v1"
        status = $Status
        projectFile = [IO.Path]::GetFullPath($projectPath)
        engineRoot = [IO.Path]::GetFullPath($enginePath)
        target = $editorTarget
        platform = "Win64"
        configuration = $Mode
        command = "Build.bat $editorTarget Win64 $Mode $quotedTargetArgs -waitmutex"
        completedAtIso = if ($Status -in @("succeeded", "failed", "skipped")) { [DateTime]::UtcNow.ToString("o") } else { $null }
        exitCode = $ExitCode
        verdict = $Status
        logPath = Join-Path $env:LOCALAPPDATA "UnrealBuildTool\Log.txt"
        diagnostic = $Diagnostic
    }
    $temp = "$buildManifestPath.tmp"
    $payload | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $temp -Encoding UTF8
    Move-Item -LiteralPath $temp -Destination $buildManifestPath -Force
}

Write-Host "=== $projectName Build and Launch Script ===" -ForegroundColor Cyan
Write-Host "Script  : $PSScriptRoot" -ForegroundColor Gray
Write-Host "Project : $projectPath" -ForegroundColor Yellow
Write-Host "Engine  : $enginePath" -ForegroundColor Yellow
Write-Host "Mode    : $Mode" -ForegroundColor Yellow

# ============================================================================
# Signals and MCP (issue #697)
# ============================================================================
function Read-JsonFile([string]$Path) {
    try { return Get-Content -LiteralPath $Path -Raw -ErrorAction Stop | ConvertFrom-Json } catch { return $null }
}

# The health file of an editor process if it was refreshed in the last $MaxAgeSec seconds (it is rewritten every ~5 s).
function Get-FreshHealth([string]$SignalsDir, [int]$ProcessId, [int]$MaxAgeSec = 20) {
    $path = Join-Path $SignalsDir "editor-$ProcessId-health.json"
    $health = Read-JsonFile $path
    if (-not $health) { return $null }
    try {
        $updated = [DateTime]::Parse($health.updatedUtc, [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::AdjustToUniversal -bor [Globalization.DateTimeStyles]::AssumeUniversal)
        if (([DateTime]::UtcNow - $updated).TotalSeconds -gt $MaxAgeSec) { return $null }
    } catch { return $null }
    return $health
}

# A minimal MCP client (JSON-RPC over HTTP, as Epic's server speaks it): initialize, then tools/call. Answers can be
# plain JSON or an SSE stream. No proxy: the endpoint is always on this machine.
function Invoke-McpPost([int]$McpPort, [hashtable]$Body, [string]$SessionId, [int]$TimeoutSec) {
    $request = [System.Net.HttpWebRequest]::Create("http://127.0.0.1:$McpPort/mcp")
    $request.Method = "POST"
    $request.Proxy = $null
    $request.Timeout = $TimeoutSec * 1000
    $request.ReadWriteTimeout = $TimeoutSec * 1000
    $request.ContentType = "application/json"
    $request.Accept = "application/json, text/event-stream"
    $request.Headers.Add("MCP-Protocol-Version", "2025-06-18")
    if ($SessionId) { $request.Headers.Add("Mcp-Session-Id", $SessionId) }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($Body | ConvertTo-Json -Depth 12 -Compress))
    $request.ContentLength = $bytes.Length
    $stream = $request.GetRequestStream()
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.Close()
    $response = $request.GetResponse()
    try {
        $reader = New-Object IO.StreamReader($response.GetResponseStream(), [Text.Encoding]::UTF8)
        $text = $reader.ReadToEnd()
        return @{ Session = $response.Headers["Mcp-Session-Id"]; Text = $text; ContentType = $response.ContentType }
    } finally { $response.Close() }
}

# Runs one Python script in the editor on $McpPort through execute_python and returns the raw text of the answer.
function Invoke-EditorPython([int]$McpPort, [string]$Script, [int]$TimeoutSec = 60) {
    $init = Invoke-McpPost $McpPort @{ jsonrpc = "2.0"; id = 1; method = "initialize"; params = @{
            protocolVersion = "2025-06-18"; capabilities = @{}; clientInfo = @{ name = "BuildAndLaunchGame"; version = "1.0" } } } "" 15
    $session = $init.Session
    try { Invoke-McpPost $McpPort @{ jsonrpc = "2.0"; method = "notifications/initialized" } $session 15 | Out-Null } catch {}
    $call = Invoke-McpPost $McpPort @{ jsonrpc = "2.0"; id = 2; method = "tools/call"; params = @{
            name = "call_tool"; arguments = @{
                toolset_name = "editor_toolset.toolsets.python_scripting.PythonScriptingTools"
                tool_name = "execute_python"
                arguments = @{ script = @{ language = "Python"; content = $Script } } } } } $session $TimeoutSec
    return $call.Text
}

# Asks editor process $ProcessId (on $McpPort) for its unsaved packages and, when there are none (or -Force), tells it
# to quit. The script checks its own pid first, so a wrong port can never close another editor. The quit is deferred
# by half a second so the MCP answer leaves before the engine loop ends. The answer is a base64url JSON payload
# between markers, so it survives however the server wraps the text.
function Request-EditorQuit([int]$McpPort, [int]$ProcessId, [bool]$QuitEvenIfDirty) {
    $forceText = if ($QuitEvenIfDirty) { "True" } else { "False" }
    $script = @"
import base64
import json
import os
import time
expected_pid = $ProcessId
quit_even_if_dirty = $forceText
pid = os.getpid()
try:
    dirty = [str(p) for p in unreal.PythonTools.get_dirty_packages()]
except AttributeError:
    utils = unreal.EditorLoadingAndSavingUtils
    dirty = sorted(set(p.get_name() for p in list(utils.get_dirty_content_packages()) + list(utils.get_dirty_map_packages())))
quitting = pid == expected_pid and (not dirty or quit_even_if_dirty)
if quitting:
    started = time.monotonic()
    def _vibeue_deferred_quit(delta_seconds):
        if time.monotonic() - started >= 0.5 and not getattr(_vibeue_deferred_quit, "done", False):
            _vibeue_deferred_quit.done = True
            unreal.SystemLibrary.quit_editor()
    unreal.register_slate_post_tick_callback(_vibeue_deferred_quit)
payload = {"pid": pid, "project": unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path()), "dirty": dirty, "quitting": quitting}
"VIBEUE697:" + base64.urlsafe_b64encode(json.dumps(payload).encode("utf-8")).decode("ascii") + ":END"
"@
    $text = Invoke-EditorPython $McpPort $script 60
    $match = [regex]::Match($text, 'VIBEUE697:([A-Za-z0-9_\-=]+):END')
    if (-not $match.Success) {
        $snippet = if ($text.Length -gt 400) { $text.Substring(0, 400) + "..." } else { $text }
        throw "unexpected answer from execute_python: $snippet"
    }
    $b64 = $match.Groups[1].Value.Replace('-', '+').Replace('_', '/')
    switch ($b64.Length % 4) { 2 { $b64 += "==" } 3 { $b64 += "=" } }
    return ([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($b64)) | ConvertFrom-Json)
}

# The MCP port an editor process serves: its fresh health file says so (and whether it listens), otherwise its
# command line, otherwise -Port, otherwise its project's config, otherwise the engine default 8000. A wrong guess is
# harmless: the Python checks the pid before it does anything.
function Get-EditorMcpPort([int]$ProcessId, [string]$CommandLine, [string]$EditorProjectPath) {
    if ($EditorProjectPath) {
        $health = Get-FreshHealth (Join-Path (Split-Path $EditorProjectPath -Parent) "Saved\VibeUE\Signals") $ProcessId
        if ($health -and $health.mcpPort -gt 0) {
            return @{ Port = [int]$health.mcpPort; Listening = [bool]$health.mcpListening; Source = "health file"; Health = $health }
        }
    }
    $m = [regex]::Match([string]$CommandLine, '-ModelContextProtocolPort=(\d+)', 'IgnoreCase')
    if ($m.Success) { return @{ Port = [int]$m.Groups[1].Value; Listening = $true; Source = "command line"; Health = $null } }
    if ($Port -gt 0) { return @{ Port = $Port; Listening = $true; Source = "-Port"; Health = $null } }
    if ($EditorProjectPath) {
        $dir = Split-Path $EditorProjectPath -Parent
        foreach ($ini in @("Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini", "Config\DefaultEditorPerProjectUserSettings.ini")) {
            $iniPath = Join-Path $dir $ini
            if (Test-Path $iniPath) {
                $line = Select-String -LiteralPath $iniPath -Pattern '^\s*ServerPortNumber\s*=\s*(\d+)' | Select-Object -Last 1
                if ($line) { return @{ Port = [int]$line.Matches[0].Groups[1].Value; Listening = $true; Source = $ini; Health = $null } }
            }
        }
    }
    return @{ Port = 8000; Listening = $true; Source = "default"; Health = $null }
}

# Stop this project's Unreal Engine processes. Editors of OTHER projects (another agent's or the user's session on
# the same machine) are left alone: only UnrealEditor* processes whose command line names this .uproject are closed,
# plus the shared helpers (CrashReportClient, UnrealTraceServer, Epic crashpad) when no foreign editor is left.
# -AllUnrealProcesses restores the old close-everything behaviour.
function Get-ProcessCommandLine([int]$ProcessId) {
    try { return (Get-CimInstance Win32_Process -Filter "ProcessId = $ProcessId" -ErrorAction Stop).CommandLine } catch { return $null }
}
function Test-OwnEditor($Process) {
    if ($AllUnrealProcesses) { return $true }
    $cmd = Get-ProcessCommandLine $Process.Id
    if (-not $cmd) { return $false }
    return $cmd.IndexOf($projectPath, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
           $cmd.IndexOf($projectPath.Replace('\', '/'), [StringComparison]::OrdinalIgnoreCase) -ge 0
}
function Get-CommandLineProject([string]$CommandLine) {
    $m = [regex]::Match([string]$CommandLine, '([A-Za-z]:[\\/][^"]*?\.uproject)', 'IgnoreCase')
    if ($m.Success) { return $m.Groups[1].Value.Replace('/', '\') }
    return $null
}
function Wait-ProcessExit($Process, [int]$Seconds) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while (-not $Process.HasExited -and $sw.Elapsed.TotalSeconds -lt $Seconds) { Start-Sleep -Milliseconds 500 }
    return $Process.HasExited
}
function Stop-ProcessTree($Process) {
    Write-Host "  Killing $($Process.ProcessName) (PID: $($Process.Id)) and child processes..." -ForegroundColor Gray
    taskkill /F /PID $Process.Id /T 2>$null | Out-Null
    if (-not (Wait-ProcessExit $Process 5)) { taskkill /F /PID $Process.Id /T 2>$null | Out-Null; Wait-ProcessExit $Process 3 | Out-Null }
}

# Closes one editor process. Returns $true when it is gone, $false when the script must refuse (the reason printed).
function Close-Editor($Process) {
    $cmd = Get-ProcessCommandLine $Process.Id
    $editorProject = Get-CommandLineProject $cmd
    if (-not $editorProject -and (Test-OwnEditor $Process)) { $editorProject = $projectPath }
    Write-Host "Closing $($Process.ProcessName) (PID: $($Process.Id)) of $editorProject" -ForegroundColor Yellow
    $signalsDirOfEditor = if ($editorProject) { Join-Path (Split-Path $editorProject -Parent) "Saved\VibeUE\Signals" } else { $null }

    # Busy editors refuse MCP calls: wait for a slow task or a modal dialog to end first.
    if ($signalsDirOfEditor) {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($true) {
            $health = Get-FreshHealth $signalsDirOfEditor $Process.Id
            if (-not $health -or -not ($health.slowTask -or $health.modal) -or $Process.HasExited) { break }
            if ($sw.Elapsed.TotalSeconds -ge $QuitTimeoutSec) { break }
            $what = if ($health.slowTask) { "a slow task ('$($health.slowTaskText)')" } else { "a modal dialog ('$($health.modalTitle)')" }
            Write-Host "  It is running $what; waiting ($([int]$sw.Elapsed.TotalSeconds)/$QuitTimeoutSec s)..." -ForegroundColor Gray
            Start-Sleep 5
        }
    }
    if ($Process.HasExited) { return $true }

    $endpoint = Get-EditorMcpPort $Process.Id $cmd $editorProject
    $answer = $null
    $mcpError = $null
    if ($endpoint.Listening) {
        try { $answer = Request-EditorQuit $endpoint.Port $Process.Id ([bool]$Force) }
        catch { $mcpError = $_.Exception.Message }
    } else {
        $mcpError = "its health file says MCP is not listening on port $($endpoint.Port)"
    }

    if ($answer -and [int]$answer.pid -ne $Process.Id) {
        # Port $($endpoint.Port) belongs to another editor: never act on that answer.
        $mcpError = "port $($endpoint.Port) ($($endpoint.Source)) is served by PID $($answer.pid) ($($answer.project)), not by PID $($Process.Id)"
        $answer = $null
    }

    if ($answer) {
        $dirty = @($answer.dirty)
        if (-not $answer.quitting) {
            Write-Host "REFUSED: $($Process.ProcessName) (PID: $($Process.Id)) has $($dirty.Count) unsaved package(s):" -ForegroundColor Red
            $dirty | ForEach-Object { Write-Host "    $_" -ForegroundColor Red }
            Write-Host "  Save or discard them in the editor (or from Python), then run this script again." -ForegroundColor Red
            Write-Host "  -Force closes it anyway and loses those changes." -ForegroundColor Red
            return $false
        }
        if ($dirty.Count -gt 0) {
            Write-Host "  -Force: discarding $($dirty.Count) unsaved package(s): $($dirty -join ', ')" -ForegroundColor Yellow
        }
        $unsavedNote = if ($dirty.Count -eq 0) { "nothing unsaved" } else { "unsaved work discarded" }
        Write-Host "  Asked it to quit over MCP (port $($endpoint.Port), from the $($endpoint.Source)); $unsavedNote. Waiting for it to exit..." -ForegroundColor Gray
        $sw = [Diagnostics.Stopwatch]::StartNew()
        if (Wait-ProcessExit $Process $QuitTimeoutSec) {
            Write-Host "  PID $($Process.Id) exited by itself after $([math]::Round($sw.Elapsed.TotalSeconds, 1)) s: no force kill." -ForegroundColor Green
            return $true
        }
        Write-Host "  It accepted the quit but is still running after $QuitTimeoutSec s; it is shutting down with nothing to save, killing it." -ForegroundColor Yellow
        Stop-ProcessTree $Process
        return $Process.HasExited
    }

    # No MCP answer: fall back to closing the main window, which lets the editor ask about unsaved work itself.
    Write-Host "  No answer over MCP ($mcpError)." -ForegroundColor Yellow
    if ($Process.MainWindowHandle -ne 0) {
        Write-Host "  Closing its main window instead (the editor asks about unsaved changes itself)..." -ForegroundColor Gray
        $Process.CloseMainWindow() | Out-Null
        if (Wait-ProcessExit $Process 15) {
            Write-Host "  PID $($Process.Id) closed gracefully." -ForegroundColor Green
            return $true
        }
    }
    if ($Force) {
        Write-Host "  -Force: it did not exit, killing it." -ForegroundColor Yellow
        Stop-ProcessTree $Process
        return $Process.HasExited
    }
    Write-Host "REFUSED: $($Process.ProcessName) (PID: $($Process.Id)) is still running and could not be asked whether it has unsaved work." -ForegroundColor Red
    Write-Host "  It may be showing a save prompt, be busy, or be wedged. Close it yourself, or pass -Force to kill it." -ForegroundColor Red
    Write-Host "  Command line: $cmd" -ForegroundColor Gray
    return $false
}

$allEditors = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -like "UnrealEditor*" })
$ownEditors = @($allEditors | Where-Object { Test-OwnEditor $_ })
$foreignEditors = @($allEditors | Where-Object { $ownEditors.Id -notcontains $_.Id })
foreach ($f in $foreignEditors) {
    Write-Host "Leaving $($f.ProcessName) (PID: $($f.Id)) running: it is not editing $projectName" -ForegroundColor Gray
}

if ($ownEditors.Count -gt 0) {
    foreach ($editor in $ownEditors) {
        if ($editor.HasExited) { continue }
        if (-not (Close-Editor $editor)) { exit 5 }
    }
    Write-Host "All of $projectName's editors are closed." -ForegroundColor Green
} else {
    Write-Host "No running Unreal Engine processes of $projectName found." -ForegroundColor Gray
}

# Shared helpers have no unsaved state: closed (then killed) as before, and only when no other project's editor uses them.
if ($foreignEditors.Count -eq 0) {
    $helpers = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -like "CrashReportClient*" -or
        $_.ProcessName -eq "UnrealTraceServer" -or
        ($_.ProcessName -eq "crashpad_handler" -and $_.MainModule.FileName -like "*Epic Games*")
    })
    foreach ($h in $helpers) {
        if ($h.MainWindowHandle -ne 0) { $h.CloseMainWindow() | Out-Null }
        if (-not (Wait-ProcessExit $h 5)) { Stop-ProcessTree $h }
    }
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

# Strict rebuild: wipe the VibeUE plugin's build artifacts so every plugin file is
# recompiled under the module's warnings-as-errors setting (catches newly-deprecated APIs).
if ($StrictRebuild -and -not $Clean) {
    Write-Host "Strict rebuild: cleaning VibeUE plugin Binaries/Intermediate..." -ForegroundColor Yellow
    foreach ($sub in @("Binaries","Intermediate")) {
        $subPath = Join-Path $PSScriptRoot $sub
        if (Test-Path $subPath) { Remove-Item $subPath -Recurse -Force }
    }
}

# Build the project
if (-not $SkipBuild) {
    Write-Host "Building $projectName in $Mode mode (strict: warnings-as-errors via VibeUE.Build.cs)..." -ForegroundColor Yellow
    Write-BuildManifest "running" $null

    # UBT refuses to build while ANY editor of this engine install has a Live Coding session (the mutex is
    # keyed on the engine's UnrealEditor.exe). This project's editors are closed by now, so a remaining
    # session belongs to another project and cannot patch our modules: skip that check.
    $ubtExtra = @()
    if ($foreignEditors.Count -gt 0) {
        Write-Host "Another project's editor is running: building with -NoHotReloadFromIDE" -ForegroundColor Gray
        $ubtExtra += "-NoHotReloadFromIDE"
    }
    & $buildBat $editorTarget Win64 $Mode @targetArgs -waitmutex @ubtExtra

    if ($LASTEXITCODE -ne 0) {
        Write-BuildManifest "failed" $LASTEXITCODE "UnrealBuildTool returned a non-zero exit code."
        Write-Host "Build failed! Exit code: $LASTEXITCODE" -ForegroundColor Red
        exit 1
    }
    Write-BuildManifest "succeeded" 0
    Write-Host "Build completed successfully!" -ForegroundColor Green
} else {
    Write-BuildManifest "skipped" $null "Build was skipped by caller; this is not compile verification."
    Write-Host "Skipping build..." -ForegroundColor Yellow
}

# ============================================================================
# Old logs: moved to Saved/Logs/Previous/<timestamp> (issue #697), never deleted, so the log of a session that just
# crashed survives the relaunch. The newest -LogGenerations folders are kept. Agent conversations go with them.
# ============================================================================
$logsPath = Join-Path $projectRoot "Saved\Logs"
if ($KeepLogs) {
    Write-Host "-KeepLogs: leaving Saved\Logs as it is." -ForegroundColor Gray
} elseif (Test-Path $logsPath) {
    $previousRoot = Join-Path $logsPath "Previous"
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $generation = Join-Path $previousRoot $stamp
    $toMove = @(Get-ChildItem $logsPath -File -ErrorAction SilentlyContinue)
    $conversationsPath = Join-Path $logsPath "AgentConversations"
    $conversations = @(Get-ChildItem $conversationsPath -File -ErrorAction SilentlyContinue)
    if ($toMove.Count + $conversations.Count -gt 0) {
        New-Item -ItemType Directory -Path $generation -Force | Out-Null
        $moved = 0
        $locked = @()
        foreach ($file in $toMove) {
            try { Move-Item -LiteralPath $file.FullName -Destination $generation -Force -ErrorAction Stop; $moved++ }
            catch { $locked += $file.Name }
        }
        if ($conversations.Count -gt 0) {
            $conversationsTarget = Join-Path $generation "AgentConversations"
            New-Item -ItemType Directory -Path $conversationsTarget -Force | Out-Null
            foreach ($file in $conversations) {
                try { Move-Item -LiteralPath $file.FullName -Destination $conversationsTarget -Force -ErrorAction Stop; $moved++ }
                catch { $locked += "AgentConversations\$($file.Name)" }
            }
        }
        Write-Host "Moved $moved log file(s) to $generation" -ForegroundColor Green
        if ($locked.Count -gt 0) {
            Write-Host "  Left in place (in use by another process): $($locked -join ', ')" -ForegroundColor Yellow
        }
    } else {
        Write-Host "No files found in logs folder." -ForegroundColor Gray
    }
    $generations = @(Get-ChildItem $previousRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)
    if ($LogGenerations -ge 0 -and $generations.Count -gt $LogGenerations) {
        $generations | Select-Object -Skip $LogGenerations | ForEach-Object {
            Remove-Item $_.FullName -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "  Removed the oldest log generation $($_.Name) (keeping $LogGenerations)" -ForegroundColor Gray
        }
    }
} else {
    Write-Host "Logs folder not found." -ForegroundColor Gray
}

# Launch Unreal Editor
Write-Host "Launching Unreal Editor..." -ForegroundColor Yellow

# The project path MUST be quoted: -ArgumentList passes the string to the child process
# verbatim, so a path with a space (e.g. Documents\Unreal Projects, Unreal's default
# location) arrives as two invalid arguments and the editor silently opens the last
# project or the Project Browser instead (issue #532).
$editorArgs = "`"$projectPath`""
if ($Map) {
    $editorArgs += " `"$Map`""
    Write-Host "Opening map: $Map" -ForegroundColor Yellow
}
if ($Port -gt 0) {
    $editorArgs += " -ModelContextProtocolPort=$Port"
    Write-Host "MCP port: $Port" -ForegroundColor Yellow
}
$launchedUtc = [DateTime]::UtcNow
$editorProcess = Start-Process -FilePath $editorExe -ArgumentList $editorArgs -PassThru

# Windows recycles process IDs, so an Editor that crashed without running OnPreExit can leave a signal
# file whose name matches the PID we just got. VibeUE also clears it in RegisterToolsets(), but that runs
# late in startup - an agent watching from now would see the stale file first and call MCP too early.
# The Editor takes tens of seconds to reach RegisterToolsets(), so deleting here cannot race its write.
$signalsDir = Join-Path $projectRoot "Saved\VibeUE\Signals"
if (Test-Path $signalsDir) {
    Get-ChildItem -Path $signalsDir -Filter "editor-$($editorProcess.Id)-*.json*" -File -ErrorAction SilentlyContinue |
        ForEach-Object {
            Remove-Item $_.FullName -Force -ErrorAction SilentlyContinue
            Write-Host "Cleared stale readiness signal: $($_.Name)" -ForegroundColor Gray
        }
}

Write-Output "Editor-PID=$($editorProcess.Id)"

# Audio: a hidden watcher mutes the new editor's session(s) (-Muted), or un-mutes it when an earlier -Muted run left
# this executable's remembered state muted.
$audioMode = $null
if ($Muted) {
    $audioMode = "mute"
    Update-MutedExeList $editorExe $true
} elseif ((Test-Path $MutedExeListPath) -and (@(Get-Content -LiteralPath $MutedExeListPath) -contains $editorExe)) {
    $audioMode = "unmute"
}
if ($audioMode) {
    $audioLog = Join-Path $projectRoot "Saved\VibeUE\audio-session-$($editorProcess.Id).log"
    New-Item -ItemType Directory -Path (Split-Path $audioLog -Parent) -Force | Out-Null
    $watchArgs = "-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$PSCommandPath`" -AudioWatchPid $($editorProcess.Id) -AudioWatchMode $audioMode -AudioWatchLog `"$audioLog`""
    $watcher = Start-Process -FilePath "powershell.exe" -ArgumentList $watchArgs -WindowStyle Hidden -PassThru
    if ($audioMode -eq "mute") {
        Write-Host "Muting the editor's Windows audio session (watcher PID $($watcher.Id), log $audioLog)." -ForegroundColor Yellow
    } else {
        Write-Host "An earlier -Muted launch left $([IO.Path]::GetFileName($editorExe)) muted in Windows: un-muting this editor's session (watcher PID $($watcher.Id))." -ForegroundColor Yellow
    }
}

if ($WaitForReady) {
    $readySignal = Join-Path $signalsDir "editor-$($editorProcess.Id)-true.json"
    $editorLog = Join-Path $logsPath "$projectName.log"
    Write-Host "Waiting for editor readiness signal (gives up after ${ReadyTimeoutSec}s without a sign of life, ${ReadyMaxSec}s at most)..." -ForegroundColor Yellow
    $total = [Diagnostics.Stopwatch]::StartNew()
    $sinceLife = [Diagnostics.Stopwatch]::StartNew()
    $lastCpu = [TimeSpan]::Zero
    $lastLogLength = -1
    $lastHealthUpdate = ""
    $nextNote = 30
    while (-not (Test-Path $readySignal)) {
        if ($editorProcess.HasExited) {
            Write-Host "Editor process exited (code $($editorProcess.ExitCode)) before signaling ready." -ForegroundColor Red
            exit 3
        }
        # Signs of life: CPU time, log growth, a refreshed health file. Any of them restarts the no-progress clock.
        $alive = $false
        try { $editorProcess.Refresh(); $cpu = $editorProcess.TotalProcessorTime; if ($cpu -gt $lastCpu + [TimeSpan]::FromMilliseconds(200)) { $alive = $true; $lastCpu = $cpu } } catch {}
        $logItem = Get-Item -LiteralPath $editorLog -ErrorAction SilentlyContinue
        if ($logItem -and $logItem.LastWriteTimeUtc -ge $launchedUtc -and $logItem.Length -ne $lastLogLength) { $alive = $true; $lastLogLength = $logItem.Length }
        $health = Get-FreshHealth $signalsDir $editorProcess.Id
        if ($health -and $health.updatedUtc -ne $lastHealthUpdate) { $alive = $true; $lastHealthUpdate = $health.updatedUtc }
        if ($alive) { $sinceLife.Restart() }
        if ($sinceLife.Elapsed.TotalSeconds -ge $ReadyTimeoutSec) {
            Write-Host "Editor did not signal ready and showed no sign of life for ${ReadyTimeoutSec}s (no CPU, no log, no health file); after $([int]$total.Elapsed.TotalSeconds)s in all." -ForegroundColor Red
            exit 2
        }
        if ($total.Elapsed.TotalSeconds -ge $ReadyMaxSec) {
            Write-Host "Editor did not signal ready within ${ReadyMaxSec}s (it is still working: it may still be loading)." -ForegroundColor Red
            exit 2
        }
        if ($total.Elapsed.TotalSeconds -ge $nextNote) {
            Write-Host "  Still starting after $([int]$total.Elapsed.TotalSeconds)s (alive; log $([int]($lastLogLength / 1024)) KB)..." -ForegroundColor Gray
            $nextNote += 30
        }
        Start-Sleep 1
    }
    $waited = [int]$total.Elapsed.TotalSeconds
    Write-Host "Editor is ready (signaled after ${waited}s)." -ForegroundColor Green

    # Verify the editor actually opened the requested map. The readiness signal publishes the loaded
    # level as "currentMap"; if it differs from -Map, world edits would land on the wrong level.
    # The signal ("toolsets registered") can be published BEFORE the -Map level finishes loading, and
    # at that instant currentMap is the transient /Temp/Untitled_N world (or empty). Treat that as
    # "not loaded yet" and re-read the signal for up to ~20s waiting for a real level; only warn when
    # a genuine /Game (or other mount-point) level is loaded that differs from -Map.
    if ($Map) {
        # Read currentMap, collapsing a full object path (/Game/Maps/L_Foo.L_Foo) to its package part.
        function Get-CurrentMapPkg {
            try {
                $j = Get-Content -LiteralPath $readySignal -Raw -ErrorAction Stop | ConvertFrom-Json
                if ($j.currentMap) { return ($j.currentMap -split '\.')[0] }
            } catch { }
            return ""
        }

        $currentMapPkg = Get-CurrentMapPkg
        $mapWaited = 0
        while (($currentMapPkg -eq "" -or $currentMapPkg -like "/Temp/*") -and $mapWaited -lt 20) {
            Start-Sleep 1
            $mapWaited++
            $currentMapPkg = Get-CurrentMapPkg
        }

        if ($currentMapPkg -eq "" -or $currentMapPkg -like "/Temp/*") {
            # Still transient after the grace window: the level is loading in the background. Not an
            # error -- just cannot confirm it here.
            Write-Host "NOTE: level still loading at signal time (currentMap '$currentMapPkg'); verify currentMap before world edits." -ForegroundColor Gray
        }
        elseif ($currentMapPkg -ne $Map) {
            # A real level is loaded and it is not the one requested (-ne is case-insensitive).
            Write-Host "WARNING: requested -Map '$Map' but the editor reports currentMap '$currentMapPkg'." -ForegroundColor Yellow
            Write-Host "         The wrong level may be open; verify before making world edits." -ForegroundColor Yellow
        }
    }

    # The MCP endpoint: listening, and on the port asked for.
    $signal = Read-JsonFile $readySignal
    if ($signal) {
        $mcpWaited = 0
        while (-not $signal.mcpListening -and $mcpWaited -lt 20 -and -not $editorProcess.HasExited) {
            Start-Sleep 1
            $mcpWaited++
            $signal = Read-JsonFile $readySignal
            if (-not $signal) { break }
        }
        if ($signal -and $signal.mcpListening) {
            Write-Host "MCP endpoint: http://127.0.0.1:$($signal.mcpPort)/mcp" -ForegroundColor Green
            if ($Port -gt 0 -and [int]$signal.mcpPort -ne $Port) {
                Write-Host "WARNING: -Port $Port was asked for but the editor serves MCP on $($signal.mcpPort)." -ForegroundColor Yellow
            }
        } elseif ($signal) {
            Write-Host "WARNING: the editor reports no MCP listener on port $($signal.mcpPort) (another editor may hold that port; use -Port)." -ForegroundColor Yellow
        }
    }
}

if ($WaitForIdle) {
    # Idle = a fresh health file with no slow task, no modal dialog, no shader jobs and no assets compiling, seen on two
    # heartbeats in a row (~5 s apart).
    Write-Host "Waiting for the editor to be idle (no slow task, shaders and assets compiled; timeout ${IdleTimeoutSec}s)..." -ForegroundColor Yellow
    $idleClock = [Diagnostics.Stopwatch]::StartNew()
    $quietBeats = 0
    $lastBeat = ""
    $nextNote = 0
    $warnedOldPlugin = $false
    while ($true) {
        if ($editorProcess.HasExited) {
            Write-Host "Editor process exited (code $($editorProcess.ExitCode)) while waiting for idle." -ForegroundColor Red
            exit 3
        }
        $health = Get-FreshHealth $signalsDir $editorProcess.Id
        if ($health -and $health.updatedUtc -ne $lastBeat) {
            $lastBeat = $health.updatedUtc
            $hasCounts = $null -ne $health.PSObject.Properties['shaderJobs']
            if (-not $hasCounts -and -not $warnedOldPlugin) {
                Write-Host "NOTE: this VibeUE build does not report shader / asset compile counts; waiting on slow tasks and dialogs only." -ForegroundColor Gray
                $warnedOldPlugin = $true
            }
            $shaders = if ($hasCounts) { [int]$health.shaderJobs } else { 0 }
            $assets = if ($hasCounts) { [int]$health.assetCompilations } else { 0 }
            $busy = $health.slowTask -or $health.modal -or $shaders -ne 0 -or $assets -ne 0
            if ($busy) { $quietBeats = 0 } else { $quietBeats++ }
            if ($quietBeats -ge 2) {
                Write-Host "Editor is idle (after $([int]$idleClock.Elapsed.TotalSeconds)s more)." -ForegroundColor Green
                break
            }
            if ($idleClock.Elapsed.TotalSeconds -ge $nextNote) {
                $parts = @()
                if ($health.slowTask) { $parts += "slow task '$($health.slowTaskText)'" }
                if ($health.modal) { $parts += "modal '$($health.modalTitle)'" }
                if ($hasCounts -and ($shaders -lt 0 -or $assets -lt 0)) { $parts += "compile counts not sampled yet" }
                elseif ($hasCounts) { $parts += "shader jobs $shaders"; $parts += "assets compiling $assets" }
                Write-Host "  $($parts -join ', ')" -ForegroundColor Gray
                $nextNote += 15
            }
        }
        if ($idleClock.Elapsed.TotalSeconds -ge $IdleTimeoutSec) {
            Write-Host "Editor was not idle within ${IdleTimeoutSec}s." -ForegroundColor Red
            exit 6
        }
        Start-Sleep 1
    }
}

Write-Host "=== Launch Complete ===" -ForegroundColor Green
Write-Host "Unreal Editor is starting with $projectName" -ForegroundColor Green
Write-Host "No debugger attached - MCP tools should work without breakpoints!" -ForegroundColor Green
