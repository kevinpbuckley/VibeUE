// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/Common/ServiceBase.h"

FServiceBase::FServiceBase(TSharedPtr<FServiceContext> InContext)
	: Context(InContext)
{
	check(Context.IsValid());
}

TResult<void> FServiceBase::ValidateNotEmpty(const FString& Value, const FString& ParamName) const
{
	if (Value.IsEmpty())
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::PARAM_EMPTY,
			FString::Printf(TEXT("Parameter '%s' cannot be empty"), *ParamName)
		);
	}
	return TResult<void>::Success();
}

TResult<void> FServiceBase::ValidateNotNull(const void* Value, const FString& ParamName) const
{
	if (Value == nullptr)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("Parameter '%s' cannot be null"), *ParamName)
		);
	}
	return TResult<void>::Success();
}

TResult<void> FServiceBase::ValidateRange(int32 Value, int32 Min, int32 Max, const FString& ParamName) const
{
	if (Value < Min || Value > Max)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::PARAM_OUT_OF_RANGE,
			FString::Printf(TEXT("Parameter '%s' value %d is out of range [%d, %d]"), *ParamName, Value, Min, Max)
		);
	}
	return TResult<void>::Success();
}

TResult<void> FServiceBase::ValidateArray(const TArray<FString>& Value, const FString& ParamName) const
{
	if (Value.Num() == 0)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::PARAM_EMPTY,
			FString::Printf(TEXT("Array parameter '%s' cannot be empty"), *ParamName)
		);
	}
	return TResult<void>::Success();
}

void FServiceBase::LogInfo(const FString& Message) const
{
	UE_LOG(LogTemp, Log, TEXT("[%s] %s"), *Context->GetLogCategoryName(), *Message);
}

void FServiceBase::LogWarning(const FString& Message) const
{
	UE_LOG(LogTemp, Warning, TEXT("[%s] %s"), *Context->GetLogCategoryName(), *Message);
}

void FServiceBase::LogError(const FString& Message) const
{
	UE_LOG(LogTemp, Error, TEXT("[%s] %s"), *Context->GetLogCategoryName(), *Message);
}
