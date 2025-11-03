// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Result type for operations that can succeed or fail
 * Provides type safety and avoids runtime JSON parsing
 * 
 * Example usage:
 * @code
 * TResult<UBlueprint*> Result = FindBlueprint(Name);
 * if (Result.IsSuccess()) {
 *     UBlueprint* BP = Result.GetValue();
 *     // Work with blueprint
 * } else {
 *     UE_LOG(LogTemp, Error, TEXT("Error: %s"), *Result.GetErrorMessage());
 * }
 * @endcode
 */
template<typename T>
class VIBEUE_API TResult
{
public:
	/** Creates a successful result with a value */
	static TResult Success(const T& Value)
	{
		return TResult(true, Value, FString(), FString());
	}

	/** Creates a successful result with a moved value */
	static TResult Success(T&& Value)
	{
		return TResult(true, MoveTemp(Value), FString(), FString());
	}

	/** Creates an error result with error code and message */
	static TResult Error(const FString& ErrorCode, const FString& ErrorMessage)
	{
		return TResult(false, T(), ErrorCode, ErrorMessage);
	}

	/** Returns true if the operation succeeded */
	bool IsSuccess() const { return bSuccess; }
	
	/** Returns true if the operation failed */
	bool IsError() const { return !bSuccess; }
	
	/** Gets the successful value (check IsSuccess() first!) */
	const T& GetValue() const 
	{ 
		check(bSuccess); 
		return Value; 
	}
	
	/** Gets the error code if operation failed */
	const FString& GetErrorCode() const { return ErrorCode; }
	
	/** Gets the error message if operation failed */
	const FString& GetErrorMessage() const { return ErrorMessage; }

	/**
	 * Functional composition - transforms the success value
	 * @param Fn Function to transform T -> U
	 * @return TResult<U> with transformed value or propagated error
	 */
	template<typename U>
	TResult<U> Map(TFunction<U(const T&)> Fn) const
	{
		if (IsSuccess())
		{
			return TResult<U>::Success(Fn(Value));
		}
		return TResult<U>::Error(ErrorCode, ErrorMessage);
	}

	/**
	 * Functional composition - chains operations that return TResult
	 * @param Fn Function that takes T and returns TResult<U>
	 * @return TResult<U> from Fn or propagated error
	 */
	template<typename U>
	TResult<U> FlatMap(TFunction<TResult<U>(const T&)> Fn) const
	{
		if (IsSuccess())
		{
			return Fn(Value);
		}
		return TResult<U>::Error(ErrorCode, ErrorMessage);
	}

private:
	TResult(bool bInSuccess, const T& InValue, const FString& InErrorCode, const FString& InErrorMessage)
		: bSuccess(bInSuccess), Value(InValue), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
	{}

	TResult(bool bInSuccess, T&& InValue, const FString& InErrorCode, const FString& InErrorMessage)
		: bSuccess(bInSuccess), Value(MoveTemp(InValue)), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
	{}

	bool bSuccess;
	T Value;
	FString ErrorCode;
	FString ErrorMessage;
};

/**
 * Specialization for void operations that don't return a value
 * 
 * Example usage:
 * @code
 * TResult<void> Result = DeleteBlueprint(Name);
 * if (Result.IsSuccess()) {
 *     // Operation completed successfully
 * }
 * @endcode
 */
template<>
class VIBEUE_API TResult<void>
{
public:
	/** Creates a successful result */
	static TResult Success()
	{
		return TResult(true, FString(), FString());
	}

	/** Creates an error result */
	static TResult Error(const FString& ErrorCode, const FString& ErrorMessage)
	{
		return TResult(false, ErrorCode, ErrorMessage);
	}

	/** Returns true if the operation succeeded */
	bool IsSuccess() const { return bSuccess; }
	
	/** Returns true if the operation failed */
	bool IsError() const { return !bSuccess; }
	
	/** Gets the error code if operation failed */
	const FString& GetErrorCode() const { return ErrorCode; }
	
	/** Gets the error message if operation failed */
	const FString& GetErrorMessage() const { return ErrorMessage; }

private:
	TResult(bool bInSuccess, const FString& InErrorCode, const FString& InErrorMessage)
		: bSuccess(bInSuccess), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
	{}

	bool bSuccess;
	FString ErrorCode;
	FString ErrorMessage;
};
