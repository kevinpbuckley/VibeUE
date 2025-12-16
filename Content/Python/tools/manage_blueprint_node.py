"""
Blueprint node management for VibeUE MCP server.

Provides operations for discovering, creating, connecting, and managing Blueprint nodes.
"""

import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context, FastMCP

logger = logging.getLogger("UnrealMCP")


def _merge(target: Dict[str, Any], values: Dict[str, Any]) -> None:
    """Merge non-None values into target in-place."""
    for key, value in values.items():
        if value is not None:
            target[key] = value


def register_node_tools(mcp: FastMCP) -> None:
    """Register Blueprint node MCP tools."""

    def _dispatch(command: str, payload: Dict[str, Any]) -> Dict[str, Any]:
        from vibe_ue_server import get_unreal_connection

        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return {"success": False, "error": "Failed to connect to Unreal Engine"}

        logger.info("Dispatching %s with payload: %s", command, payload)
        response = unreal.send_command(command, payload)
        if not response:
            logger.error("No response from Unreal Engine")
            return {"success": False, "error": "No response from Unreal Engine"}

        logger.debug("%s response: %s", command, response)
        return response

    @mcp.tool(description="""Blueprint node operations: discover, create, connect, delete, move.

WORKFLOW:
1. discover: Search for nodes by keyword -> returns spawner_key
2. create: Create node using spawner_key from discover
3. connect: Wire nodes together using node_id and pin names
4. list: See existing nodes in a graph

COMMON ACTIONS:
- discover: Find available nodes. REQUIRED: search_term (e.g., "print", "branch", "delay")
- create: Add a node. REQUIRED: blueprint_name, spawner_key (from discover), position
- connect: Wire nodes. REQUIRED: source_node_id, source_pin, target_node_id, target_pin
- list: Get nodes in graph. REQUIRED: blueprint_name
- delete: Remove node. REQUIRED: blueprint_name, node_id

IMPORTANT:
- Basic math operators (+, -, *, /) are NOT available as discoverable nodes
- Always use spawner_key from discover results to create nodes
- Use function_name to target function graphs instead of EventGraph""")
    def manage_blueprint_node(
        ctx: Context,
        action: str,
        blueprint_name: str = "",
        # Discovery parameters
        search_term: Optional[str] = None,
        # Node creation parameters  
        spawner_key: Optional[str] = None,
        position: Optional[List[float]] = None,
        # Graph targeting
        function_name: Optional[str] = None,
        graph_scope: str = "event",
        # Node operations (need node_id)
        node_id: Optional[str] = None,
        # Connection parameters
        source_node_id: Optional[str] = None,
        source_pin: Optional[str] = None,
        target_node_id: Optional[str] = None,
        target_pin: Optional[str] = None,
        # Property configuration
        property_name: Optional[str] = None,
        property_value: Optional[Any] = None,
        # Help
        help_action: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Execute Blueprint node operations."""
        
        from help_system import generate_error_response, generate_help_response
        
        # Handle help
        if action and action.lower() == "help":
            return generate_help_response("manage_blueprint_node", help_action)
        
        if not action:
            return generate_error_response(
                "manage_blueprint_node", "",
                "action is required. Common actions: discover, create, connect, list, delete"
            )
        
        action_lower = action.lower()
        
        # ===== NORMALIZE ACTION ALIASES =====
        # Map common AI mistakes to correct actions
        action_aliases = {
            "connect_pins": "connect",
            "get_pins": "list",
            "get_details": "details",
            "describe": "details",
            "search": "discover",
            "find": "discover",
            "add": "create",
            "remove": "delete",
        }
        if action_lower in action_aliases:
            old_action = action_lower
            action_lower = action_aliases[action_lower]
            logger.info(f"Action alias: '{old_action}' -> '{action_lower}'")
        
        # ===== DISCOVER ACTION =====
        if action_lower == "discover":
            if not search_term:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "discover requires 'search_term' (e.g., 'print', 'branch', 'delay')",
                    missing_params=["search_term"]
                )
            if not blueprint_name:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "discover requires 'blueprint_name'",
                    missing_params=["blueprint_name"]
                )
            
            logger.info(f"Discover: search_term='{search_term}'")
            
            # Determine graph scope
            effective_scope = "function" if function_name else graph_scope
            
            return _dispatch("get_available_blueprint_nodes", {
                "blueprint_name": blueprint_name,
                "search_term": search_term,
                "category": "",
                "graph_scope": effective_scope,
                "include_functions": True,
                "include_variables": True,
                "include_events": True,
                "max_results": 15,
                "return_descriptors": True,
                "compact": True,
            })
        
        # ===== CREATE ACTION =====
        if action_lower == "create":
            if not blueprint_name:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "create requires 'blueprint_name'",
                    missing_params=["blueprint_name"]
                )
            if not spawner_key:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "create requires 'spawner_key' from discover action",
                    missing_params=["spawner_key"]
                )
            
            logger.info(f"Create: spawner_key='{spawner_key}'")
            
            # Auto-detect graph scope
            effective_scope = "function" if function_name else graph_scope
            
            payload = {
                "blueprint_name": blueprint_name,
                "action": "create",
                "spawner_key": spawner_key,
                "graph_scope": effective_scope,
            }
            if function_name:
                payload["function_name"] = function_name
            if position:
                payload["position"] = position
            
            return _dispatch("manage_blueprint_node", payload)
        
        # ===== CONNECT ACTION =====
        if action_lower == "connect":
            missing = []
            if not source_node_id:
                missing.append("source_node_id")
            if not source_pin:
                missing.append("source_pin")
            if not target_node_id:
                missing.append("target_node_id")
            if not target_pin:
                missing.append("target_pin")
            if missing:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    f"connect requires: {', '.join(missing)}",
                    missing_params=missing
                )
            if not blueprint_name:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "connect requires 'blueprint_name'",
                    missing_params=["blueprint_name"]
                )
            
            effective_scope = "function" if function_name else graph_scope
            
            payload = {
                "blueprint_name": blueprint_name,
                "action": "connect",
                "source_node_id": source_node_id,
                "source_pin": source_pin,
                "target_node_id": target_node_id,
                "target_pin": target_pin,
                "graph_scope": effective_scope,
            }
            if function_name:
                payload["function_name"] = function_name
            
            return _dispatch("manage_blueprint_node", payload)
        
        # ===== LIST ACTION =====
        if action_lower == "list":
            if not blueprint_name:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "list requires 'blueprint_name'",
                    missing_params=["blueprint_name"]
                )
            
            effective_scope = "function" if function_name else graph_scope
            
            payload = {
                "blueprint_name": blueprint_name,
                "action": "list",
                "graph_scope": effective_scope,
            }
            if function_name:
                payload["function_name"] = function_name
            
            return _dispatch("manage_blueprint_node", payload)
        
        # ===== DELETE ACTION =====
        if action_lower == "delete":
            if not blueprint_name:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "delete requires 'blueprint_name'",
                    missing_params=["blueprint_name"]
                )
            if not node_id:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "delete requires 'node_id'",
                    missing_params=["node_id"]
                )
            
            effective_scope = "function" if function_name else graph_scope
            
            payload = {
                "blueprint_name": blueprint_name,
                "action": "delete",
                "node_id": node_id,
                "graph_scope": effective_scope,
            }
            if function_name:
                payload["function_name"] = function_name
            
            return _dispatch("manage_blueprint_node", payload)
        
        # ===== DETAILS/GET ACTION =====
        if action_lower in ["details", "get"]:
            if not blueprint_name:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    f"{action} requires 'blueprint_name'",
                    missing_params=["blueprint_name"]
                )
            if not node_id:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    f"{action} requires 'node_id'",
                    missing_params=["node_id"]
                )
            
            effective_scope = "function" if function_name else graph_scope
            
            payload = {
                "blueprint_name": blueprint_name,
                "action": "details",
                "node_id": node_id,
                "graph_scope": effective_scope,
            }
            if function_name:
                payload["function_name"] = function_name
            
            return _dispatch("manage_blueprint_node", payload)
        
        # ===== SET_PROPERTY/CONFIGURE ACTION =====
        if action_lower in ["set_property", "configure"]:
            if not blueprint_name:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    f"{action} requires 'blueprint_name'",
                    missing_params=["blueprint_name"]
                )
            if not node_id:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    f"{action} requires 'node_id'",
                    missing_params=["node_id"]
                )
            if not property_name:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    f"{action} requires 'property_name'",
                    missing_params=["property_name"]
                )
            
            effective_scope = "function" if function_name else graph_scope
            
            payload = {
                "blueprint_name": blueprint_name,
                "action": "set_property",
                "node_id": node_id,
                "property_name": property_name,
                "property_value": property_value,
                "graph_scope": effective_scope,
            }
            if function_name:
                payload["function_name"] = function_name
            
            return _dispatch("manage_blueprint_node", payload)
        
        # ===== DISCONNECT ACTION =====
        if action_lower == "disconnect":
            if not blueprint_name:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "disconnect requires 'blueprint_name'",
                    missing_params=["blueprint_name"]
                )
            if not node_id:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "disconnect requires 'node_id'",
                    missing_params=["node_id"]
                )
            
            effective_scope = "function" if function_name else graph_scope
            
            payload = {
                "blueprint_name": blueprint_name,
                "action": "disconnect",
                "node_id": node_id,
                "graph_scope": effective_scope,
            }
            if function_name:
                payload["function_name"] = function_name
            if source_pin:
                payload["source_pin"] = source_pin
            
            return _dispatch("manage_blueprint_node", payload)
        
        # ===== MOVE ACTION =====
        if action_lower == "move":
            if not blueprint_name:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "move requires 'blueprint_name'",
                    missing_params=["blueprint_name"]
                )
            if not node_id:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "move requires 'node_id'",
                    missing_params=["node_id"]
                )
            if not position:
                return generate_error_response(
                    "manage_blueprint_node", action,
                    "move requires 'position' [x, y]",
                    missing_params=["position"]
                )
            
            effective_scope = "function" if function_name else graph_scope
            
            payload = {
                "blueprint_name": blueprint_name,
                "action": "move",
                "node_id": node_id,
                "position": position,
                "graph_scope": effective_scope,
            }
            if function_name:
                payload["function_name"] = function_name
            
            return _dispatch("manage_blueprint_node", payload)
        
        # ===== FALLBACK: Pass through to C++ =====
        # For any other actions, pass through with all parameters
        effective_scope = "function" if function_name else graph_scope
        
        payload = {
            "blueprint_name": blueprint_name,
            "action": action,
            "graph_scope": effective_scope,
        }
        
        _merge(payload, {
            "function_name": function_name,
            "node_id": node_id,
            "spawner_key": spawner_key,
            "position": position,
            "source_node_id": source_node_id,
            "source_pin": source_pin,
            "target_node_id": target_node_id,
            "target_pin": target_pin,
            "property_name": property_name,
            "property_value": property_value,
        })
        
        logger.info(f"Fallback dispatch for action '{action}': {payload}")
        return _dispatch("manage_blueprint_node", payload)

    logger.info("Blueprint node tools registered")
