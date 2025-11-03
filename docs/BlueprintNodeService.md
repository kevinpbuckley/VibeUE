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
- `Public/Services/Blueprint/BlueprintNodeService.h` - Service interface (102 lines)
- `Private/Services/Blueprint/BlueprintNodeService.cpp` - Service implementation (848 lines)

## Dependencies
- Core infrastructure: `Result.h`, `ServiceBase.h`, `ErrorCodes.h`
- Unreal Engine: Blueprint, EdGraph, K2Node classes
- Commands: `CommonUtils.h`, `BlueprintReflection.h` (for node creation)
