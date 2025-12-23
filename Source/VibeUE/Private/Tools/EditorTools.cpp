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
			// Return object with parse error flag so callers can provide better error messages
			Params = MakeShareable(new FJsonObject);
			Params->SetBoolField(TEXT("__json_parse_error__"), true);
			Params->SetStringField(TEXT("__raw_json__"), ParamsJson.Left(500)); // Truncate for safety
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

FString UEditorTools::ManageAsset(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(AssetCommandsInstance->HandleCommand(TEXT("manage_asset"), Params));
}

FString UEditorTools::ManageBlueprint(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(BlueprintCommandsInstance->HandleCommand(TEXT("manage_blueprint"), Params));
}

FString UEditorTools::ManageBlueprintComponent(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(BlueprintComponentInstance->HandleCommand(TEXT("manage_blueprint_component"), Params));
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
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(BlueprintCommandsInstance->HandleCommand(TEXT("manage_blueprint_variable"), Params));
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
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(UMGCommandsInstance->HandleCommand(TEXT("manage_umg_widget"), Params));
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

// 2. manage_asset
REGISTER_VIBEUE_TOOL(manage_asset,
	"Manage assets - search, import, export, save, delete assets. Actions: search, import_texture, export_texture, delete, duplicate, save, save_all, list_references, open. For search: use search_term param (required). For duplicate: use asset_path (source), destination_path (folder), new_name (optional).",
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
	"Manage blueprints - create, compile, reparent, get/set properties, diff. Actions: create, get_info, compile, reparent, set_property, get_property, diff (or compare)",
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
	"Manage blueprint components - add, configure, organize components. Actions: add (or create), remove (or delete), get_hierarchy (or list), set_property, get_property, get_all_properties, reparent, get_available (or search_types), get_info (get component CLASS properties without a blueprint instance). ParamsJson params: blueprint_name (required for most actions), component_type (for add or get_info), component_name (for operations on specific component), property_name/property_value (for properties), parent_name (for reparent), search_filter (for get_available).",
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
	"Manage blueprint graph nodes. WORKFLOW: 1) discover/search to find nodes -> returns spawner_key; 2) create/add using spawner_key; 3) connect to wire nodes. Actions: discover (or search/find), create (or add), delete (or remove), connect, disconnect, list (nodes in graph), details (node info), set_property, configure, split, recombine, refresh_node. ParamsJson params: blueprint_name (required), search_term (for discover), spawner_key (for create, from discover result), position [X,Y] (for create), node_id (for operations), source_node_id/source_pin/target_node_id/target_pin (for connect), function_name (for function graphs).",
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
	"Manage blueprint variables. Actions: help, search_types, create, delete, get_info, list, modify. IMPORTANT: For object/class types (widgets, actors, etc.), ALWAYS use 'search_types' action FIRST with search_text param to find the full type_path (e.g., '/Script/UMG.UserWidget'). Primitive type aliases: float, int, bool, string. For create/modify: use variable_config with type_path, name, category, tooltip, default_value, is_blueprint_read_only, is_editable_in_details. For list: use filter_name or filter_category to filter results.",
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
	"Enhanced Input System management for Input Actions, Mapping Contexts, Modifiers, and Triggers. Actions: action_create, action_list, action_get_properties, action_configure, mapping_create_context, mapping_list_contexts, mapping_add_key_mapping, mapping_get_mappings, mapping_remove_mapping, mapping_add_modifier, mapping_remove_modifier, mapping_get_modifiers, mapping_add_trigger, mapping_remove_trigger, mapping_get_triggers, mapping_get_available_keys, mapping_get_available_modifier_types, mapping_get_available_trigger_types, reflection_discover_types. ParamsJson params: service (action, mapping, reflection), action_name, asset_path, value_type (Digital, Axis1D, Axis2D, Axis3D), context_name, context_path, key, mapping_index, modifier_type, trigger_type. Use action='help' for details.",
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
	"Manage level actors - spawn, transform, query, modify actors. Actions: help, add, remove, list, find, get_info, set_transform, get_transform, set_location, set_rotation, set_scale, focus, move_to_view, refresh_viewport, get_property, set_property, get_all_properties, set_folder, attach, detach, select, rename",
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
	"Manage materials - create, modify, compile materials. Actions: help, create, create_instance, save, compile, refresh_editor, open, get_info, summarize, list_properties, get_property, get_property_info, set_property, set_properties, list_parameters, get_parameter, set_parameter_default, get_instance_info, list_instance_properties, get_instance_property, set_instance_property, list_instance_parameters, set_instance_scalar_parameter, set_instance_vector_parameter, set_instance_texture_parameter, clear_instance_parameter_override, save_instance",
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
	"Manage material graph nodes - create, connect, configure nodes. Actions: help, discover_types, get_categories, create, delete, move, list, get_details, get_pins, connect, disconnect, connect_to_output, disconnect_output, list_connections, get_property, set_property, list_properties, promote_to_parameter, create_parameter, set_parameter_metadata, get_output_properties, get_output_connections. ParamsJson params: material_path (required for most), expression_id (for node operations), expression_class (for create), position [X,Y], property_name, property_value, output_name (for connect_to_output).",
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
	"Manage UMG Widget Blueprint components: add/remove/configure UI widgets inside existing Widget Blueprints. NOTE: Use manage_blueprint to CREATE widget blueprints. Actions: list_components, add_component, remove_component, validate, search_types, get_component_properties, get_property, set_property, list_properties, get_available_events, bind_events. ParamsJson params: widget_name (required), component_name, component_type, parent_name, property_name, property_value, input_mappings (for bind_events).",
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

//=============================================================================
// CLEANUP
//=============================================================================
void UEditorTools::CleanupCommandHandlers()
{
	UE_LOG(LogTemp, Display, TEXT("EditorTools: Cleaning up command handlers..."));
	
	LevelActorCommandsInstance.Reset();
	BlueprintCommandsInstance.Reset();
	BlueprintNodeCommandsInstance.Reset();
	BlueprintComponentInstance.Reset();
	UMGCommandsInstance.Reset();
	MaterialCommandsInstance.Reset();
	MaterialNodeCommandsInstance.Reset();
	AssetCommandsInstance.Reset();
	EnhancedInputCommandsInstance.Reset();
	SharedServiceContext.Reset();
	
	UE_LOG(LogTemp, Display, TEXT("EditorTools: Command handlers cleaned up"));
}
