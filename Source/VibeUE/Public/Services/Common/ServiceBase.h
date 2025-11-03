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
        UE_LOG(LogTemp, Log, TEXT("[Service] %s"), *Message);
    }
    
    void LogWarning(const FString& Message) const
    {
        UE_LOG(LogTemp, Warning, TEXT("[Service] %s"), *Message);
    }
    
    void LogError(const FString& Message) const
    {
        UE_LOG(LogTemp, Error, TEXT("[Service] %s"), *Message);
    }

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

    TResult<void> ValidateString(const FString& Value, const FString& ParamName) const
    {
        if (Value.IsEmpty())
        {
            return TResult<void>::Error(TEXT("PARAM_INVALID"), 
                FString::Printf(TEXT("%s cannot be empty"), *ParamName));
        }
        return TResult<void>::Success();
    }

private:
    TSharedPtr<FServiceContext> Context;
};
