// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/Common/ServiceBase.h"
#include "Core/ErrorCodes.h"

TResult<void> FServiceBase::ValidateString(const FString& Value, const FString& ParamName) const
{
	if (Value.IsEmpty())
	{
		FString Message = FString::Printf(TEXT("Parameter '%s' cannot be empty"), *ParamName);
		LogError(Message);
		return TResult<void>::Error(VibeUE::ErrorCodes::PARAM_INVALID, Message);
	}
	return TResult<void>::Success();
}
