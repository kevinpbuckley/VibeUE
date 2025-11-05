# Blueprint Type Definitions

This directory contains type definitions (DTOs - Data Transfer Objects) used by Blueprint services.

## Purpose

These headers provide a clean separation between data structures and service behavior:
- **Type headers** define data structures used to transfer information
- **Service headers** define service classes and their methods

## Organization

Types are organized by functional domain rather than by which service first used them. This allows multiple services to share types without creating circular dependencies.

## Available Type Headers

### BlueprintTypes.h
Common Blueprint type definitions:
- `FClassInfo` - Information about a UE class
- `FBlueprintInfo` - Metadata about a Blueprint asset

**Use when:** Working with Blueprint discovery, class information, or general Blueprint metadata.

### FunctionTypes.h
Function-related type definitions:
- `FFunctionInfo` - Information about a Blueprint function
- `FFunctionParameterInfo` - Function parameter metadata
- `FLocalVariableInfo` - Local variable information

**Use when:** Working with Blueprint functions, parameters, or local variables.

### ComponentTypes.h
Component and event type definitions:
- `FComponentInfo` - Information about a Blueprint component
- `FComponentEventsResult` - Result of component event discovery
- `FComponentEventResult` - Result of component event creation
- `FParameterInfo` - Delegate parameter information
- `FComponentEventInfo` - Component event metadata from reflection

**Use when:** Working with Blueprint components, component events, or component hierarchies.

### PropertyTypes.h
Property type definitions:
- `FPropertyInfo` - Blueprint property metadata

**Use when:** Working with Blueprint properties or property reflection.

**Note:** UMG has its own `FPropertyInfo` in `Services/UMG/Types/WidgetPropertyTypes.h`. Include the appropriate one based on your domain.

### GraphTypes.h
Graph and node type definitions:
- `FGraphInfo` - Information about a Blueprint graph
- `FNodeSummary` - Summary of a node in a graph

**Use when:** Working with Blueprint graphs or nodes.

### ReflectionTypes.h
Type reflection and discovery definitions:
- `FNodeTypeInfo` - Information about available node types
- `FNodeTypeSearchCriteria` - Search criteria for node discovery
- `FInputKeyResult` - Result structure for input key discovery

**Use when:** Working with type discovery, node palette, or Blueprint reflection.

## Usage Guidelines

### Include What You Use
Only include the type headers you actually need:

```cpp
// Good - only includes needed types
#include "Services/Blueprint/Types/FunctionTypes.h"
#include "Services/Blueprint/Types/GraphTypes.h"

// Bad - includes entire service for just a type
#include "Services/Blueprint/BlueprintFunctionService.h"  // Don't do this!
```

### Forward Declarations
If you only need a pointer or reference to a type, use forward declarations instead of includes:

```cpp
// In header file
struct FFunctionInfo;  // Forward declaration

class MyClass
{
    void ProcessFunction(const FFunctionInfo& Info);  // OK - reference
};
```

```cpp
// In implementation file
#include "Services/Blueprint/Types/FunctionTypes.h"  // Full definition needed here

void MyClass::ProcessFunction(const FFunctionInfo& Info)
{
    // Now we can access Info's members
}
```

### Avoid Circular Dependencies
Type headers should NEVER include service headers. If you find yourself needing to do this, you probably have the wrong architecture.

## Benefits

1. **Clear Dependencies**: Easy to see what data structures a service uses
2. **Reduced Coupling**: Services share types without depending on each other
3. **Faster Compilation**: Changes to types don't require recompiling all services
4. **Better Testing**: Test files can use types without pulling in service implementations
5. **Clearer API**: Easy to identify the data structures that form the public API

## Migration from Old Structure

If you have code that includes service headers just for their types, update to include type headers directly:

**Before:**
```cpp
#include "Services/Blueprint/BlueprintFunctionService.h"  // For FFunctionInfo

void MyFunction(const FFunctionInfo& Info) { }
```

**After:**
```cpp
#include "Services/Blueprint/Types/FunctionTypes.h"  // Only the type

void MyFunction(const FFunctionInfo& Info) { }
```

## Related Documentation

- See `Services/UMG/Types/README.md` for UMG type definitions
- See service headers for how types are used in context
