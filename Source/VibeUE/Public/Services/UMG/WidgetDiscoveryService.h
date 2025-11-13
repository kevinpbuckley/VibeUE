// Copyright VibeUE 2025

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/UMG/Types/WidgetTypes.h"
#include "Core/Result.h"

// Forward declarations
class UWidgetBlueprint;
class UWidget;

/**
 * @class FWidgetDiscoveryService
 * @brief Service responsible for discovering and loading widget blueprints
 * 
 * This service provides focused widget blueprint discovery functionality extracted from
 * UMGCommands.cpp. It handles finding, loading, searching, and listing widget blueprints
 * using the Unreal Engine Asset Registry.
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @see TResult
 * @see FServiceBase
 */
class VIBEUE_API FWidgetDiscoveryService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state (can be nullptr)
     */
    explicit FWidgetDiscoveryService(TSharedPtr<FServiceContext> Context);

    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("WidgetDiscoveryService"); }

    /**
     * @brief Find a widget blueprint by name or path
     * 
     * Attempts to find and load a widget blueprint using multiple strategies:
     * 1. Direct path loading if input starts with "/"
     * 2. Default path search under /Game/UI/
     * 3. Recursive Asset Registry search
     * 
     * @param WidgetBlueprintName Widget blueprint name or full path
     * @return TResult containing the widget blueprint pointer on success, or error
     */
    TResult<UWidgetBlueprint*> FindWidgetBlueprint(const FString& WidgetBlueprintName);

    /**
     * @brief Load a widget blueprint from a specific path
     * 
     * @param WidgetBlueprintPath Full asset path to the widget blueprint
     * @return TResult containing the widget blueprint pointer on success, or error
     */
    TResult<UWidgetBlueprint*> LoadWidgetBlueprint(const FString& WidgetBlueprintPath);

    /**
     * @brief Search for widget blueprints matching a search term
     * 
     * @param SearchTerm Search string to match against widget blueprint names
     * @param MaxResults Maximum number of results to return (default 100)
     * @return TResult containing array of FWidgetBlueprintInfo structures or error
     */
    TResult<TArray<FWidgetBlueprintInfo>> SearchWidgetBlueprints(const FString& SearchTerm, int32 MaxResults = 100);

    /**
     * @brief List all widget blueprints in a given base path
     * 
     * @param BasePath Root path to search from (default "/Game")
     * @return TResult containing array of widget blueprint object paths or error
     */
    TResult<TArray<FString>> ListAllWidgetBlueprints(const FString& BasePath = TEXT("/Game"));

    /**
     * @brief Get detailed information about a widget blueprint
     * 
     * @param WidgetBlueprint Widget blueprint to get information from (must not be null)
     * @return TResult containing FWidgetBlueprintInfo structure or error
     */
    TResult<FWidgetBlueprintInfo> GetWidgetBlueprintInfo(UWidgetBlueprint* WidgetBlueprint);

    /**
     * @brief Check if a widget blueprint exists
     * 
     * @param WidgetBlueprintName Widget blueprint name or path to check
     * @return TResult containing true if exists, false otherwise
     */
    TResult<bool> WidgetBlueprintExists(const FString& WidgetBlueprintName);

    /**
     * @brief Find a widget in a blueprint's widget tree by name
     * 
     * @param WidgetBlueprint Widget blueprint to search in
     * @param WidgetName Name of the widget to find
     * @return TResult containing widget pointer on success, or error
     */
    TResult<UWidget*> FindWidgetByName(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName);

    /**
     * @brief Return supported UMG widget types available for creation
     *
     * This returns a list of UMG widget type names (e.g. "TextBlock", "Button") used
     * by the tooling layer to present a palette of widget types. Implemented here so
     * handlers can delegate to the discovery service for consistent lists.
     */
    TResult<TArray<FString>> GetAvailableWidgetTypes();
};
