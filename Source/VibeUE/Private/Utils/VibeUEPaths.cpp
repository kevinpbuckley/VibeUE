// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Utils/VibeUEPaths.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogVibeUEPaths, Log, All);

// Static member initialization
FString FVibeUEPaths::CachedPluginDir;
bool FVibeUEPaths::bCacheInitialized = false;

FString FVibeUEPaths::GetPluginDir()
{
	// Return cached value if available
	if (bCacheInitialized && !CachedPluginDir.IsEmpty())
	{
		return CachedPluginDir;
	}

	bCacheInitialized = true;

	// Method 1: Use IPluginManager (most reliable)
	IPluginManager& PluginManager = IPluginManager::Get();
	TSharedPtr<IPlugin> Plugin = PluginManager.FindPlugin(TEXT("VibeUE"));
	if (Plugin.IsValid())
	{
		CachedPluginDir = Plugin->GetBaseDir();
		UE_LOG(LogVibeUEPaths, Log, TEXT("Found VibeUE plugin via IPluginManager: %s"), *CachedPluginDir);
		return CachedPluginDir;
	}

	// Method 2: Fallback - search known locations
	TArray<FString> SearchPaths;
	
	// Project plugins (local development)
	SearchPaths.Add(FPaths::ProjectPluginsDir() / TEXT("VibeUE"));
	
	// Engine Marketplace (FAB install)
	SearchPaths.Add(FPaths::EnginePluginsDir() / TEXT("Marketplace") / TEXT("VibeUE"));
	
	// Engine plugins root
	SearchPaths.Add(FPaths::EnginePluginsDir() / TEXT("VibeUE"));
	
	// Scan Marketplace folder for any VibeUE folder (FAB may use random names)
	FString MarketplacePath = FPaths::EnginePluginsDir() / TEXT("Marketplace");
	if (FPaths::DirectoryExists(MarketplacePath))
	{
		IFileManager& FileManager = IFileManager::Get();
		TArray<FString> Directories;
		FileManager.FindFiles(Directories, *(MarketplacePath / TEXT("*")), false, true);
		
		for (const FString& DirName : Directories)
		{
			FString PotentialPath = MarketplacePath / DirName;
			// Check if this directory contains VibeUE.uplugin
			if (FPaths::FileExists(PotentialPath / TEXT("VibeUE.uplugin")))
			{
				SearchPaths.Add(PotentialPath);
			}
		}
	}

	// Check each search path
	for (const FString& SearchPath : SearchPaths)
	{
		FString AbsPath = FPaths::ConvertRelativePathToFull(SearchPath);
		if (FPaths::DirectoryExists(AbsPath))
		{
			// Verify it's actually the VibeUE plugin by checking for the .uplugin file
			if (FPaths::FileExists(AbsPath / TEXT("VibeUE.uplugin")))
			{
				CachedPluginDir = AbsPath;
				UE_LOG(LogVibeUEPaths, Log, TEXT("Found VibeUE plugin via search: %s"), *CachedPluginDir);
				return CachedPluginDir;
			}
		}
	}

	UE_LOG(LogVibeUEPaths, Warning, TEXT("Could not locate VibeUE plugin directory"));
	return FString();
}

FString FVibeUEPaths::GetPluginContentDir()
{
	FString PluginDir = GetPluginDir();
	if (PluginDir.IsEmpty())
	{
		return FString();
	}
	return PluginDir / TEXT("Content");
}

FString FVibeUEPaths::GetHelpDir()
{
	FString ContentDir = GetPluginContentDir();
	if (ContentDir.IsEmpty())
	{
		return FString();
	}
	return ContentDir / TEXT("Help");
}

FString FVibeUEPaths::GetInstructionsDir()
{
	FString ContentDir = GetPluginContentDir();
	if (ContentDir.IsEmpty())
	{
		return FString();
	}
	return ContentDir / TEXT("instructions");
}

FString FVibeUEPaths::GetConfigDir()
{
	FString PluginDir = GetPluginDir();
	if (PluginDir.IsEmpty())
	{
		return FString();
	}
	return PluginDir / TEXT("Config");
}
