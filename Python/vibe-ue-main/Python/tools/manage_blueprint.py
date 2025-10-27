"""
Blueprint Manager - Unified multi-action tool for Blueprint lifecycle management.

Consolidates Blueprint creation, compilation, inspection, property management, and reparenting.
Follows the established multi-action pattern from manage_blueprint_node, manage_blueprint_function, etc.
"""

import logging
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def register_blueprint_tools(mcp: FastMCP):
    """Register unified Blueprint manager tool with the MCP server."""

    @mcp.tool()
    def manage_blueprint(
        ctx: Context,
        action: str,
        blueprint_name: str = "",
        name: str = "",
        parent_class: str = "",
        property_name: str = "",
        property_value: str = "",
        new_parent_class: str = "",
        max_nodes: int = 200
    ) -> Dict[str, Any]:
        """
         **UNIFIED BLUEPRINT LIFECYCLE MANAGER**
        
        Single multi-action tool for all Blueprint lifecycle operations.
        Replaces: create_blueprint, compile_blueprint, get_blueprint_info, 
                 set_blueprint_property, reparent_blueprint, list_custom_events,
                 summarize_event_graph
        
         **Available Actions:**
        
        **create** - Create new Blueprint
        ```python
        manage_blueprint(
            action="create",
            name="BP_MyActor",
            parent_class="Actor"
        )
        
        ️ CRITICAL DEPENDENCY ORDER after creation:
        1) Variables FIRST - Create all Blueprint variables
        2) Components SECOND - Add all components  
        3) Functions THIRD - Implement custom functions
        4) Event Graph nodes LAST - Create logic
        ```
        
        **compile** - Compile Blueprint
        ```python
        manage_blueprint(
            action="compile",
            blueprint_name="/Game/Blueprints/BP_Player"
        )
        ```
        
        **get_info** - Get comprehensive Blueprint information
        ```python
        manage_blueprint(
            action="get_info",
            blueprint_name="/Game/Blueprints/BP_Player"
        )
        # Returns: variables, components, functions, event graphs, properties
        ```
        
        **set_property** - Set Blueprint class default property
        ```python
        manage_blueprint(
            action="set_property",
            blueprint_name="/Game/Blueprints/BP_Player",
            property_name="MaxHealth",
            property_value="100.0"
        )
        ```
        
        **reparent** - Change Blueprint parent class
        ```python
        manage_blueprint(
            action="reparent",
            blueprint_name="/Game/Blueprints/BP_Player",
            new_parent_class="ProteusCharacter"
        )
        # Often fixes component hierarchy issues automatically
        ```
        
        **list_custom_events** - List all custom events in Blueprint
        ```python
        manage_blueprint(
            action="list_custom_events",
            blueprint_name="/Game/Blueprints/BP_Player"
        )
        # Returns: {"events": ["OnPlayerDeath", "OnHealthChanged", ...]}
        ```
        
        **summarize_event_graph** - Get readable outline of Event Graph
        ```python
        manage_blueprint(
            action="summarize_event_graph",
            blueprint_name="/Game/Blueprints/BP_Player",
            max_nodes=200
        )
        # Returns: {"summary": "... formatted graph outline ..."}
        ```
        
        Args:
            action: Action to perform (create|compile|get_info|set_property|reparent|list_custom_events|summarize_event_graph)
            blueprint_name: Target Blueprint name/path (required for most actions)
            name: Blueprint name for create action
            parent_class: Parent class for create action
            property_name: Property name for set_property action
            property_value: Property value for set_property action
            new_parent_class: New parent class for reparent action
            max_nodes: Maximum nodes to include in summary (for summarize_event_graph)
            
        Returns:
            Dict containing action results with success field
        """
        from vibe_ue_server import get_unreal_connection
        
        action = action.lower()
        
        # Route to appropriate handler
        if action == "create":
            return _handle_create(name, parent_class)
        elif action == "compile":
            return _handle_compile(blueprint_name)
        elif action == "get_info":
            return _handle_get_info(blueprint_name)
        elif action == "set_property":
            return _handle_set_property(blueprint_name, property_name, property_value)
        elif action == "reparent":
            return _handle_reparent(blueprint_name, new_parent_class)
        elif action == "list_custom_events":
            return _handle_list_custom_events(blueprint_name)
        elif action == "summarize_event_graph":
            return _handle_summarize_event_graph(blueprint_name, max_nodes, ctx)
        else:
            return {
                "success": False,
                "error": f"Unknown action '{action}'. Valid actions: create, compile, get_info, set_property, reparent, list_custom_events, summarize_event_graph"
            }


def _handle_create(name: str, parent_class: str) -> Dict[str, Any]:
    """Handle Blueprint creation."""
    from vibe_ue_server import get_unreal_connection
    
    if not name or not parent_class:
        return {
            "success": False,
            "error": "Both 'name' and 'parent_class' are required for create action"
        }
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        response = unreal.send_command("create_blueprint", {
            "name": name,
            "parent_class": parent_class
        })
        
        if not response:
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"Blueprint creation response: {response}")
        
        # Add dependency order reminder
        result = response or {}
        if result.get("path") or result.get("name"):
            logger.info("️ REMINDER: Create Blueprint elements in DEPENDENCY ORDER: 1) Variables FIRST, 2) Components SECOND, 3) Functions THIRD, 4) Event Graph nodes LAST")
            result["reminder"] = "Create in order: Variables → Components → Functions → Event Graph"
            result["critical_order"] = "Variables FIRST, then Components, then Functions, then Event Graph LAST"
        
        return result
        
    except Exception as e:
        logger.error(f"Error creating blueprint: {e}")
        return {"success": False, "error": str(e)}


def _handle_compile(blueprint_name: str) -> Dict[str, Any]:
    """Handle Blueprint compilation."""
    from vibe_ue_server import get_unreal_connection
    
    if not blueprint_name:
        return {
            "success": False,
            "error": "'blueprint_name' is required for compile action"
        }
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Compiling blueprint: {blueprint_name}")
        response = unreal.send_command("compile_blueprint", {
            "blueprint_name": blueprint_name
        })
        
        if not response:
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"Compile blueprint response: {response}")
        return response
        
    except Exception as e:
        logger.error(f"Error compiling blueprint: {e}")
        return {"success": False, "error": str(e)}


def _handle_get_info(blueprint_name: str) -> Dict[str, Any]:
    """Handle Blueprint info retrieval."""
    from vibe_ue_server import get_unreal_connection
    
    if not blueprint_name:
        return {
            "success": False,
            "error": "'blueprint_name' is required for get_info action"
        }
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Getting Blueprint info: {blueprint_name}")
        response = unreal.send_command("get_blueprint_info", {
            "blueprint_name": blueprint_name
        })
        
        if not response:
            return {"success": False, "error": "No response from Unreal Engine"}
        
        return response
        
    except Exception as e:
        logger.error(f"Error getting blueprint info: {e}")
        return {"success": False, "error": str(e)}


def _handle_set_property(
    blueprint_name: str,
    property_name: str,
    property_value: str
) -> Dict[str, Any]:
    """Handle Blueprint property setting."""
    from vibe_ue_server import get_unreal_connection
    
    if not blueprint_name or not property_name:
        return {
            "success": False,
            "error": "'blueprint_name' and 'property_name' are required for set_property action"
        }
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Setting property {property_name} on {blueprint_name}")
        response = unreal.send_command("set_blueprint_property", {
            "blueprint_name": blueprint_name,
            "property_name": property_name,
            "property_value": property_value
        })
        
        if not response:
            return {"success": False, "error": "No response from Unreal Engine"}
        
        return response
        
    except Exception as e:
        logger.error(f"Error setting blueprint property: {e}")
        return {"success": False, "error": str(e)}


def _handle_reparent(blueprint_name: str, new_parent_class: str) -> Dict[str, Any]:
    """Handle Blueprint reparenting."""
    from vibe_ue_server import get_unreal_connection
    
    if not blueprint_name or not new_parent_class:
        return {
            "success": False,
            "error": "'blueprint_name' and 'new_parent_class' are required for reparent action"
        }
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        logger.info(f"Reparenting {blueprint_name} to {new_parent_class}")
        response = unreal.send_command("reparent_blueprint", {
            "blueprint_name": blueprint_name,
            "new_parent_class": new_parent_class
        })
        
        if not response:
            return {"success": False, "error": "No response from Unreal Engine"}
        
        logger.info(f"Reparent response: {response}")
        return response
        
    except Exception as e:
        logger.error(f"Error reparenting blueprint: {e}")
        return {"success": False, "error": str(e)}


def _handle_list_custom_events(blueprint_name: str) -> Dict[str, Any]:
    """Handle listing custom events."""
    from vibe_ue_server import get_unreal_connection
    
    if not blueprint_name:
        return {
            "success": False,
            "error": "'blueprint_name' is required for list_custom_events action"
        }
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine", "events": []}
        
        logger.info(f"Listing custom events for '{blueprint_name}'")
        response = unreal.send_command("list_custom_events", {"blueprint_name": blueprint_name})
        
        if not response:
            return {"success": False, "error": "No response from Unreal Engine", "events": []}
        
        return response
        
    except Exception as e:
        logger.error(f"Error listing custom events: {e}")
        return {"success": False, "error": str(e), "events": []}


def _handle_summarize_event_graph(blueprint_name: str, max_nodes: int, ctx) -> Dict[str, Any]:
    """Handle event graph summarization."""
    from vibe_ue_server import get_unreal_connection
    from typing import List
    
    if not blueprint_name:
        return {
            "success": False,
            "error": "'blueprint_name' is required for summarize_event_graph action"
        }
    
    try:
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine", "summary": ""}
        
        # Use manage_blueprint_node with action="list" to get nodes
        params = {
            "action": "list",
            "blueprint_name": blueprint_name,
            "include_functions": True,
            "include_macros": True,
            "include_timeline": True,
        }
        nodes_resp = unreal.send_command("manage_blueprint_node", params)
        
        if not nodes_resp or nodes_resp.get("success") is False:
            return {"success": False, "error": nodes_resp.get("error", "No nodes"), "summary": ""}
        
        nodes = nodes_resp.get("nodes", [])[:max_nodes]
        lines: List[str] = []
        lines.append(f"Blueprint: {blueprint_name}")
        lines.append(f"Nodes: {len(nodes)} (showing up to {max_nodes})")
        
        for node in nodes:
            ntype = node.get("node_type") or node.get("type")
            title = node.get("title") or node.get("name") or node.get("id")
            nid = node.get("id")
            lines.append(f"- [{ntype}] {title}")
            
            # Fetch details for select nodes
            if nid:
                detail_resp = unreal.send_command("get_node_details", {
                    "blueprint_name": blueprint_name,
                    "node_id": nid
                })
                if detail_resp and detail_resp.get("success", True):
                    d = detail_resp.get("node", {})
                    pins = d.get("pins", [])
                    calls = [p for p in pins if p.get("direction") == "Output"]
                    if calls:
                        for p in calls[:3]:
                            connections = [c.get('to_node_id', '') for c in p.get('connections', [])]
                            if connections:
                                lines.append(f"  -> {p.get('name')} -> {','.join(connections)}")
        
        return {"success": True, "summary": "\n".join(lines)}
        
    except Exception as e:
        logger.error(f"Error summarizing event graph: {e}")
        return {"success": False, "error": str(e), "summary": ""}
