// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Module.h"
#include "Bridge.h"
#include "Modules/ModuleManager.h"
#include "EditorSubsystem.h"
#include "Editor.h"
#include "Chat/AIChatCommands.h"
#include "Core/ToolRegistry.h"
#include "MCP/MCPServer.h"
#include "HAL/IConsoleManager.h"
#include "Tools/ExampleTools.h"

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

// Console command to test metadata directly
static void TestMetadata()
{
	UClass* ExampleToolsClass = UExampleTools::StaticClass();
	if (!ExampleToolsClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UExampleTools::StaticClass() returned nullptr!"));
		return;
	}
	
	UE_LOG(LogTemp, Display, TEXT("=== Testing UExampleTools Metadata ==="));
	UE_LOG(LogTemp, Display, TEXT("Class: %s"), *ExampleToolsClass->GetName());
	
	FString Category = ExampleToolsClass->GetMetaData(TEXT("ToolCategory"));
	UE_LOG(LogTemp, Display, TEXT("ToolCategory metadata: '%s' (empty=%d)"), *Category, Category.IsEmpty());
	
	UE_LOG(LogTemp, Display, TEXT("Functions in class:"));
	int32 FuncCount = 0;
	for (TFieldIterator<UFunction> FuncIt(ExampleToolsClass); FuncIt; ++FuncIt)
	{
		UFunction* Func = *FuncIt;
		FuncCount++;
		FString ToolName = Func->GetMetaData(TEXT("ToolName"));
		UE_LOG(LogTemp, Display, TEXT("  %s: ToolName='%s'"), *Func->GetName(), *ToolName);
	}
	UE_LOG(LogTemp, Display, TEXT("Total functions: %d"), FuncCount);
}

static FAutoConsoleCommand TestMetadataCommand(
	TEXT("VibeUE.TestMetadata"),
	TEXT("Test metadata extraction from UExampleTools"),
	FConsoleCommandDelegate::CreateStatic(TestMetadata)
);

static FAutoConsoleCommandWithArgsAndOutputDevice TestToolCommand(
	TEXT("VibeUE.TestTool"),
	TEXT("Test a VibeUE tool: VibeUE.TestTool <ToolName> [ParamName=Value ...]"),
	FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(TestVibeUETool)
);

void FModule::StartupModule()
{
	UE_LOG(LogTemp, Display, TEXT("VibeUE Module has started"));
	
	// Initialize Tool Registry (reflection-based tools)
	FToolRegistry::Get().Initialize();
	
	// Initialize AI Chat commands
	FAIChatCommands::Initialize();
	
	// Initialize MCP Server (auto-starts if enabled in config)
	FMCPServer::Get().Initialize();
}

void FModule::ShutdownModule()
{
	// Shutdown MCP Server
	FMCPServer::Get().Shutdown();
	
	// Shutdown AI Chat commands
	FAIChatCommands::Shutdown();
	
	// Shutdown Tool Registry
	FToolRegistry::Get().Shutdown();
	
	UE_LOG(LogTemp, Display, TEXT("VibeUE Module has shut down"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FModule, VibeUE) 
