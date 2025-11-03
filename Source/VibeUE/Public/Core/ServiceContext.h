#pragma once

#include "CoreMinimal.h"

/**
 * Shared context for all services
 * Provides access to common resources and configuration
 */
class VIBEUE_API FServiceContext
{
public:
    FServiceContext() = default;
    virtual ~FServiceContext() = default;

    // Logging configuration
    bool IsVerboseLogging() const { return bVerboseLogging; }
    void SetVerboseLogging(bool bEnabled) { bVerboseLogging = bEnabled; }

private:
    bool bVerboseLogging = false;
};
