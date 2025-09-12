"""
UMG Widget Discovery and Inspection Tools

WIDGET DISCOVERY SYSTEM FOR AI ASSISTANTS

This module provides comprehensive widget discovery and inspection capabilities:
- Widget Blueprint search with flexible filte        # After search_items() finds exact path
        search_result = search_items(search_term="Inventory", asset_type="Widget")
        widget_name = search_result["items"][0]["name"]  # Use exact name
        get_widget_blueprint_info(widget_name)
- Complete widget hierarchy and component inspection  
- Property value retrieval and metadata
- Event availability discovery
- Validation and optimization suggestions

CRITICAL FOR AI WORKFLOW:
1. ALWAYS use search_items() first to find exact widget names
2. Use get_widget_blueprint_info() to understand widget structure before modifications
3. Use list_widget_components() to see all available components for targeting
4. Use validation tools to check widget health after changes

SEARCH PATTERNS:
- search_items() - Find widgets by name pattern or type
- search_items(search_term="Inventory", asset_type="Widget") - Find all widgets with "Inventory" in name
- search_items(asset_type="Widget", path="/Game/UI") - Search only in specific folders

INSPECTION WORKFLOW:
1. search_items() → Find widget path and verify existence
2. get_widget_blueprint_info() → Get complete structure overview
3. list_widget_components() → See all components for modification
4. get_widget_component_properties() → Check current property values
5. validate_widget_hierarchy() → Verify structure integrity

ERROR PREVENTION:
- Widget names are case-sensitive - use exact names from search results
- Always verify widget exists before attempting modifications
- Check component names before setting properties
- Validate hierarchy after complex changes
"""

import logging
from typing import Dict, Any, List, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_umg_discovery_tools(mcp: FastMCP):
    """Register UMG discovery and inspection tools with the MCP server."""
    
    @mcp.tool()
    def search_items(
        ctx: Context,
        search_term: str = "",
        asset_type: str = "",
        path: str = "/Game",
        case_sensitive: bool = False,
        include_engine_content: bool = False,
        max_results: int = 100
    ) -> Dict[str, Any]:
        """
        Search for assets in the project (widgets, textures, blueprints, materials, etc.).
        
        🔍 **UNIVERSAL ASSET SEARCH**: One tool to find all types of assets in your project.
        Use this to find exact asset names before attempting any modifications.
        
        Args:
            search_term: Text to search for in asset names (optional)
                        Examples: "Inventory", "Background", "Material", "Texture"
            asset_type: Filter by asset type (optional)
                       Examples: "Widget", "WidgetBlueprint", "Texture2D", "Material", 
                                "MaterialInstance", "Blueprint", "StaticMesh", "Audio"
            path: Content browser path to search in (default: "/Game")
                        Examples: "/Game/UI", "/Game/Textures", "/Game/Materials"
            case_sensitive: Whether search should be case sensitive (default: False)
            include_engine_content: Whether to include engine content (default: False)
            max_results: Maximum number of results to return (default: 100)
            
        Returns:
            Dict containing:
            - success: boolean indicating if search completed
            - items: array of asset objects with name, path, type, asset_class
            - search_info: metadata about the search performed
            - error: string (only if success=false)
            
        📝 **Usage Examples**:
        ```python
        # Find all widgets with "Inventory" in the name
        search_items(search_term="Inventory", asset_type="Widget")
        
        # Find all textures for backgrounds
        search_items(search_term="background", asset_type="Texture2D")
        
        # Find all materials
        search_items(asset_type="Material")
        
        # Find all assets in UI folder
        search_items(path="/Game/UI")
        
        # Find specific widget
        search_items(search_term="WBP_Inventory", asset_type="Widget")
        
        # Find textures in specific folder
        search_items(search_term="", asset_type="Texture2D", path="/Game/Textures")
        ```
        
        🚨 **AI Tips**:
        - Always use this BEFORE attempting to modify any asset
        - Copy the exact asset name from results for subsequent calls
        - If no widgets found, check path parameter or try broader search_term
        - Use partial names if unsure of exact widget names
        
        🔄 **Common Follow-up Workflow**:
        1. search_items() → Find target asset
        2. get_widget_blueprint_info() → Understand structure (for widgets)
        3. Begin modifications with exact asset name from search results
        - Use empty search_term to see all assets in a path
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine", "items": []}
            
            params = {
                "search_term": search_term,
                "asset_type": asset_type,
                "path": path,
                "case_sensitive": case_sensitive,
                "include_engine_content": include_engine_content,
                "max_results": max_results
            }
            
            logger.info(f"Searching for assets: term='{search_term}', type='{asset_type}', path='{path}'")
            response = unreal.send_command("search_items", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine", "items": []}
            
            # Handle both direct response and nested result formats
            if "result" in response and isinstance(response["result"], dict):
                # Nested format: {"status": "success", "result": {"success": true, "items": [...]}}
                result = response["result"]
                if result.get("success", False):
                    return result
                else:
                    error_msg = result.get("error", "Unknown error from Unreal Engine")
                    logger.error(f"Unreal Engine error (nested): {error_msg}")
                    return {"success": False, "error": error_msg, "items": []}
            elif response.get("success", False):
                # Direct format: {"success": true, "items": [...]}
                return response
            else:
                # Error response
                error_msg = response.get("error", "Unknown error from Unreal Engine")
                logger.error(f"Unreal Engine error: {error_msg}")
                return {"success": False, "error": error_msg, "items": []}
            
        except Exception as e:
            error_msg = f"Error searching assets: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg, "items": []}

    @mcp.tool()
    def get_widget_blueprint_info(ctx: Context, widget_name: str) -> Dict[str, Any]:
        """
        Get comprehensive information about a UMG Widget Blueprint.
        
        🔍 **ESSENTIAL FOR UNDERSTANDING WIDGET STRUCTURE**: Use this after search_items() 
        to understand the widget's current structure before making any modifications.
        
        ⚡ **PERFORMANCE TIP**: ALWAYS use full paths for best performance! Partial names
        trigger expensive Asset Registry searches that can timeout on complex projects.
        
        Args:
            widget_name: Full path or name of the widget to inspect. Can be:
                        - **RECOMMENDED**: Full asset path (e.g., "/Game/Blueprints/UI/WBP_Inventory")
                        - **RECOMMENDED**: Package path (e.g., "/Game/Blueprints/UI/WBP_Inventory.WBP_Inventory")
                        - **SLOW**: Widget name only (e.g., "WBP_Inventory") - triggers expensive search
                        
                        ⚡ **BEST PRACTICE**: Use search_items() first to get exact path!
            
        Returns:
            Dict containing:
            - success: boolean indicating if inspection completed
            - widget_info: object with complete widget metadata
              - name: widget blueprint name
              - path: full asset path
              - parent_class: base class (usually UserWidget)
              - components: array of all widget components with hierarchy
              - properties: widget-level properties and settings
              - events: available events and current bindings
              - animations: any defined widget animations
            - error: string (only if success=false)
            - suggestion: helpful next steps if widget not found
            
        📊 **Widget Info Structure**:
        ```json
        {
            "success": true,
            "widget_info": {
                "name": "WBP_Inventory",
                "path": "/Game/UI/WBP_Inventory",
                "parent_class": "UserWidget",
                "components": [
                    {
                        "name": "CanvasPanel_0",
                        "type": "CanvasPanel",
                        "is_root": true,
                        "children": [...]
                    }
                ],
                "properties": {...},
                "events": [...],
                "animations": [...]
            }
        }
        ```
        
        📝 **Usage Examples**:
        ```python
        # ✅ FAST: Use exact path from search_items()
        search_result = search_items(search_term="Inventory", asset_type="Widget")
        widget_path = search_result["items"][0]["path"]
        get_widget_blueprint_info(widget_path)
        
        # ✅ FAST: Use known full path
        get_widget_blueprint_info("/Game/Blueprints/UI/WBP_Inventory")
        
        # ❌ SLOW: Partial name triggers expensive search
        get_widget_blueprint_info("WBP_Inventory")  # Can timeout on large projects
        ```
        
        🚨 **AI Workflow Tips**:
        1. **ALWAYS use search_items() first** to get exact widget path
        2. **Pass full paths** from search results for instant loading
        3. **Avoid partial names** unless absolutely necessary
        4. Use this info to plan your modifications intelligently
        
        💡 **Performance Optimization**:
        - Full paths: ~50ms (direct asset loading)
        - Partial names: ~5-60+ seconds (Asset Registry scan)
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine", "widget_info": {}}
            
            # Validate widget_name parameter
            if not widget_name or not widget_name.strip():
                error_msg = "Widget name cannot be empty. Please provide a valid widget name."
                logger.error(error_msg)
                return {"success": False, "error": error_msg, "widget_info": {}}
            
            widget_name = widget_name.strip()
            
            # Detect if this is a full path vs partial name and warn accordingly
            is_full_path = widget_name.startswith("/Game") or "." in widget_name
            performance_mode = "FAST (direct path)" if is_full_path else "SLOW (name search)"
            
            logger.info(f"Getting widget blueprint info for: '{widget_name}' - Mode: {performance_mode}")
            if not is_full_path:
                logger.warning(f"⚠️ Using partial name '{widget_name}' may be slow! Consider using search_items() first to get exact path.")
            
            params = {
                "widget_name": widget_name
            }
            
            try:
                if is_full_path:
                    logger.info(f"Using direct path loading for fast access")
                else:
                    logger.info(f"Using Asset Registry search - this may take 30-60 seconds for complex projects")
                
                response = unreal.send_command("get_widget_blueprint_info", params)
            except Exception as e:
                error_msg = str(e)
                if "timeout" in error_msg.lower():
                    logger.error(f"Timeout getting widget info for '{widget_name}': {error_msg}")
                    
                    suggestion = "Try using search_items() first to get the exact full path, then use that path for instant loading."
                    if not is_full_path:
                        suggestion += f" Example: search_items(search_term='{widget_name}', asset_type='Widget') then use the exact path from results."
                    
                    return {
                        "success": False, 
                        "error": f"Timeout error: {error_msg}. The widget may be too complex or Unreal Engine is busy.",
                        "suggestion": suggestion,
                        "widget_info": {}
                    }
                else:
                    logger.error(f"Exception getting widget info for '{widget_name}': {error_msg}")
                    return {
                        "success": False, 
                        "error": f"Communication error: {error_msg}",
                        "widget_info": {}
                    }
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine", "widget_info": {}}
            
            # Log the exact response for debugging
            logger.info(f"Raw response from Unreal Engine: {response}")
            
            # Handle both direct response and nested result formats
            if "result" in response and isinstance(response["result"], dict):
                # Nested format: {"status": "success", "result": {"success": true, "widget_info": {...}}}
                result = response["result"]
                if result.get("success", False):
                    return result
                else:
                    # Get the actual error message, don't default to generic message
                    error_msg = result.get("error") or result.get("message") or f"Command failed - Raw response: {result}"
                    logger.error(f"Unreal Engine error (nested): {error_msg}")
                    logger.error(f"Full nested result: {result}")
                    
                    # If widget not found, suggest using search_items first
                    if "not found" in error_msg.lower():
                        suggestion = f"Widget '{widget_name}' not found. Try using search_items() first to find the exact widget name or path."
                        return {
                            "success": False, 
                            "error": error_msg,
                            "suggestion": suggestion,
                            "widget_info": {}
                        }
                    
                    return {"success": False, "error": error_msg, "widget_info": {}}
            elif response.get("success", False):
                # Direct format: {"success": true, "widget_info": {...}}
                return response
            else:
                # Error response - be more specific about what went wrong
                error_msg = response.get("error") or response.get("message")
                if not error_msg:
                    # If no specific error message, include the full response for debugging
                    error_msg = f"Command failed with response: {response}"
                    
                logger.error(f"Unreal Engine error: {error_msg}")
                logger.error(f"Full error response: {response}")
                
                # Check for specific error types
                if "timeout" in str(response).lower():
                    error_msg = f"Timeout error: {error_msg}. The widget may be too complex or Unreal Engine is busy."
                elif "not found" in error_msg.lower():
                    suggestion = f"Widget '{widget_name}' not found. Try using search_items() first to find the exact widget name or path."
                    return {
                        "success": False, 
                        "error": error_msg,
                        "suggestion": suggestion,
                        "widget_info": {}
                    }
                elif "connection" in error_msg.lower():
                    error_msg = f"Connection error: {error_msg}. Try restarting the Unreal MCP server."
                
                return {"success": False, "error": error_msg, "widget_info": {}}
            
        except Exception as e:
            error_msg = f"Error getting widget blueprint info: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg, "widget_info": {}}

    @mcp.tool()
    def list_widget_components(ctx: Context, widget_name: str) -> Dict[str, Any]:
        """
        List all components in a widget with their properties and hierarchy.
        
        🎯 **PERFECT FOR FINDING COMPONENTS TO MODIFY**: Use this to see all available 
        components and their exact names for targeted styling and property changes.
        
        Args:
            widget_name: Name of the widget to inspect
                        ⚠️ **Must be exact name from search_items() results**
            
        Returns:
            Dict containing:
            - success: boolean indicating if listing completed
            - components: array of component objects with hierarchy and properties
              - name: exact component name (use this for other tools)
              - type: component type (Button, TextBlock, CanvasPanel, etc.)
              - parent: parent component name (if any)
              - children: array of child component names
              - properties: current property values
              - position: current position [X, Y]
              - size: current size [Width, Height]
              - visibility: current visibility state
            - error: string (only if success=false)
            
        📊 **Component Hierarchy Example**:
        ```json
        {
            "success": true,
            "components": [
                {
                    "name": "CanvasPanel_Main",
                    "type": "CanvasPanel",
                    "parent": null,
                    "children": ["Background_Image", "Header_Text", "Content_Panel"],
                    "properties": {...}
                },
                {
                    "name": "Background_Image", 
                    "type": "Image",
                    "parent": "CanvasPanel_Main",
                    "children": [],
                    "properties": {...}
                }
            ]
        }
        ```
        
        📝 **Usage Examples**:
        ```python
        # List all components in inventory widget
        list_widget_components("WBP_Inventory")
        
        # Workflow: Search → Get Info → List Components → Modify
        widgets = search_items(search_term="Inventory", asset_type="Widget")
        widget_name = widgets["widgets"][0]["name"]
        info = get_widget_blueprint_info(widget_name)
        components = list_widget_components(widget_name)
        
        # Now you have all component names for targeted changes
        for component in components["components"]:
            print(f"Component: {component['name']} ({component['type']})")
        ```
        
        🚨 **AI Best Practices**:
        1. Use exact component names from this list in other tools
        2. Check component types to understand what properties are available
        3. Look at hierarchy to understand parent-child relationships
        4. Use this before set_widget_property() to ensure component exists
        5. see all components to style
        
        💡 **Styling Tips**:
        - Look for Image components for background textures
        - Find TextBlock components for neon text effects
        - Identify Button components for interactive glow effects
        - Find Panel components for overall container styling
        - Check for Progress Bar components for energy/health displays
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine", "components": []}
            
            # Validate widget_name parameter
            if not widget_name or not widget_name.strip():
                error_msg = "Widget name cannot be empty. Please provide a valid widget name."
                logger.error(error_msg)
                return {"success": False, "error": error_msg, "components": []}
            
            widget_name = widget_name.strip()
            
            params = {
                "widget_name": widget_name
            }
            
            logger.info(f"Listing components for widget: '{widget_name}'")
            response = unreal.send_command("list_widget_components", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine", "components": []}
            
            # Handle both direct response and nested result formats
            if "result" in response and isinstance(response["result"], dict):
                # Nested format: {"status": "success", "result": {"success": true, "components": [...]}}
                result = response["result"]
                if result.get("success", False):
                    return result
                else:
                    error_msg = result.get("error", "Unknown error from Unreal Engine")
                    logger.error(f"Unreal Engine error (nested): {error_msg}")
                    
                    # If widget not found, suggest using search_items first
                    if "not found" in error_msg.lower():
                        suggestion = f"Widget '{widget_name}' not found. Try using search_items() first to find the exact widget name or path."
                        return {
                            "success": False, 
                            "error": error_msg,
                            "suggestion": suggestion,
                            "components": []
                        }
                    
                    return {"success": False, "error": error_msg, "components": []}
            elif response.get("success", False):
                # Direct format: {"success": true, "components": [...]}
                return response
            else:
                # Error response
                error_msg = response.get("error", "Unknown error from Unreal Engine")
                logger.error(f"Unreal Engine error: {error_msg}")
                
                # If widget not found, suggest using search_items first
                if "not found" in error_msg.lower():
                    suggestion = f"Widget '{widget_name}' not found. Try using search_items() first to find the exact widget name or path."
                    return {
                        "success": False, 
                        "error": error_msg,
                        "suggestion": suggestion,
                        "components": []
                    }
                
                return {"success": False, "error": error_msg, "components": []}
            
        except Exception as e:
            error_msg = f"Error listing widget components: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg, "components": []}

    @mcp.tool()
    def get_widget_component_properties(
        ctx: Context,
        widget_name: str,
        component_name: str
    ) -> Dict[str, Any]:
        """
        Get all properties of a specific widget component.
        
        Args:
            widget_name: Name of the widget
            component_name: Name of the component
            
        Returns:
            Dict containing complete property list with current values and metadata
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine", "properties": {}}
            
            params = {
                "widget_name": widget_name,
                "component_name": component_name
            }
            
            logger.info(f"Getting properties for component '{component_name}' in widget '{widget_name}'")
            response = unreal.send_command("get_widget_component_properties", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine", "properties": {}}
            
            logger.info(f"Get widget component properties response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting widget component properties: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg, "properties": {}}

    @mcp.tool()
    def get_available_widget_types(ctx: Context) -> Dict[str, Any]:
        """
        Get list of all available UMG widget types that can be created.
        
        Returns:
            Dict containing list of widget types with their categories and descriptions
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine", "widget_types": []}
            
            logger.info("Getting available widget types")
            response = unreal.send_command("get_available_widget_types", {})
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine", "widget_types": []}
            
            logger.info(f"Get available widget types response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting available widget types: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg, "widget_types": []}

    @mcp.tool()
    def validate_widget_hierarchy(ctx: Context, widget_name: str) -> Dict[str, Any]:
        """
        Validate the hierarchy and structure of a widget for issues.
        
        Args:
            widget_name: Name of the widget to validate
            
        Returns:
            Dict containing validation results with any issues found
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine", "validation_results": {}}
            
            params = {
                "widget_name": widget_name
            }
            
            logger.info(f"Validating widget hierarchy: {widget_name}")
            response = unreal.send_command("validate_widget_hierarchy", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine", "validation_results": {}}
            
            logger.info(f"Validate widget hierarchy response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error validating widget hierarchy: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg, "validation_results": {}}

    logger.info("UMG discovery tools registered successfully")
