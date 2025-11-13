# MakePlugin.ps1 - VibeUE Fab Packaging Script

## Overview
This PowerShell script creates a clean VibeUE plugin package ready for Fab Marketplace submission by excluding build artifacts and development-specific files.

## Usage

```powershell
# Basic usage with default version
.\MakePlugin.ps1

# Specify custom version
.\MakePlugin.ps1 -Version "1.2.0"

# Custom package name
.\MakePlugin.ps1 -Version "1.0.0" -PackageName "VibeUE-Release"
```

## What Gets Excluded

### Directories:
- `Binaries/` - Build artifacts
- `Intermediate/` - Build artifacts  
- `Packaged/` - Packaged builds
- `.git/` - Git repository
- `.github/` - GitHub workflows and config
- `.vs/`, `.vscode/` - IDE-specific folders
- `__pycache__/` - Python cache
- `build/`, `dist/` - Build directories

### Files:
- `*.log`, `*.tmp`, `*.pdb` - Temporary and debug files
- `MakePlugin.ps1` - Build script itself
- `.gitignore` - Git-specific configuration
- Development documentation files

## Output

The script creates:
1. **Package Directory**: `../VibeUE-Fab-Package/` - Clean plugin files
2. **ZIP Archive**: `../VibeUE-Fab-Package.zip` - Compressed package ready for submission

## Verification

The script automatically:
- ✅ Verifies required files exist (`VibeUE.uplugin`, `README.md`, `Source/`, `Python/`)
- ✅ Confirms excluded items are not present
- ✅ Reports package statistics (file count, size)
- ✅ Creates compressed ZIP for easy upload

## Fab Marketplace Ready

The generated package contains only the files needed by end users:
- Plugin source code and binaries will be built by Unreal Engine
- Python MCP server with all dependencies  
- Documentation and examples
- Configuration files

Perfect for Fab Marketplace submission! 🚀