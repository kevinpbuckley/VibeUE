// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declarations
class FServiceContext;
class UDataAsset;

/**
 * Handler class for Data Asset management MCP commands
 * 
 * Provides reflection-based discovery and manipulation of UDataAsset subclasses.
 * Supports creating, loading, modifying, and saving data assets with full
 * property reflection for any data asset type.
 */
class VIBEUE_API FDataAssetCommands
{
public:
    FDataAssetCommands();

    /**
     * Main command handler - routes to specific action handlers
     * @param CommandType The command type (should be "manage_data_asset")
     * @param Params JSON parameters including "action" field
     * @return JSON response with success/error status and data
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Service context for shared services
    TSharedPtr<FServiceContext> ServiceContext;

    // ========== Help ==========
    TSharedPtr<FJsonObject> HandleHelp(const TSharedPtr<FJsonObject>& Params);

    // ========== Discovery Actions ==========
    
    /** List all available data asset classes that can be created */
    TSharedPtr<FJsonObject> HandleSearchTypes(const TSharedPtr<FJsonObject>& Params);
    
    /** List all data assets of a specific type in the project */
    TSharedPtr<FJsonObject> HandleList(const TSharedPtr<FJsonObject>& Params);

    // ========== Asset Lifecycle ==========
    
    /** Create a new data asset of a specified type */
    TSharedPtr<FJsonObject> HandleCreate(const TSharedPtr<FJsonObject>& Params);

    // ========== Property Reflection ==========
    
    /** Get all information about a data asset including its properties */
    TSharedPtr<FJsonObject> HandleGetInfo(const TSharedPtr<FJsonObject>& Params);
    
    /** List all editable properties on a data asset */
    TSharedPtr<FJsonObject> HandleListProperties(const TSharedPtr<FJsonObject>& Params);
    
    /** Get a specific property value from a data asset */
    TSharedPtr<FJsonObject> HandleGetProperty(const TSharedPtr<FJsonObject>& Params);
    
    /** Set a property value on a data asset */
    TSharedPtr<FJsonObject> HandleSetProperty(const TSharedPtr<FJsonObject>& Params);
    
    /** Set multiple properties on a data asset at once */
    TSharedPtr<FJsonObject> HandleSetProperties(const TSharedPtr<FJsonObject>& Params);

    // ========== Class Reflection ==========
    
    /** Get information about a data asset class (without an instance) */
    TSharedPtr<FJsonObject> HandleGetClassInfo(const TSharedPtr<FJsonObject>& Params);

    // ========== Response Helpers ==========
    
    TSharedPtr<FJsonObject> CreateSuccessResponse(const FString& Message = TEXT(""));
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage, const FString& ErrorCode = TEXT("ERROR"));
    TSharedPtr<FJsonObject> CreateErrorResponseWithParams(const FString& ErrorMessage, const TArray<FString>& ValidParams);
};
