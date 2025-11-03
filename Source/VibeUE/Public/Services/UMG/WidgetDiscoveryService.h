#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

// Forward declarations
class UWidgetBlueprint;
class IAssetRegistry;
struct FAssetData;

/**
 * @struct FWidgetInfo
 * @brief Structure holding basic widget blueprint information
 * 
 * Contains essential metadata about a widget blueprint asset including its name,
 * path, parent class, and type information.
 */
struct VIBEUE_API FWidgetInfo
{
    /** The name of the widget blueprint asset */
    FString Name;
    
    /** Full object path to the widget blueprint */
    FString Path;
    
    /** Package path containing the widget blueprint */
    FString PackagePath;
    
    /** Name of the parent widget class */
    FString ParentClass;
    
    /** Widget class type name */
    FString WidgetType;

    FWidgetInfo()
    {}
};

/**
 * @class FWidgetDiscoveryService
 * @brief Service responsible for discovering and loading UMG Widget Blueprints
 * 
 * This service provides focused widget blueprint discovery functionality extracted from
 * UMGCommands.cpp and CommonUtils.cpp. It handles finding, loading, searching, and listing
 * widget blueprint assets using the Unreal Engine Asset Registry with sophisticated
 * priority-based matching.
 * 
 * All methods return TResult<T> for type-safe error handling, avoiding the need
 * for runtime JSON parsing in the service layer.
 * 
 * @note This is part of Phase 3 refactoring (Task 10) to extract UMG domain into
 * focused services as per CPP_REFACTORING_DESIGN.md
 * 
 * @see TResult
 * @see FServiceBase
 * @see Issue #31
 */
class VIBEUE_API FWidgetDiscoveryService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state (can be nullptr)
     */
    explicit FWidgetDiscoveryService(TSharedPtr<FServiceContext> Context);

    /**
     * @brief Find a widget blueprint by name or path
     * 
     * Attempts to find and load a widget blueprint using sophisticated priority-based matching:
     * 1. Direct path loading for exact matches
     * 2. Constructed object path (package.asset_name)
     * 3. Asset Registry search with priority scoring:
     *    - Priority 10: Exact asset name (case sensitive)
     *    - Priority 9: Exact asset name (case insensitive)
     *    - Priority 8: Full object path (case sensitive)
     *    - Priority 7: Full package path (case sensitive)
     *    - Priority 6: Full object path (case insensitive)
     *    - Priority 5: Full package path (case insensitive)
     *    - Priority 3: Asset name prefix match
     *    - Priority 2: Asset name contains (3+ chars)
     *    - Priority 1: Package path contains (4+ chars)
     * 
     * @param WidgetName Widget blueprint name or full path (e.g., "WBP_MainMenu" or "/Game/UI/WBP_MainMenu")
     * @return TResult containing the widget blueprint pointer on success, or error code and message on failure
     * 
     * @note Extracted from CommonUtils::FindWidgetBlueprint with enhanced error handling
     */
    TResult<UWidgetBlueprint*> FindWidget(const FString& WidgetName);

    /**
     * @brief Load a widget blueprint from a specific path
     * 
     * Loads a widget blueprint directly from the specified asset path without fallback search.
     * 
     * @param WidgetPath Full asset path to the widget blueprint
     * @return TResult containing the widget blueprint pointer on success, or error code and message on failure
     */
    TResult<UWidgetBlueprint*> LoadWidget(const FString& WidgetPath);

    /**
     * @brief Search for widget blueprints matching a search term
     * 
     * Performs a case-insensitive search across all widget blueprints in the /Game directory,
     * returning detailed information about matching assets.
     * 
     * @param SearchTerm Search string to match against widget blueprint names (empty = return all)
     * @param MaxResults Maximum number of results to return (default 100)
     * @return TResult containing array of FAssetData on success, or error code and message on failure
     */
    TResult<TArray<FAssetData>> SearchWidgets(const FString& SearchTerm = TEXT(""), int32 MaxResults = 100);

    /**
     * @brief Get all widget blueprints in the project
     * 
     * Returns comprehensive list of all widget blueprints found in the Asset Registry.
     * 
     * @return TResult containing array of FAssetData for all widget blueprints
     */
    TResult<TArray<FAssetData>> GetAllWidgets();

    /**
     * @brief Check if a widget blueprint exists
     * 
     * Verifies that a widget blueprint with the given name can be found.
     * 
     * @param WidgetName Widget blueprint name to check
     * @return TResult containing true if widget exists, false otherwise
     */
    TResult<bool> WidgetExists(const FString& WidgetName);

    /**
     * @brief Validate if a widget blueprint is valid
     * 
     * Checks if the widget blueprint exists and can be loaded successfully.
     * 
     * @param WidgetName Widget blueprint name to validate
     * @return TResult containing true if widget is valid, false otherwise
     */
    TResult<bool> IsValidWidget(const FString& WidgetName);

    /**
     * @brief Get available widget types from the palette
     * 
     * Returns list of available UMG widget class names that can be added to widget blueprints.
     * 
     * @return TResult containing array of widget type names
     */
    TResult<TArray<FString>> GetAvailableWidgetTypes();

    /**
     * @brief Get available panel widget types
     * 
     * Returns list of panel widget types (Canvas, Vertical Box, Horizontal Box, etc.)
     * that can contain other widgets.
     * 
     * @return TResult containing array of panel type names
     */
    TResult<TArray<FString>> GetAvailablePanelTypes();

    /**
     * @brief Get common widget types
     * 
     * Returns list of frequently used widget types (Button, Text, Image, etc.)
     * for quick access.
     * 
     * @return TResult containing array of common widget type names
     */
    TResult<TArray<FString>> GetCommonWidgets();

private:
    /**
     * @brief Get Asset Registry instance
     * @return Reference to the Asset Registry
     */
    IAssetRegistry& GetAssetRegistry();

    /**
     * @brief Check if code is executing during serialization
     * @return True if in serialization context, false otherwise
     */
    bool IsInSerializationContext() const;
};
