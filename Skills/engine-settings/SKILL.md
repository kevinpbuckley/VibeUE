---
name: engine-settings
display_name: Engine Settings, CVars & Scalability
description: Read and write console variables, scalability levels, engine/project INI values and UDeveloperSettings sections using Unreal 5.9's own ConsoleToolset, ConfigSettingsToolset and FileSystemToolset. Use when the user asks to set a cvar, change quality/scalability, edit DefaultEngine.ini or a Project Settings value, uncap the frame rate, or inspect an engine setting.
unreal_classes:
  - UDeveloperSettings
  - UGameUserSettings
keywords:
  - cvar
  - console variable
  - scalability
  - quality settings
  - DefaultEngine.ini
  - project settings
  - engine settings
  - uncap fps
---

# Engine settings, CVars and scalability

**This is engine-native work in 5.9.** VibeUE's `EngineSettingsService` and
`ProjectSettingsService` were removed on the 5-9 branch — Epic's own toolsets cover all of it, and
cover it better (see "Why the old service is gone" at the end).

> ⚠️ **These toolsets are `call_tool` only.** Unlike VibeUE's services, Epic's `ConsoleToolset`,
> `ConfigSettingsToolset`, `FileSystemToolset` and `ObjectToolset` UFUNCTIONs carry **no
> `PythonCallable`** — `unreal.ConsoleToolset.get_cvar_value(...)` does **not** exist. Reach them
> with `call_tool`, not from inside `execute_python`. (`ConsoleToolset.LogsToolset` is the one
> exception; it is Python-callable.)
>
> Enable **ConsoleToolset**, **ConfigSettingsToolset** and **FileSystemToolset** in the project if
> `list_toolsets` does not show them.

## Console variables

| Want | Tool |
|---|---|
| Read a cvar | `ConsoleToolset.GetCVarValue` |
| Write a cvar | `ConsoleToolset.SetCVarValue` — type-checks the input and **refuses read-only cvars** with "Console variable is read-only: X" |
| Find cvars by substring | `ConsoleToolset.SearchCVars` — returns help text plus current value |
| Run any console command | `ConsoleToolset.RunConsoleCommand` |
| Read command output | `ConsoleToolset.LogsToolset.GetLogEntries` (console output goes to the log, not the tool result) |

```
call_tool SetCVarValue   {"Name": "r.ReflectionMethod", "Value": "1"}
call_tool GetCVarValue   {"Name": "r.ReflectionMethod"}
call_tool SearchCVars    {"Search": "r.Lumen"}
```

A cvar is **not** config: there is nothing to "save" afterwards. The old
`save_all_engine_config()` step after a cvar batch was always a no-op.

Batching several cvars is just several `SetCVarValue` calls, or one
`RunConsoleCommand` with a semicolon-separated command string.

**Accepted loss:** nothing in Epic's surface reports a cvar's *default* value or its `ECVF_*` flag
string. `RunConsoleCommand("<name> ?")` prints `HELP for 'x'(ReadOnly)` and `LastSetBy:` to the log,
which is the closest substitute.

## Scalability / quality

| Want | Tool |
|---|---|
| All group levels | `ConsoleToolset.RunConsoleCommand("Scalability")` → reads back from the log; or `SearchCVars {"Search": "sg."}` |
| One group | `SetCVarValue {"Name": "sg.ShadowQuality", "Value": "2"}` |
| Overall preset | `RunConsoleCommand("Scalability 3")` (or `"Scalability cine"`) |

Prefer the `Scalability` command for the overall preset: it persists to the **correct** ini
(`GEditorSettingsIni` in-editor), which VibeUE's old `set_overall_scalability_level` did not — it
always wrote `GGameUserSettingsIni`, which the editor does not read back.

## Project / engine settings (the Project Settings UI)

Anything that appears in **Project Settings** or **Editor Preferences** is a registered settings
section, and `ConfigSettingsToolset` owns it end to end — live value, correct ini, source control
checkout and undo.

| Want | Tool |
|---|---|
| Browse what exists | `ConfigSettingsToolset.ListContainers` / `ListCategories` / `ListSections` |
| Find a setting by name, tooltip or bound cvar | `ConfigSettingsToolset.SearchSettings` |
| Read values | `ConfigSettingsToolset.GetSectionPropertyValues` |
| Write values | `ConfigSettingsToolset.SetSectionProperties` |
| Force a save | `ConfigSettingsToolset.SaveSection` (usually unnecessary — the setters save) |

```
call_tool SearchSettings          {"Search": "Smooth Frame Rate"}
call_tool GetSectionPropertyValues {"Container": "Project", "Category": "Engine", "Section": "General", "Properties": ["bSmoothFrameRate"]}
call_tool SetSectionProperties     {"Container": "Project", "Category": "Engine", "Section": "General", "Values": "{\"bSmoothFrameRate\": false}"}
```

Useful section coordinates: `UEngine` → `Project / Engine / General`; `ULevelEditorPlaySettings` →
`Editor / LevelEditor / PlayIn`; `UEditorPerformanceSettings` → `Editor / General / Performance`.

> **Do not use `ObjectToolset` for settings.** `ObjectToolset.SetProperties` calls
> `RequireBlueprintOwnedTemplate`, which refuses class-default data of a non-Blueprint class — and a
> `UDeveloperSettings` object *is* a native CDO. It raises
> "...is class default data for 'X', which is not a Blueprint class". Its own source comment says
> config-backed settings "go through ConfigSettingsToolset".

## Raw INI files

For an ini key with **no** registered settings section, edit the file:

| Want | Tool |
|---|---|
| Read a config file | `FileSystemToolset.ReadTextFile` |
| Surgical edit, comments preserved | `FileSystemToolset.EditTextByAnchor` / `EditTextByLineRange` |
| Whole-file write | `FileSystemToolset.WriteTextFile` |
| List a config directory | `FileSystemToolset.ListDirectory` |

`/Saved` and `/Config` resolve inside the project; absolute native paths are allowed.

> ⚠️ **A text edit does not reach the running editor.** `GConfig` already holds the old branch in
> memory, so a file edit applies on the next launch. If the value must take effect now, find a
> registered section and use `ConfigSettingsToolset` instead.

## PIE settings — the one VibeUE helper left

`ULevelEditorPlaySettings` is reachable through `ConfigSettingsToolset`
(`Editor / LevelEditor / PlayIn`), and that is what an agent should use.

Two VibeUE functions survive as **Python-only helpers** (no longer MCP tools, so don't look for them
in `list_toolsets`), because `vibeue.py`'s `GateSession` needs them from inside `execute_python`
where Epic's toolsets are unreachable:

```python
import unreal
s = unreal.EngineSettingsService.get_pie_settings()
# set_pie_settings(net_mode, num_clients, run_under_one_process) — three positional args.
# net_mode is one of "Standalone", "ListenServer", "Client" (alias "DedicatedServer");
# anything else is refused. num_clients must be 1-10.
unreal.EngineSettingsService.set_pie_settings("ListenServer", 2, True)
```

They exist for that gate. For ordinary PIE configuration, use `ConfigSettingsToolset`.

## Why the old service is gone

The 5.8-era justification was that Epic covered **none** of cvars, scalability, raw INI or
`UDeveloperSettings` discovery. In 5.9 all four are covered, and two of VibeUE's writers were
actually broken: `set_engine_ini_value` / `set_ini_value` called `GConfig->SetString` + `Flush` on a
NoSave path and returned `bSuccess=true` without reaching disk, and
`set_overall_scalability_level` wrote the wrong ini. Epic's equivalents persist correctly, check
types, refuse read-only cvars, honour source control and support undo.
