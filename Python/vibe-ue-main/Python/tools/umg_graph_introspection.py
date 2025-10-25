"""
UMG and Blueprint Graph Introspection Tools

Adds richer APIs to inspect widget/blueprint event graphs, nodes, and connections so assistants can summarize behavior.
"""

import logging
from typing import Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")


def register_umg_graph_introspection_tools(mcp: FastMCP):
    """Register graph introspection tools with the MCP server."""

    # REMOVED: list_event_graph_nodes - Use manage_blueprint_node with action="list" instead
    # This functionality is now provided by the manage_blueprint_node tool with action="list"

    @mcp.tool()
    def get_node_details(
        ctx: Context,
        blueprint_name: str,
        node_id: str
    ) -> Dict[str, Any]:
        """
        Get detailed information for a specific graph node including pins with current default values.
        
        🔍 **PRIMARY USE**: Discover pin names and current default values before setting new values.
        
        Returns:
            Dict containing:
            - success: boolean
            - node: object with node_id, display_title, and pins array
            - pins: array of pin objects with:
              - pin_id: unique pin identifier
              - name: pin name (use this with action="configure")
              - direction: "input" or "output"
              - type: pin type (int, float, bool, string, etc.)
              - default_value: current default value set on the pin
              - connected_to: array of connected pin IDs
        
        Example:
            # Get node details to see available pins
            details = get_node_details(
                blueprint_name="/Game/Blueprints/BP_Player",
                node_id="{NODE_GUID}"
            )
            
            # Use pin names to set values with action="configure"
            for pin in details["node"]["pins"]:
                if pin["direction"] == "input":
                    print(f"Pin: {pin['name']}, Type: {pin['type']}, Current: {pin.get('default_value')}")
        """
        from vibe_ue_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "error": "Failed to connect to Unreal Engine", "node": {}}
            params = {"blueprint_name": blueprint_name, "node_id": node_id}
            logger.info(f"Getting node details for '{node_id}' in '{blueprint_name}'")
            return unreal.send_command("get_node_details", params) or {"success": False, "error": "No response", "node": {}}
        except Exception as e:
            logger.exception("get_node_details failed")
            return {"success": False, "error": str(e), "node": {}}

    @mcp.tool()
    def list_custom_events(
        ctx: Context,
        blueprint_name: str
    ) -> Dict[str, Any]:
        """
        List custom events defined on the Event Graph.
        """
        from vibe_ue_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "error": "Failed to connect to Unreal Engine", "events": []}
            params = {"blueprint_name": blueprint_name}
            logger.info(f"Listing custom events for '{blueprint_name}'")
            return unreal.send_command("list_custom_events", params) or {"success": False, "error": "No response", "events": []}
        except Exception as e:
            logger.exception("list_custom_events failed")
            return {"success": False, "error": str(e), "events": []}

    logger.info("UMG graph introspection tools registered successfully")

    @mcp.tool()
    def summarize_event_graph(
        ctx: Context,
        blueprint_name: str,
        max_nodes: int = 200
    ) -> Dict[str, Any]:
        """
        Produce a readable outline of the Event Graph: events, key function calls, branches, and custom events.
        """
        from vibe_ue_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "error": "Failed to connect to Unreal Engine", "summary": ""}
            
            # Use manage_blueprint_node with action="list" instead of the removed list_event_graph_nodes
            params = {
                "action": "list",
                "blueprint_name": blueprint_name,
                "include_functions": True,
                "include_macros": True,
                "include_timeline": True,
            }
            nodes_resp = unreal.send_command("manage_blueprint_node", params) or {"success": False, "error": "No response", "nodes": []}
            
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
                    detail = get_node_details(ctx, blueprint_name, nid)
                    if detail and detail.get("success", True):
                        d = detail.get("node", {})
                        pins = d.get("pins", [])
                        calls = [p for p in pins if p.get("direction") == "Output"]
                        if calls:
                            for p in calls[:3]:
                                lines.append(f"  -> {p.get('name')} -> {','.join([c.get('to_node_id','') for c in p.get('connections',[])])}")
            return {"success": True, "summary": "\n".join(lines)}
        except Exception as e:
            logger.exception("summarize_event_graph failed")
            return {"success": False, "error": str(e), "summary": ""}
