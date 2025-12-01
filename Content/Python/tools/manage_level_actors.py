"""
Level Actor Manager - Phases 1-4

Unified multi-action tool for level actor management.

Phase 1 Actions: add, remove, list, find, get_info
Phase 2 Actions: set_transform, get_transform, set_location, set_rotation, set_scale
Phase 3 Actions: get_property, set_property, get_all_properties
Phase 4 Actions: set_folder, attach, detach, select, rename
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
        # Actor identification
        actor_path: str = "",
        actor_label: str = "",
        actor_guid: str = "",
        actor_tag: str = "",
        # Add actor parameters
        actor_class: str = "",
        spawn_location: Optional[List[float]] = None,
        spawn_rotation: Optional[List[float]] = None,
        spawn_scale: Optional[List[float]] = None,
        actor_name: str = "",
        tags: Optional[List[str]] = None,
        # Query parameters
        class_filter: str = "",
        label_filter: str = "",
        required_tags: Optional[List[str]] = None,
        excluded_tags: Optional[List[str]] = None,
        selected_only: bool = False,
        max_results: int = 100,
        # Get info options
        include_components: bool = True,
        include_properties: bool = True,
        category_filter: str = "",
        # Remove options
        with_undo: bool = True,
        # Phase 2: Transform parameters
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        world_space: bool = True,
        sweep: bool = False,
        teleport: bool = False,
        # Editor view parameters
        instant: bool = False,
        # Phase 3: Property parameters
        property_path: str = "",
        property_value: str = "",
        component_name: str = "",
        include_inherited: bool = True,
        # Phase 4: Hierarchy & Organization parameters
        folder_path: str = "",
        parent_label: str = "",
        parent_path: str = "",
        parent_guid: str = "",
        parent_tag: str = "",
        socket_name: str = "",
        weld_simulated_bodies: bool = True,
        add_to_selection: bool = False,
        deselect: bool = False,
        deselect_all: bool = False,
        new_label: str = "",
        actors: Optional[List[str]] = None,
    ) -> Dict[str, Any]:
        """
        Level Actor Manager Tool - Phase 1 & 2
        
        Manage actors in the current level with full undo support.
        
        ## Available Actions (Phase 1):
        
        **add** - Add a new actor to the level
        ```python
        manage_level_actors(
            action="add",
            actor_class="PointLight",
            spawn_location=[100, 200, 300],
            actor_name="MainLight",
            tags=["Lighting", "Indoor"]
        )
        ```
        
        **remove** - Remove an actor from the level
        ```python
        manage_level_actors(
            action="remove",
            actor_label="MainLight"
        )
        ```
        
        **list** - List actors with optional filtering
        ```python
        manage_level_actors(
            action="list",
            class_filter="*Light*",
            max_results=50
        )
        ```
        
        **find** - Find actors matching criteria
        ```python
        manage_level_actors(
            action="find",
            required_tags=["Enemy"],
            class_filter="BP_Enemy*"
        )
        ```
        
        **get_info** - Get comprehensive actor information
        ```python
        manage_level_actors(
            action="get_info",
            actor_label="PointLight_1",
            include_components=True,
            include_properties=True
        )
        ```
        
        ## Available Actions (Phase 2 - Transforms):
        
        **set_transform** - Set full transform (location, rotation, scale)
        ```python
        manage_level_actors(
            action="set_transform",
            actor_label="MyActor",
            location=[100, 200, 300],
            rotation=[0, 45, 0],
            scale=[2, 2, 2],
            world_space=True
        )
        ```
        
        **get_transform** - Get complete transform information
        ```python
        manage_level_actors(
            action="get_transform",
            actor_label="MyActor"
        )
        # Returns: world_transform, relative_transform, forward/right/up vectors, bounds
        ```
        
        **set_location** - Set actor location only
        ```python
        manage_level_actors(
            action="set_location",
            actor_label="MyActor",
            location=[500, 0, 100],
            world_space=True,
            sweep=False
        )
        ```
        
        **set_rotation** - Set actor rotation only
        ```python
        manage_level_actors(
            action="set_rotation",
            actor_label="MyActor",
            rotation=[0, 90, 0],  # Pitch, Yaw, Roll
            world_space=True
        )
        ```
        
        **set_scale** - Set actor scale only
        ```python
        manage_level_actors(
            action="set_scale",
            actor_label="MyActor",
            scale=[2, 2, 2]
        )
        ```
        
        ## Editor View Operations:
        
        **focus** - Focus the viewport camera on an actor
        ```python
        manage_level_actors(
            action="focus",
            actor_label="MyActor",
            instant=False  # Smooth camera transition
        )
        ```
        
        **move_to_view** - Move an actor to the center of the current viewport
        ```python
        manage_level_actors(
            action="move_to_view",
            actor_label="MyActor"
        )
        # Moves the actor to where the camera is looking
        ```
        
        **refresh_viewport** - Force refresh all level editing viewports
        ```python
        manage_level_actors(
            action="refresh_viewport"
        )
        # Useful after making changes that don't auto-refresh the viewport
        ```
        
        ## Phase 3: Property Operations
        
        **get_property** - Get a single property value
        ```python
        manage_level_actors(
            action="get_property",
            actor_label="PointLight_1",
            property_path="Intensity"
        )
        # For component properties: property_path="LightComponent0.Intensity"
        # Or use component_name parameter
        ```
        
        **set_property** - Set a single property value
        ```python
        manage_level_actors(
            action="set_property",
            actor_label="PointLight_1",
            property_path="Intensity",
            property_value="5000.0"
        )
        ```
        
        **get_all_properties** - Get all properties with optional filtering
        ```python
        manage_level_actors(
            action="get_all_properties",
            actor_label="PointLight_1",
            component_name="LightComponent0",
            category_filter="Rendering",
            include_inherited=True
        )
        ```
        
        ## Phase 4: Hierarchy & Organization
        
        **set_folder** - Set actor folder in World Outliner
        ```python
        manage_level_actors(
            action="set_folder",
            actor_label="MyLight",
            folder_path="Lighting/Indoor"
        )
        ```
        
        **attach** - Attach an actor to a parent
        ```python
        manage_level_actors(
            action="attach",
            actor_label="ChildActor",
            parent_label="ParentActor",
            socket_name="",  # Optional socket
            weld_simulated_bodies=True
        )
        ```
        
        **detach** - Detach an actor from its parent
        ```python
        manage_level_actors(
            action="detach",
            actor_label="ChildActor"
        )
        ```
        
        **select** - Select/deselect actors
        ```python
        # Select single actor (replaces selection)
        manage_level_actors(action="select", actor_label="MyActor")
        
        # Add to selection
        manage_level_actors(action="select", actor_label="MyActor", add_to_selection=True)
        
        # Deselect specific actor
        manage_level_actors(action="select", actor_label="MyActor", deselect=True)
        
        # Deselect all
        manage_level_actors(action="select", deselect_all=True)
        
        # Select multiple actors
        manage_level_actors(action="select", actors=["Actor1", "Actor2", "Actor3"])
        ```
        
        **rename** - Rename an actor's label
        ```python
        manage_level_actors(
            action="rename",
            actor_label="OldName",
            new_label="NewName"
        )
        ```
        
        ## Actor Identification:
        
        Actors can be identified by:
        - actor_path: Full path (most precise)
        - actor_label: Display name in World Outliner
        - actor_guid: Unique identifier
        - actor_tag: First actor with this tag
        
        ## Query Filtering:
        
        - class_filter: Wildcard match on class name (*Light*, BP_Enemy*)
        - label_filter: Wildcard match on label
        - required_tags: Must have ALL tags
        - excluded_tags: Must have NONE of these tags
        - selected_only: Only selected actors
        
        Args:
            action: Action to perform (add, remove, list, find, get_info, set_transform, get_transform, set_location, set_rotation, set_scale, focus, move_to_view, refresh_viewport)
            actor_path: Full actor path for identification
            actor_label: Actor label for identification
            actor_guid: Actor GUID for identification
            actor_tag: Actor tag for identification
            actor_class: Class to spawn (for add)
            spawn_location: [X, Y, Z] location (for add)
            spawn_rotation: [Pitch, Yaw, Roll] rotation (for add)
            spawn_scale: [X, Y, Z] scale (for add)
            actor_name: Name/label for new actor (for add)
            tags: Tags to add to new actor (for add)
            class_filter: Class name filter with wildcards (for list/find)
            label_filter: Label filter with wildcards (for list/find)
            required_tags: Required tags (for list/find)
            excluded_tags: Excluded tags (for list/find)
            selected_only: Only selected actors (for list/find)
            max_results: Maximum results to return (for list/find)
            include_components: Include component info (for get_info)
            include_properties: Include property values (for get_info)
            category_filter: Filter properties by category (for get_info)
            with_undo: Support undo for removal (for remove)
            location: [X, Y, Z] location for transform operations
            rotation: [Pitch, Yaw, Roll] rotation for transform operations
            scale: [X, Y, Z] scale for transform operations
            world_space: Use world space (True) or relative space (False) for transforms
            sweep: Enable sweep collision check during movement
            teleport: Teleport physics state when moving
            instant: Instant camera transition for focus (default: smooth transition)
            property_path: Property name/path for get_property/set_property
            property_value: Value to set (for set_property)
            component_name: Target component name (for property operations)
            include_inherited: Include inherited properties (for get_all_properties)
            folder_path: Folder path in World Outliner (for set_folder)
            parent_label: Parent actor label (for attach)
            parent_path: Parent actor path (for attach)
            parent_guid: Parent actor GUID (for attach)
            parent_tag: Parent actor tag (for attach)
            socket_name: Socket to attach to (for attach)
            weld_simulated_bodies: Weld physics bodies when attaching
            add_to_selection: Add to current selection instead of replacing (for select)
            deselect: Deselect instead of select (for select)
            deselect_all: Deselect all actors (for select)
            new_label: New label for actor (for rename)
            actors: List of actor labels for multi-select (for select)
            
        Returns:
            Dict with success status and action-specific results
        """
        from vibe_ue_server import get_unreal_connection
        
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "error": "Failed to connect to Unreal Engine"}
        
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
        
        # Build parameters
        params = {
            "action": action_lower,
            # Identification
            "actor_path": actor_path,
            "actor_label": actor_label,
            "actor_guid": actor_guid,
            "actor_tag": actor_tag,
        }
        
        # Add action params
        if action_lower == "add":
            params["actor_class"] = actor_class
            params["actor_name"] = actor_name
            if spawn_location:
                params["spawn_location"] = spawn_location
            if spawn_rotation:
                params["spawn_rotation"] = spawn_rotation
            if spawn_scale:
                params["spawn_scale"] = spawn_scale
            if tags:
                params["tags"] = tags
        
        # Query params
        if action_lower in ["list", "find"]:
            params["class_filter"] = class_filter
            params["label_filter"] = label_filter
            params["selected_only"] = selected_only
            params["max_results"] = max_results
            if required_tags:
                params["required_tags"] = required_tags
            if excluded_tags:
                params["excluded_tags"] = excluded_tags
        
        # Get info params
        if action_lower == "get_info":
            params["include_components"] = include_components
            params["include_properties"] = include_properties
            params["category_filter"] = category_filter
        
        # Remove params
        if action_lower == "remove":
            params["with_undo"] = with_undo
        
        # Phase 2: Transform params
        if action_lower == "set_transform":
            if location:
                params["location"] = location
            if rotation:
                params["rotation"] = rotation
            if scale:
                params["scale"] = scale
            params["world_space"] = world_space
            params["sweep"] = sweep
            params["teleport"] = teleport
        
        if action_lower == "set_location":
            if location:
                params["location"] = location
            else:
                return {"success": False, "error": "location parameter is required for set_location"}
            params["world_space"] = world_space
            params["sweep"] = sweep
        
        if action_lower == "set_rotation":
            if rotation:
                params["rotation"] = rotation
            else:
                return {"success": False, "error": "rotation parameter is required for set_rotation"}
            params["world_space"] = world_space
        
        if action_lower == "set_scale":
            if scale:
                params["scale"] = scale
            else:
                return {"success": False, "error": "scale parameter is required for set_scale"}
        
        # Editor view operations
        if action_lower == "focus":
            params["instant"] = instant
        
        # move_to_view just needs the actor identifier, no extra params
        
        # Phase 3: Property operations
        if action_lower in ["get_property", "set_property", "get_all_properties"]:
            params["property_path"] = property_path
            params["component_name"] = component_name
            params["include_inherited"] = include_inherited
            params["category_filter"] = category_filter
            
            if action_lower == "set_property":
                params["property_value"] = property_value
        
        # Phase 4: Hierarchy & Organization
        if action_lower == "set_folder":
            params["folder_path"] = folder_path
        
        if action_lower == "attach":
            params["parent_path"] = parent_path
            params["parent_label"] = parent_label
            params["parent_guid"] = parent_guid
            params["parent_tag"] = parent_tag
            params["socket_name"] = socket_name
            params["weld_simulated_bodies"] = weld_simulated_bodies
        
        # detach just needs the actor identifier
        
        if action_lower == "select":
            params["add_to_selection"] = add_to_selection
            params["deselect"] = deselect
            params["deselect_all"] = deselect_all
            if actors:
                params["actors"] = actors
        
        if action_lower == "rename":
            params["new_label"] = new_label
        
        try:
            result = unreal.send_command("manage_level_actors", params)
            return result
        except Exception as e:
            logger.error(f"manage_level_actors error: {e}")
            return {"success": False, "error": str(e)}
