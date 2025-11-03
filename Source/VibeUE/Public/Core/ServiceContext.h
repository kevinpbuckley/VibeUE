#pragma once

#include "CoreMinimal.h"

/**
 * Shared context for all services
 * Provides access to common resources and configuration
 */
class VIBEUE_API FServiceContext
{
public:
	FServiceContext()
		: bIsInitialized(false)
	{
	}

	virtual ~FServiceContext() = default;

	// Lifecycle
	virtual void Initialize()
	{
		bIsInitialized = true;
	}

	virtual void Shutdown()
	{
		bIsInitialized = false;
	}

	bool IsInitialized() const { return bIsInitialized; }

	// Configuration
	void SetConfigValue(const FString& Key, const FString& Value)
	{
		ConfigValues.Add(Key, Value);
	}

	FString GetConfigValue(const FString& Key, const FString& DefaultValue = FString()) const
	{
		const FString* Value = ConfigValues.Find(Key);
		return Value ? *Value : DefaultValue;
	}

private:
	bool bIsInitialized;
	TMap<FString, FString> ConfigValues;
};
