"""
Minimal test server to isolate stdout issues
"""
import logging

# Configure logging - FILE ONLY
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('test_server.log'),
    ]
)

from mcp.server.fastmcp import FastMCP

# Create minimal server
mcp = FastMCP(name="TestServer")

@mcp.tool()
def test_tool() -> dict:
    """Test tool"""
    return {"success": True}

if __name__ == "__main__":
    mcp.run(transport='stdio')
