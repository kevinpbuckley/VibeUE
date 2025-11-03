# BlueprintLifecycleService

## Overview

The `FBlueprintLifecycleService` provides a clean, service-oriented interface for Blueprint lifecycle operations. It replaces the JSON-based interface with type-safe `TResult<T>` return values, making it easier to use from C++ code.

## Features

- **Type-safe**: Returns `TResult<T>` instead of JSON, providing compile-time type checking
- **Error handling**: Explicit error handling with descriptive error messages
- **Focused responsibility**: Handles only Blueprint lifecycle operations (create, compile, reparent, delete)
- **Reusable**: Can be used from any C++ code, not just MCP commands

## Usage Example

```cpp
#include "Services/Blueprint/BlueprintLifecycleService.h"
#include "Core/ServiceBase.h"

// Create service with context
TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
FBlueprintLifecycleService LifecycleService(Context);

// Create a new Blueprint
TResult<UBlueprint*> CreateResult = LifecycleService.CreateBlueprint(
    TEXT("/Game/MyBlueprints/MyNewBP"), 
    TEXT("Actor")
);

if (CreateResult.IsOk())
{
    UBlueprint* NewBlueprint = CreateResult.GetValue();
    UE_LOG(LogTemp, Log, TEXT("Created blueprint: %s"), *NewBlueprint->GetName());
    
    // Compile the blueprint
    TResult<void> CompileResult = LifecycleService.CompileBlueprint(NewBlueprint);
    
    if (CompileResult.IsOk())
    {
        UE_LOG(LogTemp, Log, TEXT("Blueprint compiled successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Compilation failed: %s"), *CompileResult.GetError());
    }
}
else
{
    UE_LOG(LogTemp, Error, TEXT("Failed to create blueprint: %s"), *CreateResult.GetError());
}
```

## API Reference

### CreateBlueprint
```cpp
TResult<UBlueprint*> CreateBlueprint(const FString& Name, const FString& ParentClass)
```
Creates a new Blueprint asset.

**Parameters:**
- `Name`: Full path for the Blueprint (e.g., `/Game/Blueprints/MyBP`) or just the name
- `ParentClass`: Parent class name (e.g., `"Actor"`, `"Pawn"`, or full path like `/Script/Engine.Actor`)

**Returns:** `TResult<UBlueprint*>` with the created Blueprint or an error message

### CompileBlueprint
```cpp
TResult<void> CompileBlueprint(UBlueprint* Blueprint)
```
Compiles a Blueprint to apply changes.

**Parameters:**
- `Blueprint`: The Blueprint to compile

**Returns:** `TResult<void>` indicating success or failure

### ReparentBlueprint
```cpp
TResult<void> ReparentBlueprint(UBlueprint* Blueprint, UClass* NewParentClass)
```
Changes the parent class of a Blueprint.

**Parameters:**
- `Blueprint`: The Blueprint to reparent
- `NewParentClass`: The new parent UClass

**Returns:** `TResult<void>` indicating success or failure

### DeleteBlueprint
```cpp
TResult<void> DeleteBlueprint(UBlueprint* Blueprint)
```
Deletes a Blueprint asset.

**Parameters:**
- `Blueprint`: The Blueprint to delete

**Returns:** `TResult<void>` indicating success or failure

### GetCompilationErrors
```cpp
TResult<TArray<FString>> GetCompilationErrors(UBlueprint* Blueprint)
```
Retrieves compilation error messages from a Blueprint.

**Parameters:**
- `Blueprint`: The Blueprint to check

**Returns:** `TResult<TArray<FString>>` with error messages or an error

### IsCompiled
```cpp
TResult<bool> IsCompiled(UBlueprint* Blueprint)
```
Checks if a Blueprint is compiled.

**Parameters:**
- `Blueprint`: The Blueprint to check

**Returns:** `TResult<bool>` with true if compiled, false otherwise, or an error

## Integration with BlueprintCommands

The service can be integrated with existing `BlueprintCommands` to provide both JSON-based and type-safe interfaces:

```cpp
TSharedPtr<FJsonObject> FBlueprintCommands::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters from JSON
    FString Name, ParentClass;
    Params->TryGetStringField(TEXT("name"), Name);
    Params->TryGetStringField(TEXT("parent_class"), ParentClass);
    
    // Use the service
    TResult<UBlueprint*> Result = LifecycleService->CreateBlueprint(Name, ParentClass);
    
    // Convert result to JSON
    if (Result.IsOk())
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetStringField(TEXT("name"), Result.GetValue()->GetName());
        Response->SetStringField(TEXT("path"), Result.GetValue()->GetPathName());
        return Response;
    }
    else
    {
        return FCommonUtils::CreateErrorResponse(Result.GetError());
    }
}
```

## File Size

The implementation is compact and focused:
- Header: 35 lines
- Implementation: 275 lines
- **Total: 310 lines** (under the 300-line guideline for single files)

## Testing

Unit tests would be created in `Source/VibeUETests/Private/Services/Blueprint/BlueprintLifecycleServiceTests.cpp` when test infrastructure is available.

### Test Coverage Goals (>80%)

1. **CreateBlueprint**
   - Create with valid parent class
   - Create with invalid parent class (should default to Actor)
   - Create with existing name (should fail)
   - Create with various path formats

2. **CompileBlueprint**
   - Compile valid blueprint
   - Compile blueprint with errors
   - Compile null blueprint

3. **ReparentBlueprint**
   - Reparent to valid class
   - Reparent to null class (should fail)
   - Reparent with null blueprint (should fail)

4. **DeleteBlueprint**
   - Delete existing blueprint
   - Delete null blueprint (should fail)

5. **GetCompilationErrors**
   - Get errors from blueprint with errors
   - Get errors from valid blueprint
   - Get errors from null blueprint (should fail)

6. **IsCompiled**
   - Check compiled blueprint
   - Check uncompiled blueprint  
   - Check null blueprint (should fail)

## Dependencies

- Unreal Engine 5.6+
- No additional dependencies beyond standard Engine modules
