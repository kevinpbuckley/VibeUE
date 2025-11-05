# UMG Widget Type Definitions

This directory contains type definitions (DTOs - Data Transfer Objects) used by UMG Widget services.

## Purpose

These headers provide a clean separation between data structures and service behavior:
- **Type headers** define data structures used to transfer information
- **Service headers** define service classes and their methods

## Organization

Types are organized by functional domain rather than by which service first used them. This allows multiple services to share types without creating circular dependencies.

## Available Type Headers

### WidgetTypes.h
Core widget type definitions:
- `FWidgetComponentInfo` - Information about a widget component in a widget tree
- `FWidgetInfo` - Metadata about a Widget Blueprint asset
- `FWidgetTypeInfo` - Information about a widget class type

**Use when:** Working with widget discovery, widget hierarchies, or widget type information.

### WidgetPropertyTypes.h
Widget property and event type definitions:
- `FPropertyInfo` - Widget property metadata
- `FEventInfo` - Widget event information

**Use when:** Working with widget properties or widget events.

**Note:** Blueprint has its own `FPropertyInfo` in `Services/Blueprint/Types/PropertyTypes.h`. The UMG version has additional fields specific to widgets (like `EnumValues`). Include the appropriate one based on your domain.

### WidgetStyleTypes.h
Widget styling type definitions:
- `FVibeWidgetStyle` - Complete widget style definition including colors, fonts, padding, and alignment

**Use when:** Working with widget styling, themes, or visual customization.

## Usage Guidelines

### Include What You Use
Only include the type headers you actually need:

```cpp
// Good - only includes needed types
#include "Services/UMG/Types/WidgetTypes.h"
#include "Services/UMG/Types/WidgetPropertyTypes.h"

// Bad - includes entire service for just a type
#include "Services/UMG/WidgetComponentService.h"  // Don't do this!
```

### Forward Declarations
If you only need a pointer or reference to a type, use forward declarations instead of includes:

```cpp
// In header file
struct FWidgetComponentInfo;  // Forward declaration

class MyClass
{
    void ProcessWidget(const FWidgetComponentInfo& Info);  // OK - reference
};
```

```cpp
// In implementation file
#include "Services/UMG/Types/WidgetTypes.h"  // Full definition needed here

void MyClass::ProcessWidget(const FWidgetComponentInfo& Info)
{
    // Now we can access Info's members
}
```

### Avoid Circular Dependencies
Type headers should NEVER include service headers. If you find yourself needing to do this, you probably have the wrong architecture.

## Type Header Dependencies

### WidgetStyleTypes.h Dependencies
`WidgetStyleTypes.h` includes some Unreal Engine types needed for the `FVibeWidgetStyle` struct:
- `Math/Color.h` - For `FLinearColor`
- `Layout/Margin.h` - For `FMargin`
- `Fonts/SlateFontInfo.h` - For `FSlateFontInfo`
- `Types/SlateEnums.h` - For alignment enums

This is intentional to keep these dependencies in one place rather than forcing every service that uses `FVibeWidgetStyle` to include them.

## FPropertyInfo Note

Both Blueprint and UMG domains have an `FPropertyInfo` struct. They are similar but serve different purposes:

**Blueprint FPropertyInfo** (`Services/Blueprint/Types/PropertyTypes.h`):
- Used for Blueprint default object properties
- Has object-specific fields like `ObjectClass` and `ObjectValue`

**UMG FPropertyInfo** (`Services/UMG/Types/WidgetPropertyTypes.h`):
- Used for widget component properties
- Has widget-specific fields like `EnumValues` for enum properties

Make sure to include the correct one for your use case. The compiler will catch it if you mix them up since they're in different headers.

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
#include "Services/UMG/WidgetComponentService.h"  // For FWidgetComponentInfo

void MyFunction(const FWidgetComponentInfo& Info) { }
```

**After:**
```cpp
#include "Services/UMG/Types/WidgetTypes.h"  // Only the type

void MyFunction(const FWidgetComponentInfo& Info) { }
```

## Related Documentation

- See `Services/Blueprint/Types/README.md` for Blueprint type definitions
- See service headers for how types are used in context
