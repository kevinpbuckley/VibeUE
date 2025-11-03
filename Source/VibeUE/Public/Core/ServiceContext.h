// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// Forward declarations
class IAssetRegistry;

DECLARE_LOG_CATEGORY_EXTERN(LogVibeUEServices, Log, All);

/**
 * Shared context for all VibeUE services
 * Provides access to common resources and configuration
 * 
 * Usage:
 * @code
 * TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
 * Context->Initialize();
 * 
 * FBlueprintDiscoveryService Service(Context);
 * @endcode
 */
class VIBEUE_API FServiceContext
{
public:
	FServiceContext();
	virtual ~FServiceContext();

	/** Initialize the service context */
	void Initialize();

	/** Shutdown and cleanup resources */
	void Shutdown();

	/** Returns true if the context has been initialized */
	bool IsInitialized() const { return bInitialized; }

	/** Gets the asset registry interface */
	IAssetRegistry* GetAssetRegistry() const { return AssetRegistry; }

	/** Gets the global service context logger */
	static FLogCategoryBase& GetLog() { return LogVibeUEServices; }

private:
	/** Whether the context has been initialized */
	bool bInitialized;

	/** Cached asset registry interface */
	IAssetRegistry* AssetRegistry;
};
