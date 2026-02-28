# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

VibeUE is an Unreal Engine 5.7+ editor plugin that provides AI-powered development capabilities through an In-Editor Chat Client and Model Context Protocol (MCP) integration. The plugin exposes 14 multi-action tools with 177 total actions for manipulating Blueprints, UMG widgets, materials, assets, and more through natural language.

## Essential Commands

### Building the Plugin

**Quick build** (after cloning or pulling changes):
```bash
# From plugin root directory
./BuildPlugin.bat
```

**Build and launch Unreal Editor** (PowerShell):
```powershell
# From plugin root directory
./buildandlaunch.ps1
```

This script builds the plugin and automatically launches Unreal Engine with your project.

The build scripts automatically locate your Unreal Engine installation and project file. If the build fails:
- Ensure Visual Studio 2022 with C++ workload is installed
- Verify Unreal Engine 5.7+ is installed
- Delete `Binaries/` and `Intermediate/` folders and retry

**Manual build** (if scripts fail):
```bash
# Generate project files first (right-click .uproject → Generate VS project files)
# Then open the .sln and build from Visual Studio
```

### Running Tests

VibeUE uses **manual integration testing** with natural language prompts instead of automated unit tests. Test files are located in `test_prompts/`.

**Quick smoke test** (verifies basic functionality):
1. Open your Unreal project with VibeUE enabled
2. Open AI Chat: `Window > VibeUE > AI Chat`
3. Run through prompts in `test_prompts/Smoke_Test.md` sequentially
4. Each prompt creates/modifies assets in a test folder

**Domain-specific tests**:
- Blueprint operations: `test_prompts/blueprint/`
- UMG widgets: `test_prompts/umg/`
- Materials: `test_prompts/materials/`
- Enhanced Input: `test_prompts/enhanced_input/`
- Level actors: `test_prompts/level_actors/`
- Data assets/tables: `test_prompts/data_asset/`, `test_prompts/data_table/`

### Development Workflow

**Typical workflow when adding a new feature**:
1. Identify the service domain (Blueprint, UMG, Material, etc.)
2. Add/modify service methods in `Source/VibeUE/Public/Services/<Domain>/`
3. Update DTOs in `Source/VibeUE/Public/Services/<Domain>/Types/` if needed
4. Update command handlers in `Source/VibeUE/Private/Commands/<Domain>Commands.cpp`
5. Update tool actions in `Source/VibeUE/Private/Tools/EditorTools.cpp`
6. Rebuild the plugin
7. Test with natural language prompts in the AI Chat

## Architecture

### Layered Architecture

VibeUE follows a strict layered architecture with clear separation of concerns:

```
UI Layer (Slate)
    ↓ (user interaction)
Tools Layer (UFUNCTION-exposed multi-action tools)
    ↓ (JSON-RPC dispatch)
Commands Layer (parameter parsing, validation, JSON conversion)
    ↓ (type-safe operations)
Service Layer (domain services with single responsibility)
    ↓ (UE API calls)
Core Infrastructure (TResult, ErrorCodes, ServiceContext)
    ↓
Unreal Engine Editor APIs
```

**Never bypass layers**: Tools should call Commands, Commands should call Services. Direct calls from Tools to Services or UE APIs breaks the architecture.

### Service-Oriented Design

The codebase is organized into **~30 focused services**, each under 500 lines, replacing the original monolithic 26,000-line implementation.

**Service categories**:
- **Blueprint Services** (7 services): Lifecycle, Discovery, Property, Component, Function, Node, Graph
- **UMG Services** (9 services): Lifecycle, Discovery, Hierarchy, Property, Reflection, Widget, Asset, Info, Event
- **Material Services** (2 services): MaterialService, MaterialNodeService
- **Asset Services** (3 services): Discovery, Import, Lifecycle
- **Data Services** (4 services): DataAsset (Discovery, Lifecycle, Property), DataTable (Lifecycle)
- **Enhanced Input Services** (3 services): Actions, Mappings, Reflection
- **Level Actor Services** (1 service): LevelActorService

**All services inherit from `FServiceBase`** which provides:
- Access to `FServiceContext` (shared resources, logging, configuration)
- Validation helpers (`ValidateNotEmpty`, `ValidateNotNull`)
- Logging helpers (`LogInfo`, `LogWarning`, `LogError`)

**Service location**: `Source/VibeUE/Public/Services/<Domain>/`

### Type-Safe Error Handling with TResult<T>

**ALL service operations return `TResult<T>`** instead of throwing exceptions or returning raw pointers:

```cpp
TResult<UBlueprint*> CreateBlueprint(const FString& Name);
TResult<void> CompileBlueprint(UBlueprint* Blueprint);
TResult<bool> HasVariable(UBlueprint* Blueprint, const FString& VarName);
```

**Key benefits**:
- Compile-time error handling enforcement
- Self-documenting error paths
- Railway-oriented programming (chain operations)
- AI-friendly structured error responses

**Common patterns**:
```cpp
// Success
return TResult<T>::Success(value);

// Error with centralized error code
return TResult<T>::Error(ErrorCodes::BLUEPRINT_NOT_FOUND, "Blueprint not found");

// Chaining operations
auto Result = FindBlueprint(Name);
if (Result.IsError()) {
    return TResult<void>::Error(Result.GetErrorCode(), Result.GetErrorMessage());
}
UBlueprint* BP = Result.GetValue();
```

### Centralized Error Codes

**Never use hardcoded error strings**. All errors use constants from `ErrorCodes.h`:

```cpp
namespace VibeUE::ErrorCodes {
    // Parameter Validation (1000-1099)
    constexpr const TCHAR* PARAM_MISSING = TEXT("PARAM_MISSING");

    // Blueprint (2000-2099)
    constexpr const TCHAR* BLUEPRINT_NOT_FOUND = TEXT("BLUEPRINT_NOT_FOUND");

    // Variable (2100-2199)
    constexpr const TCHAR* VARIABLE_NOT_FOUND = TEXT("VARIABLE_NOT_FOUND");

    // Component (2200-2299)
    constexpr const TCHAR* COMPONENT_NOT_FOUND = TEXT("COMPONENT_NOT_FOUND");

    // UMG (3000-3099)
    constexpr const TCHAR* WIDGET_NOT_FOUND = TEXT("WIDGET_NOT_FOUND");

    // Material (7000-7099)
    constexpr const TCHAR* MATERIAL_NOT_FOUND = TEXT("MATERIAL_NOT_FOUND");
}
```

**Error code ranges**:
- 1000-1099: Parameter validation
- 2000-2999: Blueprint domain (subdivided by subdomain)
- 3000-3999: UMG domain
- 4000-4999: Asset domain
- 5000-5999: Enhanced Input
- 6000-6999: Level Actors
- 7000-7999: Materials
- 8000-8999: Data Assets/Tables

### ServiceContext - Shared State

`FServiceContext` is the dependency injection container providing:
- **Unreal Engine subsystems**: `GetWorld()`, `GetEditorEngine()`, `GetAssetRegistry()`
- **Service discovery**: `RegisterService()`, `GetService()`
- **Configuration**: `GetConfigValue()`, `SetConfigValue()`
- **Logging**: Centralized logging with service name prefixes
- **Thread safety**: Uses `FCriticalSection` for concurrent access

Services receive a `TSharedPtr<FServiceContext>` in their constructor and store it for the lifetime of the service.

### Data Transfer Objects (DTOs)

**Type definitions are separated from service implementations** in `Types/` subdirectories to avoid circular dependencies:

**Blueprint DTOs** (`Services/Blueprint/Types/`):
- `BlueprintTypes.h` - FClassInfo, FBlueprintInfo
- `FunctionTypes.h` - FFunctionInfo, FFunctionParameterInfo, FLocalVariableInfo
- `ComponentTypes.h` - FComponentInfo, FComponentEventInfo
- `PropertyTypes.h` - FPropertyInfo
- `GraphTypes.h` - FGraphInfo, FNodeSummary
- `ReflectionTypes.h` - FNodeTypeInfo, FNodeTypeSearchCriteria

**UMG DTOs** (`Services/UMG/Types/`):
- `WidgetTypes.h` - FWidgetInfo, FWidgetComponentInfo
- `PropertyTypes.h` - FWidgetPropertyInfo
- `EventTypes.h` - FWidgetEventInfo

**When adding new types**:
1. Create them in the appropriate `Types/` directory
2. Keep them simple (data + serialization, no business logic)
3. Use USTRUCT() for UE reflection if needed for property access
4. Include JSON serialization helpers (ToJson, FromJson)

### Multi-Action Tool Pattern

VibeUE exposes **14 tools with 177 total actions** instead of 177 individual tools. Each tool is a "hub" for related operations:

```cpp
UFUNCTION()
static FString ManageBlueprint(const FString& Action, const FString& ParamsJson);
```

**Example actions for `manage_blueprint`**:
- `create` - Create new blueprint
- `compile` - Compile blueprint
- `get_info` - Get blueprint metadata
- `set_property` - Set CDO property
- `reparent` - Change parent class
- `diff` - Compare two blueprints

**Benefits**:
- Reduces API surface (14 vs 177)
- Groups related operations logically
- Consistent parameter passing
- Each action documented in help system

**Tool list**:
1. `manage_asset` (9 actions)
2. `manage_blueprint` (7 actions)
3. `manage_blueprint_component` (9 actions)
4. `manage_blueprint_node` (12 actions)
5. `manage_blueprint_function` (11 actions)
6. `manage_blueprint_variable` (6 actions)
7. `manage_umg_widget` (22 actions)
8. `manage_enhanced_input` (19 actions)
9. `manage_level_actors` (21 actions)
10. `manage_material` (26 actions)
11. `manage_material_node` (21 actions)
12. `manage_data_asset` (9 actions)
13. `manage_data_table` (15 actions)
14. `check_unreal_connection` (1 action)

### MCP Integration (Dual Roles)

VibeUE acts as **both MCP client AND server**:

**As MCP Client** (`FMCPClient`):
- Connects to external MCP servers (stdio or HTTP)
- Discovers tools from external sources
- Executes external tools on behalf of AI
- Configuration: `Config/vibeue.mcp.json`

**As MCP Server** (`FMCPServer`):
- Exposes internal 14 tools to external AI clients (VS Code, Claude Desktop, Cursor, Windsurf)
- Streamable HTTP transport (MCP 2025-11-25 spec)
- Server-Sent Events (SSE) for streaming responses
- Default port: 8088, endpoint: `/mcp`
- Configured in Project Settings > Plugins > VibeUE

**Thread-safe design**:
- HTTP server runs on background thread
- Requests queued for game thread processing
- Tool execution happens on game thread (required for UE APIs)
- Results returned via HTTP response or SSE stream

## Key Unreal Engine Patterns

### Blueprint Compilation is Critical

Blueprints cache their node/pin structure. **Always compile after modifications**:

```cpp
// After adding nodes, changing variables, or modifying graphs
Blueprint->PostEditChange();
FKismetEditorUtilities::CompileBlueprint(Blueprint);
```

Without compilation:
- Nodes appear broken with "Unknown" pins in editor
- Property changes don't take effect
- Function signatures remain stale

### Pin Type Resolution

`FEdGraphPinType` requires precise configuration:

```cpp
FEdGraphPinType PinType;
PinType.PinCategory = UEdGraphSchema_K2::PC_Object;  // Or PC_Struct, PC_Int, PC_Float, etc.
PinType.PinSubCategoryObject = UStaticClass;  // UClass* or UScriptStruct*
PinType.ContainerType = EPinContainerType::None;  // Or Array, Set, Map
```

Wrong types cause "Type mismatch" errors impossible to connect.

### UMG Slot vs Widget Properties

**Widget properties** (Text, Color) belong to the `UWidget`.
**Slot properties** (Alignment, Padding) belong to the `UPanelSlot`.

```cpp
// Get widget
UTextBlock* TextWidget = WidgetTree->FindWidget<UTextBlock>(Name);

// Get slot (for layout properties)
UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(TextWidget->Slot);
Slot->SetAlignment(FVector2D(0.5f, 0.5f));
```

### Asset Registry Timing

Asset Registry scans asynchronously. **Check if loading is complete**:

```cpp
IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

if (AssetRegistry.IsLoadingAssets()) {
    AssetRegistry.OnFilesLoaded().AddLambda([this]() {
        // Retry query after loading completes
    });
}
```

### Transaction System for Undo/Redo

**Wrap editor modifications in FScopedTransaction**:

```cpp
FScopedTransaction Transaction(LOCTEXT("CreateVariable", "Create Blueprint Variable"));
Blueprint->Modify();  // Mark for undo
// ... make changes ...
```

Without this, users can't undo operations.

## Build Configuration

### Module Dependencies

**Public dependencies** (exposed in headers):
```
Core, CoreUObject, Engine, InputCore, Networking, Sockets, HTTP,
Json, JsonUtilities, DeveloperSettings, ApplicationCore, WebSockets
```

**Private dependencies** (implementation only):
```
UnrealEd, EditorScriptingUtilities, Slate, SlateCore, UMG, Kismet,
KismetCompiler, BlueprintGraph, AssetRegistry, PropertyEditor,
EnhancedInput, MaterialEditor, UMGEditor, AudioCapture, ...
```

**Plugin dependencies** (required in .uplugin):
- EditorScriptingUtilities
- EnhancedInput
- AudioCapture

### Build Settings

```csharp
PCHUsage = UseExplicitOrSharedPCHs;  // Shared PCH for faster builds
IWYUSupport = IWYUSupport.None;  // Disabled for compatibility
bUseUnity = false;  // Each file compiles independently (faster incremental builds)
```

### Platform Support

- Windows (Win64) - fully supported
- Mac - supported (manual build required)
- Linux - supported (manual build required)

**Editor-only plugin**: Not packaged with shipping builds.

## Common Development Patterns

### Adding a New Service

1. **Create service header** in `Source/VibeUE/Public/Services/<Domain>/`:
```cpp
class VIBEUE_API FMyNewService : public FServiceBase {
public:
    FMyNewService(TSharedPtr<FServiceContext> InContext);

    virtual FString GetServiceName() const override { return TEXT("MyNewService"); }

    TResult<void> DoSomething(const FString& Param);
};
```

2. **Create service implementation** in `Source/VibeUE/Private/Services/<Domain>/`:
```cpp
FMyNewService::FMyNewService(TSharedPtr<FServiceContext> InContext)
    : FServiceBase(InContext) {}

TResult<void> FMyNewService::DoSomething(const FString& Param) {
    auto ValidationResult = ValidateNotEmpty(Param, TEXT("Param"));
    if (ValidationResult.IsError()) {
        return ValidationResult;
    }

    // Implementation
    LogInfo(TEXT("Doing something"));

    return TResult<void>::Success();
}
```

3. **Register service** in module startup:
```cpp
Context->RegisterService(TEXT("MyNewService"), MakeShared<FMyNewService>(Context));
```

### Adding a New Tool Action

1. **Add service method** (see above)

2. **Update command handler** in `Source/VibeUE/Private/Commands/<Domain>Commands.cpp`:
```cpp
TSharedPtr<FJsonObject> FMyCommands::HandleDoSomething(const TSharedPtr<FJsonObject>& Params) {
    FString Param = Params->GetStringField(TEXT("param"));

    auto Result = MyService->DoSomething(Param);
    if (Result.IsError()) {
        return ErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
    }

    return SuccessResponse(TEXT("Operation completed"));
}
```

3. **Add action to tool** in `Source/VibeUE/Private/Tools/EditorTools.cpp`:
```cpp
FString UEditorTools::ManageMyThing(const FString& Action, const FString& ParamsJson) {
    if (Action == TEXT("do_something")) {
        return FMyCommands::HandleDoSomething(ParseJson(ParamsJson))->ToJsonString();
    }
    // ... other actions ...
}
```

4. **Update help text** in `Content/Help/manage_my_thing/do_something.md`

### Working with Reflection

**Finding properties by name**:
```cpp
FProperty* FindProperty(UStruct* Struct, const FString& PropertyName) {
    for (TFieldIterator<FProperty> It(Struct); It; ++It) {
        if (It->GetName() == PropertyName) {
            return *It;
        }
    }
    return nullptr;
}
```

**Getting/setting property values**:
```cpp
void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructInstance);
Property->GetValue_InContainer(StructInstance);
Property->SetValue_InContainer(StructInstance, &NewValue);
```

**Iterating UClass hierarchy**:
```cpp
for (UClass* Class = MyClass; Class; Class = Class->GetSuperClass()) {
    // Process each class in hierarchy
}
```

## Important Conventions

### Branch Strategy

**ALWAYS USE THE DEV BRANCH** for development work. The main branch is for stable releases only.

### File Organization

- **Public headers**: `Source/VibeUE/Public/<Category>/`
- **Private implementation**: `Source/VibeUE/Private/<Category>/`
- **DTOs**: `Source/VibeUE/Public/Services/<Domain>/Types/`
- **Tests**: `test_prompts/<domain>/`
- **Help files**: `Content/Help/<tool_name>/<action>.md`
- **Configuration**: `Config/`

### Naming Conventions

- **Services**: `F<Domain><Purpose>Service` (e.g., `FBlueprintLifecycleService`)
- **DTOs**: `F<Domain><Type>Info` (e.g., `FBlueprintInfo`, `FComponentInfo`)
- **Commands**: `F<Domain>Commands` (e.g., `FBlueprintCommands`)
- **Error codes**: `UPPER_SNAKE_CASE` (e.g., `BLUEPRINT_NOT_FOUND`)
- **Tool names**: `manage_<domain>` (e.g., `manage_blueprint`)
- **Actions**: `lowercase_snake_case` (e.g., `create`, `get_info`)

### Code Style

- **Use TResult for all operations** that can fail
- **Use ErrorCodes constants** for all errors
- **Validate inputs early** with `ValidateNotEmpty`, `ValidateNotNull`
- **Log important operations** with `LogInfo`, `LogWarning`, `LogError`
- **Keep services under 500 lines** - split if growing larger
- **Separate DTOs from services** to avoid circular dependencies
- **Use FScopedTransaction** for undo-able editor operations
- **Compile blueprints** after graph/variable modifications

## Debugging

### Enabling Debug Logging

Set `Debug Mode` in Project Settings > Plugins > VibeUE, or click the gear icon in the AI Chat window.

Debug mode logs:
- All tool calls with parameters
- Service method entry/exit
- Asset Registry queries
- HTTP requests (MCP server)
- Tool execution timing

### Log Files

VibeUE writes detailed log files that are invaluable for debugging chat sessions and LLM interactions:

**Log file locations** (relative to project root):
```
../../Saved/Logs/
├── VibeUE_Chat.log    # Chat session logs (user messages, AI responses, tool calls)
├── VibeUE_RawLLM.log  # Raw LLM API requests/responses (full JSON payloads)
└── <ProjectName>.log  # Standard Unreal Engine log (includes VibeUE debug output)
```

**What's in each log**:
- **VibeUE_Chat.log** - High-level conversation flow, tool execution results, session events
- **VibeUE_RawLLM.log** - Complete HTTP request/response bodies to LLM providers (OpenRouter, VibeUE API)
- **<ProjectName>.log** - All Unreal Engine logging including VibeUE service-level debug messages

**When to check logs**:
- Chat responses are unexpected or incorrect
- Tool calls are failing silently
- LLM provider connectivity issues
- Debugging MCP server requests
- Understanding token usage and context management

Logs are appended during the session and rotated on editor restart.

### Common Issues

**"Missing Modules" error on startup**:
- Run `BuildPlugin.bat` or `buildandlaunch.ps1` to compile the plugin
- Delete `Binaries/` and `Intermediate/` folders and rebuild

**Blueprint compilation fails**:
- Check UE output log for specific errors
- Verify pin types match expected types
- Ensure all node connections are valid

**Asset not found errors**:
- Use full asset paths from `search_items()` results
- Check Asset Registry has finished loading (`IsLoadingAssets()`)
- Verify asset exists in Content Browser

**MCP server not responding**:
- Check port is not in use (default: 8088)
- Verify Unreal Editor is running
- Enable debug mode and check HTTP logs
- Confirm API key matches client configuration

## Resources

- **Documentation**: See `Docs/` directory
- **Examples**: Natural language test prompts in `test_prompts/`
- **Help system**: Built-in help via `get_help()` tool or `Content/Help/` markdown files
- **Copilot guidance**: `.github/copilot-agent-vibeue.md` (advanced C++/UE patterns)
- **Website**: https://www.vibeue.com/
- **Discord**: https://discord.gg/hZs73ST59a
