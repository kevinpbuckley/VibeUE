// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Module.h"
#include "Modules/ModuleManager.h"
#include "EditorSubsystem.h"
#include "Editor.h"
#include "Core/ToolRegistry.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Tools/PythonTools.h"
#include "IPythonScriptPlugin.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "Core/VibeUEMCPToolBridge.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Utils/VibeUEPaths.h"
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

// Console command to write the VibeUE agent guide to the project root from the bundled
// sample — mirrors Epic's ModelContextProtocol.GenerateClientConfig.
//
// Resolves the plugin location via FVibeUEPaths so it works whether VibeUE was installed
// from FAB (Engine/Plugins/Marketplace) or Git (Project/Plugins) — the install-dependent
// path is the whole reason this command exists.
//
// Each agent expects a different memory file, and only Claude Code (CLAUDE.md) and Gemini
// CLI (GEMINI.md) resolve `@path` imports — Codex (AGENTS.md), Cursor (AGENTS.md) and
// Copilot do not. So the default COPIES the guide in (universal); pass "import" to instead
// write a one-line `@<resolved sample path>` for the two agents that support it (others
// fall back to copy). Copilot also reads AGENTS.md/CLAUDE.md/GEMINI.md, so "All" covers it.
static void GenerateVibeUEAgentConfig(const TArray<FString>& Args, FOutputDevice& Ar)
{
	const FString Client = (Args.Num() > 0) ? Args[0].ToLower() : TEXT("all");

	bool bImportRequested = false;
	for (int32 i = 1; i < Args.Num(); ++i)
	{
		const FString A = Args[i].ToLower();
		if (A == TEXT("import") || A == TEXT("link") || A == TEXT("-import") || A == TEXT("--import"))
		{
			bImportRequested = true;
		}
	}

	const FString ContentDir = FVibeUEPaths::GetPluginContentDir();
	if (ContentDir.IsEmpty())
	{
		Ar.Log(TEXT("VibeUE.GenerateAgentConfig: ERROR — could not locate the VibeUE plugin."));
		return;
	}

	// Absolute path — used both to load the sample and (in import mode) as the @import
	// target. Must be absolute: with a FAB install the plugin lives outside the project,
	// so a relative path from the project root would be long and CWD-fragile.
	const FString SamplePath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(ContentDir, TEXT("samples"), TEXT("AGENTS.md.sample")));
	FString SampleContent;
	if (!FFileHelper::LoadFileToString(SampleContent, *SamplePath))
	{
		Ar.Logf(TEXT("VibeUE.GenerateAgentConfig: ERROR — could not read sample at %s"), *SamplePath);
		return;
	}

	// Target file per agent, and whether that agent resolves @-imports.
	TArray<TPair<FString, bool>> Targets; // (filename relative to project root, bSupportsImport)
	if (Client == TEXT("claude") || Client == TEXT("claudecode"))
	{
		Targets.Add(TPair<FString, bool>(TEXT("CLAUDE.md"), true));
	}
	else if (Client == TEXT("gemini"))
	{
		Targets.Add(TPair<FString, bool>(TEXT("GEMINI.md"), true));
	}
	else if (Client == TEXT("copilot"))
	{
		Targets.Add(TPair<FString, bool>(TEXT(".github/copilot-instructions.md"), false));
	}
	else if (Client == TEXT("codex") || Client == TEXT("cursor") || Client == TEXT("agents") || Client == TEXT("agent"))
	{
		Targets.Add(TPair<FString, bool>(TEXT("AGENTS.md"), false));
	}
	else if (Client == TEXT("all"))
	{
		Targets.Add(TPair<FString, bool>(TEXT("CLAUDE.md"), true));   // Claude Code
		Targets.Add(TPair<FString, bool>(TEXT("GEMINI.md"), true));   // Gemini CLI
		Targets.Add(TPair<FString, bool>(TEXT("AGENTS.md"), false));  // Codex, Cursor (and Copilot reads it too)
	}
	else
	{
		Ar.Logf(TEXT("VibeUE.GenerateAgentConfig: unknown client '%s'. Use: ClaudeCode | Gemini | Codex | Cursor | Copilot | All."), *Client);
		return;
	}

	// VibeUE-managed block. The markers make re-runs idempotent and let the command refresh
	// only its own section without disturbing the user's own content in the file.
	const FString Version = FVibeUEPaths::GetPluginVersionName();
	const FString BeginMarker = FString::Printf(
		TEXT("<!-- BEGIN VibeUE (v%s) — generated by VibeUE.GenerateAgentConfig; re-run to refresh -->"), *Version);
	const FString EndMarker = TEXT("<!-- END VibeUE -->");

	const FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	IFileManager& FileManager = IFileManager::Get();

	for (const TPair<FString, bool>& Target : Targets)
	{
		const FString& FileName = Target.Key;
		const bool bSupportsImport = Target.Value;
		const bool bUseImport = bImportRequested && bSupportsImport;
		if (bImportRequested && !bSupportsImport)
		{
			Ar.Logf(TEXT("VibeUE.GenerateAgentConfig: %s does not resolve @-imports — copying the guide in instead."), *FileName);
		}

		FString Body;
		if (bUseImport)
		{
			Body = FString::Printf(TEXT("@%s\n"), *SamplePath);
		}
		else
		{
			Body = SampleContent;
			if (!Body.EndsWith(TEXT("\n")))
			{
				Body += TEXT("\n");
			}
		}
		const FString Block = BeginMarker + TEXT("\n") + Body + EndMarker + TEXT("\n");

		const FString FullPath = FPaths::Combine(ProjectRoot, FileName);

		FString NewContent;
		FString Existing;
		if (FFileHelper::LoadFileToString(Existing, *FullPath))
		{
			const int32 BeginIdx = Existing.Find(TEXT("<!-- BEGIN VibeUE"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
			if (BeginIdx != INDEX_NONE)
			{
				int32 EndIdx = Existing.Find(*EndMarker, ESearchCase::IgnoreCase, ESearchDir::FromStart, BeginIdx);
				if (EndIdx != INDEX_NONE)
				{
					EndIdx += EndMarker.Len();
					// Swallow one trailing newline so repeated runs don't accrete blank lines.
					if (EndIdx < Existing.Len() && Existing[EndIdx] == TEXT('\n'))
					{
						++EndIdx;
					}
					NewContent = Existing.Left(BeginIdx) + Block + Existing.RightChop(EndIdx);
				}
				else
				{
					// Begin marker but no end marker (hand-edited / truncated) — replace to EOF.
					NewContent = Existing.Left(BeginIdx) + Block;
				}
				Ar.Logf(TEXT("VibeUE.GenerateAgentConfig: refreshed VibeUE block in %s"), *FullPath);
			}
			else
			{
				NewContent = Existing;
				if (!NewContent.EndsWith(TEXT("\n")))
				{
					NewContent += TEXT("\n");
				}
				NewContent += TEXT("\n") + Block;
				Ar.Logf(TEXT("VibeUE.GenerateAgentConfig: appended VibeUE block to existing %s"), *FullPath);
			}
		}
		else
		{
			NewContent = Block;
			Ar.Logf(TEXT("VibeUE.GenerateAgentConfig: created %s (%s)"), *FullPath, bUseImport ? TEXT("import") : TEXT("copy"));
		}

		// Ensure the parent directory exists (e.g. .github/ for Copilot).
		FileManager.MakeDirectory(*FPaths::GetPath(FullPath), /*Tree=*/true);

		// Force UTF-8 (no BOM) — markdown agent files must be UTF-8; the default
		// AutoDetect writes UTF-16 when the guide contains non-ASCII (em dashes, etc.).
		if (!FFileHelper::SaveStringToFile(NewContent, *FullPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Ar.Logf(TEXT("VibeUE.GenerateAgentConfig: ERROR — failed to write %s"), *FullPath);
		}
	}

	Ar.Logf(TEXT("VibeUE.GenerateAgentConfig: done (source: %s). Tip: also run 'ModelContextProtocol.GenerateClientConfig %s' to write .mcp.json."),
		*SamplePath, (Args.Num() > 0) ? *Args[0] : TEXT("All"));
}

static FAutoConsoleCommandWithArgsAndOutputDevice GenerateAgentConfigCommand(
	TEXT("VibeUE.GenerateAgentConfig"),
	TEXT("Write the VibeUE agent guide to the project root from the bundled sample. ")
	TEXT("Usage: VibeUE.GenerateAgentConfig [ClaudeCode|Gemini|Codex|Cursor|Copilot|All] [import]. ")
	TEXT("Default All -> CLAUDE.md + GEMINI.md + AGENTS.md. 'import' writes a one-line @import for Claude/Gemini (others copy)."),
	FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(GenerateVibeUEAgentConfig)
);

void FModule::StartupModule()
{
	UE_LOG(LogTemp, Display, TEXT("VibeUE Module has started"));

	// Skip all interactive services when running as a commandlet (e.g. -run=Cook).
	// The MCP HTTP server and Python services keep threads alive that prevent
	// UnrealEditor-Cmd.exe from exiting cleanly, causing UAT cook timeouts.
	if (IsRunningCommandlet())
	{
		return;
	}

	bServicesInitialized = true;

	// Clear screenshots directory from previous sessions to save disk space
	FVibeUEPaths::ClearScreenshotsDir();

	// Initialize Tool Registry (reflection-based tools)
	FToolRegistry::Get().Initialize();

	// Register PreExit callback to cleanup Python references before Unreal GC
	FCoreDelegates::OnPreExit.AddRaw(this, &FModule::OnPreExit);

	// Expose VibeUE service toolsets on UE 5.8's native ToolsetRegistry / MCP endpoint.
	// The registry needs GEditor; register now if it's already up (late/hot-reload load),
	// otherwise defer to PostEngineInit. Branch on GEditor directly to avoid the registry's
	// "Editor is not available" warning during normal startup.
	if (GEditor)
	{
		RegisterToolsets();
	}
	else
	{
		FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FModule::RegisterToolsets);
	}
}

// Collect every non-abstract UToolsetDefinition subclass defined in this module (/Script/VibeUE)
// that exposes at least one AICallable tool. Reflection-based so new services are picked up
// automatically without editing this file.
static void GatherVibeUEToolsetClasses(TArray<UClass*>& OutClasses)
{
	TArray<UClass*> Derived;
	GetDerivedClasses(UToolsetDefinition::StaticClass(), Derived, /*bRecursive*/ true);

	const UPackage* VibeUEPackage = FindPackage(nullptr, TEXT("/Script/VibeUE"));
	for (UClass* Class : Derived)
	{
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract) || Class->GetOutermost() != VibeUEPackage)
		{
			continue;
		}

		// Skip toolsets with no AICallable functions (e.g. instance-only services) — they'd
		// register as an empty toolset.
		bool bHasAICallable = false;
		for (TFieldIterator<UFunction> It(Class); It && !bHasAICallable; ++It)
		{
			const TValueOrError<bool, FString> Result = UToolsetDefinition::IsFunctionAICallable(*It);
			bHasAICallable = Result.HasValue() && Result.GetValue();
		}
		if (bHasAICallable)
		{
			OutClasses.Add(Class);
		}
	}
}

void FModule::RegisterToolsets()
{
	// Service layer -> Epic's ToolsetRegistry (AICallable tools).
	if (UToolsetRegistry::IsAvailable())
	{
		TArray<UClass*> ToolsetClasses;
		GatherVibeUEToolsetClasses(ToolsetClasses);
		for (UClass* Class : ToolsetClasses)
		{
			UToolsetRegistry::RegisterToolsetClass(Class);
		}
		UE_LOG(LogTemp, Display, TEXT("VibeUE: registered %d service toolset(s) with ToolsetRegistry."), ToolsetClasses.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("VibeUE: ToolsetRegistry not available; service toolsets not registered."));
	}

	// Dynamic FToolRegistry tools -> Epic's MCP endpoint (independent of ToolsetRegistry).
	VibeUEMCPToolBridge::RegisterAll();
}

void FModule::UnregisterToolsets()
{
	VibeUEMCPToolBridge::UnregisterAll();

	if (UToolsetRegistry::IsAvailable())
	{
		TArray<UClass*> ToolsetClasses;
		GatherVibeUEToolsetClasses(ToolsetClasses);
		for (UClass* Class : ToolsetClasses)
		{
			UToolsetRegistry::UnregisterToolsetClass(Class);
		}
	}
}

void FModule::ShutdownModule()
{
	if (!bServicesInitialized)
	{
		UE_LOG(LogTemp, Display, TEXT("VibeUE Module has shut down"));
		return;
	}

	// Unregister PreExit / PostEngineInit callbacks
	FCoreDelegates::OnPreExit.RemoveAll(this);
	FCoreDelegates::GetOnPostEngineInit().RemoveAll(this);

	// Unregister service toolsets + MCP tools
	UnregisterToolsets();

	// Shutdown Tool Registry
	FToolRegistry::Get().Shutdown();

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
