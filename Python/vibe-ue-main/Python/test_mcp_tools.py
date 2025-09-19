#!/usr/bin/env python3
"""Test MCP tool parameter mapping"""

import sys
import inspect
sys.path.append('.')

from tools.node_tools import register_blueprint_node_tools

# Create a mock MCP instance to test tool registration
class MockMCP:
    def __init__(self):
        self.tools = {}
    
    def tool(self):
        def decorator(func):
            self.tools[func.__name__] = func
            return func
        return decorator

def test_tool_registration():
    """Test that the MCP tools are registered correctly"""
    print("🔧 Testing MCP Tool Registration")
    print("=" * 40)
    
    # Create mock MCP and register tools
    mock_mcp = MockMCP()
    register_blueprint_node_tools(mock_mcp)
    
    # Check what tools were registered
    print(f"Registered tools: {list(mock_mcp.tools.keys())}")
    
    # Check the add_blueprint_node tool specifically
    if 'add_blueprint_node' in mock_mcp.tools:
        func = mock_mcp.tools['add_blueprint_node']
        sig = inspect.signature(func)
        params = list(sig.parameters.keys())
        print(f"✅ add_blueprint_node tool found")
        print(f"Parameters: {params}")
        
        # Check if the fix is in place
        if 'node_type' in params:
            print("✅ node_type parameter exists (MCP interface)")
            print("✅ Parameter mapping fix should be working")
        else:
            print("❌ node_type parameter missing")
            
        return True
    else:
        print("❌ add_blueprint_node tool not found")
        return False

if __name__ == '__main__':
    test_tool_registration()