#pragma once

#include "CoreMinimal.h"
#include "Core/Result.h"
#include "Core/ServiceContext.h"

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
        check(Context.IsValid());
    }

    virtual ~FServiceBase() = default;

    // Lifecycle
    virtual void Initialize() {}
    virtual void Shutdown() {}

protected:
    // Context access
    TSharedPtr<FServiceContext> GetContext() const { return Context; }
    
    // Validation helpers
    template<typename T>
    TResult<T*> ValidateNotNull(T* Pointer, const FString& ErrorCode, const FString& Message) const
    {
        if (!Pointer)
        {
            return TResult<T*>::Error(ErrorCode, Message);
        }
        return TResult<T*>::Success(Pointer);
    }

    TResult<void> ValidateString(const FString& Value, const FString& ParamName) const;
    TResult<void> ValidateArray(const TArray<FString>& Value, const FString& ParamName) const;

private:
    TSharedPtr<FServiceContext> Context;
};
