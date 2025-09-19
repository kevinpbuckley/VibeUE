#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Asset Discovery and Procedural Generation MCP commands
 */
class VIBEUE_API FAssetCommands
{
public:
    FAssetCommands();

    // Handle asset commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Asset Discovery command handlers
    TSharedPtr<FJsonObject> HandleImportTextureAsset(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleExportTextureForAnalysis(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleOpenAssetInEditor(const TSharedPtr<FJsonObject>& Params);
    
    // Helper functions
    TSharedPtr<FJsonObject> CreateSuccessResponse(const FString& Message = TEXT(""));
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage);
};
