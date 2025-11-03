// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Context for services - can be extended in the future to hold shared resources
 */
class VIBEUE_API FServiceContext
{
public:
    FServiceContext() = default;
    virtual ~FServiceContext() = default;
};

/**
 * Base class for all services
 * Provides common functionality and patterns for service implementations
 */
class VIBEUE_API FServiceBase
{
public:
    explicit FServiceBase(TSharedPtr<FServiceContext> InContext)
        : Context(InContext)
    {
    }

    virtual ~FServiceBase() = default;

protected:
    TSharedPtr<FServiceContext> Context;
};
