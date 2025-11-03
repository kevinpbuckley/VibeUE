#pragma once

#include "CoreMinimal.h"

/**
 * Context object that can be passed to services
 * Currently minimal, can be extended with shared state
 */
class VIBEUE_API FServiceContext
{
public:
    FServiceContext() = default;
    virtual ~FServiceContext() = default;
};
