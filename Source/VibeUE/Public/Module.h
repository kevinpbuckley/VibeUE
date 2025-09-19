#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FModule>("VibeUE");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("VibeUE");
	}
}; 
