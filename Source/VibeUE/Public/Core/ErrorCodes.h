#pragma once

#include "CoreMinimal.h"

/**
 * Centralized error codes for VibeUE operations
 */
namespace VibeUEErrorCodes
{
    // Blueprint Component Errors
    static const FString BLUEPRINT_NOT_FOUND = TEXT("BLUEPRINT_NOT_FOUND");
    static const FString COMPONENT_TYPE_INVALID = TEXT("COMPONENT_TYPE_INVALID");
    static const FString COMPONENT_NAME_EXISTS = TEXT("COMPONENT_NAME_EXISTS");
    static const FString COMPONENT_NOT_FOUND = TEXT("COMPONENT_NOT_FOUND");
    static const FString COMPONENT_CREATE_FAILED = TEXT("COMPONENT_CREATE_FAILED");
    static const FString PARENT_COMPONENT_NOT_FOUND = TEXT("PARENT_COMPONENT_NOT_FOUND");
    static const FString PARENT_NOT_SCENE_COMPONENT = TEXT("PARENT_NOT_SCENE_COMPONENT");
    static const FString SCS_NOT_AVAILABLE = TEXT("SCS_NOT_AVAILABLE");
    
    // General Errors
    static const FString INVALID_PARAMETERS = TEXT("INVALID_PARAMETERS");
    static const FString OPERATION_FAILED = TEXT("OPERATION_FAILED");
    static const FString NOT_IMPLEMENTED = TEXT("NOT_IMPLEMENTED");
}
