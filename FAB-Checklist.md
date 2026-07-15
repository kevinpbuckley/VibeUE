# FAB Marketplace Submission Checklist

_Last verified: 2026-06-21 against the `5-8` branch (UE 5.8)._

## Product Listing

### General
- [x] All text must be accurate and relevant to the asset — FAB-DESCRIPTION.md and FAB_Tech_Details.md
  rewritten for the 5.8 MCP-Expansion positioning; `.uplugin` Description updated (no longer references chat)
- [x] All text fields must contain an English version

### Media
- [ ] Media must accurately display the relevant functionality or contents of the project ⚠️ *Needs screenshots/video*

### Technical Information
- [ ] All Technical Information fields must be filled out in their entirety ⚠️ *Completed during submission from FAB_Tech_Details.md*
- [x] Technical Information identifies dependencies/prerequisites — see FAB_Tech_Details.md (UE 5.8+, native
  MCP stack must be enabled first; auto-enabled engine plugins listed)

## Project Files
- [x] Each Project File Link hosts only one Plugin folder with the proper structure
- [x] Project(s) match the Supported Engine Versions listed (**UE 5.8**)
- [x] Distribution Method is appropriate (code plugin)

## Content

### Files
- [x] All asset types are inside their respective folders
- [x] Project contains no unused folders or assets — removed orphan `ErrorCodes_python_addition.h`
  (a redundant, unreferenced dev fragment already merged into `ErrorCodes.h`); MakePlugin.ps1 excludes dev artifacts
- [ ] .uproject has unused plugins disabled — *N/A for plugin-only distribution*

### Documentation
- [x] Publisher provides linked or in-editor documentation/tutorials
  - **README.md**: installation + usage guide (shipped)
  - **Resources/Icon128.png**: plugin icon shipped (Resources/BUILD_PLUGIN.md is a dev doc, excluded)
  - **docs/**: developer documentation (excluded from package by MakePlugin.ps1)
  - **Content/samples/AGENTS.md.sample**: agent integration guide (shipped)

## Legal
- [x] Products must not be offensive, vulgar, or slanderous
- [x] Megascans content is not re-distributed — *no Megascans used*
- [x] Epic sample content/source used for display/example only

## Code Plugins

### .uplugin Configuration
- [x] `EngineVersion` = **"5.8.0"**
- [x] `WhitelistPlatforms` = Win64 (matches Supported Target Platforms)
- [x] `FabURL` set with Listing ID
- [x] `DocsURL`, `SupportURL`, `CreatedByURL` populated (vibeue.com) — confirm `/support` resolves
- [x] `Resources/Icon128.png` present (128×128) — placeholder lettermark; swap for final art if desired

### Copyright & Source Code
- [x] All source/header files (**105 files**) contain the copyright notice
  - Format: `// Copyright Buckley Builds LLC 2026 All Rights Reserved.`

### File Structure
- [x] Plugin folder contains no unused/local folders in packaged distribution
  - **MakePlugin.ps1 excludes**: Binaries, Intermediate, .git, .github, docs, *.exe,
    and dev files (FAB-DESCRIPTION.md, FAB_Tech_Details.md, FAB-Checklist.md, .gitignore)
- [x] FilterPlugin.ini includes custom distribution folders
  - Includes: `/README.md`, `/test_prompts/...` (Content/, Resources/, Source/, Config/ and the
    `.uplugin` are included automatically; `Content/Python/init_unreal.py` registers skills at startup)
- [x] All file paths from the plugin folder are ≤170 characters (longest ≈78)

### Build & Dependencies
- [x] Plugin generates no errors or consequential warnings — builds clean under warnings-as-errors
  (`bWarningsAsErrors = true`, `-StrictRebuild`)
- [ ] C++ third-party code/libraries in ThirdParty folder — *N/A, no C++ third-party code*
- [x] Shipped Python (`Content/Python/init_unreal.py`) imports only engine modules (`unreal`, `toolset_registry`) — no third-party packages
- [x] Third Party Software form — *N/A, no third-party libraries shipped*

## Validation Summary

### ✅ Passing
- All shipped text accurate and in English (descriptions + .uplugin updated for 5.8)
- Plugin file structure correct; no unused assets (orphan header removed)
- Engine version 5.8 supported; platform Win64
- Documentation accessible (README.md + Resources/ + AGENTS.md.sample)
- No offensive content; no redistributed Epic/Megascans content
- EngineVersion + WhitelistPlatforms + FabURL configured
- All 105 source files have copyright headers
- FilterPlugin.ini + MakePlugin.ps1 exclusions configured correctly
- File path lengths within limits
- No build errors/warnings

### ⚠️ Needs Attention
1. **Media**: create screenshots/video showing functionality (agent driving the editor via MCP, terrain, profiling)
2. **Technical Information**: paste FAB_Tech_Details.md content into the FAB form during submission

## Pre-Submission Tasks
- [ ] Create 3-5 screenshots (MCP agent in action, terrain heightmap import, performance profiling, Blueprint/UMG)
- [ ] Create a 1-2 minute demo video
- [ ] Run `MakePlugin.ps1` to generate the final package
- [ ] Test the package in a clean **UE 5.8** project

**Status**: code/text ready | **Remaining**: media assets + final package test
