# Copyright Kevin Buckley 2025 All Rights Reserved.
"""
Material Node Management Tool for Unreal Engine MCP Server.

Provides comprehensive material graph node (expression) management including
creating, connecting, and configuring material expressions. Supports promoting
constants to parameters.

Discovery Actions:
- discover_types: Discover available material expression types
- get_categories: Get expression categories

Expression Lifecycle Actions:
- create: Create a new material expression
- delete: Delete an expression
- move: Move an expression to a new position

Expression Information Actions:
- list: List all expressions in a material
- get_details: Get detailed expression information
- get_pins: Get all pins for an expression

Connection Actions:
- connect: Connect two expressions
- disconnect: Disconnect an input
- connect_to_output: Connect expression to material output property
- disconnect_output: Disconnect a material output property
- list_connections: List all connections in material

Expression Property Actions:
- get_property: Get expression property value
- set_property: Set expression property value
- list_properties: List all editable properties

Parameter Operations:
- promote_to_parameter: Convert constant to parameter
- create_parameter: Create a parameter expression
- set_parameter_metadata: Set parameter group/priority

Material Output Actions:
- get_output_properties: Get available material output properties
- get_output_connections: Get current material output connections
"""

import logging
from typing import Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def register_material_node_tools(mcp: FastMCP):
    """Register material node management tool with MCP server."""
    logger.info("Registering material node management tools...")

    @mcp.tool()
    def manage_material_node(
        ctx: Context,
        action: str,
        # Material identification
        material_path: str = "",
        # Expression identification
        expression_id: str = "",
        expression_class: str = "",
        # Position
        pos_x: int = 0,
        pos_y: int = 0,
        # Connection parameters
        source_expression_id: str = "",
        source_output: str = "",
        target_expression_id: str = "",
        target_input: str = "",
        input_name: str = "",
        output_name: str = "",
        material_property: str = "",
        # Property operations
        property_name: str = "",
        value: str = "",
        # Parameter operations
        parameter_name: str = "",
        parameter_type: str = "",
        group_name: str = "",
        default_value: str = "",
        sort_priority: int = 0,
        # Discovery options
        category: str = "",
        search_term: str = "",
        max_results: int = 100,
    ) -> Dict[str, Any]:
        """
        Material Node Management Tool
        
        Unified interface for material graph node (expression) operations.
        Supports creating, connecting, and configuring material expressions.
        
        ============================================================================
        DISCOVERY ACTIONS
        ============================================================================
        
        **action**: "discover_types"
        Discover available material expression types
        ```python
        manage_material_node(
            action="discover_types",
            category="Math",  # Optional filter
            search_term="Add",  # Optional search
            max_results=50
        )
        # Returns: {"success": true, "expression_types": [...], "count": 25}
        ```
        
        **action**: "get_categories"
        Get expression categories
        ```python
        manage_material_node(
            action="get_categories"
        )
        # Returns: {"success": true, "categories": ["Coordinates", "Math", "Texture", ...]}
        ```
        
        ============================================================================
        EXPRESSION LIFECYCLE ACTIONS
        ============================================================================
        
        **action**: "create"
        Create a new material expression
        ```python
        manage_material_node(
            action="create",
            material_path="/Game/Materials/M_MyMaterial",
            expression_class="Add",  # or "MaterialExpressionAdd"
            pos_x=200,
            pos_y=100
        )
        # Returns: {"success": true, "expression": {...}, "expression_id": "..."}
        ```
        
        **action**: "delete"
        Delete an expression
        ```python
        manage_material_node(
            action="delete",
            material_path="/Game/Materials/M_MyMaterial",
            expression_id="MaterialExpressionAdd_0x12345678"
        )
        # Returns: {"success": true}
        ```
        
        **action**: "move"
        Move an expression to a new position
        ```python
        manage_material_node(
            action="move",
            material_path="/Game/Materials/M_MyMaterial",
            expression_id="MaterialExpressionAdd_0x12345678",
            pos_x=400,
            pos_y=200
        )
        # Returns: {"success": true}
        ```
        
        ============================================================================
        EXPRESSION INFORMATION ACTIONS
        ============================================================================
        
        **action**: "list"
        List all expressions in a material
        ```python
        manage_material_node(
            action="list",
            material_path="/Game/Materials/M_MyMaterial"
        )
        # Returns: {"success": true, "expressions": [...], "count": 5}
        ```
        
        **action**: "get_details"
        Get detailed expression information
        ```python
        manage_material_node(
            action="get_details",
            material_path="/Game/Materials/M_MyMaterial",
            expression_id="MaterialExpressionAdd_0x12345678"
        )
        # Returns: {"success": true, "expression": {...}}
        ```
        
        **action**: "get_pins"
        Get all pins for an expression
        ```python
        manage_material_node(
            action="get_pins",
            material_path="/Game/Materials/M_MyMaterial",
            expression_id="MaterialExpressionAdd_0x12345678"
        )
        # Returns: {"success": true, "pins": [...]}
        ```
        
        ============================================================================
        CONNECTION ACTIONS
        ============================================================================
        
        **action**: "connect"
        Connect two expressions
        ```python
        manage_material_node(
            action="connect",
            material_path="/Game/Materials/M_MyMaterial",
            source_expression_id="MaterialExpressionConstant_0x12345678",
            source_output="",  # Empty = first output
            target_expression_id="MaterialExpressionAdd_0x87654321",
            target_input="A"
        )
        # Returns: {"success": true}
        ```
        
        **action**: "disconnect"
        Disconnect an input
        ```python
        manage_material_node(
            action="disconnect",
            material_path="/Game/Materials/M_MyMaterial",
            expression_id="MaterialExpressionAdd_0x87654321",
            input_name="A"
        )
        # Returns: {"success": true}
        ```
        
        **action**: "connect_to_output"
        Connect expression to material output property (BaseColor, Metallic, etc.)
        ```python
        manage_material_node(
            action="connect_to_output",
            material_path="/Game/Materials/M_MyMaterial",
            expression_id="MaterialExpressionMultiply_0x12345678",
            output_name="",  # Empty = first output
            material_property="BaseColor"
        )
        # Returns: {"success": true}
        ```
        
        **action**: "disconnect_output"
        Disconnect a material output property
        ```python
        manage_material_node(
            action="disconnect_output",
            material_path="/Game/Materials/M_MyMaterial",
            material_property="BaseColor"
        )
        # Returns: {"success": true}
        ```
        
        **action**: "list_connections"
        List all connections in material
        ```python
        manage_material_node(
            action="list_connections",
            material_path="/Game/Materials/M_MyMaterial"
        )
        # Returns: {"success": true, "connections": [...], "count": 10}
        ```
        
        ============================================================================
        EXPRESSION PROPERTY ACTIONS
        ============================================================================
        
        **action**: "get_property"
        Get expression property value
        ```python
        manage_material_node(
            action="get_property",
            material_path="/Game/Materials/M_MyMaterial",
            expression_id="MaterialExpressionConstant_0x12345678",
            property_name="R"
        )
        # Returns: {"success": true, "property_name": "R", "value": "1.0"}
        ```
        
        **action**: "set_property"
        Set expression property value
        ```python
        manage_material_node(
            action="set_property",
            material_path="/Game/Materials/M_MyMaterial",
            expression_id="MaterialExpressionConstant_0x12345678",
            property_name="R",
            value="0.5"
        )
        # Returns: {"success": true}
        ```
        
        **action**: "list_properties"
        List all editable properties
        ```python
        manage_material_node(
            action="list_properties",
            material_path="/Game/Materials/M_MyMaterial",
            expression_id="MaterialExpressionConstant_0x12345678"
        )
        # Returns: {"success": true, "properties": [{"name": "R", "value": "1.0"}, ...]}
        ```
        
        ============================================================================
        PARAMETER OPERATIONS
        ============================================================================
        
        **action**: "promote_to_parameter"
        Convert constant to parameter (Promote to Parameter)
        ```python
        manage_material_node(
            action="promote_to_parameter",
            material_path="/Game/Materials/M_MyMaterial",
            expression_id="MaterialExpressionConstant_0x12345678",  # Must be Constant/Vector/Texture
            parameter_name="Roughness",
            group_name="Surface"  # Optional group
        )
        # Returns: {"success": true, "parameter": {...}, "expression_id": "..."}
        ```
        
        **action**: "create_parameter"
        Create a parameter expression directly
        ```python
        manage_material_node(
            action="create_parameter",
            material_path="/Game/Materials/M_MyMaterial",
            parameter_type="Scalar",  # Scalar, Vector, Texture, StaticBool
            parameter_name="Roughness",
            group_name="Surface",
            default_value="0.5",
            pos_x=100,
            pos_y=200
        )
        # Returns: {"success": true, "parameter": {...}, "expression_id": "..."}
        ```
        
        **action**: "set_parameter_metadata"
        Set parameter group/priority
        ```python
        manage_material_node(
            action="set_parameter_metadata",
            material_path="/Game/Materials/M_MyMaterial",
            expression_id="MaterialExpressionScalarParameter_0x12345678",
            group_name="NewGroup",
            sort_priority=10
        )
        # Returns: {"success": true}
        ```
        
        ============================================================================
        MATERIAL OUTPUT ACTIONS
        ============================================================================
        
        **action**: "get_output_properties"
        Get available material output properties
        ```python
        manage_material_node(
            action="get_output_properties",
            material_path="/Game/Materials/M_MyMaterial"
        )
        # Returns: {"success": true, "output_properties": ["BaseColor", "Metallic", ...]}
        ```
        
        **action**: "get_output_connections"
        Get current material output connections
        ```python
        manage_material_node(
            action="get_output_connections",
            material_path="/Game/Materials/M_MyMaterial"
        )
        # Returns: {"success": true, "output_connections": {"BaseColor": "expr_id", ...}}
        ```
        
        ============================================================================
        COMMON EXPRESSION TYPES
        ============================================================================
        
        **Math:**
        - Add, Subtract, Multiply, Divide
        - Dot, Cross, Normalize
        - Power, SquareRoot, Abs
        - Clamp, Lerp (LinearInterpolate)
        - OneMinus, Saturate
        - ComponentMask, AppendVector
        
        **Constants:**
        - Constant (scalar)
        - Constant2Vector, Constant3Vector, Constant4Vector
        
        **Parameters:**
        - ScalarParameter, VectorParameter
        - TextureSampleParameter2D, TextureObjectParameter
        - StaticBoolParameter, StaticSwitchParameter
        
        **Texture:**
        - TextureSample, TextureObject
        - TextureCoordinate
        - Panner, Rotator
        
        **Utility:**
        - Comment (for organization)
        - Fresnel
        - Time
        
        **World:**
        - WorldPosition, CameraPositionWS
        - PixelNormalWS, VertexNormalWS
        
        ============================================================================
        MATERIAL OUTPUT PROPERTIES
        ============================================================================
        
        - BaseColor: RGB surface color
        - Metallic: 0-1 metallic value
        - Specular: Non-metallic specular
        - Roughness: Surface roughness
        - EmissiveColor: Emission
        - Opacity: Transparency (requires Translucent blend mode)
        - OpacityMask: Cutout (requires Masked blend mode)
        - Normal: Normal map
        - WorldPositionOffset: Vertex offset
        - AmbientOcclusion: Baked AO
        - Refraction: Refraction (Translucent)
        
        ============================================================================
        NOTE: Material graphs do NOT support split/recombine pins like Blueprints.
        Use ComponentMask and AppendVector expressions instead.
        ============================================================================
        """
        from vibe_ue_server import get_unreal_connection
        
        action_lower = action.lower()
        
        # Build params
        params = {
            "action": action_lower
        }
        
        # Material identification
        if material_path:
            params["material_path"] = material_path
        
        # Expression identification
        if expression_id:
            params["expression_id"] = expression_id
        if expression_class:
            params["expression_class"] = expression_class
        
        # Position
        if pos_x != 0:
            params["pos_x"] = pos_x
        if pos_y != 0:
            params["pos_y"] = pos_y
        
        # Connection parameters
        if source_expression_id:
            params["source_expression_id"] = source_expression_id
        if source_output:
            params["source_output"] = source_output
        if target_expression_id:
            params["target_expression_id"] = target_expression_id
        if target_input:
            params["target_input"] = target_input
        if input_name:
            params["input_name"] = input_name
        if output_name:
            params["output_name"] = output_name
        if material_property:
            params["material_property"] = material_property
        
        # Property operations
        if property_name:
            params["property_name"] = property_name
        if value:
            params["value"] = value
        
        # Parameter operations
        if parameter_name:
            params["parameter_name"] = parameter_name
        if parameter_type:
            params["parameter_type"] = parameter_type
        if group_name:
            params["group_name"] = group_name
        if default_value:
            params["default_value"] = default_value
        if sort_priority != 0:
            params["sort_priority"] = sort_priority
        
        # Discovery options
        if category:
            params["category"] = category
        if search_term:
            params["search_term"] = search_term
        if max_results != 100:
            params["max_results"] = max_results
        
        logger.info(f"manage_material_node: action={action_lower}")
        
        try:
            unreal = get_unreal_connection()
            if not unreal.connect():
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            result = unreal.send_command("manage_material_node", params)
            return result
        except Exception as e:
            logger.error(f"manage_material_node error: {e}")
            return {"success": False, "error": str(e)}
    
    logger.info("Material node management tools registered successfully")
