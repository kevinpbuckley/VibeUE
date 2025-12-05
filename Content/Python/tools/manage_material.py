# Copyright Kevin Buckley 2025 All Rights Reserved.
"""
Material Management Tool for Unreal Engine MCP Server.

Provides comprehensive material lifecycle and property management through
reflection-based discovery. Supports creating materials, creating material instances,
setting properties, and managing parameters.

Base Material Actions:
- create: Create a new material asset
- create_instance: Create a new material instance (MIC) from a parent material
- get_info: Get comprehensive material information
- list_properties: List all editable properties via reflection
- get_property: Get a property value
- get_property_info: Get detailed property metadata
- set_property: Set a property value
- set_properties: Set multiple properties at once
- list_parameters: List all material parameters
- get_parameter: Get a specific parameter
- set_parameter_default: Set a parameter's default value
- save: Save material to disk
- compile: Recompile material shaders
- refresh_editor: Refresh open Material Editor

Material Instance Actions:
- get_instance_info: Get comprehensive info about a material instance
- list_instance_properties: List all editable properties on a material instance
- get_instance_property: Get a single property value from instance
- set_instance_property: Set a property on material instance (e.g., PhysMaterial)
- list_instance_parameters: List all parameters with current/default values
- set_instance_scalar_parameter: Set a scalar parameter override
- set_instance_vector_parameter: Set a vector/color parameter override
- set_instance_texture_parameter: Set a texture parameter override
- clear_instance_parameter_override: Remove parameter override, revert to parent
- save_instance: Save material instance to disk
"""

import logging
from typing import Dict, Any, Optional, Union
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def register_material_tools(mcp: FastMCP):
    """Register material management tool with MCP server."""
    logger.info("Registering material management tools...")

    @mcp.tool(description="Material and material instance management: create, configure properties, manage parameters. Base actions: create, get_info, list_properties, set_property, save, compile. Instance actions: create_instance, set_instance_scalar_parameter, set_instance_vector_parameter, set_instance_texture_parameter. Use get_help(topic='material-management') for examples.")
    def manage_material(
        ctx: Context,
        action: str,
        # Material identification
        material_path: str = "",
        destination_path: str = "",
        material_name: str = "",
        # Material instance identification
        instance_path: str = "",
        # Material instance creation
        parent_material_path: str = "",
        instance_name: str = "",
        scalar_parameters: Optional[Dict[str, float]] = None,
        vector_parameters: Optional[Dict[str, Any]] = None,
        texture_parameters: Optional[Dict[str, str]] = None,
        # Property operations
        property_name: str = "",
        property_value: Union[str, int, float, bool] = "",
        value: str = "",  # Alias for property_value (used by some actions)
        properties: Optional[Dict[str, Any]] = None,
        include_advanced: bool = False,
        # Parameter operations
        parameter_name: str = "",
        # Vector parameter values (for set_instance_vector_parameter)
        r: Optional[float] = None,
        g: Optional[float] = None,
        b: Optional[float] = None,
        a: Optional[float] = None,
        # Texture parameter (for set_instance_texture_parameter)
        texture_path: str = "",
        # Extra parameters
        initial_properties: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Route to material action handlers."""
        # Import connection handler inside function to avoid module-level import issues
        from vibe_ue_server import get_unreal_connection
        
        action_lower = action.lower()
        
        # Build parameters dict
        params = {
            "action": action_lower
        }
        
        # Material identification
        if material_path:
            params["material_path"] = material_path
        if destination_path:
            params["destination_path"] = destination_path
        if material_name:
            params["material_name"] = material_name
        
        # Material instance identification
        if instance_path:
            params["instance_path"] = instance_path
        
        # Material instance creation
        if parent_material_path:
            params["parent_material_path"] = parent_material_path
        if instance_name:
            params["instance_name"] = instance_name
        if scalar_parameters:
            params["scalar_parameters"] = scalar_parameters
        if vector_parameters:
            params["vector_parameters"] = vector_parameters
        if texture_parameters:
            params["texture_parameters"] = texture_parameters
            
        # Property operations
        if property_name:
            params["property_name"] = property_name
        # Support both property_value and value parameters (value is alias)
        actual_value = value if value else (str(property_value) if property_value != "" else "")
        if actual_value:
            params["value"] = actual_value
        if properties:
            params["properties"] = properties
        if include_advanced:
            params["include_advanced"] = include_advanced
            
        # Parameter operations
        if parameter_name:
            params["parameter_name"] = parameter_name
        
        # Vector parameter values (for set_instance_vector_parameter)
        if r is not None and g is not None and b is not None:
            params["r"] = r
            params["g"] = g
            params["b"] = b
            params["a"] = a if a is not None else 1.0
        
        # Texture parameter (for set_instance_texture_parameter)
        if texture_path:
            params["texture_path"] = texture_path
            
        # Initial properties for create
        if initial_properties:
            params["initial_properties"] = initial_properties
            
        logger.info(f"manage_material: action={action_lower}")
        
        try:
            unreal = get_unreal_connection()
            if not unreal.connect():
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            result = unreal.send_command("manage_material", params)
            return result
        except Exception as e:
            logger.error(f"manage_material error: {e}")
            return {"success": False, "error": str(e)}
    
    logger.info("Material management tools registered successfully")
