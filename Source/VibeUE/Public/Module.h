// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Called before engine exit to cleanup Python references */
	void OnPreExit();

	static inline FModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FModule>("VibeUE");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("VibeUE");
	}
}; 
