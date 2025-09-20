"""
UMG Components - Legacy module cleaned up.

All tools have been removed and replaced by the reflection system.
Use tools/umg_reflection.py instead.
"""

from mcp.server.fastmcp import FastMCP
import logging

logger = logging.getLogger(__name__)

def register_umg_component_tools(mcp: FastMCP):
    """Legacy function - no tools registered."""
    logger.info("UMG component tools registered (empty - use reflection system)")
