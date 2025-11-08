# BlueprintDiscoveryService

## Overview

The `FBlueprintDiscoveryService` is a focused service responsible for discovering and loading Blueprint assets in Unreal Engine. This service is part of the Phase 2 refactoring effort to extract the Blueprint domain from monolithic command classes into focused, single-responsibility services.

## Purpose

This service was extracted from `BlueprintCommands.cpp` to provide:
- **Focused responsibility**: Only handles blueprint discovery and loading
- **Type-safe error handling**: Uses `TResult<T>` instead of JSON parsing
- **Reusability**: Can be used by multiple command handlers
- **Testability**: Easier to unit test than monolithic command classes

## Features

### Core Operations

1. **FindBlueprint** - Find a blueprint by name or path with multiple fallback strategies
2. **LoadBlueprint** - Load a blueprint from a specific asset path
3. **SearchBlueprints** - Search for blueprints matching a search term
4. **ListAllBlueprints** - List all blueprints under a given path
5. **GetBlueprintInfo** - Extract metadata from a blueprint
6. **BlueprintExists** - Check if a blueprint exists without loading full data

### Search Strategies

The `FindBlueprint` method uses multiple strategies to locate blueprints:

1. **Direct Path Loading** - If the input starts with "/", treats it as a full asset path
2. **Default Path Convention** - Checks `/Game/Blueprints/{Name}` if no path is provided
3. **Recursive Asset Registry Search** - Performs a case-insensitive search across all blueprints

## Usage Example

See `docs/examples/BlueprintServiceIntegrationExample.h` for detailed usage patterns.

### Basic Usage

```cpp
// Create service instance
TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
TSharedPtr<FBlueprintDiscoveryService> Service = MakeShared<FBlueprintDiscoveryService>(Context);

// Find a blueprint
TResult<UBlueprint*> Result = Service->FindBlueprint("MyBlueprint");
if (Result.IsSuccess())
{
    UBlueprint* Blueprint = Result.GetValue();
    // Use the blueprint...
}
else
{
    UE_LOG(LogTemp, Error, TEXT("Error: %s - %s"), 
        *Result.GetErrorCode(), 
        *Result.GetErrorMessage());
}
```

### Search Blueprints

```cpp
TResult<TArray<FBlueprintInfo>> Result = Service->SearchBlueprints("Player", 50);
if (Result.IsSuccess())
{
    for (const FBlueprintInfo& Info : Result.GetValue())
    {
        UE_LOG(LogTemp, Log, TEXT("Found: %s at %s"), *Info.Name, *Info.Path);
    }
}
```

## Error Handling

All methods return `TResult<T>` which provides type-safe error handling:

```cpp
TResult<UBlueprint*> Result = Service->FindBlueprint("NonExistent");
if (Result.IsError())
{
    // Error codes from ErrorCodes namespace
    FString ErrorCode = Result.GetErrorCode();  // e.g., "BLUEPRINT_NOT_FOUND"
    FString ErrorMsg = Result.GetErrorMessage(); // Human-readable message
}
```

### Standard Error Codes

- `PARAM_INVALID` - Invalid or missing parameters
- `BLUEPRINT_NOT_FOUND` - Blueprint could not be found
- `BLUEPRINT_LOAD_FAILED` - Blueprint failed to load

## Implementation Details

### File Structure

```
Source/VibeUE/
├── Public/
│   ├── Core/
│   │   ├── TResult.h         - Result type for type-safe error handling
│   │   ├── ErrorCodes.h      - Standard error codes
│   │   ├── ServiceBase.h     - Base class for all services
│   │   └── ServiceContext.h  - Service context for shared state
│   └── Services/
│       └── Blueprint/
│           └── BlueprintDiscoveryService.h
└── Private/
    └── Services/
        └── Blueprint/
            └── BlueprintDiscoveryService.cpp
```

### Dependencies

- **Phase 1 Foundation Classes**:
  - `TResult<T>` - Type-safe result wrapper
  - `FServiceBase` - Base service class
  - `FServiceContext` - Service context
  - `ErrorCodes` - Standard error codes

- **Unreal Engine Modules**:
  - `AssetRegistry` - For blueprint discovery
  - `EditorAssetLibrary` - For asset loading
  - `Engine` - For blueprint types

### Performance Characteristics

- **FindBlueprint**: O(1) for direct paths, O(n) for name-based search where n = total blueprints
- **LoadBlueprint**: O(1) direct asset load
- **SearchBlueprints**: O(n) where n = total blueprints, with early termination at MaxResults
- **ListAllBlueprints**: O(n) where n = blueprints under BasePath

## Testing

### Manual Testing

Since there's no automated test infrastructure yet, manual testing should verify:

1. Finding blueprints by name
2. Finding blueprints by full path
3. Handling missing blueprints
4. Searching with various terms
5. Listing all blueprints
6. Widget blueprint detection

### Python Integration Tests

The existing Python MCP tests should continue to pass when commands are migrated to use this service.

## Migration Strategy

To migrate existing command handlers to use this service:

1. Create service instance in command handler constructor
2. Replace direct blueprint finding logic with service calls
3. Convert `TResult<T>` to JSON responses for MCP compatibility
4. Remove duplicate code from command handlers

See the migration example in `docs/examples/BlueprintServiceIntegrationExample.h`.

## Future Enhancements

Potential improvements for future iterations:

- **Caching**: Add blueprint cache to avoid repeated Asset Registry queries
- **Async Operations**: Support asynchronous blueprint loading
- **Filtering**: Add filters for blueprint type, parent class, etc.
- **Metadata Expansion**: Include more blueprint metadata (variables, functions, components)

## References

- [CPP_REFACTORING_DESIGN.md](../CPP_REFACTORING_DESIGN.md) - Overall refactoring design
- [Integration Example](examples/BlueprintServiceIntegrationExample.h) - Usage examples
- Issue #21 - Phase 2, Task 1: Extract BlueprintDiscoveryService

## License

MIT License - See repository root for full license text
