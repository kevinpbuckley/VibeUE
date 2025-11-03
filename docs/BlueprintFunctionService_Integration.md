# Integration Guide: BlueprintFunctionService

## Overview

This document explains how to integrate the newly extracted `FBlueprintFunctionService` with the existing `BlueprintNodeCommands.cpp` command handler.

## Current State

The BlueprintFunctionService has been created as a standalone service but is **not yet integrated** with the existing command handlers. This was intentional to keep the changes minimal and non-breaking.

## Integration Strategy

### Phase 1: Service Creation (Completed)

✅ Created foundation classes:
- `TResult<T>` - Type-safe result wrapper
- `ErrorCodes` - Centralized error codes
- `FServiceBase` - Base class for services
- `FServiceContext` - Shared context

✅ Created `FBlueprintFunctionService` with all required methods

### Phase 2: Integration (Future Work)

To integrate the service with existing code:

#### Option A: Update BlueprintNodeCommands (Recommended)

Modify `FBlueprintNodeCommands` to use the service internally:

```cpp
// In BlueprintNodeCommands.h
class VIBEUE_API FBlueprintNodeCommands
{
public:
    FBlueprintNodeCommands();
    
private:
    // Add service member
    TSharedPtr<FBlueprintFunctionService> FunctionService;
    
    // Existing methods become thin wrappers
    TSharedPtr<FJsonObject> HandleManageBlueprintFunction(const TSharedPtr<FJsonObject>& Params);
    // ... other methods
};

// In BlueprintNodeCommands.cpp
FBlueprintNodeCommands::FBlueprintNodeCommands()
{
    // Initialize service
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    FunctionService = MakeShared<FBlueprintFunctionService>(Context);
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleManageBlueprintFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString Action = Params->GetStringField(TEXT("action"));
    FString BlueprintName = Params->GetStringField(TEXT("blueprint_name"));
    
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Blueprint not found"));
    }
    
    if (Action == TEXT("create_function"))
    {
        FString FunctionName = Params->GetStringField(TEXT("function_name"));
        
        // Use service
        TResult<UEdGraph*> Result = FunctionService->CreateFunction(Blueprint, FunctionName);
        
        if (Result.IsSuccess())
        {
            TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
            Response->SetBoolField(TEXT("success"), true);
            Response->SetStringField(TEXT("function_name"), FunctionName);
            Response->SetStringField(TEXT("graph_guid"), Result.GetValue()->GraphGuid.ToString());
            return Response;
        }
        else
        {
            return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
        }
    }
    else if (Action == TEXT("delete_function"))
    {
        FString FunctionName = Params->GetStringField(TEXT("function_name"));
        
        TResult<void> Result = FunctionService->DeleteFunction(Blueprint, FunctionName);
        
        if (Result.IsSuccess())
        {
            TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
            Response->SetBoolField(TEXT("success"), true);
            return Response;
        }
        else
        {
            return FCommonUtils::CreateErrorResponse(Result.GetErrorMessage());
        }
    }
    // ... handle other actions
    
    return FCommonUtils::CreateErrorResponse(TEXT("Unknown action"));
}
```

#### Option B: Create New Command Handler (Clean Slate)

Create a new `FBlueprintFunctionCommandHandler` that uses only the service:

```cpp
// BlueprintFunctionCommandHandler.h
#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Services/Blueprint/BlueprintFunctionService.h"

class VIBEUE_API FBlueprintFunctionCommandHandler
{
public:
    FBlueprintFunctionCommandHandler();
    
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
    
private:
    TSharedPtr<FBlueprintFunctionService> FunctionService;
    
    TSharedPtr<FJsonObject> HandleCreateFunction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteFunction(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListFunctions(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveParameter(const TSharedPtr<FJsonObject>& Params);
    // ... other handlers
};
```

## Helper Function: Convert TResult to JSON

Create a utility to convert service results to JSON responses:

```cpp
// In CommonUtils.h
class VIBEUE_API FCommonUtils
{
public:
    // Existing methods...
    
    template<typename T>
    static TSharedPtr<FJsonObject> ConvertResultToJson(const TResult<T>& Result, 
        TFunction<void(TSharedPtr<FJsonObject>&, const T&)> OnSuccess)
    {
        if (Result.IsError())
        {
            return CreateErrorResponse(Result.GetErrorMessage());
        }
        
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), true);
        OnSuccess(Response, Result.GetValue());
        return Response;
    }
    
    static TSharedPtr<FJsonObject> ConvertVoidResultToJson(const TResult<void>& Result)
    {
        if (Result.IsError())
        {
            return CreateErrorResponse(Result.GetErrorMessage());
        }
        
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), true);
        return Response;
    }
};
```

Usage example:

```cpp
TResult<TArray<FFunctionInfo>> Result = FunctionService->ListFunctions(Blueprint);

return FCommonUtils::ConvertResultToJson(Result, [](TSharedPtr<FJsonObject>& Response, const TArray<FFunctionInfo>& Functions)
{
    TArray<TSharedPtr<FJsonValue>> FunctionArray;
    for (const FFunctionInfo& Info : Functions)
    {
        TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
        FuncObj->SetStringField(TEXT("name"), Info.Name);
        FuncObj->SetStringField(TEXT("graph_guid"), Info.GraphGuid);
        FuncObj->SetNumberField(TEXT("node_count"), Info.NodeCount);
        FunctionArray.Add(MakeShared<FJsonValueObject>(FuncObj));
    }
    Response->SetArrayField(TEXT("functions"), FunctionArray);
});
```

## Testing Integration

After integration:

1. **Verify existing Python MCP tests still pass**
   - Run `test_function_call_fix.py`
   - Run any other function-related tests
   
2. **Test new service directly**
   - Create test Blueprint in Unreal Editor
   - Call service methods directly from C++
   - Verify Blueprint changes in editor

3. **Test through command handlers**
   - Use MCP server to create/delete functions
   - Verify parameter management
   - Check local variable operations

## Migration Timeline

### Immediate (Done)
- ✅ Service extraction complete
- ✅ Foundation classes in place
- ✅ Documentation complete

### Short-term (Next PR)
- Integrate service into `BlueprintNodeCommands`
- Add helper utilities for JSON conversion
- Update existing methods to use service

### Medium-term
- Remove duplicate code from `BlueprintNodeCommands.cpp`
- Create additional services (NodeService, GraphService)
- Standardize error responses

### Long-term
- Full refactoring per design document
- Comprehensive test suite
- Performance optimizations

## Benefits

### Before (Current State)
```
BlueprintNodeCommands.cpp (6,125 lines)
├── Function creation (inline, ~50 lines)
├── Parameter management (inline, ~400 lines)
├── Local variables (inline, ~300 lines)
├── Type parsing (inline, ~200 lines)
└── Many other concerns (5,175 lines)
```

### After Integration
```
BlueprintNodeCommands.cpp (~5,000 lines)
├── Command routing (thin layer)
└── Delegates to services

FBlueprintFunctionService (903 lines)
├── Function operations
├── Parameter operations
├── Variable operations
└── Type utilities
```

**Reduction**: ~25% in main file, better separation of concerns

## Rollback Strategy

If integration causes issues:

1. The new service files can be removed without affecting existing functionality
2. No existing code was modified
3. Simply don't use the service and keep existing implementation
4. Service can be improved iteratively before integration

## Questions?

For questions about integration:
- Review `docs/BlueprintFunctionService.md` for service usage
- Review `docs/CPP_REFACTORING_DESIGN.md` for architecture
- Check existing `BlueprintVariableReflectionServices.cpp` for service pattern example
