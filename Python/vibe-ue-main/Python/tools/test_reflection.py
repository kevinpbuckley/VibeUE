"""
Test reflection tool to isolate import issues.
"""

import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_test_reflection_tools(mcp: FastMCP):
    """Register a simple test reflection tool."""
    
    @mcp.tool()
    def test_basic_tool(ctx: Context) -> Dict[str, Any]:
        """Test basic MCP tool functionality without any imports."""
        return {"success": True, "message": "Basic tool works"}
    
    @mcp.tool()
    def test_reflection_import(ctx: Context) -> Dict[str, Any]:
        """Test if imports work properly."""
        try:
            from vibe_ue_server import get_unreal_connection
            unreal = get_unreal_connection()
            if not unreal:
                return {"success": False, "error": "Cannot connect to Unreal"}
            
            return {"success": True, "message": "Import and connection work"}
            
        except Exception as e:
            import traceback
            return {"success": False, "error": f"Import error: {str(e)}", "traceback": traceback.format_exc()}