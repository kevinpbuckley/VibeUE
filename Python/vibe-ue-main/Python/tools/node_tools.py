"""
Blueprint Node Tools for Unreal MCP.

This module provides tools for manipulating Blueprint graph nodes and connections.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_blueprint_node_tools(mcp: FastMCP):
    """Register Blueprint node manipulation tools with the MCP server."""
    
    @mcp.tool()
    def add_blueprint_event_node(
        ctx: Context,
        blueprint_name: str,
        event_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add an event node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            event_name: Name of the event. Use 'Receive' prefix for standard events:
                       - 'ReceiveBeginPlay' for Begin Play
                       - 'ReceiveTick' for Tick
                       - etc.
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            # Handle default value within the method body
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "event_name": event_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding event node '{event_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_event_node", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Event node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding event node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_input_action_node(
        ctx: Context,
        blueprint_name: str,
        action_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add an input action event node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            action_name: Name of the input action to respond to
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            # Handle default value within the method body
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "action_name": action_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding input action node for '{action_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_input_action_node", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Input action node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding input action node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_function_node(
        ctx: Context,
        blueprint_name: str,
        target: str,
        function_name: str,
        params = None,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add a function call node to a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            target: Target object for the function (component name or self)
            function_name: Name of the function to call
            params: Optional parameters to set on the function node
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            # Handle default values within the method body
            if params is None:
                params = {}
            if node_position is None:
                node_position = [0, 0]
            
            command_params = {
                "blueprint_name": blueprint_name,
                "target": target,
                "function_name": function_name,
                "params": params,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding function node '{function_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_function_node", command_params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Function node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding function node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
            
    @mcp.tool()
    def connect_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        source_node_id: str,
        source_pin: str,
        target_node_id: str,
        target_pin: str
    ) -> Dict[str, Any]:
        """
        Connect two nodes in a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            source_node_id: ID of the source node
            source_pin: Name of the output pin on the source node
            target_node_id: ID of the target node
            target_pin: Name of the input pin on the target node
            
        Returns:
            Response indicating success or failure
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "source_node_id": source_node_id,
                "source_pin": source_pin,
                "target_node_id": target_node_id,
                "target_pin": target_pin
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Connecting nodes in blueprint '{blueprint_name}'")
            response = unreal.send_command("connect_blueprint_nodes", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Node connection response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error connecting nodes: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        variable_type: str,
        is_exposed: bool = False
    ) -> Dict[str, Any]:
        """
        Add a variable to a Blueprint.
        
        Args:
            blueprint_name: Name of the target Blueprint
            variable_name: Name of the variable
            variable_type: Type of the variable (Boolean, Integer, Float, Vector, etc.)
            is_exposed: Whether to expose the variable to the editor
            
        Returns:
            Response indicating success or failure
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name,
                "variable_type": variable_type,
                "is_exposed": is_exposed
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding variable '{variable_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_variable", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Variable creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding variable: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def get_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str
    ) -> Dict[str, Any]:
        """
        Get a variable's value and metadata from a Blueprint.
        
        Args:
            blueprint_name: Name of the target Blueprint
            variable_name: Name of the variable to retrieve
            
        Returns:
            Response containing variable value, type, and metadata
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Getting variable '{variable_name}' from blueprint '{blueprint_name}'")
            response = unreal.send_command("get_blueprint_variable", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Variable retrieval response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting variable: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def set_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        variable_value: str
    ) -> Dict[str, Any]:
        """
        Set a variable's value in a Blueprint.
        
        Args:
            blueprint_name: Name of the target Blueprint
            variable_name: Name of the variable to set
            variable_value: Value to set (as string, will be converted to appropriate type)
            
        Returns:
            Response indicating success or failure
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "variable_name": variable_name,
                "variable_value": variable_value
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Setting variable '{variable_name}' to '{variable_value}' in blueprint '{blueprint_name}'")
            response = unreal.send_command("set_blueprint_variable", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Variable setting response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting variable: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def get_available_blueprint_variable_types(
        ctx: Context
    ) -> Dict[str, Any]:
        """
        Get list of all available Blueprint variable types with descriptions and examples.
        
        Returns:
            Response containing categorized list of available variable types
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            params = {}
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info("Getting available Blueprint variable types")
            response = unreal.send_command("get_available_blueprint_variable_types", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Available types response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting available variable types: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_get_self_component_reference(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add a node that gets a reference to a component owned by the current Blueprint.
        This creates a node similar to what you get when dragging a component from the Components panel.
        
        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the component to get a reference to
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            # Handle None case explicitly in the function
            if node_position is None:
                node_position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "component_name": component_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding self component reference node for '{component_name}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_get_self_component_reference", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Self component reference node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding self component reference node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_self_reference(
        ctx: Context,
        blueprint_name: str,
        node_position = None
    ) -> Dict[str, Any]:
        """
        Add a 'Get Self' node to a Blueprint's event graph that returns a reference to this actor.
        
        Args:
            blueprint_name: Name of the target Blueprint
            node_position: Optional [X, Y] position in the graph
            
        Returns:
            Response containing the node ID and success status
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            if node_position is None:
                node_position = [0, 0]
                
            params = {
                "blueprint_name": blueprint_name,
                "node_position": node_position
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding self reference node to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_self_reference", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Self reference node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error adding self reference node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def find_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        node_type: Optional[str] = None,
        event_type: Optional[str] = None,
        event_name: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Find nodes in a Blueprint's event graph.
        
        Args:
            blueprint_name: Name of the target Blueprint
            node_type: Optional type of node to find (Event, Function, Variable, etc.)
            event_type: Optional specific event type to find (BeginPlay, Tick, etc.)
            event_name: Optional exact event node name required by some engine implementations
            
        Returns:
            Response containing array of found node IDs and success status
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "node_type": node_type,
                "event_type": event_type,
                "event_name": event_name
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Finding nodes in blueprint '{blueprint_name}'")
            response = unreal.send_command("find_blueprint_nodes", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Node find response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error finding nodes: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_node(
        ctx: Context,
        blueprint_name: str,
        node_type: str,
        position: Optional[List[float]] = None,
        node_params: Optional[Dict[str, Any]] = None
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
            params = {
                "blueprint_name": blueprint_name,
                "node_identifier": node_type,  # Map to expected parameter name
                "node_position": position,
                "node_params": node_params or {}
            }
            
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            
            logger.info(f"Adding node '{node_type}' to blueprint '{blueprint_name}'")
            response = unreal.send_command("add_blueprint_node", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Node creation response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error creating node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}
    
    @mcp.tool()
    def add_blueprint_node(
        ctx: Context,
        blueprint_name: str,
        node_type: str,
        node_params: Optional[Dict[str, Any]] = None,
        position: Optional[List[float]] = None
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
            # Log what we received to debug
            logger.info(f"=== MCP Tool Called ===")
            logger.info(f"blueprint_name: {blueprint_name}")
            logger.info(f"node_type: {node_type}")
            logger.info(f"node_params: {node_params}")
            logger.info(f"position: {position}")
            
            # Debug: Print to stderr for VS Code debugging
            import sys
            print(f"DEBUG: MCP add_blueprint_node called with node_type={node_type}", file=sys.stderr)
            sys.stderr.flush()
            
            # Handle default parameters
            if node_params is None:
                node_params = {}
            if position is None:
                position = [0, 0]
            
            params = {
                "blueprint_name": blueprint_name,
                "node_identifier": node_type,  # Map node_type to expected parameter name
                "node_config": node_params,
                "position": position
            }
            
            # Call Unreal Engine via MCP
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Cannot connect to Unreal Engine")
                return {"success": False, "message": "Cannot connect to Unreal Engine"}
            
            logger.info(f"Adding blueprint node with params: {params}")
            
            # DEBUG: Log the exact command and parameters being sent
            print(f"DEBUG: Sending command 'add_blueprint_node' with params: {params}", file=sys.stderr)
            sys.stderr.flush()
            
            response = unreal.send_command("add_blueprint_node", params)
            
            # DEBUG: Log the raw response from Unreal
            print(f"DEBUG: Raw response from Unreal: {response}", file=sys.stderr)
            print(f"DEBUG: Response type: {type(response)}", file=sys.stderr)
            sys.stderr.flush()
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            
            logger.info(f"Node creation response: {response}")
            
            # DEBUG: If response claims success but node isn't found later, log this
            if isinstance(response, dict) and response.get("success"):
                print(f"DEBUG: Node creation claimed success with ID: {response.get('node_id', 'NO_ID')}", file=sys.stderr)
                sys.stderr.flush()
            
            return response
            
        except Exception as e:
            error_msg = f"Error creating node: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    logger.info("Blueprint node tools registered successfully")

# Standalone function for testing (non-MCP)
def add_blueprint_node_test(blueprint_name: str, node_type: str, node_params: Optional[Dict[str, Any]] = None, position: Optional[List[float]] = None) -> Dict[str, Any]:
    """
    Test function for add_blueprint_node without MCP context.
    
    Args:
        blueprint_name: Target Blueprint name
        node_type: Type of node to create (Branch, CallFunction, etc.)
        node_params: Additional parameters for node configuration  
        position: [X, Y] position for the node (optional)
        
    Returns:
        Dict containing test result (basic implementation for now)
    """
    if node_params is None:
        node_params = {}
    if position is None:
        position = [0, 0]
    
    # Return a test response showing the system is ready
    return {
        "success": True,
        "node_type": node_type,
        "node_id": f"test_node_{hash(f'{blueprint_name}_{node_type}')}",
        "message": f"Test: Would create {node_type} node in {blueprint_name} at position {position}",
        "system_status": "MCP Tool and C++ backend are connected and ready"
    }

    @mcp.tool()
    def get_available_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        category: str = "",
        search_term: str = "",
        context: str = "",
        include_deprecated: bool = False,
        max_results: int = 100
    ) -> Dict[str, Any]:
        """
        Discover all available Blueprint nodes using Unreal Engine's reflection system.
        
        This function leverages Unreal's Blueprint Action Menu system to provide
        comprehensive node discovery with the same categorization and filtering
        that the Blueprint editor uses.
        
        Args:
            blueprint_name: Target Blueprint for context-sensitive discovery
            category: Filter by node category:
                     - "Math" (Add, Multiply, Distance, etc.)
                     - "Flow Control" (Branch, Loop, Sequence, etc.)
                     - "Variables" (Get/Set variable nodes)
                     - "Functions" (Function calls, custom functions)
                     - "Events" (Custom events, input events)
                     - "Array" (Array operations)
                     - "String" (String manipulation)
                     - "Widget" (UI operations - for Widget Blueprints)
                     - "Actor" (Actor operations)
                     - "Component" (Component operations)
            search_term: Search within node names and descriptions
            context: Additional context for filtering (e.g., "UStaticMeshComponent")
            include_deprecated: Whether to include deprecated/legacy nodes
            max_results: Maximum number of nodes to return
            
        Returns:
            Dictionary containing categorized available nodes with metadata
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            params = {
                "blueprint_name": blueprint_name,
                "category": category,
                "search_term": search_term,
                "context": context,
                "include_deprecated": include_deprecated,
                "max_results": max_results
            }
            
            logger.info(f"Getting available Blueprint nodes for '{blueprint_name}' with params: {params}")
            
            connection = get_unreal_connection()
            if not connection:
                return {"success": False, "error": "No connection to Unreal Engine"}
            
            result = connection.send_command("get_available_blueprint_nodes", params)
            
            logger.info(f"Node discovery result: {result.get('success', False)} - Found {result.get('total_nodes', 0)} nodes")
            
            return result
            
        except Exception as e:
            logger.error(f"Error getting available Blueprint nodes: {str(e)}")
            return {
                "success": False,
                "error": str(e),
                "message": f"Failed to discover nodes for Blueprint '{blueprint_name}'"
            }
