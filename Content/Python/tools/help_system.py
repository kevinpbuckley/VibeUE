"""
Tool Help System

Centralized help content and formatting for all VibeUE MCP tools.
Each tool can use this to provide consistent help information via the 'help' action.
"""

from typing import Dict, Any, List, Optional

# Tool help metadata - maps tool names to their documentation
TOOL_HELP = {
    "manage_asset": {
        "summary": "Manage project assets including search, import/export, and file operations",
        "topic": "asset-discovery",
        "actions": {
            "search": {
                "description": "Search for assets in the project",
                "parameters": {
                    "search_term": "Text to search for in asset names/paths",
                    "asset_type": "Filter by asset type (e.g., 'Texture2D', 'Material')",
                    "path": "Root path to search from (default: /Game)",
                    "case_sensitive": "Whether search should be case-sensitive",
                    "include_engine_content": "Include engine content in search",
                    "max_results": "Maximum number of results to return"
                },
                "example": 'manage_asset(action="search", search_term="Player", asset_type="Blueprint", path="/Game/Blueprints")'
            },
            "import_texture": {
                "description": "Import a texture file into the project",
                "parameters": {
                    "file_path": "Path to the texture file to import",
                    "destination_path": "Unreal path where texture will be imported",
                    "texture_name": "Optional name for the imported texture",
                    "compression_settings": "Texture compression (TC_Default, TC_Normalmap, etc.)",
                    "generate_mipmaps": "Whether to generate mipmap levels",
                    "auto_convert_svg": "Automatically convert SVG files to PNG"
                },
                "example": 'manage_asset(action="import_texture", file_path="C:/Images/icon.png", destination_path="/Game/Textures")'
            },
            "export_texture": {
                "description": "Export a texture asset to a file",
                "parameters": {
                    "asset_path": "Unreal path to the texture asset",
                    "export_format": "Output format (PNG, TGA, etc.)",
                    "max_size": "Optional max size as [width, height]",
                    "temp_folder": "Optional temporary folder path"
                },
                "example": 'manage_asset(action="export_texture", asset_path="/Game/Textures/T_Icon", export_format="PNG")'
            },
            "delete": {
                "description": "Delete an asset from the project",
                "parameters": {
                    "asset_path": "Unreal path to the asset to delete",
                    "force_delete": "Skip confirmations and force delete",
                    "show_confirmation": "Show confirmation dialog"
                },
                "example": 'manage_asset(action="delete", asset_path="/Game/Temp/TestAsset")'
            },
            "open_in_editor": {
                "description": "Open an asset in its editor",
                "parameters": {
                    "asset_path": "Unreal path to the asset",
                    "force_open": "Force open even if already open"
                },
                "example": 'manage_asset(action="open_in_editor", asset_path="/Game/Blueprints/BP_Player")'
            },
            "svg_to_png": {
                "description": "Convert an SVG file to PNG format",
                "parameters": {
                    "svg_path": "Path to the SVG file",
                    "output_path": "Optional output PNG path",
                    "size": "Output size as [width, height]",
                    "scale": "Scale multiplier (default: 1.0)",
                    "background": "Optional background color"
                },
                "example": 'manage_asset(action="svg_to_png", svg_path="C:/Icons/logo.svg", size=[512, 512])'
            },
            "duplicate": {
                "description": "Duplicate an asset",
                "parameters": {
                    "asset_path": "Unreal path to the asset to duplicate",
                    "destination_path": "Where to create the duplicate",
                    "new_name": "Name for the duplicated asset"
                },
                "example": 'manage_asset(action="duplicate", asset_path="/Game/Materials/M_Base", new_name="M_Base_Copy")'
            },
            "save": {
                "description": "Save a specific asset",
                "parameters": {
                    "asset_path": "Unreal path to the asset to save"
                },
                "example": 'manage_asset(action="save", asset_path="/Game/Blueprints/BP_Player")'
            },
            "save_all": {
                "description": "Save all dirty (modified) assets",
                "parameters": {
                    "prompt_user": "Whether to show save dialog to user"
                },
                "example": 'manage_asset(action="save_all", prompt_user=False)'
            },
            "list_references": {
                "description": "List all assets that reference this asset",
                "parameters": {
                    "asset_path": "Unreal path to the asset",
                    "include_dependencies": "Also include assets this asset depends on"
                },
                "example": 'manage_asset(action="list_references", asset_path="/Game/Materials/M_Base")'
            }
        }
    },
    
    "manage_blueprint": {
        "summary": "Blueprint lifecycle management including creation, compilation, and configuration",
        "topic": "blueprint-workflow",
        "actions": {
            "create": {
                "description": "Create a new Blueprint class",
                "parameters": {
                    "name": "Name for the new Blueprint (without BP_ prefix)",
                    "parent_class": "Parent class (Actor, Pawn, Character, UserWidget, ActorComponent, etc.)"
                },
                "example": 'manage_blueprint(action="create", name="PlayerController", parent_class="Actor")'
            },
            "compile": {
                "description": "Compile a Blueprint to apply changes",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint (e.g., /Game/Blueprints/BP_Player)"
                },
                "example": 'manage_blueprint(action="compile", blueprint_name="/Game/Blueprints/BP_Player")'
            },
            "get_info": {
                "description": "Get detailed information about a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "include_class_defaults": "Include Class Default Object properties"
                },
                "example": 'manage_blueprint(action="get_info", blueprint_name="/Game/Blueprints/BP_Player")'
            },
            "get_property": {
                "description": "Get a specific property value from a Blueprint's Class Default Object",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "property_name": "Name of the property to get"
                },
                "example": 'manage_blueprint(action="get_property", blueprint_name="/Game/Blueprints/BP_Player", property_name="MaxHealth")'
            },
            "set_property": {
                "description": "Set a property value on a Blueprint's Class Default Object",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "property_name": "Name of the property to set",
                    "property_value": "Value to set (string, number, or boolean)"
                },
                "example": 'manage_blueprint(action="set_property", blueprint_name="/Game/Blueprints/BP_Player", property_name="MaxHealth", property_value=100.0)'
            },
            "reparent": {
                "description": "Change the parent class of a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "new_parent_class": "New parent class name"
                },
                "example": 'manage_blueprint(action="reparent", blueprint_name="/Game/Blueprints/BP_Enemy", new_parent_class="Character")'
            },
            "list_custom_events": {
                "description": "List all custom events in a Blueprint's Event Graph",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint"
                },
                "example": 'manage_blueprint(action="list_custom_events", blueprint_name="/Game/Blueprints/BP_Player")'
            },
            "summarize_event_graph": {
                "description": "Get a summary of the Event Graph structure",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "max_nodes": "Maximum number of nodes to include in summary"
                },
                "example": 'manage_blueprint(action="summarize_event_graph", blueprint_name="/Game/Blueprints/BP_Player", max_nodes=100)'
            }
        }
    },
    
    "manage_blueprint_component": {
        "summary": "Blueprint component management including creation, properties, and hierarchy",
        "topic": "blueprint-workflow",
        "actions": {
            "search_types": {
                "description": "Search for available component types",
                "parameters": {
                    "search_term": "Text to search for in component type names",
                    "max_results": "Maximum number of results to return"
                },
                "example": 'manage_blueprint_component(action="search_types", search_term="Light")'
            },
            "list": {
                "description": "List all components in a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint"
                },
                "example": 'manage_blueprint_component(action="list", blueprint_name="/Game/Blueprints/BP_Player")'
            },
            "create": {
                "description": "Add a new component to a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "component_name": "Name for the new component",
                    "component_type": "Type of component (e.g., 'PointLightComponent', 'StaticMeshComponent')",
                    "parent_component": "Optional parent component name for attachment"
                },
                "example": 'manage_blueprint_component(action="create", blueprint_name="/Game/Blueprints/BP_Lamp", component_name="MainLight", component_type="PointLightComponent")'
            },
            "delete": {
                "description": "Remove a component from a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "component_name": "Name of the component to remove"
                },
                "example": 'manage_blueprint_component(action="delete", blueprint_name="/Game/Blueprints/BP_Lamp", component_name="MainLight")'
            },
            "get_property": {
                "description": "Get a component property value",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "component_name": "Name of the component",
                    "property_name": "Name of the property"
                },
                "example": 'manage_blueprint_component(action="get_property", blueprint_name="/Game/Blueprints/BP_Lamp", component_name="MainLight", property_name="Intensity")'
            },
            "set_property": {
                "description": "Set a component property value",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "component_name": "Name of the component",
                    "property_name": "Name of the property",
                    "property_value": "Value to set (use proper format for colors: {\"R\":1.0,\"G\":1.0,\"B\":0.0,\"A\":1.0})"
                },
                "example": 'manage_blueprint_component(action="set_property", blueprint_name="/Game/Blueprints/BP_Lamp", component_name="MainLight", property_name="Intensity", property_value=5000.0)'
            },
            "get_all_properties": {
                "description": "Get all properties of a component",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "component_name": "Name of the component"
                },
                "example": 'manage_blueprint_component(action="get_all_properties", blueprint_name="/Game/Blueprints/BP_Lamp", component_name="MainLight")'
            },
            "reparent": {
                "description": "Change the parent of a component in the hierarchy",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "component_name": "Name of the component to reparent",
                    "new_parent": "New parent component name (or empty for root)"
                },
                "example": 'manage_blueprint_component(action="reparent", blueprint_name="/Game/Blueprints/BP_Lamp", component_name="LightMesh", new_parent="MainLight")'
            },
            "reorder": {
                "description": "Change the order of components in the component list",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "component_name": "Name of the component to reorder",
                    "new_index": "New position index"
                },
                "example": 'manage_blueprint_component(action="reorder", blueprint_name="/Game/Blueprints/BP_Lamp", component_name="MainLight", new_index=0)'
            },
            "get_info": {
                "description": "Get detailed information about a component",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "component_name": "Name of the component"
                },
                "example": 'manage_blueprint_component(action="get_info", blueprint_name="/Game/Blueprints/BP_Lamp", component_name="MainLight")'
            },
            "get_property_metadata": {
                "description": "Get metadata about a component's properties",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "component_name": "Name of the component",
                    "property_name": "Optional specific property name"
                },
                "example": 'manage_blueprint_component(action="get_property_metadata", blueprint_name="/Game/Blueprints/BP_Lamp", component_name="MainLight")'
            },
            "compare_properties": {
                "description": "Compare properties between two components",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "component_name": "First component name",
                    "other_component": "Second component name to compare with"
                },
                "example": 'manage_blueprint_component(action="compare_properties", blueprint_name="/Game/Blueprints/BP_Lamp", component_name="Light1", other_component="Light2")'
            }
        }
    },
    
    "manage_blueprint_variable": {
        "summary": "Blueprint variable management including creation, deletion, and property access",
        "topic": "multi-action-tools",
        "actions": {
            "search_types": {
                "description": "Search for available variable types",
                "parameters": {
                    "search_term": "Text to search for in type names",
                    "max_results": "Maximum number of results"
                },
                "example": 'manage_blueprint_variable(action="search_types", search_term="Float")'
            },
            "create": {
                "description": "Create a new variable in a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "variable_name": "Name for the new variable",
                    "type_path": "Full type path (e.g., '/Script/CoreUObject.FloatProperty')",
                    "default_value": "Optional default value",
                    "category": "Optional category name",
                    "is_replicated": "Whether variable replicates over network",
                    "tooltip": "Optional tooltip description"
                },
                "example": 'manage_blueprint_variable(action="create", blueprint_name="/Game/Blueprints/BP_Player", variable_name="Health", type_path="/Script/CoreUObject.FloatProperty", default_value=100.0)'
            },
            "delete": {
                "description": "Delete a variable from a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "variable_name": "Name of the variable to delete"
                },
                "example": 'manage_blueprint_variable(action="delete", blueprint_name="/Game/Blueprints/BP_Player", variable_name="Health")'
            },
            "get_info": {
                "description": "Get detailed information about a variable",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "variable_name": "Name of the variable"
                },
                "example": 'manage_blueprint_variable(action="get_info", blueprint_name="/Game/Blueprints/BP_Player", variable_name="Health")'
            },
            "list": {
                "description": "List all variables in a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint"
                },
                "example": 'manage_blueprint_variable(action="list", blueprint_name="/Game/Blueprints/BP_Player")'
            },
            "get_property": {
                "description": "Get a variable's metadata property (e.g., category, tooltip)",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "variable_name": "Name of the variable",
                    "property_name": "Property to get (Category, Tooltip, etc.)"
                },
                "example": 'manage_blueprint_variable(action="get_property", blueprint_name="/Game/Blueprints/BP_Player", variable_name="Health", property_name="Category")'
            },
            "set_property": {
                "description": "Set a variable's metadata property",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "variable_name": "Name of the variable",
                    "property_name": "Property to set",
                    "property_value": "New value"
                },
                "example": 'manage_blueprint_variable(action="set_property", blueprint_name="/Game/Blueprints/BP_Player", variable_name="Health", property_name="Category", property_value="Player Stats")'
            }
        }
    },
    
    "manage_blueprint_node": {
        "summary": "Blueprint node/graph operations including discovery, creation, connections, and configuration",
        "topic": "node-tools",
        "actions": {
            "discover": {
                "description": "Discover available Blueprint nodes with spawner keys for exact creation",
                "parameters": {
                    "search_term": "Text to search for in node names/descriptions",
                    "max_results": "Maximum number of results",
                    "category": "Optional category filter"
                },
                "example": 'manage_blueprint_node(action="discover", search_term="Print String")'
            },
            "create": {
                "description": "Create a new node in the Event Graph",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "spawner_key": "Exact spawner key from discover action",
                    "position_x": "X position in graph",
                    "position_y": "Y position in graph",
                    "node_params": "Optional dict of node-specific parameters (required for Variable Get/Set, Cast, etc.)"
                },
                "example": 'manage_blueprint_node(action="create", blueprint_name="/Game/Blueprints/BP_Player", spawner_key="K2_CallFunction_PrintString", position_x=100, position_y=200)'
            },
            "connect_pins": {
                "description": "Connect two node pins together",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "source_node_guid": "GUID of the source node",
                    "source_pin_name": "Name of the source pin",
                    "target_node_guid": "GUID of the target node",
                    "target_pin_name": "Name of the target pin"
                },
                "example": 'manage_blueprint_node(action="connect_pins", blueprint_name="/Game/Blueprints/BP_Player", source_node_guid="ABC123", source_pin_name="execute", target_node_guid="DEF456", target_pin_name="execute")'
            },
            "disconnect_pins": {
                "description": "Disconnect two node pins",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_guid": "GUID of the node",
                    "pin_name": "Name of the pin to disconnect"
                },
                "example": 'manage_blueprint_node(action="disconnect_pins", blueprint_name="/Game/Blueprints/BP_Player", node_guid="ABC123", pin_name="execute")'
            },
            "delete": {
                "description": "Delete a node from the Event Graph",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_guid": "GUID of the node to delete"
                },
                "example": 'manage_blueprint_node(action="delete", blueprint_name="/Game/Blueprints/BP_Player", node_guid="ABC123")'
            },
            "move": {
                "description": "Move a node to a new position",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_guid": "GUID of the node",
                    "position_x": "New X position",
                    "position_y": "New Y position"
                },
                "example": 'manage_blueprint_node(action="move", blueprint_name="/Game/Blueprints/BP_Player", node_guid="ABC123", position_x=500, position_y=300)'
            },
            "list": {
                "description": "List all nodes in the Event Graph",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "include_details": "Include full node details (slower)"
                },
                "example": 'manage_blueprint_node(action="list", blueprint_name="/Game/Blueprints/BP_Player")'
            },
            "describe": {
                "description": "Get description and pins for a node type",
                "parameters": {
                    "spawner_key": "Spawner key from discover action"
                },
                "example": 'manage_blueprint_node(action="describe", spawner_key="K2_CallFunction_PrintString")'
            },
            "configure": {
                "description": "Configure node properties or pin default values",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_guid": "GUID of the node",
                    "pin_name": "Name of the pin to set default value",
                    "pin_value": "Default value to set"
                },
                "example": 'manage_blueprint_node(action="configure", blueprint_name="/Game/Blueprints/BP_Player", node_guid="ABC123", pin_name="InString", pin_value="Hello World")'
            },
            "get_details": {
                "description": "Get detailed information about a specific node instance",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_guid": "GUID of the node"
                },
                "example": 'manage_blueprint_node(action="get_details", blueprint_name="/Game/Blueprints/BP_Player", node_guid="ABC123")'
            },
            "split": {
                "description": "Split a struct pin into its individual components",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_guid": "GUID of the node",
                    "pin_name": "Name of the struct pin to split"
                },
                "example": 'manage_blueprint_node(action="split", blueprint_name="/Game/Blueprints/BP_Player", node_guid="ABC123", pin_name="Location")'
            },
            "recombine": {
                "description": "Recombine a split struct pin",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_guid": "GUID of the node",
                    "pin_name": "Name of the struct pin to recombine"
                },
                "example": 'manage_blueprint_node(action="recombine", blueprint_name="/Game/Blueprints/BP_Player", node_guid="ABC123", pin_name="Location")'
            },
            "refresh_node": {
                "description": "Refresh a single node to update its structure",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_guid": "GUID of the node"
                },
                "example": 'manage_blueprint_node(action="refresh_node", blueprint_name="/Game/Blueprints/BP_Player", node_guid="ABC123")'
            },
            "refresh_nodes": {
                "description": "Refresh all nodes in the Event Graph",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint"
                },
                "example": 'manage_blueprint_node(action="refresh_nodes", blueprint_name="/Game/Blueprints/BP_Player")'
            }
        }
    },
    
    "manage_blueprint_function": {
        "summary": "Blueprint function management including creation, parameters, and local variables",
        "topic": "multi-action-tools",
        "actions": {
            "help": {
                "description": "Show help for this tool or a specific action",
                "parameters": {
                    "help_action": "Optional: specific action to get help for"
                },
                "example": 'manage_blueprint_function(action="help") or manage_blueprint_function(action="help", help_action="create")'
            }
        },
        "note": "This tool supports 15+ actions for function lifecycle management. Use action='help' for complete action list."
    },
    
    "manage_enhanced_input": {
        "summary": "Enhanced Input System management across all services: reflection, action, mapping, modifier, trigger, and AI",
        "topic": "enhanced-input",
        "actions": {
            "help": {
                "description": "Show help for this tool or a specific action",
                "parameters": {
                    "help_action": "Optional: specific action to get help for"
                },
                "example": 'manage_enhanced_input(action="help") or manage_enhanced_input(action="help", help_action="action_create")'
            },
            # Reflection service
            "reflection_discover_types": {
                "description": "Discover all Enhanced Input types (actions, modifiers, triggers)",
                "parameters": {},
                "example": 'manage_enhanced_input(action="reflection_discover_types")'
            },
            "reflection_get_metadata": {
                "description": "Get metadata for a specific Enhanced Input type",
                "parameters": {
                    "input_type": "REQUIRED: Type name to get metadata for"
                },
                "example": 'manage_enhanced_input(action="reflection_get_metadata", input_type="Negate")'
            },
            # Action service
            "action_create": {
                "description": "Create a new Input Action asset",
                "parameters": {
                    "action_name": "REQUIRED: Name for the action (e.g., 'IA_Jump')",
                    "asset_path": "REQUIRED: Content path (e.g., '/Game/Input/Actions')",
                    "value_type": "REQUIRED: Digital, Axis1D, Axis2D, or Axis3D"
                },
                "example": 'manage_enhanced_input(action="action_create", action_name="IA_Interact", asset_path="/Game/Input/Actions", value_type="Digital")'
            },
            "action_list": {
                "description": "List all Input Actions in the project",
                "parameters": {},
                "example": 'manage_enhanced_input(action="action_list")'
            },
            "action_get_properties": {
                "description": "Get properties of an Input Action",
                "parameters": {
                    "action_path": "REQUIRED: Full path to the Input Action"
                },
                "example": 'manage_enhanced_input(action="action_get_properties", action_path="/Game/Input/Actions/IA_Jump.IA_Jump")'
            },
            "action_configure": {
                "description": "Configure a property on an Input Action",
                "parameters": {
                    "action_path": "REQUIRED: Full path to the Input Action",
                    "property_name": "REQUIRED: Property to configure",
                    "property_value": "REQUIRED: New value for the property"
                },
                "example": 'manage_enhanced_input(action="action_configure", action_path="/Game/Input/Actions/IA_Jump.IA_Jump", property_name="bConsumeInput", property_value="true")'
            },
            # Mapping service
            "mapping_create_context": {
                "description": "Create a new Input Mapping Context",
                "parameters": {
                    "context_name": "REQUIRED: Name for the context (e.g., 'IMC_Default')",
                    "asset_path": "REQUIRED: Content path (e.g., '/Game/Input')",
                    "priority": "Optional: Priority level (default: 0)"
                },
                "example": 'manage_enhanced_input(action="mapping_create_context", context_name="IMC_Combat", asset_path="/Game/Input")'
            },
            "mapping_list_contexts": {
                "description": "List all Input Mapping Contexts",
                "parameters": {},
                "example": 'manage_enhanced_input(action="mapping_list_contexts")'
            },
            "mapping_add_key_mapping": {
                "description": "Add a key binding to a mapping context",
                "parameters": {
                    "context_path": "REQUIRED: Full path to the Mapping Context",
                    "action_path": "REQUIRED: Full path to the Input Action",
                    "key": "REQUIRED: Key name (e.g., 'E', 'SpaceBar', 'LeftMouseButton')"
                },
                "example": 'manage_enhanced_input(action="mapping_add_key_mapping", context_path="/Game/Input/IMC_Default.IMC_Default", action_path="/Game/Input/Actions/IA_Jump.IA_Jump", key="SpaceBar")'
            },
            "mapping_get_mappings": {
                "description": "Get all key mappings in a context",
                "parameters": {
                    "context_path": "REQUIRED: Full path to the Mapping Context"
                },
                "example": 'manage_enhanced_input(action="mapping_get_mappings", context_path="/Game/Input/IMC_Default.IMC_Default")'
            },
            "mapping_add_modifier": {
                "description": "Add a modifier to a key mapping",
                "parameters": {
                    "context_path": "REQUIRED: Full path to the Mapping Context",
                    "mapping_index": "Index of the mapping (default: 0)",
                    "modifier_type": "REQUIRED: Modifier type (e.g., 'Negate', 'Swizzle', 'DeadZone', 'Scalar')"
                },
                "example": 'manage_enhanced_input(action="mapping_add_modifier", context_path="/Game/Input/IMC_Default.IMC_Default", mapping_index=0, modifier_type="Negate")'
            },
            "mapping_add_trigger": {
                "description": "Add a trigger to a key mapping",
                "parameters": {
                    "context_path": "REQUIRED: Full path to the Mapping Context",
                    "mapping_index": "Index of the mapping (default: 0)",
                    "trigger_type": "REQUIRED: Trigger type (e.g., 'Pressed', 'Released', 'Hold', 'Tap')"
                },
                "example": 'manage_enhanced_input(action="mapping_add_trigger", context_path="/Game/Input/IMC_Default.IMC_Default", mapping_index=0, trigger_type="Pressed")'
            },
            "mapping_get_available_keys": {
                "description": "List all available input keys",
                "parameters": {},
                "example": 'manage_enhanced_input(action="mapping_get_available_keys")'
            },
            "mapping_get_available_modifier_types": {
                "description": "List all available modifier types",
                "parameters": {},
                "example": 'manage_enhanced_input(action="mapping_get_available_modifier_types")'
            },
            "mapping_get_available_trigger_types": {
                "description": "List all available trigger types",
                "parameters": {},
                "example": 'manage_enhanced_input(action="mapping_get_available_trigger_types")'
            }
        },
        "note": "For deleting Input Actions or Mapping Contexts, use manage_asset(action='delete', asset_path='...'). This tool supports 25+ actions across 6 services."
    },
    
    "manage_level_actors": {
        "summary": "Level actor operations including add/remove, transforms, properties, and hierarchy management",
        "topic": "level-actors",
        "actions": {
            "add": {
                "description": "Add/spawn a new actor to the level",
                "parameters": {
                    "actor_class": "REQUIRED: Class path (e.g., '/Script/Engine.PointLight', '/Script/Engine.SpotLight', '/Script/Engine.StaticMeshActor')",
                    "actor_label": "Optional: Display name for the actor",
                    "location": "Optional: Spawn location as [x, y, z]",
                    "extra.spawn_rotation": "Optional: Spawn rotation as [pitch, yaw, roll]",
                    "extra.spawn_scale": "Optional: Spawn scale as [x, y, z]",
                    "extra.tags": "Optional: List of actor tags"
                },
                "example": 'manage_level_actors(action="add", actor_class="/Script/Engine.SpotLight", location=[0, 0, 200], actor_label="MyLight")'
            },
            "remove": {
                "description": "Remove an actor from the level",
                "parameters": {
                    "actor_label": "Label/name of the actor to remove",
                    "actor_path": "Or full path to the actor",
                    "extra.with_undo": "Whether removal can be undone (default: True)"
                },
                "example": 'manage_level_actors(action="remove", actor_label="PointLight1")'
            },
            "list": {
                "description": "List all actors in the level",
                "parameters": {
                    "extra.class_filter": "Optional: Filter by class name",
                    "extra.label_filter": "Optional: Filter by label substring",
                    "extra.selected_only": "Only return selected actors",
                    "extra.max_results": "Maximum results to return (default: 100)"
                },
                "example": 'manage_level_actors(action="list", extra={"class_filter": "PointLight"})'
            },
            "find": {
                "description": "Find actors matching criteria",
                "parameters": {
                    "actor_label": "Label to search for",
                    "extra.class_filter": "Optional: Filter by class",
                    "extra.required_tags": "Optional: List of required tags",
                    "extra.max_results": "Maximum results (default: 100)"
                },
                "example": 'manage_level_actors(action="find", actor_label="Light")'
            },
            "get_info": {
                "description": "Get detailed information about an actor",
                "parameters": {
                    "actor_label": "Label of the actor",
                    "actor_path": "Or full path to the actor",
                    "extra.include_components": "Include component info (default: True)",
                    "extra.include_properties": "Include property info (default: True)"
                },
                "example": 'manage_level_actors(action="get_info", actor_label="PointLight1")'
            },
            "set_transform": {
                "description": "Set the full transform (location, rotation, scale) of an actor",
                "parameters": {
                    "actor_label": "Label of the actor",
                    "location": "New location as [x, y, z]",
                    "rotation": "New rotation as [pitch, yaw, roll]",
                    "scale": "New scale as [x, y, z]",
                    "extra.world_space": "Use world space (default: True)"
                },
                "example": 'manage_level_actors(action="set_transform", actor_label="Cube1", location=[100, 200, 0], rotation=[0, 45, 0])'
            },
            "set_location": {
                "description": "Set only the location of an actor",
                "parameters": {
                    "actor_label": "Label of the actor",
                    "location": "REQUIRED: New location as [x, y, z]",
                    "extra.world_space": "Use world space (default: True)"
                },
                "example": 'manage_level_actors(action="set_location", actor_label="Cube1", location=[100, 200, 0])'
            },
            "set_rotation": {
                "description": "Set only the rotation of an actor",
                "parameters": {
                    "actor_label": "Label of the actor",
                    "rotation": "REQUIRED: New rotation as [pitch, yaw, roll]"
                },
                "example": 'manage_level_actors(action="set_rotation", actor_label="Cube1", rotation=[0, 45, 0])'
            },
            "set_scale": {
                "description": "Set only the scale of an actor",
                "parameters": {
                    "actor_label": "Label of the actor",
                    "scale": "REQUIRED: New scale as [x, y, z]"
                },
                "example": 'manage_level_actors(action="set_scale", actor_label="Cube1", scale=[2, 2, 2])'
            },
            "focus": {
                "description": "Move the VIEWPORT/CAMERA to focus on an actor (does NOT move the actor)",
                "parameters": {
                    "actor_label": "Label of the actor to focus on",
                    "extra.instant": "Instant focus without animation (default: False)"
                },
                "example": 'manage_level_actors(action="focus", actor_label="PlayerStart")'
            },
            "move_to_view": {
                "description": "Move the ACTOR to the current viewport camera location (opposite of focus)",
                "parameters": {
                    "actor_label": "Label of the actor to move"
                },
                "example": 'manage_level_actors(action="move_to_view", actor_label="PointLight1")'
            },
            "get_property": {
                "description": "Get a property value from an actor",
                "parameters": {
                    "actor_label": "Label of the actor",
                    "property_path": "Path to the property"
                },
                "example": 'manage_level_actors(action="get_property", actor_label="PointLight1", property_path="Intensity")'
            },
            "set_property": {
                "description": "Set a property value on an actor",
                "parameters": {
                    "actor_label": "Label of the actor",
                    "property_path": "Path to the property",
                    "property_value": "Value to set"
                },
                "example": 'manage_level_actors(action="set_property", actor_label="PointLight1", property_path="Intensity", property_value="5000")'
            },
            "select": {
                "description": "Select an actor in the editor",
                "parameters": {
                    "actor_label": "Label of the actor to select",
                    "extra.add_to_selection": "Add to current selection (default: False)",
                    "extra.deselect": "Deselect instead of select",
                    "extra.deselect_all": "Deselect all actors first"
                },
                "example": 'manage_level_actors(action="select", actor_label="Cube1")'
            },
            "rename": {
                "description": "Rename an actor's label",
                "parameters": {
                    "actor_label": "Current label of the actor",
                    "extra.new_label": "New label to assign"
                },
                "example": 'manage_level_actors(action="rename", actor_label="Cube1", extra={"new_label": "MainCube"})'
            },
            "help": {
                "description": "Show help for this tool or a specific action",
                "parameters": {
                    "help_action": "Optional: specific action to get help for"
                },
                "example": 'manage_level_actors(action="help") or manage_level_actors(action="help", help_action="add")'
            }
        },
        "note": "This tool supports 21 actions across 4 phases. Use action='help' for complete action list. IMPORTANT: 'focus' moves camera TO actor, 'move_to_view' moves actor TO camera."
    },
    
    "manage_material": {
        "summary": "Material and Material Instance management including creation, properties, and parameters",
        "topic": "material-management",
        "actions": {
            "help": {
                "description": "Show help for this tool or a specific action",
                "parameters": {
                    "help_action": "Optional: specific action to get help for"
                },
                "example": 'manage_material(action="help") or manage_material(action="help", help_action="create")'
            }
        },
        "note": "This tool supports 24 actions (11 base material + 13 instance). Use action='help' for complete action list."
    },
    
    "manage_material_node": {
        "summary": "Material graph node (expression) operations including discovery, creation, connections, and parameters",
        "topic": "material-node-tools",
        "actions": {
            "help": {
                "description": "Show help for this tool or a specific action",
                "parameters": {
                    "help_action": "Optional: specific action to get help for"
                },
                "example": 'manage_material_node(action="help") or manage_material_node(action="help", help_action="create")'
            }
        },
        "note": "This tool supports 21 actions for material graph operations. Use action='help' for complete action list."
    },
    
    "manage_umg_widget": {
        "summary": "UMG Widget Blueprint management including components, properties, and events",
        "topic": "umg-guide",
        "actions": {
            "help": {
                "description": "Show help for this tool or a specific action",
                "parameters": {
                    "help_action": "Optional: specific action to get help for"
                },
                "example": 'manage_umg_widget(action="help") or manage_umg_widget(action="help", help_action="add_component")'
            }
        },
        "note": "This tool supports 11 actions for UMG widget operations. Use action='help' for complete action list."
    }
}


def generate_help_response(tool_name: str, help_action: Optional[str] = None) -> Dict[str, Any]:
    """
    Generate standardized help response for a tool.
    
    Args:
        tool_name: Name of the tool (e.g., "manage_asset")
        help_action: Optional specific action to get help for
        
    Returns:
        Dict with help information
    """
    if tool_name not in TOOL_HELP:
        return {
            "success": False,
            "error": f"No help available for tool '{tool_name}'"
        }
    
    tool_help = TOOL_HELP[tool_name]
    
    # If specific action requested, return detailed action help
    if help_action:
        if help_action not in tool_help["actions"]:
            available_actions = list(tool_help["actions"].keys())
            return {
                "success": False,
                "error": f"Action '{help_action}' not found",
                "available_actions": available_actions,
                "suggestion": f"Use action='help' without help_action to see all available actions"
            }
        
        action_info = tool_help["actions"][help_action]
        return {
            "success": True,
            "tool": tool_name,
            "action": help_action,
            "description": action_info["description"],
            "parameters": action_info.get("parameters", {}),
            "example": action_info.get("example", ""),
            "help_type": "action_detail"
        }
    
    # Return overview of all actions
    action_list = []
    for action_name, action_info in tool_help["actions"].items():
        action_list.append({
            "action": action_name,
            "description": action_info["description"]
        })
    
    return {
        "success": True,
        "tool": tool_name,
        "summary": tool_help["summary"],
        "topic": tool_help.get("topic", ""),
        "topic_help": f"For detailed examples and workflows, use get_help(topic='{tool_help.get('topic', '')}'))" if tool_help.get("topic") else "",
        "actions": action_list,
        "total_actions": len(action_list),
        "usage": f"For detailed help on a specific action: {tool_name}(action='help', help_action='action_name')",
        "note": tool_help.get("note", ""),
        "help_type": "tool_overview"
    }


def generate_error_response(tool_name: str, action: str, error_message: str, missing_params: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    Generate a helpful error response that includes:
    - The error message
    - The correct format/example for the action
    - A reminder to use the help system
    
    This should be used by ALL VibeUE MCP tools when returning errors.
    """
    response = {
        "success": False,
        "error": error_message,
        "action": action,
        "tool": tool_name,
    }
    
    # Add missing parameters if specified
    if missing_params:
        response["missing_parameters"] = missing_params
    
    # Try to get the correct example from help system
    if tool_name in TOOL_HELP and action in TOOL_HELP[tool_name].get("actions", {}):
        action_help = TOOL_HELP[tool_name]["actions"][action]
        response["correct_format"] = action_help.get("example", "")
        response["required_parameters"] = action_help.get("parameters", {})
        response["description"] = action_help.get("description", "")
    
    # Always add the help reminder
    response["help_tip"] = f"Use {tool_name}(action='help', help_action='{action}') to see all parameters and examples for this action."
    response["general_help"] = f"Use {tool_name}(action='help') to see all available actions."
    
    return response


def get_required_params_for_action(tool_name: str, action: str) -> List[str]:
    """
    Get a list of required parameters for a given action.
    Returns empty list if tool/action not found.
    """
    if tool_name not in TOOL_HELP:
        return []
    
    actions = TOOL_HELP[tool_name].get("actions", {})
    if action not in actions:
        return []
    
    # Return all parameters (the help system doesn't distinguish required vs optional currently)
    return list(actions[action].get("parameters", {}).keys())
