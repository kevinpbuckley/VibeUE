"""
System Diagnostics Tools

ESSENTIAL CONNECTION AND DOCUMENTATION TOOLS

This module provides essential diagnostic tools for connection testing
and accessing styling documentation.
"""

import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_system_diagnostic_tools(mcp: FastMCP):
    """Register essential system diagnostic tools."""
    
    @mcp.tool()
    def check_unreal_connection(ctx: Context) -> Dict[str, Any]:
        """
        Test connection to Unreal Engine and verify plugin status.
        
        🔧 **FIRST DIAGNOSTIC TOOL**: Use this when any MCP tools fail to verify basic connectivity.
        
        Returns:
            Dict containing:
            - success: boolean indicating if connection test passed
            - connection_status: detailed connection information
            - plugin_status: UnrealMCP plugin status
            - troubleshooting: suggestions if connection fails
        """
        from vibe_ue_server import get_unreal_connection
        
        try:
            logger.info("Testing connection to Unreal Engine...")
            unreal = get_unreal_connection()
            
            if not unreal:
                return {
                    "success": False,
                    "connection_status": "Failed to establish connection",
                    "troubleshooting": [
                        "1. Verify Unreal Engine 5.6 is running",
                        "2. Check that UnrealMCP plugin is loaded and enabled",
                        "3. Ensure port 55557 is available",
                        "4. Verify project has UnrealMCP plugin in Plugins folder"
                    ]
                }
            
            # Test with a simple command
            test_response = unreal.send_command("check_connection", {})
            
            return {
                "success": True,
                "connection_status": "Connected successfully",
                "plugin_status": "UnrealMCP plugin is responding",
                "test_response": test_response,
                "port": "55557",
                "host": "127.0.0.1"
            }
            
        except Exception as e:
            return {
                "success": False,
                "connection_status": f"Connection error: {str(e)}",
                "troubleshooting": [
                    "1. Restart Unreal Engine",
                    "2. Reload the UnrealMCP plugin",
                    "3. Check Windows Firewall settings for port 55557",
                    "4. Verify no other applications are using port 55557"
                ]
            }

    
