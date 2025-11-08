// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @class TResult
 * @brief Result type for operations that can succeed or fail
 * 
 * Provides type safety and avoids runtime JSON parsing.
 * Supports both value types and void operations.
 * 
 * @tparam T The type of value returned on success
 */
template<typename T>
class VIBEUE_API TResult
{
public:
	/**
	 * @brief Creates a successful result with a value (copy)
	 * @param Value The value to store in the result
	 * @return A successful TResult containing the value
	 */
	static TResult Success(const T& Value)
	{
		return TResult(true, Value, FString(), FString());
	}

	/**
	 * @brief Creates a successful result with a value (move)
	 * @param Value The value to store in the result
	 * @return A successful TResult containing the value
	 */
	static TResult Success(T&& Value)
	{
		return TResult(true, MoveTemp(Value), FString(), FString());
	}

	/**
	 * @brief Creates an error result with error details
	 * @param ErrorCode The standardized error code
	 * @param ErrorMessage Human-readable error message
	 * @return An error TResult
	 */
	static TResult Error(const FString& ErrorCode, const FString& ErrorMessage)
	{
		return TResult(false, T(), ErrorCode, ErrorMessage);
	}

	/**
	 * @brief Checks if the result represents success
	 * @return true if successful, false otherwise
	 */
	bool IsSuccess() const { return bSuccess; }

	/**
	 * @brief Checks if the result represents an error
	 * @return true if error, false otherwise
	 */
	bool IsError() const { return !bSuccess; }

	/**
	 * @brief Gets the value from a successful result
	 * @return The stored value
	 * @note Will trigger assertion if called on an error result
	 */
	const T& GetValue() const
	{
		check(bSuccess);
		return Value;
	}

	/**
	 * @brief Gets the error code from an error result
	 * @return The error code string
	 */
	const FString& GetErrorCode() const { return ErrorCode; }

	/**
	 * @brief Gets the error message from an error result
	 * @return The error message string
	 */
	const FString& GetErrorMessage() const { return ErrorMessage; }

	/**
	 * @brief Maps the value to a different type if successful
	 * @tparam U The new type to map to
	 * @param Fn Function to transform the value
	 * @return A new TResult with the transformed value or the same error
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
	 * @brief Chains another operation that returns a TResult
	 * @tparam U The type of the chained result
	 * @param Fn Function that takes the value and returns a new TResult
	 * @return The result of the chained operation or the original error
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
 * @class TResult<void>
 * @brief Specialization for void operations
 * 
 * Used for operations that don't return a value but can still fail.
 */
template<>
class VIBEUE_API TResult<void>
{
public:
	/**
	 * @brief Creates a successful void result
	 * @return A successful TResult<void>
	 */
	static TResult Success()
	{
		return TResult(true, FString(), FString());
	}

	/**
	 * @brief Creates an error result with error details
	 * @param ErrorCode The standardized error code
	 * @param ErrorMessage Human-readable error message
	 * @return An error TResult<void>
	 */
	static TResult Error(const FString& ErrorCode, const FString& ErrorMessage)
	{
		return TResult(false, ErrorCode, ErrorMessage);
	}

	/**
	 * @brief Checks if the result represents success
	 * @return true if successful, false otherwise
	 */
	bool IsSuccess() const { return bSuccess; }

	/**
	 * @brief Checks if the result represents an error
	 * @return true if error, false otherwise
	 */
	bool IsError() const { return !bSuccess; }

	/**
	 * @brief Gets the error code from an error result
	 * @return The error code string
	 */
	const FString& GetErrorCode() const { return ErrorCode; }

	/**
	 * @brief Gets the error message from an error result
	 * @return The error message string
	 */
	const FString& GetErrorMessage() const { return ErrorMessage; }

private:
	TResult(bool bInSuccess, const FString& InErrorCode, const FString& InErrorMessage)
		: bSuccess(bInSuccess), ErrorCode(InErrorCode), ErrorMessage(InErrorMessage)
	{}

	bool bSuccess;
	FString ErrorCode;
	FString ErrorMessage;
};
