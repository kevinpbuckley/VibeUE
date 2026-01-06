// Copyright Buckley Builds LLC 2025 All Rights Reserved.

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
#include "Commands/DataAssetCommands.h"
#include "Commands/DataTableCommands.h"
#include "Commands/PythonCommands.h"
#include "Commands/FileSystemCommands.h"
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
static TSharedPtr<FDataAssetCommands> DataAssetCommandsInstance;
static TSharedPtr<FDataTableCommands> DataTableCommandsInstance;
static TSharedPtr<VibeUE::FPythonCommands> PythonCommandsInstance;
static TSharedPtr<FFileSystemCommands> FileSystemCommandsInstance;
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
	if (!DataAssetCommandsInstance.IsValid())
	{
		DataAssetCommandsInstance = MakeShared<FDataAssetCommands>();
	}
	if (!DataTableCommandsInstance.IsValid())
	{
		DataTableCommandsInstance = MakeShared<FDataTableCommands>();
	}
	if (!PythonCommandsInstance.IsValid())
	{
		PythonCommandsInstance = MakeShared<VibeUE::FPythonCommands>();
	}
	if (!FileSystemCommandsInstance.IsValid())
	{
		FileSystemCommandsInstance = MakeShared<FFileSystemCommands>();
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

FString UEditorTools::ManageDataAsset(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(DataAssetCommandsInstance->HandleCommand(TEXT("manage_data_asset"), Params));
}

FString UEditorTools::ManageDataTable(const FString& Action, const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), Action);
	return SerializeResult(DataTableCommandsInstance->HandleCommand(TEXT("manage_data_table"), Params));
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

// Python Execution - Individual Tool Functions
FString UEditorTools::DiscoverPythonModule(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), TEXT("discover_module"));
	return SerializeResult(PythonCommandsInstance->HandleCommand(TEXT("manage_python_execution"), Params));
}

FString UEditorTools::DiscoverPythonClass(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), TEXT("discover_class"));
	return SerializeResult(PythonCommandsInstance->HandleCommand(TEXT("manage_python_execution"), Params));
}

FString UEditorTools::DiscoverPythonFunction(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), TEXT("discover_function"));
	return SerializeResult(PythonCommandsInstance->HandleCommand(TEXT("manage_python_execution"), Params));
}

FString UEditorTools::ListPythonSubsystems(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), TEXT("list_subsystems"));
	return SerializeResult(PythonCommandsInstance->HandleCommand(TEXT("manage_python_execution"), Params));
}

FString UEditorTools::ExecutePythonCode(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), TEXT("execute_code"));
	return SerializeResult(PythonCommandsInstance->HandleCommand(TEXT("manage_python_execution"), Params));
}

FString UEditorTools::EvaluatePythonExpression(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), TEXT("evaluate_expression"));
	return SerializeResult(PythonCommandsInstance->HandleCommand(TEXT("manage_python_execution"), Params));
}

FString UEditorTools::GetPythonExamples(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), TEXT("get_examples"));
	return SerializeResult(PythonCommandsInstance->HandleCommand(TEXT("manage_python_execution"), Params));
}

FString UEditorTools::GetPythonHelp(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	Params->SetStringField(TEXT("action"), TEXT("help"));
	return SerializeResult(PythonCommandsInstance->HandleCommand(TEXT("manage_python_execution"), Params));
}

//=============================================================================
// FILESYSTEM TOOLS
//=============================================================================

FString UEditorTools::ReadFile(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	return SerializeResult(FileSystemCommandsInstance->HandleCommand(TEXT("read_file"), Params));
}

FString UEditorTools::ListDir(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	return SerializeResult(FileSystemCommandsInstance->HandleCommand(TEXT("list_dir"), Params));
}

FString UEditorTools::FileSearch(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	return SerializeResult(FileSystemCommandsInstance->HandleCommand(TEXT("file_search"), Params));
}

FString UEditorTools::GrepSearch(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	return SerializeResult(FileSystemCommandsInstance->HandleCommand(TEXT("grep_search"), Params));
}

FString UEditorTools::GetDirectories(const FString& ParamsJson)
{
	EnsureCommandHandlersInitialized();
	TSharedPtr<FJsonObject> Params = ParseParams(ParamsJson);
	return SerializeResult(FileSystemCommandsInstance->HandleCommand(TEXT("get_directories"), Params));
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

// 3. manage_data_asset
REGISTER_VIBEUE_TOOL(manage_data_asset,
	"Manage UDataAsset instances with reflection-based property access. Actions: help, search_types (find UDataAsset subclasses), list (list data assets), create, get_info, list_properties, get_property, set_property, set_properties, get_class_info. Use search_types first to discover available data asset classes. For delete/duplicate/save, use manage_asset tool. ParamsJson params: asset_path, class_name, property_name, property_value, properties (object for set_properties), search_filter.",
	"Data",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageDataAsset(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 4. manage_data_table
REGISTER_VIBEUE_TOOL(manage_data_table,
	"Manage UDataTable assets with reflection-based row operations. Actions: help, search_row_types (find available row struct types), list (list data tables), create (create new data table), get_info (get table structure and rows), get_row_struct (get row struct columns), list_rows, get_row, add_row, update_row, remove_row, rename_row, add_rows (bulk add), clear_rows, import_json, export_json. Use search_row_types first to discover available row struct types for create. ParamsJson params: table_path (required for most), row_struct (for create), row_name (for row ops), data (JSON object for row values), rows (for add_rows), json_data (for import_json).",
	"Data",
	TOOL_PARAMS(
		TOOL_PARAM("Action", "Action to perform", "string", true),
		TOOL_PARAM_DEFAULT("ParamsJson", "Action parameters as JSON", "string", "{}")
	),
	{
		return UEditorTools::ManageDataTable(
			Params.FindRef(TEXT("Action")),
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 5. manage_blueprint
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
	"Manage blueprint functions - create, delete, modify functions. Actions: create, delete, get_info, add_input, add_output, remove_param, list, add_local_variable, remove_local_variable, update_local_variable, list_local_variables, get_available_local_types",
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
	"Enhanced Input System management for Input Actions, Mapping Contexts, Modifiers, and Triggers. Actions: action_create (requires action_name + asset_path + value_type), action_list, action_get_properties (action_path), action_configure (action_path + property_name/value), mapping_create_context (context_name + context_path), mapping_list_contexts, mapping_add_key_mapping (context_path + action_path + key), mapping_get_mappings, mapping_remove_mapping, mapping_add_modifier, mapping_remove_modifier, mapping_get_modifiers, mapping_add_trigger, mapping_remove_trigger, mapping_get_triggers, mapping_get_available_keys, mapping_get_available_modifier_types, mapping_get_available_trigger_types, reflection_discover_types. value_type: Digital, Axis1D, Axis2D, Axis3D. Use action='help' for details.",
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

// 14. discover_python_module
REGISTER_VIBEUE_TOOL(discover_python_module,
	"Introspect and discover a Python/Unreal module's contents (functions, classes, constants). Returns detailed module structure. CRITICAL: Use this BEFORE working with unfamiliar modules to understand available APIs. ParamsJson params: module_name (required, e.g., 'unreal', 'unreal_engine'), max_items (optional, default 100).",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "JSON with module_name (required)", "string", "{}")
	),
	{
		return UEditorTools::DiscoverPythonModule(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 15. discover_python_class
REGISTER_VIBEUE_TOOL(discover_python_class,
	"Discover a Python/Unreal class structure - methods, properties, inheritance, docstrings. **CRITICAL: ALWAYS use this BEFORE accessing unfamiliar classes.** Returns complete class API. On AttributeError: call this immediately to learn correct API. ParamsJson params: class_name (required, e.g., 'unreal.EditorAssetLibrary', 'BlueprintFactory'), include_inherited (optional bool, default true), include_private (optional bool, default false).",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "JSON with class_name (required)", "string", "{}")
	),
	{
		return UEditorTools::DiscoverPythonClass(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 16. discover_python_function
REGISTER_VIBEUE_TOOL(discover_python_function,
	"Get detailed function signature, parameters, return type, and docstring. Use BEFORE calling unfamiliar functions to learn correct parameter names and types. ParamsJson params: function_path (required, e.g., 'unreal.EditorAssetLibrary.load_asset', 'MyClass.my_method').",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "JSON with function_path (required)", "string", "{}")
	),
	{
		return UEditorTools::DiscoverPythonFunction(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 17. list_python_subsystems
REGISTER_VIBEUE_TOOL(list_python_subsystems,
	"List all available Unreal Engine editor subsystems (EditorActorSubsystem, EditorAssetSubsystem, etc.). Returns subsystem names and brief descriptions. Use to discover what editor functionality is available via Python. No parameters required.",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "Empty JSON object", "string", "{}")
	),
	{
		return UEditorTools::ListPythonSubsystems(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 18. execute_python_code
REGISTER_VIBEUE_TOOL(execute_python_code,
	"Execute Python code in Unreal Engine context. **USE LAST after discovery tools.** Has access to 'unreal' module and all editor APIs. Returns stdout/stderr output and any errors. **NEVER modify CDOs (Class Default Objects) - causes crashes.** ParamsJson params: code (required, Python code string), timeout (optional, milliseconds, default 5000), capture_output (optional bool, default true).",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "JSON with code (required)", "string", "{}")
	),
	{
		return UEditorTools::ExecutePythonCode(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 19. evaluate_python_expression
REGISTER_VIBEUE_TOOL(evaluate_python_expression,
	"Evaluate a Python expression and return its value (converted to string). Useful for quick queries and inspecting objects. Safer than execute_code for simple expressions. ParamsJson params: expression (required, Python expression string), timeout (optional, milliseconds, default 5000).",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "JSON with expression (required)", "string", "{}")
	),
	{
		return UEditorTools::EvaluatePythonExpression(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 20. get_python_examples
REGISTER_VIBEUE_TOOL(get_python_examples,
	"Get working code examples from the plugin's examples/ folder. Returns example code with explanations for common tasks (blueprint operations, asset management, material editing, etc.). Use to learn patterns BEFORE implementing. ParamsJson params: category (optional, filter by category: 'blueprint', 'asset', 'material', 'common', 'level'), search_term (optional, search example titles/descriptions), tags (optional array, filter by tags).",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "Optional filter params as JSON", "string", "{}")
	),
	{
		return UEditorTools::GetPythonExamples(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 21. get_python_help
REGISTER_VIBEUE_TOOL(get_python_help,
	"Get comprehensive help documentation for Python tools. Returns detailed usage information, parameter descriptions, examples, and common patterns. Use when unsure how to use Python tools or need workflow guidance. ParamsJson params: topic (optional, specific action/topic name for detailed help, omit for general overview).",
	"Python",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "Optional topic parameter as JSON", "string", "{}")
	),
	{
		return UEditorTools::GetPythonHelp(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

//=============================================================================
// FILESYSTEM TOOLS
//=============================================================================

// 22. read_file
REGISTER_VIBEUE_TOOL(read_file,
	"Read file contents with line range support (like VSCode read_file). Supports any text file in project. ParamsJson params: filePath (required, absolute or relative path), startLine (optional, default 1, 1-indexed), endLine (optional, default -1 for EOF).",
	"Filesystem",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "JSON with filePath (required), startLine (optional), endLine (optional)", "string", "{}")
	),
	{
		return UEditorTools::ReadFile(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 23. list_dir
REGISTER_VIBEUE_TOOL(list_dir,
	"List directory contents - files and subdirectories. ParamsJson params: path (required, directory path).",
	"Filesystem",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "JSON with path (required)", "string", "{}")
	),
	{
		return UEditorTools::ListDir(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 24. file_search
REGISTER_VIBEUE_TOOL(file_search,
	"Find files matching glob patterns (like VSCode file_search). Searches from project root. ParamsJson params: query (required, glob pattern like '**/*.cpp', '**/*.h', 'Source/**'), maxResults (optional, default 100).",
	"Filesystem",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "JSON with query (required), maxResults (optional)", "string", "{}")
	),
	{
		return UEditorTools::FileSearch(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 25. grep_search
REGISTER_VIBEUE_TOOL(grep_search,
	"Search for text/regex patterns in files (like VSCode grep_search). Fast code search across workspace. ParamsJson params: query (required, search pattern), isRegexp (optional bool, default false), includePattern (optional, file glob to search in), includeIgnoredFiles (optional bool, default false, search build directories), maxResults (optional, default 50).",
	"Filesystem",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "JSON with query (required), isRegexp, includePattern, includeIgnoredFiles, maxResults (all optional)", "string", "{}")
	),
	{
		return UEditorTools::GrepSearch(
			Params.FindRef(TEXT("ParamsJson"))
		);
	}
);

// 26. get_directories
REGISTER_VIBEUE_TOOL(get_directories,
	"Get important project directories: game directory (project root), VibeUE plugin directory, and Unreal Engine's Python API installation directories (include, lib, site-packages). Returns platform-specific paths (Windows/Mac/Linux) for locating project files, plugin source, and UE Python API. Use with read_file, list_dir, or grep_search to explore these directories. No parameters required.",
	"Filesystem",
	TOOL_PARAMS(
		TOOL_PARAM_DEFAULT("ParamsJson", "Empty JSON object", "string", "{}")
	),
	{
		return UEditorTools::GetDirectories(
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
	DataAssetCommandsInstance.Reset();
	DataTableCommandsInstance.Reset();
	PythonCommandsInstance.Reset();
	FileSystemCommandsInstance.Reset();
	SharedServiceContext.Reset();
	
	UE_LOG(LogTemp, Display, TEXT("EditorTools: Command handlers cleaned up"));
}
