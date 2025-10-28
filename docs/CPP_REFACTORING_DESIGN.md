# VibeUE C++ Refactoring Design Document

**Document Version:** 1.0  
**Date:** October 27, 2025  
**Status:** Proposal  
**Target:** Unreal Engine Marketplace Release

---

## Executive Summary

This document proposes a comprehensive refactoring of the VibeUE C++ codebase to improve **maintainability**, **consistency**, **testability**, and **adherence to Unreal Engine best practices**. The current implementation (26,000+ lines across 14 files) suffers from inconsistent naming, duplicated error handling, massive file sizes, and unclear separation of concerns.

**Key Goals:**
1. **Reduce complexity** - Break down 5,000+ line files into focused, single-responsibility modules
2. **Standardize patterns** - Consistent naming (Services vs Commands vs Reflection), error handling, and response formatting
3. **Improve testability** - Introduce dependency injection and result types for unit testing
4. **Enhance performance** - Add caching strategies and async operation support
5. **Marketplace readiness** - Professional code quality with comprehensive documentation

---

## Current State Analysis

### File Size Distribution

| File | Lines | Status | Issues |
|------|-------|--------|--------|
| BlueprintNodeCommands.cpp | 5,453 | 🔴 Critical | God object, needs 5+ file split |
| UMGCommands.cpp | 5,163 | 🔴 Critical | Mixed concerns, poor separation |
| BlueprintReflection.cpp | 4,054 | 🔴 Critical | Duplicate logic with BlueprintCommands |
| BlueprintCommands.cpp | 4,007 | 🔴 Critical | 80 methods, unclear organization |
| BlueprintVariableReflectionServices.cpp | 2,281 | 🟡 Warning | Good structure but inconsistent with others |
| BlueprintComponentReflection.cpp | 2,201 | 🟡 Warning | Needs alignment with pattern |
| CommonUtils.cpp | 1,378 | 🟢 Acceptable | Utility class, reasonable size |
| AssetCommands.cpp | 578 | 🟢 Good | Focused scope |
| UMGReflectionCommands.cpp | 514 | 🟢 Good | Clean implementation |

**Total:** ~26,000 lines of C++ implementation

### Identified Problems

#### 1. **Inconsistent Naming Conventions**

**Problem:** Three different naming patterns coexist without clear distinction:
- `*Commands` (BlueprintCommands, UMGCommands, AssetCommands)
- `*Reflection` (BlueprintReflection, BlueprintComponentReflection, UMGReflectionCommands)
- `*ReflectionServices` (BlueprintVariableReflectionServices - only one!)

**Impact:**
- Developers cannot predict file names
- Unclear when to use Commands vs Reflection vs Services
- BlueprintVariableReflectionServices has superior architecture (6 service classes) but isn't applied elsewhere

**Example:**
```cpp
// Current chaos:
FBlueprintCommands          // 4,000 lines, everything Blueprint
FBlueprintReflection        // 4,000 lines, also everything Blueprint
FBlueprintComponentReflection // Just components
FBlueprintVariableReflectionServices // Modern architecture (Services pattern)
```

#### 2. **Duplicate Error Handling**

**Problem:** Two competing error handling systems:
- `FCommonUtils::CreateErrorResponse()` / `CreateSuccessResponse()` (simple)
- `FResponseSerializer::CreateErrorResponse()` (structured, error codes)

**Impact:**
- 50% of files use one pattern, 50% use the other
- Inconsistent error messages (some have error codes, some don't)
- Python clients must handle two different response formats

**Example:**
```cpp
// Pattern A (CommonUtils - simple strings)
return FCommonUtils::CreateErrorResponse(TEXT("Blueprint not found"));

// Pattern B (ResponseSerializer - structured with error codes)
return FResponseSerializer::CreateErrorResponse(
    TEXT("BLUEPRINT_NOT_FOUND"), 
    TEXT("Blueprint not found"),
    Details
);
```

#### 3. **God Object Anti-Pattern**

**Problem:** BlueprintNodeCommands.cpp (5,453 lines) handles:
- Node discovery and creation
- Pin connections and validation
- Function management (parameters, locals, properties)
- Node configuration and property setting
- Graph introspection
- Descriptor generation

**Impact:**
- Impossible to understand the full scope
- Hard to test individual features
- Merge conflicts in team development
- Long compile times (touches everything)

#### 4. **Missing Abstraction Layers**

**Problem:** Direct UE API calls scattered throughout command handlers

**Example from BlueprintCommands.cpp:**
```cpp
TSharedPtr<FJsonObject> FBlueprintCommands::HandleSetBlueprintProperty(...)
{
    // 150 lines of direct UE API calls
    UBlueprint* Blueprint = LoadObject<UBlueprint>(...);
    UBlueprintGeneratedClass* BPClass = ...;
    FProperty* Property = FindFProperty<FProperty>(...);
    // Direct property manipulation
    // Direct JSON serialization
    // No abstraction, no testing possible
}
```

**Impact:**
- Cannot unit test without running full Unreal Editor
- Cannot mock Blueprint/UMG systems
- Tightly coupled to Unreal's internal APIs

#### 5. **Lack of Result Types**

**Problem:** Functions return raw `TSharedPtr<FJsonObject>` with success/error mixed

**Example:**
```cpp
// Caller must parse JSON to determine success/failure
TSharedPtr<FJsonObject> Result = HandleCreateBlueprint(Params);
bool bSuccess = Result->GetBoolField(TEXT("success")); // Runtime check
```

**Better approach:**
```cpp
// Compile-time safety with TResult<T>
TResult<FBlueprintInfo> Result = CreateBlueprint(Params);
if (Result.IsSuccess()) {
    FBlueprintInfo Info = Result.GetValue();
    // Work with typed data
}
```

#### 6. **Inconsistent Architecture**

**Why does only BlueprintVariableReflectionServices follow Service pattern?**

`BlueprintVariableReflectionServices.h` demonstrates **best practices**:
- 6 focused service classes (Catalog, Resolver, Definition, PropertyAccess, Serializer, Context)
- Single Responsibility Principle
- Dependency injection ready
- Clear separation between data (structs) and behavior (services)

**But:** This pattern isn't applied to:
- BlueprintCommands (should be 8+ services)
- UMGCommands (should be 6+ services)
- BlueprintNodeCommands (should be 10+ services)

---

## Proposed Architecture

### Design Principles

1. **Service-Oriented Architecture** - All command handlers become thin facades over domain services
2. **Single Responsibility** - Each service class has one clear purpose (max 300-500 lines)
3. **Dependency Injection** - Services receive dependencies through constructors
4. **Result Types** - All operations return `TResult<T>` for type safety
5. **Consistent Naming** - `F{Domain}{Capability}Service` pattern everywhere
6. **Error Code Standards** - All errors use standardized error codes
7. **Async Support** - Long operations return `TFuture<TResult<T>>`

### Proposed File Structure

```
VibeUE/
├── Source/VibeUE/
│   ├── Public/
│   │   ├── Core/
│   │   │   ├── Result.h                    // TResult<T> wrapper
│   │   │   ├── ErrorCodes.h                // Centralized error codes
│   │   │   ├── ServiceContext.h            // Shared service context
│   │   │   └── AsyncOperations.h           // TFuture helpers
│   │   │
│   │   ├── Services/
│   │   │   ├── Blueprint/
│   │   │   │   ├── BlueprintDiscoveryService.h      // Find/load blueprints
│   │   │   │   ├── BlueprintLifecycleService.h      // Create/compile/reparent
│   │   │   │   ├── BlueprintPropertyService.h       // Get/set properties
│   │   │   │   ├── BlueprintComponentService.h      // Component management
│   │   │   │   ├── BlueprintVariableService.h       // Variable CRUD
│   │   │   │   ├── BlueprintFunctionService.h       // Function management
│   │   │   │   ├── BlueprintNodeService.h           // Node operations
│   │   │   │   ├── BlueprintGraphService.h          // Graph manipulation
│   │   │   │   └── BlueprintReflectionService.h     // Type discovery
│   │   │   │
│   │   │   ├── UMG/
│   │   │   │   ├── WidgetDiscoveryService.h         // Find widget blueprints
│   │   │   │   ├── WidgetLifecycleService.h         // Create/delete widgets
│   │   │   │   ├── WidgetComponentService.h         // Add/remove components
│   │   │   │   ├── WidgetPropertyService.h          // Get/set properties
│   │   │   │   ├── WidgetStyleService.h             // Styling operations
│   │   │   │   ├── WidgetEventService.h             // Event binding
│   │   │   │   └── WidgetReflectionService.h        // Type catalog
│   │   │   │
│   │   │   ├── Asset/
│   │   │   │   ├── AssetDiscoveryService.h          // Search assets
│   │   │   │   ├── AssetImportService.h             // Import textures/models
│   │   │   │   ├── AssetExportService.h             // Export for analysis
│   │   │   │   └── AssetManagementService.h         // Open/edit assets
│   │   │   │
│   │   │   └── Common/
│   │   │       ├── JsonSerializationService.h       // JSON utilities
│   │   │       ├── TypeConversionService.h          // Type conversions
│   │   │       └── CacheService.h                   // Caching strategy
│   │   │
│   │   ├── Commands/
│   │   │   ├── BlueprintCommandHandler.h            // Thin facade
│   │   │   ├── UMGCommandHandler.h                  // Thin facade
│   │   │   ├── AssetCommandHandler.h                // Thin facade
│   │   │   └── CommandDispatcher.h                  // Route to handlers
│   │   │
│   │   └── Bridge.h
│   │
│   └── Private/
│       ├── Core/
│       │   ├── Result.cpp
│       │   └── ErrorCodes.cpp
│       │
│       ├── Services/
│       │   ├── Blueprint/          // Service implementations
│       │   ├── UMG/
│       │   ├── Asset/
│       │   └── Common/
│       │
│       ├── Commands/
│       │   ├── BlueprintCommandHandler.cpp    // ~200 lines
│       │   ├── UMGCommandHandler.cpp          // ~200 lines
│       │   └── AssetCommandHandler.cpp        // ~150 lines
│       │
│       └── Bridge.cpp
```

### Service Breakdown Examples

#### Blueprint Domain (9 services replace 3 monoliths)

| Service | Lines | Responsibility | Replaces |
|---------|-------|----------------|----------|
| BlueprintDiscoveryService | ~200 | Find/load blueprints, search | BlueprintCommands (partial) |
| BlueprintLifecycleService | ~300 | Create, compile, reparent operations | BlueprintCommands (partial) |
| BlueprintPropertyService | ~250 | Get/set class default properties | BlueprintCommands (partial) |
| BlueprintComponentService | ~400 | Component CRUD operations | BlueprintComponentReflection |
| BlueprintVariableService | ~300 | Variable CRUD (already good!) | BlueprintVariableReflectionServices |
| BlueprintFunctionService | ~350 | Function/parameter management | BlueprintNodeCommands (partial) |
| BlueprintNodeService | ~450 | Node create/connect/configure | BlueprintNodeCommands (partial) |
| BlueprintGraphService | ~300 | Graph introspection/manipulation | BlueprintNodeCommands (partial) |
| BlueprintReflectionService | ~250 | Type discovery and metadata | BlueprintReflection (partial) |

**Before:** 15,700 lines across 4 files  
**After:** ~2,800 lines across 9 focused services  
**Reduction:** 82% code reduction through deduplication and focused design

#### UMG Domain (7 services replace 2 files)

| Service | Lines | Responsibility | Replaces |
|---------|-------|----------------|----------|
| WidgetDiscoveryService | ~150 | Search widget blueprints | UMGCommands (partial) |
| WidgetLifecycleService | ~200 | Create/delete widget blueprints | UMGCommands (partial) |
| WidgetComponentService | ~300 | Add/remove widget components | UMGCommands + UMGReflectionCommands |
| WidgetPropertyService | ~250 | Get/set widget properties | UMGCommands (partial) |
| WidgetStyleService | ~200 | Styling and layout operations | UMGCommands (partial) |
| WidgetEventService | ~150 | Event binding | UMGCommands (partial) |
| WidgetReflectionService | ~250 | Widget type catalog | UMGReflectionCommands (partial) |

**Before:** 5,677 lines across 2 files  
**After:** ~1,500 lines across 7 focused services  
**Reduction:** 74% code reduction

---

## Implementation Details

### 1. Result Type Pattern

**File:** `Public/Core/Result.h`

```cpp
#pragma once

#include "CoreMinimal.h"

/**
 * Result type for operations that can succeed or fail
 * Provides type safety and avoids runtime JSON parsing
 */
template<typename T>
class VIBEUE_API TResult
{
public:
    // Success constructor
    static TResult Success(const T& Value)
    {
        return TResult(true, Value, FString(), FString());
    }

    static TResult Success(T&& Value)
    {
        return TResult(true, MoveTemp(Value), FString(), FString());
    }

    // Error constructor with error code
    static TResult Error(const FString& ErrorCode, const FString& ErrorMessage)
    {
        return TResult(false, T(), ErrorCode, ErrorMessage);
    }

    // Accessors
    bool IsSuccess() const { return bSuccess; }
    bool IsError() const { return !bSuccess; }
    
    const T& GetValue() const 
    { 
        check(bSuccess); 
        return Value; 
    }
    
    const FString& GetErrorCode() const { return ErrorCode; }
    const FString& GetErrorMessage() const { return ErrorMessage; }

    // Functional composition
    template<typename U>
    TResult<U> Map(TFunction<U(const T&)> Fn) const
    {
        if (IsSuccess())
        {
            return TResult<U>::Success(Fn(Value));
        }
        return TResult<U>::Error(ErrorCode, ErrorMessage);
    }

    template<typename U>
    TResult<U> FlatMap(TFunction<TResult<U>(const T&)> Fn) const
    {
        if (IsSuccess())
        {
            return Fn(Value);
        }
        return TResult<U>::Error(ErrorCode, ErrorMessage);
    }

private:
    TResult(bool bInSuccess, const T& InValue, const FString& InErrorCode, const FString& InErrorMessage)
        : bSuccess(bInSuccess), Value(InValue), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
    {}

    TResult(bool bInSuccess, T&& InValue, const FString& InErrorCode, const FString& InErrorMessage)
        : bSuccess(bInSuccess), Value(MoveTemp(InValue)), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
    {}

    bool bSuccess;
    T Value;
    FString ErrorCode;
    FString ErrorMessage;
};

// Specialization for void operations
template<>
class VIBEUE_API TResult<void>
{
public:
    static TResult Success()
    {
        return TResult(true, FString(), FString());
    }

    static TResult Error(const FString& ErrorCode, const FString& ErrorMessage)
    {
        return TResult(false, ErrorCode, ErrorMessage);
    }

    bool IsSuccess() const { return bSuccess; }
    bool IsError() const { return !bSuccess; }
    
    const FString& GetErrorCode() const { return ErrorCode; }
    const FString& GetErrorMessage() const { return ErrorMessage; }

private:
    TResult(bool bInSuccess, const FString& InErrorCode, const FString& InErrorMessage)
        : bSuccess(bInSuccess), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
    {}

    bool bSuccess;
    FString ErrorCode;
    FString ErrorMessage;
};
```

**Usage Example:**

```cpp
// Service layer - returns typed results
TResult<UBlueprint*> FBlueprintDiscoveryService::FindBlueprint(const FString& BlueprintName)
{
    if (BlueprintName.IsEmpty())
    {
        return TResult<UBlueprint*>::Error(
            TEXT("PARAM_INVALID"),
            TEXT("Blueprint name cannot be empty")
        );
    }

    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintName);
    if (!BP)
    {
        return TResult<UBlueprint*>::Error(
            TEXT("BLUEPRINT_NOT_FOUND"),
            FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName)
        );
    }

    return TResult<UBlueprint*>::Success(BP);
}

// Command handler - converts result to JSON
TSharedPtr<FJsonObject> FBlueprintCommandHandler::HandleFindBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName = Params->GetStringField(TEXT("blueprint_name"));
    
    TResult<UBlueprint*> Result = BlueprintDiscovery->FindBlueprint(BlueprintName);
    
    if (Result.IsError())
    {
        return FResponseSerializer::CreateErrorResponse(
            Result.GetErrorCode(),
            Result.GetErrorMessage()
        );
    }

    // Convert successful result to JSON
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("blueprint_name"), Result.GetValue()->GetName());
    Data->SetStringField(TEXT("blueprint_path"), Result.GetValue()->GetPathName());
    
    return FResponseSerializer::CreateSuccessResponse(Data);
}
```

### 2. Centralized Error Codes

**File:** `Public/Core/ErrorCodes.h`

```cpp
#pragma once

#include "CoreMinimal.h"

/**
 * Centralized error codes for consistent error handling
 * Organized by domain and operation type
 */
namespace VibeUE::ErrorCodes
{
    // Parameter validation errors (1000-1099)
    constexpr const TCHAR* PARAM_MISSING = TEXT("PARAM_MISSING");
    constexpr const TCHAR* PARAM_INVALID = TEXT("PARAM_INVALID");
    constexpr const TCHAR* PARAM_TYPE_MISMATCH = TEXT("PARAM_TYPE_MISMATCH");

    // Blueprint errors (2000-2099)
    constexpr const TCHAR* BLUEPRINT_NOT_FOUND = TEXT("BLUEPRINT_NOT_FOUND");
    constexpr const TCHAR* BLUEPRINT_LOAD_FAILED = TEXT("BLUEPRINT_LOAD_FAILED");
    constexpr const TCHAR* BLUEPRINT_COMPILATION_FAILED = TEXT("BLUEPRINT_COMPILATION_FAILED");
    constexpr const TCHAR* BLUEPRINT_ALREADY_EXISTS = TEXT("BLUEPRINT_ALREADY_EXISTS");
    constexpr const TCHAR* BLUEPRINT_INVALID_PARENT = TEXT("BLUEPRINT_INVALID_PARENT");

    // Variable errors (2100-2199)
    constexpr const TCHAR* VARIABLE_NOT_FOUND = TEXT("VARIABLE_NOT_FOUND");
    constexpr const TCHAR* VARIABLE_ALREADY_EXISTS = TEXT("VARIABLE_ALREADY_EXISTS");
    constexpr const TCHAR* VARIABLE_TYPE_INVALID = TEXT("VARIABLE_TYPE_INVALID");
    constexpr const TCHAR* VARIABLE_CREATE_FAILED = TEXT("VARIABLE_CREATE_FAILED");

    // Component errors (2200-2299)
    constexpr const TCHAR* COMPONENT_NOT_FOUND = TEXT("COMPONENT_NOT_FOUND");
    constexpr const TCHAR* COMPONENT_TYPE_INVALID = TEXT("COMPONENT_TYPE_INVALID");
    constexpr const TCHAR* COMPONENT_ADD_FAILED = TEXT("COMPONENT_ADD_FAILED");

    // Property errors (2300-2399)
    constexpr const TCHAR* PROPERTY_NOT_FOUND = TEXT("PROPERTY_NOT_FOUND");
    constexpr const TCHAR* PROPERTY_READ_ONLY = TEXT("PROPERTY_READ_ONLY");
    constexpr const TCHAR* PROPERTY_TYPE_MISMATCH = TEXT("PROPERTY_TYPE_MISMATCH");
    constexpr const TCHAR* PROPERTY_SET_FAILED = TEXT("PROPERTY_SET_FAILED");

    // Node errors (2400-2499)
    constexpr const TCHAR* NODE_NOT_FOUND = TEXT("NODE_NOT_FOUND");
    constexpr const TCHAR* NODE_CREATE_FAILED = TEXT("NODE_CREATE_FAILED");
    constexpr const TCHAR* NODE_TYPE_INVALID = TEXT("NODE_TYPE_INVALID");
    constexpr const TCHAR* PIN_NOT_FOUND = TEXT("PIN_NOT_FOUND");
    constexpr const TCHAR* PIN_CONNECTION_FAILED = TEXT("PIN_CONNECTION_FAILED");
    constexpr const TCHAR* PIN_TYPE_INCOMPATIBLE = TEXT("PIN_TYPE_INCOMPATIBLE");

    // UMG/Widget errors (3000-3099)
    constexpr const TCHAR* WIDGET_NOT_FOUND = TEXT("WIDGET_NOT_FOUND");
    constexpr const TCHAR* WIDGET_CREATE_FAILED = TEXT("WIDGET_CREATE_FAILED");
    constexpr const TCHAR* WIDGET_TYPE_INVALID = TEXT("WIDGET_TYPE_INVALID");
    constexpr const TCHAR* WIDGET_COMPONENT_NOT_FOUND = TEXT("WIDGET_COMPONENT_NOT_FOUND");
    constexpr const TCHAR* WIDGET_PARENT_INCOMPATIBLE = TEXT("WIDGET_PARENT_INCOMPATIBLE");

    // Asset errors (4000-4099)
    constexpr const TCHAR* ASSET_NOT_FOUND = TEXT("ASSET_NOT_FOUND");
    constexpr const TCHAR* ASSET_IMPORT_FAILED = TEXT("ASSET_IMPORT_FAILED");
    constexpr const TCHAR* ASSET_EXPORT_FAILED = TEXT("ASSET_EXPORT_FAILED");

    // System errors (9000-9099)
    constexpr const TCHAR* OPERATION_NOT_SUPPORTED = TEXT("OPERATION_NOT_SUPPORTED");
    constexpr const TCHAR* INTERNAL_ERROR = TEXT("INTERNAL_ERROR");
    constexpr const TCHAR* TIMEOUT = TEXT("TIMEOUT");
}
```

### 3. Service Base Class

**File:** `Public/Services/Common/ServiceBase.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Core/Result.h"
#include "Core/ServiceContext.h"

/**
 * Base class for all VibeUE services
 * Provides common functionality and enforces patterns
 */
class VIBEUE_API FServiceBase
{
public:
    explicit FServiceBase(TSharedPtr<FServiceContext> InContext)
        : Context(InContext)
    {
        check(Context.IsValid());
    }

    virtual ~FServiceBase() = default;

    // Lifecycle
    virtual void Initialize() {}
    virtual void Shutdown() {}

protected:
    // Context access
    TSharedPtr<FServiceContext> GetContext() const { return Context; }
    
    // Logging helpers
    void LogInfo(const FString& Message) const;
    void LogWarning(const FString& Message) const;
    void LogError(const FString& Message) const;

    // Validation helpers
    template<typename T>
    TResult<T> ValidateNotNull(T* Pointer, const FString& ErrorCode, const FString& Message) const
    {
        if (!Pointer)
        {
            return TResult<T>::Error(ErrorCode, Message);
        }
        return TResult<T>::Success(Pointer);
    }

    TResult<void> ValidateString(const FString& Value, const FString& ParamName) const;
    TResult<void> ValidateArray(const TArray<FString>& Value, const FString& ParamName) const;

private:
    TSharedPtr<FServiceContext> Context;
};
```

### 4. Command Handler Pattern

**File:** `Public/Commands/BlueprintCommandHandler.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Services/Blueprint/BlueprintDiscoveryService.h"
#include "Services/Blueprint/BlueprintLifecycleService.h"
#include "Services/Blueprint/BlueprintPropertyService.h"
// ... other services

/**
 * Thin facade that routes Blueprint commands to appropriate services
 * Converts service results to JSON responses
 */
class VIBEUE_API FBlueprintCommandHandler
{
public:
    explicit FBlueprintCommandHandler(TSharedPtr<FServiceContext> Context);
    ~FBlueprintCommandHandler() = default;

    // Command routing
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Command handlers (delegate to services)
    TSharedPtr<FJsonObject> HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetBlueprintProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBlueprintProperty(const TSharedPtr<FJsonObject>& Params);
    // ... ~15 total command handlers

    // Service instances (injected dependencies)
    TSharedPtr<FBlueprintDiscoveryService> DiscoveryService;
    TSharedPtr<FBlueprintLifecycleService> LifecycleService;
    TSharedPtr<FBlueprintPropertyService> PropertyService;
    TSharedPtr<FBlueprintComponentService> ComponentService;
    TSharedPtr<FBlueprintVariableService> VariableService;
    TSharedPtr<FBlueprintFunctionService> FunctionService;
    TSharedPtr<FBlueprintNodeService> NodeService;
    TSharedPtr<FBlueprintGraphService> GraphService;
    TSharedPtr<FBlueprintReflectionService> ReflectionService;
};
```

**Implementation Example:**

```cpp
// BlueprintCommandHandler.cpp (~200 lines total)

FBlueprintCommandHandler::FBlueprintCommandHandler(TSharedPtr<FServiceContext> Context)
{
    // Dependency injection - services share context
    DiscoveryService = MakeShared<FBlueprintDiscoveryService>(Context);
    LifecycleService = MakeShared<FBlueprintLifecycleService>(Context);
    PropertyService = MakeShared<FBlueprintPropertyService>(Context);
    ComponentService = MakeShared<FBlueprintComponentService>(Context);
    VariableService = MakeShared<FBlueprintVariableService>(Context);
    FunctionService = MakeShared<FBlueprintFunctionService>(Context);
    NodeService = MakeShared<FBlueprintNodeService>(Context);
    GraphService = MakeShared<FBlueprintGraphService>(Context);
    ReflectionService = MakeShared<FBlueprintReflectionService>(Context);
}

TSharedPtr<FJsonObject> FBlueprintCommandHandler::HandleCommand(
    const FString& CommandType, 
    const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_blueprint"))
    {
        return HandleCreateBlueprint(Params);
    }
    else if (CommandType == TEXT("compile_blueprint"))
    {
        return HandleCompileBlueprint(Params);
    }
    // ... 15 more command routes
    
    return FResponseSerializer::CreateErrorResponse(
        VibeUE::ErrorCodes::OPERATION_NOT_SUPPORTED,
        FString::Printf(TEXT("Unknown Blueprint command: %s"), *CommandType)
    );
}

TSharedPtr<FJsonObject> FBlueprintCommandHandler::HandleCreateBlueprint(
    const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString Name = Params->GetStringField(TEXT("name"));
    FString ParentClass = Params->GetStringField(TEXT("parent_class"));

    // Call service layer
    TResult<FBlueprintInfo> Result = LifecycleService->CreateBlueprint(Name, ParentClass);

    // Convert result to JSON
    if (Result.IsError())
    {
        return FResponseSerializer::CreateErrorResponse(
            Result.GetErrorCode(),
            Result.GetErrorMessage()
        );
    }

    // Serialize success data
    FBlueprintInfo Info = Result.GetValue();
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("name"), Info.Name);
    Data->SetStringField(TEXT("path"), Info.Path);
    Data->SetStringField(TEXT("parent_class"), Info.ParentClass);

    return FResponseSerializer::CreateSuccessResponse(Data);
}
```

---

## Migration Strategy

### Phase 1: Foundation (Week 1)
**Goal:** Establish core infrastructure without breaking existing code

**Tasks:**
1. Create `Core/` folder with Result, ErrorCodes, ServiceContext
2. Create `Services/Common/` with ServiceBase, JsonSerializationService
3. Write unit tests for Result<T> wrapper
4. Document service patterns in `docs/SERVICE_PATTERNS.md`

**Deliverables:**
- `Result.h` with comprehensive tests
- `ErrorCodes.h` with all ~50 error codes
- `ServiceBase.h` with validation helpers
- Zero impact on existing code (new files only)

### Phase 2: Blueprint Services (Week 2-3)
**Goal:** Extract Blueprint domain into 9 focused services

**Migration Order:**
1. **BlueprintVariableReflectionServices** ✅ Already follows pattern - just rename and align
2. **BlueprintDiscoveryService** - Extract from BlueprintCommands (find/load logic)
3. **BlueprintLifecycleService** - Extract from BlueprintCommands (create/compile/reparent)
4. **BlueprintPropertyService** - Extract from BlueprintCommands (get/set properties)
5. **BlueprintComponentService** - Refactor BlueprintComponentReflection
6. **BlueprintFunctionService** - Extract from BlueprintNodeCommands (function management)
7. **BlueprintNodeService** - Extract from BlueprintNodeCommands (node operations)
8. **BlueprintGraphService** - Extract from BlueprintNodeCommands (graph introspection)
9. **BlueprintReflectionService** - Consolidate from BlueprintReflection

**Approach:**
- Create new service files alongside old command files
- Move methods one at a time to services
- Update command handlers to delegate to services
- Add unit tests for each service
- Remove old code once verified

**Testing Strategy:**
- Python MCP tests must pass 100% after each service migration
- Maintain backward compatibility during transition
- Add new unit tests (target: 80% coverage)

### Phase 3: UMG Services (Week 4)
**Goal:** Extract UMG domain into 7 focused services

**Migration Order:**
1. **WidgetDiscoveryService** - Extract search/find logic
2. **WidgetLifecycleService** - Extract create/delete operations
3. **WidgetComponentService** - Consolidate component addition
4. **WidgetPropertyService** - Extract property get/set
5. **WidgetStyleService** - Extract styling operations
6. **WidgetEventService** - Extract event binding
7. **WidgetReflectionService** - Consolidate type discovery

### Phase 4: Command Handler Simplification (Week 5)
**Goal:** Replace monolithic command classes with thin facades

**Tasks:**
1. Create `BlueprintCommandHandler` (replaces `FBlueprintCommands`)
2. Create `UMGCommandHandler` (replaces `FUMGCommands`)
3. Create `AssetCommandHandler` (replaces `FAssetCommands`)
4. Update `Bridge.cpp` to use new command handlers
5. Delete old command classes

**Before/After Comparison:**
```cpp
// BEFORE (Bridge.cpp)
TSharedPtr<FBlueprintCommands> BlueprintCommands;  // 4,000 lines
TSharedPtr<FBlueprintNodeCommands> BlueprintNodeCommands;  // 5,453 lines
TSharedPtr<FBlueprintComponentReflection> BlueprintComponentReflection;  // 2,201 lines

// AFTER (Bridge.cpp)
TSharedPtr<FBlueprintCommandHandler> BlueprintHandler;  // 200 lines (delegates to services)
```

### Phase 5: Documentation & Testing (Week 6)
**Goal:** Comprehensive documentation and test coverage

**Tasks:**
1. Doxygen comments for all public service APIs
2. Unit tests for all services (target: 80% coverage)
3. Integration tests for command handlers
4. Performance benchmarks (before/after comparison)
5. Migration guide for external developers
6. API reference documentation

### Phase 6: Performance Optimization (Week 7)
**Goal:** Add caching and async operations

**Tasks:**
1. Implement `CacheService` for Blueprint/Widget discovery
2. Add async variants for slow operations (compilation, import)
3. Profile and optimize hot paths
4. Memory leak detection and fixes
5. Performance comparison report

---

## Testing Strategy

### Unit Testing Framework

**Tool:** Unreal Engine's built-in `Automation Testing Framework`

**Test Organization:**
```
VibeUE/
├── Source/
│   └── VibeUETests/
│       ├── VibeUETests.Build.cs
│       ├── Private/
│       │   ├── Core/
│       │   │   ├── ResultTests.cpp
│       │   │   └── ErrorCodesTests.cpp
│       │   │
│       │   ├── Services/
│       │   │   ├── Blueprint/
│       │   │   │   ├── BlueprintDiscoveryServiceTests.cpp
│       │   │   │   ├── BlueprintLifecycleServiceTests.cpp
│       │   │   │   └── BlueprintPropertyServiceTests.cpp
│       │   │   │
│       │   │   └── UMG/
│       │   │       ├── WidgetDiscoveryServiceTests.cpp
│       │   │       └── WidgetComponentServiceTests.cpp
│       │   │
│       │   └── Commands/
│       │       ├── BlueprintCommandHandlerTests.cpp
│       │       └── UMGCommandHandlerTests.cpp
```

**Example Test:**

```cpp
// BlueprintDiscoveryServiceTests.cpp

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlueprintDiscoveryService_FindBlueprint_ValidName_ReturnsBlueprint,
    "VibeUE.Services.Blueprint.Discovery.FindBlueprint.ValidName",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FBlueprintDiscoveryService_FindBlueprint_ValidName_ReturnsBlueprint::RunTest(const FString& Parameters)
{
    // Arrange
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FBlueprintDiscoveryService Service(Context);
    
    // Create a test blueprint
    FString TestBlueprintName = TEXT("/Game/Test/BP_TestActor");
    // ... setup test blueprint

    // Act
    TResult<UBlueprint*> Result = Service.FindBlueprint(TestBlueprintName);

    // Assert
    TestTrue(TEXT("Result should be success"), Result.IsSuccess());
    TestNotNull(TEXT("Blueprint should not be null"), Result.GetValue());
    TestEqual(TEXT("Blueprint name should match"), Result.GetValue()->GetName(), TEXT("BP_TestActor"));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlueprintDiscoveryService_FindBlueprint_InvalidName_ReturnsError,
    "VibeUE.Services.Blueprint.Discovery.FindBlueprint.InvalidName",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FBlueprintDiscoveryService_FindBlueprint_InvalidName_ReturnsError::RunTest(const FString& Parameters)
{
    // Arrange
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FBlueprintDiscoveryService Service(Context);

    // Act
    TResult<UBlueprint*> Result = Service.FindBlueprint(TEXT("NonExistent"));

    // Assert
    TestTrue(TEXT("Result should be error"), Result.IsError());
    TestEqual(TEXT("Error code should be BLUEPRINT_NOT_FOUND"), 
              Result.GetErrorCode(), 
              VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND);

    return true;
}
```

### Integration Testing

**Python MCP Test Suite:**
- All existing Python tests must pass after each phase
- Add new tests for service-level functionality
- Performance regression tests (track execution time)

### Performance Benchmarks

**Metrics to track:**
- Command execution time (before/after)
- Memory usage per command
- Compilation times
- Blueprint discovery speed with caching

**Target Improvements:**
- 20% faster command execution (through caching)
- 50% reduction in memory allocations
- Zero memory leaks (verified with profiler)

---

## Success Criteria

### Quantitative Metrics

| Metric | Current | Target | Measurement |
|--------|---------|--------|-------------|
| Average file size | 1,900 lines | <500 lines | Line count per .cpp file |
| Largest file size | 5,453 lines | <800 lines | BlueprintNodeCommands → 6 services |
| Test coverage | ~0% | >80% | Unit test coverage report |
| Error handling consistency | ~50% | 100% | All use ErrorCodes constants |
| Response format consistency | ~50% | 100% | All use FResponseSerializer |
| Service naming consistency | 33% | 100% | All follow F{Domain}{Capability}Service |
| Command handler size | 4,000+ lines | <300 lines | Thin facade pattern |
| Duplicate code | High | <5% | SonarQube analysis |

### Qualitative Goals

1. **Marketplace Ready** - Professional code quality that passes Unreal Marketplace review
2. **Developer Friendly** - New contributors can understand architecture in <1 hour
3. **Testable** - All business logic has unit tests
4. **Maintainable** - Changes localized to single service (no ripple effects)
5. **Documented** - Every public API has Doxygen comments
6. **Performant** - No regression in command execution time
7. **Backward Compatible** - All existing Python MCP tools continue working

---

## Risk Assessment

### High Risk

**Risk:** Breaking existing Python MCP integration during refactoring  
**Mitigation:** 
- Maintain 100% backward compatibility during migration
- Run full Python test suite after each service extraction
- Keep old code alongside new code until verified
- Feature flag new services (opt-in testing)

**Risk:** Performance regression from abstraction layers  
**Mitigation:**
- Performance benchmarks before/after each phase
- Profiling to identify hot paths
- Optimize critical paths if needed
- Caching strategy for expensive operations

### Medium Risk

**Risk:** Team learning curve for new architecture  
**Mitigation:**
- Comprehensive documentation with examples
- Code review sessions to share knowledge
- Pair programming for first service migrations
- Clear migration guide

**Risk:** Incomplete error code coverage  
**Mitigation:**
- Audit all existing error paths
- Create comprehensive ErrorCodes enum
- Code review checklist includes error handling
- Static analysis to detect string literals

### Low Risk

**Risk:** Increased compile times from more files  
**Mitigation:**
- Unity build support
- Forward declarations
- Precompiled headers
- Measure and track compile times

---

## Appendix A: Naming Convention Standards

### Service Naming Pattern

```cpp
F{Domain}{Capability}Service

Examples:
✅ FBlueprintDiscoveryService      // Domain=Blueprint, Capability=Discovery
✅ FBlueprintLifecycleService       // Domain=Blueprint, Capability=Lifecycle
✅ FWidgetComponentService          // Domain=Widget, Capability=Component
✅ FCacheService                    // Domain=Cache (shared), Capability implicit

❌ FBlueprintCommands               // Old pattern - too vague
❌ FBlueprintReflection             // Old pattern - inconsistent with Commands
❌ FBlueprintVariableReflectionServices // Old pattern - plural, too long
```

### Command Handler Naming Pattern

```cpp
F{Domain}CommandHandler

Examples:
✅ FBlueprintCommandHandler
✅ FUMGCommandHandler
✅ FAssetCommandHandler

❌ FBlueprintCommands               // Old pattern
```

### File Naming Convention

```cpp
// Service headers
Public/Services/{Domain}/{ServiceName}.h
Example: Public/Services/Blueprint/BlueprintDiscoveryService.h

// Service implementations
Private/Services/{Domain}/{ServiceName}.cpp
Example: Private/Services/Blueprint/BlueprintDiscoveryService.cpp

// Command handlers
Public/Commands/{HandlerName}.h
Private/Commands/{HandlerName}.cpp
```

---

## Appendix B: Code Review Checklist

### Service Implementation Checklist

- [ ] Service inherits from `FServiceBase`
- [ ] Constructor takes `TSharedPtr<FServiceContext>`
- [ ] Public methods return `TResult<T>` (not raw pointers or JSON)
- [ ] All errors use `VibeUE::ErrorCodes` constants
- [ ] No direct JSON serialization (delegate to command handler)
- [ ] File size <500 lines
- [ ] All public methods have Doxygen comments
- [ ] Unit tests with >80% coverage
- [ ] No raw `new`/`delete` (use `MakeShared`/`MakeUnique`)
- [ ] No `UE_LOG` with string literals (use service logging helpers)

### Command Handler Checklist

- [ ] Handler is thin facade (<300 lines total)
- [ ] All services injected through constructor
- [ ] Delegates all business logic to services
- [ ] Converts `TResult<T>` to JSON using `FResponseSerializer`
- [ ] No direct Unreal API calls (all through services)
- [ ] Command routing uses string matching (simple if/else or switch)

### Error Handling Checklist

- [ ] All errors use `VibeUE::ErrorCodes` constants
- [ ] Error messages are user-friendly and actionable
- [ ] Structured error responses include error code + message
- [ ] No generic "error" strings
- [ ] No `nullptr` dereferencing (use `ValidateNotNull`)

---

## Appendix C: Performance Optimization Guidelines

### Caching Strategy

**What to cache:**
- Blueprint discovery results (path → UBlueprint*)
- Widget type catalog (expensive reflection)
- Component property metadata
- Node descriptor catalog

**Cache invalidation:**
- On asset deletion (IAssetRegistry callbacks)
- On hot reload (FCoreDelegates::OnHotReload)
- Manual invalidation API for debugging

**Example:**

```cpp
class VIBEUE_API FCacheService : public FServiceBase
{
public:
    // Get blueprint with caching
    TResult<UBlueprint*> GetBlueprint(const FString& Path)
    {
        // Check cache first
        if (UBlueprint** Cached = BlueprintCache.Find(Path))
        {
            if (*Cached && (*Cached)->IsValidLowLevel())
            {
                return TResult<UBlueprint*>::Success(*Cached);
            }
            // Stale cache entry, remove it
            BlueprintCache.Remove(Path);
        }

        // Load and cache
        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Path);
        if (BP)
        {
            BlueprintCache.Add(Path, BP);
            return TResult<UBlueprint*>::Success(BP);
        }

        return TResult<UBlueprint*>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            FString::Printf(TEXT("Blueprint '%s' not found"), *Path)
        );
    }

private:
    TMap<FString, UBlueprint*> BlueprintCache;
    
    // Asset registry callbacks for cache invalidation
    void OnAssetDeleted(const FAssetData& AssetData);
};
```

### Async Operations

**Operations that should be async:**
- Blueprint compilation (1-5 seconds)
- Asset import (variable time)
- Large asset searches (Asset Registry queries)

**Pattern:**

```cpp
// Async compilation
TFuture<TResult<void>> FBlueprintLifecycleService::CompileBlueprintAsync(UBlueprint* Blueprint)
{
    return Async(EAsyncExecution::TaskGraph, [Blueprint]()
    {
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        
        if (Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings)
        {
            return TResult<void>::Success();
        }
        
        return TResult<void>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_COMPILATION_FAILED,
            TEXT("Blueprint compilation failed")
        );
    });
}

// Usage in command handler
TSharedPtr<FJsonObject> FBlueprintCommandHandler::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // ... get blueprint
    
    // Start async compilation
    TFuture<TResult<void>> Future = LifecycleService->CompileBlueprintAsync(Blueprint);
    
    // For now, block (in future, support async responses)
    TResult<void> Result = Future.Get();
    
    if (Result.IsError())
    {
        return FResponseSerializer::CreateErrorResponse(
            Result.GetErrorCode(),
            Result.GetErrorMessage()
        );
    }
    
    return FResponseSerializer::CreateSuccessResponse();
}
```

---

## Conclusion

This refactoring will transform VibeUE from a functional but inconsistent codebase into a **professional, maintainable, testable, and marketplace-ready plugin**. The service-oriented architecture with dependency injection, result types, and consistent error handling will:

1. **Reduce complexity** by 70-80% through focused services
2. **Improve reliability** with comprehensive unit tests (80%+ coverage)
3. **Enhance developer experience** with clear patterns and documentation
4. **Enable future expansion** through loosely-coupled services
5. **Meet marketplace standards** with professional code quality

**Estimated Timeline:** 7 weeks (1 developer full-time)  
**Estimated Effort:** 280 hours  
**Risk Level:** Medium (mitigated through phased approach)  
**ROI:** High (enables marketplace release and future maintenance)

---

**Next Steps:**
1. Review and approve this design document
2. Set up project board with Phase 1 tasks
3. Begin Core infrastructure implementation (Week 1)
4. Schedule weekly progress reviews
5. Track metrics dashboard (file sizes, test coverage, performance)
