# VibeUE Service Patterns Guide

**Version:** 1.0  
**Date:** November 3, 2025  
**Status:** Phase 1 Complete

---

## Overview

This document describes the service-oriented architecture patterns used in VibeUE after the Phase 1 refactoring. All new code should follow these patterns for consistency and maintainability.

## Core Principles

### 1. Service-Oriented Architecture
- All business logic lives in **Services** (not Commands)
- Command handlers are thin facades that delegate to services
- Services are focused on a single responsibility (<500 lines)

### 2. Dependency Injection
- Services receive dependencies through constructors
- All services take `TSharedPtr<FServiceContext>` as first parameter
- Services never create their own dependencies

### 3. Result Types
- All service methods return `TResult<T>` (not raw pointers or JSON)
- This provides compile-time type safety
- Errors are explicit in the type system

### 4. Consistent Error Handling
- All errors use `VibeUE::ErrorCodes` constants
- Error messages are user-friendly and actionable
- No generic "error" strings

### 5. Standard Naming
- Services: `F{Domain}{Capability}Service`
- Command Handlers: `F{Domain}CommandHandler`
- Examples: `FBlueprintDiscoveryService`, `FWidgetComponentService`

---

## Phase 1 Infrastructure

### TResult<T> - Type-Safe Results

The `TResult<T>` wrapper provides type-safe success/error handling:

```cpp
#include "Core/Result.h"

// Return success with a value
TResult<UBlueprint*> Result = TResult<UBlueprint*>::Success(Blueprint);

// Return an error
TResult<UBlueprint*> Result = TResult<UBlueprint*>::Error(
    VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
    TEXT("Blueprint not found")
);

// Check result
if (Result.IsSuccess()) {
    UBlueprint* BP = Result.GetValue();
    // Use blueprint
} else {
    UE_LOG(LogTemp, Error, TEXT("Error %s: %s"), 
        *Result.GetErrorCode(), 
        *Result.GetErrorMessage());
}
```

For void operations (no return value):

```cpp
TResult<void> Result = TResult<void>::Success();

TResult<void> Result = TResult<void>::Error(
    VibeUE::ErrorCodes::BLUEPRINT_COMPILATION_FAILED,
    TEXT("Compilation failed")
);
```

### ErrorCodes - Centralized Error Constants

All error codes are defined in `Core/ErrorCodes.h`:

```cpp
#include "Core/ErrorCodes.h"

return TResult<UBlueprint*>::Error(
    VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
    FString::Printf(TEXT("Blueprint '%s' not found"), *Name)
);
```

Error codes are organized by domain:
- **1000-1099**: Parameter validation
- **2000-2099**: Blueprint operations
- **2100-2199**: Variable operations
- **2200-2299**: Component operations
- **2300-2399**: Property operations
- **2400-2499**: Node operations
- **2500-2599**: Function operations
- **2600-2699**: Graph operations
- **3000-3099**: UMG/Widget operations
- **4000-4099**: Asset operations
- **9000-9099**: System errors

### ServiceContext - Shared Resources

The `FServiceContext` provides access to shared resources:

```cpp
#include "Core/ServiceContext.h"

// Create and initialize context
TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
Context->Initialize();

// Access shared resources
IAssetRegistry* AssetRegistry = Context->GetAssetRegistry();

// Cleanup
Context->Shutdown();
```

### ServiceBase - Base Class for Services

All services should inherit from `FServiceBase`:

```cpp
#include "Services/Common/ServiceBase.h"

class VIBEUE_API FBlueprintDiscoveryService : public FServiceBase
{
public:
    explicit FBlueprintDiscoveryService(TSharedPtr<FServiceContext> InContext)
        : FServiceBase(InContext)
    {
    }

    TResult<UBlueprint*> FindBlueprint(const FString& Name)
    {
        // Validate parameters
        TResult<void> ValidationResult = ValidateString(Name, TEXT("Name"));
        if (ValidationResult.IsError())
        {
            return TResult<UBlueprint*>::Error(
                ValidationResult.GetErrorCode(),
                ValidationResult.GetErrorMessage()
            );
        }

        // Use logging helpers
        LogInfo(FString::Printf(TEXT("Finding blueprint: %s"), *Name));

        // Business logic here...
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Name);

        // Validate result
        TResult<UBlueprint*> Result = ValidateNotNull(
            Blueprint,
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            FString::Printf(TEXT("Blueprint '%s' not found"), *Name)
        );

        if (Result.IsSuccess())
        {
            LogInfo(TEXT("Blueprint found successfully"));
        }

        return Result;
    }
};
```

---

## Creating a New Service

### Step 1: Create Header File

Location: `Source/VibeUE/Public/Services/{Domain}/{ServiceName}.h`

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

/**
 * Service description
 * List of responsibilities
 */
class VIBEUE_API F{Domain}{Capability}Service : public FServiceBase
{
public:
    explicit F{Domain}{Capability}Service(TSharedPtr<FServiceContext> InContext);
    virtual ~F{Domain}{Capability}Service() = default;

    /**
     * Method description
     * @param ParamName Description
     * @return TResult with value or error
     */
    TResult<ReturnType> MethodName(const FString& ParamName);

private:
    // Private helper methods
};
```

### Step 2: Create Implementation File

Location: `Source/VibeUE/Private/Services/{Domain}/{ServiceName}.cpp`

```cpp
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/{Domain}/{ServiceName}.h"
#include "Core/ErrorCodes.h"

F{Domain}{Capability}Service::F{Domain}{Capability}Service(
    TSharedPtr<FServiceContext> InContext)
    : FServiceBase(InContext)
{
}

TResult<ReturnType> F{Domain}{Capability}Service::MethodName(
    const FString& ParamName)
{
    // 1. Validate parameters
    TResult<void> ValidationResult = ValidateString(ParamName, TEXT("ParamName"));
    if (ValidationResult.IsError())
    {
        return TResult<ReturnType>::Error(
            ValidationResult.GetErrorCode(),
            ValidationResult.GetErrorMessage()
        );
    }

    // 2. Log operation
    LogInfo(FString::Printf(TEXT("Performing operation: %s"), *ParamName));

    // 3. Business logic
    ReturnType Result = /* ... */;

    // 4. Validate result
    if (!Result)
    {
        FString Message = FString::Printf(TEXT("Operation failed for %s"), *ParamName);
        LogError(Message);
        return TResult<ReturnType>::Error(
            VibeUE::ErrorCodes::OPERATION_FAILED,
            Message
        );
    }

    // 5. Return success
    LogInfo(TEXT("Operation completed successfully"));
    return TResult<ReturnType>::Success(Result);
}
```

### Step 3: Update Build File

Add the new service to `VibeUE.Build.cs` if needed (usually automatic with proper folder structure).

---

## Service Design Guidelines

### Do's ✅

- **Do** inherit from `FServiceBase`
- **Do** return `TResult<T>` from all public methods
- **Do** use `VibeUE::ErrorCodes` constants for all errors
- **Do** validate all input parameters
- **Do** log important operations and errors
- **Do** keep services focused (<500 lines)
- **Do** add Doxygen comments to all public APIs
- **Do** use descriptive error messages

### Don'ts ❌

- **Don't** return raw pointers or JSON from services
- **Don't** use string literals for error codes
- **Don't** create dependencies (use constructor injection)
- **Don't** call other services directly (use composition)
- **Don't** mix business logic with JSON serialization
- **Don't** use `new`/`delete` (use `MakeShared`/`MakeUnique`)
- **Don't** create services >500 lines (split them)

---

## Command Handler Pattern

Command handlers are thin facades that:
1. Extract parameters from JSON
2. Call service methods
3. Convert `TResult<T>` to JSON responses

Example:

```cpp
// In command handler
TSharedPtr<FJsonObject> FBlueprintCommandHandler::HandleFindBlueprint(
    const TSharedPtr<FJsonObject>& Params)
{
    // 1. Extract parameters
    FString Name = Params->GetStringField(TEXT("name"));

    // 2. Call service
    TResult<UBlueprint*> Result = DiscoveryService->FindBlueprint(Name);

    // 3. Convert to JSON
    if (Result.IsError())
    {
        return FResponseSerializer::CreateErrorResponse(
            Result.GetErrorCode(),
            Result.GetErrorMessage()
        );
    }

    // Serialize success data
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("name"), Result.GetValue()->GetName());
    Data->SetStringField(TEXT("path"), Result.GetValue()->GetPathName());

    return FResponseSerializer::CreateSuccessResponse(Data);
}
```

---

## Testing Services

Services should be unit testable using Unreal's automation framework:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBlueprintDiscoveryService_FindBlueprint_ValidName,
    "VibeUE.Services.Blueprint.Discovery.FindBlueprint.ValidName",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FBlueprintDiscoveryService_FindBlueprint_ValidName::RunTest(
    const FString& Parameters)
{
    // Arrange
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    Context->Initialize();
    FBlueprintDiscoveryService Service(Context);
    
    // Act
    TResult<UBlueprint*> Result = Service.FindBlueprint(TEXT("/Game/Test/BP_Test"));

    // Assert
    TestTrue(TEXT("Result should be success"), Result.IsSuccess());
    TestNotNull(TEXT("Blueprint should not be null"), Result.GetValue());

    // Cleanup
    Context->Shutdown();
    return true;
}
```

---

## Migration from Old Code

### Before (Old Pattern):
```cpp
// Direct UE API calls in command handler
TSharedPtr<FJsonObject> FBlueprintCommands::HandleFindBlueprint(
    const TSharedPtr<FJsonObject>& Params)
{
    FString Name = Params->GetStringField(TEXT("name"));
    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Name);
    
    if (!BP)
    {
        return CreateErrorResponse(TEXT("Blueprint not found"));  // ❌ String literal
    }

    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("name"), BP->GetName());
    return Response;
}
```

### After (New Pattern):
```cpp
// Service layer - business logic
TResult<UBlueprint*> FBlueprintDiscoveryService::FindBlueprint(
    const FString& Name)
{
    TResult<void> ValidationResult = ValidateString(Name, TEXT("Name"));
    if (ValidationResult.IsError())
    {
        return TResult<UBlueprint*>::Error(
            ValidationResult.GetErrorCode(),
            ValidationResult.GetErrorMessage()
        );
    }

    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Name);
    
    return ValidateNotNull(
        BP,
        VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,  // ✅ Error code constant
        FString::Printf(TEXT("Blueprint '%s' not found"), *Name)
    );
}

// Command handler - thin facade
TSharedPtr<FJsonObject> FBlueprintCommandHandler::HandleFindBlueprint(
    const TSharedPtr<FJsonObject>& Params)
{
    FString Name = Params->GetStringField(TEXT("name"));
    TResult<UBlueprint*> Result = DiscoveryService->FindBlueprint(Name);
    
    if (Result.IsError())
    {
        return FResponseSerializer::CreateErrorResponse(
            Result.GetErrorCode(),
            Result.GetErrorMessage()
        );
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("name"), Result.GetValue()->GetName());
    
    return FResponseSerializer::CreateSuccessResponse(Data);
}
```

---

## Next Steps

### Phase 2: Blueprint Services (Upcoming)
- Implement 9 Blueprint services following these patterns
- Migrate logic from existing command files
- Maintain backward compatibility

### Phase 3: UMG Services (Upcoming)
- Implement 7 UMG services
- Follow same migration approach

### Phase 4: Command Handlers (Upcoming)
- Create thin command handler facades
- Remove old command classes

---

## References

- **Design Document**: `docs/CPP_REFACTORING_DESIGN.md`
- **Summary**: `docs/CPP_REFACTORING_SUMMARY.md`
- **Error Codes**: `Source/VibeUE/Public/Core/ErrorCodes.h`
- **Result Type**: `Source/VibeUE/Public/Core/Result.h`
- **Service Base**: `Source/VibeUE/Public/Services/Common/ServiceBase.h`

---

**Questions or Issues?**  
Refer to the full design document or create a GitHub issue.
