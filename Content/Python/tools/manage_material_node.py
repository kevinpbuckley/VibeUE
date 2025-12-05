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

    @mcp.tool(description="Material graph node operations: discover types, create expressions, connect pins, configure properties. Actions: discover_types, create, delete, move, list, connect, disconnect, connect_to_output, get_property, set_property, promote_to_parameter. Use get_help(topic='material-management') for examples.")
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
        """Route to material node action handlers."""
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
