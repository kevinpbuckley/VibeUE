# Phase 1: Reflection & Discovery Test Prompts

These prompts test the reflection infrastructure, type discovery, and validation services.

## Reflection Service Tests

### Test: Discover All Modifier Types
Help me discover all available Enhanced Input modifier types in the system. I want to see the complete list of modifiers that can be used to modify input values, including any inheritance hierarchy information.

### Test: Get Modifier Type Metadata
I need detailed information about the "SwizzleAxis" modifier type. Can you retrieve its properties, default values, and any constraints it has?

### Test: Discover All Trigger Types
Show me all the Enhanced Input trigger types available in the engine. I want to understand what different trigger types exist and how they're organized.

### Test: Get Trigger Type Metadata
I need complete metadata for the "Pressed" trigger type including its properties and how it works. What are the default values and configuration options?

## Discovery Service Tests

### Test: Enumerate All Modifiers with Details
List every modifier class available in the Enhanced Input system. Include information about each modifier's category, inheritance, and what it does.

### Test: Enumerate All Triggers with Organization
Get all trigger types organized by category. I want to see how triggers are grouped and what each one is used for.

### Test: Get Detailed Modifier Information
Retrieve comprehensive information about the "Deadzone" modifier. What properties can I configure on it? What are the recommended settings?

### Test: Get Detailed Trigger Information
Give me full details about the "Held" trigger. What parameters does it support and how should I configure it for typical use cases?

## Validation Service Tests

### Test: Validate a New Input Action
I want to create an input action called "IA_Jump" with a value type of "Digital" (simple on/off). Can you validate that this configuration is correct before I create it?

### Test: Validate Mapping Context Configuration
I'm planning to create an input mapping context named "IMC_Combat" with priority 10. Is this a valid configuration?

### Test: Check for Action Naming Conflicts
Before creating new input actions, can you check if there are any naming conflicts or validation issues with the action names I'm planning to use?

### Test: Validate Complex Input Action Setup
I want to create a "IA_Look" action with 2D axis input. Is this the correct value type to use for mouse look or controller look input?

## Combined Phase 1 Workflow Tests

### Test: Complete Type Discovery Workflow
Walk me through the process of discovering what modifiers and triggers are available, then validate that I can use them together in an input action setup.

### Test: Research Before Implementation
Before implementing a gamepad-based input system, discover what trigger types would work best with gamepad input, and validate a mapping configuration.

### Test: Metadata-Driven Configuration
Get metadata for common modifier types (Deadzone, Normalize, Swizzle) and suggest what combinations would work well together for typical game inputs.

### Test: Validate Multiple Actions
I want to set up several input actions for an RPG game. Can you validate each one: "IA_Move", "IA_Attack", "IA_Cast", "IA_Interact"?
