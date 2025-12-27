// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

// Forward declarations
class UWorld;
class UEditorEngine;
class FServiceBase;
class IAssetRegistry;

/**
 * Shared context object that services use to access common resources.
 * Provides thread-safe access to logging, caching, and Unreal Engine state.
 * Enables dependency injection for testability.
 */
class VIBEUE_API FServiceContext
{
public:
	/**
	 * Default constructor.
	 * Initializes the service context with default values.
	 */
	FServiceContext();

	/**
	 * Destructor.
	 * Cleans up registered services and resources.
	 */
	~FServiceContext();

	// ========================================
	// Logging
	// ========================================

	/**
	 * Logs an informational message from a service.
	 * Thread-safe.
	 *
	 * @param Message The message to log
	 * @param ServiceName The name of the service logging the message
	 */
	void LogInfo(const FString& Message, const FString& ServiceName) const;

	/**
	 * Logs a warning message from a service.
	 * Thread-safe.
	 *
	 * @param Message The warning message to log
	 * @param ServiceName The name of the service logging the warning
	 */
	void LogWarning(const FString& Message, const FString& ServiceName) const;

	/**
	 * Logs an error message from a service.
	 * Thread-safe.
	 *
	 * @param Message The error message to log
	 * @param ServiceName The name of the service logging the error
	 */
	void LogError(const FString& Message, const FString& ServiceName) const;

	// ========================================
	// Unreal Engine Access
	// ========================================

	/**
	 * Gets the current world context.
	 * Thread-safe.
	 *
	 * @return Pointer to the current UWorld, or nullptr if not available
	 */
	UWorld* GetWorld() const;

	/**
	 * Gets the editor engine instance.
	 * Thread-safe.
	 *
	 * @return Pointer to the UEditorEngine, or nullptr if not in editor mode
	 */
	UEditorEngine* GetEditorEngine() const;

	/**
	 * Gets the asset registry instance.
	 * Caches the registry on first access for performance.
	 * Thread-safe.
	 *
	 * @return Pointer to the IAssetRegistry, or nullptr if not available
	 */
	IAssetRegistry* GetAssetRegistry() const;

	// ========================================
	// Service Registration (for inter-service communication)
	// ========================================

	/**
	 * Registers a service with the context.
	 * Thread-safe.
	 *
	 * @param ServiceName The unique name identifier for the service
	 * @param Service Shared pointer to the service instance
	 */
	void RegisterService(const FString& ServiceName, TSharedPtr<FServiceBase> Service);

	/**
	 * Retrieves a registered service by name.
	 * Thread-safe.
	 *
	 * @param ServiceName The unique name identifier for the service
	 * @return Shared pointer to the service if found, nullptr otherwise
	 */
	TSharedPtr<FServiceBase> GetService(const FString& ServiceName) const;

	// ========================================
	// Configuration
	// ========================================

	/**
	 * Gets a configuration value by key.
	 * Thread-safe.
	 *
	 * @param Key The configuration key
	 * @param DefaultValue The default value to return if key not found
	 * @return The configuration value, or DefaultValue if not found
	 */
	FString GetConfigValue(const FString& Key, const FString& DefaultValue = TEXT("")) const;

	/**
	 * Sets a configuration value.
	 * Thread-safe.
	 *
	 * @param Key The configuration key
	 * @param Value The value to store
	 */
	void SetConfigValue(const FString& Key, const FString& Value);

private:
	/** Map of registered services for inter-service communication */
	TMap<FString, TSharedPtr<FServiceBase>> Services;

	/** Map of configuration key-value pairs */
	TMap<FString, FString> ConfigValues;

	/** Cached asset registry instance for performance */
	mutable IAssetRegistry* CachedAssetRegistry;

	/** Critical section for thread-safe access to shared resources */
	mutable FCriticalSection Lock;
};
