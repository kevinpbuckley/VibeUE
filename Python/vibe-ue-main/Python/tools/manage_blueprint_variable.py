"""
Blueprint Variable Management Tool

This module provides unified variable management for Blueprint assets.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_blueprint_variable_tools(mcp: FastMCP):
    """Register Blueprint variable management tools with the MCP server."""
    
    @mcp.tool()
    def manage_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        action: str,
        variable_name: Optional[str] = None,
        variable_config: Optional[Dict[str, Any]] = None,
        property_path: Optional[str] = None,
        value: Optional[Any] = None,
        delete_options: Optional[Dict[str, Any]] = None,
        list_criteria: Optional[Dict[str, Any]] = None,
        info_options: Optional[Dict[str, Any]] = None,
        search_criteria: Optional[Dict[str, Any]] = None,
        options: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """
        UNIFIED BLUEPRINT VARIABLE MANAGEMENT SYSTEM
        
        CRITICAL: variable_config MUST use "type_path" NOT "type"
        WRONG: {"type": "UserWidget"} or {"type": "float"}
        CORRECT: {"type_path": "/Script/UMG.UserWidget"} or {"type_path": "/Script/CoreUObject.FloatProperty"}
        
        SOLVES THE BLUEPRINT CHALLENGE TYPE ISSUES
        This tool replaces all individual variable tools with a unified, reflection-based system
        that properly handles UserWidget, NiagaraSystem, SoundBase, and Blueprint class types.
        
        Available Actions:
        
        **create** - Create new Blueprint variable with proper typing
        ```python
        # REQUIRED: Use search_types action first to find the exact type_path
        type_search = manage_blueprint_variable(
            blueprint_name="BP_Player2",
            action="search_types",
            search_criteria={"search_text": "UserWidget"}
        )
        # Result includes type_path like "/Script/UMG.UserWidget"
        
        # ️ CRITICAL: Use "type_path" key, NOT "type"!
        # Then create variable with exact type_path from search results
        manage_blueprint_variable(
            blueprint_name="BP_Player2",
            action="create", 
            variable_name="AttributeWidget",
            variable_config={
                "type_path": "/Script/UMG.UserWidget",  #  REQUIRED: Use "type_path" NOT "type"
                "category": "UI",
                "tooltip": "Player's attribute display widget",
                "is_editable": True,
                "default_value": None
            }
        )
        ```
        
    **search_types** - Discover available variable types (200+ vs old 27)
        ```python
        manage_blueprint_variable(
            blueprint_name="BP_Player2",
            action="search_types",
            search_criteria={
                "category": "Widget|Audio|Particle|Blueprint|Basic|Struct",
                "search_text": "Widget",
                "include_blueprints": True,
                "include_engine_types": True
            }
        )
        ```
        
        **list** - List all variables with filtering
        ```python
        manage_blueprint_variable(
            blueprint_name="/Game/Blueprints/BP_Player",
            action="list",
            list_criteria={
                "category": "UI",  # Optional: filter by category
                "name_contains": "Health",  # Optional: filter by name substring
                "include_private": True,  # Optional: include private variables
                "include_metadata": False  # Optional: include metadata
            }
        )
        # Returns: {
        #   "success": True,
        #   "variables": [
        #     {
        #       "name": "Health",
        #       "display_type": "Float",
        #       "category": "Combat",
        #       "is_editable": True,
        #       ...
        #     }
        #   ]
        # }
        ```
        
        **ℹ️ get_info** - Get detailed variable information
        **️ delete** - Remove variables with reference checking  
        ** get_property** - Get nested property values from complex variables
        **️ set_property** - Set nested property values (arrays, maps, structs)
        ** modify** - Modify existing variable config (FUTURE)
        
        ** Blueprint Challenge Solution:**
        This tool fixes the exact variable type issues blocking challenge completion:
        - AttributeWidget → UserWidget (type_path: "/Script/UMG.UserWidget") 
        - Death_Niagara_System → NiagaraSystem (type_path: "/Script/Niagara.NiagaraSystem")   
        - Death_Sound → SoundBase (type_path: "/Script/Engine.SoundBase") 
        - Microsub HUD → BP_MicrosubHUD_C (type_path: "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C") 
        - ExplosionForce → float (type_path: "/Script/CoreUObject.FloatProperty") 
        - Loading Start Delay → float (type_path: "/Script/CoreUObject.FloatProperty") 
        
        ** Common Type Paths Reference:**
        
        **Primitives (CRITICAL - Use exact paths):**
        - **float**: "/Script/CoreUObject.FloatProperty"
        - **int**: "/Script/CoreUObject.IntProperty"  
        - **bool**: "/Script/CoreUObject.BoolProperty"
        - **double**: "/Script/CoreUObject.DoubleProperty"
        - **string**: "/Script/CoreUObject.StrProperty"
        - **name**: "/Script/CoreUObject.NameProperty"
        - **byte**: "/Script/CoreUObject.ByteProperty"
        
        **Common Object Types:**
        - **Widgets**: "/Script/UMG.UserWidget"
        - **Niagara**: "/Script/Niagara.NiagaraSystem", "/Script/Niagara.NiagaraComponent"
        - **Audio**: "/Script/Engine.SoundBase", "/Script/Engine.SoundCue", "/Script/Engine.SoundWave"
        - **Actors**: "/Script/Engine.Actor", "/Script/Engine.Pawn", "/Script/Engine.Character"
        - **Components**: "/Script/Engine.SceneComponent", "/Script/Engine.ActorComponent"
        - **Materials**: "/Script/Engine.Material", "/Script/Engine.MaterialInstance"
        - **Textures**: "/Script/Engine.Texture2D", "/Script/Engine.TextureRenderTarget2D"
        
        **Blueprint Classes:**
        - Full asset path with _C suffix: "/Game/Blueprints/UI/BP_MicrosubHUD.BP_MicrosubHUD_C"
        - Use search_items() to find the correct path, then append ".ClassName_C"
        
        ** Best Practice Workflow:**
        1. Use `search_types` action to find exact type_path for complex types
        2. Use canonical type_path in `create` action's variable_config
        3. For Blueprint classes, use full package path with _C suffix
        
        ** Parameters:**
        - blueprint_name: Target Blueprint name
        - action: Operation to perform (create|delete|get_info|get_property|set_property|search_types)
        - variable_name: Variable name (required for most actions)
        - variable_config: Variable configuration dict for create action
          
          ️ **CRITICAL REQUIREMENT**: MUST use "type_path" key, NOT "type"
           WRONG: {"type": "float"} or {"type": "UserWidget"}
           CORRECT: {"type_path": "/Script/CoreUObject.FloatProperty"}
          
          **Required field:**
          - **type_path** (REQUIRED): Canonical type path string (e.g., "/Script/UMG.UserWidget")
            ALWAYS use search_types action first to find the correct type_path
          
          **Optional fields:**
          - category (optional): Variable category for organization
          - tooltip (optional): Description tooltip
          - is_editable (optional): Whether editable in editor (default: True)
          - is_blueprint_readonly (optional): Whether read-only in Blueprint (default: False)
          - is_expose_on_spawn (optional): Expose on spawn (default: False)
          - default_value (optional): Default value for the variable
        - property_path: Dotted path for property operations (e.g., "Health.CurrentValue")
        - value: Value for set_property action
        - search_criteria: Filters for search_types action:
          - search_text: Text to search for in type names
          - category: Filter by category (Widget|Audio|Particle|Blueprint|Basic|Struct)
          - include_blueprints: Include custom Blueprint types (default: True)
          - include_engine_types: Include engine types (default: True)
        - options: Additional options for various actions
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # ️ CRITICAL VALIDATION: Catch common "type" vs "type_path" mistake
            if action == "create" and variable_config:
                if "type" in variable_config and "type_path" not in variable_config:
                    error_msg = (
                        " CRITICAL ERROR: variable_config uses 'type' but should use 'type_path'\n"
                        f" WRONG: variable_config={variable_config}\n"
                        " CORRECT: Use 'type_path' key with full canonical path:\n"
                        "   Examples:\n"
                        '   - {"type_path": "/Script/CoreUObject.FloatProperty"} for float\n'
                        '   - {"type_path": "/Script/UMG.UserWidget"} for widgets\n'
                        '   - {"type_path": "/Script/Engine.SoundBase"} for audio\n'
                        "Use manage_blueprint_variable(action='search_types') to find correct type_path"
                    )
                    logger.error(error_msg)
                    return {"success": False, "message": error_msg}
                
                if "type_path" not in variable_config:
                    error_msg = (
                        " CRITICAL ERROR: variable_config missing required 'type_path' field\n"
                        f"Current variable_config: {variable_config}\n"
                        " REQUIRED: variable_config must include 'type_path' with full canonical path:\n"
                        "   Examples:\n"
                        '   - {"type_path": "/Script/CoreUObject.FloatProperty"} for float\n'
                        '   - {"type_path": "/Script/UMG.UserWidget"} for widgets\n'
                        "Use manage_blueprint_variable(action='search_types') to find correct type_path"
                    )
                    logger.error(error_msg)
                    return {"success": False, "message": error_msg}
            
            # Build command parameters
            params = {
                "action": action,
                "blueprint_name": blueprint_name
            }
            
            # Add optional parameters based on action
            if variable_name:
                params["variable_name"] = variable_name
            if variable_config:
                params["variable_config"] = variable_config
            if property_path:
                params["property_path"] = property_path
                params["path"] = property_path
            if value is not None:
                params["value"] = value
            if delete_options:
                params["delete_options"] = delete_options
            if list_criteria:
                params["list_criteria"] = list_criteria
            if info_options:
                params["info_options"] = info_options
            if search_criteria:
                params["search_criteria"] = search_criteria
            if options:
                params["options"] = options
                
            response = unreal.send_command("manage_blueprint_variable", params)
            
            if not response:
                logger.error("No response from Unreal Engine for manage_blueprint_variable")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Unified variable management response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error in unified variable management: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    logger.info("Blueprint variable management tools registered")
