// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Utility class for resolving VibeUE plugin paths.
 * Works whether plugin is installed in Project/Plugins, Engine/Plugins/Marketplace, or elsewhere.
 */
class VIBEUE_API FVibeUEPaths
{
public:
	/**
	 * Get the base directory of the VibeUE plugin.
	 * Uses IPluginManager to find the actual plugin location regardless of install type.
	 * 
	 * @return Absolute path to the VibeUE plugin directory, or empty string if not found
	 */
	static FString GetPluginDir();

	/**
	 * Get the Content directory of the VibeUE plugin.
	 * 
	 * @return Absolute path to VibeUE/Content, or empty string if not found
	 */
	static FString GetPluginContentDir();

	/**
	 * Get the Help content directory.
	 * 
	 * @return Absolute path to VibeUE/Content/Help, or empty string if not found
	 */
	static FString GetHelpDir();

	/**
	 * Get the instructions content directory.
	 * 
	 * @return Absolute path to VibeUE/Content/instructions, or empty string if not found
	 */
	static FString GetInstructionsDir();

	/**
	 * Get the Config directory of the VibeUE plugin.
	 * 
	 * @return Absolute path to VibeUE/Config, or empty string if not found
	 */
	static FString GetConfigDir();

private:
	/** Cached plugin directory path */
	static FString CachedPluginDir;
	
	/** Whether cache has been initialized */
	static bool bCacheInitialized;
};
