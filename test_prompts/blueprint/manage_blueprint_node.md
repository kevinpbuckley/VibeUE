# manage_blueprint_node - Test Prompt

## Purpose

This test validates Blueprint node management including creating nodes, connecting nodes, positioning nodes, and manipulating the Blueprint event graph through the VibeUE MCP tools.

## Prerequisites

- Unreal Engine is running with your project loaded
- VibeUE plugin is enabled
- MCP client is connected to the VibeUE server
- Connection verified with `check_unreal_connection`
- A test Blueprint exists with at least one function or the Event Graph
- Familiarity with Blueprint node concepts (execution pins, data pins, node connections)

## Test Steps

### Test 1: Get Available Nodes

1. Ask your AI assistant: "What Blueprint nodes are available for creating in 'BP_TestActor'?"

2. Review the list of available node types

### Test 2: Create a Print String Node

3. Ask your AI assistant: "In the Event Graph of 'BP_TestActor', create a 'Print String' node"

4. Verify the node was created

### Test 3: Create an Event BeginPlay Node

5. Ask your AI assistant: "Create an 'Event BeginPlay' node in the Event Graph of 'BP_TestActor'"

6. Confirm the event node was added

### Test 4: Connect Nodes

7. Ask your AI assistant: "Connect the execution output of 'Event BeginPlay' to the execution input of 'Print String' in 'BP_TestActor'"

8. Verify the connection was made

### Test 5: Set Node Property

9. Ask your AI assistant: "Set the 'In String' property of the Print String node to 'Hello from Blueprint!' in 'BP_TestActor'"

10. Verify the string value was set

### Test 6: Create Variable Get Node

11. Ask your AI assistant: "Create a Get node for the 'Health' variable in the Event Graph of 'BP_TestActor'"

12. Confirm the variable getter node was created

### Test 7: Create a Branch Node

13. Ask your AI assistant: "Create a Branch node in the Event Graph of 'BP_TestActor'"

14. Verify the branch/conditional node was added

### Test 8: Position Nodes

15. Ask your AI assistant: "Position the Print String node at coordinates (500, 200) in the Event Graph of 'BP_TestActor'"

16. Confirm the node was repositioned

### Test 9: Get Node Details

17. Ask your AI assistant: "Get detailed information about the Print String node in 'BP_TestActor'"

18. Review the node's properties, pins, and connections

### Test 10: Delete Node (if supported)

19. Ask your AI assistant: "Delete the Branch node from the Event Graph of 'BP_TestActor'"

20. Verify the node was removed

### Test 11: Error Handling - Invalid Node Type

21. Ask your AI assistant: "Create a node of type 'InvalidNodeType' in 'BP_TestActor'"

22. Verify appropriate error handling

## Expected Outcomes

### Test 1 - Get Available Nodes
- ✅ Returns a comprehensive list of available node types
- ✅ Includes common nodes (Print String, Branch, Sequence, etc.)
- ✅ Includes mathematical nodes (Add, Multiply, etc.)
- ✅ Includes event nodes (Event BeginPlay, Event Tick, etc.)
- ✅ List is categorized or searchable

### Test 2 - Create Print String Node
- ✅ Print String node is created in the Event Graph
- ✅ Node has correct pins (execution in/out, In String input)
- ✅ Confirmation message includes node name and type
- ✅ Node is created at a default position

### Test 3 - Create Event BeginPlay
- ✅ Event BeginPlay node is created successfully
- ✅ Node appears in the Event Graph
- ✅ Node has execution output pin
- ✅ Only one Event BeginPlay node can exist (or appropriate handling)

### Test 4 - Connect Nodes
- ✅ Execution pin of Event BeginPlay connected to Print String
- ✅ Connection is visible in the graph
- ✅ Confirmation message indicates successful connection
- ✅ Both nodes remain in valid state

### Test 5 - Set Node Property
- ✅ In String value is set to "Hello from Blueprint!"
- ✅ Property change is confirmed
- ✅ Node retains all other properties
- ✅ String value is properly formatted

### Test 6 - Create Variable Get Node
- ✅ Get node for 'Health' variable is created
- ✅ Node shows correct variable name
- ✅ Node has output pin with correct type (Float)
- ✅ Confirmation includes variable name

### Test 7 - Create Branch Node
- ✅ Branch node is created successfully
- ✅ Node has execution input, True output, False output
- ✅ Node has boolean Condition input pin
- ✅ Confirmation message includes node type

### Test 8 - Position Nodes
- ✅ Print String node is moved to (500, 200)
- ✅ Position change is confirmed
- ✅ Node connections remain intact
- ✅ Other nodes are not affected

### Test 9 - Get Node Details
- ✅ Returns complete node information
- ✅ Shows node type and position
- ✅ Lists all input and output pins
- ✅ Shows current pin values
- ✅ Shows connections to other nodes

### Test 10 - Delete Node
- ✅ Branch node is removed from the graph
- ✅ Node no longer appears in the graph
- ✅ Any connections to the node are removed
- ✅ Confirmation message indicates successful deletion

### Test 11 - Error Handling
- ✅ Clear error message indicating invalid node type
- ✅ No crash or unexpected behavior
- ✅ Graph remains unchanged
- ✅ Helpful suggestion (e.g., list available nodes)

## Notes

- Node creation requires specifying the graph (Event Graph, function name, etc.)
- Node types use Unreal Engine's internal node class names
- Execution pins connect the flow of Blueprint logic
- Data pins connect values between nodes
- Some nodes (like Event nodes) can only appear once per graph
- Node positioning uses X,Y coordinates in the graph space
- Pin connections require matching types (execution-to-execution, float-to-float, etc.)
- Always compile the Blueprint after modifying the graph
- Use `get_help(topic="node-tools")` for detailed node manipulation guidance
- Complex graphs benefit from organizing nodes into comments or functions

## Cleanup

After testing, the test nodes remain in the Blueprint graph. You can:
- Delete nodes manually in the Blueprint editor
- Clear the entire Event Graph
- Delete the entire test Blueprint
- Keep them to see your test Blueprint in action
