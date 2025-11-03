# BlueprintNodeService

## Overview
The `FBlueprintNodeService` is a focused service class that provides Blueprint node operations (create, connect, configure) extracted from `BlueprintNodeCommands.cpp` as part of Phase 2, Task 7.

## Architecture
- **Returns**: `TResult<T>` for type-safe error handling instead of JSON
- **Dependencies**: Uses `FServiceBase` for common service functionality
- **Error Handling**: Centralized error codes from `Core/ErrorCodes.h`

## Methods

### Node Lifecycle
- `CreateNode()` - Create a new node in a Blueprint graph
- `DeleteNode()` - Delete a node from a Blueprint
- `MoveNode()` - Move a node to a new position

### Pin Connections
- `ConnectPins()` - Connect two pins between nodes
- `DisconnectPins()` - Disconnect pins
- `GetPinConnections()` - Get all connections for a node

### Node Configuration
- `SetPinDefaultValue()` - Set the default value of a pin
- `GetPinDefaultValue()` - Get the default value of a pin
- `ConfigureNode()` - Configure multiple pin default values at once

### Node Discovery
- `DiscoverAvailableNodes()` - Find available node types
- `GetNodeDetails()` - Get detailed information about a specific node
- `ListNodes()` - List all nodes in a graph

## Integration Status
This service is currently standalone and ready for integration with `BlueprintNodeCommands`. 
The integration will happen as part of the broader Phase 2 refactoring effort once dependencies 
(Issues #17-20, #26) are completed.

## Example Usage

```cpp
// Create a service instance
TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
TSharedPtr<FBlueprintNodeService> NodeService = MakeShared<FBlueprintNodeService>(Context);

// Create a node
TResult<UK2Node*> Result = NodeService->CreateNode(
    Blueprint, 
    TEXT("EventGraph"), 
    TEXT("K2Node_CallFunction"),
    FVector2D(100, 200)
);

if (Result.IsSuccess())
{
    UK2Node* NewNode = Result.GetValue();
    // Use the node...
}
else
{
    UE_LOG(LogTemp, Error, TEXT("Failed to create node: %s"), *Result.GetErrorMessage());
}
```

## Testing
Python MCP tests in `/Python/vibe-ue-main/Python/scripts/` will continue to work once this service 
is integrated with the existing command handlers.

## File Structure
- `Public/Services/Blueprint/BlueprintNodeService.h` - Service interface (92 lines)
- `Private/Services/Blueprint/BlueprintNodeService.cpp` - Service implementation (662 lines)
- `Public/Services/Blueprint/BlueprintNodeServiceHelpers.h` - Helper utilities interface (29 lines)
- `Private/Services/Blueprint/BlueprintNodeServiceHelpers.cpp` - Helper utilities implementation (187 lines)

**Total**: 970 lines (main service: 754 lines including header)

## Line Count Analysis
The issue specifies "<450 lines" for the service. The current implementation:
- Main service (.h + .cpp): 754 lines
- Helper utilities (.h + .cpp): 216 lines

The implementation exceeds the target but provides:
- Complete functionality for all specified operations
- Comprehensive error handling with typed TResult returns
- Proper transaction support for undo/redo
- Blueprint modification tracking
- Separation of concerns (helpers in separate class)

## Optimization Opportunities
To reach the <450 line target, consider:
1. Further simplification of error handling
2. Removing some of the defensive null checks
3. Combining similar operations
4. Using more delegation to existing utilities (CommonUtils, BlueprintReflection)

## Dependencies
- Core infrastructure: `Result.h`, `ServiceBase.h`, `ErrorCodes.h`
- Unreal Engine: Blueprint, EdGraph, K2Node classes
- Commands: `CommonUtils.h`, `BlueprintReflection.h` (for node creation)
