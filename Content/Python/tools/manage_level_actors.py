"""
Level Actor Manager - Phases 1-4

Unified multi-action tool for level actor management.

Phase 1 Actions: add, remove, list, find, get_info
Phase 2 Actions: set_transform, get_transform, set_location, set_rotation, set_scale
Phase 3 Actions: get_property, set_property, get_all_properties
Phase 4 Actions: set_folder, attach, detach, select, rename

REFACTORED: Consolidated 45 parameters down to 10 core params + extra dict
for improved MCP tool stability.
"""

import logging
from typing import Dict, Any, List, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def register_level_actor_tools(mcp: FastMCP):
    """Register level actor management tool with MCP server."""

    @mcp.tool(description="Level actor operations: add/remove, transform, properties, hierarchy. Actions: add, remove, list, find, get_info, set_transform, get_transform, set_location, set_rotation, set_scale, focus, move_to_view, refresh_viewport, get_property, set_property, get_all_properties, set_folder, attach, detach, select, rename. For 'add' action, actor_class is REQUIRED (e.g. '/Script/Engine.SpotLight'). Use action='help' for detailed parameter info.")
    def manage_level_actors(
        ctx: Context,
        action: str,
        help_action: str = "",
        # Core actor identification (most common)
        actor_label: str = "",
        actor_path: str = "",
        # Actor class for 'add' action (e.g. '/Script/Engine.SpotLight', '/Script/Engine.PointLight')
        actor_class: str = "",
        # Core transform (used by many actions)
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        # Core property access
        property_path: str = "",
        property_value: str = "",
        # All other action-specific parameters
        extra: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Route to level actor action handlers."""
        # Handle help action
        if action and action.lower() == "help":
            from help_system import generate_help_response
            return generate_help_response("manage_level_actors", help_action if help_action else None)
        
        from vibe_ue_server import get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        # Initialize extra if None
        if extra is None:
            extra = {}
        
        # Validate action
        valid_actions = [
            # Phase 1: Basic operations
            "add", "remove", "list", "find", "get_info",
            # Phase 2: Transform operations
            "set_transform", "get_transform", "set_location", "set_rotation", "set_scale",
            # Editor view operations
            "focus", "move_to_view", "refresh_viewport",
            # Phase 3: Property operations
            "get_property", "set_property", "get_all_properties",
            # Phase 4: Hierarchy and organization
            "set_folder", "attach", "detach", "select", "rename"
        ]
        action_lower = action.lower()
        if action_lower not in valid_actions:
            return {
                "success": False,
                "error": f"Unknown action: {action}. Supported: {', '.join(valid_actions)}",
                "help_tip": "Use manage_level_actors(action='help') to see all available actions and their parameters."
            }
        
        # Validate actor identification for actions that require it
        actions_requiring_actor = [
            "remove", "get_info", "set_transform", "get_transform", 
            "set_location", "set_rotation", "set_scale",
            "focus", "move_to_view", "get_property", "set_property", 
            "get_all_properties", "set_folder", "attach", "detach", "select", "rename"
        ]
        if action_lower in actions_requiring_actor:
            has_actor_id = actor_label or actor_path or extra.get("actor_guid") or extra.get("actor_tag")
            if not has_actor_id:
                from help_system import generate_error_response
                return generate_error_response(
                    tool_name="manage_level_actors",
                    action=action_lower,
                    error_message=f"Actor identification required for '{action_lower}' action. Provide actor_label, actor_path, or extra.actor_guid",
                    missing_params=["actor_label or actor_path"]
                )
        
        # Build parameters - start with identification
        params = {
            "action": action_lower,
            "actor_path": actor_path,
            "actor_label": actor_label,
            "actor_guid": extra.get("actor_guid", ""),
            "actor_tag": extra.get("actor_tag", ""),
        }
        
        # Add action params
        if action_lower == "add":
            # Use top-level actor_class parameter, fall back to extra dict for backwards compatibility
            effective_actor_class = actor_class or extra.get("actor_class", "")
            if not effective_actor_class:
                from help_system import generate_error_response
                return generate_error_response(
                    tool_name="manage_level_actors",
                    action="add",
                    error_message="actor_class parameter is required for 'add' action",
                    missing_params=["actor_class"]
                )
            params["actor_class"] = effective_actor_class
            params["actor_name"] = extra.get("actor_name", "")
            if location:
                params["spawn_location"] = location
            spawn_rotation = extra.get("spawn_rotation")
            if spawn_rotation:
                params["spawn_rotation"] = spawn_rotation
            spawn_scale = extra.get("spawn_scale")
            if spawn_scale:
                params["spawn_scale"] = spawn_scale
            tags = extra.get("tags")
            if tags:
                params["tags"] = tags
        
        # Query params
        if action_lower in ["list", "find"]:
            params["class_filter"] = extra.get("class_filter", "")
            params["label_filter"] = extra.get("label_filter", "")
            params["selected_only"] = extra.get("selected_only", False)
            params["max_results"] = extra.get("max_results", 100)
            required_tags = extra.get("required_tags")
            if required_tags:
                params["required_tags"] = required_tags
            excluded_tags = extra.get("excluded_tags")
            if excluded_tags:
                params["excluded_tags"] = excluded_tags
        
        # Get info params
        if action_lower == "get_info":
            params["include_components"] = extra.get("include_components", True)
            params["include_properties"] = extra.get("include_properties", True)
            params["category_filter"] = extra.get("category_filter", "")
        
        # Remove params
        if action_lower == "remove":
            params["with_undo"] = extra.get("with_undo", True)
        
        # Phase 2: Transform params
        if action_lower == "set_transform":
            if location:
                params["location"] = location
            if rotation:
                params["rotation"] = rotation
            if scale:
                params["scale"] = scale
            params["world_space"] = extra.get("world_space", True)
            params["sweep"] = extra.get("sweep", False)
            params["teleport"] = extra.get("teleport", False)
        
        if action_lower == "set_location":
            if location:
                params["location"] = location
            else:
                from help_system import generate_error_response
                return generate_error_response(
                    tool_name="manage_level_actors",
                    action="set_location",
                    error_message="location parameter is required for 'set_location' action",
                    missing_params=["location"]
                )
            params["world_space"] = extra.get("world_space", True)
            params["sweep"] = extra.get("sweep", False)
        
        if action_lower == "set_rotation":
            if rotation:
                params["rotation"] = rotation
            else:
                from help_system import generate_error_response
                return generate_error_response(
                    tool_name="manage_level_actors",
                    action="set_rotation",
                    error_message="rotation parameter is required for 'set_rotation' action",
                    missing_params=["rotation"]
                )
            params["world_space"] = extra.get("world_space", True)
        
        if action_lower == "set_scale":
            if scale:
                params["scale"] = scale
            else:
                from help_system import generate_error_response
                return generate_error_response(
                    tool_name="manage_level_actors",
                    action="set_scale",
                    error_message="scale parameter is required for 'set_scale' action",
                    missing_params=["scale"]
                )
        
        # Editor view operations
        if action_lower == "focus":
            params["instant"] = extra.get("instant", False)
        
        # Phase 3: Property operations
        if action_lower in ["get_property", "set_property", "get_all_properties"]:
            params["property_path"] = property_path
            params["component_name"] = extra.get("component_name", "")
            params["include_inherited"] = extra.get("include_inherited", True)
            params["category_filter"] = extra.get("category_filter", "")
            
            if action_lower == "set_property":
                params["property_value"] = property_value
        
        # Phase 4: Hierarchy & Organization
        if action_lower == "set_folder":
            params["folder_path"] = extra.get("folder_path", "")
        
        if action_lower == "attach":
            params["parent_path"] = extra.get("parent_path", "")
            params["parent_label"] = extra.get("parent_label", "")
            params["parent_guid"] = extra.get("parent_guid", "")
            params["parent_tag"] = extra.get("parent_tag", "")
            params["socket_name"] = extra.get("socket_name", "")
            params["weld_simulated_bodies"] = extra.get("weld_simulated_bodies", True)
        
        if action_lower == "select":
            params["add_to_selection"] = extra.get("add_to_selection", False)
            params["deselect"] = extra.get("deselect", False)
            params["deselect_all"] = extra.get("deselect_all", False)
            actors_list = extra.get("actors")
            if actors_list:
                params["actors"] = actors_list
        
        if action_lower == "rename":
            params["new_label"] = extra.get("new_label", "")
        
        try:
            result = unreal.send_command("manage_level_actors", params)
            return result
        except Exception as e:
            logger.error(f"manage_level_actors error: {e}")
            return {"success": False, "error": str(e)}
