#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Core/Result.h"

// Forward declarations
class FAssetDiscoveryService;
class FAssetLifecycleService;
class FAssetImportService;
class FServiceContext;

/**
 * Handler class for Asset Discovery and Procedural Generation MCP commands
 * 
 * Refactored in Phase 4 (Task 19) to delegate to specialized asset services.
 * This is now a thin command handler that converts between JSON and service calls.
 */
class VIBEUE_API FAssetCommands
{
public:
    FAssetCommands();

    // Handle asset commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Asset services
    TSharedPtr<FAssetDiscoveryService> DiscoveryService;
    TSharedPtr<FAssetLifecycleService> LifecycleService;
    TSharedPtr<FAssetImportService> ImportService;
    TSharedPtr<FServiceContext> ServiceContext;

    // Asset Discovery command handlers
    TSharedPtr<FJsonObject> HandleImportTextureAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleExportTextureForAnalysis(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleOpenAssetInEditor(const TSharedPtr<FJsonObject>& Params);
    
    // Helper functions for converting TResult to JSON
    TSharedPtr<FJsonObject> CreateSuccessResponse(const FString& Message = TEXT(""));
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage);
    
    template<typename T>
    TSharedPtr<FJsonObject> ConvertResultToResponse(const TResult<T>& Result, const FString& SuccessMessage);
};
