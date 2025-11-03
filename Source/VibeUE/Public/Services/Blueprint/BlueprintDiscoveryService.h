#pragma once

#include "CoreMinimal.h"
#include "Core/ServiceBase.h"
#include "Core/TResult.h"

// Forward declarations
class UBlueprint;

/**
 * Structure holding basic blueprint information
 */
struct VIBEUE_API FBlueprintInfo
{
    FString Name;
    FString Path;
    FString PackagePath;
    FString ParentClass;
    FString BlueprintType;
    bool bIsWidgetBlueprint;

    FBlueprintInfo()
        : bIsWidgetBlueprint(false)
    {}
};

/**
 * Service responsible for discovering and loading blueprints
 * Extracted from BlueprintCommands.cpp to provide focused blueprint discovery functionality
 */
class VIBEUE_API FBlueprintDiscoveryService : public FServiceBase
{
public:
    /**
     * Constructor
     * @param Context Service context for shared state
     */
    explicit FBlueprintDiscoveryService(TSharedPtr<FServiceContext> Context);

    /**
     * Find a blueprint by name or path
     * @param BlueprintName Blueprint name or full path (e.g., "MyBlueprint" or "/Game/Blueprints/MyBlueprint")
     * @return Result containing the blueprint pointer or error
     */
    TResult<UBlueprint*> FindBlueprint(const FString& BlueprintName);

    /**
     * Load a blueprint from a specific path
     * @param BlueprintPath Full asset path to the blueprint
     * @return Result containing the blueprint pointer or error
     */
    TResult<UBlueprint*> LoadBlueprint(const FString& BlueprintPath);

    /**
     * Search for blueprints matching a search term
     * @param SearchTerm Search string to match against blueprint names
     * @param MaxResults Maximum number of results to return (default 100)
     * @return Result containing array of blueprint info structures or error
     */
    TResult<TArray<FBlueprintInfo>> SearchBlueprints(const FString& SearchTerm, int32 MaxResults = 100);

    /**
     * List all blueprints in a given base path
     * @param BasePath Root path to search from (default "/Game")
     * @return Result containing array of blueprint paths or error
     */
    TResult<TArray<FString>> ListAllBlueprints(const FString& BasePath = TEXT("/Game"));

    /**
     * Get detailed information about a blueprint
     * @param Blueprint Blueprint to get information from
     * @return Result containing blueprint info or error
     */
    TResult<FBlueprintInfo> GetBlueprintInfo(UBlueprint* Blueprint);

    /**
     * Check if a blueprint exists
     * @param BlueprintName Blueprint name or path to check
     * @return Result containing true if exists, false otherwise, or error
     */
    TResult<bool> BlueprintExists(const FString& BlueprintName);
};
