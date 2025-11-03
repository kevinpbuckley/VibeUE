# BlueprintNodeService Testing Guide

## Overview
This document describes how to test the BlueprintNodeService implementation.

## Unit Testing (C++)

Since this is an Unreal Engine plugin, unit testing requires a full Unreal Engine editor environment. The service can be tested by:

1. Creating a test Blueprint in the Unreal Editor
2. Instantiating the service with a context
3. Calling service methods and verifying results

### Example Test Code

```cpp
#include "Services/Blueprint/BlueprintNodeService.h"
#include "Core/ServiceContext.h"
#include "Engine/Blueprint.h"

void TestBlueprintNodeService()
{
    // Setup
    TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
    TSharedPtr<FBlueprintNodeService> Service = MakeShared<FBlueprintNodeService>(Context);
    
    UBlueprint* TestBlueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Test/BP_TestBlueprint"));
    check(TestBlueprint);
    
    // Test: Create a node
    TResult<UK2Node*> CreateResult = Service->CreateNode(
        TestBlueprint,
        TEXT("EventGraph"),
        TEXT("K2Node_CallFunction"),
        FVector2D(100, 200)
    );
    
    check(CreateResult.IsSuccess());
    UK2Node* CreatedNode = CreateResult.GetValue();
    check(CreatedNode != nullptr);
    
    // Test: List nodes
    TResult<TArray<FString>> ListResult = Service->ListNodes(TestBlueprint, TEXT("EventGraph"));
    check(ListResult.IsSuccess());
    check(ListResult.GetValue().Num() > 0);
    
    // Test: Delete node
    FString NodeId = CreatedNode->NodeGuid.ToString();
    TResult<void> DeleteResult = Service->DeleteNode(TestBlueprint, NodeId);
    check(DeleteResult.IsSuccess());
    
    UE_LOG(LogTemp, Log, TEXT("BlueprintNodeService tests passed!"));
}
```

## Integration Testing (Python MCP)

The existing Python MCP tests should continue to work once this service is integrated with BlueprintNodeCommands.

### Test Files
- `Python/vibe-ue-main/Python/scripts/test_phase2_creation.py` - Tests node creation
- `Python/vibe-ue-main/Python/scripts/test_phase2_discovery.py` - Tests node discovery

### Running Python Tests

1. Ensure Unreal Engine editor is running with the VibeUE plugin loaded
2. Run the MCP server: `python vibe_ue_server.py`
3. Execute test scripts:

```bash
cd Python/vibe-ue-main/Python/scripts
python test_phase2_creation.py
python test_phase2_discovery.py
```

## Test Coverage Goals

According to the issue requirements:

- [ ] Test node creation with various types
- [ ] Test pin connections with type compatibility
- [ ] Test pin default value setting
- [ ] Test node discovery
- [ ] Target: >80% coverage

## Current Status

The service is implemented and ready for testing. Integration with BlueprintNodeCommands is pending completion of dependency issues (#17-20, #26).

### Test Scenarios

1. **Node Creation**
   - Create function call nodes
   - Create variable get/set nodes
   - Create event nodes
   - Create custom nodes
   - Verify node position
   - Verify node type

2. **Node Deletion**
   - Delete single node
   - Verify pin disconnection
   - Verify graph update

3. **Node Movement**
   - Move node to new position
   - Verify position update

4. **Pin Connections**
   - Connect compatible pins
   - Attempt incompatible pin connection (should fail)
   - Disconnect pins
   - Get pin connections

5. **Pin Configuration**
   - Set pin default values
   - Get pin default values
   - Configure multiple pins

6. **Node Discovery**
   - Discover available nodes with search term
   - Get node details
   - List all nodes in graph

## Error Handling Tests

Test that appropriate TResult errors are returned for:

- Null blueprint
- Invalid node ID
- Invalid graph name
- Invalid pin name
- Incompatible pin types
- Protected nodes (cannot delete)

## Performance Benchmarks

For large blueprints (100+ nodes):

- Node creation: <100ms
- Node deletion: <50ms
- Pin connection: <50ms
- List nodes: <200ms
- Node discovery: <500ms

## Manual Testing in Unreal Editor

1. Open a Blueprint in the editor
2. Note existing nodes
3. Use MCP tools to create/modify/delete nodes
4. Verify changes appear in the editor
5. Compile blueprint and verify no errors
6. Play in editor and verify functionality
