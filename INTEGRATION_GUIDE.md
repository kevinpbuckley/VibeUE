# BlueprintReflectionService - Integration Guide

## Quick Integration Steps

### Step 1: Add Service to Bridge

**File**: `Source/VibeUE/Public/Bridge.h`

Add include:
```cpp
#include "Services/Blueprint/BlueprintReflectionService.h"
```

Add member variable (around line 50, with other command handlers):
```cpp
TSharedPtr<FBlueprintReflectionService> BlueprintReflectionService;
```

### Step 2: Initialize Service

**File**: `Source/VibeUE/Private/Bridge.cpp`

In the constructor (around line 73):
```cpp
UBridge::UBridge()
{
    BlueprintCommands = MakeShared<FBlueprintCommands>();
    BlueprintNodeCommands = MakeShared<FBlueprintNodeCommands>();
    BlueprintComponentReflection = MakeShared<FBlueprintComponentReflection>();
    BlueprintReflectionService = MakeShared<FBlueprintReflectionService>(); // ADD THIS LINE
    UMGCommands = MakeShared<FUMGCommands>();
    UMGReflectionCommands = MakeShared<FUMGReflectionCommands>();
    AssetCommands = MakeShared<FAssetCommands>();
}
```

In the destructor (around line 83):
```cpp
UBridge::~UBridge()
{
    BlueprintCommands.Reset();
    BlueprintNodeCommands.Reset();
    BlueprintComponentReflection.Reset();
    BlueprintReflectionService.Reset(); // ADD THIS LINE
    UMGCommands.Reset();
    UMGReflectionCommands.Reset();
    AssetCommands.Reset();
}
```

### Step 3: Route Commands

**File**: `Source/VibeUE/Private/Bridge.cpp`

In the command processing section (around line 250-325), add routing for service commands:

```cpp
// Add this section after existing command routing blocks
else if (CommandType == TEXT("get_available_parent_classes"))
{
    ResultJson = BlueprintReflectionService->GetAvailableParentClasses();
}
else if (CommandType == TEXT("get_available_component_types"))
{
    ResultJson = BlueprintReflectionService->GetAvailableComponentTypes();
}
else if (CommandType == TEXT("get_available_property_types"))
{
    ResultJson = BlueprintReflectionService->GetAvailablePropertyTypes();
}
else if (CommandType == TEXT("get_class_info"))
{
    // Extract class from params and call service
    FString ClassName;
    if (Params->TryGetStringField(TEXT("class_name"), ClassName))
    {
        TSharedPtr<FJsonObject> ResolveResult = BlueprintReflectionService->ResolveClass(ClassName);
        if (ResolveResult->GetBoolField(TEXT("success")))
        {
            FString ClassPath = ResolveResult->GetStringField(TEXT("class_path"));
            UClass* Class = LoadObject<UClass>(nullptr, *ClassPath);
            if (Class)
            {
                ResultJson = BlueprintReflectionService->GetClassInfo(Class);
            }
            else
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), TEXT("Failed to load class"));
            }
        }
        else
        {
            ResultJson = ResolveResult; // Return the resolution error
        }
    }
}
else if (CommandType == TEXT("get_class_properties"))
{
    // Similar to get_class_info
    FString ClassName;
    if (Params->TryGetStringField(TEXT("class_name"), ClassName))
    {
        TSharedPtr<FJsonObject> ResolveResult = BlueprintReflectionService->ResolveClass(ClassName);
        if (ResolveResult->GetBoolField(TEXT("success")))
        {
            FString ClassPath = ResolveResult->GetStringField(TEXT("class_path"));
            UClass* Class = LoadObject<UClass>(nullptr, *ClassPath);
            if (Class)
            {
                ResultJson = BlueprintReflectionService->GetClassProperties(Class);
            }
        }
        else
        {
            ResultJson = ResolveResult;
        }
    }
}
else if (CommandType == TEXT("get_class_functions"))
{
    // Similar to get_class_info
    FString ClassName;
    if (Params->TryGetStringField(TEXT("class_name"), ClassName))
    {
        TSharedPtr<FJsonObject> ResolveResult = BlueprintReflectionService->ResolveClass(ClassName);
        if (ResolveResult->GetBoolField(TEXT("success")))
        {
            FString ClassPath = ResolveResult->GetStringField(TEXT("class_path"));
            UClass* Class = LoadObject<UClass>(nullptr, *ClassPath);
            if (Class)
            {
                ResultJson = BlueprintReflectionService->GetClassFunctions(Class);
            }
        }
        else
        {
            ResultJson = ResolveResult;
        }
    }
}
else if (CommandType == TEXT("is_valid_parent_class"))
{
    FString ClassName;
    if (Params->TryGetStringField(TEXT("class_name"), ClassName))
    {
        ResultJson = BlueprintReflectionService->IsValidParentClass(ClassName);
    }
}
else if (CommandType == TEXT("is_valid_component_type"))
{
    FString ComponentType;
    if (Params->TryGetStringField(TEXT("component_type"), ComponentType))
    {
        ResultJson = BlueprintReflectionService->IsValidComponentType(ComponentType);
    }
}
else if (CommandType == TEXT("is_valid_property_type"))
{
    FString PropertyType;
    if (Params->TryGetStringField(TEXT("property_type"), PropertyType))
    {
        ResultJson = BlueprintReflectionService->IsValidPropertyType(PropertyType);
    }
}
else if (CommandType == TEXT("resolve_class"))
{
    FString ClassName;
    if (Params->TryGetStringField(TEXT("class_name"), ClassName))
    {
        ResultJson = BlueprintReflectionService->ResolveClass(ClassName);
    }
}
else if (CommandType == TEXT("get_class_path"))
{
    FString ClassName;
    if (Params->TryGetStringField(TEXT("class_name"), ClassName))
    {
        TSharedPtr<FJsonObject> ResolveResult = BlueprintReflectionService->ResolveClass(ClassName);
        if (ResolveResult->GetBoolField(TEXT("success")))
        {
            FString ClassPath = ResolveResult->GetStringField(TEXT("class_path"));
            UClass* Class = LoadObject<UClass>(nullptr, *ClassPath);
            if (Class)
            {
                ResultJson = BlueprintReflectionService->GetClassPath(Class);
            }
        }
        else
        {
            ResultJson = ResolveResult;
        }
    }
}
```

### Step 4: Add Python Tool Wrappers

**File**: `Python/vibe-ue-main/Python/tools/blueprint_tools.py`

Add these functions:

```python
from mcp import send_command

def get_available_parent_classes():
    """Get list of available parent classes for Blueprints"""
    return send_command({
        "type": "get_available_parent_classes",
        "params": {}
    })

def get_available_component_types():
    """Get list of available component types"""
    return send_command({
        "type": "get_available_component_types",
        "params": {}
    })

def get_available_property_types():
    """Get list of available property types"""
    return send_command({
        "type": "get_available_property_types",
        "params": {}
    })

def is_valid_parent_class(class_name: str):
    """Validate if a class is a valid Blueprint parent"""
    return send_command({
        "type": "is_valid_parent_class",
        "params": {"class_name": class_name}
    })

def is_valid_component_type(component_type: str):
    """Validate if a type is a valid component"""
    return send_command({
        "type": "is_valid_component_type",
        "params": {"component_type": component_type}
    })

def is_valid_property_type(property_type: str):
    """Validate if a type is valid for Blueprint properties"""
    return send_command({
        "type": "is_valid_property_type",
        "params": {"property_type": property_type}
    })

def resolve_class(class_name: str):
    """Resolve a class name to its full path"""
    return send_command({
        "type": "resolve_class",
        "params": {"class_name": class_name}
    })

def get_class_info(class_name: str):
    """Get comprehensive class information"""
    return send_command({
        "type": "get_class_info",
        "params": {"class_name": class_name}
    })

def get_class_properties(class_name: str):
    """Get all properties of a class"""
    return send_command({
        "type": "get_class_properties",
        "params": {"class_name": class_name}
    })

def get_class_functions(class_name: str):
    """Get all functions of a class"""
    return send_command({
        "type": "get_class_functions",
        "params": {"class_name": class_name}
    })
```

### Step 5: Build and Test

1. **Rebuild the Unreal Engine plugin**:
   - Right-click your `.uproject` file
   - Select "Generate Visual Studio project files"
   - Build in Visual Studio (Development Editor configuration)

2. **Test basic functionality**:
   ```python
   # In Python console or test script
   from tools.blueprint_tools import *
   
   # Test type discovery
   result = get_available_parent_classes()
   print(f"Parent classes: {result}")
   
   # Test validation
   result = is_valid_parent_class("Actor")
   print(f"Is Actor valid: {result}")
   
   # Test class info
   result = get_class_info("Actor")
   print(f"Actor info: {result}")
   ```

3. **Verify responses**:
   - Check that `success: true` is returned
   - Verify data structures match documentation
   - Test error cases (invalid class names)

## Example Usage

### From Python

```python
# Discover available parent classes
parent_classes = get_available_parent_classes()
print(f"Available parent classes: {parent_classes['parent_classes']}")

# Validate a component type
result = is_valid_component_type("StaticMeshComponent")
if result['is_valid']:
    print("StaticMeshComponent is valid!")

# Get detailed class information
class_info = get_class_info("Actor")
print(f"Actor inherits from: {class_info['class_info']['parent_class']}")

# Get all properties of a class
properties = get_class_properties("Actor")
print(f"Actor has {properties['count']} properties")
for prop in properties['properties']:
    print(f"  - {prop['name']}: {prop['type']}")
```

### From C++

```cpp
// Get the service instance
TSharedPtr<FBlueprintReflectionService> Service = 
    GetWorld()->GetSubsystem<UBridge>()->BlueprintReflectionService;

// Discover parent classes
TSharedPtr<FJsonObject> ParentClasses = Service->GetAvailableParentClasses();

// Validate a component type
TSharedPtr<FJsonObject> ValidationResult = 
    Service->IsValidComponentType(TEXT("StaticMeshComponent"));
    
if (ValidationResult->GetBoolField(TEXT("is_valid")))
{
    UE_LOG(LogTemp, Log, TEXT("Component type is valid"));
}

// Get class metadata
UClass* ActorClass = AActor::StaticClass();
TSharedPtr<FJsonObject> ClassInfo = Service->GetClassInfo(ActorClass);
```

## Testing Checklist

After integration:

- [ ] Service initializes correctly in Bridge
- [ ] `get_available_parent_classes` returns expected classes
- [ ] `get_available_component_types` returns component types
- [ ] `get_available_property_types` returns property types
- [ ] `is_valid_parent_class` correctly validates Actor
- [ ] `is_valid_component_type` correctly validates StaticMeshComponent
- [ ] `is_valid_property_type` correctly validates int32
- [ ] `resolve_class` resolves "Actor" to correct path
- [ ] `get_class_info` returns comprehensive info for Actor
- [ ] `get_class_properties` returns properties for Actor
- [ ] `get_class_functions` returns functions for Actor
- [ ] Error cases return proper error responses
- [ ] Python tools work correctly
- [ ] No crashes or memory leaks

## Troubleshooting

### Service not initializing
- Check that include path is correct in Bridge.h
- Verify BlueprintReflectionService.h is in Public/Services/Blueprint/
- Rebuild project to update includes

### Commands not routing
- Verify command type strings match exactly
- Check that params are being passed correctly
- Add logging to see which command path is taken

### Empty results
- Check that type catalogs are being initialized
- Verify UObjectIterator is finding classes
- Add logging to PopulateXXXCatalog methods

### Python tools not working
- Verify MCP server is running
- Check that command format matches Bridge expectations
- Test direct command sending first

## Performance Considerations

1. **Caching**: Type catalogs are cached on first use
2. **Lazy Initialization**: Catalogs only populated when needed
3. **TObjectIterator**: Component type discovery may be slow on first call
4. **Class Loading**: ResolveClass may trigger asset loading

## Future Enhancements

When foundation infrastructure is available:

1. Migrate to TResult<T> return types
2. Use ErrorCodes enum for error handling
3. Inherit from FServiceBase
4. Add service context for dependency injection
5. Add async variants for expensive operations
6. Implement metadata caching
7. Add filtering parameters to discovery methods

## Support

For questions or issues:
- See `IMPLEMENTATION_SUMMARY.md` for implementation details
- See `Public/Services/Blueprint/README.md` for API reference
- Check existing command handlers for patterns
- Review test file for usage examples

---

**Integration Status**: 🔧 Ready for integration - Follow steps above to expose service through Bridge
