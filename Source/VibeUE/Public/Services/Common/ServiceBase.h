#pragma once

#include "CoreMinimal.h"
#include "Core/Result.h"
#include "Core/ServiceContext.h"
#include "Core/ErrorCodes.h"

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
	
	// Logging helpers
	void LogInfo(const FString& Message) const
	{
		UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
	}

	void LogWarning(const FString& Message) const
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
	}

	void LogError(const FString& Message) const
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
	}

	// Validation helpers
	template<typename T>
	TResult<T> ValidateNotNull(T* Pointer, const FString& ErrorCode, const FString& Message) const
	{
		if (!Pointer)
		{
			return TResult<T>::Error(ErrorCode, Message);
		}
		return TResult<T>::Success(Pointer);
	}

	TResult<void> ValidateString(const FString& Value, const FString& ParamName) const
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

	TResult<void> ValidateArray(const TArray<FString>& Value, const FString& ParamName) const
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

private:
	TSharedPtr<FServiceContext> Context;
};
