#pragma once

#include "CoreMinimal.h"

/**
 * Centralized error codes for consistent error handling
 * Organized by domain and operation type
 */
namespace VibeUE::ErrorCodes
{
    // Parameter validation errors
    constexpr const TCHAR* PARAM_MISSING = TEXT("PARAM_MISSING");
    constexpr const TCHAR* PARAM_INVALID = TEXT("PARAM_INVALID");

    // Blueprint errors
    constexpr const TCHAR* BLUEPRINT_NOT_FOUND = TEXT("BLUEPRINT_NOT_FOUND");
    constexpr const TCHAR* BLUEPRINT_LOAD_FAILED = TEXT("BLUEPRINT_LOAD_FAILED");

    // Graph errors
    constexpr const TCHAR* GRAPH_NOT_FOUND = TEXT("GRAPH_NOT_FOUND");
    constexpr const TCHAR* GRAPH_INVALID = TEXT("GRAPH_INVALID");
    constexpr const TCHAR* GRAPH_OPERATION_FAILED = TEXT("GRAPH_OPERATION_FAILED");

    // Node errors
    constexpr const TCHAR* NODE_NOT_FOUND = TEXT("NODE_NOT_FOUND");
    constexpr const TCHAR* NODE_LIMIT_EXCEEDED = TEXT("NODE_LIMIT_EXCEEDED");

    // System errors
    constexpr const TCHAR* INTERNAL_ERROR = TEXT("INTERNAL_ERROR");
}
