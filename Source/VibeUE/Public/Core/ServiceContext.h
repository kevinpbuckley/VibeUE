// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @struct FServiceContext
 * @brief Shared context for all services
 * 
 * Provides access to common resources and configuration needed by services.
 * Services receive this context through dependency injection.
 */
struct VIBEUE_API FServiceContext
{
public:
	/**
	 * @brief Default constructor
	 */
	FServiceContext()
		: LogCategoryName(TEXT("VibeUE"))
	{
	}

	/**
	 * @brief Gets the log category name for service logging
	 * @return The log category name
	 */
	const FString& GetLogCategoryName() const { return LogCategoryName; }

	/**
	 * @brief Sets the log category name for service logging
	 * @param InLogCategoryName The new log category name
	 */
	void SetLogCategoryName(const FString& InLogCategoryName)
	{
		LogCategoryName = InLogCategoryName;
	}

private:
	/** Log category name used for all service logging */
	FString LogCategoryName;
};
