# Phase 1: Reflection Test Prompts

These prompts test the reflection infrastructure and type discovery.

## Reflection Service Tests

### Test: Discover All Modifier Types
Help me discover all available Enhanced Input modifier types in the system. I want to see the complete list of modifiers that can be used to modify input values, including any inheritance hierarchy information.

### Test: Get Modifier Type Metadata
I need detailed information about the "SwizzleAxis" modifier type. Can you retrieve its properties, default values, and any constraints it has?

### Test: Discover All Trigger Types
Show me all the Enhanced Input trigger types available in the engine. I want to understand what different trigger types exist and how they're organized.

### Test: Get Trigger Type Metadata
I need complete metadata for the "Pressed" trigger type including its properties and how it works. What are the default values and configuration options?

### Test: Get Detailed Modifier Information
Retrieve comprehensive information about the "Deadzone" modifier. What properties can I configure on it? What are the recommended settings?

### Test: Get Detailed Trigger Information
Give me full details about the "Hold" trigger. What parameters does it support and how should I configure it for typical use cases?

## Combined Phase 1 Workflow Tests

### Test: Complete Type Discovery Workflow
Walk me through the process of discovering what modifiers and triggers are available, then validate that I can use them together in an input action setup.

### Test: Research Before Implementation
Before implementing a gamepad-based input system, discover what trigger types would work best with gamepad input, and validate a mapping configuration.

### Test: Metadata-Driven Configuration
Get metadata for common modifier types (Deadzone, Normalize, Swizzle) and suggest what combinations would work well together for typical game inputs.

### Test: Validate Multiple Actions
I want to set up several input actions for an RPG game. Can you validate each one: "IA_Move", "IA_Attack", "IA_Cast", "IA_Interact"?
## Combined Phase 1 Workflow Tests

### Test: Complete Type Discovery Workflow
Walk me through the process of discovering what modifiers and triggers are available, and show me how to use them together in an input action setup.

### Test: Research Before Implementation
Before implementing a gamepad-based input system, discover what trigger types would work best with gamepad input.

### Test: Metadata-Driven Configuration
Get metadata for common modifier types (Deadzone, Scalar, Swizzle) and suggest what combinations would work well together for typical game inputs.