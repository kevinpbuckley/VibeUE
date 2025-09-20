"""
UMG Layout Tools - DEPRECATED

All tools replaced by reflection system.
Use add_widget_component() and get_available_widgets() instead.
See CLEANUP.md for migration guide.
"""

import logging
from mcp.server.fastmcp import FastMCP

logger = logging.getLogger("UnrealMCP")

def register_umg_layout_tools(mcp: FastMCP):
    """All tools removed - replaced by reflection system"""
    logger.info("UMG Layout tools registration completed - all tools replaced by reflection system")
    pass
