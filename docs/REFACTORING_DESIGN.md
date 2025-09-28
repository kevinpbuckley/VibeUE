# VibeUE Refactoring Design Document

## Executive Summary

This document outlines a comprehensive refactoring strategy for the VibeUE codebase to improve maintainability, reduce code duplication, and establish a more modular architecture. The current codebase (~26,000 lines across C++ and Python) exhibits significant patterns that can be abstracted and consolidated.

## Current Architecture Analysis

### Codebase Statistics
- **C++ Source Files**: 22 files, ~19,500 lines
- **Python Files**: 26 files, ~7,300 lines  
- **Large Files**: 5 files over 1,000 lines (UMGCommands.cpp: 5,377 lines)
- **JSON Operations**: 452+ TSharedPtr<FJsonObject> usages
- **Error Patterns**: 528+ CreateErrorResponse calls
- **Command Handlers**: 7 distinct command handler classes

### Current Problems

#### 1. Code Duplication
- **Pattern**: Every command class has identical `HandleCommand` structure with large if/else chains
- **Impact**: ~200 lines of duplicate routing logic per class
- **Example**: BlueprintCommands, UMGCommands, AssetCommands all have nearly identical command dispatch patterns

#### 2. Large Monolithic Classes
- **UMGCommands.cpp**: 5,377 lines handling 30+ distinct operations
- **BlueprintCommands.cpp**: 4,082 lines mixing variable management, component handling, and compilation
- **Problem**: Single Responsibility Principle violations, difficult testing and maintenance

#### 3. Repeated JSON Processing
- **Pattern**: Manual JSON parsing and validation in every command
- **Issues**: No type safety, inconsistent error messages, duplicate validation logic
- **Impact**: 100+ lines of similar parsing code per command

#### 4. Tight Coupling
- **Commands directly instantiate Unreal Engine classes**
- **Hard-coded string literals for command types throughout codebase**
- **Direct dependency on specific UE5 APIs without abstraction layers**

#### 5. Inconsistent Error Handling
- **Multiple error response formats across different commands**
- **Inconsistent logging patterns (LogTemp vs LogVibeUE)**
- **No standardized validation framework**

## Proposed Architecture

### 1. Command Framework Architecture

```cpp
// Base command interface
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params) = 0;
    virtual FString GetCommandName() const = 0;
    virtual bool ValidateParams(const TSharedPtr<FJsonObject>& Params, FString& OutError) const = 0;
};

// Base command implementation with common functionality
class VIBEUE_API FBaseCommand : public ICommand {
protected:
    TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr);
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Error);
    void LogCommandExecution(const FString& CommandName, bool bSuccess, const FString& Details = "");
    
    // Template-based parameter extraction with validation
    template<typename T>
    bool GetParam(const TSharedPtr<FJsonObject>& Params, const FString& Key, T& OutValue, FString& OutError) const;
};

// Command registry for automatic registration and dispatch
class VIBEUE_API FCommandRegistry {
public:
    static void RegisterCommand(TSharedPtr<ICommand> Command);
    static TSharedPtr<ICommand> GetCommand(const FString& CommandName);
    static TArray<FString> GetAllCommandNames();
    
private:
    static TMap<FString, TSharedPtr<ICommand>> Commands;
};
```

### 2. Modular Command Structure

#### Blueprint Module
```cpp
namespace BlueprintCommands {
    class FCreateBlueprintCommand : public FBaseCommand { /* ... */ };
    class FCompileBlueprintCommand : public FBaseCommand { /* ... */ };
    class FManageVariablesCommand : public FBaseCommand { /* ... */ };
    class FManageComponentsCommand : public FBaseCommand { /* ... */ };
}
```

#### UMG Module  
```cpp
namespace UMGCommands {
    class FCreateWidgetCommand : public FBaseCommand { /* ... */ };
    class FAddComponentCommand : public FBaseCommand { /* ... */ };
    class FSetPropertyCommand : public FBaseCommand { /* ... */ };
    class FLayoutManagementCommand : public FBaseCommand { /* ... */ };
}
```

### 3. Shared Utilities Layer

#### JSON Processing Framework
```cpp
class VIBEUE_API FParameterValidator {
public:
    static bool ValidateRequired(const TSharedPtr<FJsonObject>& Params, const TArray<FString>& RequiredFields, FString& OutError);
    static bool ValidateTypes(const TSharedPtr<FJsonObject>& Params, const TMap<FString, FString>& TypeMap, FString& OutError);
    static bool ExtractVector2D(const TSharedPtr<FJsonObject>& Params, const FString& Key, FVector2D& OutValue, FString& OutError);
    // ... more type-safe extractors
};

class VIBEUE_API FResponseBuilder {
public:
    static TSharedPtr<FJsonObject> Success(const TSharedPtr<FJsonObject>& Data = nullptr);
    static TSharedPtr<FJsonObject> Error(const FString& Message, const FString& Code = "GENERIC_ERROR");
    static TSharedPtr<FJsonObject> ValidationError(const TArray<FString>& FieldErrors);
    static TSharedPtr<FJsonObject> NotFoundError(const FString& ResourceType, const FString& ResourceName);
};
```

#### Engine Abstraction Layer
```cpp
class VIBEUE_API FBlueprintService {
public:
    static UBlueprint* FindBlueprint(const FString& Name);
    static bool CompileBlueprint(UBlueprint* Blueprint, FString& OutError);
    static bool AddVariable(UBlueprint* Blueprint, const FBlueprintVariableData& VariableData, FString& OutError);
    // ... other blueprint operations
};

class VIBEUE_API FWidgetService {
public:
    static UWidgetBlueprint* FindWidget(const FString& Name);
    static bool AddComponent(UWidgetBlueprint* Widget, const FWidgetComponentData& ComponentData, FString& OutError);
    static bool SetProperty(UWidget* Widget, const FString& PropertyPath, const FString& Value, FString& OutError);
    // ... other widget operations
};
```

### 4. Configuration-Driven Command Registration

```json
// Commands.json - Configuration file for command metadata
{
  "commands": {
    "create_blueprint": {
      "handler": "BlueprintCommands::FCreateBlueprintCommand",
      "description": "Creates a new Blueprint class",
      "required_params": ["name", "parent_class"],
      "optional_params": ["path", "components"],
      "category": "blueprint"
    },
    "create_umg_widget_blueprint": {
      "handler": "UMGCommands::FCreateWidgetCommand", 
      "description": "Creates a new UMG Widget Blueprint",
      "required_params": ["name"],
      "optional_params": ["path", "parent_widget"],
      "category": "umg"
    }
  }
}
```

### 5. Plugin Architecture for Extensibility

```cpp
class VIBEUE_API ICommandPlugin {
public:
    virtual ~ICommandPlugin() = default;
    virtual void RegisterCommands(FCommandRegistry& Registry) = 0;
    virtual FString GetPluginName() const = 0;
    virtual FString GetVersion() const = 0;
};

class VIBEUE_API FBlueprintCommandPlugin : public ICommandPlugin {
public:
    void RegisterCommands(FCommandRegistry& Registry) override;
    // ...
};
```

## Implementation Roadmap

### Phase 1: Foundation (Week 1-2)
- [x] Analyze current architecture and identify duplication patterns  
- [ ] Create base command framework (`FBaseCommand`, `ICommand`)
- [ ] Implement `FCommandRegistry` for centralized command management
- [ ] Create shared utilities (`FParameterValidator`, `FResponseBuilder`)
- [ ] Establish logging and error handling standards

### Phase 2: Command Extraction (Week 3-4)
- [ ] Extract Blueprint commands to individual command classes
- [ ] Extract UMG commands to individual command classes  
- [ ] Extract Asset commands to individual command classes
- [ ] Create service abstraction layers (`FBlueprintService`, `FWidgetService`)
- [ ] Migrate command registration to use registry pattern

### Phase 3: Integration (Week 5)
- [ ] Update `Bridge.cpp` to use `FCommandRegistry` instead of if/else chains
- [ ] Implement configuration-driven command metadata
- [ ] Add comprehensive parameter validation to all commands
- [ ] Standardize error responses and logging across all commands

### Phase 4: Advanced Features (Week 6)
- [ ] Implement plugin architecture for extensible commands
- [ ] Add command pipeline support (pre/post processing hooks)
- [ ] Create command composition framework for complex workflows
- [ ] Add performance monitoring and metrics collection

### Phase 5: Python Integration (Week 7)
- [ ] Refactor Python MCP server to use modular tool registration
- [ ] Extract common MCP patterns into reusable base classes
- [ ] Standardize Python-side error handling and logging
- [ ] Create consistent parameter validation framework

### Phase 6: Testing & Documentation (Week 8)
- [ ] Create comprehensive unit tests for command framework
- [ ] Add integration tests for command registration and execution
- [ ] Update documentation to reflect new architecture
- [ ] Create developer guide for adding new commands

## Expected Benefits

### Maintainability Improvements
- **80% reduction** in duplicate code across command handlers
- **Centralized command registration** eliminates scattered if/else chains
- **Consistent error handling** and logging across entire codebase
- **Modular architecture** enables independent testing and development

### Code Quality Improvements
- **Type-safe parameter handling** eliminates manual JSON parsing errors
- **Single Responsibility Principle** compliance with focused command classes  
- **Dependency Injection** support through service abstraction layers
- **Configuration-driven** command metadata reduces hard-coded strings

### Developer Experience Improvements
- **Plugin architecture** enables easy extension without core changes
- **Comprehensive logging** and error messages aid debugging
- **Standardized patterns** reduce learning curve for new contributors
- **Automated testing** framework ensures reliability

### Performance Improvements
- **Command registry** enables O(1) command lookup vs O(n) if/else chains
- **Reduced memory allocation** through shared utility classes
- **Lazy loading** of command instances only when needed
- **Caching** of frequently accessed Unreal Engine objects

## Migration Strategy

### Backward Compatibility
- Maintain existing command names and parameter formats during transition
- Create adapter layer to support legacy Python MCP clients
- Gradual migration approach - new commands use new framework immediately
- Deprecation warnings for old patterns with migration guide

### Risk Mitigation
- **Incremental rollout** - migrate one command category at a time
- **Comprehensive testing** at each phase to prevent regressions
- **Feature flags** to enable/disable new architecture during development  
- **Rollback plan** to revert to previous architecture if critical issues arise

### Success Metrics
- **Lines of Code**: Target 30% reduction in overall codebase size
- **Cyclomatic Complexity**: Reduce average method complexity by 50%
- **Test Coverage**: Achieve 80% code coverage for command framework
- **Development Velocity**: Measure time to implement new commands (target 50% reduction)

## Conclusion

This refactoring will transform VibeUE from a monolithic, tightly-coupled system into a modular, extensible architecture that follows modern software engineering principles. The investment in this refactoring will pay dividends in reduced maintenance costs, improved reliability, and faster feature development.

The phased approach ensures minimal disruption to existing functionality while providing immediate benefits as each phase completes. The resulting architecture will position VibeUE as a more robust and maintainable solution for Unreal Engine automation.