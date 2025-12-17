"""
Tool Help System

Centralized help content and formatting for all VibeUE MCP tools.
Each tool can use this to provide consistent help information via the 'help' action.
"""

from typing import Dict, Any, List, Optional
from pathlib import Path

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
                "description": "Compare properties between two components (same or different blueprints)",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint containing the first component",
                    "component_name": "First component name to compare",
                    "other_component": "Second component name to compare with",
                    "other_blueprint": "Optional: Blueprint path for second component (defaults to same blueprint)"
                },
                "example": 'manage_blueprint_component(action="compare_properties", blueprint_name="/Game/Blueprints/BP_Lamp", component_name="MainLight", other_component="FillLight")',
                "cross_blueprint_example": 'manage_blueprint_component(action="compare_properties", blueprint_name="/Game/Blueprints/BP_Lamp1", component_name="Light", other_blueprint="/Game/Blueprints/BP_Lamp2", other_component="Light")'
            }
        }
    },
    
    "manage_blueprint_variable": {
        "summary": "Blueprint variable management including creation, deletion, and property access",
        "topic": "multi-action-tools",
        "actions": {
            "search_types": {
                "description": "Search for available variable types using reflection. ALWAYS use this before create to find correct type_path.",
                "parameters": {
                    "search_text": "Text to search for in type names (e.g., 'Float', 'Int', 'Vector')",
                    "max_results": "Maximum number of results"
                },
                "example": 'manage_blueprint_variable(action="search_types", blueprint_name="any", search_text="Float")'
            },
            "create": {
                "description": "Create a new variable in a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "variable_name": "Name for the new variable",
                    "variable_config": "Config object with type_path and options",
                    "type_path": "Type path - use search_types to discover, or aliases: 'float', 'double', 'int', 'int64', 'bool', 'string', 'name', 'text', 'byte'",
                    "default_value": "Optional default value",
                    "category": "Optional category name",
                    "is_replicated": "Whether variable replicates over network",
                    "tooltip": "Optional tooltip description"
                },
                "example": 'manage_blueprint_variable(action="create", blueprint_name="/Game/Blueprints/BP_Player", variable_name="Health", variable_config={"type_path": "float", "default_value": 100.0})'
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
                "description": "Get a variable's current default value. The property_path is the variable name itself.",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "property_path": "The variable name to read (e.g., 'Health', 'MaxHealth', 'IsAlive')"
                },
                "example": 'manage_blueprint_variable(action="get_property", blueprint_name="/Game/Blueprints/BP_Player", property_path="Health")'
            },
            "set_property": {
                "description": "Set a variable's default value. The property_path is the variable name, value is the new default.",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "property_path": "The variable name to set (e.g., 'Health', 'MaxHealth', 'IsAlive')",
                    "value": "The new default value (number, string, boolean, etc.)"
                },
                "example": 'manage_blueprint_variable(action="set_property", blueprint_name="/Game/Blueprints/BP_Player", property_path="Health", value=100)'
            },
            "modify": {
                "description": "Modify a variable's type or properties",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "variable_name": "Name of the variable to modify",
                    "new_type_path": "New type path if changing type",
                    "new_name": "New name if renaming"
                },
                "example": 'manage_blueprint_variable(action="modify", blueprint_name="/Game/Blueprints/BP_Player", variable_name="Health", new_name="PlayerHealth")'
            },
            "diagnostics": {
                "description": "Get diagnostic information about variable issues",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "variable_name": "Optional: specific variable to diagnose"
                },
                "example": 'manage_blueprint_variable(action="diagnostics", blueprint_name="/Game/Blueprints/BP_Player")'
            },
            "get_property_metadata": {
                "description": "Get metadata about a variable property",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "variable_name": "Name of the variable",
                    "property_path": "Path to the property metadata"
                },
                "example": 'manage_blueprint_variable(action="get_property_metadata", blueprint_name="/Game/Blueprints/BP_Player", variable_name="Health", property_path="Category")'
            },
            "set_property_metadata": {
                "description": "Set metadata on a variable property",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "variable_name": "Name of the variable",
                    "property_path": "Path to the property metadata",
                    "value": "Value to set"
                },
                "example": 'manage_blueprint_variable(action="set_property_metadata", blueprint_name="/Game/Blueprints/BP_Player", variable_name="Health", property_path="ReplicationCondition", value="Always")'
            }
        }
    },
    
    "manage_blueprint_node": {
        "summary": "Blueprint node/graph operations including discovery, creation, connections, and configuration",
        "topic": "node-tools",
        "actions": {
            "discover": {
                "description": "Discover available Blueprint nodes with spawner keys for exact creation. IMPORTANT: Basic math operators (+, -, *, /) are NOT available through discovery - they use special internal nodes that cannot be created programmatically.",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint (REQUIRED)",
                    "search_term": "Simple keywords like 'print', 'branch', 'FTrunc' (REQUIRED). DO NOT use function signatures like 'KismetMathLibrary::Multiply'",
                    "max_results": "Maximum number of results (default: 10)",
                    "category": "Optional category filter - use simple names like 'Math', 'Math|Float', 'Utilities', 'Flow Control'. DO NOT pass function names as category"
                },
                "example": 'manage_blueprint_node(action="discover", blueprint_name="/Game/Blueprints/MyBP", search_term="print")',
                "important_notes": [
                    "Basic math operators (multiply, add, subtract, divide floats) DO NOT EXIST in Blueprint discovery",
                    "If search returns 0 results with a hint, STOP searching - do not loop with variations",
                    "Valid categories: Math, Math|Float, Math|Integer, Flow Control, Utilities, Development, Game",
                    "Use spawner_key from results to create nodes with action='create'"
                ]
            },
            "create": {
                "description": "Create a new node in the Event Graph or function graph",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "spawner_key": "Exact spawner key from discover action",
                    "position": "[x, y] position in graph as array",
                    "function_name": "Optional: function name to create node in function graph",
                    "graph_scope": "Optional: 'event' (default) or 'function'"
                },
                "example": 'manage_blueprint_node(action="create", blueprint_name="/Game/Blueprints/BP_Player", spawner_key="K2_CallFunction_PrintString", position=[100, 200])'
            },
            "delete": {
                "description": "Delete a node from the Event Graph or function graph",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_id": "GUID of the node to delete",
                    "function_name": "Optional: function name if node is in function graph"
                },
                "example": 'manage_blueprint_node(action="delete", blueprint_name="/Game/Blueprints/BP_Player", node_id="ABC123")'
            },
            "move": {
                "description": "Move a node to a new position",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_id": "GUID of the node",
                    "position": "[x, y] new position as array",
                    "function_name": "Optional: function name if node is in function graph"
                },
                "example": 'manage_blueprint_node(action="move", blueprint_name="/Game/Blueprints/BP_Player", node_id="ABC123", position=[500, 300])'
            },
            "list": {
                "description": "List all nodes in the Event Graph or function graph",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "function_name": "Optional: function name to list nodes in function graph",
                    "graph_scope": "Optional: 'event' (default) or 'function'"
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
                "description": "Configure node properties or pin default values (alias for set_property)",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_id": "GUID of the node",
                    "property_name": "Name of the property or pin to set",
                    "property_value": "Value to set",
                    "function_name": "Optional: function name if node is in function graph"
                },
                "example": 'manage_blueprint_node(action="configure", blueprint_name="/Game/Blueprints/BP_Player", node_id="ABC123", property_name="InString", property_value="Hello World")'
            },
            "get_details": {
                "description": "Get detailed information about a specific node instance (alias: details)",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_id": "GUID of the node",
                    "function_name": "Optional: function name if node is in function graph"
                },
                "example": 'manage_blueprint_node(action="get_details", blueprint_name="/Game/Blueprints/BP_Player", node_id="ABC123")'
            },
            "split": {
                "description": "Split a struct pin into its individual components",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_id": "GUID of the node",
                    "property_name": "Name of the struct pin to split"
                },
                "example": 'manage_blueprint_node(action="split", blueprint_name="/Game/Blueprints/BP_Player", node_id="ABC123", property_name="Location")'
            },
            "recombine": {
                "description": "Recombine a split struct pin",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_id": "GUID of the node",
                    "property_name": "Name of the struct pin to recombine"
                },
                "example": 'manage_blueprint_node(action="recombine", blueprint_name="/Game/Blueprints/BP_Player", node_id="ABC123", property_name="Location")'
            },
            "refresh_node": {
                "description": "Refresh a single node to update its structure",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_id": "GUID of the node"
                },
                "example": 'manage_blueprint_node(action="refresh_node", blueprint_name="/Game/Blueprints/BP_Player", node_id="ABC123")'
            },
            "refresh_nodes": {
                "description": "Refresh all nodes in the Event Graph",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint"
                },
                "example": 'manage_blueprint_node(action="refresh_nodes", blueprint_name="/Game/Blueprints/BP_Player")'
            },
            "connect": {
                "description": "Connect two node pins together",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "source_node_id": "GUID of the source node",
                    "source_pin": "Name of the source pin (e.g., 'ReturnValue', 'then', 'execute')",
                    "target_node_id": "GUID of the target node",
                    "target_pin": "Name of the target pin (e.g., 'self', 'execute', input pin name)",
                    "function_name": "Optional: function name if nodes are in function graph"
                },
                "example": 'manage_blueprint_node(action="connect", blueprint_name="/Game/Blueprints/BP_Player", source_node_id="ABC123", source_pin="then", target_node_id="DEF456", target_pin="execute")'
            },
            "disconnect": {
                "description": "Disconnect a node pin from all its connections",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_id": "GUID of the node",
                    "source_pin": "Optional: Name of specific pin to disconnect (disconnects all if omitted)",
                    "function_name": "Optional: function name if node is in function graph"
                },
                "example": 'manage_blueprint_node(action="disconnect", blueprint_name="/Game/Blueprints/BP_Player", node_id="ABC123", source_pin="execute")'
            },
            "set_property": {
                "description": "Set a property value on a node (alias: configure)",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "node_id": "GUID of the node",
                    "property_name": "Name of the property or pin default to set",
                    "property_value": "Value to set",
                    "function_name": "Optional: function name if node is in function graph"
                },
                "example": 'manage_blueprint_node(action="set_property", blueprint_name="/Game/Blueprints/BP_Player", node_id="ABC123", property_name="DefaultValue", property_value="100")'
            },
            "list_custom_events": {
                "description": "List all custom events in the Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint"
                },
                "example": 'manage_blueprint_node(action="list_custom_events", blueprint_name="/Game/Blueprints/BP_Player")'
            },
            "create_component_event": {
                "description": "Create an event node for a component event",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "extra.component_name": "Name of the component",
                    "extra.event_name": "Name of the event to create"
                },
                "example": 'manage_blueprint_node(action="create_component_event", blueprint_name="/Game/Blueprints/BP_Player", extra={"component_name": "Collision", "event_name": "OnComponentBeginOverlap"})'
            },
            "discover_component_events": {
                "description": "Discover available events for a component",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "extra.component_name": "Name of the component"
                },
                "example": 'manage_blueprint_node(action="discover_component_events", blueprint_name="/Game/Blueprints/BP_Player", extra={"component_name": "Collision"})'
            },
            "discover_input_keys": {
                "description": "Discover available input keys for input actions",
                "parameters": {},
                "example": 'manage_blueprint_node(action="discover_input_keys")'
            },
            "create_input_key": {
                "description": "Create an input action/axis event node",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "extra.input_key": "Input key name (e.g., 'SpaceBar')",
                    "extra.input_event": "Event type (Pressed, Released, etc.)"
                },
                "example": 'manage_blueprint_node(action="create_input_key", blueprint_name="/Game/Blueprints/BP_Player", extra={"input_key": "SpaceBar", "input_event": "Pressed"})'
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
            },
            "create": {
                "description": "Create a new function in a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "function_name": "Name for the new function"
                },
                "example": 'manage_blueprint_function(action="create", blueprint_name="/Game/Blueprints/BP_Player", function_name="CalculateDamage")'
            },
            "delete": {
                "description": "Delete a function from a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "function_name": "Name of the function to delete"
                },
                "example": 'manage_blueprint_function(action="delete", blueprint_name="/Game/Blueprints/BP_Player", function_name="CalculateDamage")'
            },
            "list": {
                "description": "List all functions in a Blueprint",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint"
                },
                "example": 'manage_blueprint_function(action="list", blueprint_name="/Game/Blueprints/BP_Player")'
            },
            "list_params": {
                "description": "List all parameters of a function",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "function_name": "Name of the function"
                },
                "example": 'manage_blueprint_function(action="list_params", blueprint_name="/Game/Blueprints/BP_Player", function_name="CalculateDamage")'
            },
            "add_param": {
                "description": "Add a parameter to a function",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "function_name": "Name of the function",
                    "param_name": "Name for the new parameter",
                    "type": "Type path (e.g., 'float', 'int32', '/Script/CoreUObject.FloatProperty')",
                    "direction": "Parameter direction: 'input'/'in' or 'output'/'out'"
                },
                "example": 'manage_blueprint_function(action="add_param", blueprint_name="/Game/Blueprints/BP_Player", function_name="CalculateDamage", param_name="BaseDamage", type="float", direction="input")'
            },
            "remove_param": {
                "description": "Remove a parameter from a function",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "function_name": "Name of the function",
                    "param_name": "Name of the parameter to remove"
                },
                "example": 'manage_blueprint_function(action="remove_param", blueprint_name="/Game/Blueprints/BP_Player", function_name="CalculateDamage", param_name="BaseDamage")'
            },
            "update_param": {
                "description": "Update a function parameter (rename or change type)",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "function_name": "Name of the function",
                    "param_name": "Current name of the parameter",
                    "new_name": "New name for the parameter (optional)",
                    "new_type": "New type path (optional)"
                },
                "example": 'manage_blueprint_function(action="update_param", blueprint_name="/Game/Blueprints/BP_Player", function_name="CalculateDamage", param_name="BaseDamage", new_type="int")'
            },
            "add_local_var": {
                "description": "Add a local variable to a function",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "function_name": "Name of the function",
                    "local_name": "Name for the local variable",
                    "type": "Type path for the variable (e.g., 'float', 'int', 'bool')"
                },
                "example": 'manage_blueprint_function(action="add_local_var", blueprint_name="/Game/Blueprints/BP_Player", function_name="CalculateDamage", local_name="TempDamage", type="float")'
            },
            "remove_local_var": {
                "description": "Remove a local variable from a function",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "function_name": "Name of the function",
                    "local_name": "Name of the local variable to remove"
                },
                "example": 'manage_blueprint_function(action="remove_local_var", blueprint_name="/Game/Blueprints/BP_Player", function_name="CalculateDamage", local_name="TempDamage")'
            },
            "update_local_var": {
                "description": "Update a local variable in a function (change type, rename, or modify properties)",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "function_name": "Name of the function",
                    "local_name": "Current name of the local variable (or use param_name)",
                    "new_type": "New type path (e.g., 'float', 'int', 'bool')",
                    "new_name": "New name for the variable (optional)",
                    "default_value": "Default value for the variable (optional)"
                },
                "example": 'manage_blueprint_function(action="update_local_var", blueprint_name="/Game/Blueprints/BP_Player", function_name="CalculateDamage", local_name="TempDamage", new_type="int")'
            },
            "list_local_vars": {
                "description": "List all local variables in a function",
                "parameters": {
                    "blueprint_name": "Full path to the Blueprint",
                    "function_name": "Name of the function"
                },
                "example": 'manage_blueprint_function(action="list_local_vars", blueprint_name="/Game/Blueprints/BP_Player", function_name="CalculateDamage")'
            }
        },
        "note": "This tool supports 11 actions for function lifecycle management. Use action='help' for complete action list."
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
                    "context_path": "REQUIRED: Destination folder (e.g., '/Game/Input')",
                    "priority": "Optional: Priority level (default: 0)"
                },
                "example": 'manage_enhanced_input(action="mapping_create_context", context_name="IMC_Combat", context_path="/Game/Input")'
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
            "mapping_remove_mapping": {
                "description": "Remove a key mapping from a context by index",
                "parameters": {
                    "context_path": "REQUIRED: Full path to the Mapping Context",
                    "mapping_index": "REQUIRED: Index of the mapping to remove (use mapping_get_mappings to find the index)"
                },
                "example": 'manage_enhanced_input(action="mapping_remove_mapping", context_path="/Game/Input/IMC_Default.IMC_Default", mapping_index=4)'
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
            "mapping_remove_modifier": {
                "description": "Remove a modifier from a key mapping by index",
                "parameters": {
                    "context_path": "REQUIRED: Full path to the Mapping Context",
                    "mapping_index": "Index of the mapping (default: 0)",
                    "modifier_index": "REQUIRED: Index of the modifier to remove"
                },
                "example": 'manage_enhanced_input(action="mapping_remove_modifier", context_path="/Game/Input/IMC_Default.IMC_Default", mapping_index=0, modifier_index=0)'
            },
            "mapping_get_modifiers": {
                "description": "Get all modifiers for a key mapping",
                "parameters": {
                    "context_path": "REQUIRED: Full path to the Mapping Context",
                    "mapping_index": "Index of the mapping (default: 0)"
                },
                "example": 'manage_enhanced_input(action="mapping_get_modifiers", context_path="/Game/Input/IMC_Default.IMC_Default", mapping_index=0)'
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
            "mapping_remove_trigger": {
                "description": "Remove a trigger from a key mapping by index",
                "parameters": {
                    "context_path": "REQUIRED: Full path to the Mapping Context",
                    "mapping_index": "Index of the mapping (default: 0)",
                    "trigger_index": "REQUIRED: Index of the trigger to remove"
                },
                "example": 'manage_enhanced_input(action="mapping_remove_trigger", context_path="/Game/Input/IMC_Default.IMC_Default", mapping_index=0, trigger_index=0)'
            },
            "mapping_get_triggers": {
                "description": "Get all triggers for a key mapping",
                "parameters": {
                    "context_path": "REQUIRED: Full path to the Mapping Context",
                    "mapping_index": "Index of the mapping (default: 0)"
                },
                "example": 'manage_enhanced_input(action="mapping_get_triggers", context_path="/Game/Input/IMC_Default.IMC_Default", mapping_index=0)'
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
            "get_transform": {
                "description": "Get the current transform (location, rotation, scale) of an actor",
                "parameters": {
                    "actor_label": "Label of the actor",
                    "actor_path": "Or full path to the actor"
                },
                "example": 'manage_level_actors(action="get_transform", actor_label="Cube1")'
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
            "refresh_viewport": {
                "description": "Refresh the viewport to update visual changes",
                "parameters": {},
                "example": 'manage_level_actors(action="refresh_viewport")'
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
            "get_all_properties": {
                "description": "Get all properties of an actor",
                "parameters": {
                    "actor_label": "Label of the actor",
                    "actor_path": "Or full path to the actor"
                },
                "example": 'manage_level_actors(action="get_all_properties", actor_label="PointLight1")'
            },
            "set_folder": {
                "description": "Set the folder path for an actor in the World Outliner",
                "parameters": {
                    "actor_label": "Label of the actor",
                    "extra.folder_path": "Folder path (e.g., 'Lighting/Main')"
                },
                "example": 'manage_level_actors(action="set_folder", actor_label="PointLight1", extra={"folder_path": "Lighting/Main"})'
            },
            "attach": {
                "description": "Attach an actor to a parent actor",
                "parameters": {
                    "actor_label": "Label of the actor to attach",
                    "extra.parent_label": "Label of the parent actor",
                    "extra.socket_name": "Optional: Socket name to attach to"
                },
                "example": 'manage_level_actors(action="attach", actor_label="Light1", extra={"parent_label": "Lamp"})'
            },
            "detach": {
                "description": "Detach an actor from its parent",
                "parameters": {
                    "actor_label": "Label of the actor to detach"
                },
                "example": 'manage_level_actors(action="detach", actor_label="Light1")'
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
            },
            "create": {
                "description": "Create a new material asset",
                "parameters": {
                    "material_name": "Name for the new material",
                    "destination_path": "Package path (e.g., /Game/Materials)"
                },
                "example": 'manage_material(action="create", material_name="M_MyMaterial", destination_path="/Game/Materials")'
            },
            "get_info": {
                "description": "Get comprehensive material information",
                "parameters": {
                    "material_path": "Full path to the material"
                },
                "example": 'manage_material(action="get_info", material_path="/Game/Materials/M_Base")'
            },
            "list_properties": {
                "description": "List all editable properties",
                "parameters": {
                    "material_path": "Full path to the material",
                    "include_advanced": "Optional: Include advanced properties (default false)"
                },
                "example": 'manage_material(action="list_properties", material_path="/Game/Materials/M_Base", include_advanced=True)'
            },
            "get_property": {
                "description": "Get a property value",
                "parameters": {
                    "material_path": "Full path to the material",
                    "property_name": "Name of the property"
                },
                "example": 'manage_material(action="get_property", material_path="/Game/Materials/M_Base", property_name="TwoSided")'
            },
            "set_property": {
                "description": "Set a property value",
                "parameters": {
                    "material_path": "Full path to the material",
                    "property_name": "Name of the property",
                    "property_value": "New value for the property"
                },
                "example": 'manage_material(action="set_property", material_path="/Game/Materials/M_Base", property_name="TwoSided", property_value=True)'
            },
            "set_properties": {
                "description": "Set multiple properties at once",
                "parameters": {
                    "material_path": "Full path to the material",
                    "properties": "Dict of property_name: value pairs"
                },
                "example": 'manage_material(action="set_properties", material_path="/Game/Materials/M_Base", properties={"TwoSided": True, "BlendMode": "BLEND_Masked"})'
            },
            "get_property_info": {
                "description": "Get detailed property metadata",
                "parameters": {
                    "material_path": "Full path to the material",
                    "property_name": "Name of the property"
                },
                "example": 'manage_material(action="get_property_info", material_path="/Game/Materials/M_Base", property_name="BlendMode")'
            },
            "list_parameters": {
                "description": "List all material parameters",
                "parameters": {
                    "material_path": "Full path to the material"
                },
                "example": 'manage_material(action="list_parameters", material_path="/Game/Materials/M_Base")'
            },
            "get_parameter": {
                "description": "Get a specific parameter",
                "parameters": {
                    "material_path": "Full path to the material",
                    "parameter_name": "Name of the parameter"
                },
                "example": 'manage_material(action="get_parameter", material_path="/Game/Materials/M_Base", parameter_name="Roughness")'
            },
            "set_parameter_default": {
                "description": "Set a parameter's default value",
                "parameters": {
                    "material_path": "Full path to the material",
                    "parameter_name": "Name of the parameter",
                    "value": "New default value"
                },
                "example": 'manage_material(action="set_parameter_default", material_path="/Game/Materials/M_Base", parameter_name="Roughness", value=0.75)'
            },
            "compile": {
                "description": "Recompile material shaders",
                "parameters": {
                    "material_path": "Full path to the material"
                },
                "example": 'manage_material(action="compile", material_path="/Game/Materials/M_Base")'
            },
            "save": {
                "description": "Save material to disk",
                "parameters": {
                    "material_path": "Full path to the material"
                },
                "example": 'manage_material(action="save", material_path="/Game/Materials/M_Base")'
            },
            "refresh_editor": {
                "description": "Refresh open Material Editor",
                "parameters": {
                    "material_path": "Full path to the material"
                },
                "example": 'manage_material(action="refresh_editor", material_path="/Game/Materials/M_Base")'
            },
            "create_instance": {
                "description": "Create a material instance from a parent material",
                "parameters": {
                    "parent_material_path": "Full path to parent material",
                    "instance_name": "Name for the new instance",
                    "destination_path": "Package path (e.g., /Game/Materials)"
                },
                "example": 'manage_material(action="create_instance", parent_material_path="/Game/Materials/M_Base", instance_name="MI_Red", destination_path="/Game/Materials")'
            },
            "get_instance_info": {
                "description": "Get comprehensive info about a material instance",
                "parameters": {
                    "instance_path": "Full path to the material instance"
                },
                "example": 'manage_material(action="get_instance_info", instance_path="/Game/Materials/MI_Red")'
            },
            "list_instance_properties": {
                "description": "List all editable properties on a material instance",
                "parameters": {
                    "instance_path": "Full path to the material instance"
                },
                "example": 'manage_material(action="list_instance_properties", instance_path="/Game/Materials/MI_Red")'
            },
            "get_instance_property": {
                "description": "Get a single property value from instance",
                "parameters": {
                    "instance_path": "Full path to the material instance",
                    "property_name": "Name of the property"
                },
                "example": 'manage_material(action="get_instance_property", instance_path="/Game/Materials/MI_Red", property_name="PhysMaterial")'
            },
            "set_instance_property": {
                "description": "Set a property on material instance",
                "parameters": {
                    "instance_path": "Full path to the material instance",
                    "property_name": "Name of the property",
                    "property_value": "New value"
                },
                "example": 'manage_material(action="set_instance_property", instance_path="/Game/Materials/MI_Red", property_name="PhysMaterial", property_value="/Game/Physics/PM_Metal")'
            },
            "list_instance_parameters": {
                "description": "List all parameters with current/default values",
                "parameters": {
                    "instance_path": "Full path to the material instance"
                },
                "example": 'manage_material(action="list_instance_parameters", instance_path="/Game/Materials/MI_Red")'
            },
            "set_instance_scalar_parameter": {
                "description": "Set a scalar parameter override",
                "parameters": {
                    "instance_path": "Full path to the material instance",
                    "parameter_name": "Name of the parameter",
                    "value": "Scalar value"
                },
                "example": 'manage_material(action="set_instance_scalar_parameter", instance_path="/Game/Materials/MI_Red", parameter_name="Roughness", value=0.8)'
            },
            "set_instance_vector_parameter": {
                "description": "Set a vector/color parameter override",
                "parameters": {
                    "instance_path": "Full path to the material instance",
                    "parameter_name": "Name of the parameter",
                    "r": "Red component (0-1)",
                    "g": "Green component (0-1)",
                    "b": "Blue component (0-1)",
                    "a": "Optional: Alpha component (0-1, default 1.0)"
                },
                "example": 'manage_material(action="set_instance_vector_parameter", instance_path="/Game/Materials/MI_Red", parameter_name="BaseColor", r=1.0, g=0.0, b=0.0, a=1.0)'
            },
            "set_instance_texture_parameter": {
                "description": "Set a texture parameter override",
                "parameters": {
                    "instance_path": "Full path to the material instance",
                    "parameter_name": "Name of the parameter",
                    "texture_path": "Full path to texture asset"
                },
                "example": 'manage_material(action="set_instance_texture_parameter", instance_path="/Game/Materials/MI_Red", parameter_name="BaseColorTexture", texture_path="/Game/Textures/T_Red")'
            },
            "clear_instance_parameter_override": {
                "description": "Remove parameter override, revert to parent",
                "parameters": {
                    "instance_path": "Full path to the material instance",
                    "parameter_name": "Name of the parameter"
                },
                "example": 'manage_material(action="clear_instance_parameter_override", instance_path="/Game/Materials/MI_Red", parameter_name="Roughness")'
            },
            "save_instance": {
                "description": "Save material instance to disk",
                "parameters": {
                    "instance_path": "Full path to the material instance"
                },
                "example": 'manage_material(action="save_instance", instance_path="/Game/Materials/MI_Red")'
            }
        },
        "note": "This tool supports 24 actions (11 base material + 13 instance). Base actions work with materials, instance actions work with material instances (MIC)."
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
            },
            "list_components": {
                "description": "List all components in a widget blueprint hierarchy",
                "parameters": {
                    "widget_name": "REQUIRED: Full widget path (e.g., /Game/UI/WBP_MainMenu)",
                    "options": "Optional dictionary for future filters"
                },
                "example": 'manage_umg_widget(action="list_components", widget_name="/Game/UI/WBP_MainMenu")'
            },
            "add_component": {
                "description": "Add a widget component to the hierarchy",
                "parameters": {
                    "widget_name": "REQUIRED: Full widget path",
                    "component_type": "REQUIRED: Widget class (Button, TextBlock, Image, etc.)",
                    "component_name": "REQUIRED: Unique name for the new component",
                    "parent_name": "Parent component name (default root)",
                    "is_variable": "Whether to expose as variable (default True)",
                    "properties": "Optional property dict applied on creation"
                },
                "example": 'manage_umg_widget(action="add_component", widget_name="/Game/UI/WBP_MainMenu", component_type="Button", component_name="PlayButton", parent_name="root")'
            },
            "remove_component": {
                "description": "Remove a component from the widget hierarchy",
                "parameters": {
                    "widget_name": "REQUIRED: Full widget path",
                    "component_name": "REQUIRED: Name of component to remove",
                    "remove_children": "Also remove child components (default True)",
                    "remove_from_variables": "Remove backing variable (default True)"
                },
                "example": 'manage_umg_widget(action="remove_component", widget_name="/Game/UI/WBP_MainMenu", component_name="TitleText")'
            },
            "validate": {
                "description": "Run consistency checks on a widget hierarchy",
                "parameters": {
                    "widget_name": "REQUIRED: Full widget path"
                },
                "example": 'manage_umg_widget(action="validate", widget_name="/Game/UI/WBP_MainMenu")'
            },
            "search_types": {
                "description": "Discover available widget types for creation",
                "parameters": {
                    "category": "Optional category filter (Common, Panels, etc.)",
                    "search_text": "Optional substring match against type/display name",
                    "include_custom": "Include project widgets (default True)",
                    "include_engine": "Include engine widgets (default True)",
                    "parent_compatibility": "Filter by compatible parent class"
                },
                "example": 'manage_umg_widget(action="search_types", category="Common", search_text="Button")'
            },
            "get_component_properties": {
                "description": "Get editable property metadata for a component",
                "parameters": {
                    "widget_name": "REQUIRED: Full widget path",
                    "component_name": "REQUIRED: Component to inspect"
                },
                "example": 'manage_umg_widget(action="get_component_properties", widget_name="/Game/UI/WBP_MainMenu", component_name="PlayButton")'
            },
            "get_property": {
                "description": "Read a specific property from a component",
                "parameters": {
                    "widget_name": "REQUIRED: Full widget path",
                    "component_name": "REQUIRED: Component name",
                    "property_name": "REQUIRED: Property path (e.g., ColorAndOpacity)"
                },
                "example": 'manage_umg_widget(action="get_property", widget_name="/Game/UI/WBP_MainMenu", component_name="PlayButton", property_name="ColorAndOpacity")'
            },
            "set_property": {
                "description": "Set a component property or slot property. NOTE: Slot properties vary by parent panel type - CanvasPanel uses Slot.LayoutData.Anchors, Overlay/Box use Slot.HorizontalAlignment",
                "parameters": {
                    "widget_name": "REQUIRED: Full widget path",
                    "component_name": "REQUIRED: Component name",
                    "property_name": "REQUIRED: Property path. For CanvasPanel children use Slot.LayoutData.Anchors.Minimum/Maximum. For Overlay/Box children use Slot.HorizontalAlignment/VerticalAlignment.",
                    "property_value": "REQUIRED: Value to assign (numbers, bools, dicts, strings)",
                    "property_type": "Optional override for value coercion (auto by default)"
                },
                "slot_properties_by_panel": {
                    "CanvasPanel": ["Slot.LayoutData.Anchors.Minimum", "Slot.LayoutData.Anchors.Maximum", "Slot.LayoutData.Offsets", "Slot.LayoutData.Alignment", "Slot.bAutoSize", "Slot.ZOrder"],
                    "Overlay": ["Slot.HorizontalAlignment", "Slot.VerticalAlignment", "Slot.Padding"],
                    "HorizontalBox/VerticalBox": ["Slot.HorizontalAlignment", "Slot.VerticalAlignment", "Slot.Padding", "Slot.Size.SizeRule", "Slot.Size.Value"],
                    "ScrollBox": ["Slot.HorizontalAlignment", "Slot.VerticalAlignment", "Slot.Padding"]
                },
                "example": 'manage_umg_widget(action="set_property", widget_name="/Game/UI/WBP_MainMenu", component_name="PlayButton", property_name="ColorAndOpacity", property_value={"R":1,"G":0.5,"B":0.1,"A":1})'
            },
            "list_properties": {
                "description": "List properties for a component including categories",
                "parameters": {
                    "widget_name": "REQUIRED: Full widget path",
                    "component_name": "REQUIRED: Component name",
                    "include_inherited": "Include inherited properties (default True)",
                    "category_filter": "Optional category filter"
                },
                "example": 'manage_umg_widget(action="list_properties", widget_name="/Game/UI/WBP_MainMenu", component_name="PlayButton", include_inherited=False)'
            },
            "get_available_events": {
                "description": "List bindable input events for a component",
                "parameters": {
                    "widget_name": "REQUIRED: Full widget path",
                    "component_name": "REQUIRED: Component name"
                },
                "example": 'manage_umg_widget(action="get_available_events", widget_name="/Game/UI/WBP_MainMenu", component_name="PlayButton")'
            },
            "bind_events": {
                "description": "Bind widget delegate events to blueprint functions",
                "parameters": {
                    "widget_name": "REQUIRED: Full widget path",
                    "component_name": "REQUIRED: Component generating events",
                    "input_events": "REQUIRED: Dict mapping event names to handler function paths"
                },
                "example": 'manage_umg_widget(action="bind_events", widget_name="/Game/UI/WBP_MainMenu", component_name="PlayButton", input_events={"OnClicked": "BP_MainMenu_C::HandlePlay"})'
            }
        },
        "note": "This tool supports 11 actions for UMG widget operations. Use action='help' for complete action list."
    }
}


TOOL_ACTION_OVERRIDES = {
    "manage_material_node": [
        "help",
        "discover_types", "get_categories",
        "create", "delete", "move", "list", "get_details", "get_pins",
        "connect", "disconnect", "connect_to_output", "disconnect_output", "list_connections",
        "get_property", "set_property", "list_properties",
        "promote_to_parameter", "create_parameter", "set_parameter_metadata",
        "get_output_properties", "get_output_connections"
    ]
}


_TOOLS_DIR = Path(__file__).resolve().parent
_RESOURCES_DIR = _TOOLS_DIR.parent / "resources"
_TOPICS_DIR = _RESOURCES_DIR / "topics"
_TOPIC_CACHE: Dict[str, Dict[str, Any]] = {}


def _load_topic_content(topic: str) -> Dict[str, Any]:
    """Load markdown content for a help topic from resources/topics."""
    if topic in _TOPIC_CACHE:
        return _TOPIC_CACHE[topic]
    topic_path = _TOPICS_DIR / f"{topic}.md"
    if topic_path.exists():
        try:
            content = topic_path.read_text(encoding="utf-8")
            data = {"success": True, "content": content, "path": str(topic_path)}
        except Exception as exc:
            data = {"success": False, "error": f"Failed to read topic '{topic}': {exc}"}
    else:
        data = {"success": False, "error": f"Topic file not found for '{topic}'"}
    _TOPIC_CACHE[topic] = data
    return data


def _get_available_actions(tool_name: str) -> List[str]:
    """Return complete action list for a tool, honoring overrides when needed."""
    if tool_name in TOOL_ACTION_OVERRIDES:
        return TOOL_ACTION_OVERRIDES[tool_name]
    if tool_name in TOOL_HELP:
        actions = list(TOOL_HELP[tool_name].get("actions", {}).keys())
        return actions
    return []


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
        available_actions = _get_available_actions(tool_name)
        
        # Check if action exists in detailed help
        if help_action in tool_help["actions"]:
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
        
        # Check if action exists in overrides (valid action but no detailed help)
        if help_action in available_actions:
            topic_name = tool_help.get("topic")
            response = {
                "success": True,
                "tool": tool_name,
                "action": help_action,
                "description": f"Action '{help_action}' is available. Detailed parameter help not documented yet.",
                "suggestion": f"Try the action directly or see the tool description for parameter hints.",
                "help_type": "action_brief"
            }
            # Include topic content if available
            if topic_name:
                topic_result = _load_topic_content(topic_name)
                if topic_result.get("success"):
                    response["topic_guide"] = topic_result.get("content", "")
            return response
        
        # Action not found anywhere
        return {
            "success": False,
            "error": f"Action '{help_action}' not found",
            "available_actions": available_actions,
            "suggestion": f"Use action='help' without help_action to see all available actions"
        }
    
    # Return overview of all actions
    # Use overrides if available to get the complete action list
    all_actions = _get_available_actions(tool_name)
    action_list = []
    for action_name in all_actions:
        if action_name in tool_help["actions"]:
            action_info = tool_help["actions"][action_name]
            action_list.append({
                "action": action_name,
                "description": action_info["description"]
            })
        else:
            # Action exists but no detailed help - add with brief description
            action_list.append({
                "action": action_name,
                "description": f"(Use help_action='{action_name}' for topic guide)"
            })
    
    response = {
        "success": True,
        "tool": tool_name,
        "summary": tool_help["summary"],
        "topic": tool_help.get("topic", ""),
        "actions": action_list,
        "total_actions": len(action_list),
        "usage": f"For detailed help on a specific action: {tool_name}(action='help', help_action='action_name')",
        "note": tool_help.get("note", ""),
        "help_type": "tool_overview"
    }
    topic_name = tool_help.get("topic")
    if topic_name:
        topic_result = _load_topic_content(topic_name)
        if topic_result.get("success"):
            response["topic_help"] = f"Topic '{topic_name}' guide loaded from documentation."
            response["topic_content"] = topic_result.get("content", "")
            response["topic_source"] = topic_result.get("path", "")
        else:
            response["topic_help"] = topic_result.get("error", "")
    else:
        response["topic_help"] = ""
    return response


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

    # Topic-specific guidance (e.g., UMG styling guide)
    topic_name = TOOL_HELP.get(tool_name, {}).get("topic") if tool_name in TOOL_HELP else None
    if topic_name:
        if topic_name == "umg-guide":
            response["topic_help"] = "UMG styling guide available. Run manage_umg_widget(action='help') to load the complete guide." 
            topic_result = _load_topic_content(topic_name)
            if topic_result.get("success"):
                response["topic_content_preview"] = topic_result.get("content", "")[:600]
            else:
                response["topic_content_error"] = topic_result.get("error", "")
        else:
            response["topic_help"] = f"Additional guidance available under topic '{topic_name}'."
    
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
