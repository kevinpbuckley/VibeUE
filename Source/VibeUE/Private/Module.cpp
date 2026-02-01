// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Module.h"
#include "Modules/ModuleManager.h"
#include "EditorSubsystem.h"
#include "Editor.h"
#include "Chat/AIChatCommands.h"
#include "Core/ToolRegistry.h"
#include "MCP/MCPServer.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "Tools/PythonTools.h"
#include "IPythonScriptPlugin.h"
#include "UI/ChatRichTextStyles.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "FModule"

// Console command to list all registered tools
static void ListVibeUETools()
{
	FToolRegistry& Registry = FToolRegistry::Get();
	if (!Registry.IsInitialized())
	{
		UE_LOG(LogTemp, Warning, TEXT("Tool Registry not initialized"));
		return;
	}

	const TArray<FToolMetadata>& Tools = Registry.GetAllTools();
	UE_LOG(LogTemp, Display, TEXT("=== VibeUE Tool Registry ==="));
	UE_LOG(LogTemp, Display, TEXT("Total tools: %d"), Tools.Num());
	
	for (const FToolMetadata& Tool : Tools)
	{
		UE_LOG(LogTemp, Display, TEXT("  Tool: %s"), *Tool.Name);
		UE_LOG(LogTemp, Display, TEXT("    Category: %s"), *Tool.Category);
		UE_LOG(LogTemp, Display, TEXT("    Description: %s"), *Tool.Description);
		UE_LOG(LogTemp, Display, TEXT("    Parameters: %d"), Tool.Parameters.Num());
		for (const FToolParameter& Param : Tool.Parameters)
		{
			UE_LOG(LogTemp, Display, TEXT("      - %s (%s, %s)"), 
				*Param.Name, 
				*Param.Type, 
				Param.bRequired ? TEXT("required") : TEXT("optional"));
		}
	}
}

// Console command to test a tool
static void TestVibeUETool(const TArray<FString>& Args, FOutputDevice& Ar)
{
	if (Args.Num() < 1)
	{
		Ar.Log(TEXT("Usage: VibeUE.TestTool <ToolName> [ParamName=Value ...]"));
		return;
	}

	FToolRegistry& Registry = FToolRegistry::Get();
	if (!Registry.IsInitialized())
	{
		Ar.Log(TEXT("Tool Registry not initialized"));
		return;
	}

	FString ToolName = Args[0];
	TMap<FString, FString> Parameters;

	// Parse parameters
	for (int32 i = 1; i < Args.Num(); ++i)
	{
		FString Arg = Args[i];
		int32 EqualsIndex;
		if (Arg.FindChar(TEXT('='), EqualsIndex))
		{
			FString ParamName = Arg.Left(EqualsIndex);
			FString ParamValue = Arg.Mid(EqualsIndex + 1);
			Parameters.Add(ParamName, ParamValue);
		}
	}

	Ar.Logf(TEXT("Executing tool: %s"), *ToolName);
	FString Result = Registry.ExecuteTool(ToolName, Parameters);
	Ar.Logf(TEXT("Result: %s"), *Result);
}

static FAutoConsoleCommand ListToolsCommand(
	TEXT("VibeUE.ListTools"),
	TEXT("List all registered VibeUE tools"),
	FConsoleCommandDelegate::CreateStatic(ListVibeUETools)
);

// Console command to refresh tool registry
static void RefreshVibeUETools()
{
	FToolRegistry& Registry = FToolRegistry::Get();
	Registry.Refresh();
	UE_LOG(LogTemp, Display, TEXT("Tool Registry refreshed. Total tools: %d"), Registry.GetAllTools().Num());
}

static FAutoConsoleCommand RefreshToolsCommand(
	TEXT("VibeUE.RefreshTools"),
	TEXT("Refresh the VibeUE tool registry"),
	FConsoleCommandDelegate::CreateStatic(RefreshVibeUETools)
);

static FAutoConsoleCommandWithArgsAndOutputDevice TestToolCommand(
	TEXT("VibeUE.TestTool"),
	TEXT("Test a VibeUE tool: VibeUE.TestTool <ToolName> [ParamName=Value ...]"),
	FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(TestVibeUETool)
);

void FModule::StartupModule()
{
	UE_LOG(LogTemp, Display, TEXT("VibeUE Module has started"));

	// Initialize Chat Rich Text Styles (for markdown rendering)
	FChatRichTextStyles::Initialize();

	// Initialize Tool Registry (reflection-based tools)
	FToolRegistry::Get().Initialize();

	// Initialize AI Chat commands
	FAIChatCommands::Initialize();

	// Initialize MCP Server (auto-starts if enabled in config)
	FMCPServer::Get().Initialize();

	// Register PreExit callback to cleanup Python references before Unreal GC
	FCoreDelegates::OnPreExit.AddRaw(this, &FModule::OnPreExit);
}

void FModule::ShutdownModule()
{
	// Unregister PreExit callback
	FCoreDelegates::OnPreExit.RemoveAll(this);

	// Shutdown MCP Server
	FMCPServer::Get().Shutdown();

	// Shutdown AI Chat commands
	FAIChatCommands::Shutdown();

	// Shutdown Tool Registry
	FToolRegistry::Get().Shutdown();

	// Shutdown Chat Rich Text Styles
	FChatRichTextStyles::Shutdown();

	UE_LOG(LogTemp, Display, TEXT("VibeUE Module has shut down"));
}

void FModule::OnPreExit()
{
	UE_LOG(LogTemp, Display, TEXT("VibeUE OnPreExit - cleaning up Python services"));
	
	// Release all C++ Python service instances
	// This is safe because we're just clearing our own pointers
	UPythonTools::Shutdown();
	
	// NOTE: We deliberately do NOT call into Python here.
	// During OnPreExit, the Python interpreter may already be partially shut down
	// or in an inconsistent state. Calling ExecPythonCommand can cause access
	// violations (reading address 0x28 = null pointer + offset).
	// The C++ cleanup above is sufficient - Python's own shutdown will handle
	// the rest of the garbage collection.
	
	UE_LOG(LogTemp, Display, TEXT("VibeUE OnPreExit - C++ cleanup complete"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FModule, VibeUE) 
