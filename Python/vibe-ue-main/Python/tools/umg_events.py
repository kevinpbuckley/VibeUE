"""
UMG Widget Event Binding and C++ Integration Tools

This module provides tools for binding widget events to functions,
including direct C++ integration and advanced event handling.
"""

import logging
from typing import Dict, Any, List, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_umg_event_tools(mcp: FastMCP):
    """Register UMG event binding and C++ integration tools with the MCP server."""

    # Advanced Event Handling
    @mcp.tool()
    def bind_input_events(
        ctx: Context,
        widget_name: str,
        component_name: str,
        input_events: Dict[str, str]
    ) -> Dict[str, Any]:
        """
        Bind multiple input events at once.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            component_name: Name of the component
            input_events: Dictionary mapping event names to function names
            
        Example input_events:
            {
                "OnMouseButtonDown": "HandleMouseDown",
                "OnMouseButtonUp": "HandleMouseUp",
                "OnKeyDown": "HandleKeyPress",
                "OnFocusReceived": "HandleFocusGained"
            }
            
        Returns:
            Dict containing success status and binding results
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine"}
            
            params = {
                "widget_name": widget_name,
                "component_name": component_name,
                "input_mappings": [
                    {"event_name": event_name, "function_name": function_name}
                    for event_name, function_name in input_events.items()
                ]
            }
            
            logger.info(f"Binding input events for '{component_name}': {list(input_events.keys())}")
            response = unreal.send_command("bind_input_events", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine"}
            
            logger.info(f"Bind input events response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error binding input events: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg}


    @mcp.tool()
    def get_available_events(
        ctx: Context,
        widget_name: str,
        component_name: str
    ) -> Dict[str, Any]:
        """
        Get list of available events for a widget component.
        
        Args:
            widget_name: Name of the target Widget Blueprint
            component_name: Name of the component
            
        Returns:
            Dict containing list of available events
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "error": "Failed to connect to Unreal Engine", "events": []}
            
            params = {
                "widget_name": widget_name,
                "component_name": component_name
            }
            
            logger.info(f"Getting available events for '{component_name}'")
            response = unreal.send_command("get_available_events", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "error": "No response from Unreal Engine", "events": []}
            
            logger.info(f"Get available events response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error getting available events: {e}"
            logger.error(error_msg)
            return {"success": False, "error": error_msg, "events": []}



    logger.info("UMG event tools registered successfully")
