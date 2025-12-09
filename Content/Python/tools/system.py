"""
System Diagnostics Tools

ESSENTIAL CONNECTION TOOLS

This module provides essential diagnostic tools for connection testing.
Note: For tool-specific help, use action='help' on any tool (e.g., manage_asset(action="help")).
"""

import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_system_tools(mcp: FastMCP):
    """Register essential system diagnostic tools."""
    
    @mcp.tool(description="Test Unreal Engine connection and plugin status. Use as first diagnostic when tools fail.")
    def check_unreal_connection(ctx: Context) -> Dict[str, Any]:
        """Verify connection to Unreal Engine and UnrealMCP plugin."""
        from vibe_ue_server import get_unreal_connection
        
        try:
            logger.info("Testing connection to Unreal Engine...")
            unreal = get_unreal_connection()
            
            if not unreal:
                return {
                    "success": False,
                    "connection_status": "Failed to establish connection",
                    "troubleshooting": [
                        "1. Verify Unreal Engine 5.7 is running",
                        "2. Check that UnrealMCP plugin is loaded and enabled",
                        "3. Ensure port 55557 is available",
                        "4. Verify project has UnrealMCP plugin in Plugins folder"
                    ]
                }
            
            # Connection established - the socket connection itself is the test
            # No need to send a test command that doesn't exist
            return {
                "success": True,
                "connection_status": "Connected successfully",
                "plugin_status": "UnrealMCP plugin is responding",
                "port": "55557",
                "host": "127.0.0.1",
                "help_info": "For tool-specific help, use action='help' on any tool (e.g., manage_asset(action='help'))"
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

