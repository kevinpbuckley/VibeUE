// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/Result.h"
#include "Core/ServiceContext.h"

/**
 * Base class for all VibeUE services
 * Provides common functionality and enforces consistent patterns
 * 
 * All services should:
 * - Inherit from FServiceBase
 * - Accept TSharedPtr<FServiceContext> in constructor
 * - Return TResult<T> from public methods (not raw JSON)
 * - Use ErrorCodes constants for all errors
 * - Be focused on a single responsibility (<500 lines)
 * 
 * Example:
 * @code
 * class FBlueprintDiscoveryService : public FServiceBase
 * {
 * public:
 *     explicit FBlueprintDiscoveryService(TSharedPtr<FServiceContext> InContext);
 *     
 *     TResult<UBlueprint*> FindBlueprint(const FString& Name);
 * };
 * @endcode
 */
class VIBEUE_API FServiceBase
{
public:
	/**
	 * Constructor
	 * @param InContext Shared service context (must be valid)
	 */
	explicit FServiceBase(TSharedPtr<FServiceContext> InContext)
		: Context(InContext)
	{
		check(Context.IsValid());
		check(Context->IsInitialized());
	}

	virtual ~FServiceBase() = default;

	/** Called after construction to initialize service-specific resources */
	virtual void Initialize() {}
	
	/** Called before destruction to cleanup service-specific resources */
	virtual void Shutdown() {}

protected:
	/** Gets the shared service context */
	TSharedPtr<FServiceContext> GetContext() const { return Context; }
	
	/**
	 * Logs an informational message
	 * @param Message The message to log
	 */
	void LogInfo(const FString& Message) const
	{
		UE_LOG(LogVibeUEServices, Log, TEXT("%s"), *Message);
	}

	/**
	 * Logs a warning message
	 * @param Message The warning message to log
	 */
	void LogWarning(const FString& Message) const
	{
		UE_LOG(LogVibeUEServices, Warning, TEXT("%s"), *Message);
	}

	/**
	 * Logs an error message
	 * @param Message The error message to log
	 */
	void LogError(const FString& Message) const
	{
		UE_LOG(LogVibeUEServices, Error, TEXT("%s"), *Message);
	}

	/**
	 * Validates that a pointer is not null
	 * @param Pointer The pointer to validate
	 * @param ErrorCode The error code to use if validation fails
	 * @param Message The error message to use if validation fails
	 * @return Success with pointer if valid, Error otherwise
	 */
	template<typename T>
	TResult<T*> ValidateNotNull(T* Pointer, const FString& ErrorCode, const FString& Message) const
	{
		if (!Pointer)
		{
			LogError(Message);
			return TResult<T*>::Error(ErrorCode, Message);
		}
		return TResult<T*>::Success(Pointer);
	}

	/**
	 * Validates that a string is not empty
	 * @param Value The string to validate
	 * @param ParamName The parameter name (for error messages)
	 * @return Success if valid, Error otherwise
	 */
	TResult<void> ValidateString(const FString& Value, const FString& ParamName) const;

	/**
	 * Validates that an array is not empty
	 * @param Value The array to validate
	 * @param ParamName The parameter name (for error messages)
	 * @return Success if valid, Error otherwise
	 */
	template<typename T>
	TResult<void> ValidateArray(const TArray<T>& Value, const FString& ParamName) const
	{
		if (Value.Num() == 0)
		{
			FString Message = FString::Printf(TEXT("Parameter '%s' cannot be empty"), *ParamName);
			LogError(Message);
			return TResult<void>::Error(TEXT("PARAM_INVALID"), Message);
		}
		return TResult<void>::Success();
	}

private:
	TSharedPtr<FServiceContext> Context;
};
