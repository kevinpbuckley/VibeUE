# FAB Marketplace Submission Checklist

## Product Listing

### General
- [x] All text must be accurate and relevant to the asset ✅
- [x] All text fields must contain an English version ✅

### Media
- [ ] Media must accurately display the relevant functionality or contents of the project ⚠️ *Needs screenshots/videos*

### Technical Information
- [ ] All Technical Information fields must be filled out in their entirety ⚠️ *To be completed during submission*
- [ ] Technical Information text must identify dependencies (if any), prerequisites, or other requirements for use of the asset ⚠️ *To be documented*

## Project Files
- [x] Each Project File Link hosts only one UE Project or Plugin folder with the proper folder structure ✅
- [x] Project(s) provided match the Supported Engine Versions listed (UE 5.7) ✅
- [x] Distribution Method is appropriate for the content and functionality of the product ✅

## Content

### Files
- [x] All asset types are inside of their respective folders ✅
- [x] Project contains no unused folders or assets ✅ *MakePlugin.ps1 excludes dev artifacts*
- [ ] .uproject has unused plugins disabled ⚠️ *N/A for plugin-only distribution*

### Documentation
- [x] Publisher provides either linked or in-editor documentation/tutorials ✅
  - **README.md**: Comprehensive installation and usage guide
  - **Resources/**: User-facing documentation
  - **docs/**: Developer documentation (excluded from package)

## Legal
- [x] Products must not be offensive, vulgar, or slanderous ✅
- [x] Megascans content is not re-distributed ✅ *No Megascans used*
- [x] Substantial portions of sample content or source code from Epic Games is used for display/example only ✅

## Code Plugins

### .uplugin Configuration
- [x] .uplugin has "EngineVersion" key with value "5.7" ✅
- [x] .uplugin has "WhitelistPlatforms" key matching Supported Target Platforms (Win64, Mac, Linux) ✅
- [x] .uplugin has "FabURL" key with Listing ID ✅

### Copyright & Source Code
- [x] All source and header files (86 files) contain copyright notice ✅
  - Format: `// Copyright Kevin Buckley 2025 All Rights Reserved.`

### File Structure
- [x] Plugin folder contains no unused or local folders in packaged distribution ✅
  - **MakePlugin.ps1 excludes**: Binaries, Intermediate, Saved, Build, .git, .vscode, __pycache__, .venv
- [x] FilterPlugin.ini filters in custom folders intended for distribution ✅
  - Includes: /Resources/, /README.md, /BuildPlugin.bat, /test_prompts/, /docs/
- [x] All file paths starting from plugin folder are ≤170 characters ✅

### Build & Dependencies
- [x] Plugin generates no errors or consequential warnings ✅
- [ ] C++ third-party code and libraries are in ThirdParty folder ⚠️ *No C++ third-party code*
- [x] Python third-party code and libraries are in Content/Python/Lib/site-packages/ ✅
- [ ] Third Party Software form accurately filled out ⚠️ *Required for Python dependencies*

## Validation Summary

### ✅ Passing (20 items)
- All text accurate and in English
- Plugin file structure correct
- Engine version 5.7 supported
- File organization correct
- Documentation accessible (README.md + Resources/)
- No offensive content
- EngineVersion + WhitelistPlatforms + FabURL configured
- All 86 source files have copyright headers
- FilterPlugin.ini configured correctly
- File path lengths within limits
- No build errors
- Python dependencies in Content/Python/Lib/site-packages/

### ⚠️ Needs Attention (3 items)
1. **Media**: Create screenshots/videos showing plugin functionality
2. **Technical Information**: Complete during FAB submission
3. **Third Party Software Form**: Document Python dependencies (FastMCP, Pydantic, HTTPX, Uvicorn, etc.)

## Pre-Submission Tasks
- [ ] Create 3-5 screenshots (plugin UI, VS Code integration, Blueprint/UMG demos)
- [ ] Create 1-2 minute demo video
- [ ] Fill out Third Party Software form
- [ ] Run `MakePlugin.ps1` to generate final package
- [ ] Test package in clean UE 5.7 project

**Status**: ~95% complete | **Remaining**: Media assets + Third Party form
