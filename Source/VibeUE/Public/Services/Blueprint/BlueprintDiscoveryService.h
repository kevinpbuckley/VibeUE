// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/Blueprint/Types/BlueprintTypes.h"
#include "Core/Result.h"

// Forward declarations
class UBlueprint;

/**
 * @class FBlueprintDiscoveryService
 * @brief Service responsible for discovering and loading blueprints
 * 
 * This service provides focused blueprint discovery functionality extracted from
 * BlueprintCommands.cpp. It handles finding, loading, searching, and listing
 * blueprint assets using the Unreal Engine Asset Registry.
 * 
 * All methods return TResult<T> for type-safe error handling, avoiding the need
 * for runtime JSON parsing in the service layer.
 * 
 * @note This is part of Phase 2 refactoring to extract Blueprint domain into
 * focused services as per CPP_REFACTORING_DESIGN.md
 * 
 * @see TResult
 * @see FServiceBase
 */
class VIBEUE_API FBlueprintDiscoveryService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state (can be nullptr)
     */
    explicit FBlueprintDiscoveryService(TSharedPtr<FServiceContext> Context);

    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("BlueprintDiscoveryService"); }

    /**
     * @brief Find a blueprint by name or path
     * 
     * Attempts to find and load a blueprint using multiple strategies:
     * 1. Direct path loading if input starts with "/"
     * 2. Default path search under /Game/Blueprints/
     * 3. Recursive Asset Registry search
     * 
     * @param BlueprintName Blueprint name or full path (e.g., "MyBlueprint" or "/Game/Blueprints/MyBlueprint")
     * @return TResult containing the blueprint pointer on success, or error code and message on failure
     * 
     * @note This method is case-insensitive when searching by name
     */
    TResult<UBlueprint*> FindBlueprint(const FString& BlueprintName);

    /**
     * @brief Load a blueprint from a specific path
     * 
     * Loads a blueprint directly from the specified asset path without fallback search.
     * 
     * @param BlueprintPath Full asset path to the blueprint
     * @return TResult containing the blueprint pointer on success, or error code and message on failure
     */
    TResult<UBlueprint*> LoadBlueprint(const FString& BlueprintPath);

    /**
     * @brief Search for blueprints matching a search term
     * 
     * Performs a case-insensitive search across all blueprints in the /Game directory,
     * returning detailed information about matching assets.
     * 
     * @param SearchTerm Search string to match against blueprint names
     * @param MaxResults Maximum number of results to return (default 100)
     * @return TResult containing array of FBlueprintInfo structures or error
     */
    TResult<TArray<FBlueprintInfo>> SearchBlueprints(const FString& SearchTerm, int32 MaxResults = 100);

    /**
     * @brief List all blueprints in a given base path
     * 
     * Returns object paths for all blueprints found recursively under the specified path.
     * Includes both regular Blueprints and Widget Blueprints.
     * 
     * @param BasePath Root path to search from (default "/Game")
     * @return TResult containing array of blueprint object paths or error
     */
    TResult<TArray<FString>> ListAllBlueprints(const FString& BasePath = TEXT("/Game"));

    /**
     * @brief Get detailed information about a blueprint
     * 
     * Extracts metadata from a blueprint including name, path, parent class,
     * type, and whether it's a Widget Blueprint.
     * 
     * @param Blueprint Blueprint to get information from (must not be null)
     * @return TResult containing FBlueprintInfo structure or error
     */
    TResult<FBlueprintInfo> GetBlueprintInfo(UBlueprint* Blueprint);

    /**
     * @brief Check if a blueprint exists
     * 
     * Convenience method to check blueprint existence without loading full metadata.
     * 
     * @param BlueprintName Blueprint name or path to check
     * @return TResult containing true if blueprint exists, false otherwise, or error on invalid input
     */
    TResult<bool> BlueprintExists(const FString& BlueprintName);
};
