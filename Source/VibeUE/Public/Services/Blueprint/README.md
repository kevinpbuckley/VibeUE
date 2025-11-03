# BlueprintReflectionService

## Overview

The `BlueprintReflectionService` consolidates Blueprint type discovery and metadata extraction into a focused service layer. This service provides the foundation for type-safe Blueprint operations and serves as a building block for higher-level Blueprint manipulation commands.

## Purpose

Extracted from `BlueprintReflection.cpp` (~250 lines), this service provides:
- **Type Discovery**: Enumerate available parent classes, component types, and property types
- **Class Metadata**: Extract properties, functions, and class information
- **Type Validation**: Validate parent classes, component types, and property types
- **Type Conversion**: Resolve class names to UClass* and vice versa

## Architecture

### Current Implementation

The service currently uses `TSharedPtr<FJsonObject>` for return values to maintain compatibility with the existing codebase. This pragmatic approach allows immediate integration while the foundation infrastructure (TResult, FServiceBase, FServiceContext, ErrorCodes) is being developed.

### Future Refactoring

When the foundation work is complete (issues #17-20), the service will be refactored to:
- Return `TResult<T>` instead of JSON
- Use `ErrorCodes` for validation failures
- Inherit from `FServiceBase` with `FServiceContext`

## API Reference

### Type Discovery

#### GetAvailableParentClasses()
Returns list of classes suitable as Blueprint parents.

**Response:**
```json
{
  "success": true,
  "parent_classes": ["Actor", "ActorComponent", "SceneComponent", "Object"],
  "count": 4
}
```

#### GetAvailableComponentTypes()
Returns list of available component types for Blueprint components.

**Response:**
```json
{
  "success": true,
  "component_types": ["StaticMeshComponent", "BoxComponent", "..."],
  "count": 250
}
```

#### GetAvailablePropertyTypes()
Returns list of available property types for Blueprint variables.

**Response:**
```json
{
  "success": true,
  "property_types": ["bool", "int32", "float", "FString", "..."],
  "count": 20
}
```

### Class Metadata

#### GetClassInfo(UClass* Class)
Extract comprehensive class information.

**Response:**
```json
{
  "success": true,
  "class_info": {
    "name": "Actor",
    "path": "/Script/Engine.Actor",
    "display_name": "Actor",
    "parent_class": "Object",
    "parent_path": "/Script/CoreUObject.Object",
    "is_abstract": false,
    "is_blueprint_type": true,
    "is_placeable": true
  }
}
```

#### GetClassProperties(UClass* Class)
Extract all properties from a class.

**Response:**
```json
{
  "success": true,
  "properties": [
    {
      "name": "Location",
      "type": "FVector",
      "category": "Transform",
      "tooltip": "Location of the component",
      "is_editable": true,
      "is_blueprint_visible": true,
      "is_blueprint_readonly": false
    }
  ],
  "count": 10,
  "class_name": "Actor"
}
```

#### GetClassFunctions(UClass* Class)
Extract all functions from a class.

**Response:**
```json
{
  "success": true,
  "functions": [
    {
      "name": "SetActorLocation",
      "category": "Transformation",
      "tooltip": "Set the Actor's world-space position",
      "is_static": false,
      "is_const": false,
      "is_pure": false,
      "is_callable": true,
      "parameters": [
        {
          "name": "NewLocation",
          "type": "FVector",
          "is_return": false,
          "is_out": false
        }
      ]
    }
  ],
  "count": 50,
  "class_name": "Actor"
}
```

### Type Validation

#### IsValidParentClass(const FString& ClassName)
Validate if a class is suitable as a Blueprint parent.

**Response:**
```json
{
  "success": true,
  "is_valid": true,
  "class_name": "Actor"
}
```

#### IsValidComponentType(const FString& ComponentType)
Validate if a type is a valid component.

**Response:**
```json
{
  "success": true,
  "is_valid": true,
  "component_type": "StaticMeshComponent"
}
```

#### IsValidPropertyType(const FString& PropertyType)
Validate if a type is valid for Blueprint properties.

**Response:**
```json
{
  "success": true,
  "is_valid": true,
  "property_type": "int32"
}
```

### Type Conversion

#### ResolveClass(const FString& ClassName)
Resolve a class name or path to a UClass*.

**Response:**
```json
{
  "success": true,
  "class_path": "/Script/Engine.Actor",
  "class_name": "Actor"
}
```

**Error Response:**
```json
{
  "success": false,
  "error_code": "CLASS_NOT_FOUND",
  "error": "Could not resolve class: InvalidClass"
}
```

#### GetClassPath(UClass* Class)
Get the full path of a UClass.

**Response:**
```json
{
  "success": true,
  "class_path": "/Script/Engine.Actor",
  "class_name": "Actor"
}
```

## Error Handling

The service uses standardized error responses with error codes:

| Error Code | Description |
|------------|-------------|
| `INVALID_CLASS` | Null class pointer provided |
| `EMPTY_CLASS_NAME` | Empty class name provided |
| `CLASS_NOT_FOUND` | Could not resolve class from name |

**Error Response Format:**
```json
{
  "success": false,
  "error_code": "INVALID_CLASS",
  "error": "Class is null"
}
```

## Performance

The service implements caching for expensive operations:
- Parent class catalog (populated on first use)
- Component type catalog (uses TObjectIterator)
- Property type catalog (populated on first use)

Caches are initialized lazily and persist for the lifetime of the service instance.

## Testing

### Unit Tests
See `Python/vibe-ue-main/Python/scripts/test_blueprint_reflection_service.py` for API validation tests.

### Integration Tests
To test with actual Unreal Engine:
1. Integrate service with UBridge
2. Expose service methods through command routing
3. Call from Python MCP tools
4. Verify responses match expected format

## Usage Example

```cpp
// Create service instance
FBlueprintReflectionService Service;

// Get available parent classes
TSharedPtr<FJsonObject> ParentClasses = Service.GetAvailableParentClasses();
if (ParentClasses->GetBoolField(TEXT("success")))
{
    TArray<TSharedPtr<FJsonValue>> Classes = ParentClasses->GetArrayField(TEXT("parent_classes"));
    for (const auto& ClassValue : Classes)
    {
        FString ClassName = ClassValue->AsString();
        UE_LOG(LogTemp, Log, TEXT("Parent class: %s"), *ClassName);
    }
}

// Validate a component type
TSharedPtr<FJsonObject> ValidationResult = Service.IsValidComponentType(TEXT("StaticMeshComponent"));
if (ValidationResult->GetBoolField(TEXT("is_valid")))
{
    UE_LOG(LogTemp, Log, TEXT("StaticMeshComponent is a valid component type"));
}

// Resolve a class
TSharedPtr<FJsonObject> ResolveResult = Service.ResolveClass(TEXT("Actor"));
if (ResolveResult->GetBoolField(TEXT("success")))
{
    FString ClassPath = ResolveResult->GetStringField(TEXT("class_path"));
    UClass* ActorClass = LoadObject<UClass>(nullptr, *ClassPath);
    // Use the resolved class...
}
```

## Integration with Bridge

To expose the service through the MCP command system:

1. Add service instance to `UBridge`:
```cpp
// In Bridge.h
TSharedPtr<FBlueprintReflectionService> BlueprintReflectionService;

// In Bridge.cpp constructor
BlueprintReflectionService = MakeShared<FBlueprintReflectionService>();
```

2. Route commands in `ProcessCommand`:
```cpp
if (CommandType == TEXT("get_available_parent_classes"))
{
    ResultJson = BlueprintReflectionService->GetAvailableParentClasses();
}
```

3. Add Python tool wrappers in `tools/blueprint_tools.py`:
```python
def get_available_parent_classes():
    """Get list of available parent classes for Blueprints"""
    return send_command({
        "type": "get_available_parent_classes",
        "params": {}
    })
```

## File Structure

```
Source/VibeUE/
├── Public/Services/Blueprint/
│   └── BlueprintReflectionService.h      (144 lines - Service interface)
└── Private/Services/Blueprint/
    └── BlueprintReflectionService.cpp    (542 lines - Implementation)
```

## Dependencies

- Unreal Engine Core modules (CoreMinimal, Engine)
- Blueprint modules (Blueprint, BlueprintGraph)
- Component classes (ActorComponent, SceneComponent)

## Future Enhancements

1. **Foundation Integration**: Migrate to TResult when available
2. **Expanded Catalogs**: Add more parent classes, property types
3. **Filtering**: Add filter parameters to type discovery methods
4. **Metadata Caching**: Cache class metadata to improve performance
5. **Async Support**: Add async variants for expensive operations

## Related Documentation

- [CPP_REFACTORING_DESIGN.md](../../../docs/CPP_REFACTORING_DESIGN.md) - Overall refactoring plan
- [BlueprintReflection.cpp](../../Commands/BlueprintReflection.cpp) - Original source code
- Issue #[number] - Phase 2, Task 9: Extract BlueprintReflectionService

## License

MIT License - See project LICENSE file for details.
