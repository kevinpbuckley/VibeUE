#pragma once

#include "CoreMinimal.h"

/**
 * Shared context for all services
 * Contains common dependencies and configuration
 */
class VIBEUE_API FServiceContext
{
public:
    FServiceContext();
    ~FServiceContext();

    // Logging category
    FName GetLogCategoryName() const { return TEXT("LogVibeUE"); }
    
    // Configuration
    bool IsDebugMode() const { return bDebugMode; }
    void SetDebugMode(bool bEnabled) { bDebugMode = bEnabled; }

private:
    bool bDebugMode = false;
};
