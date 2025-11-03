#include "Core/ServiceBase.h"
#include "Core/ErrorCodes.h"

TResult<void> FServiceBase::ValidateString(const FString& Value, const FString& ParamName) const
{
    if (Value.IsEmpty())
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PARAM_INVALID,
            FString::Printf(TEXT("Parameter '%s' cannot be empty"), *ParamName)
        );
    }
    return TResult<void>::Success();
}

TResult<void> FServiceBase::ValidateArray(const TArray<FString>& Value, const FString& ParamName) const
{
    if (Value.Num() == 0)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::PARAM_INVALID,
            FString::Printf(TEXT("Parameter '%s' cannot be empty"), *ParamName)
        );
    }
    return TResult<void>::Success();
}
