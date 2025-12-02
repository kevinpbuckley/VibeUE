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
    
    @mcp.tool()
    def check_unreal_connection(ctx: Context) -> Dict[str, Any]:
        """
        Test connection to Unreal Engine and verify plugin status.
        
         **FIRST DIAGNOSTIC TOOL**: Use this when any MCP tools fail to verify basic connectivity.
        
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
                        "1. Verify Unreal Engine 5.7 is running",
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
    def get_help(ctx: Context, topic: str = "overview") -> Dict[str, Any]:
        """
        Get comprehensive help documentation for VibeUE MCP tools organized by topic.
        
         **AI CRITICAL**: ALWAYS use this tool when:
        - You can't find a specific tool or don't know which tool to use
        - You need parameter details for multi-action tools
        - You're unsure about tool workflows or usage patterns
        - You encounter errors and need troubleshooting guidance
        - You need examples of how to use complex tools
        
         **AVAILABLE TOPICS**:
        - **overview**: VibeUE MCP overview and quick reference (default)
        - **blueprint-workflow**: Complete Blueprint development workflow with dependency order
        - **node-tools**: Node discovery, creation, connections, spawner_key patterns
        - **multi-action-tools**: manage_blueprint_function/variables/components/node reference
        - **umg-guide**: UMG widget development, styling, hierarchy, containers
        - **material-management**: Material and Material Instance management (⭐ NEW!)
        - **asset-discovery**: Asset search, import/export, management, performance tips
        - **troubleshooting**: Connection/Blueprint/Node/UMG issues, diagnostics
        - **topics**: Complete topic listing with descriptions
        
         **USAGE EXAMPLES**:
        - get_help() - Returns overview (default)
        - get_help(topic="blueprint-workflow") - Blueprint development guide
        - get_help(topic="node-tools") - Node creation reference
        - get_help(topic="troubleshooting") - Problem solving guide
        - get_help(topic="topics") - See all available topics
        
        Args:
            topic: Topic name to retrieve (default: "overview")
        
        Returns:
            Dict containing topic content and metadata
        """
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

    
