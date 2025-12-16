"""
Unreal Engine MCP Server

Advanced Model Context Protocol server for comprehensive Unreal Engine 5.7 integration.
Provides complete UMG widget system, Blueprint management, actor manipulation, and 
enhanced UI building capabilities with persistent style sets and complex property support.

Features:
- Socket-based TCP communication on localhost:55557
- Hierarchical UI construction with JSON definitions
- Style set system for consistent theming
- Data binding and MVVM pattern support
- Widget animation and event handling
- Blueprint graph introspection and node management
- Actor spawning and property manipulation
- Enhanced Blueprint node property system with pin value support

Architecture:
- FastMCP framework with async context management
- Connection-per-command pattern (Unreal closes after each operation)
- Comprehensive error handling and logging
- Modular tool registration system

Usage:
1. Ensure Unreal Engine 5.7 is running with VibeUE plugin loaded
2. Start this server via stdio transport
3. Use MCP client to interact with registered tools
4. All responses include 'success' field for error checking

For complete tool documentation, use any tool's help action:
- Tool overview: manage_asset(action="help")
- Action details: manage_asset(action="help", help_action="search")
"""

import sys
import os

# Add the tools directory to sys.path so tools can import from each other
# (e.g., 'from help_system import ...' works inside tool modules)
_script_dir = os.path.dirname(os.path.abspath(__file__))
_tools_dir = os.path.join(_script_dir, 'tools')
if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)

# Fix for Windows: MCP protocol expects LF (\n) only, not CRLF (\r\n)
# See: https://github.com/laravel/boost/issues/362
if sys.platform == "win32":
    # Reconfigure stdout to use LF only
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(newline="\n")
    # Also try to set environment variable for Python 3.7+
    os.environ['PYTHONIOENCODING'] = 'utf-8'

import logging
import socket
import json
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional

from mcp.server.fastmcp import FastMCP

# Configure logging with more detailed format
logging.basicConfig(
    level=logging.DEBUG,  # Change to DEBUG level for more details
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        logging.FileHandler('vibe_ue.log'),
        # logging.StreamHandler(sys.stdout) # Remove this handler to unexpected non-whitespace characters in JSON
    ]
)
logger = logging.getLogger("VibeUE")

# Configuration - AI Assistant Guidelines
# IMPORTANT FOR AI: These settings control the connection to Unreal Engine
# If tools fail with "Failed to connect", verify Unreal is running on these settings
UNREAL_HOST = "127.0.0.1"  # Always localhost - Unreal plugin listens locally only
UNREAL_PORT = 55557        # Default VibeUE plugin port - must match plugin settings

# Connection Pattern for AI Understanding:
# - Unreal closes connection after each command (unlike Unity)
# - Each tool call requires a new connection
# - 5-second timeout prevents hanging on closed connections
# - Large buffer sizes handle complex widget hierarchies and Blueprint data

class UnrealConnection:
    """
    Connection manager for Unreal Engine MCP communication.
    
    CRITICAL FOR AI ASSISTANTS:
    - Unreal closes connections after each command (different from Unity)
    - Always reconnect for each operation - this is expected behavior
    - Connection failures usually mean Unreal isn't running or plugin not loaded
    - Use 5-second timeout to prevent hanging on dead connections
    
    Error Patterns for AI Recognition:
    - "Failed to connect" = Unreal Engine not running
    - "Connection closed before receiving data" = Plugin may have crashed
    - "Timeout receiving Unreal response" = Command took too long or stalled
    - JSON decode errors = Partial response or plugin communication issue
    
    Success Indicators:
    - "Connected to Unreal Engine" = Ready for commands
    - "Received complete response" = Command executed successfully
    """
    
    def __init__(self):
        """Initialize the connection."""
        self.socket = None
        self.connected = False
    
    def connect(self) -> bool:
        """Connect to the Unreal Engine instance."""
        try:
            # Close any existing socket
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
            
            logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(5)  # 5 second timeout
            
            # Set socket options for better stability
            self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            
            # Set larger buffer sizes
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
            
            self.socket.connect((UNREAL_HOST, UNREAL_PORT))
            self.connected = True
            logger.info("Connected to Unreal Engine")
            return True
            
        except Exception as e:
            logger.error(f"Failed to connect to Unreal: {e}")
            self.connected = False
            return False
    
    def disconnect(self):
        """Disconnect from the Unreal Engine instance."""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.socket = None
        self.connected = False

    def receive_full_response(self, sock, buffer_size=4096) -> bytes:
        """Receive a complete response from Unreal, handling chunked data."""
        chunks = []
        sock.settimeout(30)  # 30 second max timeout
        try:
            while True:
                chunk = sock.recv(buffer_size)
                if not chunk:
                    if not chunks:
                        raise Exception("Connection closed before receiving data")
                    break
                chunks.append(chunk)
                
                # Process the data received so far
                data = b''.join(chunks)
                try:
                    decoded_data = data.decode('utf-8').strip()
                except UnicodeDecodeError as e:
                    logger.debug(f"Unicode decode error, continuing to receive: {e}")
                    continue
                
                # Try to parse as JSON to check if complete
                try:
                    json.loads(decoded_data)
                    logger.info(f"Received complete response ({len(data)} bytes)")
                    return decoded_data.encode('utf-8')
                except json.JSONDecodeError:
                    continue
                except Exception as e:
                    logger.warning(f"Error processing response chunk: {str(e)}")
                    continue
        except socket.timeout:
            logger.warning("Socket timeout during receive")
            if chunks:
                # If we have some data already, try to use it
                data = b''.join(chunks)
                try:
                    decoded_data = data.decode('utf-8')
                    parsed_json = json.loads(decoded_data)
                    logger.info(f"Using partial response after timeout ({len(data)} bytes)")
                    return data
                except Exception as e:
                    logger.error(f"Could not parse partial response: {e}")
                    logger.error(f"Partial data: {data[:500]}...")  # Log first 500 bytes
            raise Exception("Timeout receiving Unreal response")
        except Exception as e:
            logger.error(f"Error during receive: {str(e)}")
            raise
    
    def send_command(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """
        Send a command to Unreal Engine and get the response.
        
        AI ASSISTANT GUIDANCE:
        - This is the core communication method used by all tools
        - Always creates fresh connection (Unreal requirement)
        - Handles both error formats: {"status": "error"} and {"success": false}
        - Returns standardized error structure for consistent handling
        
        Parameters:
            command (str): Tool command type (e.g., "CreateUMGWidget", "SpawnActor")
            params (Dict): Tool-specific parameters
            
        Returns:
            Dict: Response with 'success' field or error information
            None: If connection completely failed
            
        Common Response Patterns:
            Success: {"success": true, "data": {...}}
            Error: {"status": "error", "error": "message"}
            Connection Failure: None (check logs for details)
        """
        # Always reconnect for each command, since Unreal closes the connection after each command
        # This is different from Unity which keeps connections alive
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
        
        if not self.connect():
            logger.error("Failed to connect to Unreal Engine for command")
            return None
        
        try:
            # Match Unity's command format exactly
            command_obj = {
                "type": command,  # Use "type" instead of "command"
                "params": params or {}  # Use Unity's params or {} pattern
            }
            
            # Send without newline, exactly like Unity
            command_json = json.dumps(command_obj)
            logger.info(f"Sending command: {command_json}")
            
            # DEBUG: Extra logging for node management commands
            if command == "manage_blueprint_node":
                import sys
                print(
                    f"DEBUG: Sending manage_blueprint_node command to Unreal Engine",
                    file=sys.stderr,
                )
                print(f"DEBUG: Command JSON: {command_json}", file=sys.stderr)
                sys.stderr.flush()
            
            self.socket.sendall(command_json.encode('utf-8'))
            
            # Read response using improved handler
            response_data = self.receive_full_response(self.socket)
            
            try:
                response = json.loads(response_data.decode('utf-8'))
            except json.JSONDecodeError as e:
                logger.error(f"Failed to parse JSON response: {e}")
                logger.error(f"Raw response data: {response_data[:1000]}...")  # Log first 1000 bytes
                return {
                    "status": "error",
                    "error": f"Invalid JSON response from Unreal Engine: {str(e)}"
                }
            
            # Log complete response for debugging
            logger.info(f"Complete response from Unreal: {response}")
            
            # DEBUG: Extra logging for node management responses
            if command == "manage_blueprint_node":
                import sys

                print(
                    f"DEBUG: Received response for manage_blueprint_node: {response}",
                    file=sys.stderr,
                )
                print(f"DEBUG: Response type: {type(response)}", file=sys.stderr)
                if isinstance(response, dict) and "result" in response:
                    print(f"DEBUG: Response result: {response['result']}", file=sys.stderr)
                sys.stderr.flush()
            
      # Enhanced error format handling
      # Check for nested result structures first
            result = response.get("result") if isinstance(response, dict) else None
            if isinstance(result, dict):
                # Check if the result has error information
                if result.get("success") is False:
                    error_message = result.get("error") or result.get("message")
                    if not error_message:
                        error_message = f"Command failed - Full result: {result}"
                    logger.error(f"Unreal error (nested result success=false): {error_message}")
                    # Preserve additional error fields (available_commands, help_tip, etc.)
                    error_response = {
                        "success": False,
                        "error": error_message,
                        "components": [],
                        "widget_info": {},
                        "widgets": []
                    }
                    # Copy over any additional fields from result
                    for key in result:
                        if key not in error_response:
                            error_response[key] = result[key]
                    return error_response
                elif result.get("status") == "error":
                    error_message = result.get("error") or result.get("message")
                    if not error_message:
                        error_message = f"Command failed - Full result: {result}"
                    logger.error(f"Unreal error (nested result status=error): {error_message}")
                    # Preserve additional error fields
                    error_response = {
                        "success": False,
                        "error": error_message,
                        "components": [],
                        "widget_info": {},
                        "widgets": []
                    }
                    for key in result:
                        if key not in error_response:
                            error_response[key] = result[key]
                    return error_response
            
            # Check for direct error formats: {"status": "error", ...} and {"success": false, ...}
            if response.get("status") == "error":
                error_message = response.get("error") or response.get("message")
                if not error_message:
                    error_message = f"Command failed - Full response: {response}"
                logger.error(f"Unreal error (status=error): {error_message}")
                # Preserve additional error fields
                error_response = {
                    "success": False,
                    "error": error_message,
                    "components": [],
                    "widget_info": {},
                    "widgets": []
                }
                for key in response:
                    if key not in error_response and key != "status":
                        error_response[key] = response[key]
                return error_response
            elif response.get("success") is False:
                # This format uses {"success": false, "error": "message"} or {"success": false, "message": "message"}
                error_message = response.get("error") or response.get("message")
                if not error_message:
                    error_message = f"Command failed - Full response: {response}"
                logger.error(f"Unreal error (success=false): {error_message}")
                # Preserve additional error fields
                error_response = {
                    "success": False,
                    "error": error_message,
                    "components": [],
                    "widget_info": {},
                    "widgets": []
                }
                for key in response:
                    if key not in error_response:
                        error_response[key] = response[key]
                return error_response
            
            # Handle successful responses - if we get here and no error conditions were met,
            # check if we have a successful result to return
            if result and result.get("success") is True:
                logger.info(f"Unreal command succeeded: {command}")
                return result
            elif response.get("status") == "success":
                logger.info(f"Unreal command succeeded: {command}")
                return response.get("result", response)
            
            # Always close the connection after command is complete
            # since Unreal will close it on its side anyway
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
            
            return response
            
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            # Always reset connection state on any error
            self.connected = False
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            return {
                "status": "error",
                "error": str(e)
            }

# Global connection state
_unreal_connection: UnrealConnection = None

def get_unreal_connection() -> Optional[UnrealConnection]:
    """Get the connection to Unreal Engine."""
    global _unreal_connection
    try:
        if _unreal_connection is None:
            _unreal_connection = UnrealConnection()
            if not _unreal_connection.connect():
                logger.warning("Could not connect to Unreal Engine")
                _unreal_connection = None
        else:
            # Verify connection is still valid with a ping-like test
            try:
                # Check if socket exists before testing
                if _unreal_connection.socket is None:
                    raise Exception("Socket is None")
                # Simple test by sending an empty buffer to check if socket is still connected
                _unreal_connection.socket.sendall(b'\x00')
                logger.debug("Connection verified with ping test")
            except Exception as e:
                logger.debug(f"Existing connection needs refresh: {e}")
                _unreal_connection.disconnect()
                _unreal_connection = None
                # Try to reconnect
                _unreal_connection = UnrealConnection()
                if not _unreal_connection.connect():
                    logger.warning("Could not reconnect to Unreal Engine")
                    _unreal_connection = None
                else:
                    logger.info("Successfully reconnected to Unreal Engine")
        
        return _unreal_connection
    except Exception as e:
        logger.error(f"Error getting Unreal connection: {e}")
        return None

@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    """Handle server startup and shutdown."""
    global _unreal_connection
    logger.info("VibeUE server starting up")
    try:
        _unreal_connection = get_unreal_connection()
        if _unreal_connection:
            logger.info("Connected to Unreal Engine on startup")
        else:
            logger.warning("Could not connect to Unreal Engine on startup")
    except Exception as e:
        logger.error(f"Error connecting to Unreal Engine on startup: {e}")
        _unreal_connection = None
    
    try:
        yield {}
    finally:
        if _unreal_connection:
            _unreal_connection.disconnect()
            _unreal_connection = None
        logger.info("Unreal MCP server shut down")

# Initialize server with comprehensive metadata
mcp = FastMCP(
    name="VibeUE",
    lifespan=server_lifespan
)

# ============================================================================
# TOOL REGISTRATION - AI ASSISTANT REFERENCE
# ============================================================================
# This section registers all available MCP tools in logical groupings.
# AI assistants should understand the tool organization for better selection:

# CORE BLUEPRINT TOOLS (Blueprint lifecycle and management)
from tools.manage_blueprint import register_blueprint_tools  # Blueprint lifecycle (7 actions)
from tools.manage_blueprint_variable import register_blueprint_variable_tools  # Variable management
from tools.manage_blueprint_component import register_blueprint_component_tools  # Component management
from tools.manage_blueprint_node import register_node_tools  # Node operations
from tools.manage_blueprint_function import register_blueprint_function_tools  # Function operations

# ENHANCED INPUT SYSTEM (Input system management - Phases 1-3)
from tools.manage_enhanced_input import register_enhanced_input_tools  # Enhanced Input (40+ actions across all phases)

# LEVEL ACTOR SYSTEM (Level actor management)
from tools.manage_level_actors import register_level_actor_tools  # Level actors (Phase 1: 5 actions)

# MATERIAL SYSTEM (Material management)
from tools.manage_material import register_material_tools  # Material properties and parameters (24 actions)
from tools.manage_material_node import register_material_node_tools  # Material graph nodes (19 actions)

# UMG SYSTEM (Widget management)
from tools.manage_umg_widget import register_umg_tools  # Unified UMG tool (11 actions)

# ASSET SYSTEM (Asset management)
from tools.manage_asset import register_asset_tools  # Asset operations (4 actions)

# SYSTEM DIAGNOSTICS (AI Assistant Support)
from tools.system import register_system_tools  # Connection testing, get_help

# TOOL REGISTRATION ORDER (Important for AI understanding):
# 1. Core tools first (editor, blueprints, project)
# 2. Basic UMG tools (simple widget operations)  
# 3. Enhanced UMG tools (advanced capabilities)
# This order ensures basic tools are available before enhanced features

# ⚠️ CRITICAL AI REQUIREMENT: Each tool now has action='help' for inline documentation
# Call tool(action='help') for tool overview, or tool(action='help', help_action='action_name')
# for specific action details. No separate get_help tool needed anymore.

# Core Blueprint Tools (9 files → 6 tools)
register_blueprint_tools(mcp)  # manage_blueprint: create, compile, get_info, set_property, reparent, etc. (7 actions)
register_blueprint_variable_tools(mcp)  # manage_blueprint_variable
register_blueprint_component_tools(mcp)  # manage_blueprint_component
register_node_tools(mcp)  # manage_blueprint_node (with discover action)
register_blueprint_function_tools(mcp)  # manage_blueprint_function (15+ actions)

# Enhanced Input System (1 file → 1 tool) - Phases 1-3 services
register_enhanced_input_tools(mcp)  # manage_enhanced_input: 40+ actions for complete Enhanced Input lifecycle

# Level Actor System (1 file → 1 tool) - Phases 1-4
register_level_actor_tools(mcp)  # manage_level_actors: 21 actions (add, remove, list, find, get_info, transforms, get_transform, set_location, set_rotation, set_scale, focus, move_to_view, refresh_viewport, properties, hierarchy)

# Material System (2 files → 2 tools)
register_material_tools(mcp)  # manage_material: 24 actions (create, properties, parameters, instances)
register_material_node_tools(mcp)  # manage_material_node: 19 actions (graph expressions, connections, parameters)

# UMG Unified Manager (1 file → 1 tool)
register_umg_tools(mcp)  # manage_umg_widget: all UMG operations (11 actions)

# Asset Management (1 file → 1 tool)
register_asset_tools(mcp)  # manage_asset: import, export, open, convert (4 actions)

# System Tools (1 file → 1 tool)
register_system_tools(mcp)  # check_unreal_connection

# ✅ TOTAL: 11 Python files providing 12 MCP tools (including 1 system tool)
# 🆕 REMOVED: get_help tool - now each tool has action='help' for inline help
# 🆕 ADDED: manage_material_node tool for material graph operations

# ============================================================================
# SERVER STARTUP - AI ASSISTANT OPERATIONAL NOTES
# ============================================================================

if __name__ == "__main__":
    """
    STARTUP CHECKLIST FOR AI ASSISTANTS:
    
    Before Using Tools:
    1. Verify Unreal Engine 5.7 is running
    2. Confirm VibeUE plugin is loaded (check plugin manager)
    3. Ensure port 55557 is available (default plugin setting)
    4. Check vibe_ue.log for connection status
    
    Tool Usage Patterns:
    - Always check response 'success' field before proceeding
    - Use search/inspect tools before making modifications
    - Compile Blueprints after graph changes
    - Handle connection failures gracefully (retry or guide user)
    - **CRITICAL: Use action='help' on any tool when stuck or need guidance**
    
    Getting Help:
    - Tool overview: tool_name(action='help')
    - Specific action: tool_name(action='help', help_action='action_name')
    - Example: manage_asset(action='help', help_action='search')
    
    When You Need Help:
    - Can't find the right tool? → Use action='help' on similar tools
    - Don't know tool parameters? → Use action='help'
    - Getting errors? → Check action='help' for troubleshooting
    - Need examples? → action='help' shows parameter examples
    - Want to see all actions? → Call action='help' without help_action
    
    Common Issues:
    - "Failed to connect" = Start Unreal Engine with plugin
    - Tools return None = Check log for specific error
    - Partial responses = Network timeout, retry operation
    - JSON errors = Plugin communication issue, restart Unreal
    - **Tool confusion = Use action='help' for inline documentation**
    
    Success Indicators:
    - Log shows "Connected to Unreal Engine on startup"
    - Tools return {"success": true} responses
    - Widget operations reflect in Unreal editor
    """
    
    # NOTE: When using stdio transport, stdout is reserved for JSON-RPC messages only.
    # Logging statements here would corrupt the protocol stream, causing "invalid trailing data" errors.
    # All logging goes to vibe_ue.log file instead (see logging.basicConfig above).
    mcp.run(transport='stdio') 
