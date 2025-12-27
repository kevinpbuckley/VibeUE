# VibeUE Plugin Packaging Script for Fab Marketplace Submission
# Creates a clean plugin package excluding build artifacts and development files
# Usage: .\MakePlugin.ps1 [-Version <version>]

param(
    [string]$Version = "1.0.0",
    [string]$PackageName = "VibeUE-Package"
)

# Script configuration
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SourceDir = $ScriptDir
$ParentDir = Split-Path -Parent $ScriptDir
$PackageDir = Join-Path $ParentDir $PackageName
$ZipPath = Join-Path $ParentDir "VibeUE.zip"

Write-Host "=== VibeUE Plugin Packaging Script ===" -ForegroundColor Cyan
Write-Host "Source Directory: $SourceDir" -ForegroundColor Gray
Write-Host "Package Directory: $PackageDir" -ForegroundColor Gray
Write-Host "Zip Output: $ZipPath" -ForegroundColor Gray
Write-Host ""

# Always clean previous package and zip file at startup
Write-Host "Cleaning previous package and zip file..." -ForegroundColor Yellow
if (Test-Path $PackageDir) {
    Remove-Item $PackageDir -Recurse -Force
    Write-Host "  Removed existing package directory" -ForegroundColor Gray
}
if (Test-Path $ZipPath) {
    Remove-Item $ZipPath -Force
    Write-Host "  Removed existing zip file" -ForegroundColor Gray
}

# Create package directory
Write-Host "Creating package directory..." -ForegroundColor Green
New-Item -ItemType Directory -Path $PackageDir -Force | Out-Null

# Define files and directories to exclude for Fab submission
$ExcludeDirectories = @(
    "Binaries",           # Build artifacts
    "Intermediate",       # Build artifacts  
    "Packaged",          # Packaged builds
    ".git",              # Git repository
    ".github",           # GitHub workflows and config (development only)
    ".vs",               # Visual Studio
    ".vscode",           # VS Code
    "__pycache__",       # Python cache
    ".pytest_cache",     # Python test cache
    ".venv",             # Python virtual environment (users should create their own)
    "venv",              # Alternative virtual environment naming
    "build",             # Python build directories
    "dist",              # Python distribution directories
    "*.egg-info",        # Python package metadata
    "docs",              # Development documentation (user docs moved to Resources)
    "node_modules"       # Node.js modules
)

$ExcludeFiles = @(
    "*.log",             # Log files
    "*.tmp",             # Temporary files
    "*.pdb",             # Program database files
    "*.lib",             # Library files (will be rebuilt)
    "*.exp",             # Export files
    "*.ilk",             # Incremental linking files
    "*.exe",             # Executable files (not allowed in FAB)
    "*.dll",             # Dynamic libraries (will be rebuilt)
    "*~",                # Backup files
    "*.pyc",             # Python compiled files
    "*.pyo",             # Python optimized files
    ".DS_Store",         # macOS system files
    "Thumbs.db",         # Windows thumbnail cache
    "Desktop.ini",       # Windows folder settings
    "vibe_ue.log"        # Specific log file
)

# Development-only files that shouldn't be in marketplace submission
$ExcludeDevFiles = @(
    "DEAD_HANDLERS_DELETED.md",
    "HANDLER_AUDIT.md", 
    "HANDLER_AUDIT_COMPLETE.md",
    "ISSUE_SUMMARY.md",
    "BUILD_PLUGIN.md",       # Development documentation not needed by end users
    "BuildPlugin.bat",
    "MCP-Inspector.bat",
    "MakePlugin.ps1",        # Build script not needed by end users
    "AddCopyrights.ps1",     # Development script not needed by end users
    "FAB-DESCRIPTION.md",    # Development file not needed by end users
    "FAB-Checklist.md",      # Internal checklist not needed by end users
    ".gitignore"             # Git-specific file not needed by end users
)

# Note: test_prompts folder is now included for user reference and examples

Write-Host "Copying plugin files (excluding build artifacts)..." -ForegroundColor Green

# Use robocopy for efficient copying with exclusions
$RobocopyArgs = @(
    $SourceDir,
    $PackageDir,
    "/E",                # Copy subdirectories including empty ones
    "/XD"                # Exclude directories
) + $ExcludeDirectories + @(
    "/XF"                # Exclude files
) + $ExcludeFiles + $ExcludeDevFiles

Write-Host "Running robocopy with exclusions..." -ForegroundColor Gray
& robocopy @RobocopyArgs | Out-Null

# Robocopy exit codes: 0=no files copied, 1=files copied successfully, 2=extra files/folders detected
# Exit codes 0-7 are considered successful
$RobocopyExitCode = $LASTEXITCODE
if ($RobocopyExitCode -le 7) {
    Write-Host "  Files copied successfully" -ForegroundColor Gray
} else {
    Write-Host "  Robocopy completed with warnings (exit code: $RobocopyExitCode)" -ForegroundColor Yellow
}

# Verify essential files are present
Write-Host "Verifying package contents..." -ForegroundColor Green

$RequiredFiles = @(
    "VibeUE.uplugin",
    "README.md"
)

$RequiredDirs = @(
    "Source",
    "Content", 
    "Resources",
    "Config"
)

$MissingItems = @()

foreach ($file in $RequiredFiles) {
    $filePath = Join-Path $PackageDir $file
    if (-not (Test-Path $filePath)) {
        $MissingItems += "File: $file"
    }
}

foreach ($dir in $RequiredDirs) {
    $dirPath = Join-Path $PackageDir $dir
    if (-not (Test-Path $dirPath)) {
        $MissingItems += "Directory: $dir"
    }
}

if ($MissingItems.Count -gt 0) {
    Write-Host "ERROR: Missing required items:" -ForegroundColor Red
    foreach ($item in $MissingItems) {
        Write-Host "  - $item" -ForegroundColor Red
    }
    exit 1
}

# Verify excluded items are not present
$ExcludedItems = @()
foreach ($dir in $ExcludeDirectories) {
    $dirPath = Join-Path $PackageDir $dir
    if (Test-Path $dirPath) {
        $ExcludedItems += "Directory: $dir"
    }
}

if ($ExcludedItems.Count -gt 0) {
    Write-Host "WARNING: Found excluded items that should not be in package:" -ForegroundColor Yellow
    foreach ($item in $ExcludedItems) {
        Write-Host "  - $item" -ForegroundColor Yellow
    }
}

# Calculate package size
Write-Host "Calculating package size..." -ForegroundColor Green
$PackageStats = Get-ChildItem $PackageDir -Recurse | Measure-Object -Property Length -Sum
$PackageSizeMB = [math]::Round($PackageStats.Sum / 1MB, 2)
$FileCount = $PackageStats.Count

Write-Host "  Files: $FileCount" -ForegroundColor Gray
Write-Host "  Size: $PackageSizeMB MB" -ForegroundColor Gray

# Update plugin version if specified
if ($Version -ne "1.0.0") {
    Write-Host "Updating plugin version to $Version..." -ForegroundColor Green
    $PluginPath = Join-Path $PackageDir "VibeUE.uplugin"
    if (Test-Path $PluginPath) {
        $PluginContent = Get-Content $PluginPath -Raw | ConvertFrom-Json
        $PluginContent.VersionName = $Version
        $PluginContent | ConvertTo-Json -Depth 10 | Set-Content $PluginPath -Encoding UTF8
        Write-Host "  Updated version in VibeUE.uplugin" -ForegroundColor Gray
    }
}

# Create ZIP archive
Write-Host "Creating ZIP archive..." -ForegroundColor Green
try {
    # Use .NET compression for better control
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    
    if (Test-Path $ZipPath) {
        Remove-Item $ZipPath -Force
    }
    
    [System.IO.Compression.ZipFile]::CreateFromDirectory($PackageDir, $ZipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
    
    $ZipStats = Get-Item $ZipPath
    $ZipSizeMB = [math]::Round($ZipStats.Length / 1MB, 2)
    
    Write-Host "  ZIP created successfully" -ForegroundColor Gray
    Write-Host "  ZIP size: $ZipSizeMB MB" -ForegroundColor Gray
}
catch {
    Write-Host "ERROR: Failed to create ZIP archive: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Delete the package directory to prevent build conflicts
Write-Host "Cleaning up package directory..." -ForegroundColor Yellow
try {
    if (Test-Path $PackageDir) {
        Remove-Item $PackageDir -Recurse -Force
        Write-Host "  Package directory removed" -ForegroundColor Gray
    }
}
catch {
    Write-Host "WARNING: Failed to delete package directory: $($_.Exception.Message)" -ForegroundColor Yellow
}

# Final summary
Write-Host ""
Write-Host "=== Package Creation Complete ===" -ForegroundColor Green
Write-Host "ZIP Archive: $ZipPath" -ForegroundColor White
Write-Host "ZIP Size: $ZipSizeMB MB" -ForegroundColor White
Write-Host ""
Write-Host "Ready for Fab Marketplace submission!" -ForegroundColor Cyan