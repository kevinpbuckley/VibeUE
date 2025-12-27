// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Core/ToolRegistry.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY(LogToolRegistry);

// Config section and key for disabled tools
static const TCHAR* ConfigSection = TEXT("VibeUE.Tools");
static const TCHAR* DisabledToolsKey = TEXT("DisabledTools");

FToolRegistry& FToolRegistry::Get()
{
	static FToolRegistry Instance;
	return Instance;
}

FToolRegistry::FToolRegistry()
	: bInitialized(false)
{
}

FToolRegistry::~FToolRegistry()
{
	Shutdown();
}

void FToolRegistry::Initialize()
{
	if (bInitialized)
	{
		UE_LOG(LogToolRegistry, Log, TEXT("Tool Registry already initialized with %d tools"), Tools.Num());
		return;
	}

	UE_LOG(LogToolRegistry, Log, TEXT("Initializing Tool Registry..."));
	
	// Load disabled tools from config
	LoadDisabledToolsFromConfig();
	
	// Process any tools that were registered before Initialize() was called
	ProcessPendingRegistrations();
	
	bInitialized = true;
	
	int32 EnabledCount = Tools.Num() - DisabledTools.Num();
	UE_LOG(LogToolRegistry, Log, TEXT("Tool Registry initialized with %d tools (%d enabled, %d disabled)"), 
		Tools.Num(), EnabledCount, DisabledTools.Num());
	
	// Log all registered tools
	for (const FToolMetadata& Tool : Tools)
	{
		bool bEnabled = !DisabledTools.Contains(Tool.Name);
		UE_LOG(LogToolRegistry, Log, TEXT("  Tool: %s (Category: %s) - %d params [%s]"), 
			*Tool.Name, *Tool.Category, Tool.Parameters.Num(),
			bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
	}
}

void FToolRegistry::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	Tools.Empty();
	ToolNameToIndex.Empty();
	ToolExecuteFuncs.Empty();
	bInitialized = false;
	UE_LOG(LogToolRegistry, Log, TEXT("Tool Registry shut down"));
}

void FToolRegistry::RegisterTool(const FToolRegistration& Registration)
{
	// If not initialized yet, queue for later
	if (!bInitialized)
	{
		PendingRegistrations.Add(Registration);
		UE_LOG(LogToolRegistry, Verbose, TEXT("Queued tool for registration: %s"), *Registration.Name);
		return;
	}

	// Check if tool already exists
	if (ToolNameToIndex.Contains(Registration.Name))
	{
		UE_LOG(LogToolRegistry, Warning, TEXT("Tool '%s' already registered, skipping"), *Registration.Name);
		return;
	}

	// Create metadata
	FToolMetadata Metadata;
	Metadata.Name = Registration.Name;
	Metadata.Description = Registration.Description;
	Metadata.Category = Registration.Category;
	Metadata.Parameters = Registration.Parameters;

	// Add tool
	int32 Index = Tools.Add(Metadata);
	ToolNameToIndex.Add(Registration.Name, Index);
	ToolExecuteFuncs.Add(Registration.Name, Registration.ExecuteFunc);

	UE_LOG(LogToolRegistry, Log, TEXT("Registered tool: %s (Category: %s)"), *Registration.Name, *Registration.Category);
}

void FToolRegistry::ProcessPendingRegistrations()
{
	UE_LOG(LogToolRegistry, Log, TEXT("Processing %d pending tool registrations..."), PendingRegistrations.Num());
	
	for (const FToolRegistration& Registration : PendingRegistrations)
	{
		// Check if tool already exists
		if (ToolNameToIndex.Contains(Registration.Name))
		{
			UE_LOG(LogToolRegistry, Warning, TEXT("Tool '%s' already registered, skipping"), *Registration.Name);
			continue;
		}

		// Create metadata
		FToolMetadata Metadata;
		Metadata.Name = Registration.Name;
		Metadata.Description = Registration.Description;
		Metadata.Category = Registration.Category;
		Metadata.Parameters = Registration.Parameters;

		// Add tool
		int32 Index = Tools.Add(Metadata);
		ToolNameToIndex.Add(Registration.Name, Index);
		ToolExecuteFuncs.Add(Registration.Name, Registration.ExecuteFunc);

		UE_LOG(LogToolRegistry, Log, TEXT("Registered tool: %s (Category: %s)"), *Registration.Name, *Registration.Category);
	}
	
	PendingRegistrations.Empty();
}

void FToolRegistry::Refresh()
{
	UE_LOG(LogToolRegistry, Log, TEXT("Refreshing Tool Registry"));
	Tools.Empty();
	ToolNameToIndex.Empty();
	ToolExecuteFuncs.Empty();
	bInitialized = false;
	Initialize();
}

TArray<FToolMetadata> FToolRegistry::GetToolsByCategory(const FString& Category) const
{
	TArray<FToolMetadata> Result;
	for (const FToolMetadata& Tool : Tools)
	{
		if (Tool.Category == Category)
		{
			Result.Add(Tool);
		}
	}
	return Result;
}

const FToolMetadata* FToolRegistry::FindTool(const FString& ToolName) const
{
	const int32* IndexPtr = ToolNameToIndex.Find(ToolName);
	if (IndexPtr && Tools.IsValidIndex(*IndexPtr))
	{
		return &Tools[*IndexPtr];
	}
	return nullptr;
}

bool FToolRegistry::ValidateParameters(
	const FToolMetadata& Tool,
	const TMap<FString, FString>& Parameters,
	FString& OutError)
{
	// Check required parameters
	for (const FToolParameter& Param : Tool.Parameters)
	{
		if (Param.bRequired && !Parameters.Contains(Param.Name))
		{
			OutError = FString::Printf(TEXT("Missing required parameter: %s"), *Param.Name);
			return false;
		}
	}
	return true;
}

FString FToolRegistry::ExecuteTool(
	const FString& ToolName,
	const TMap<FString, FString>& Parameters)
{
	// Check if tool is disabled FIRST
	if (!IsToolEnabled(ToolName))
	{
		UE_LOG(LogToolRegistry, Warning, TEXT("Attempted to execute disabled tool: %s"), *ToolName);
		
		TSharedPtr<FJsonObject> ErrorResult = MakeShareable(new FJsonObject);
		ErrorResult->SetBoolField(TEXT("success"), false);
		ErrorResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Tool '%s' is disabled"), *ToolName));
		ErrorResult->SetStringField(TEXT("error_code"), TEXT("TOOL_DISABLED"));

		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(ErrorResult.ToSharedRef(), Writer);
		return OutputString;
	}
	
	const FToolMetadata* Tool = FindTool(ToolName);
	if (!Tool)
	{
		TSharedPtr<FJsonObject> ErrorResult = MakeShareable(new FJsonObject);
		ErrorResult->SetBoolField(TEXT("success"), false);
		ErrorResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Tool '%s' not found"), *ToolName));

		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(ErrorResult.ToSharedRef(), Writer);
		return OutputString;
	}

	// Validate parameters
	FString ValidationError;
	if (!ValidateParameters(*Tool, Parameters, ValidationError))
	{
		TSharedPtr<FJsonObject> ErrorResult = MakeShareable(new FJsonObject);
		ErrorResult->SetBoolField(TEXT("success"), false);
		ErrorResult->SetStringField(TEXT("error"), ValidationError);

		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(ErrorResult.ToSharedRef(), Writer);
		return OutputString;
	}

	// Execute
	FToolExecuteFunc* ExecuteFunc = ToolExecuteFuncs.Find(ToolName);
	if (ExecuteFunc && *ExecuteFunc)
	{
		UE_LOG(LogToolRegistry, Log, TEXT("Executing tool: %s"), *ToolName);
		return (*ExecuteFunc)(Parameters);
	}

	TSharedPtr<FJsonObject> ErrorResult = MakeShareable(new FJsonObject);
	ErrorResult->SetBoolField(TEXT("success"), false);
	ErrorResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Tool '%s' has no execute function"), *ToolName));

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(ErrorResult.ToSharedRef(), Writer);
	return OutputString;
}

TArray<FToolMetadata> FToolRegistry::GetEnabledTools() const
{
	TArray<FToolMetadata> Result;
	UE_LOG(LogToolRegistry, Log, TEXT("GetEnabledTools: Total tools=%d, Disabled count=%d"), Tools.Num(), DisabledTools.Num());
	
	for (const FToolMetadata& Tool : Tools)
	{
		bool bDisabled = DisabledTools.Contains(Tool.Name);
		if (!bDisabled)
		{
			Result.Add(Tool);
		}
		else
		{
			UE_LOG(LogToolRegistry, Log, TEXT("  SKIPPING disabled tool: %s"), *Tool.Name);
		}
	}
	
	UE_LOG(LogToolRegistry, Log, TEXT("GetEnabledTools: Returning %d enabled tools"), Result.Num());
	return Result;
}

bool FToolRegistry::IsToolEnabled(const FString& ToolName) const
{
	return !DisabledTools.Contains(ToolName);
}

void FToolRegistry::SetToolEnabled(const FString& ToolName, bool bEnabled)
{
	bool bChanged = false;
	
	UE_LOG(LogToolRegistry, Log, TEXT("SetToolEnabled called: Tool='%s', bEnabled=%s, CurrentlyDisabled=%s"), 
		*ToolName, 
		bEnabled ? TEXT("true") : TEXT("false"),
		DisabledTools.Contains(ToolName) ? TEXT("true") : TEXT("false"));
	
	if (bEnabled)
	{
		// Enabling a tool - remove from disabled set
		if (DisabledTools.Contains(ToolName))
		{
			DisabledTools.Remove(ToolName);
			bChanged = true;
			UE_LOG(LogToolRegistry, Log, TEXT("Enabled tool: %s (removed from disabled set)"), *ToolName);
		}
		else
		{
			UE_LOG(LogToolRegistry, Log, TEXT("Tool %s already enabled, no change needed"), *ToolName);
		}
	}
	else
	{
		// Disabling a tool - add to disabled set
		if (!DisabledTools.Contains(ToolName))
		{
			DisabledTools.Add(ToolName);
			bChanged = true;
			UE_LOG(LogToolRegistry, Log, TEXT("Disabled tool: %s (added to disabled set)"), *ToolName);
		}
		else
		{
			UE_LOG(LogToolRegistry, Log, TEXT("Tool %s already disabled, no change needed"), *ToolName);
		}
	}
	
	if (bChanged)
	{
		UE_LOG(LogToolRegistry, Log, TEXT("Tool state changed, saving to config..."));
		SaveDisabledToolsToConfig();
		OnToolEnabledChanged.Broadcast(ToolName, bEnabled);
	}
}

void FToolRegistry::SetDisabledToolsAndSave(const TSet<FString>& NewDisabledTools)
{
	UE_LOG(LogToolRegistry, Log, TEXT("=== SetDisabledToolsAndSave called ==="));
	UE_LOG(LogToolRegistry, Log, TEXT("Old disabled count: %d, New disabled count: %d"), DisabledTools.Num(), NewDisabledTools.Num());
	
	// Log what's changing
	for (const FString& Tool : NewDisabledTools)
	{
		if (!DisabledTools.Contains(Tool))
		{
			UE_LOG(LogToolRegistry, Log, TEXT("  NEWLY DISABLED: %s"), *Tool);
		}
	}
	for (const FString& Tool : DisabledTools)
	{
		if (!NewDisabledTools.Contains(Tool))
		{
			UE_LOG(LogToolRegistry, Log, TEXT("  NEWLY ENABLED: %s"), *Tool);
		}
	}
	
	// Replace the entire set
	DisabledTools = NewDisabledTools;
	
	// Force save regardless of whether anything changed
	SaveDisabledToolsToConfig();
	
	UE_LOG(LogToolRegistry, Log, TEXT("=== SetDisabledToolsAndSave complete ==="));
}

void FToolRegistry::LoadDisabledToolsFromConfig()
{
	DisabledTools.Empty();
	
	// Use INI config file in Saved/Config directory
	FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("VibeUE.ini");
	
	UE_LOG(LogToolRegistry, Log, TEXT("=== LOADING DISABLED TOOLS ==="));
	UE_LOG(LogToolRegistry, Log, TEXT("Config path: %s"), *ConfigPath);
	
	// Read from INI file
	FString DisabledToolsStr;
	if (GConfig->GetString(ConfigSection, DisabledToolsKey, DisabledToolsStr, ConfigPath))
	{
		UE_LOG(LogToolRegistry, Log, TEXT("INI value: [%s]"), *DisabledToolsStr);
		
		// Parse comma-separated list
		TArray<FString> ToolNames;
		DisabledToolsStr.ParseIntoArray(ToolNames, TEXT(","), true);
		
		for (const FString& ToolName : ToolNames)
		{
			FString TrimmedName = ToolName.TrimStartAndEnd();
			if (!TrimmedName.IsEmpty())
			{
				DisabledTools.Add(TrimmedName);
				UE_LOG(LogToolRegistry, Log, TEXT("  Added to DisabledTools: [%s]"), *TrimmedName);
			}
		}
		
		UE_LOG(LogToolRegistry, Log, TEXT("Total disabled tools loaded: %d"), DisabledTools.Num());
	}
	else
	{
		UE_LOG(LogToolRegistry, Log, TEXT("No disabled tools found in config - first run or all tools enabled"));
	}
	UE_LOG(LogToolRegistry, Log, TEXT("=== END LOADING DISABLED TOOLS ==="));
}

void FToolRegistry::SaveDisabledToolsToConfig()
{
	// Use INI config file in Saved/Config directory
	FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("VibeUE.ini");
	
	UE_LOG(LogToolRegistry, Log, TEXT("=== SaveDisabledToolsToConfig ==="));
	UE_LOG(LogToolRegistry, Log, TEXT("Config path: %s"), *ConfigPath);
	
	// Build comma-separated string
	FString DisabledToolsStr;
	for (const FString& ToolName : DisabledTools)
	{
		if (!DisabledToolsStr.IsEmpty())
		{
			DisabledToolsStr += TEXT(",");
		}
		DisabledToolsStr += ToolName;
	}
	
	UE_LOG(LogToolRegistry, Log, TEXT("DisabledTools string: [%s]"), *DisabledToolsStr);
	
	// Write directly to file to ensure it actually saves
	FString FileContent = FString::Printf(TEXT("[%s]\n%s=%s\n"), ConfigSection, DisabledToolsKey, *DisabledToolsStr);
	
	if (FFileHelper::SaveStringToFile(FileContent, *ConfigPath))
	{
		UE_LOG(LogToolRegistry, Log, TEXT("Successfully wrote to file: %s"), *ConfigPath);
	}
	else
	{
		UE_LOG(LogToolRegistry, Error, TEXT("FAILED to write to file: %s"), *ConfigPath);
	}
	
	// Also update GConfig so in-memory state is consistent
	GConfig->SetString(ConfigSection, DisabledToolsKey, *DisabledToolsStr, ConfigPath);
	
	UE_LOG(LogToolRegistry, Log, TEXT("Saved %d disabled tools"), DisabledTools.Num());
}
