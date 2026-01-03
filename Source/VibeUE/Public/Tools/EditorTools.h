// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EditorTools.generated.h"

/**
 * Editor Tools - Multi-action tools for Unreal Editor operations
 * These tools wrap the existing command handlers and expose them via reflection
 */
UCLASS()
class VIBEUE_API UEditorTools : public UObject
{
	GENERATED_BODY()

public:
	//========================================================================
	// CONNECTION/STATUS TOOLS
	//========================================================================
	
	/**
	 * Check Unreal Engine connection status and get system information
	 */
	UFUNCTION()
	static FString CheckUnrealConnection();

	//========================================================================
	//========================================================================
	// ASSET TOOLS
	//========================================================================
	
	/**
	 * Manage assets - import, export, search, organize assets
	 * 
	 * Actions:
	 * - search: Search for assets by name/type/path
	 * - import_texture: Import texture from file
	 * - export_texture: Export texture for analysis
	 * - delete: Delete asset
	 * - duplicate: Duplicate asset
	 * - save: Save asset
	 * - save_all: Save all dirty assets
	 * - list_references: List asset references
	 * - open: Open asset in editor
	 */
	UFUNCTION()
	static FString ManageAsset(const FString& Action, const FString& ParamsJson);

	/**
	 * Manage UDataAsset instances with reflection-based property access
	 * 
	 * Actions:
	 * - help: Get help for actions
	 * - search_types: Find UDataAsset subclasses (use search_filter param)
	 * - list: List data assets (use class_name and path params)
	 * - create: Create data asset (class_name, asset_path, asset_name)
	 * - get_info: Get data asset info with all properties (asset_path)
	 * - list_properties: List available properties (asset_path or class_name)
	 * - get_property: Get property value (asset_path, property_name)
	 * - set_property: Set property value (asset_path, property_name, property_value)
	 * - set_properties: Set multiple properties (asset_path, properties object)
	 * - get_class_info: Get class info and properties (class_name)
	 * 
	 * Note: For delete/duplicate/save operations, use manage_asset tool instead.
	 */
	UFUNCTION()
	static FString ManageDataAsset(const FString& Action, const FString& ParamsJson);

	/**
	 * Manage UDataTable assets with reflection-based row operations
	 * 
	 * Actions:
	 * - help: Get help for actions
	 * - search_row_types: Find available row struct types (use search_filter param)
	 * - list: List data tables (use row_struct and path params to filter)
	 * - create: Create data table (row_struct, asset_path, asset_name)
	 * - get_info: Get data table info with columns and rows (table_path)
	 * - get_row_struct: Get row struct column definitions (table_path or struct_name)
	 * - list_rows: List row names in table (table_path)
	 * - get_row: Get single row data (table_path, row_name)
	 * - add_row: Add new row (table_path, row_name, data)
	 * - update_row: Update existing row (table_path, row_name, data)
	 * - remove_row: Remove row (table_path, row_name)
	 * - rename_row: Rename row (table_path, row_name, new_name)
	 * - add_rows: Bulk add rows (table_path, rows object)
	 * - clear_rows: Clear all rows (table_path, confirm=true)
	 * - import_json: Import rows from JSON (table_path, json_data, mode)
	 * - export_json: Export rows to JSON (table_path, format)
	 * 
	 * Note: For delete/duplicate/save operations, use manage_asset tool instead.
	 */
	UFUNCTION()
	static FString ManageDataTable(const FString& Action, const FString& ParamsJson);

	//========================================================================
	// BLUEPRINT TOOLS
	//========================================================================
	
	/**
	 * Manage blueprints - create, compile, reparent blueprint assets
	 * 
	 * Actions:
	 * - create: Create new blueprint (path, parent_class)
	 * - get_info: Get blueprint information
	 * - compile: Compile a blueprint
	 * - reparent: Change blueprint parent class
	 * - set_property: Set blueprint class default property
	 * - get_property: Get blueprint class default property
	 */
	UFUNCTION()
	static FString ManageBlueprint(const FString& Action, const FString& ParamsJson);

	/**
	 * Manage blueprint components - add, configure, organize components
	 * 
	 * Actions:
	 * - add: Add component to blueprint
	 * - remove: Remove component
	 * - get_hierarchy: Get component hierarchy
	 * - set_property: Set component property
	 * - get_property: Get component property
	 * - get_all_properties: List all component properties
	 * - reparent: Change component parent
	 * - get_available: Get available component types
	 */
	UFUNCTION()
	static FString ManageBlueprintComponent(const FString& Action, const FString& ParamsJson);

	/**
	 * Manage blueprint functions - create, modify, delete blueprint functions
	 * 
	 * Actions:
	 * - create: Create a new function
	 * - delete: Delete a function
	 * - get_info: Get function info
	 * - add_input: Add input parameter
	 * - add_output: Add output parameter
	 * - remove_param: Remove parameter
	 * - list: List all functions in blueprint
	 */
	UFUNCTION()
	static FString ManageBlueprintFunction(const FString& Action, const FString& ParamsJson);

	/**
	 * Manage blueprint nodes - add, connect, modify blueprint graph nodes
	 * 
	 * Actions:
	 * - add: Add a node to blueprint graph
	 * - remove: Remove a node
	 * - connect: Connect two pins
	 * - disconnect: Disconnect pins
	 * - get_nodes: List all nodes in a graph
	 * - get_node_info: Get info about a specific node
	 * - discover: Discover available nodes
	 */
	UFUNCTION()
	static FString ManageBlueprintNode(const FString& Action, const FString& ParamsJson);

	/**
	 * Manage blueprint variables - create, inspect, modify, and delete blueprint variables.
	 * 
	 * IMPORTANT: Use get_info action to discover ALL modifiable properties (replication settings, 
	 * instance editability, tooltips, categories, etc.), then use modify to change any of them.
	 * Use get_property_options to discover valid values for enum properties like replication_condition.
	 * 
	 * Actions:
	 * - help: Get help information about variable management
	 * - search_types: Search for available variable types  
	 * - create: Create new variable in blueprint
	 * - delete: Remove variable from blueprint
	 * - get_info: Get complete variable information (returns ALL modifiable properties)
	 * - get_property_options: Discover valid values for a property (e.g., replication_condition options)
	 * - list: List all variables in blueprint
	 * - modify: Modify any property returned by get_info (replication, default value, tooltip, etc.)
	 */
	UFUNCTION()
	static FString ManageBlueprintVariable(const FString& Action, const FString& ParamsJson);

	//========================================================================
	// INPUT TOOLS
	//========================================================================
	
	/**
	 * Manage enhanced input - configure input actions and mappings
	 * 
	 * Actions:
	 * - create_action: Create input action
	 * - create_context: Create mapping context
	 * - add_mapping: Add key mapping
	 * - remove_mapping: Remove key mapping
	 * - get_actions: List input actions
	 * - get_contexts: List mapping contexts
	 */
	UFUNCTION()
	static FString ManageEnhancedInput(const FString& Action, const FString& ParamsJson);

	//========================================================================
	// LEVEL ACTOR TOOLS
	//========================================================================
	
	/**
	 * Manage level actors - spawn, modify, query actors in the level
	 * 
	 * Actions:
	 * - add: Spawn a new actor (class_name, location, rotation, scale, label)
	 * - remove: Delete an actor by name or label
	 * - list: List all actors (optional: class_filter, folder_filter)
	 * - find: Find actors by name pattern or class
	 * - get_info: Get detailed info about an actor
	 * - set_transform: Set full transform (location, rotation, scale)
	 * - get_transform: Get actor transform
	 * - set_location: Set actor location (x, y, z)
	 * - set_rotation: Set actor rotation (pitch, yaw, roll)
	 * - set_scale: Set actor scale (x, y, z)
	 * - focus: Focus editor viewport on actor
	 * - get_property: Get actor property value
	 * - set_property: Set actor property value
	 * - attach: Attach actor to parent
	 * - detach: Detach actor from parent
	 * - select: Select actor(s) in editor
	 * - rename: Rename actor label
	 */
	UFUNCTION()
	static FString ManageLevelActors(const FString& Action, const FString& ParamsJson);

	//========================================================================
	// MATERIAL TOOLS
	//========================================================================
	
	/**
	 * Manage materials - create and modify material assets
	 * 
	 * Actions:
	 * - create: Create new material
	 * - get_info: Get material info
	 * - set_property: Set material property
	 * - get_property: Get material property
	 * - compile: Compile material
	 */
	UFUNCTION()
	static FString ManageMaterial(const FString& Action, const FString& ParamsJson);

	/**
	 * Manage material nodes - add and connect material graph nodes
	 * 
	 * Actions:
	 * - add: Add node to material graph
	 * - remove: Remove node
	 * - connect: Connect pins
	 * - disconnect: Disconnect pins
	 * - get_nodes: List nodes
	 * - set_property: Set node property
	 */
	UFUNCTION()
	static FString ManageMaterialNode(const FString& Action, const FString& ParamsJson);

	//========================================================================
	// UMG/WIDGET TOOLS
	//========================================================================
	
	/**
	 * Manage UMG widgets - create and modify UI widget blueprints
	 * 
	 * Actions:
	 * - create: Create widget blueprint
	 * - delete: Delete widget blueprint
	 * - get_info: Get widget blueprint info
	 * - add_child: Add child widget to panel
	 * - remove_child: Remove widget from parent
	 * - set_property: Set widget property
	 * - get_property: Get widget property
	 * - list_components: List widget components
	 * - get_available_types: Get available widget types
	 */
	UFUNCTION()
	static FString ManageUMGWidget(const FString& Action, const FString& ParamsJson);

	/**
	 * Cleanup all static command handler instances
	 * Called during module shutdown to ensure proper cleanup
	 */
	static void CleanupCommandHandlers();

private:
	/** Helper to parse JSON params */
	static TSharedPtr<FJsonObject> ParseParams(const FString& ParamsJson);
	
	/** Helper to serialize result to JSON string */
	static FString SerializeResult(const TSharedPtr<FJsonObject>& Result);
};
