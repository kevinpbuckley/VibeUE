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

    @mcp.tool(description="Material graph node operations: discover types, create expressions, connect pins, configure properties. Actions: discover_types, get_categories, create, delete, move, list, get_details, get_pins, connect, disconnect, connect_to_output, disconnect_output, list_connections, get_property, set_property, list_properties, promote_to_parameter, create_parameter, set_parameter_metadata, get_output_properties, get_output_connections. Use action='help' for all actions and detailed parameter info.")
    def manage_material_node(
        ctx: Context,
        action: str,
        help_action: str = "",
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
        # Handle help action
        if action and action.lower() == "help":
            from help_system import generate_help_response
            return generate_help_response("manage_material_node", help_action if help_action else None)
        
        # Import error response helper
        from help_system import generate_error_response
        
        from vibe_ue_server import get_unreal_connection
        
        # Validate action is provided
        if not action:
            return generate_error_response(
                "manage_material_node", "",
                "action is required. Use action='help' to see all available actions."
            )
        
        action_lower = action.lower()
        
        # Valid actions
        valid_actions = [
            "discover_types", "get_categories",
            "create", "delete", "move", "list", "get_details", "get_pins",
            "connect", "disconnect", "connect_to_output", "disconnect_output", "list_connections",
            "get_property", "set_property", "list_properties",
            "promote_to_parameter", "create_parameter", "set_parameter_metadata",
            "get_output_properties", "get_output_connections"
        ]
        
        if action_lower not in valid_actions:
            error_response = generate_error_response(
                "manage_material_node", action,
                f"Invalid action '{action}'. Valid actions: {', '.join(valid_actions)}"
            )
            # Override available_actions with actual valid actions list
            error_response["available_actions"] = valid_actions
            return error_response
        
        # Action-specific validation
        missing = []
        
        # Most actions require material_path
        material_required_actions = [
            "create", "delete", "move", "list", "get_details", "get_pins",
            "connect", "disconnect", "connect_to_output", "disconnect_output", "list_connections",
            "get_property", "set_property", "list_properties",
            "promote_to_parameter", "create_parameter", "set_parameter_metadata",
            "get_output_properties", "get_output_connections"
        ]
        
        if action_lower in material_required_actions and not material_path:
            return generate_error_response(
                "manage_material_node", action,
                f"{action} requires 'material_path'",
                missing_params=["material_path"]
            )
        
        # Expression-based actions need expression_id
        expression_required_actions = ["delete", "move", "get_details", "get_pins", "promote_to_parameter", "set_parameter_metadata"]
        if action_lower in expression_required_actions and not expression_id:
            return generate_error_response(
                "manage_material_node", action,
                f"{action} requires 'expression_id'",
                missing_params=["expression_id"]
            )
        
        # Create action validation
        if action_lower == "create":
            if not expression_class:
                return generate_error_response(
                    "manage_material_node", action,
                    "create requires 'expression_class'. Use discover_types to find available expression types.",
                    missing_params=["expression_class"]
                )
        
        # Connect action validation
        if action_lower == "connect":
            if not source_expression_id:
                missing.append("source_expression_id")
            if not target_expression_id:
                missing.append("target_expression_id")
            if not target_input and not input_name:
                missing.append("target_input (or input_name)")
            if missing:
                return generate_error_response(
                    "manage_material_node", action,
                    f"connect requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
        # Disconnect action validation
        if action_lower == "disconnect":
            if not expression_id:
                return generate_error_response(
                    "manage_material_node", action,
                    "disconnect requires 'expression_id' and optionally 'input_name'",
                    missing_params=["expression_id"]
                )
        
        # Connect to output validation
        if action_lower == "connect_to_output":
            if not expression_id:
                missing.append("expression_id")
            if not material_property:
                missing.append("material_property")
            if missing:
                return generate_error_response(
                    "manage_material_node", action,
                    f"connect_to_output requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
        # Property operations validation
        if action_lower == "get_property":
            if not expression_id:
                missing.append("expression_id")
            if not property_name:
                missing.append("property_name")
            if missing:
                return generate_error_response(
                    "manage_material_node", action,
                    f"get_property requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
        if action_lower == "set_property":
            if not expression_id:
                missing.append("expression_id")
            if not property_name:
                missing.append("property_name")
            if not value:
                missing.append("value")
            if missing:
                return generate_error_response(
                    "manage_material_node", action,
                    f"set_property requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
        # Create parameter validation
        if action_lower == "create_parameter":
            if not parameter_type:
                missing.append("parameter_type")
            if not parameter_name:
                missing.append("parameter_name")
            if missing:
                return generate_error_response(
                    "manage_material_node", action,
                    f"create_parameter requires: {', '.join(missing)}",
                    missing_params=missing
                )
        
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
