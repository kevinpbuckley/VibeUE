#pragma once

#include "CoreMinimal.h"
#include "Core/ServiceContext.h"

/**
 * Base class for all services in VibeUE
 * Provides common functionality and context access
 */
class VIBEUE_API FServiceBase
{
public:
    explicit FServiceBase(TSharedPtr<FServiceContext> InContext)
        : Context(InContext)
    {
        if (!Context.IsValid())
        {
            Context = MakeShared<FServiceContext>();
        }
    }

    virtual ~FServiceBase() = default;

protected:
    TSharedPtr<FServiceContext> Context;
};
