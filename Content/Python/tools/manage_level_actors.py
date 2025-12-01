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

    @mcp.tool()
    def manage_level_actors(
        ctx: Context,
        action: str,
        # Core actor identification (most common)
        actor_label: str = "",
        actor_path: str = "",
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
        """
        Level Actor Manager Tool - Phases 1-4
        
        Manage actors in the current level with full undo support.
        
        ## Core Parameters (always available):
        - action: The operation to perform
        - actor_label: Actor display name (most common identifier)
        - actor_path: Full actor path (most precise identifier)
        - location: [X, Y, Z] for transform operations
        - rotation: [Pitch, Yaw, Roll] for transform operations  
        - scale: [X, Y, Z] for transform operations
        - property_path: Property name for get/set_property
        - property_value: Value for set_property
        - extra: Dict with action-specific parameters
        
        ## Available Actions:
        
        ### Phase 1 - Core Operations
        
        **add** - Add a new actor to the level
        ```python
        manage_level_actors(
            action="add",
            location=[100, 200, 300],
            extra={
                "actor_class": "PointLight",
                "actor_name": "MainLight",
                "tags": ["Lighting", "Indoor"],
                "spawn_rotation": [0, 45, 0],
                "spawn_scale": [1, 1, 1]
            }
        )
        ```
        
        **remove** - Remove an actor from the level
        ```python
        manage_level_actors(action="remove", actor_label="MainLight")
        ```
        
        **list** - List actors with optional filtering
        ```python
        manage_level_actors(
            action="list",
            extra={"class_filter": "*Light*", "max_results": 50}
        )
        ```
        
        **find** - Find actors matching criteria
        ```python
        manage_level_actors(
            action="find",
            extra={"required_tags": ["Enemy"], "class_filter": "BP_Enemy*"}
        )
        ```
        
        **get_info** - Get comprehensive actor information
        ```python
        manage_level_actors(
            action="get_info",
            actor_label="PointLight_1",
            extra={"include_components": True, "include_properties": True}
        )
        ```
        
        ### Phase 2 - Transform Operations
        
        **set_transform** - Set full transform (location, rotation, scale)
        ```python
        manage_level_actors(
            action="set_transform",
            actor_label="MyActor",
            location=[100, 200, 300],
            rotation=[0, 45, 0],
            scale=[2, 2, 2],
            extra={"world_space": True, "sweep": False}
        )
        ```
        
        **get_transform** - Get complete transform information
        ```python
        manage_level_actors(action="get_transform", actor_label="MyActor")
        ```
        
        **set_location** - Set actor location only
        ```python
        manage_level_actors(
            action="set_location",
            actor_label="MyActor",
            location=[500, 0, 100]
        )
        ```
        
        **set_rotation** - Set actor rotation only
        ```python
        manage_level_actors(
            action="set_rotation",
            actor_label="MyActor",
            rotation=[0, 90, 0]
        )
        ```
        
        **set_scale** - Set actor scale only
        ```python
        manage_level_actors(action="set_scale", actor_label="MyActor", scale=[2, 2, 2])
        ```
        
        **focus** - Focus viewport camera on an actor
        ```python
        manage_level_actors(action="focus", actor_label="MyActor")
        ```
        
        **move_to_view** - Move actor to viewport center
        ```python
        manage_level_actors(action="move_to_view", actor_label="MyActor")
        ```
        
        ### Phase 3 - Property Operations
        
        **get_property** - Get a single property value
        ```python
        manage_level_actors(
            action="get_property",
            actor_label="PointLight_1",
            property_path="Intensity",
            extra={"component_name": "LightComponent0"}
        )
        ```
        
        **set_property** - Set a single property value
        ```python
        manage_level_actors(
            action="set_property",
            actor_label="PointLight_1",
            property_path="LightColor",
            property_value="(R=255,G=180,B=100,A=255)"
        )
        ```
        
        **get_all_properties** - Get all properties with filtering
        ```python
        manage_level_actors(
            action="get_all_properties",
            actor_label="PointLight_1",
            extra={"component_name": "LightComponent0", "category_filter": "Light"}
        )
        ```
        
        ### Phase 4 - Hierarchy & Organization
        
        **set_folder** - Set actor folder in World Outliner
        ```python
        manage_level_actors(
            action="set_folder",
            actor_label="MyLight",
            extra={"folder_path": "Lighting/Indoor"}
        )
        ```
        
        **attach** - Attach actor to a parent
        ```python
        manage_level_actors(
            action="attach",
            actor_label="ChildActor",
            extra={"parent_label": "ParentActor", "socket_name": ""}
        )
        ```
        
        **detach** - Detach actor from parent
        ```python
        manage_level_actors(action="detach", actor_label="ChildActor")
        ```
        
        **select** - Select/deselect actors
        ```python
        manage_level_actors(action="select", actor_label="MyActor")
        manage_level_actors(action="select", extra={"actors": ["A1", "A2"], "add_to_selection": True})
        manage_level_actors(action="select", extra={"deselect_all": True})
        ```
        
        **rename** - Rename an actor's label
        ```python
        manage_level_actors(
            action="rename",
            actor_label="OldName",
            extra={"new_label": "NewName"}
        )
        ```
        
        ## Extra Dict Parameters by Action:
        
        - **add**: actor_class, actor_name, tags, spawn_rotation, spawn_scale
        - **list/find**: class_filter, label_filter, required_tags, excluded_tags, selected_only, max_results
        - **get_info**: include_components, include_properties, category_filter
        - **remove**: with_undo
        - **transforms**: world_space, sweep, teleport
        - **focus**: instant
        - **properties**: component_name, include_inherited, category_filter
        - **set_folder**: folder_path
        - **attach**: parent_label, parent_path, parent_guid, parent_tag, socket_name, weld_simulated_bodies
        - **select**: actors, add_to_selection, deselect, deselect_all
        - **rename**: new_label
        - **identification**: actor_guid, actor_tag (alternative to actor_label/actor_path)
        
        Args:
            action: Action to perform (see actions above)
            actor_label: Actor display name for identification
            actor_path: Full actor path for identification
            location: [X, Y, Z] for add/transform operations
            rotation: [Pitch, Yaw, Roll] for transform operations
            scale: [X, Y, Z] for transform operations
            property_path: Property name for get/set_property
            property_value: Value string for set_property
            extra: Action-specific parameters (see docs above)
            
        Returns:
            Dict with success status and action-specific results
        """
        from vibe_ue_server import get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
        # Initialize extra if None
        if extra is None:
            extra = {}
        
        # Validate action
        valid_actions = [
            # Phase 1
            "add", "remove", "list", "find", "get_info",
            # Phase 2
            "set_transform", "get_transform", "set_location", "set_rotation", "set_scale",
            # Editor view
            "focus", "move_to_view", "refresh_viewport",
            # Phase 3
            "get_property", "set_property", "get_all_properties",
            # Phase 4
            "set_folder", "attach", "detach", "select", "rename"
        ]
        action_lower = action.lower()
        if action_lower not in valid_actions:
            return {
                "success": False,
                "error": f"Unknown action: {action}. Supported: {', '.join(valid_actions)}"
            }
        
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
            params["actor_class"] = extra.get("actor_class", "")
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
                return {"success": False, "error": "location parameter is required for set_location"}
            params["world_space"] = extra.get("world_space", True)
            params["sweep"] = extra.get("sweep", False)
        
        if action_lower == "set_rotation":
            if rotation:
                params["rotation"] = rotation
            else:
                return {"success": False, "error": "rotation parameter is required for set_rotation"}
            params["world_space"] = extra.get("world_space", True)
        
        if action_lower == "set_scale":
            if scale:
                params["scale"] = scale
            else:
                return {"success": False, "error": "scale parameter is required for set_scale"}
        
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
