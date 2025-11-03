#pragma once

#include "CoreMinimal.h"

/**
 * Shared context for services
 * Provides access to common resources and settings
 */
class VIBEUE_API FServiceContext
{
public:
    FServiceContext() = default;
    virtual ~FServiceContext() = default;

    // Add context fields as needed for services
};
