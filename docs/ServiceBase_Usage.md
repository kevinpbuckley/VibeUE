# ServiceBase Usage Guide

## Overview
ServiceBase is the foundation class for all VibeUE services, providing common functionality for validation, logging, and lifecycle management.

## Quick Start

### Creating a Service

```cpp
// MyService.h
#pragma once

#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

class VIBEUE_API FMyService : public FServiceBase
{
public:
    explicit FMyService(TSharedPtr<FServiceContext> InContext)
        : FServiceBase(InContext)
    {
    }

    // Override lifecycle methods if needed
    virtual void Initialize() override
    {
        FServiceBase::Initialize();
        // Your initialization code here
        LogInfo(TEXT("MyService initialized"));
    }

    virtual void Shutdown() override
    {
        // Your cleanup code here
        LogInfo(TEXT("MyService shutting down"));
        FServiceBase::Shutdown();
    }

    // Service methods return TResult<T> for type safety
    TResult<int32> CalculateValue(const FString& Input)
    {
        // Use validation helpers
        TResult<void> ValidationResult = ValidateNotEmpty(Input, TEXT("Input"));
        if (ValidationResult.IsError())
        {
            return TResult<int32>::Error(
                ValidationResult.GetErrorCode(),
                ValidationResult.GetErrorMessage()
            );
        }

        // Your business logic here
        int32 Result = Input.Len();
        
        LogInfo(FString::Printf(TEXT("Calculated value: %d"), Result));
        
        return TResult<int32>::Success(Result);
    }
};
```

### Using the Service

```cpp
// Create service context
TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
Context->SetLogCategoryName(TEXT("MyApp"));

// Create service
TSharedPtr<FMyService> Service = MakeShared<FMyService>(Context);
Service->Initialize();

// Use service methods
TResult<int32> Result = Service->CalculateValue(TEXT("Hello"));
if (Result.IsSuccess())
{
    int32 Value = Result.GetValue();
    // Use value
}
else
{
    UE_LOG(LogTemp, Error, TEXT("Error: %s - %s"), 
        *Result.GetErrorCode(), 
        *Result.GetErrorMessage());
}

// Cleanup
Service->Shutdown();
```

## Validation Helpers

### ValidateNotEmpty
```cpp
TResult<void> Result = ValidateNotEmpty(MyString, TEXT("MyString"));
if (Result.IsError())
{
    // Handle error: PARAM_EMPTY
}
```

### ValidateNotNull
```cpp
TResult<void> Result = ValidateNotNull(MyPointer, TEXT("MyPointer"));
if (Result.IsError())
{
    // Handle error: PARAM_INVALID
}
```

### ValidateRange
```cpp
TResult<void> Result = ValidateRange(Value, 0, 100, TEXT("Value"));
if (Result.IsError())
{
    // Handle error: PARAM_OUT_OF_RANGE
}
```

### ValidateArray
```cpp
TResult<void> Result = ValidateArray(MyArray, TEXT("MyArray"));
if (Result.IsError())
{
    // Handle error: PARAM_EMPTY
}
```

## Logging Helpers

```cpp
// Info level
LogInfo(TEXT("Operation completed successfully"));

// Warning level
LogWarning(TEXT("Deprecated method called"));

// Error level
LogError(TEXT("Failed to process request"));
```

Logs are formatted as: `[CategoryName] Message`

## Error Handling with TResult

### Basic Error Handling
```cpp
TResult<UBlueprint*> FindBlueprint(const FString& Name)
{
    if (Name.IsEmpty())
    {
        return TResult<UBlueprint*>::Error(
            VibeUE::ErrorCodes::PARAM_EMPTY,
            TEXT("Blueprint name cannot be empty")
        );
    }

    UBlueprint* BP = LoadBlueprint(Name);
    if (!BP)
    {
        return TResult<UBlueprint*>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            FString::Printf(TEXT("Blueprint '%s' not found"), *Name)
        );
    }

    return TResult<UBlueprint*>::Success(BP);
}
```

### Chaining Operations
```cpp
// Map to transform values
TResult<int32> LengthResult = GetStringResult()
    .Map([](const FString& Str) { return Str.Len(); });

// FlatMap to chain operations that return TResult
TResult<UBlueprint*> Result = GetBlueprintName()
    .FlatMap([this](const FString& Name) { 
        return FindBlueprint(Name); 
    });
```

### Void Operations
```cpp
TResult<void> ValidateInput(const FString& Input)
{
    if (Input.IsEmpty())
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PARAM_EMPTY,
            TEXT("Input cannot be empty")
        );
    }
    return TResult<void>::Success();
}
```

## Available Error Codes

See `Public/Core/ErrorCodes.h` for the full list. Common codes:

**Parameter Validation (1000s)**
- `PARAM_MISSING`
- `PARAM_INVALID`
- `PARAM_EMPTY`
- `PARAM_OUT_OF_RANGE`

**Blueprint (2000s)**
- `BLUEPRINT_NOT_FOUND`
- `BLUEPRINT_LOAD_FAILED`
- `BLUEPRINT_COMPILATION_FAILED`

**Components (2200s)**
- `COMPONENT_NOT_FOUND`
- `COMPONENT_ADD_FAILED`

**Properties (2300s)**
- `PROPERTY_NOT_FOUND`
- `PROPERTY_SET_FAILED`

**System (9000s)**
- `INTERNAL_ERROR`
- `OPERATION_NOT_SUPPORTED`

## Best Practices

1. **Always use TResult<T>** for methods that can fail
2. **Validate inputs early** using the provided helpers
3. **Use standardized error codes** from `VibeUE::ErrorCodes`
4. **Provide descriptive error messages** that help users understand what went wrong
5. **Log important operations** using LogInfo/LogWarning/LogError
6. **Keep services focused** - follow Single Responsibility Principle
7. **Initialize and Shutdown** properly to manage resources

## Testing

When testing services, create a test helper class to expose protected methods:

```cpp
class FTestMyService : public FMyService
{
public:
    using FMyService::FMyService;
    
    // Expose protected methods for testing
    using FMyService::ValidateNotEmpty;
    using FMyService::LogInfo;
};
```

See `ServiceBaseTests.cpp` for comprehensive examples.
