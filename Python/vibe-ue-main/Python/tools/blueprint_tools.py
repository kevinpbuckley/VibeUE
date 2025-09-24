"""
Blueprint Tools for Unreal MCP.

This module provides tools for creating and manipulating Blueprint assets in Unreal Engine.
"""

import logging
from typing import Dict, List, Any
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
        """Create a new Blueprint class."""
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
            return response or {}
            
        except Exception as e:
            error_msg = f"Error creating blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def get_available_components(
        ctx: Context,
        category: str = "",
        base_class: str = "",
        search_text: str = "",
        include_abstract: bool = False,
        include_deprecated: bool = False
    ) -> Dict[str, Any]:
        """
        Discover all available component types using Unreal's reflection system.
        
        🔍 **100% REFLECTION-BASED DISCOVERY**: Uses TObjectIterator to find all component classes
        with comprehensive metadata including properties, methods, hierarchy rules, and usage examples.
        
        Args:
            category: Filter by component category (optional)
            base_class: Filter by base class (optional)
            search_text: Text to search for in component names (optional)
            include_abstract: Whether to include abstract component types (default: False)
            include_deprecated: Whether to include deprecated component types (default: False)
            
        Returns:
            Dict containing:
            - success: boolean indicating if discovery completed
            - components: array of component objects with comprehensive metadata
            - categories: array of all available categories
            - total_count: total number of components found
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            params = {
                "category": category,
                "base_class": base_class,
                "search_text": search_text,
                "include_abstract": include_abstract,
                "include_deprecated": include_deprecated
            }
            
            response = unreal.send_command("get_available_components", params)
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Found {response.get('total_count', 0)} component types via reflection")
            return response
            
        except Exception as e:
            error_msg = f"Error discovering components: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def get_component_info(
        ctx: Context,
        component_type: str
    ) -> Dict[str, Any]:
        """
        Get comprehensive information about a specific component type using reflection.
        
        🔍 **COMPLETE COMPONENT ANALYSIS**: Extracts all metadata including properties,
        methods, hierarchy rules, compatibility, and usage examples.
        
        Args:
            component_type: Name of the component type to analyze
            
        Returns:
            Dict containing complete component information with reflection-based metadata
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("get_component_info", {
                "component_type": component_type
            })
            
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Retrieved component info for: {component_type}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting component info: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def get_property_metadata(
        ctx: Context,
        component_type: str,
        property_name: str
    ) -> Dict[str, Any]:
        """
        Get detailed metadata for a specific property on a component type.
        
        🔍 **PROPERTY REFLECTION**: Extracts complete property information including
        type, constraints, flags, and metadata from UPROPERTY tags.
        
        Args:
            component_type: Name of the component type
            property_name: Name of the property to analyze
            
        Returns:
            Dict containing comprehensive property metadata
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("get_property_metadata", {
                "component_type": component_type,
                "property_name": property_name
            })
            
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Retrieved property metadata for {component_type}.{property_name}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting property metadata: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def get_component_hierarchy(
        ctx: Context,
        blueprint_name: str
    ) -> Dict[str, Any]:
        """
        Get the component hierarchy of a specific Blueprint.
        
        🏗️ **HIERARCHY ANALYSIS**: Extracts complete component tree with parent-child
        relationships, transforms, and component metadata.
        
        Args:
            blueprint_name: Name of the Blueprint to analyze
            
        Returns:
            Dict containing complete component hierarchy information
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("get_component_hierarchy", {
                "blueprint_name": blueprint_name
            })
            
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Retrieved component hierarchy for Blueprint: {blueprint_name}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting component hierarchy: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_component(
        ctx: Context,
        blueprint_name: str,
        component_type: str,
        component_name: str,
        parent_name: str = "",
        properties: Dict[str, Any] = {},
        location: List[float] = [],
        rotation: List[float] = [],
        scale: List[float] = []
    ) -> Dict[str, Any]:
        """
        Add a component to a Blueprint with full hierarchy and property support.
        
        🔧 **REFLECTION-BASED COMPONENT CREATION**: Uses validated component types and
        supports complete property initialization and hierarchy placement.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_type: Type of component to add (discovered via get_available_components)
            component_name: Name for the new component
            parent_name: Name of parent component for attachment (optional)
            properties: Initial properties to set on the component (optional)
            location: [X, Y, Z] transform location for scene components (optional)
            rotation: [Pitch, Yaw, Roll] transform rotation for scene components (optional)
            scale: [X, Y, Z] transform scale for scene components (optional)
            
        Returns:
            Dict containing information about the created component
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
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
            
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Added component {component_name} ({component_type}) to {blueprint_name}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding component: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_component_property(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        property_name: str,
        property_value: Any
    ) -> Dict[str, Any]:
        """
        Set a property on a Blueprint component using reflection-based type conversion.
        
        🔧 **REFLECTION-BASED PROPERTY SETTING**: Uses component property metadata to
        perform intelligent type conversion and validation.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the target component
            property_name: Name of the property to set
            property_value: Value to set (auto-converted to appropriate type)
            
        Returns:
            Dict containing success status and property information
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("set_component_property", {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "property_name": property_name,
                "property_value": property_value
            })
            
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set property {component_name}.{property_name} in {blueprint_name}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting component property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def remove_component(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        remove_children: bool = True
    ) -> Dict[str, Any]:
        """
        Remove a component from a Blueprint with intelligent child handling.
        
        🗑️ **SAFE COMPONENT REMOVAL**: Handles child components and hierarchy
        preservation with options for recursive removal or reparenting.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the component to remove
            remove_children: Whether to remove child components recursively (default: True)
            
        Returns:
            Dict containing information about removed and orphaned components
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("remove_component", {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "remove_children": remove_children
            })
            
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Removed component {component_name} from {blueprint_name}")
            return response
            
        except Exception as e:
            error_msg = f"Error removing component: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def reorder_components(
        ctx: Context,
        blueprint_name: str,
        component_order: List[str]
    ) -> Dict[str, Any]:
        """
        Reorder components in a Blueprint's component hierarchy.
        
        🔄 **HIERARCHY REORDERING**: Changes the order of components in the Blueprint's
        Simple Construction Script with hierarchy preservation.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_order: Array of component names in desired order
            
        Returns:
            Dict containing success status and final component order
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("reorder_components", {
                "blueprint_name": blueprint_name,
                "component_order": component_order
            })
            
            if not response:
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Reordered components in {blueprint_name}")
            return response
            
        except Exception as e:
            error_msg = f"Error reordering components: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_component_to_blueprint(
        ctx: Context,
        blueprint_name: str,
        component_type: str,
        component_name: str,
        location: List[float] = [],
        rotation: List[float] = [],
        scale: List[float] = [],
        component_properties: Dict[str, Any] = {}
    ) -> Dict[str, Any]:
        """
        Add a component to a Blueprint.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_type: Type of component to add (use component class name without U prefix)
            component_name: Name for the new component
            location: [X, Y, Z] coordinates for component's position
            rotation: [Pitch, Yaw, Roll] values for component's rotation
            scale: [X, Y, Z] values for component's scale
            component_properties: Additional properties to set on the component
        
        Returns:
            Information about the added component
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            # Ensure all parameters are properly formatted
            params = {
                "blueprint_name": blueprint_name,
                "component_type": component_type,
                "component_name": component_name,
                "location": location or [0.0, 0.0, 0.0],
                "rotation": rotation or [0.0, 0.0, 0.0],
                "scale": scale or [1.0, 1.0, 1.0]
            }
            
            # Add component_properties if provided
            if component_properties and len(component_properties) > 0:
                params["component_properties"] = component_properties
            
            # Validate location, rotation, and scale formats
            for param_name in ["location", "rotation", "scale"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return {"success": False, "message": f"Invalid {param_name} format. Must be a list of 3 float values."}
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
                
            logger.info(f"Adding component to blueprint with params: {params}")
            response = unreal.send_command("add_component_to_blueprint", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Component addition response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding component to blueprint: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    
    
    @mcp.tool()
    def set_component_property(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """Set a property on a component in a Blueprint."""
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "property_name": property_name,
                "property_value": property_value
            }
            
            logger.info(f"Setting component property with params: {params}")
            response = unreal.send_command("set_component_property", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set component property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting component property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    
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
    def get_variable_property(
        ctx: Context,
        blueprint_name: str,
        path: str
    ) -> Dict[str, Any]:
        """
        Get a nested Blueprint variable property via reflection.

        Args:
            blueprint_name: Target Blueprint name or full path
            path: Dotted path to variable and subfields (arrays/maps/structs). Examples:
                  "RadarSettings.Range"
                  "Inventory.Items[2].ID"
                  "Config.Values[KeyName]"

        Returns:
            Dict with success flag and value payload (reflection-serialized)
        """
        from vibe_ue_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "path": path
            }

            logger.info(f"Getting variable property '{path}' from blueprint '{blueprint_name}'")
            response = unreal.send_command("get_variable_property", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Get variable property response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error getting variable property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_variable_property(
        ctx: Context,
        blueprint_name: str,
        path: str,
        value
    ) -> Dict[str, Any]:
        """
        Set a nested Blueprint variable property via reflection.

        Args:
            blueprint_name: Target Blueprint name or full path
            path: Dotted path to variable and subfields (arrays/maps/structs). Examples:
                  "RadarSettings.Range", "Inventory.Items[2].ID", "Config.Values[KeyName]"
            value: JSON-serializable value to apply (supports structs/arrays/maps/enums)

        Returns:
            Dict with success flag and normalized_value payload (reflection-applied)
        """
        from vibe_ue_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "path": path,
                "value": value
            }

            logger.info(f"Setting variable property '{path}' on blueprint '{blueprint_name}' to '{value}'")
            response = unreal.send_command("set_variable_property", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Set variable property response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting variable property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def get_blueprint_variable_info(
        ctx: Context,
        blueprint_name: str,
        variable_name: str
    ) -> Dict[str, Any]:
        """
        Get comprehensive information about a Blueprint variable including value, type, metadata, and property flags.
        
        Args:
            blueprint_name: Name of the target Blueprint
            variable_name: Name of the variable to retrieve information about
            
        Returns:
            Response containing:
            - value: The current variable value
            - variable_type: Type of the variable (Float, Integer, Vector, etc.)
            - category: Variable category in the Blueprint editor
            - tooltip: Variable description/tooltip
            - metadata: Complete metadata including instance_editable, blueprint_readonly, etc.
            - container_type: Array/Set/Map information if applicable
            - property_flags: Detailed property flag information
        """
        from vibe_ue_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name
            }

            logger.info(f"Getting blueprint variable info for '{variable_name}' on blueprint '{blueprint_name}'")
            response = unreal.send_command("get_blueprint_variable_info", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Get blueprint variable info response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error getting blueprint variable info: {e}"
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
        Reparent a Blueprint to a new parent class.
        
        Args:
            blueprint_name: Name of the Blueprint to reparent
            new_parent_class: Name of the new parent class (e.g., "Actor", "Pawn", "UserWidget")
            
        Returns:
            Dict containing success status and reparenting information
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
    
    # NEW: Reflection-based Blueprint tools
    @mcp.tool()
    def get_available_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        category: str = "",
        context: str = ""
    ) -> Dict[str, Any]:
        """
        Discover all available Blueprint nodes using Unreal's reflection system.
        
        Args:
            blueprint_name: Target Blueprint name
            category: Optional category filter (Flow Control, Variables, Functions, etc.)
            context: Optional context for node filtering
            
        Returns:
            Dict containing categorized list of available nodes with metadata
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
            if category:
                params["category"] = category
            if context:
                params["context"] = context
                
            response = unreal.send_command("get_available_blueprint_nodes", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Discovered {response.get('total_nodes', 0)} Blueprint nodes for {blueprint_name}")
            return response
            
        except Exception as e:
            error_msg = f"Error discovering Blueprint nodes: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_node(
        ctx: Context,
        blueprint_name: str,
        node_type: str,
        position: List[float] = None,
        node_params: Dict[str, Any] = None
    ) -> Dict[str, Any]:
        """
        Create a Blueprint node using Unreal's reflection system.
        
        Args:
            blueprint_name: Target Blueprint name
            node_type: Type of node to create (Branch, CallFunction, GetVariable, etc.)
            position: [X, Y] position for the node (optional)
            node_params: Additional parameters for node configuration
            
        Returns:
            Dict containing created node information and ID
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            params = {
                "blueprint_name": blueprint_name,
                "node_identifier": node_type  # ✅ FIXED: Map node_type to node_identifier
            }
            
            if position:
                params["node_params"] = params.get("node_params", {})
                params["node_params"]["position"] = position
                
            if node_params:
                existing_params = params.get("node_params", {})
                existing_params.update(node_params)
                params["node_params"] = existing_params
                
            response = unreal.send_command("add_blueprint_node", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Created {node_type} node in Blueprint {blueprint_name}")
            return response
            
        except Exception as e:
            error_msg = f"Error creating Blueprint node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def set_blueprint_node_property(
        ctx: Context,
        blueprint_name: str,
        node_id: str,
        property_name: str,
        property_value: str
    ) -> Dict[str, Any]:
        """
        Set a property on a Blueprint node using reflection.
        
        Args:
            blueprint_name: Target Blueprint name
            node_id: ID of the node to modify
            property_name: Name of the property to set
            property_value: Value to set (will be converted to appropriate type)
            
        Returns:
            Dict containing success status and property information
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            response = unreal.send_command("set_blueprint_node_property", {
                "blueprint_name": blueprint_name,
                "node_id": node_id,
                "property_name": property_name,
                "property_value": property_value
            })
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Set property {property_name} on node {node_id} in Blueprint {blueprint_name}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting Blueprint node property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def get_blueprint_node_property(
        ctx: Context,
        blueprint_name: str,
        node_id: str,
        property_name: str
    ) -> Dict[str, Any]:
        """
        Get a property value from a Blueprint node using reflection.
        
        Args:
            blueprint_name: Target Blueprint name
            node_id: ID of the node to read from
            property_name: Name of the property to get
            
        Returns:
            Dict containing success status, property value, and metadata
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            response = unreal.send_command("get_blueprint_node_property", {
                "blueprint_name": blueprint_name,
                "node_id": node_id,
                "property_name": property_name
            })
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Got property {property_name} from node {node_id} in Blueprint {blueprint_name}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting Blueprint node property: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    logger.info("Blueprint tools registered successfully") 
