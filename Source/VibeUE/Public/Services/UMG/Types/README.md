# UMG Service Types

This directory contains type definitions used by UMG services following the Phase 4 service layer architecture.

## Type Files

### WidgetTypes.h
Core widget type definitions:
- `FWidgetInfo` - Information about a single widget component
- `FWidgetHierarchy` - Complete widget hierarchy
- `FWidgetBlueprintInfo` - Widget Blueprint metadata
- `FWidgetSlotInfo` - Widget slot configuration

### WidgetPropertyTypes.h
Property-related types:
- `FWidgetPropertyInfo` - Property metadata
- `FWidgetPropertyDescriptor` - Extended property information with constraints
- `FWidgetPropertyUpdate` - Property update requests

### WidgetReflectionTypes.h
Reflection and discovery types:
- `FWidgetClassInfo` - Widget class information from reflection
- `FWidgetCompatibilityInfo` - Parent-child compatibility information

## Design Principles

1. **Separation of Concerns**: Types are separated by domain (core widgets, properties, reflection)
2. **Type Safety**: All structures use strongly-typed fields
3. **Documentation**: All types and fields are documented
4. **Consistency**: Follows the same pattern as Blueprint/Types/

## Usage

These types are used by UMG services and should not contain business logic.
Services use `TResult<T>` wrappers around these types for error handling.
