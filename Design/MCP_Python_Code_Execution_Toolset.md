# MCP Python Code Execution Toolset - Design Document

## Executive Summary

Create a new MCP-based toolset that enables LLMs to discover and execute Python code within Unreal Engine 5.7. This design follows the Cloudflare Code Mode pattern where LLMs generate executable Python scripts rather than compose tool calls directly. The toolset will provide runtime API discovery, in-memory script execution, and automatic cleanup.

**Key Design Principles:**
- Generate code, don't compose tool calls (Code Mode pattern)
- Trusted execution within UE (no disk I/O, only unreal module APIs)
- Hybrid discovery (runtime introspection + source code reading)
- Automatic cleanup (ephemeral scripts)
- Follow existing VibeUE architecture patterns

---

## 1. Architecture Overview

### Tool Name: `manage_python_execution`

**Actions:**

**Runtime Discovery:**
1. `discover_module` - Introspect the `unreal` module to discover classes, functions, signatures
2. `discover_class` - Get detailed information about a specific UE class
3. `discover_function` - Get signature and documentation for a function
4. `list_subsystems` - List available editor subsystems (for `get_editor_subsystem()`)

**Code Execution:**
5. `execute_code` - Execute Python code string with output capture
6. `evaluate_expression` - Evaluate Python expression and return result

**Source Code Access (for understanding UE Python API patterns):**
9. `read_source_file` - Read a specific source file from UE Python plugin
10. `search_source_files` - Search for patterns in UE Python plugin source
11. `list_source_files` - List available source files in UE Python plugin

**Examples & Help:**
7. `get_examples` - Return curated example scripts for common tasks
8. `help` - Get help documentation for available actions

### Execution Flow

```
External MCP Client (Claude Code, IDE, etc.)
    ↓
1. Discovery: discover_module → returns {classes: [...], functions: [...]}
2. Introspection: discover_class → returns {methods: [...], properties: [...]}
3. Code Generation: LLM writes Python script using unreal APIs
4. Execution: execute_code → returns {success: true, output: "..."}
    ↓
VibeUE: ManagePythonExecution tool
    ↓
PythonCommands handler routes to services
    ↓
Services: PythonDiscoveryService, PythonExecutionService
    ↓
IPythonScriptPlugin::ExecPythonCommandEx()
    ↓
Embedded Python interpreter executes in-memory code
    ↓
Returns: stdout, result, errors (automatic cleanup)
```

---

## 2. Service Layer Architecture

### A. Python Types (`Services/Python/Types/PythonTypes.h`)

**Data Structures:**
```cpp
struct FPythonFunctionInfo {
    FString Name;
    FString Signature;        // e.g., "load_asset(path: str) -> Object"
    FString Docstring;
    TArray<FString> Parameters;
    TArray<FString> ParamTypes;
    FString ReturnType;
    bool bIsMethod, bIsStatic, bIsClassMethod;
};

struct FPythonClassInfo {
    FString Name;
    FString FullPath;         // e.g., "unreal.EditorActorSubsystem"
    FString Docstring;
    TArray<FString> BaseClasses;
    TArray<FPythonFunctionInfo> Methods;
    TArray<FString> Properties;
};

struct FPythonModuleInfo {
    FString ModuleName;
    TArray<FString> Classes;
    TArray<FString> Functions;
    TArray<FString> Constants;
    int32 TotalMembers;
};

struct FPythonExecutionResult {
    bool bSuccess;
    FString Output;           // stdout from print statements
    FString Result;           // Return value (EvaluateStatement mode)
    FString ErrorMessage;     // Exception traceback
    TArray<FString> LogMessages;
    float ExecutionTimeMs;
};

struct FPythonExampleScript {
    FString Title;
    FString Description;
    FString Category;         // "Asset Management", "Blueprint Editing"
    FString Code;
    TArray<FString> Tags;
};
```

### B. Python Discovery Service

**Purpose:** Runtime introspection of the `unreal` module using Python's `inspect` module

**Key Methods:**
```cpp
class FPythonDiscoveryService : public FServiceBase {
    // Discover all members of unreal module
    TResult<FPythonModuleInfo> DiscoverUnrealModule(
        int32 MaxDepth = 1,
        const FString& Filter = FString()
    );

    // Get detailed class information
    TResult<FPythonClassInfo> DiscoverClass(const FString& ClassName);

    // Get function signature and documentation
    TResult<FPythonFunctionInfo> DiscoverFunction(const FString& FunctionPath);

    // List all editor subsystems
    TResult<TArray<FString>> ListEditorSubsystems();

    // Search API by pattern
    TResult<TArray<FString>> SearchAPI(
        const FString& SearchPattern,
        const FString& SearchType = TEXT("all")
    );

private:
    // Execute introspection Python scripts and parse JSON results
    TResult<FString> ExecuteIntrospectionScript(const FString& PythonCode);

    // Cache discovered classes
    TMap<FString, FPythonClassInfo> ClassCache;
};
```

**Implementation Pattern:**
- Generates Python scripts that use `inspect.getmembers()`, `inspect.signature()`, etc.
- Executes via `IPythonScriptPlugin::ExecPythonCommandEx()`
- Returns results as JSON, parses into C++ structures
- Caches results per editor session (module is stable)

**Example Introspection Script:**
```python
import unreal
import inspect
import json

result = {"classes": [], "functions": []}
for name, obj in inspect.getmembers(unreal):
    if inspect.isclass(obj):
        result["classes"].append({
            "name": name,
            "docstring": inspect.getdoc(obj) or "",
            "methods": [m[0] for m in inspect.getmembers(obj, inspect.ismethod)]
        })
print(json.dumps(result))
```

### C. Python Execution Service

**Purpose:** Execute LLM-generated Python code with output capture and error handling

**Key Methods:**
```cpp
class FPythonExecutionService : public FServiceBase {
    // Execute multi-line Python script
    TResult<FPythonExecutionResult> ExecuteCode(
        const FString& Code,
        EPythonFileExecutionScope ExecutionScope = EPythonFileExecutionScope::Private,
        int32 TimeoutMs = 30000
    );

    // Evaluate single expression and return result
    TResult<FPythonExecutionResult> EvaluateExpression(const FString& Expression);

    // Execute with optional validation
    TResult<FPythonExecutionResult> ExecuteCodeSafe(
        const FString& Code,
        bool bValidateBeforeExecution = false
    );

    // Check Python availability
    TResult<bool> IsPythonAvailable();

    // Get Python version and interpreter info
    TResult<FString> GetPythonInfo();

private:
    // Convert FPythonCommandEx to FPythonExecutionResult
    FPythonExecutionResult ConvertExecutionResult(
        const FPythonCommandEx& CommandEx,
        float ExecutionTimeMs
    );

    // Optional validation for dangerous patterns
    TResult<void> ValidateCode(const FString& Code);

    // Parse exception tracebacks
    FString ParsePythonException(const FString& Traceback);
};
```

**Security Model (Trusted Execution):**
- No disk-based script files (in-memory execution only)
- Full access to `unreal` module APIs
- Allow standard library imports (inspect, json, sys, etc.)
- Optional validation (configurable):
  - Level 1: No validation (default, fully trusted)
  - Level 2: Warn on dangerous patterns (log only)
  - Level 3: Block dangerous patterns (subprocess, os.system, file I/O)

**Execution Using UE APIs:**
```cpp
FPythonCommandEx Command;
Command.Command = Code;
Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
Command.FileExecutionScope = EPythonFileExecutionScope::Private;
Command.Flags = EPythonCommandFlags::None;

bool bSuccess = IPythonScriptPlugin::Get()->ExecPythonCommandEx(Command);

// Parse Command.CommandResult and Command.LogOutput
// Return FPythonExecutionResult
```

### D. Python Schema Service

**Purpose:** Generate JSON schemas and curated examples for LLMs

**Key Methods:**
```cpp
class FPythonSchemaService : public FServiceBase {
    // Generate JSON schema for a class
    TResult<FString> GenerateClassSchema(const FPythonClassInfo& ClassInfo);

    // Generate function signature
    TResult<FString> GenerateFunctionSignature(const FPythonFunctionInfo& FuncInfo);

    // Generate comprehensive API documentation
    TResult<FString> GenerateAPIDocumentation(
        const FPythonModuleInfo& ModuleInfo,
        bool bIncludeExamples = true
    );

    // Get curated example scripts
    TResult<TArray<FPythonExampleScript>> GetExampleScripts(
        const FString& Category = FString()
    );

private:
    // Convert Python types to JSON schema types
    FString ConvertPythonTypeToJsonType(const FString& PythonType);

    // Load/initialize example scripts
    void InitializeExamples();
    TArray<FPythonExampleScript> ExampleScripts;
};
```

**Example Scripts (Pre-populated):**
```cpp
// Asset Management
{
  .Title = "Load and Modify Asset",
  .Code = "import unreal\n"
          "asset = unreal.load_asset('/Game/Blueprints/BP_MyActor')\n"
          "default_obj = asset.get_default_object()\n"
          "component = default_obj.get_component_by_class(unreal.StaticMeshComponent)\n"
          "component.set_editor_property('Mass', 100.0)\n"
          "print(f'Updated {asset.get_name()}')"
}

// Level Editing
{
  .Title = "Spawn Actor in Level",
  .Code = "import unreal\n"
          "subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n"
          "actor_class = unreal.load_class(None, '/Game/BP.BP_C')\n"
          "location = unreal.Vector(0, 0, 100)\n"
          "actor = subsys.spawn_actor_from_class(actor_class, location)\n"
          "print(f'Spawned: {actor.get_actor_label()}')"
}
```

---

## 3. Command Handler Layer

### Python Commands (`Commands/PythonCommands.h`)

**Purpose:** Route actions to appropriate services and convert between JSON and C++ types

**Structure:**
```cpp
class FPythonCommands {
    TSharedPtr<FJsonObject> HandleCommand(
        const FString& CommandType,
        const TSharedPtr<FJsonObject>& Params
    );

private:
    // Action handlers
    TSharedPtr<FJsonObject> HandleDiscoverModule(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDiscoverClass(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDiscoverFunction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListSubsystems(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleExecuteCode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleEvaluateExpression(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetExamples(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleHelp(const TSharedPtr<FJsonObject>& Params);

    // Service instances
    TSharedPtr<FPythonDiscoveryService> DiscoveryService;
    TSharedPtr<FPythonExecutionService> ExecutionService;
    TSharedPtr<FPythonSchemaService> SchemaService;
};
```

**Response Format:**
```json
{
  "success": true,
  "data": {
    "output": "Spawned actor: BP_TestActor",
    "result": "None",
    "execution_time_ms": 125.3
  }
}
```

---

## 4. Tool Integration

### Add to EditorTools

**Declaration (`Tools/EditorTools.h`):**
```cpp
/**
 * Manage Python code execution with runtime API discovery
 */
UFUNCTION()
static FString ManagePythonExecution(const FString& Action, const FString& ParamsJson);
```

**Implementation (`Tools/EditorTools.cpp`):**
```cpp
static TSharedPtr<FPythonCommands> PythonCommandsInstance;

FString UEditorTools::ManagePythonExecution(const FString& Action, const FString& ParamsJson)
{
    EnsureCommandHandlersInitialized();
    TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
    Params->SetStringField(TEXT("action"), Action);
    return SerializeResult(PythonCommandsInstance->HandleCommand(
        TEXT("manage_python_execution"), Params));
}
```

**Tool Registration:**
```cpp
REGISTER_VIBEUE_TOOL(manage_python_execution,
    "Execute Python code in Unreal Engine with runtime API discovery",
    "Python",
    TOOL_PARAMS(
        TOOL_PARAM("Action", "The action to perform", "string", true),
        TOOL_PARAM("ParamsJson", "JSON parameters for the action", "string", false)
    ),
    {
        FString Action = Params.FindRef(TEXT("Action"));
        FString ParamsJson = Params.FindRef(TEXT("ParamsJson"));
        return UEditorTools::ManagePythonExecution(Action, ParamsJson);
    }
);
```

**Add Dependency (`VibeUE.Build.cs`):**
```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    // ... existing ...
    "PythonScriptPlugin"
});
```

---

## 5. Error Handling

### Error Code Range: 9100-9199 (Python Operations)

**Add to `Core/ErrorCodes.h`:**
```cpp
namespace VibeUE::ErrorCodes {
    // Python Errors (9100-9199)
    constexpr const TCHAR* PYTHON_NOT_AVAILABLE = TEXT("PYTHON_NOT_AVAILABLE");
    constexpr const TCHAR* PYTHON_SYNTAX_ERROR = TEXT("PYTHON_SYNTAX_ERROR");
    constexpr const TCHAR* PYTHON_RUNTIME_ERROR = TEXT("PYTHON_RUNTIME_ERROR");
    constexpr const TCHAR* PYTHON_EXECUTION_TIMEOUT = TEXT("PYTHON_EXECUTION_TIMEOUT");
    constexpr const TCHAR* PYTHON_MODULE_NOT_FOUND = TEXT("PYTHON_MODULE_NOT_FOUND");
    constexpr const TCHAR* PYTHON_CLASS_NOT_FOUND = TEXT("PYTHON_CLASS_NOT_FOUND");
    constexpr const TCHAR* PYTHON_FUNCTION_NOT_FOUND = TEXT("PYTHON_FUNCTION_NOT_FOUND");
    constexpr const TCHAR* PYTHON_INTROSPECTION_FAILED = TEXT("PYTHON_INTROSPECTION_FAILED");
    constexpr const TCHAR* PYTHON_UNSAFE_CODE = TEXT("PYTHON_UNSAFE_CODE");
    constexpr const TCHAR* PYTHON_INVALID_EXPRESSION = TEXT("PYTHON_INVALID_EXPRESSION");
}
```

**Error Scenarios:**
1. **Python Not Available** - PythonScriptPlugin not enabled
2. **Syntax Error** - Parse `FPythonCommandEx.CommandResult` for `SyntaxError`
3. **Runtime Exception** - Parse log output for exception tracebacks
4. **Timeout** - Execution exceeds configured timeout (default 30s)
5. **Class/Function Not Found** - Introspection returns no results

---

## 6. Discovery Mechanism

### Source Code Locations for API Discovery

**Unreal Python Plugin Source:**
- **C++ Headers:** `E:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Experimental\PythonScriptPlugin\Source\PythonScriptPlugin\`
  - `Public\IPythonScriptPlugin.h` - Main plugin interface
  - `Public\PythonScriptTypes.h` - Execution types and enums
  - `Private\PyCore.h` - Core Python wrappers
  - `Private\PyWrapperObject.h` - Object wrapper implementation
  - `Private\PyConversion.h` - UE ↔ Python type conversions

**Python Module Source:**
- **Python Scripts:** `E:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python\`
  - `unreal_core.py` - Core Python module setup (imports `_unreal_core`)
  - `remote_execution.py` - Remote execution framework
  - `test_wrapper_types.py` - Test examples showing API usage

**The `unreal` Module:**
- Generated at runtime from UE's C++ reflection system (UCLASS, UPROPERTY, UFUNCTION)
- NOT available as static files (dynamically created)
- Discoverable via Python's `inspect` module at runtime

### Hybrid Discovery Strategy

**1. Runtime Introspection (Primary):**
```python
import unreal
import inspect

# Discover classes
for name, obj in inspect.getmembers(unreal):
    if inspect.isclass(obj):
        methods = inspect.getmembers(obj, inspect.ismethod)
        signature = inspect.signature(method)
        docstring = inspect.getdoc(obj)
```

**2. Source Code Reading (Secondary):**

LLMs need dedicated tools to read UE Python plugin source code for understanding API patterns and usage examples.

**Required Tools (New - Add to `manage_python_execution`):**

9. `read_source_file` - Read a specific source file from UE Python plugin
   ```json
   {
     "action": "read_source_file",
     "params": {
       "file_path": "Public/IPythonScriptPlugin.h",  // Relative to plugin source root
       "start_line": 0,     // Optional: start line (default: 0)
       "max_lines": 100     // Optional: max lines to read (default: 1000)
     }
   }
   ```

10. `search_source_files` - Search for patterns in UE Python plugin source
    ```json
    {
      "action": "search_source_files",
      "params": {
        "pattern": "EditorActorSubsystem",  // Regex pattern to search
        "file_pattern": "*.h",               // File glob (default: "*.h,*.cpp,*.py")
        "context_lines": 3                   // Lines of context (default: 3)
      }
    }
    ```

11. `list_source_files` - List available source files in UE Python plugin
    ```json
    {
      "action": "list_source_files",
      "params": {
        "directory": "Public",     // Optional: subdirectory to list
        "pattern": "*.h"           // Optional: file pattern filter
      }
    }
    ```

**Source File Access Patterns:**
- **C++ Headers:** Read to understand type mappings (e.g., `FVector` → `unreal.Vector`)
  - `Public\IPythonScriptPlugin.h` - Main API interface
  - `Public\PythonScriptTypes.h` - Enum and struct definitions
  - `Private\PyWrapperObject.h` - How UObjects are wrapped

- **Python Examples:** Read to see usage patterns
  - `Content\Python\test_wrapper_types.py` - Example API calls
  - `Content\Python\unreal_core.py` - Module initialization

- **Search Use Cases:**
  - Find all classes that inherit from a base class
  - Search for function signatures
  - Find usage examples for specific APIs

**Implementation in PythonDiscoveryService:**
```cpp
class FPythonDiscoveryService : public FServiceBase {
    // ... existing methods ...

    /**
     * Read a source file from UE Python plugin
     * @param RelativePath Relative path from plugin source root
     */
    TResult<FString> ReadSourceFile(
        const FString& RelativePath,
        int32 StartLine = 0,
        int32 MaxLines = 1000
    );

    /**
     * Search for pattern in UE Python plugin source files
     * @param Pattern Regex pattern to search
     * @param FilePattern Glob pattern for files (e.g., "*.h")
     */
    TResult<TArray<FSourceSearchResult>> SearchSourceFiles(
        const FString& Pattern,
        const FString& FilePattern = TEXT("*.h,*.cpp,*.py"),
        int32 ContextLines = 3
    );

    /**
     * List source files in UE Python plugin directory
     * @param SubDirectory Optional subdirectory (e.g., "Public", "Private")
     */
    TResult<TArray<FString>> ListSourceFiles(
        const FString& SubDirectory = FString(),
        const FString& FilePattern = TEXT("*")
    );

private:
    /** Get the UE Python plugin source root path */
    FString GetPluginSourceRoot() const;

    /** Validate that file path is within plugin source (security) */
    bool IsValidSourcePath(const FString& Path) const;
};
```

**Security Considerations:**
- Restrict file reading to UE Python plugin directory only
- Validate paths to prevent directory traversal attacks
- No write access to source files (read-only)

**3. Curated Examples (Tertiary):**
- Pre-populated example scripts in `FPythonSchemaService`
- Common patterns: asset loading, blueprint editing, actor spawning

### Caching Strategy

**Cache per editor session:**
```cpp
TMap<FString, FPythonClassInfo> ClassCache;
TMap<FString, FPythonModuleInfo> ModuleCache;
```

**Cache invalidation:** Only on Python module reload (rare)

**Benefits:**
- Avoid expensive introspection (thousands of classes)
- Module is stable during editor session
- Significant performance improvement

---

## 7. Implementation Steps

### Phase 1: Core Infrastructure
1. Create `Services/Python/Types/PythonTypes.h` - Define all data structures
2. Create `Services/Python/PythonExecutionService.h/.cpp` - Implement code execution
3. Create `Core/ErrorCodes.h` additions - Add Python error codes
4. Add `PythonScriptPlugin` dependency to `VibeUE.Build.cs`

### Phase 2: Discovery System
5. Create `Services/Python/PythonDiscoveryService.h/.cpp` - Implement runtime introspection
6. Create `Services/Python/PythonSchemaService.h/.cpp` - Implement schema generation and examples

### Phase 3: Command Layer
7. Create `Commands/PythonCommands.h/.cpp` - Implement command handler
8. Update `Tools/EditorTools.h/.cpp` - Add ManagePythonExecution UFUNCTION

### Phase 4: Tool Registration
9. Register tool in `FToolRegistry` (follow existing pattern)
10. Verify MCP server exposes new tool

### Phase 5: Testing
11. Unit tests for services
12. Integration tests for command handler
13. MCP end-to-end tests with external client
14. Manual testing with Claude Code

---

## 8. Critical Files to Create/Modify

### New Files (Create)
1. `Plugins/VibeUE/Source/VibeUE/Public/Services/Python/Types/PythonTypes.h`
2. `Plugins/VibeUE/Source/VibeUE/Public/Services/Python/PythonExecutionService.h`
3. `Plugins/VibeUE/Source/VibeUE/Private/Services/Python/PythonExecutionService.cpp`
4. `Plugins/VibeUE/Source/VibeUE/Public/Services/Python/PythonDiscoveryService.h`
5. `Plugins/VibeUE/Source/VibeUE/Private/Services/Python/PythonDiscoveryService.cpp`
6. `Plugins/VibeUE/Source/VibeUE/Public/Services/Python/PythonSchemaService.h`
7. `Plugins/VibeUE/Source/VibeUE/Private/Services/Python/PythonSchemaService.cpp`
8. `Plugins/VibeUE/Source/VibeUE/Public/Commands/PythonCommands.h`
9. `Plugins/VibeUE/Source/VibeUE/Private/Commands/PythonCommands.cpp`

### Existing Files (Modify)
10. `Plugins/VibeUE/Source/VibeUE/Public/Tools/EditorTools.h` - Add ManagePythonExecution declaration
11. `Plugins/VibeUE/Source/VibeUE/Private/Tools/EditorTools.cpp` - Add implementation and registration
12. `Plugins/VibeUE/Source/VibeUE/Public/Core/ErrorCodes.h` - Add Python error codes (9100-9199)
13. `Plugins/VibeUE/Source/VibeUE/VibeUE.Build.cs` - Add PythonScriptPlugin dependency

---

## 9. Usage Example

### External LLM (Claude Code) Workflow

**Step 1: Discover Available APIs**
```json
{
  "tool": "manage_python_execution",
  "action": "discover_module",
  "params": {"filter": "Editor"}
}
```
Returns: List of editor-related classes and functions

**Step 2: Get Class Details**
```json
{
  "tool": "manage_python_execution",
  "action": "discover_class",
  "params": {"class_name": "EditorActorSubsystem"}
}
```
Returns: Methods like `spawn_actor_from_class`, `get_all_level_actors`, etc.

**Step 3: Generate and Execute Code**
```json
{
  "tool": "manage_python_execution",
  "action": "execute_code",
  "params": {
    "code": "import unreal\nsubsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\nactors = subsys.get_all_level_actors()\nprint(f'Found {len(actors)} actors')",
    "scope": "private",
    "timeout_ms": 5000
  }
}
```
Returns: `{success: true, output: "Found 42 actors", execution_time_ms: 123}`

---

## 10. Security and Validation

### Trusted Execution Model

**Allowed:**
- Full `unreal` module access (all UE APIs)
- Standard library imports (inspect, json, sys, etc.)
- In-memory code execution
- Asset loading/modification
- Editor subsystem access

**Blocked (Optional, Configurable):**
- Disk-based file writes (no script persistence)
- External Python package installation (no pip)
- Network operations (unless explicitly needed)
- Subprocess execution
- OS-level commands

**Validation Levels:**
```cpp
enum class EPythonValidationLevel : uint8 {
    None,        // Fully trusted (default)
    Warn,        // Log warnings for dangerous patterns
    Block        // Reject execution of dangerous patterns
};
```

**Configuration:**
Project Settings → Plugins → VibeUE → Python Execution → Validation Level

---

## 11. Performance Considerations

### Caching
- Cache discovered class info per editor session
- Cache module info (invalidate on reload)
- Lazy initialization of services

### Timeouts
- Default: 30 seconds
- Configurable per execution
- Prevent infinite loops/hangs

### Rate Limiting (Optional)
- Max 60 executions per minute per client
- Prevent abuse/overload
- Configurable in project settings

---

## 12. Future Enhancements

### V1 (Current Design)
- Single response after execution completes
- JSON-based results
- Manual discovery (LLM must request)

### V2 (Future)
- Streaming output via Server-Sent Events (SSE)
- Progress updates for long-running scripts
- Automatic API change detection
- Python debugging integration (debugpy)
- Transaction support for undo/redo

---

## Summary

This design provides a comprehensive, production-ready architecture for Python code execution in Unreal Engine following the Cloudflare Code Mode pattern. The implementation:

1. **Follows VibeUE patterns** - Services → Commands → Tools, TResult<T>, ErrorCodes
2. **Enables LLM code generation** - Discover APIs, generate scripts, execute in-memory
3. **Provides trusted execution** - No disk I/O, only unreal module APIs
4. **Supports hybrid discovery** - Runtime introspection + source code reading
5. **Ensures automatic cleanup** - Ephemeral in-memory scripts
6. **Exposes via MCP** - External clients can discover and use Python capabilities

The toolset will integrate seamlessly with the existing VibeUE MCP server and provide powerful code execution capabilities for AI agents to automate Unreal Engine workflows.
