#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FVibeUEModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FVibeUEModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FVibeUEModule>("VibeUE");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("VibeUE");
	}
}; 