#include "VibeUEModule.h"
#include "VibeUEBridge.h"
#include "Modules/ModuleManager.h"
#include "EditorSubsystem.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "FVibeUEModule"

void FVibeUEModule::StartupModule()
{
	UE_LOG(LogTemp, Display, TEXT("VibeUE Module has started"));
}

void FVibeUEModule::ShutdownModule()
{
	UE_LOG(LogTemp, Display, TEXT("VibeUE Module has shut down"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FVibeUEModule, VibeUE) 
