#pragma once

#include "CoreMinimal.h"

/**
 * Standard error codes used across VibeUE services
 */
namespace ErrorCodes
{
    // Parameter validation errors
    inline const FString PARAM_MISSING = TEXT("PARAM_MISSING");
    inline const FString PARAM_INVALID = TEXT("PARAM_INVALID");
    
    // Blueprint errors
    inline const FString BLUEPRINT_NOT_FOUND = TEXT("BLUEPRINT_NOT_FOUND");
    inline const FString BLUEPRINT_LOAD_FAILED = TEXT("BLUEPRINT_LOAD_FAILED");
    inline const FString BLUEPRINT_ALREADY_EXISTS = TEXT("BLUEPRINT_ALREADY_EXISTS");
    
    // Asset errors
    inline const FString ASSET_NOT_FOUND = TEXT("ASSET_NOT_FOUND");
    inline const FString ASSET_LOAD_FAILED = TEXT("ASSET_LOAD_FAILED");
    
    // General errors
    inline const FString OPERATION_FAILED = TEXT("OPERATION_FAILED");
    inline const FString INTERNAL_ERROR = TEXT("INTERNAL_ERROR");
}
