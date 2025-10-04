"""
Blueprint Tools for Unreal MCP.

This module provides tools for creating and manipulating Blueprint assets in Unreal Engine.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_blueprint_tools(mcp: FastMCP):
    """Register Blueprint tools with the MCP server."""
    
    @mcp.tool()
    def create_blueprint(
        ctx: Context,
        name: str,
        parent_class: str
    ) -> Dict[str, Any]:
        """Create a new Blueprint class.
        
        ⚠️ CRITICAL DEPENDENCY ORDER: After creating the Blueprint, you MUST create elements in this order:
        1) Variables FIRST - Create all Blueprint variables before any Event Graph nodes
        2) Components SECOND - Add all components and configure hierarchy  
        3) Functions THIRD - Implement all custom functions
        4) Event Graph nodes LAST - Create logic that references the above elements
        
        This order prevents dependency failures and ensures proper Blueprint compilation.
        """
        # Import inside function to avoid circular imports
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("create_blueprint", {
                "name": name,
                "parent_class": parent_class
            })
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Blueprint creation response: {response}")
            
            # Add dependency order reminder to response
            result = response or {}
            if result.get("path") or result.get("name"):  # If Blueprint was created successfully
                logger.info("⚠️ REMINDER: Create Blueprint elements in DEPENDENCY ORDER: 1) Variables FIRST, 2) Components SECOND, 3) Functions THIRD, 4) Event Graph nodes LAST")
                # Try different field names to see what gets through
                result["reminder"] = "Create in order: Variables → Components → Functions → Event Graph"
                result["critical_order"] = "Variables FIRST, then Components, then Functions, then Event Graph LAST"
            
            return result
            
        except Exception as e:
            error_msg = f"Error creating blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    # --- Blueprint variable management -------------------------------------------------

    @mcp.tool()
    def compile_blueprint(
        ctx: Context,
        blueprint_name: str
    ) -> Dict[str, Any]:
        """Compile a Blueprint."""
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name
            }
            
            logger.info(f"Compiling blueprint: {blueprint_name}")
            response = unreal.send_command("compile_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Compile blueprint response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error compiling blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_blueprint_property(
        ctx: Context,
        blueprint_name: str,
        property_name: str,
        property_value
    ) -> Dict[str, Any]:
        """
        Set a property on a Blueprint class default object.
        
        Args:
            blueprint_name: Name of the target Blueprint
            property_name: Name of the property to set
            property_value: Value to set the property to
            
        Returns:
            Response indicating success or failure
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name,
                "property_name": property_name,
                "property_value": property_value
            }
            
            logger.info(f"Setting blueprint property with params: {params}")
            response = unreal.send_command("set_blueprint_property", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set blueprint property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting blueprint property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}


    @mcp.tool()
    def get_blueprint_info(
        ctx: Context,
        blueprint_name: str
    ) -> Dict[str, Any]:
        """
        Get comprehensive information about any Blueprint using reflection.
        
        🔍 **UNIVERSAL BLUEPRINT INSPECTOR**: Get complete information about any Blueprint type
        - Regular Blueprints, Widget Blueprints, Actor Blueprints, etc.
        - Variables, Components, UMG Components, Functions, Events, Properties
        - All information gathered through reflection, not hardcoded
        
        Args:
            blueprint_name: Name or full path of the Blueprint to inspect. Can be:
                          - Full asset path (e.g., "/Game/Blueprints/BP_Player")
                          - Package path (e.g., "/Game/Blueprints/BP_Player.BP_Player") 
                          - Blueprint name only (e.g., "BP_Player") - slower search
                          
        Returns:
            Dict containing:
            - success: boolean indicating if inspection completed
            - blueprint_info: comprehensive blueprint metadata including:
              - name: blueprint name
              - path: full asset path  
              - blueprint_type: UBlueprint, UWidgetBlueprint, etc.
              - parent_class: base class
              - is_widget_blueprint: boolean flag
              - variables: array of all Blueprint variables with types and metadata
              - components: array of all components (mesh, camera, light, etc.)
              - widget_components: array of UMG widgets (if widget blueprint)
              - functions: array of Blueprint functions
              - event_graphs: array of event graphs with node statistics
              - blueprint_properties: array of Class Default Object properties
            - error: string (only if success=false)
        
        🎯 **Comprehensive Information Includes**:
        
        **Variables**: Type, container type, category, editability, flags
        **Components**: Name, type, parent relationships, native status  
        **Widget Components**: Name, type, parent, visibility, enabled state
        **Functions**: Name, node count, graph type
        **Event Graphs**: Node counts by type (events, function calls, variables)
        **Properties**: Class default properties with metadata
        
        💡 **Usage Examples**:
        ```python
        # Get info for any Blueprint type
        get_blueprint_info("BP_Player")        # Actor Blueprint
        get_blueprint_info("WBP_MainMenu")     # Widget Blueprint  
        get_blueprint_info("BP_GameMode")      # GameMode Blueprint
        
        # Use full paths for best performance
        get_blueprint_info("/Game/Blueprints/BP_Player")
        ```
        
        ⚡ **Performance**: Use full paths for instant loading, partial names trigger expensive searches
        🔄 **Replaces**: get_widget_blueprint_info - this works for ALL Blueprint types
        """
        from vibe_ue_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name
            }

            logger.info(f"Getting comprehensive blueprint info for '{blueprint_name}'")
            response = unreal.send_command("get_blueprint_info", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Get blueprint info response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error getting blueprint info: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def reparent_blueprint(
        ctx: Context,
        blueprint_name: str,
        new_parent_class: str
    ) -> Dict[str, Any]:
        """
        Reparent a Blueprint to a new parent class with automatic hierarchy and inheritance fixes.
        
        🔧 **CRITICAL BLUEPRINT CREATION FIX**: This tool solves common Blueprint creation issues:
        - **Parent Class Problems**: Fix Blueprints that inherited wrong parent class during creation
        - **Component Hierarchy Issues**: Reparenting often resolves component hierarchy problems automatically
        - **Custom Class Inheritance**: Enable inheritance from custom C++ classes or other Blueprints
        - **Blueprint Migration**: Change Blueprint inheritance without losing existing structure
        
        💡 **Common Use Cases**:
        - Fix Blueprint created with "Actor" when it should inherit from custom character class
        - Change from default parent to specialized parent (Character → MyCustomCharacter)  
        - Migrate Blueprint from one base class to another during development
        - Resolve component hierarchy issues that appear after Blueprint creation
        
        🎯 **Integration Benefits**:
        - **Component Auto-Fix**: Often automatically resolves component parent-child relationships
        - **Property Preservation**: Maintains existing variables and components during reparent
        - **Compilation Safety**: Ensures Blueprint remains compilable after parent class change
        - **Development Workflow**: Essential for iterative Blueprint development and refactoring
        
        Args:
            blueprint_name: Name or path of the Blueprint to reparent
                           Examples: "BP_Player", "/Game/Blueprints/Characters/BP_Player"
            new_parent_class: Name of the new parent class
                             Examples: "ProteusCharacter", "Character", "Pawn", "Actor", "UserWidget"
                             Supports: Engine classes, custom C++ classes, other Blueprint classes
            
        Returns:
            Dict containing:
            - success: boolean indicating if reparenting completed successfully
            - blueprint_name: name of the Blueprint that was reparented
            - old_parent_class: previous parent class (for verification)
            - new_parent_class: new parent class that was applied
            - message: detailed result information
            - additional fixes: any automatic fixes applied (component hierarchy, etc.)
            
        ⚠️ **AI Best Practices**:
        1. Use this immediately after create_blueprint() if wrong parent class detected
        2. Verify result with get_blueprint_info() to confirm parent class change
        3. Check component hierarchy - often gets automatically fixed
        4. Compile Blueprint after reparenting to ensure no errors
        5. Essential tool for Blueprint creation workflows and troubleshooting
        
        🔄 **Workflow Integration**:
        ```
        create_blueprint() → Wrong parent detected → reparent_blueprint() → Verify with get_blueprint_info()
        ```
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            logger.info(f"Reparenting blueprint '{blueprint_name}' to parent class '{new_parent_class}'")
            
            response = unreal.send_command("reparent_blueprint", {
                "blueprint_name": blueprint_name,
                "new_parent_class": new_parent_class
            })
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Reparenting response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error reparenting blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def manage_blueprint_variables(
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
        🔧 **UNIFIED BLUEPRINT VARIABLE MANAGEMENT SYSTEM** 
        
        **🎯 SOLVES THE BLUEPRINT CHALLENGE TYPE ISSUES**
        This tool replaces all individual variable tools with a unified, reflection-based system
        that properly handles UserWidget, NiagaraSystem, SoundBase, and Blueprint class types.
        
        **📋 Available Actions:**
        
        **🆕 create** - Create new Blueprint variable with proper typing
        ```python
        # REQUIRED: Use search_types action first to find the exact type_path
        type_search = manage_blueprint_variables(
            blueprint_name="BP_Player2",
            action="search_types",
            search_criteria={"search_text": "UserWidget"}
        )
        # Result includes type_path like "/Script/UMG.UserWidget"
        
        # Then create variable with exact type_path from search results
        manage_blueprint_variables(
            blueprint_name="BP_Player2",
            action="create", 
            variable_name="AttributeWidget",
            variable_config={
                "type_path": "/Script/UMG.UserWidget",  # ✅ REQUIRED: Canonical type path
                "category": "UI",
                "tooltip": "Player's attribute display widget",
                "is_editable": True,
                "default_value": None
            }
        )
        ```
        
        **🔍 search_types** - Discover available variable types (200+ vs old 27)
        ```python
        manage_blueprint_variables(
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
        
        **ℹ️ get_info** - Get detailed variable information
        **🗑️ delete** - Remove variables with reference checking  
        **📝 get_property** - Get nested property values from complex variables
        **✏️ set_property** - Set nested property values (arrays, maps, structs)
        **📋 list** - List all variables with filtering (FUTURE)
        **🔧 modify** - Modify existing variable config (FUTURE)
        
        **🎯 Blueprint Challenge Solution:**
        This tool fixes the exact variable type issues blocking challenge completion:
        - AttributeWidget → UserWidget (type_path: "/Script/UMG.UserWidget") ✅
        - Death_Niagara_System → NiagaraSystem (type_path: "/Script/Niagara.NiagaraSystem") ✅  
        - Death_Sound → SoundBase (type_path: "/Script/Engine.SoundBase") ✅
        - Microsub HUD → BP_MicrosubHUD_C (type_path: "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C") ✅
        - ExplosionForce → float (type_path: "/Script/CoreUObject.FloatProperty") ✅
        - Loading Start Delay → float (type_path: "/Script/CoreUObject.FloatProperty") ✅
        
        **📌 Common Type Paths Reference:**
        
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
        
        **💡 Best Practice Workflow:**
        1. Use `search_types` action to find exact type_path for complex types
        2. Use canonical type_path in `create` action's variable_config
        3. For Blueprint classes, use full package path with _C suffix
        
        **📋 Parameters:**
        - blueprint_name: Target Blueprint name
        - action: Operation to perform (create|delete|get_info|get_property|set_property|search_types)
        - variable_name: Variable name (required for most actions)
        - variable_config: Variable configuration for create action, MUST include:
          - **type_path** (REQUIRED): Canonical type path (e.g., "/Script/UMG.UserWidget")
            Use search_types action first to find the correct type_path
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
                
            response = unreal.send_command("manage_blueprint_variables", params)
            
            if not response:
                logger.error("No response from Unreal Engine for manage_blueprint_variables")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Unified variable management response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error in unified variable management: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def manage_blueprint_components(
        ctx: Context,
        blueprint_name: str,
        action: str,
        # Component identification
        component_type: str = "",
        component_name: str = "",
        # Property operations
        property_name: str = "",
        property_value: Any = None,
        # Component creation parameters
        parent_name: str = "",
        properties: Dict[str, Any] = None,
        location: List[float] = None,
        rotation: List[float] = None,
        scale: List[float] = None,
        # Hierarchy operations
        component_order: List[str] = None,
        remove_children: bool = True,
        # Discovery parameters
        category: str = "",
        base_class: str = "",
        search_text: str = "",
        include_abstract: bool = False,
        include_deprecated: bool = False,
        include_property_values: bool = False,
        include_inherited: bool = True,
        # Additional options
        options: Dict[str, Any] = None
    ) -> Dict[str, Any]:
        """
        🔧 **UNIFIED BLUEPRINT COMPONENT MANAGEMENT SYSTEM**
        
        Consolidates all Blueprint component operations into a single multi-action tool
        following the successful patterns of manage_blueprint_function and manage_blueprint_variables.
        
        **📋 Available Actions:**
        
        ## Discovery & Inspection
        
        **search_types** - Discover available component types
        ```python
        manage_blueprint_components(
            blueprint_name="",  # Not required for type search
            action="search_types",
            category="Rendering",
            search_text="Light"
        )
        ```
        
        **get_info** - Get comprehensive component type information
        ```python
        # Get type metadata only
        manage_blueprint_components(
            blueprint_name="",
            action="get_info",
            component_type="SpotLightComponent"
        )
        
        # Get metadata + actual property values from instance
        manage_blueprint_components(
            blueprint_name="BP_Player",
            action="get_info",
            component_type="SpotLightComponent",
            component_name="SpotLight_Top",
            include_property_values=True
        )
        ```
        
        **get_property_metadata** - Get detailed property metadata
        ```python
        manage_blueprint_components(
            blueprint_name="",
            action="get_property_metadata",
            component_type="SpotLightComponent",
            property_name="Intensity"
        )
        ```
        
        **list** - List all components in Blueprint
        ```python
        manage_blueprint_components(
            blueprint_name="BP_Player",
            action="list"
        )
        ```
        
        ## Component Lifecycle
        
        **create** - Add new component to Blueprint
        ```python
        manage_blueprint_components(
            blueprint_name="BP_Player2",
            action="create",
            component_type="SpotLightComponent",
            component_name="SpotLight_Top",
            parent_name="BoxCollision",
            location=[0, 0, 100],
            rotation=[0, -90, 0],
            properties={"Intensity": 5000}
        )
        ```
        
        **delete** - Remove component from Blueprint
        ```python
        manage_blueprint_components(
            blueprint_name="BP_Player2",
            action="delete",
            component_name="SpotLight_Top",
            remove_children=True
        )
        ```
        
        ## Property Management
        
        **get_property** - Get single property value from component instance
        ```python
        result = manage_blueprint_components(
            blueprint_name="BP_Player",
            action="get_property",
            component_name="SpotLight_Top",
            property_name="Intensity"
        )
        # Returns: {"value": 5000.0, "type": "float"}
        ```
        
        **set_property** - Set component property value
        ```python
        manage_blueprint_components(
            blueprint_name="BP_Player2",
            action="set_property",
            component_name="SpotLight_Top",
            property_name="Intensity",
            property_value=5000.0
        )
        ```
        
        **get_all_properties** - Get all property values from component
        ```python
        result = manage_blueprint_components(
            blueprint_name="BP_Player",
            action="get_all_properties",
            component_name="SpotLight_Top",
            include_inherited=True
        )
        # Returns: {"properties": {"Intensity": 5000.0, "InnerConeAngle": 0.0, ...}}
        ```
        
        **compare_properties** - Compare component properties between Blueprints
        ```python
        result = manage_blueprint_components(
            blueprint_name="BP_Player2",
            action="compare_properties",
            component_name="SpotLight_Top",
            options={
                "compare_to_blueprint": "BP_Player",
                "compare_to_component": "SpotLight_Top"
            }
        )
        # Returns: {"matches": True/False, "differences": [...], "matching_count": 45}
        ```
        
        ## Hierarchy Operations
        
        **reorder** - Change component order
        ```python
        manage_blueprint_components(
            blueprint_name="BP_Player2",
            action="reorder",
            component_order=["SpotLight_Top", "SpotLight_Right", "TrailVFX"]
        )
        ```
        
        **reparent** - Change component's parent attachment
        ```python
        manage_blueprint_components(
            blueprint_name="BP_Player2",
            action="reparent",
            component_name="SpotLight_Top",
            parent_name="CameraBoom"
        )
        ```
        
        **💡 CRITICAL PROPERTY NAMING DISCOVERIES:**
        
        **SkeletalMesh Component:**
        - ✅ Use `SkeletalMeshAsset` or `SkinnedAsset` for mesh asset (NOT `SkeletalMesh`)
        - ✅ Use `OverrideMaterials` for material array
        - ⚠️ UI may require Blueprint tab close/reopen to refresh after property changes
        
        **Common Property Names:**
        - Lights: `Intensity`, `LightColor`, `AttenuationRadius`, `CastShadows`
        - SpotLights: `InnerConeAngle`, `OuterConeAngle`
        - Transforms: `RelativeLocation`, `RelativeRotation`, `RelativeScale`
        - Niagara: `Asset` for NiagaraSystem reference
        
        **⚠️ REPLACES LEGACY TOOLS:**
        - get_available_components() → action="search_types"
        - get_component_info() → action="get_info"
        - get_property_metadata() → action="get_property_metadata"
        - get_component_hierarchy() → action="list"
        - add_component() → action="create"
        - remove_component() → action="delete"
        - set_component_property() → action="set_property"
        - reorder_components() → action="reorder"
        
        Args:
            blueprint_name: Name of target Blueprint (empty for type discovery)
            action: Action to perform (see above for available actions)
            component_type: Component class name (for get_info, create actions)
            component_name: Component instance name (for property/delete/reparent actions)
            property_name: Property name (for get_property, set_property actions)
            property_value: Value to set (for set_property action)
            parent_name: Parent component name (for create, reparent actions)
            properties: Initial properties dict (for create action)
            location: [X, Y, Z] transform (for create action)
            rotation: [Pitch, Yaw, Roll] transform (for create action)
            scale: [X, Y, Z] transform (for create action)
            component_order: Ordered list of component names (for reorder action)
            remove_children: Whether to remove children (for delete action)
            category: Filter by category (for search_types action)
            base_class: Filter by base class (for search_types action)
            search_text: Text search filter (for search_types action)
            include_abstract: Include abstract types (for search_types action)
            include_deprecated: Include deprecated types (for search_types action)
            include_property_values: Include actual values (for get_info action)
            include_inherited: Include inherited properties (for get_all_properties action)
            options: Additional action-specific options dict
            
        Returns:
            Dict containing action results with success field and action-specific data
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            # Validate action
            valid_actions = [
                "search_types", "get_info", "get_property_metadata", "list",
                "create", "delete",
                "get_property", "set_property", "get_all_properties", "compare_properties",
                "reorder", "reparent"
            ]
            
            if action not in valid_actions:
                return {
                    "success": False,
                    "message": f"Invalid action '{action}'. Valid actions: {', '.join(valid_actions)}"
                }
            
            # Route to appropriate legacy tool or new implementation
            # This provides backward compatibility while consolidating the interface
            
            if action == "search_types":
                # Route to get_available_components
                params = {
                    "category": category,
                    "base_class": base_class,
                    "search_text": search_text,
                    "include_abstract": include_abstract,
                    "include_deprecated": include_deprecated
                }
                response = unreal.send_command("get_available_components", params)
                
            elif action == "get_info":
                # Route to get_component_info with optional property values
                params = {
                    "component_type": component_type,
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "include_property_values": include_property_values
                }
                response = unreal.send_command("get_component_info", params)
                
            elif action == "get_property_metadata":
                # Route to get_property_metadata
                params = {
                    "component_type": component_type,
                    "property_name": property_name
                }
                response = unreal.send_command("get_property_metadata", params)
                
            elif action == "list":
                # Route to get_component_hierarchy
                params = {"blueprint_name": blueprint_name}
                response = unreal.send_command("get_component_hierarchy", params)
                
            elif action == "create":
                # Route to add_component
                params = {
                    "blueprint_name": blueprint_name,
                    "component_type": component_type,
                    "component_name": component_name
                }
                if parent_name:
                    params["parent_name"] = parent_name
                if properties:
                    params["properties"] = properties
                if location:
                    params["location"] = [float(x) for x in location]
                if rotation:
                    params["rotation"] = [float(x) for x in rotation]
                if scale:
                    params["scale"] = [float(x) for x in scale]
                response = unreal.send_command("add_component", params)
                
            elif action == "delete":
                # Route to remove_component
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "remove_children": remove_children
                }
                response = unreal.send_command("remove_component", params)
                
            elif action == "get_property":
                # NEW: Get single property value
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "property_name": property_name
                }
                response = unreal.send_command("get_component_property", params)
                
            elif action == "set_property":
                # Route to set_component_property
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "property_name": property_name,
                    "property_value": property_value
                }
                response = unreal.send_command("set_component_property", params)
                
            elif action == "get_all_properties":
                # NEW: Get all property values from component
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "include_inherited": include_inherited
                }
                response = unreal.send_command("get_all_component_properties", params)
                
            elif action == "compare_properties":
                # NEW: Compare properties between components
                if not options or "compare_to_blueprint" not in options or "compare_to_component" not in options:
                    return {
                        "success": False,
                        "message": "compare_properties requires options with 'compare_to_blueprint' and 'compare_to_component'"
                    }
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "compare_to_blueprint": options["compare_to_blueprint"],
                    "compare_to_component": options["compare_to_component"]
                }
                response = unreal.send_command("compare_component_properties", params)
                
            elif action == "reorder":
                # Route to reorder_components
                params = {
                    "blueprint_name": blueprint_name,
                    "component_order": component_order
                }
                response = unreal.send_command("reorder_components", params)
                
            elif action == "reparent":
                # NEW: Reparent component
                params = {
                    "blueprint_name": blueprint_name,
                    "component_name": component_name,
                    "parent_name": parent_name
                }
                response = unreal.send_command("reparent_component", params)
            
            else:
                return {"success": False, "message": f"Action '{action}' not yet implemented"}
            
            if not response:
                logger.error(f"No response from Unreal Engine for component action: {action}")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Component management action '{action}' completed successfully")
            return response
            
        except Exception as e:
            error_msg = f"Error in component management (action={action}): {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    logger.info("Blueprint tools registered successfully (including NEW unified manage_blueprint_components)") 
