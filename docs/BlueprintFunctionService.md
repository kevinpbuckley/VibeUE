# BlueprintFunctionService

## Overview

The `FBlueprintFunctionService` is a focused service class extracted from `BlueprintNodeCommands.cpp` to handle Blueprint function and parameter management operations. This service follows the Service-Oriented Architecture pattern outlined in the VibeUE refactoring design document.

## Location

- **Header**: `Source/VibeUE/Public/Services/Blueprint/BlueprintFunctionService.h`
- **Implementation**: `Source/VibeUE/Private/Services/Blueprint/BlueprintFunctionService.cpp`

## Features

### Function Lifecycle Operations

- `CreateFunction()` - Create a new function in a Blueprint
- `DeleteFunction()` - Remove a function from a Blueprint
- `GetFunctionGraph()` - Get the GUID of a function's graph
- `ListFunctions()` - List all functions in a Blueprint

### Parameter Management

- `AddParameter()` - Add input, output, or return parameters
- `RemoveParameter()` - Remove parameters by name and direction
- `UpdateParameter()` - Update parameter type or name
- `ListParameters()` - List all parameters of a function

### Local Variable Management

- `AddLocalVariable()` - Add local variables to a function
- `RemoveLocalVariable()` - Remove local variables
- `ListLocalVariables()` - List all local variables in a function

## Architecture

### Result Type Pattern

All service methods return `TResult<T>` for type-safe error handling:

```cpp
TResult<UEdGraph*> result = service->CreateFunction(Blueprint, "MyFunction");
if (result.IsSuccess())
{
    UEdGraph* graph = result.GetValue();
    // Use the graph
}
else
{
    FString errorCode = result.GetErrorCode();
    FString errorMessage = result.GetErrorMessage();
    // Handle error
}
```

### Error Codes

The service uses centralized error codes from `Core/ErrorCodes.h`:

- `BLUEPRINT_NOT_FOUND` - Blueprint is null or invalid
- `FUNCTION_NOT_FOUND` - Function doesn't exist
- `FUNCTION_ALREADY_EXISTS` - Function name collision
- `PARAMETER_ALREADY_EXISTS` - Parameter name collision
- `VARIABLE_NOT_FOUND` - Local variable doesn't exist
- And more...

### Data Structures

#### FFunctionInfo
```cpp
struct FFunctionInfo
{
    FString Name;          // Function name
    FString GraphGuid;     // Graph GUID as string
    int32 NodeCount;       // Number of nodes in graph
};
```

#### FFunctionParameterInfo
```cpp
struct FFunctionParameterInfo
{
    FString Name;          // Parameter name
    FString Direction;     // "input", "out", or "return"
    FString Type;          // Type descriptor (e.g., "int", "object:Actor")
};
```

#### FLocalVariableInfo
```cpp
struct FLocalVariableInfo
{
    FString Name;
    FString FriendlyName;
    FString Type;
    FString DisplayType;
    FString DefaultValue;
    FString Category;
    FString PinCategory;
    FString Guid;
    bool bIsConst;
    bool bIsReference;
};
```

## Usage Examples

### Creating a Function

```cpp
#include "Services/Blueprint/BlueprintFunctionService.h"
#include "Core/ServiceContext.h"

// Create service
TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
FBlueprintFunctionService FunctionService(Context);

// Create function
UBlueprint* MyBlueprint = ...;
TResult<UEdGraph*> Result = FunctionService.CreateFunction(MyBlueprint, "CalculateScore");

if (Result.IsSuccess())
{
    UEdGraph* FunctionGraph = Result.GetValue();
    UE_LOG(LogTemp, Log, TEXT("Created function with GUID: %s"), *FunctionGraph->GraphGuid.ToString());
}
else
{
    UE_LOG(LogTemp, Error, TEXT("Failed: %s - %s"), 
        *Result.GetErrorCode(), *Result.GetErrorMessage());
}
```

### Adding Parameters

```cpp
// Add input parameter
TResult<void> AddInputResult = FunctionService.AddParameter(
    MyBlueprint, 
    "CalculateScore",
    "BaseScore",
    "int",
    "input"
);

// Add return value
TResult<void> AddReturnResult = FunctionService.AddParameter(
    MyBlueprint,
    "CalculateScore", 
    "", // Name not used for return
    "int",
    "return"
);

// Add output parameter
TResult<void> AddOutputResult = FunctionService.AddParameter(
    MyBlueprint,
    "CalculateScore",
    "WasSuccessful",
    "bool",
    "out"
);
```

### Managing Local Variables

```cpp
// Add local variable
TResult<void> AddLocalResult = FunctionService.AddLocalVariable(
    MyBlueprint,
    "CalculateScore",
    "TempScore",
    "int",
    "0",      // Default value
    false,    // Not const
    false     // Not reference
);

// List local variables
TResult<TArray<FLocalVariableInfo>> ListResult = 
    FunctionService.ListLocalVariables(MyBlueprint, "CalculateScore");

if (ListResult.IsSuccess())
{
    for (const FLocalVariableInfo& VarInfo : ListResult.GetValue())
    {
        UE_LOG(LogTemp, Log, TEXT("Local: %s (%s)"), *VarInfo.Name, *VarInfo.Type);
    }
}
```

### Type Descriptors

The service uses a simple type descriptor format:

- **Primitives**: `bool`, `byte`, `int`, `int64`, `float`, `double`, `string`, `name`, `text`
- **Structs**: `vector`, `vector2d`, `rotator`, `transform`
- **Objects**: `object:ClassName` (e.g., `object:Actor`, `object:Pawn`)
- **Arrays**: `array<type>` (e.g., `array<int>`, `array<object:Actor>`)

## Testing

### Manual Testing

Since there is no existing C++ test infrastructure in the VibeUE project, the service should be tested manually:

1. Create a test Blueprint in Unreal Editor
2. Use the service methods to:
   - Create new functions
   - Add parameters of various types
   - Add local variables
   - Modify parameters
   - Delete functions
3. Verify in the Blueprint editor that changes are applied correctly
4. Compile the Blueprint and verify no errors

### Integration Testing

The service is designed to integrate with existing command handlers. Test through:

1. Python MCP server tests (when Unreal is running)
2. Direct C++ integration in command handlers
3. Manual testing in Unreal Editor

### Expected Test Coverage Areas

If unit tests were to be created, they should cover:

- ✅ Function creation with valid/invalid names
- ✅ Function deletion (existing/non-existing)
- ✅ Parameter addition with all directions (input/out/return)
- ✅ Parameter removal and updates
- ✅ Local variable CRUD operations
- ✅ Type descriptor parsing for all supported types
- ✅ Error handling for null blueprints
- ✅ Error handling for non-existent functions
- ✅ Blueprint compilation after modifications

## Dependencies

### Foundation Classes

- `TResult<T>` - Type-safe result wrapper (Core/Result.h)
- `ErrorCodes` - Centralized error code constants (Core/ErrorCodes.h)
- `FServiceBase` - Base class for all services (Services/Common/ServiceBase.h)
- `FServiceContext` - Shared service context (Core/ServiceContext.h)

### Unreal Engine APIs

- `UBlueprint` - Blueprint asset class
- `UEdGraph` - Graph representation
- `UK2Node_FunctionEntry` - Function entry node
- `UK2Node_FunctionResult` - Function result/output node
- `FBlueprintEditorUtils` - Blueprint manipulation utilities
- `FKismetEditorUtilities` - Kismet graph utilities

## File Size

- **Header**: 186 lines (well under 350 line target)
- **Implementation**: 903 lines (includes comprehensive type parsing and helper functions)
- **Total**: 1,089 lines

The implementation is larger than the initial 350-line estimate because it includes:
- Complete type descriptor parsing (~150 lines)
- Complete type description generation (~80 lines)  
- Helper functions for finding nodes (~80 lines)
- Comprehensive error handling for all operations
- Detailed logging and validation

## Migration Notes

This service extracts functionality from `BlueprintNodeCommands.cpp` without modifying the original file. Future work:

1. Update `BlueprintNodeCommands::HandleManageBlueprintFunction()` to use this service
2. Remove duplicate code from `BlueprintNodeCommands.cpp`
3. Create similar services for other Blueprint operations

## Performance Considerations

- All operations mark the Blueprint as modified and trigger compilation
- Consider batching multiple operations before compilation for better performance
- The service performs no caching - each call queries the Blueprint directly

## Thread Safety

This service is **not thread-safe**. All operations must be called from the game thread, as they interact with Unreal Engine's Blueprint system which is not thread-safe.

## Future Enhancements

Potential improvements for future versions:

1. **Batch Operations**: Allow multiple parameter adds/removes before compilation
2. **Caching**: Cache function graphs to avoid repeated lookups
3. **Async Support**: Wrap long operations in `TFuture<TResult<T>>` for async execution
4. **Function Properties**: Implement `UpdateFunctionProperties()` for pure/const flags
5. **Validation**: Add pre-flight validation before making changes
6. **Undo/Redo**: Integration with Unreal's transaction system

## License

This code is part of the VibeUE project and follows the same MIT license.
