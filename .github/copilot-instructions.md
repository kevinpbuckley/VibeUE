# VibeUE Plugin - AI Coding Agent Instructions

## Project Overview

**VibeUE** is an Unreal Engine 5.7+ editor plugin that brings AI-powered development directly into the Unreal Editor through an In-Editor Chat Client and Model Context Protocol (MCP) integration. The plugin exposes 14 multi-action tools with 177 total actions for manipulating Blueprints, UMG widgets, materials, assets, and the level through natural language.

### Core Capabilities
- **In-Editor AI Chat**: Slate-based chat interface running inside Unreal Editor
- **MCP Server**: HTTP server exposing tools to external AI IDEs (VS Code, Claude Desktop, Cursor, Windsurf)
- **Python Execution**: Execute Python code in Unreal Editor with full API access
- **14 Multi-Action Tools**: Blueprint, UMG, Material, Asset, Level Actor, Data Asset, Data Table, Enhanced Input, Python, Search, File operations, and more
- **Custom Instructions**: Project-specific context via markdown files in `Config/Instructions/`

### Tech Stack
- **C++17**: Core plugin implementation following UE5 standards
- **Unreal Engine 5.7+**: Editor subsystems, Python scripting, Slate UI
- **JSON-RPC**: Tool communication protocol
- **MCP (Model Context Protocol)**: Standard for AI tool integration
- **Python 3.11**: Embedded Python for scripting automation

## Critical Developer Workflows

### Building the Plugin

**Quick build** (after cloning or pulling changes):
```bash
# From plugin root (Plugins/VibeUE/)
./BuildPlugin.bat
```

**Build and launch** Unreal Editor (PowerShell):
```powershell
# From FPS57 project root
./BuildAndLaunch.ps1
```

This script:
1. Saves all dirty assets via MCP `manage_asset` tool with `action="save_all"`
2. Closes any running Unreal Editor instances
3. Builds the plugin using UAT
4. Launches Unreal Editor with the FPS57 project

**NEVER ask the user to manually close the editor** - use `BuildAndLaunch.ps1` instead.

### Testing Strategy

VibeUE uses **manual integration testing** with natural language prompts instead of automated unit tests.

**Test location**: `test_prompts/` directory
- Each `.md` file contains sequential prompts to test a feature
- Prompts are separated by `---` markers
- Execute prompts one at a time, verifying results

**Quick smoke test**:
1. Open AI Chat: `Window > VibeUE > AI Chat`
2. Execute prompts from `test_prompts/Smoke_Test.md` sequentially
3. Verify assets are created in `/Game/Tests/` folder

**Domain-specific tests**:
- `test_prompts/blueprint/` - Blueprint creation, variables, functions, nodes
- `test_prompts/umg/` - Widget blueprints, hierarchy, bindings
- `test_prompts/materials/` - Material graphs, parameters, functions
- `test_prompts/enhanced_input/` - Input actions, mappings, contexts

**Testing with MCP tools in AI Chat**: Use the VibeUE MCP tools directly through natural language, as if you were an external AI assistant.

### Development Workflow for New Features

1. **Identify service domain**: Blueprint, UMG, Material, Asset, Level, Data, Input
2. **Add service method**: `Source/VibeUE/Public/Services/<Domain>/<ServiceName>.h`
3. **Update DTOs if needed**: `Source/VibeUE/Public/Services/<Domain>/Types/`
4. **Add command handler**: `Source/VibeUE/Private/Commands/<Domain>Commands.cpp`
5. **Expose tool action**: `Source/VibeUE/Private/Tools/EditorTools.cpp`
6. **Rebuild**: `./BuildPlugin.bat` or `./BuildAndLaunch.ps1`
7. **Test**: Create test prompts in `test_prompts/<domain>/`

## Architecture

### Layered Architecture (NEVER bypass layers)

```
UI Layer (Slate Chat Interface)
    ↓
Tools Layer (14 UFUNCTION-exposed tools in EditorTools.cpp)
    ↓
Commands Layer (JSON-RPC dispatch, parameter validation, JSON↔DTO conversion)
    ↓
Service Layer (~30 domain services, single responsibility, <500 lines each)
    ↓
Core Infrastructure (TResult<T>, ErrorCodes, ServiceContext)
    ↓
Unreal Engine Editor APIs (Subsystems, Blueprints, Assets, Python)
```

**CRITICAL**: Tools call Commands, Commands call Services. Never bypass this flow.

### Service-Oriented Design

**~30 focused services** replacing the original 26,000-line monolithic implementation:

**Blueprint Services** (7 services):
- `BlueprintLifecycleService` - Create, delete, save, duplicate
- `BlueprintDiscoveryService` - Find, list, inspect blueprints
- `BlueprintPropertyService` - Get/set variables, defaults
- `BlueprintComponentService` - Add/remove/modify components
- `BlueprintFunctionService` - Create functions, add parameters
- `BlueprintNodeService` - Add/connect/disconnect nodes
- `BlueprintGraphService` - Graph manipulation, compilation

**UMG Services** (9 services):
- `UMGLifecycleService`, `UMGDiscoveryService`, `UMGHierarchyService`
- `UMGPropertyService`, `UMGWidgetService`, `UMGBindingService`
- `UMGAssetService`, `UMGEventService`, `UMGInfoService`

**Other services**: Material (2), Asset (3), Data (4), Enhanced Input (3), Level Actor (1)

Each service:
- Lives in `Source/VibeUE/Public/Services/<Domain>/`
- Returns `TResult<TReturnType>` for error handling
- Uses DTOs from `Types/` subdirectory
- Operates only within its domain

### Key Directories

```
Source/VibeUE/
├── Public/
│   ├── Services/        # Domain services (~30 services)
│   │   ├── Blueprint/   # 7 Blueprint services + Types/
│   │   ├── UMG/         # 9 UMG services + Types/
│   │   ├── Material/    # Material services + Types/
│   │   ├── Asset/       # Asset services
│   │   └── ...
│   ├── Core/            # TResult, ErrorCodes, ServiceContext
│   └── Tools/           # EditorTools.h (UFUNCTION tool declarations)
├── Private/
│   ├── Commands/        # JSON-RPC command handlers
│   │   ├── BlueprintCommands.cpp
│   │   ├── UMGCommands.cpp
│   │   └── ...
│   ├── Tools/           # EditorTools.cpp (tool implementations)
│   ├── Services/        # Service implementations
│   ├── MCP/             # MCP server (HTTP + stdio)
│   ├── Chat/            # Slate chat UI
│   └── Utils/           # JSON helpers, validation, logging
Content/
├── instructions/        # AI system prompts (vibeue.instructions.md)
└── examples/            # Python code examples for AI discovery
Config/
├── Instructions/        # User-provided project context
└── vibeue.mcp.json      # External MCP server configuration
test_prompts/            # Integration test prompts (.md files)
```

## Project-Specific Conventions

### Error Handling Pattern

**ALWAYS use `TResult<T>` for operations that can fail**:

```cpp
// In Service
TResult<FMyDTO> MyService::DoSomething(const FString& Input)
{
    if (Input.IsEmpty())
    {
        return TResult<FMyDTO>::Failure(
            EErrorCode::InvalidParameters,
            TEXT("Input cannot be empty")
        );
    }
    
    FMyDTO Result;
    // ... populate Result ...
    
    return TResult<FMyDTO>::Success(Result);
}

// In Command Handler
TResult<FMyDTO> Result = MyService->DoSomething(Input);
if (!Result.IsSuccess())
{
    return CreateErrorJson(Result.GetErrorCode(), Result.GetError());
}
```

**Error codes** are defined in `Source/VibeUE/Public/Core/ErrorCodes.h`

### JSON ↔ DTO Conversion Pattern

**Commands layer handles all JSON conversion**:

```cpp
// In BlueprintCommands.cpp
TSharedPtr<FJsonObject> FBlueprintCommands::HandleCreateBlueprint(const FJsonObject& Params)
{
    // 1. Extract parameters from JSON
    FString BlueprintName;
    if (!Params.TryGetStringField(TEXT("name"), BlueprintName))
    {
        return CreateErrorJson(EErrorCode::InvalidParameters, TEXT("Missing 'name'"));
    }
    
    // 2. Call service
    TResult<FBlueprintInfoDTO> Result = BlueprintLifecycleService->CreateBlueprint(
        BlueprintName, ParentClass, BlueprintPath
    );
    
    // 3. Convert DTO to JSON
    if (Result.IsSuccess())
    {
        return CreateSuccessJson(BlueprintInfoDTOToJson(Result.GetValue()));
    }
    return CreateErrorJson(Result.GetErrorCode(), Result.GetError());
}
```

**DTO conversion helpers** are in `Source/VibeUE/Private/Utils/JsonHelpers.h`

### Naming Conventions

**Services**: `<Domain><Purpose>Service` (e.g., `BlueprintLifecycleService`)
**DTOs**: `F<Name>DTO` (e.g., `FBlueprintInfoDTO`)
**Commands**: `F<Domain>Commands` (e.g., `FBlueprintCommands`)
**Tools**: Single `UEditorTools` class with 14 multi-action UFUNCTIONs
**Test prompts**: `NN_descriptive_name.md` (e.g., `01_create_basic_blueprint.md`)

### UE5 API Patterns

**Getting subsystems**:
```cpp
// Editor subsystems
UEditorAssetSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();

// Subobject editing (Blueprints)
USubobjectDataSubsystem* SubobjectSubsystem = USubobjectDataSubsystem::Get();
```

**Blueprint manipulation requires**:
1. `UBlueprint*` asset
2. `UBlueprintGeneratedClass*` generated class
3. `UObject*` CDO (Class Default Object) for defaults
4. Compile after changes: `FKismetEditorUtilities::CompileBlueprint(Blueprint)`

**NEVER modify CDOs (Class Default Objects)** - causes crashes. Only read from them.

### Python Integration

**Python code execution** happens through `FPythonExecutionService`:
- Code runs in Unreal's embedded Python 3.11 interpreter
- Has access to `unreal` module with full Editor API
- Use `discover_class`, `discover_module`, `discover_function` actions first
- Then use `execute_code` action with learned APIs

**Discovery workflow** (from `Content/instructions/vibeue.instructions.md`):
1. **Discover** - Use `discover_class`, `discover_module`, `help`, `get_examples`
2. **Learn** - Study methods, properties, parameters from response
3. **Execute** - Write code using exact APIs from discovery

**Example** - Don't guess, discover first:
```python
# WRONG: Guessing the API
blueprint.add_variable('Health', 'float')  # May not exist

# RIGHT: Discover then execute
# First: action="discover_class", class_name="unreal.BlueprintEditorLibrary"
# Learn: add_member_variable(blueprint, member_name, variable_type)
# Then execute with correct signature
```

## Integration Points

### MCP Server

**Location**: `Source/VibeUE/Private/MCP/MCPServer.cpp`
**Protocols**: HTTP (port 8088) and stdio
**Authentication**: Bearer token `5432112345` (configurable in plugin settings)

**External MCP servers** can be configured in `Config/vibeue.mcp.json`:
```json
{
  "servers": {
    "my-tool": {
      "type": "stdio",
      "command": "python",
      "args": ["-m", "my_mcp_tool"],
      "env": {"PYTHONPATH": "/path/to/tool"}
    }
  }
}
```

### Chat UI

**Slate implementation**: `Source/VibeUE/Private/Chat/`
- `SChatWindow.cpp` - Main chat window
- `SChatMessage.cpp` - Individual message widgets
- `SChatInput.cpp` - Input field with tool manager

**LLM providers**: VibeUE API (default) or OpenRouter
**Settings**: `Project Settings > Plugins > VibeUE`

### Tool Registry

**Dynamic tool registration**: `Source/VibeUE/Private/Core/ToolRegistry.cpp`
- Tools register themselves via `REGISTER_TOOL_ACTION` macro
- Can be enabled/disabled per-conversation in UI
- Saved to `Saved/VibeUE_DisabledTools.txt`

## Common Pitfalls

1. **Opening UI windows from Python** - Causes access violations. Use async or defer to game thread.
2. **Modifying CDOs** - Never `cdo.set_editor_property()` - read-only access only.
3. **Bypassing architecture layers** - Always: Tools → Commands → Services → UE API.
4. **Guessing Python APIs** - Always use `discover_*` actions before `execute_code`.
5. **Forgetting to compile blueprints** - Changes don't take effect until compiled.
6. **Not saving dirty assets before BuildAndLaunch** - Use MCP `manage_asset` with `action="save_all"`.

## Key Files to Reference

**Architecture examples**:
- `CLAUDE.md` - Comprehensive development guide
- `Source/VibeUE/Private/Commands/BlueprintCommands.cpp` - Full command layer example
- `Source/VibeUE/Public/Services/Blueprint/BlueprintLifecycleService.h` - Service pattern

**Testing examples**:
- `test_prompts/blueprint/01_create_basic_blueprint.md` - Simple test structure
- `test_prompts/umg/05_create_complex_ui.md` - Multi-step test workflow

**Python integration**:
- `Content/instructions/vibeue.instructions.md` - AI assistant behavior (discovery-first workflow)
- `Content/examples/` - Python code examples for common operations
