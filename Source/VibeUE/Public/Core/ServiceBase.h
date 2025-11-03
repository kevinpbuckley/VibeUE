#pragma once

#include "CoreMinimal.h"

// Forward declarations
class FServiceContext;

/**
 * Base class for all VibeUE services.
 * Provides common functionality and enforces patterns.
 * Services should inherit from this class to access shared context.
 */
class VIBEUE_API FServiceBase
{
public:
	/**
	 * Constructor that takes a service context.
	 * 
	 * @param InContext Shared pointer to the service context (must be valid)
	 */
	explicit FServiceBase(TSharedPtr<FServiceContext> InContext)
		: Context(InContext)
	{
		check(Context.IsValid());
	}

	/**
	 * Virtual destructor for proper cleanup of derived classes.
	 */
	virtual ~FServiceBase() = default;

	// ========================================
	// Lifecycle
	// ========================================

	/**
	 * Called when the service is initialized.
	 * Override in derived classes to perform initialization logic.
	 */
	virtual void Initialize() {}

	/**
	 * Called when the service is shutting down.
	 * Override in derived classes to perform cleanup logic.
	 */
	virtual void Shutdown() {}

protected:
	/**
	 * Gets the service context.
	 * 
	 * @return Shared pointer to the service context
	 */
	TSharedPtr<FServiceContext> GetContext() const { return Context; }

	/**
	 * Logs an informational message.
	 * 
	 * @param Message The message to log
	 */
	void LogInfo(const FString& Message) const;

	/**
	 * Logs a warning message.
	 * 
	 * @param Message The warning message to log
	 */
	void LogWarning(const FString& Message) const;

	/**
	 * Logs an error message.
	 * 
	 * @param Message The error message to log
	 */
	void LogError(const FString& Message) const;

	/**
	 * Gets the name of this service for logging purposes.
	 * Override in derived classes to provide a meaningful name.
	 * 
	 * @return The service name
	 */
	virtual FString GetServiceName() const { return TEXT("UnnamedService"); }

private:
	/** Shared pointer to the service context */
	TSharedPtr<FServiceContext> Context;
};
