// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Module.h"
#include "Bridge.h"
#include "Modules/ModuleManager.h"
#include "EditorSubsystem.h"
#include "Editor.h"
#include "Chat/AIChatCommands.h"

#define LOCTEXT_NAMESPACE "FModule"

void FModule::StartupModule()
{
	UE_LOG(LogTemp, Display, TEXT("VibeUE Module has started"));
	
	// Initialize AI Chat commands
	FAIChatCommands::Initialize();
}

void FModule::ShutdownModule()
{
	// Shutdown AI Chat commands
	FAIChatCommands::Shutdown();
	
	UE_LOG(LogTemp, Display, TEXT("VibeUE Module has shut down"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FModule, VibeUE) 
