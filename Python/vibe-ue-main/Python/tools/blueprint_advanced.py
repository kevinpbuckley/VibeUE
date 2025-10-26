"""
Blueprint Advanced Tools - Variables and Components multi-action managers.

These were originally in blueprint_tools.py but separated to allow
independent registration after blueprint_manager.py consolidation.
"""

import logging
from mcp.server.fastmcp import FastMCP

logger = logging.getLogger("UnrealMCP")


def register_blueprint_advanced_tools(mcp: FastMCP):
    """Register advanced Blueprint management tools (variables and components)."""
    
    # Import the existing tools from blueprint_tools
    from tools.blueprint_tools import register_blueprint_tools
    
    # Create a temporary MCP instance to extract just the tools we want
    temp_mcp = FastMCP("temp")
    register_blueprint_tools(temp_mcp)
    
    # Copy only the multi-action tools we want to keep
    for tool_name in ["manage_blueprint_variables", "manage_blueprint_components"]:
        if tool_name in temp_mcp._tool_manager._tools:
            mcp._tool_manager._tools[tool_name] = temp_mcp._tool_manager._tools[tool_name]
    
    logger.info("Blueprint advanced tools registered (manage_blueprint_variables, manage_blueprint_components)")
