---
name: project-settings
display_name: Project Settings System
description: Configure Unreal Engine project settings including general project info, maps, rendering, physics, and direct INI file access
vibeue_classes:
  - ProjectSettingsService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - project settings
  - config
  - ini
  - configuration
  - developer settings
  - project name
  - default map
  - settings
---

## Overview

The ProjectSettingsService provides comprehensive access to Unreal Engine project configuration through a clean Python API. It supports:

- **Predefined categories** (general, maps, custom)
- **Dynamic discovery** of UDeveloperSettings subclasses
- **Batch operations** via JSON
- **Direct INI access** for advanced scenarios
