// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Tools/EditorTools.h"
#include "Core/ToolRegistry.h"
#include "Commands/LevelActorCommands.h"
#include "Commands/BlueprintCommands.h"
#include "Commands/BlueprintNodeCommands.h"
#include "Commands/BlueprintComponentReflection.h"
#include "Commands/UMGCommands.h"
#include "Commands/MaterialCommands.h"
#include "Commands/MaterialNodeCommands.h"
#include "Commands/AssetCommands.h"
#include "Commands/EnhancedInputCommands.h"
#include "Core/ServiceContext.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Misc/EngineVersion.h"

//=============================================================================
// COMMAND HANDLER INSTANCES (lazy initialized)
//=============================================================================
static TSharedPtr<FLevelActorCommands> LevelActorCommandsInstance;
static TSharedPtr<FBlueprintCommands> BlueprintCommandsInstance;
static TSharedPtr<FBlueprintNodeCommands> BlueprintNodeCommandsInstance;
static TSharedPtr<FBlueprintComponentReflection> BlueprintComponentInstance;
static TSharedPtr<FUMGCommands> UMGCommandsInstance;
static TSharedPtr<FMaterialCommands> MaterialCommandsInstance;
static TSharedPtr<FMaterialNodeCommands> MaterialNodeCommandsInstance;
static TSharedPtr<FAssetCommands> AssetCommandsInstance;
static TSharedPtr<FEnhancedInputCommands> EnhancedInputCommandsInstance;
static TSharedPtr<FServiceContext> SharedServiceContext;

static void EnsureCommandHandlersInitialized()
{
	if (!SharedServiceContext.IsValid())
	{
		SharedServiceContext = MakeShared<FServiceContext>();
	}
	if (!LevelActorCommandsInstance.IsValid())
	{
		LevelActorCommandsInstance = MakeShared<FLevelActorCommands>();
	}
	if (!BlueprintCommandsInstance.IsValid())
	{
		BlueprintCommandsInstance = MakeShared<FBlueprintCommands>();
	}
	if (!BlueprintNodeCommandsInstance.IsValid())
	{
		BlueprintNodeCommandsInstance = MakeShared<FBlueprintNodeCommands>();
	}
	if (!BlueprintComponentInstance.IsValid())
	{
		BlueprintComponentInstance = MakeShared<FBlueprintComponentReflection>();
	}
	if (!UMGCommandsInstance.IsValid())
	{
		UMGCommandsInstance = MakeShared<FUMGCommands>(SharedServiceContext);
	}
	if (!MaterialCommandsInstance.IsValid())
	{
		MaterialCommandsInstance = MakeShared<FMaterialCommands>();
	}
	if (!MaterialNodeCommandsInstance.IsValid())
	{
		MaterialNodeCommandsInstance = MakeShared<FMaterialNodeCommands>();
	}
	if (!AssetCommandsInstance.IsValid())
	{
		AssetCommandsInstance = MakeShared<FAssetCommands>();
	}
	if (!EnhancedInputCommandsInstance.IsValid())
	{
		EnhancedInputCommandsInstance = MakeShared<FEnhancedInputCommands>();
	}
}

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================
TSharedPtr<FJsonObject> UEditorTools::ParseParams(const FString& ParamsJson)
{
	TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject);
	if (!ParamsJson.IsEmpty())
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ParamsJson);
		if (!FJsonSerializer::Deserialize(Reader, Params))
		{
			Params = MakeShareable(new FJsonObject);
		}
	}
	return Params;
}

FString UEditorTools::SerializeResult(const TSharedPtr<FJsonObject>& Result)
{
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
	return OutputString;
}

//=============================================================================
// TOOL IMPLEMENTATIONS
//=============================================================================

FString UEditorTools::CheckUnrealConnection()
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("connected"), true);
	Result->SetStringField(TEXT("unreal_version"), ENGINE_VERSION_STRING);
	Result->SetStringField(TEXT("plugin_version"), TEXT("2.0.0"));
	Result->SetStringField(TEXT("server_status"), TEXT("running"));
	Result->SetBoolField(TEXT("editor_connected"), GEditor != nullptr);
	return SerializeResult(Result);
}

FString UEditorTools::DeepResearcher(const FString& Query, const FString& SearchScope, const FString& OptionsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(OptionsJson);
	Params->SetStringField(TEXT("query"), Query);
	Params->SetStringField(TEXT("search_type"), SearchScope.IsEmpty() ? TEXT("all") : SearchScope);
	TSharedPtr<FJsonObject> Result = UMGCommandsInstance->HandleCommand(TEXT("search_items"), Params);
	return SerializeResult(Result);
}

FString UEditorTools::ManageAsset(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	
	if (Action.ToLower() == TEXT("search"))
	{
		return SerializeResult(UMGCommandsInstance->HandleCommand(TEXT("search_items"), Params));
	}
	
	FString CommandType;
	if (Action == TEXT("import_texture")) CommandType = TEXT("import_texture_asset");
	else if (Action == TEXT("export_texture")) CommandType = TEXT("export_texture_for_analysis");
	else if (Action == TEXT("delete")) CommandType = TEXT("delete_asset");
	else if (Action == TEXT("duplicate")) CommandType = TEXT("duplicate_asset");
	else if (Action == TEXT("save")) CommandType = TEXT("save_asset");
	else if (Action == TEXT("save_all")) CommandType = TEXT("save_all_assets");
	else if (Action == TEXT("list_references")) CommandType = TEXT("list_references");
	else if (Action == TEXT("open")) CommandType = TEXT("OpenAssetInEditor");
	else
	{
		TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject);
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown action: %s"), *Action));
		return SerializeResult(Err);
	}
	
	return SerializeResult(AssetCommandsInstance->HandleCommand(CommandType, Params));
}

FString UEditorTools::ManageBlueprint(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	
	FString CommandType;
	if (Action == TEXT("create")) CommandType = TEXT("create_blueprint");
	else if (Action == TEXT("get_info")) CommandType = TEXT("get_blueprint_info");
	else if (Action == TEXT("compile")) CommandType = TEXT("compile_blueprint");
	else if (Action == TEXT("reparent")) CommandType = TEXT("reparent_blueprint");
	else if (Action == TEXT("set_property")) CommandType = TEXT("set_blueprint_property");
	else if (Action == TEXT("get_property")) CommandType = TEXT("get_blueprint_property");
	else
	{
		TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject);
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown action: %s"), *Action));
		return SerializeResult(Err);
	}
	
	return SerializeResult(BlueprintCommandsInstance->HandleCommand(CommandType, Params));
}

FString UEditorTools::ManageBlueprintComponent(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	
	FString CommandType;
	if (Action == TEXT("add")) CommandType = TEXT("add_component");
	else if (Action == TEXT("remove")) CommandType = TEXT("remove_component");
	else if (Action == TEXT("get_hierarchy")) CommandType = TEXT("get_component_hierarchy");
	else if (Action == TEXT("set_property")) CommandType = TEXT("set_component_property");
	else if (Action == TEXT("get_property")) CommandType = TEXT("get_component_property");
	else if (Action == TEXT("get_all_properties")) CommandType = TEXT("get_all_component_properties");
	else if (Action == TEXT("reparent")) CommandType = TEXT("reparent_component");
	else if (Action == TEXT("get_available")) CommandType = TEXT("get_available_components");
	else
	{
		TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject);
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown action: %s"), *Action));
		return SerializeResult(Err);
	}
	
	return SerializeResult(BlueprintComponentInstance->HandleCommand(CommandType, Params));
}

FString UEditorTools::ManageBlueprintFunction(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(BlueprintNodeCommandsInstance->HandleCommand(TEXT("manage_blueprint_function"), Params));
}

FString UEditorTools::ManageBlueprintNode(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), Action);
	
	if (Action.ToLower() == TEXT("discover"))
	{
		return SerializeResult(BlueprintNodeCommandsInstance->HandleCommand(TEXT("discover_nodes_with_descriptors"), Params));
	}
	
	return SerializeResult(BlueprintNodeCommandsInstance->HandleCommand(TEXT("manage_blueprint_node"), Params));
}

FString UEditorTools::ManageBlueprintVariable(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	
	FString CommandType;
	if (Action == TEXT("add")) CommandType = TEXT("add_blueprint_variable");
	else if (Action == TEXT("delete")) CommandType = TEXT("delete_blueprint_variable");
	else if (Action == TEXT("get")) CommandType = TEXT("get_blueprint_variable");
	else if (Action == TEXT("get_property")) CommandType = TEXT("get_variable_property");
	else if (Action == TEXT("set_property")) CommandType = TEXT("set_variable_property");
	else if (Action == TEXT("list")) CommandType = TEXT("get_blueprint_info");
	else if (Action == TEXT("get_available_types")) CommandType = TEXT("get_available_blueprint_variable_types");
	else
	{
		TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject);
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown action: %s"), *Action));
		return SerializeResult(Err);
	}
	
	return SerializeResult(BlueprintCommandsInstance->HandleCommand(CommandType, Params));
}

FString UEditorTools::ManageEnhancedInput(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(EnhancedInputCommandsInstance->HandleCommand(TEXT("manage_enhanced_input"), Params));
}

FString UEditorTools::ManageLevelActors(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(LevelActorCommandsInstance->HandleCommand(TEXT("manage_level_actors"), Params));
}

FString UEditorTools::ManageMaterial(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(MaterialCommandsInstance->HandleCommand(TEXT("manage_material"), Params));
}

FString UEditorTools::ManageMaterialNode(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(MaterialNodeCommandsInstance->HandleCommand(TEXT("manage_material_node"), Params));
}

FString UEditorTools::ManageUMGWidget(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	
	FString CommandType;
	if (Action == TEXT("create")) CommandType = TEXT("create_umg_widget_blueprint");
	else if (Action == TEXT("delete")) CommandType = TEXT("delete_widget_blueprint");
	else if (Action == TEXT("get_info")) CommandType = TEXT("get_widget_blueprint_info");
	else if (Action == TEXT("add_child")) CommandType = TEXT("add_child_to_panel");
	else if (Action == TEXT("remove_child")) CommandType = TEXT("remove_umg_component");
	else if (Action == TEXT("set_property")) CommandType = TEXT("set_widget_property");
	else if (Action == TEXT("get_property")) CommandType = TEXT("get_widget_property");
	else if (Action == TEXT("list_components")) CommandType = TEXT("list_widget_components");
	else if (Action == TEXT("get_available_types")) CommandType = TEXT("get_available_widget_types");
	else if (Action == TEXT("add_widget")) CommandType = TEXT("add_widget_component");
	else
	{
		TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject);
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown action: %s"), *Action));
		return SerializeResult(Err);
	}
	
	return SerializeResult(UMGCommandsInstance->HandleCommand(CommandType, Params));
}

//=============================================================================
// AUTO-REGISTRATION
// Tools register themselves when this file is loaded!
//=============================================================================

// 1. check_unreal_connection
REGISTER_VIBEUE_TOOL(check_unreal_connection,
	"Check Unreal Engine connection status and get system information",
	"System",
	TOOL_PARAMS(),
	{
		return UEditorTools::CheckUnrealConnection();
	}
);

// 2. deep_researcher
REGISTER_VIBEUE_TOOL(deep_researcher,
	"Deep researcher - comprehensive search and analysis across assets, actors, blueprints, and classes",
	"Discovery",
	TOOL_PARAMS(
		TOOL_PARAM("Query", "Search query string", "string", true),
		TOOL_PARAM_DEFAULT("SearchScope", "Scope: all, assets, actors, blueprints, classes", "string", "all"),
		TOOL_PARAM_DEFAULT("OptionsJson", "Additional search options as JSON", "string", "{}")
	),
	{
		return UEditorTools::DeepResearcher(
			Params.FindRef(TEXT("Query")),
			Params.FindRef(TEXT("SearchScope")),
			Params.FindRef(TEXT("OptionsJson"))
		);
	}
);

// 3. manage_asset
REGISTER_VIBEUE_TOOL(manage_asset,
	"Manage assets - search, import, export, save, delete assets. Actions: search, import_texture, export_texture, delete, duplicate, save, save_all, list_references, open",
	"Asset",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageAsset(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 4. manage_blueprint
REGISTER_VIBEUE_TOOL(manage_blueprint,
	"Manage blueprints - create, compile, reparent, get/set properties. Actions: create, get_info, compile, reparent, set_property, get_property",
	"Blueprint",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageBlueprint(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 5. manage_blueprint_component
REGISTER_VIBEUE_TOOL(manage_blueprint_component,
	"Manage blueprint components - add, configure, organize components. Actions: add, remove, get_hierarchy, set_property, get_property, get_all_properties, reparent, get_available",
	"Blueprint",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageBlueprintComponent(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 6. manage_blueprint_function
REGISTER_VIBEUE_TOOL(manage_blueprint_function,
	"Manage blueprint functions - create, delete, modify functions. Actions: create, delete, get_info, add_input, add_output, remove_param, list",
	"Blueprint",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageBlueprintFunction(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 7. manage_blueprint_node
REGISTER_VIBEUE_TOOL(manage_blueprint_node,
	"Manage blueprint graph nodes - add, remove, connect nodes. Actions: add, remove, connect, disconnect, get_nodes, get_node_info, discover",
	"Blueprint",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageBlueprintNode(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 8. manage_blueprint_variable
REGISTER_VIBEUE_TOOL(manage_blueprint_variable,
	"Manage blueprint variables - add, delete, get/set variables. Actions: add, delete, get, get_property, set_property, list, get_available_types",
	"Blueprint",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageBlueprintVariable(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 9. manage_enhanced_input
REGISTER_VIBEUE_TOOL(manage_enhanced_input,
	"Manage enhanced input - create input actions, mapping contexts, key bindings. Actions: create_action, create_context, add_mapping, remove_mapping, get_actions, get_contexts",
	"Input",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageEnhancedInput(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 10. manage_level_actors
REGISTER_VIBEUE_TOOL(manage_level_actors,
	"Manage level actors - spawn, transform, query, modify actors. Actions: add, remove, list, find, get_info, set_transform, set_location, set_rotation, set_scale, focus, get_property, set_property, attach, detach, select, rename",
	"Level",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageLevelActors(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 11. manage_material
REGISTER_VIBEUE_TOOL(manage_material,
	"Manage materials - create, modify, compile materials. Actions: create, get_info, set_property, get_property, compile",
	"Material",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageMaterial(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 12. manage_material_node
REGISTER_VIBEUE_TOOL(manage_material_node,
	"Manage material graph nodes - add, connect, configure nodes. Actions: add, remove, connect, disconnect, get_nodes, set_property",
	"Material",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageMaterialNode(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 13. manage_umg_widget
REGISTER_VIBEUE_TOOL(manage_umg_widget,
	"Manage UMG widgets - create, modify widget blueprints. Actions: create, delete, get_info, add_child, remove_child, set_property, get_property, list_components, get_available_types, add_widget",
	"UI",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageUMGWidget(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);
