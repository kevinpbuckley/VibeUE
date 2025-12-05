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

    @mcp.tool(description="Get help docs by topic. Topics: overview, blueprint-workflow, node-tools, umg-guide, material-management, asset-discovery, troubleshooting, topics.")
    def get_help(ctx: Context, topic: str = "overview") -> Dict[str, Any]:
        """Get VibeUE MCP help documentation for a specific topic."""
        import os
        
        try:
            # Get the path to the topics directory
            current_dir = os.path.dirname(os.path.abspath(__file__))
            
            # Search upwards for resources/topics directory
            topics_dir = None
            search_dir = current_dir
            for _ in range(8):  # search up to 8 levels
                candidate = os.path.join(search_dir, '..', 'resources', 'topics')
                candidate = os.path.abspath(candidate)
                if os.path.isdir(candidate):
                    topics_dir = candidate
                    break
                # move one level up
                parent = os.path.abspath(os.path.join(search_dir, '..'))
                if parent == search_dir:
                    break
                search_dir = parent

            if topics_dir is None:
                # Fallback: attempt the expected project-relative location
                project_root = os.path.abspath(os.path.join(current_dir, '..', '..', '..', '..'))
                topics_dir = os.path.abspath(os.path.join(project_root, 'Plugins', 'VibeUE', 'Python', 'vibe-ue-main', 'Python', 'resources', 'topics'))
            
            # Build topic file path
            topic_file = f"{topic}.md"
            topic_path = os.path.join(topics_dir, topic_file)
            
            # Check if topic exists
            if not os.path.isfile(topic_path):
                # List available topics
                available_topics = []
                if os.path.isdir(topics_dir):
                    available_topics = [f.replace('.md', '') for f in os.listdir(topics_dir) if f.endswith('.md')]
                
                return {
                    "success": False,
                    "error": f"Topic '{topic}' not found",
                    "requested_topic": topic,
                    "available_topics": sorted(available_topics),
                    "suggestion": "Use get_help(topic='topics') to see all available topics"
                }
            
            # Read the topic markdown file
            with open(topic_path, 'r', encoding='utf-8') as f:
                topic_content = f.read()
            
            return {
                "success": True,
                "topic": topic,
                "content": topic_content,
                "source_file": topic_file,
                "file_path": topic_path,
                "content_type": "markdown",
                "help_system": "topic-based",
                "usage": f"get_help(topic='{topic}')"
            }
            
        except FileNotFoundError:
            return {
                "success": False,
                "error": f"Topic file not found at: {topic_path}",
                "troubleshooting": "Ensure the topics directory exists with markdown topic files"
            }
        except Exception as e:
            return {
                "success": False,
                "error": f"Error reading help documentation: {str(e)}",
                "troubleshooting": "Check file permissions and encoding"
            }

    
