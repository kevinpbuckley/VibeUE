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
    
    logger.info("Blueprint tools (including reflection-based tools) registered successfully") 
