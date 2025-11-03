#pragma once

#include "CoreMinimal.h"

/**
 * Context shared between Blueprint services
 * Contains shared state and utilities
 */
class VIBEUE_API FServiceContext
{
public:
    FServiceContext() {}
    virtual ~FServiceContext() {}

    // Add shared context data as needed
};

/**
 * Base class for Blueprint services
 * Provides common functionality for all services
 */
class VIBEUE_API FServiceBase
{
public:
    explicit FServiceBase(TSharedPtr<FServiceContext> InContext)
        : Context(InContext)
    {
    }

    virtual ~FServiceBase() {}

protected:
    TSharedPtr<FServiceContext> Context;
};
