---
name: engine-settings
display_name: Engine Settings System
description: Configure Unreal Engine core settings including rendering, physics, audio, garbage collection, console variables (cvars), and scalability levels
vibeue_classes:
  - EngineSettingsService
unreal_classes:
  - RendererSettings
  - PhysicsSettings
  - AudioSettings
  - IConsoleManager
  - Scalability
keywords:
  - engine settings
  - cvar
  - console variable
  - rendering
  - physics
  - audio
  - garbage collection
  - gc
  - scalability
  - quality
  - graphics
  - ini
  - config
---

## Overview

The EngineSettingsService provides comprehensive access to Unreal Engine configuration through a clean Python API. It supports:

- **Predefined categories** (rendering, physics, audio, gc, streaming, network, collision, ai, input)
- **Console variables (cvars)** with search and prefix filtering
- **Scalability settings** for quality level control
- **Direct engine INI access** for advanced scenarios
- **Batch operations** via JSON

## Key Differences from ProjectSettingsService

| Feature | ProjectSettingsService | EngineSettingsService |
|---------|----------------------|---------------------|
| Focus | Project-specific settings | Engine-wide configuration |
| Config Files | DefaultGame.ini | DefaultEngine.ini, DefaultInput.ini |
| CVars | No | Full cvar support |
| Scalability | No | Full scalability API |
| Categories | general, maps, custom | rendering, physics, audio, gc, etc. |

## When to Use

- **Rendering optimization**: Adjust r.* cvars, ray tracing, shadows
- **Physics tuning**: Configure collision profiles, physics simulation
- **Audio configuration**: Sample rates, channels, spatialization
- **Performance profiling**: GC settings, streaming, scalability
- **Platform settings**: Windows graphics API, shader formats
- **Quality presets**: Apply overall or per-group scalability levels
