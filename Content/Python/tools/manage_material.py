# Copyright Kevin Buckley 2025 All Rights Reserved.
"""
Material Management Tool for Unreal Engine MCP Server.

Provides comprehensive material lifecycle and property management through
reflection-based discovery. Supports creating materials, setting properties,
and managing parameters.

Actions:
- create: Create a new material asset
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
"""

import logging
from typing import Dict, Any, Optional, Union
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def register_material_tools(mcp: FastMCP):
    """Register material management tool with MCP server."""
    logger.info("Registering material management tools...")

    @mcp.tool()
    def manage_material(
        ctx: Context,
        action: str,
        # Material identification
        material_path: str = "",
        destination_path: str = "",
        material_name: str = "",
        # Property operations
        property_name: str = "",
        property_value: Union[str, int, float, bool] = "",
        properties: Optional[Dict[str, Any]] = None,
        include_advanced: bool = False,
        # Parameter operations
        parameter_name: str = "",
        # Extra parameters
        initial_properties: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """
        Material Management Tool
        
        Unified interface for material lifecycle, properties, and parameters.
        All operations use Unreal reflection for type-safe property handling.
        
        ============================================================================
        MATERIAL LIFECYCLE ACTIONS
        ============================================================================
        
        **action**: "create"
        Create a new material asset
        ```python
        manage_material(
            action="create",
            destination_path="/Game/Materials",
            material_name="M_NewMaterial",
            initial_properties={
                "TwoSided": "true",
                "BlendMode": "Translucent"
            }
        )
        # Returns: {"success": true, "material_path": "/Game/Materials/M_NewMaterial"}
        ```
        
        **action**: "save"
        Save a material to disk
        ```python
        manage_material(
            action="save",
            material_path="/Game/Materials/M_MyMaterial"
        )
        # Returns: {"success": true, "message": "Material saved successfully"}
        ```
        
        **action**: "compile"
        Recompile material shaders
        ```python
        manage_material(
            action="compile",
            material_path="/Game/Materials/M_MyMaterial"
        )
        # Returns: {"success": true, "message": "Material compiled successfully"}
        ```
        
        ============================================================================
        MATERIAL INFORMATION ACTIONS
        ============================================================================
        
        **action**: "get_info"
        Get comprehensive information about a material
        ```python
        manage_material(
            action="get_info",
            material_path="/Game/Materials/M_MyMaterial"
        )
        # Returns:
        # {
        #     "success": true,
        #     "asset_path": "/Game/Materials/M_MyMaterial",
        #     "name": "M_MyMaterial",
        #     "material_domain": "Surface",
        #     "blend_mode": "Opaque",
        #     "shading_model": "DefaultLit",
        #     "two_sided": false,
        #     "expression_count": 5,
        #     "texture_sample_count": 2,
        #     "parameter_count": 3,
        #     "parameter_names": ["BaseColor", "Roughness", "Metallic"],
        #     "properties": [...]
        # }
        ```
        
        **action**: "list_properties"
        List all editable properties via reflection
        ```python
        manage_material(
            action="list_properties",
            material_path="/Game/Materials/M_MyMaterial",
            include_advanced=True  # Include advanced/hidden properties
        )
        # Returns:
        # {
        #     "success": true,
        #     "properties": [
        #         {
        #             "name": "TwoSided",
        #             "display_name": "Two Sided",
        #             "type": "bool",
        #             "category": "Material",
        #             "current_value": "false",
        #             "is_editable": true,
        #             "is_advanced": false
        #         },
        #         {
        #             "name": "BlendMode",
        #             "type": "enum",
        #             "allowed_values": ["Opaque", "Masked", "Translucent", ...]
        #         },
        #         {
        #             "name": "NaniteOverrideMaterial",
        #             "type": "struct",
        #             "object_class": "NaniteOverrideMaterial",
        #             "struct_members": [
        #                 {"name": "bEnableOverride", "type": "bool", "current_value": "false"},
        #                 {"name": "OverrideMaterialRef", "type": "object", "object_class": "MaterialInterface"}
        #             ],
        #             "current_value": "(bEnableOverride=false,...)"
        #         },
        #         ...
        #     ],
        #     "count": 25
        # }
        ```
        
        ============================================================================
        PROPERTY MANAGEMENT ACTIONS
        ============================================================================
        
        **action**: "get_property"
        Get a single property value
        ```python
        manage_material(
            action="get_property",
            material_path="/Game/Materials/M_MyMaterial",
            property_name="TwoSided"
        )
        # Returns: {"success": true, "property_name": "TwoSided", "value": "false"}
        ```
        
        **action**: "get_property_info"
        Get detailed property metadata
        ```python
        manage_material(
            action="get_property_info",
            material_path="/Game/Materials/M_MyMaterial",
            property_name="BlendMode"
        )
        # Returns:
        # {
        #     "success": true,
        #     "name": "BlendMode",
        #     "display_name": "Blend Mode",
        #     "type": "enum",
        #     "allowed_values": ["Opaque", "Masked", "Translucent", ...],
        #     "current_value": "Opaque",
        #     "tooltip": "Determines how the material is rendered..."
        # }
        ```
        
        **action**: "set_property"
        Set a single property value
        ```python
        manage_material(
            action="set_property",
            material_path="/Game/Materials/M_MyMaterial",
            property_name="TwoSided",
            value="true"
        )
        # Returns: {"success": true, "message": "Set TwoSided = true"}
        ```
        
        **action**: "set_properties"
        Set multiple properties at once
        ```python
        manage_material(
            action="set_properties",
            material_path="/Game/Materials/M_MyMaterial",
            properties={
                "TwoSided": "true",
                "BlendMode": "Masked",
                "OpacityMaskClipValue": "0.33"
            }
        )
        # Returns: {"success": true, "properties_set": 3}
        ```
        
        ============================================================================
        PARAMETER MANAGEMENT ACTIONS
        ============================================================================
        
        **action**: "list_parameters"
        List all material parameters (from expression nodes)
        ```python
        manage_material(
            action="list_parameters",
            material_path="/Game/Materials/M_MyMaterial"
        )
        # Returns:
        # {
        #     "success": true,
        #     "parameters": [
        #         {
        #             "name": "BaseColorTint",
        #             "type": "Vector",
        #             "group": "Base",
        #             "current_value": "(1.0,0.5,0.0,1.0)",
        #             "default_value": "(1.0,1.0,1.0,1.0)",
        #             "sort_priority": 0
        #         },
        #         {
        #             "name": "RoughnessValue",
        #             "type": "Scalar",
        #             "current_value": "0.5"
        #         }
        #     ],
        #     "count": 2
        # }
        ```
        
        **action**: "get_parameter"
        Get a specific parameter's info
        ```python
        manage_material(
            action="get_parameter",
            material_path="/Game/Materials/M_MyMaterial",
            parameter_name="RoughnessValue"
        )
        # Returns: {"success": true, "name": "RoughnessValue", "type": "Scalar", ...}
        ```
        
        **action**: "set_parameter_default"
        Set a parameter's default value
        ```python
        manage_material(
            action="set_parameter_default",
            material_path="/Game/Materials/M_MyMaterial",
            parameter_name="RoughnessValue",
            value="0.75"
        )
        # Returns: {"success": true, "message": "Set parameter RoughnessValue = 0.75"}
        ```
        
        ============================================================================
        COMMON MATERIAL PROPERTIES
        ============================================================================
        
        **Rendering Properties:**
        - TwoSided (bool): Render both faces
        - BlendMode (enum): Opaque, Masked, Translucent, Additive, Modulate
        - MaterialDomain (enum): Surface, DeferredDecal, LightFunction, Volume, PostProcess, UI
        - ShadingModel (enum): DefaultLit, Unlit, Subsurface, ClearCoat, etc.
        
        **Opacity Properties:**
        - OpacityMaskClipValue (float): Clip threshold for masked materials
        - DitheredLODTransition (bool): Use dithered LOD transitions
        
        **Physical Properties:**
        - bUsedWithSkeletalMesh (bool): Optimize for skeletal meshes
        - bUsedWithStaticLighting (bool): Support static lighting
        
        ============================================================================
        STRUCT PROPERTY FORMAT
        ============================================================================
        
        Struct properties can be set using parenthesized member=value format:
        
        ```python
        # Example: Setting NaniteOverrideMaterial struct
        manage_material(
            action="set_property",
            material_path="/Game/Materials/M_MyMaterial",
            property_name="NaniteOverrideMaterial",
            value="(bEnableOverride=true,OverrideMaterialRef=/Game/Materials/M_Other)"
        )
        
        # Use list_properties to discover struct members:
        # The response includes "struct_members" array showing each member's:
        # - name: Member property name
        # - type: Member type (bool, float, object, enum, etc.)
        # - object_class: For object members, the expected class type
        # - current_value: Current value as string
        # - allowed_values: For enum members, list of valid values
        ```
        
        ============================================================================
        PARAMETER REFERENCE
        ============================================================================
        
        Args:
            action: Action to perform (create|get_info|list_properties|get_property|
                   get_property_info|set_property|set_properties|list_parameters|
                   get_parameter|set_parameter_default|save|compile)
            material_path: Full asset path to the material (for existing materials)
            destination_path: Content browser path for new material (for create)
            material_name: Name for new material (for create)
            property_name: Name of property to get/set
            value: Value to set (as string - auto-converted based on type)
            properties: Dict of property name->value pairs (for set_properties)
            include_advanced: Include advanced/hidden properties (for list_properties)
            parameter_name: Name of material parameter to get/set
            initial_properties: Dict of initial property values (for create)
            
        Returns:
            Dict containing action results with success field
        """
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
            
        # Property operations
        if property_name:
            params["property_name"] = property_name
        if property_value != "":
            params["value"] = str(property_value)
        if properties:
            params["properties"] = properties
        if include_advanced:
            params["include_advanced"] = include_advanced
            
        # Parameter operations
        if parameter_name:
            params["parameter_name"] = parameter_name
            
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
