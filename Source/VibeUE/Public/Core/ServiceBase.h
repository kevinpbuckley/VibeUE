#pragma once

#include "CoreMinimal.h"
#include "Core/Result.h"
#include "Core/ServiceContext.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVibeUEServices, Log, All);

/**
 * Base class for all VibeUE services
 * Provides common functionality and enforces patterns
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

    // Lifecycle
    virtual void Initialize() {}
    virtual void Shutdown() {}

protected:
    // Context access
    TSharedPtr<FServiceContext> GetContext() const { return Context; }
    
    // Logging helpers
    void LogInfo(const FString& Message) const
    {
        UE_LOG(LogVibeUEServices, Log, TEXT("%s"), *Message);
    }
    
    void LogWarning(const FString& Message) const
    {
        UE_LOG(LogVibeUEServices, Warning, TEXT("%s"), *Message);
    }
    
    void LogError(const FString& Message) const
    {
        UE_LOG(LogVibeUEServices, Error, TEXT("%s"), *Message);
    }

private:
    TSharedPtr<FServiceContext> Context;
};
