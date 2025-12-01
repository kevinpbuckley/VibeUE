# manage_level_actors Design Document

## Overview

The `manage_level_actors` tool provides comprehensive **editor-time** control over actors in the current level, enabling AI assistants to add, remove, position, and configure actors entirely through reflection-based operations with zero hardcoded types.

**Important Distinction:**
- **Editor/Level Design (this tool)**: Uses editor APIs (`GEditor->AddActor()`, etc.) with full undo support
- **Runtime Spawning (NOT this tool)**: Uses `SpawnActor()`/`Destroy()` for gameplay - no undo, no editor integration

This tool is for **level design in the Unreal Editor**, not runtime gameplay.

**Branch:** `level-actors`  
**Status:** Design Phase  
**Author:** VibeUE Team  
**Date:** November 30, 2025

---

## Goals

1. **Add actors** of any type (Blueprint or native) to the current level via editor APIs
2. **Remove actors** from the level with undo support
3. **Transform actors** (position, rotation, scale) with absolute or relative operations
4. **Modify properties** on actors using reflection (simple and complex types)
5. **Query actors** in the level with flexible filtering
6. **Zero hardcoding** - all actor types discovered and configured via reflection
7. **Full undo/redo support** - all operations are undoable in the editor

---

## Architecture

### Pattern: Multi-Action Tool

Following the established pattern of `manage_enhanced_input`, `manage_blueprint_node`, etc.:

```
┌─────────────────────────────────────────────────────────────────┐
│                     manage_level_actors                          │
│                    (Python MCP Tool)                             │
├─────────────────────────────────────────────────────────────────┤
│  Actions:                                                        │
│  - add, remove, list, find, select                              │
│  - set_transform, get_transform                                  │
│  - set_property, get_property, list_properties                   │
│  - discover_types, get_actor_info                               │
│  - attach, detach                                                │
└───────────────────────┬─────────────────────────────────────────┘
                        │ TCP Socket (JSON)
                        ▼
┌─────────────────────────────────────────────────────────────────┐
│                  FLevelActorCommands                             │
│                    (C++ Bridge)                                  │
├─────────────────────────────────────────────────────────────────┤
│  HandleCommand() - Routes to appropriate service                 │
└───────────────────────┬─────────────────────────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────────┐
│FLevelActor   │ │FActorQuery   │ │FActorProperty    │
│Service       │ │Service       │ │Service           │
├──────────────┤ ├──────────────┤ ├──────────────────┤
│- AddActor    │ │- FindActors  │ │- GetProperty     │
│- RemoveActor │ │- ListActors  │ │- SetProperty     │
│- SetTransform│ │- GetActorInfo│ │- ListProperties  │
│              │ │- SelectActor │ │- GetPropertyMeta │
└──────────────┘ └──────────────┘ └──────────────────┘
```

---

## Editor vs Runtime APIs

### Editor APIs (What We Use)

```cpp
// Adding actors to level (editor-time)
GEditor->AddActor(World->GetCurrentLevel(), ActorClass, Transform);

// Or using the actor factory system
UActorFactory* Factory = GEditor->FindActorFactoryForActorClass(ActorClass);
AActor* NewActor = GEditor->UseActorFactory(Factory, AssetData, &Transform);

// Removing actors (with undo support)
GEditor->GetEditorSubsystem<ULayersSubsystem>()->DisassociateActorFromLayers(Actor);
GEditor->SelectActor(Actor, false, true);
World->EditorDestroyActor(Actor, true);  // bShouldModifyLevel = true
// OR use transactions for undo:
GEditor->BeginTransaction(TEXT("Delete Actor"));
Actor->Modify();
World->DestroyActor(Actor);
GEditor->EndTransaction();
```

### Runtime APIs (NOT for level design)

```cpp
// These are for gameplay, NOT level design
World->SpawnActor<T>(Class, Transform);  // No undo, runtime only
Actor->Destroy();  // No undo, runtime only
```

---

## C++ Services

### 1. FLevelActorService (Main Service)

**Header:** `Source/VibeUE/Public/Services/LevelActor/LevelActorService.h`  
**Source:** `Source/VibeUE/Private/Services/LevelActor/LevelActorService.cpp`

```cpp
class VIBEUE_API FLevelActorService : public FServiceBase
{
public:
    explicit FLevelActorService(TSharedPtr<FServiceContext> Context);
    
    // Actor Lifecycle (Editor APIs with undo support)
    TResult<AActor*> AddActorToLevel(
        const FString& ActorClassPath,      // Blueprint path or native class
        const FTransform& Transform,
        const FString& ActorLabel,          // Optional display label
        const TMap<FString, FString>& InitialProperties  // Properties to set after creation
    );
    
    TResult<int32> RemoveActorsFromLevel(
        const FActorQueryCriteria& Criteria,
        bool bWithUndo = true               // Support undo/redo
    );
    
    // Transform Operations
    TResult<void> SetActorTransform(
        const FActorIdentifier& Actor,
        const FTransform& NewTransform,
        bool bSweep = false,
        bool bTeleport = true
    );
    
    TResult<FTransform> GetActorTransform(const FActorIdentifier& Actor);
    
    TResult<void> SetActorLocation(
        const FActorIdentifier& Actor,
        const FVector& NewLocation,
        bool bSweep = false
    );
    
    TResult<void> SetActorRotation(
        const FActorIdentifier& Actor,
        const FRotator& NewRotation
    );
    
    TResult<void> SetActorScale(
        const FActorIdentifier& Actor,
        const FVector& NewScale
    );
    
    // Property Operations (Reflection-based)
    TResult<FString> GetActorProperty(
        const FActorIdentifier& Actor,
        const FString& PropertyPath  // Supports dot notation: "RootComponent.RelativeLocation.X"
    );
    
    TResult<void> SetActorProperty(
        const FActorIdentifier& Actor,
        const FString& PropertyPath,
        const FString& PropertyValue,  // JSON-encoded for complex types
        const FString& PropertyType = TEXT("")  // Optional type hint
    );
    
    TResult<TArray<FPropertyInfo>> ListActorProperties(
        const FActorIdentifier& Actor,
        bool bIncludeComponents = true,
        bool bIncludeInherited = true,
        const FString& CategoryFilter = TEXT("")
    );
    
    // Query Operations
    TResult<TArray<FActorInfo>> FindActors(const FActorQueryCriteria& Criteria);
    
    TResult<FActorInfo> GetActorInfo(const FActorIdentifier& Actor);
    
    // Hierarchy Operations
    TResult<void> AttachActor(
        const FActorIdentifier& Child,
        const FActorIdentifier& Parent,
        const FName& SocketName = NAME_None
    );
    
    TResult<void> DetachActor(const FActorIdentifier& Actor);
    
    // Type Discovery
    TResult<TArray<FActorTypeInfo>> DiscoverSpawnableActorTypes(
        const FActorTypeSearchCriteria& Criteria
    );
    
protected:
    virtual FString GetServiceName() const override { return TEXT("LevelActorService"); }
    
private:
    // Helper to resolve actor from identifier
    TResult<AActor*> ResolveActor(const FActorIdentifier& Identifier);
    
    // Helper to get world
    UWorld* GetEditorWorld() const;
};
```

### 2. Supporting Structures

```cpp
// Actor identification - multiple ways to reference an actor
struct FActorIdentifier
{
    FString ActorPath;      // Full path: /Game/Maps/Level.Level:PersistentLevel.BP_Player_1
    FString ActorLabel;     // Display name in editor
    FString ActorGuid;      // Unique GUID
    FName ActorTag;         // First matching tag
    
    bool IsValid() const;
    static FActorIdentifier FromPath(const FString& Path);
    static FActorIdentifier FromLabel(const FString& Label);
    static FActorIdentifier FromGuid(const FString& Guid);
};

// Query criteria for finding actors
struct FActorQueryCriteria
{
    FString ClassFilter;        // Filter by class path (supports wildcards)
    FString LabelFilter;        // Filter by label (supports wildcards)
    TArray<FName> RequiredTags; // Must have all these tags
    TArray<FName> ExcludeTags;  // Must not have these tags
    FBox BoundingBox;           // Spatial filter
    bool bSelectedOnly;         // Only selected actors
    bool bHiddenActors;         // Include hidden actors
    int32 MaxResults;           // Limit results
};

// Actor type information for adding to level
struct FActorTypeInfo
{
    FString ClassPath;          // /Script/Engine.StaticMeshActor or Blueprint path
    FString DisplayName;
    FString Category;
    FString Description;
    bool bIsBlueprint;
    bool bIsPlaceable;
    TArray<FPropertyInfo> DefaultProperties;  // Editable properties
};

// Returned actor information (comprehensive)
struct FActorInfo
{
    // Identity
    FString ActorPath;
    FString ActorLabel;
    FString ActorGuid;
    FString ClassName;
    FString ClassPath;
    
    // Transform
    FTransform Transform;
    
    // Metadata
    TArray<FName> Tags;
    FString FolderPath;         // Editor folder organization
    bool bIsSelected;
    bool bIsHidden;
    bool bIsTemporarilyHidden;
    bool bIsEditable;
    
    // Hierarchy
    FString ParentActorPath;    // If attached to another actor
    FString AttachSocketName;
    TArray<FString> ChildActorPaths;
    
    // Components (names and types)
    TArray<FComponentInfo> Components;
    
    // All properties with current values (reflection-based)
    TArray<FActorPropertyInfo> Properties;  // Full property dump
};

// Property info with current value
struct FActorPropertyInfo
{
    FString Name;
    FString DisplayName;
    FString Category;
    FString TypeName;
    FString CurrentValue;       // JSON-encoded for complex types
    bool bIsEditable;
    bool bIsAdvanced;
    FString Tooltip;
};

// Component info 
struct FComponentInfo
{
    FString Name;
    FString ClassName;
    FString ParentName;         // Component hierarchy
    bool bIsRootComponent;
    TArray<FActorPropertyInfo> Properties;  // Component properties
};
```

### 3. FLevelActorCommands (Bridge Command Handler)

**Header:** `Source/VibeUE/Public/Commands/LevelActorCommands.h`  
**Source:** `Source/VibeUE/Private/Commands/LevelActorCommands.cpp`

```cpp
class VIBEUE_API FLevelActorCommands
{
public:
    FLevelActorCommands();
    
    TSharedPtr<FJsonObject> HandleCommand(
        const FString& CommandType,
        const TSharedPtr<FJsonObject>& Params
    );
    
private:
    TSharedPtr<FLevelActorService> LevelActorService;
    TSharedPtr<FServiceContext> ServiceContext;
    
    // Command handlers
    TSharedPtr<FJsonObject> HandleAddActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetTransform(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetTransform(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindActors(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDiscoverActorTypes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAttachActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDetachActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSelectActor(const TSharedPtr<FJsonObject>& Params);
};
```

---

## Python Tool

**File:** `Content/Python/tools/manage_level_actors.py`

### Tool Registration

```python
def register_level_actor_tools(mcp: FastMCP) -> None:
    """Register unified Level Actor management tool with MCP server."""
    
    @mcp.tool()
    def manage_level_actors(
        ctx: Context,
        action: str,
        # Actor identification
        actor_path: str = "",
        actor_label: str = "",
        actor_guid: str = "",
        actor_tag: str = "",
        # Spawn parameters
        actor_class: str = "",
        spawn_location: Optional[List[float]] = None,  # [X, Y, Z]
        spawn_rotation: Optional[List[float]] = None,  # [Pitch, Yaw, Roll]
        spawn_scale: Optional[List[float]] = None,     # [X, Y, Z]
        actor_name: str = "",
        # Transform operations
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        relative: bool = False,
        sweep: bool = False,
        teleport: bool = True,
        # Property operations
        property_name: str = "",
        property_value: Optional[Any] = None,
        property_type: str = "",
        include_components: bool = True,
        include_inherited: bool = True,
        category_filter: str = "",
        # Query parameters
        class_filter: str = "",
        label_filter: str = "",
        required_tags: Optional[List[str]] = None,
        exclude_tags: Optional[List[str]] = None,
        selected_only: bool = False,
        max_results: int = 100,
        # Hierarchy
        parent_actor: str = "",
        socket_name: str = "",
        # Type discovery
        search_text: str = "",
        include_blueprints: bool = True,
        include_native: bool = True,
        placeable_only: bool = True,
        # Removal options
        with_undo: bool = True,          # Support undo/redo for removal
    ) -> Dict[str, Any]:
```

### Available Actions (18 actions)

| Action | Purpose |
|--------|--------|
| `add` | Add actor of any type to the level (editor API with undo) |
| `remove` | Remove actor(s) from level (editor API with undo) |
| `list` | List all actors in level with filtering |
| `find` | Find actors matching query criteria |
| `get_info` | Get comprehensive actor info including ALL properties and values via reflection |
| `select` | Select actor(s) in the editor |
| `set_transform` | Set actor world transform |
| `get_transform` | Get actor world transform |
| `set_location` | Set actor world location |
| `set_rotation` | Set actor world rotation |
| `set_scale` | Set actor world scale |
| `set_property` | Set any actor property via reflection |
| `get_property` | Get any actor property via reflection |
| `list_properties` | Discover available properties on actor (with type, category, current value) |
| `attach` | Attach actor to parent |
| `detach` | Detach actor from parent |
| `discover_types` | Discover spawnable actor types |
| `get_type_info` | Get detailed info about actor type |

---

## Reflection-Based Property System

### Property Path Notation

Support dot notation for nested properties:

```python
# Simple property
set_property(actor_path="/Game/Level.Level:BP_Light_1", property_name="Intensity", property_value=5000)

# Component property
set_property(actor_path="...", property_name="RootComponent.RelativeLocation", property_value=[100, 200, 50])

# Deeply nested
set_property(actor_path="...", property_name="StaticMeshComponent.StaticMesh.Materials[0].BaseColor", property_value=[1, 0, 0, 1])
```

### Complex Type Handling

Following the pattern from `manage_blueprint_variable` and `manage_enhanced_input`:

```python
# Struct (FVector, FRotator, FLinearColor, etc.)
set_property(property_name="LightColor", property_value={"R": 1.0, "G": 0.5, "B": 0.2, "A": 1.0})

# Array
set_property(property_name="Tags", property_value=["Enemy", "Hostile", "Boss"])

# Object reference
set_property(property_name="StaticMesh", property_value="/Game/Meshes/SM_Cube.SM_Cube")

# Enum
set_property(property_name="Mobility", property_value="Movable")  # or numeric: 2
```

---

## Usage Examples

### Add Actor to Level

```python
# Add Blueprint actor to level
manage_level_actors(
    action="add",
    actor_class="/Game/Blueprints/BP_Enemy.BP_Enemy_C",
    location=[1000, 500, 100],
    rotation=[0, 90, 0],
    actor_label="Enemy_Boss_01"
)

# Add native actor with properties
manage_level_actors(
    action="add",
    actor_class="PointLight",
    location=[0, 0, 300],
    actor_label="MainLight"
)
# Then configure:
manage_level_actors(
    action="set_property",
    actor_label="MainLight",
    property_name="PointLightComponent.Intensity",
    property_value=10000
)
```

### Remove Actor from Level

```python
# Remove by label
manage_level_actors(
    action="remove",
    actor_label="Enemy_Boss_01"
)

# Remove all actors matching criteria
manage_level_actors(
    action="remove",
    class_filter="*BP_Enemy*",
    required_tags=["Hostile"]
)
# Note: All removals support undo (Ctrl+Z)
```

### Query and Modify Actors

```python
# Find all enemies
result = manage_level_actors(
    action="find",
    class_filter="*BP_Enemy*",
    required_tags=["Hostile"]
)

# Move an actor
manage_level_actors(
    action="set_location",
    actor_label="PlayerStart",
    location=[0, 0, 100]
)
```

### Property Discovery and Modification Workflow

Following the same pattern as UMG widget properties (`manage_umg_widget`):

```python
# Step 1: Discover what properties exist on an actor
props = manage_level_actors(
    action="list_properties",
    actor_label="PointLight_1",
    include_components=True,      # Include component properties
    include_inherited=True,       # Include inherited properties
    category_filter="Light"       # Optional: filter by category
)
# Returns:
# {
#     "success": true,
#     "properties": [
#         {
#             "name": "Intensity",
#             "type": "float",
#             "category": "Light",
#             "current_value": "5000.0",
#             "is_editable": true,
#             "path": "PointLightComponent.Intensity"  # Full path for set_property
#         },
#         {
#             "name": "LightColor",
#             "type": "FLinearColor",
#             "category": "Light",
#             "current_value": "(R=1.0,G=1.0,B=1.0,A=1.0)",
#             "is_editable": true,
#             "path": "PointLightComponent.LightColor",
#             "nested_properties": ["R", "G", "B", "A"]  # For struct types
#         },
#         {
#             "name": "AttenuationRadius",
#             "type": "float",
#             "category": "Light",
#             "current_value": "1000.0",
#             "is_editable": true,
#             "path": "PointLightComponent.AttenuationRadius"
#         }
#     ],
#     "categories": ["Light", "Rendering", "Transform", "Collision"]
# }

# Step 2: Get a specific property value
value = manage_level_actors(
    action="get_property",
    actor_label="PointLight_1",
    property_name="PointLightComponent.Intensity"
)
# Returns: {"success": true, "value": 5000.0, "type": "float"}

# Step 3: Set the property using the discovered path
manage_level_actors(
    action="set_property",
    actor_label="PointLight_1",
    property_name="PointLightComponent.Intensity",
    property_value=10000.0
)

# Step 4: Set a struct property (color)
manage_level_actors(
    action="set_property",
    actor_label="PointLight_1",
    property_name="PointLightComponent.LightColor",
    property_value={"R": 1.0, "G": 0.5, "B": 0.2, "A": 1.0}
)

# Alternative: Set individual struct members
manage_level_actors(
    action="set_property",
    actor_label="PointLight_1",
    property_name="PointLightComponent.LightColor.R",
    property_value=0.8
)
```

### Property Discovery for Actor Classes (Before Adding)

```python
# Discover properties on an actor CLASS before adding to level
type_info = manage_level_actors(
    action="get_type_info",
    actor_class="PointLight",
    include_properties=True
)
# Returns all properties that will be available on instances of this class

# Now add with specific property values
manage_level_actors(
    action="add",
    actor_class="PointLight",
    spawn_location=[100, 200, 300],
    actor_name="MainLight",
    properties={
        "PointLightComponent.Intensity": 15000,
        "PointLightComponent.LightColor": {"R": 1.0, "G": 0.9, "B": 0.8, "A": 1.0},
        "PointLightComponent.AttenuationRadius": 2000
    }
)
```

### Get Comprehensive Actor Info

```python
# Get ALL information about an actor including all properties
result = manage_level_actors(
    action="get_info",
    actor_label="PointLight_1",
    include_components=True,   # Include full component breakdown
    include_inherited=True     # Include inherited properties
)

# Returns complete FActorInfo with ALL properties and values:
# {
#     "success": true,
#     "actor": {
#         "actor_path": "/Game/Maps/Lvl_Main.Lvl_Main:PersistentLevel.PointLight_1",
#         "actor_label": "PointLight_1",
#         "actor_guid": "A1B2C3D4E5F6...",
#         "class_name": "PointLight",
#         "transform": {
#             "location": [100.0, 200.0, 300.0],
#             "rotation": [0.0, 45.0, 0.0],
#             "scale": [1.0, 1.0, 1.0]
#         },
#         "tags": ["MainLighting"],
#         "is_selected": false,
#         "is_hidden": false,
#         "properties": {
#             "RootComponent": {
#                 "name": "RootComponent",
#                 "type": "SceneComponent",
#                 "value": "PointLightComponent0",
#                 "category": "Transform",
#                 "is_editable": true
#             },
#             "bHidden": {
#                 "name": "bHidden",
#                 "type": "bool",
#                 "value": "false",
#                 "category": "Rendering"
#             }
#             // ... ALL other actor properties
#         },
#         "components": [
#             {
#                 "name": "PointLightComponent0",
#                 "class_name": "PointLightComponent",
#                 "parent": "RootComponent",
#                 "properties": {
#                     "Intensity": {
#                         "name": "Intensity",
#                         "type": "float",
#                         "value": "5000.0",
#                         "category": "Light"
#                     },
#                     "LightColor": {
#                         "name": "LightColor",
#                         "type": "FColor",
#                         "value": "(R=255,G=255,B=255,A=255)",
#                         "category": "Light",
#                         "nested_properties": [
#                             {"name": "R", "type": "uint8", "value": "255"},
#                             {"name": "G", "type": "uint8", "value": "255"},
#                             {"name": "B", "type": "uint8", "value": "255"},
#                             {"name": "A", "type": "uint8", "value": "255"}
#                         ]
#                     },
#                     "AttenuationRadius": {
#                         "name": "AttenuationRadius",
#                         "type": "float",
#                         "value": "1000.0",
#                         "category": "Light"
#                     },
#                     "CastShadows": {
#                         "name": "CastShadows",
#                         "type": "bool",
#                         "value": "true",
#                         "category": "Light"
#                     }
#                     // ... ALL component properties
#                 }
#             }
#         ]
#     }
# }
```

### Discover Available Types

```python
# Find all spawnable light actors
manage_level_actors(
    action="discover_types",
    search_text="Light",
    placeable_only=True
)
# Returns: PointLight, SpotLight, DirectionalLight, RectLight, SkyLight, etc.

# Find all Blueprint actors in a folder
manage_level_actors(
    action="discover_types",
    search_text="/Game/Blueprints/Enemies",
    include_blueprints=True,
    include_native=False
)
```

---

## Implementation Phases

### Phase 1: Core Actor Operations
- [ ] `FLevelActorService` base implementation
- [ ] `FLevelActorCommands` command router
- [ ] `manage_level_actors.py` Python tool
- [ ] Actions: `add`, `remove`, `list`, `find`, `get_info`
- [ ] Basic transform operations: `set_transform`, `get_transform`

### Phase 2: Property System
- [ ] Reflection-based property getter/setter
- [ ] Property path notation support
- [ ] Complex type serialization/deserialization
- [ ] Actions: `set_property`, `get_property`, `list_properties`

### Phase 3: Advanced Features
- [ ] Actor type discovery
- [ ] Hierarchy operations: `attach`, `detach`
- [ ] Editor selection: `select`
- [ ] Spatial queries (bounding box filter)
- [ ] Actions: `discover_types`, `get_type_info`, `attach`, `detach`, `select`

### Phase 4: Documentation & Testing
- [ ] Help topic: `level-actors`
- [ ] Smoke test additions
- [ ] README update with new tool

---

## Bridge Integration

Add to `Bridge.cpp` `RouteCommand()`:

```cpp
// Level Actor Commands
else if (CommandType == TEXT("manage_level_actors"))
{
    UE_LOG(LogTemp, Display, TEXT("MCP: Dispatching to LevelActorCommands: %s"), *CommandType);
    ResultJson = LevelActorCommands->HandleCommand(CommandType, Params);
}
```

---

## Key Design Decisions

1. **Editor APIs, Not Runtime**: Use `GEditor->AddActor()` and `EditorDestroyActor()` instead of `SpawnActor()`/`Destroy()` - this gives us proper undo support and editor integration

2. **Actor Identification**: Support multiple ways to identify actors (path, label, GUID, tag) for flexibility

3. **Reflection-First**: All property operations use UE reflection, no hardcoded property names

4. **Property Path Notation**: Dot notation allows accessing nested properties and component properties

5. **JSON Serialization**: Complex types (structs, arrays, objects) serialized as JSON for transport

6. **Query System**: Flexible filtering by class, tags, labels, spatial bounds, selection state

7. **Type Discovery**: Discover placeable types at runtime, including Blueprint classes

8. **Undo Support**: All add/remove operations support Ctrl+Z undo in the editor

9. **Error Handling**: Use TResult pattern consistent with other services

---

## File Structure

```
Plugins/VibeUE/
├── Source/VibeUE/
│   ├── Public/
│   │   ├── Services/
│   │   │   └── LevelActor/
│   │   │       └── LevelActorService.h
│   │   └── Commands/
│   │       └── LevelActorCommands.h
│   └── Private/
│       ├── Services/
│       │   └── LevelActor/
│       │       └── LevelActorService.cpp
│       └── Commands/
│           └── LevelActorCommands.cpp
├── Content/Python/
│   ├── tools/
│   │   └── manage_level_actors.py
│   └── resources/
│       └── topics/
│           └── level-actors.md
```

---

## Success Criteria

1. ✅ Add any actor type (Blueprint or native) to level with initial transform
2. ✅ Remove actors with full undo/redo support
3. ✅ Query actors with flexible filtering
4. ✅ Set/get any property via reflection (no hardcoding)
5. ✅ Support complex property types (structs, arrays, objects)
6. ✅ Property path notation for nested access
7. ✅ Discover available placeable actor types at runtime
8. ✅ All operations work in editor context with undo support
9. ✅ Consistent error handling with other tools
10. ✅ Passes smoke tests
