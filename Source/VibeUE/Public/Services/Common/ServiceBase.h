// Copyright VibeUE 2025

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/Result.h"
#include "Core/ErrorCodes.h"
#include "Core/ServiceContext.h"

/**
 * @class FServiceBase
 * @brief Base class for all VibeUE services
 * 
 * Provides common functionality including:
 * - Validation helpers for input parameters
 * - Logging helpers with consistent formatting
 * - Service lifecycle management (Initialize/Shutdown)
 * - Access to shared service context
 * 
 * All services should inherit from this class to ensure consistent patterns.
 */
class VIBEUE_API FServiceBase
{
public:
	/**
	 * @brief Constructor with dependency injection
	 * @param InContext Shared service context
	 */
	explicit FServiceBase(TSharedPtr<FServiceContext> InContext);

	/**
	 * @brief Virtual destructor
	 */
	virtual ~FServiceBase() = default;

	/**
	 * @brief Initialize the service
	 * 
	 * Override to perform service-specific initialization.
	 * Called once during service setup.
	 */
	virtual void Initialize() {}

	/**
	 * @brief Shutdown the service
	 * 
	 * Override to perform cleanup.
	 * Called once during service teardown.
	 */
	virtual void Shutdown() {}

protected:
	/**
	 * @brief Gets the shared service context
	 * @return The service context
	 */
	TSharedPtr<FServiceContext> GetContext() const { return Context; }

	/**
	 * @brief Validates that a string is not empty
	 * @param Value The string to validate
	 * @param ParamName The parameter name for error messages
	 * @return Success or error result
	 */
	TResult<void> ValidateNotEmpty(const FString& Value, const FString& ParamName) const;

	/**
	 * @brief Validates that a pointer is not null
	 * @param Value The pointer to validate
	 * @param ParamName The parameter name for error messages
	 * @return Success or error result
	 */
	TResult<void> ValidateNotNull(const void* Value, const FString& ParamName) const;

	/**
	 * @brief Validates that a value is within a specified range
	 * @param Value The value to validate
	 * @param Min The minimum allowed value (inclusive)
	 * @param Max The maximum allowed value (inclusive)
	 * @param ParamName The parameter name for error messages
	 * @return Success or error result
	 */
	TResult<void> ValidateRange(int32 Value, int32 Min, int32 Max, const FString& ParamName) const;

	/**
	 * @brief Validates that an array is not empty
	 * @param Value The array to validate
	 * @param ParamName The parameter name for error messages
	 * @return Success or error result
	 */
	TResult<void> ValidateArray(const TArray<FString>& Value, const FString& ParamName) const;

	/**
	 * @brief Logs an informational message
	 * @param Message The message to log
	 */
	void LogInfo(const FString& Message) const;

	/**
	 * @brief Logs a warning message
	 * @param Message The message to log
	 */
	void LogWarning(const FString& Message) const;

	/**
	 * @brief Logs an error message
	 * @param Message The message to log
	 */
	void LogError(const FString& Message) const;

	/**
	 * @brief Gets the service name for logging
	 * @return The service name
	 * 
	 * This is a pure virtual method that must be implemented by derived classes.
	 */
	virtual FString GetServiceName() const = 0;

private:
	/** Shared service context */
	TSharedPtr<FServiceContext> Context;
};
