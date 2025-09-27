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

    @mcp.tool()
    def get_help(ctx: Context) -> Dict[str, Any]:
        """
        Get comprehensive help documentation for all VibeUE MCP tools.
        
        🚀 **AI CRITICAL**: ALWAYS use this tool when:
        - You can't find a specific tool or don't know which tool to use
        - You need parameter details for multi-action tools (manage_blueprint_function, manage_blueprint_node)
        - You're unsure about tool workflows or usage patterns
        - You encounter errors and need troubleshooting guidance
        - You need examples of how to use complex tools
        
        📚 **COMPREHENSIVE COVERAGE**: 
        - All 50+ VibeUE MCP tools with detailed parameters
        - Multi-action tools with all available actions documented
        - Common usage patterns and workflows
        - Error handling and troubleshooting guides
        - Performance tips and best practices
        
        🎯 **WHEN TO USE**: Before attempting any complex operation, when stuck, or when error messages are unclear.
        
        Returns:
            Dict containing the complete help documentation from help.md
        """
        import os
        
        try:
            # Get the path to the help.md file
            current_dir = os.path.dirname(os.path.abspath(__file__))
            # Search upwards for resources/help.md file
            help_path = None
            search_dir = current_dir
            for _ in range(8):  # search up to 8 levels
                candidate = os.path.join(search_dir, '..', 'resources', 'help.md')
                candidate = os.path.abspath(candidate)
                if os.path.isfile(candidate):
                    help_path = candidate
                    break
                # move one level up
                parent = os.path.abspath(os.path.join(search_dir, '..'))
                if parent == search_dir:
                    break
                search_dir = parent

            if help_path is None:
                # Fallback: attempt the expected project-relative location
                project_root = os.path.abspath(os.path.join(current_dir, '..', '..', '..', '..'))
                help_path = os.path.abspath(os.path.join(project_root, 'Plugins', 'VibeUE', 'Python', 'vibe-ue-main', 'Python', 'resources', 'help.md'))
            
            # Read the markdown file
            with open(help_path, 'r', encoding='utf-8') as f:
                help_content = f.read()
            
            return {
                "success": True,
                "help_content": help_content,
                "source_file": "help.md",
                "file_path": help_path,
                "content_type": "markdown",
                "description": "Complete VibeUE MCP tools reference guide with multi-action tool documentation",
                "sections": [
                    "Quick Start Guide",
                    "Tool Categories (System, Asset, Blueprint, UMG, etc.)",
                    "Multi-Action Tools Reference (manage_blueprint_function, manage_blueprint_node)",
                    "Common Usage Patterns",
                    "Important Guidelines",
                    "Troubleshooting Guide",
                    "Pro Tips"
                ],
                "key_features": [
                    "Comprehensive tool documentation",
                    "Multi-action tool parameter reference",
                    "Usage patterns and workflows", 
                    "Error handling guidelines",
                    "Performance optimization tips"
                ]
            }
            
        except FileNotFoundError:
            return {
                "success": False,
                "error": f"help.md not found at expected location: {help_path}",
                "troubleshooting": "Ensure the help.md file exists in the resources directory"
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Error reading help documentation: {str(e)}",
                "troubleshooting": "Check file permissions and encoding"
            }

    
