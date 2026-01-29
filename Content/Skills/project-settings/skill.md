---
name: project-settings
display_name: Project & Editor Settings
description: Configure Unreal Engine project settings AND editor preferences including UI appearance, toolbar icons, scale, colors, and all UDeveloperSettings subclasses
vibeue_classes:
  - ProjectSettingsService
unreal_classes:
  - EditorAssetLibrary
  - EditorStyleSettings
  - EditorPerProjectUserSettings
keywords:
  - project settings
  - editor settings
  - editor style
  - config
  - ini
  - configuration
  - developer settings
  - project name
  - default map
  - settings
  - toolbar
  - icons
  - UI scale
  - ApplicationScale
  - appearance
  - SmallToolBarIcons
---

## Overview

The ProjectSettingsService provides comprehensive access to Unreal Engine project configuration AND editor preferences through a clean Python API. It dynamically discovers ALL UDeveloperSettings subclasses, including:

- **Project settings** (general, maps)
- **Editor appearance** (editorstylesettings - UI scale, toolbar icons, colors)
- **Editor behavior** (editorperprojectusersettings)
- **Any custom UDeveloperSettings** in plugins

It supports:

- **Predefined categories** (general, maps, custom)
- **Dynamic discovery** of UDeveloperSettings subclasses
- **Batch operations** via JSON
- **Direct INI access** for advanced scenarios
