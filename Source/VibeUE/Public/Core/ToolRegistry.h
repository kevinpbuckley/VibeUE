// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ToolMetadata.h"
#include "UObject/NoExportTypes.h"

class UFunction;
class UClass;
class FProperty;

DECLARE_LOG_CATEGORY_EXTERN(LogToolRegistry, Log, All);

/** Function type for tool execution */
using FToolExecuteFunc = TFunction<FString(const TMap<FString, FString>&)>;

/**
 * Tool registration info - used for auto-registration
 */
struct VIBEUE_API FToolRegistration
{
	FString Name;
	FString Description;
	FString Category;
	TArray<FToolParameter> Parameters;
	FToolExecuteFunc ExecuteFunc;
};

/**
 * Registry of all available AI tools
 * Supports automatic registration via REGISTER_VIBEUE_TOOL macro
 */
class VIBEUE_API FToolRegistry
{
public:
	/** Get singleton instance */
	static FToolRegistry& Get();

	/** Initialize - processes all pending registrations */
	void Initialize();

	/** Shutdown and cleanup */
	void Shutdown();

	/** Get all available tools */
	const TArray<FToolMetadata>& GetAllTools() const { return Tools; }

	/** Get only enabled tools (for chat/AI use) */
	TArray<FToolMetadata> GetEnabledTools() const;

	/** Get tools by category */
	TArray<FToolMetadata> GetToolsByCategory(const FString& Category) const;

	/** Find tool by name */
	const FToolMetadata* FindTool(const FString& ToolName) const;

	/**
	 * Register a tool (called automatically by REGISTER_VIBEUE_TOOL macro)
	 * Can be called before Initialize() - registrations are queued
	 */
	void RegisterTool(const FToolRegistration& Registration);

	/** Check if a tool is enabled */
	bool IsToolEnabled(const FString& ToolName) const;

	/** Enable or disable a tool */
	void SetToolEnabled(const FString& ToolName, bool bEnabled);

	/** Set all disabled tools at once and save to config (bypasses change detection) */
	void SetDisabledToolsAndSave(const TSet<FString>& NewDisabledTools);

	/** Get the set of disabled tools */
	const TSet<FString>& GetDisabledTools() const { return DisabledTools; }

	/** Delegate for tool enabled state changes */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnToolEnabledChanged, const FString& /*ToolName*/, bool /*bEnabled*/);
	FOnToolEnabledChanged OnToolEnabledChanged;

	/**
	 * Execute a tool call
	 * @param ToolName Name of the tool to execute
	 * @param Parameters Map of parameter names to JSON string values
	 * @return JSON string result
	 */
	FString ExecuteTool(
		const FString& ToolName,
		const TMap<FString, FString>& Parameters
	);

	/** Refresh tool registry (re-process registrations) */
	void Refresh();

	/** Check if registry is initialized */
	bool IsInitialized() const { return bInitialized; }

private:
	FToolRegistry();
	~FToolRegistry();

	/** Process all pending registrations */
	void ProcessPendingRegistrations();

	/** Validate parameters before execution */
	bool ValidateParameters(
		const FToolMetadata& Tool,
		const TMap<FString, FString>& Parameters,
		FString& OutError
	);

	/** Load disabled tools from config */
	void LoadDisabledToolsFromConfig();

	/** Save disabled tools to config */
	void SaveDisabledToolsToConfig();

	TArray<FToolMetadata> Tools;
	TMap<FString, int32> ToolNameToIndex;
	TMap<FString, FToolExecuteFunc> ToolExecuteFuncs;
	TArray<FToolRegistration> PendingRegistrations;
	TSet<FString> DisabledTools;
	bool bInitialized = false;
};

//=============================================================================
// AUTO-REGISTRATION MACROS
// Use these to register tools - they will be discovered automatically!
//=============================================================================

/**
 * Helper class for auto-registration
 */
struct VIBEUE_API FToolAutoRegistrar
{
	FToolAutoRegistrar(const FToolRegistration& Registration)
	{
		FToolRegistry::Get().RegisterTool(Registration);
	}
};

/**
 * Register a tool with no parameters
 * Usage: REGISTER_VIBEUE_TOOL(tool_name, "Description", "Category", []() { return MyFunc(); })
 */
#define REGISTER_VIBEUE_TOOL_SIMPLE(ToolName, Description, Category, Lambda) \
	static FToolAutoRegistrar AutoRegister_##ToolName( \
		FToolRegistration{ \
			TEXT(#ToolName), \
			TEXT(Description), \
			TEXT(Category), \
			{}, \
			[](const TMap<FString, FString>& Params) -> FString { return Lambda(); } \
		} \
	);

/**
 * Register a tool with custom parameters and execution
 * Usage in .cpp file:
 * 
 * REGISTER_VIBEUE_TOOL(manage_level_actors,
 *     "Manage level actors - spawn, transform, query actors",
 *     "Level",
 *     TOOL_PARAMS(
 *         TOOL_PARAM("Action", "The action to perform", "string", true),
 *         TOOL_PARAM("ParamsJson", "JSON parameters for the action", "string", false)
 *     ),
 *     {
 *         FString Action = Params.FindRef(TEXT("Action"));
 *         FString ParamsJson = Params.FindRef(TEXT("ParamsJson"));
 *         return UEditorTools::ManageLevelActors(Action, ParamsJson);
 *     }
 * );
 */
#define REGISTER_VIBEUE_TOOL(ToolName, Description, Category, ParamList, ExecuteBody) \
	static FToolAutoRegistrar AutoRegister_##ToolName( \
		FToolRegistration{ \
			TEXT(#ToolName), \
			TEXT(Description), \
			TEXT(Category), \
			ParamList, \
			[](const TMap<FString, FString>& Params) -> FString ExecuteBody \
		} \
	);

/** Helper to define parameter list */
#define TOOL_PARAMS(...) TArray<FToolParameter>({ __VA_ARGS__ })

/** Helper to define a single parameter */
#define TOOL_PARAM(Name, Desc, Type, Required) \
	FToolParameter(TEXT(Name), TEXT(Desc), TEXT(Type), Required)

/** Helper to define an optional parameter with default */
#define TOOL_PARAM_DEFAULT(Name, Desc, Type, Default) \
	FToolParameter(TEXT(Name), TEXT(Desc), TEXT(Type), false, TEXT(Default))
