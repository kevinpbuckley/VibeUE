# Building VibeUE Plugin

This document explains how to build the VibeUE plugin to resolve the "Missing Modules" error when adding it to a fresh Unreal Engine project.

## Quick Start

**Double-click this file to build:**
```
Plugins/VibeUE/BuildPlugin.bat
```

That's it! The script will automatically find your Unreal Engine installation and project file, then build the plugin.

## Why Build the Plugin?

When you copy VibeUE to a fresh Unreal Engine project, you may see this error:
```
Missing Modules

The following modules are missing or built with a different engine version:
  VibeUE

Engine modules cannot be compiled at runtime. Please build through your IDE.
```

This happens because VibeUE is a **C++ Editor plugin** that requires compilation. The build script compiles the plugin for your specific Unreal Engine version.

## How the Build Script Works

The `BuildPlugin.bat` script automatically:
1. **Searches for Unreal Engine 5.7** in common installation locations
2. **Finds your project's .uproject file** by searching parent directories
3. **Builds the VibeUE plugin** using Unreal's build tools
4. **Shows clear success/failure messages** with helpful troubleshooting tips

## Advanced Usage

The build script works automatically, but if you need to customize it:

**Edit the BuildPlugin.bat file** to:
- Add different Unreal Engine installation paths (search the section with common paths)
- Change the build configuration from Development to DebugGame or Shipping
- Modify search behavior for project files

## Troubleshooting

### "Could not find Unreal Engine installation"
**Solution**: Edit `BuildPlugin.bat` and add your UE installation path to the search list (look for the "for %%P in" section)

### "Could not find .uproject file"
**Solution**: Make sure the plugin is located in `YourProject/Plugins/VibeUE/`

### "Build failed" with compilation errors
**Common causes**:
1. **Missing Visual Studio** - Install Visual Studio 2022 with C++ workload
2. **Wrong UE version** - The script looks for UE 5.7 by default
3. **Corrupted files** - Delete `Binaries` and `Intermediate` folders, then run BuildPlugin.bat again

## What Gets Built?

After a successful build, you'll find:
```
VibeUE/
  Binaries/
    Win64/
      UnrealEditor-VibeUE.dll    # Main plugin binary
      UnrealEditor-VibeUE.pdb    # Debug symbols
  Intermediate/                   # Build artifacts (can be deleted)
```

## For Plugin Developers

### Building for Distribution
If you're preparing to distribute the plugin:

1. **Clean build first**:
   ```powershell
   .\BuildPlugin.ps1 -Clean
   ```

2. **Build for all configurations**:
   ```powershell
   .\BuildPlugin.ps1 -Configuration "Development"
   .\BuildPlugin.ps1 -Configuration "DebugGame"
   .\BuildPlugin.ps1 -Configuration "Shipping"
   ```

3. **Remove binaries before committing** to git (Epic Marketplace requirement):
   - Delete `Binaries/` folder
   - Delete `Intermediate/` folder
   
   The Marketplace compiles plugins automatically for all supported engine versions.

### Testing on Fresh Project
To verify the plugin works after building:

1. Create a new Unreal Engine project
2. Copy the entire VibeUE folder to `MyProject/Plugins/`
3. Run the build script from the plugin folder
4. Launch Unreal Engine - it should open without the "Missing Modules" error

## Platform Support

Currently, the build script supports:
- ✅ Windows (Win64)
- ⏳ Mac (coming soon)
- ⏳ Linux (coming soon)

For Mac/Linux, you can manually build using:
```bash
# Navigate to UE installation
cd /Path/To/UE_5.7

# Build the plugin
Engine/Build/BatchFiles/Mac/Build.sh MyProjectEditor Mac Development /Path/To/MyProject.uproject -waitmutex
```

## Integration with IDEs

### Visual Studio
You can also build from Visual Studio:
1. Generate project files: Right-click .uproject → "Generate Visual Studio project files"
2. Open the .sln file
3. Build the project in Visual Studio

### Rider
1. Open the .uproject file in Rider
2. Build the project from the Build menu

## See Also

- [Main VibeUE README](README.md)
- [Installation Guide](docs/installation.md)
- [Troubleshooting Guide](docs/troubleshooting.md)
