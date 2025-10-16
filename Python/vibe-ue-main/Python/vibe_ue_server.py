"""
Unreal Engine MCP Server

Advanced Model Context Protocol server for comprehensive Unreal Engine 5.6 integration.
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
1. Ensure Unreal Engine 5.6 is running with VibeUE plugin loaded
2. Start this server via stdio transport
3. Use MCP client to interact with registered tools
4. All responses include 'success' field for error checking

For complete tool documentation, use the info() prompt.
"""

import logging
import socket
import sys
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
        sock.settimeout(15)  # 15 second timeout for MCP calls
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
                    decoded_data = data.decode('utf-8')
                except UnicodeDecodeError as e:
                    logger.debug(f"Unicode decode error, continuing to receive: {e}")
                    continue
                
                # Try to parse as JSON to check if complete
                try:
                    parsed_json = json.loads(decoded_data)
                    logger.info(f"Received complete response ({len(data)} bytes)")
                    return data
                except json.JSONDecodeError:
                    # Not complete JSON yet, continue reading
                    logger.debug(f"Received partial response ({len(data)} bytes), waiting for more data...")
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
                    return {
                        "success": False,
                        "error": error_message,
                        "components": [],
                        "widget_info": {},
                        "widgets": []
                    }
                elif result.get("status") == "error":
                    error_message = result.get("error") or result.get("message")
                    if not error_message:
                        error_message = f"Command failed - Full result: {result}"
                    logger.error(f"Unreal error (nested result status=error): {error_message}")
                    return {
                        "success": False,
                        "error": error_message,
                        "components": [],
                        "widget_info": {},
                        "widgets": []
                    }
            
            # Check for direct error formats: {"status": "error", ...} and {"success": false, ...}
            if response.get("status") == "error":
                error_message = response.get("error") or response.get("message")
                if not error_message:
                    error_message = f"Command failed - Full response: {response}"
                logger.error(f"Unreal error (status=error): {error_message}")
                return {
                    "success": False,
                    "error": error_message,
                    "components": [],
                    "widget_info": {},
                    "widgets": []
                }
            elif response.get("success") is False:
                # This format uses {"success": false, "error": "message"} or {"success": false, "message": "message"}
                error_message = response.get("error") or response.get("message")
                if not error_message:
                    error_message = f"Command failed - Full response: {response}"
                logger.error(f"Unreal error (success=false): {error_message}")
                return {
                    "success": False,
                    "error": error_message,
                    "components": [],
                    "widget_info": {},
                    "widgets": []
                }
            
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
                # Simple test by sending an empty buffer to check if socket is still connected
                _unreal_connection.socket.sendall(b'\x00')
                logger.debug("Connection verified with ping test")
            except Exception as e:
                logger.warning(f"Existing connection failed: {e}")
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

# CORE UNREAL TOOLS (Basic engine interaction)
from tools.editor_tools import register_editor_tools          # Viewport, screenshots, basic editor
from tools.blueprint_tools import register_blueprint_tools    # Blueprint creation, compilation, components
from tools.node_tools import register_blueprint_node_tools    # Blueprint graph nodes and connections

# BASIC UMG TOOLS (UMG guide + deprecated tools)
from tools.umg_tools import register_umg_tools               # UMG guide and deprecated legacy tools

# ENHANCED UMG SYSTEM (Advanced widget capabilities)
from tools.umg_discovery import register_umg_discovery_tools         # Widget search and inspection
from tools.umg_components import register_umg_component_tools        # DEPRECATED - All replaced by reflection system
from tools.umg_layout import register_umg_layout_tools              # DEPRECATED - All replaced by reflection system
from tools.umg_styling import register_umg_styling_tools            # Style sets and theming system
from tools.umg_events import register_umg_event_tools               # Event binding and interaction
from tools.umg_data_binding import register_umg_data_binding_tools   # MVVM and data sources
from tools.umg_graph_introspection import register_umg_graph_introspection_tools  # Blueprint graph analysis

# UMG REFLECTION SYSTEM (Generic widget creation using Unreal's reflection)
from tools.umg_reflection import register_umg_reflection_tools       # Reflection-based widget discovery and creation

# ENHANCED ASSET SYSTEM (Advanced asset management capabilities)
from tools.asset_discovery import register_asset_discovery_tools     # Smart asset search, import, export, application

# SYSTEM DIAGNOSTICS (AI Assistant Support)
from tools.system_diagnostics import register_system_diagnostic_tools  # Connection testing, validation, AI guidance

# CLIENT INTEGRATION SUPPORT
try:
    from mcp_client_guide import format_integration_guide_for_prompt
except ImportError:
    def format_integration_guide_for_prompt():
        return "Client integration guide not available - check mcp_client_guide.py file"

# TOOL REGISTRATION ORDER (Important for AI understanding):
# 1. Core tools first (editor, blueprints, project)
# 2. Basic UMG tools (simple widget operations)  
# 3. Enhanced UMG tools (advanced capabilities)
# This order ensures basic tools are available before enhanced features

# ⚠️ CRITICAL AI REQUIREMENT: UMG tools require get_help(topic="umg-guide") to be called FIRST
# before styling, adding components, or implementing backgrounds. The guide contains
# essential container-specific patterns and widget hierarchy requirements.

# Core Unreal Engine Tools
register_editor_tools(mcp)         # Actor management, viewport control, screenshots
register_blueprint_tools(mcp)      # Blueprint creation, compilation, component management
register_blueprint_node_tools(mcp) # Event graph manipulation, node connections

# Basic UMG Widget Tools (includes get_help(topic="umg-guide") for guidance)
register_umg_tools(mcp)            # UMG guide and deprecated legacy tools (now references reflection system)

# Enhanced UMG Widget System (Advanced Features) - REQUIRES UMG GUIDE FIRST
register_umg_discovery_tools(mcp)     # Widget search, inspection, validation
register_umg_component_tools(mcp)     # DEPRECATED - All tools replaced by reflection system
register_umg_layout_tools(mcp)        # DEPRECATED - All tools replaced by reflection system
register_umg_styling_tools(mcp)       # Style sets, theming, property management - REQUIRES GUIDE
register_umg_event_tools(mcp)         # Event binding, delegates, interactivity
register_umg_data_binding_tools(mcp)  # Data binding, MVVM patterns
register_umg_graph_introspection_tools(mcp)  # Blueprint analysis and introspection

# UMG Reflection System (Generic Widget Creation) - FUTURE-PROOF APPROACH
register_umg_reflection_tools(mcp)    # Reflection-based widget discovery and creation using Widget Palette patterns

# Enhanced Asset Management System (Advanced Capabilities)
register_asset_discovery_tools(mcp)   # Smart asset search, import, export, AI analysis

# System Diagnostics and AI Support Tools
register_system_diagnostic_tools(mcp)  # Connection testing, validation, troubleshooting guides, get_help() tool  

# Image Conversion Tools
@mcp.tool()
def convert_svg_to_png(
    svg_path: str,
    output_path: str = None,
    size: list = None,
    scale: float = 1.0,
    background: str = None
) -> dict:
    """Convert an SVG to PNG using direct CairoSVG integration.

        Args:
            svg_path: Source SVG path
            output_path: Destination PNG path (default side-by-side)
            size: Optional [W,H] override pixels (forces output size)
            scale: Scalar multiplier (ignored if size provided)
            background: Optional background color (e.g. '#00000000' / '#1e293b')

        Returns: success, png_path, method, conversion details.
        """
    import io
    from pathlib import Path
    
    try:
        # Import dependencies with better error handling
        try:
            import cairosvg
            from PIL import Image, ImageEnhance
            import numpy as np
        except ImportError as e:
            return {
                "success": False,
                "error": f"Missing required package: {e}. Install with: pip install cairosvg pillow numpy",
                "png_path": None
            }
        
        # Validate input
        svg_file = Path(svg_path)
        if not svg_file.exists():
            return {
                "success": False,
                "error": f"SVG file not found: {svg_path}",
                "png_path": None
            }
        
        # Determine output path
        if output_path:
            png_file = Path(output_path)
        else:
            png_file = svg_file.with_suffix('.png')
        
        # Determine dimensions
        width = height = None
        if size and len(size) >= 2:
            width, height = int(size[0]), int(size[1])
        elif scale != 1.0:
            # Default base size with scaling
            width = height = int(1024 * scale)
        
        # Convert SVG to PNG bytes using CairoSVG
        logger.info(f"Converting SVG {svg_path} to PNG with dimensions {width}x{height}")
        
        png_bytes = cairosvg.svg2png(
            url=str(svg_file),
            output_width=width,
            output_height=height,
            background_color=background if background else None
        )
        
        # Load into PIL for post-processing
        img = Image.open(io.BytesIO(png_bytes))
        
        # Ensure RGBA format
        if img.mode != 'RGBA':
            img = img.convert('RGBA')
        
        # UMG optimizations
        arr = np.array(img, dtype=np.uint8)
        
        # Clean up fully transparent pixels (set RGB to 0 for better compression)
        transparent_mask = arr[..., 3] == 0
        arr[transparent_mask, 0:3] = 0
        
        # Optional: Premultiply alpha for UMG (better blending)
        if True:  # Always premultiply for UMG
            rgb = arr[..., 0:3].astype(np.float32) / 255.0
            alpha = arr[..., 3:4].astype(np.float32) / 255.0
            
            # Simple premultiply (no gamma for now to keep it fast)
            rgb_premult = rgb * alpha
            
            arr[..., 0:3] = (rgb_premult * 255.0 + 0.5).astype(np.uint8)
        
        # Convert back to PIL and save
        final_img = Image.fromarray(arr, 'RGBA')
        
        # Ensure output directory exists
        png_file.parent.mkdir(parents=True, exist_ok=True)
        
        # Save with optimization
        final_img.save(png_file, format='PNG', optimize=True, compress_level=6)
        
        # Get final file info
        file_size = png_file.stat().st_size
        final_width, final_height = final_img.size
        
        logger.info(f"Successfully converted {svg_path} to {png_file} ({file_size} bytes)")
        
        return {
            "success": True,
            "png_path": str(png_file),
            "width": final_width,
            "height": final_height,
            "file_size": file_size,
            "method": "cairosvg_direct",
            "optimizations": ["premultiplied_alpha", "transparent_cleanup", "png_optimized"],
            "source_svg": str(svg_file)
        }
        
    except Exception as e:
        logger.error(f"SVG conversion error: {e}")
        return {
            "success": False,
            "error": str(e),
            "png_path": None
        }

register_umg_graph_introspection_tools(mcp)

@mcp.prompt()
def server_capabilities():
    """Detailed server capabilities and feature matrix for client understanding."""
    return """
    # VibeUE Server Capabilities Matrix
    
    ## Server Information
    - **Name:** VibeUE
    - **Version:** 2.0.0 (Enhanced)
    - **Protocol:** MCP (Model Context Protocol)
    - **Transport:** stdio
    - **Target Engine:** Unreal Engine 5.6
    - **Plugin Required:** VibeUE (must be loaded)
    
    ## Core Capabilities
    
    ### 🔌 Connection Management
    - TCP Socket communication (localhost:55557)
    - Per-command connection pattern (Unreal requirement)
    - Automatic reconnection handling
    - Connection health diagnostics
    - Error recovery and retry logic
    
    ### 🎮 Engine Integration
    - **Actor Management:** Create, delete, transform, property manipulation
    - **Blueprint System:** Complete lifecycle management, compilation, component addition
    - **Graph Manipulation:** Node creation, connections, event handling
    - **Project Tools:** Input mappings, settings configuration
    
    ### 🎨 Advanced UMG System
    - **Widget Discovery:** Search, inspection, validation
    - **Component Library:** 15+ widget types with full property support
    - **Layout System:** Canvas, Box, Grid, Scroll containers with hierarchy
    - **Styling Engine:** Style sets, theming, complex property types
    - **Event System:** Binding, delegates, C++ integration
    - **Data Binding:** MVVM patterns, external data sources
    - **Animation System:** Keyframes, sequences, triggers
    
    ### 📊 System Diagnostics
    - Connection testing and validation
    - Tool functionality verification
    - Error pattern recognition
    - Performance monitoring
    - AI guidance and troubleshooting
    
    ## Feature Matrix
    
    | Category | Features | Tool Count | Complexity |
    |----------|----------|------------|------------|
    | Core Engine | Actor, Blueprint, Project | 15+ | Intermediate |
    | UMG Basic | Widget creation, events | 8+ | Basic |
    | UMG Enhanced | Advanced UI, styling, data | 40+ | Advanced |
    | Diagnostics | Testing, validation | 4+ | Utility |
    
    ## Property Type Support
    
    ### Simple Types
    - String, Integer, Float, Boolean
    - Arrays and basic collections
    - Enums and named values
    
    ### Complex Types  
    - **Color:** RGBA objects and arrays
    - **Transform:** Position, rotation, scale vectors
    - **Font:** Size, family, style objects
    - **Padding/Margin:** Box model objects
    - **Anchors:** Layout positioning objects
    
    ### UE5-Specific Types
    - **Asset References:** Static meshes, textures, blueprints
    - **Component References:** Blueprint component targeting
    - **Event Delegates:** Function binding objects
    - **Style Brushes:** Advanced appearance objects
    
    ## Client Integration Notes
    
    ### For AI Assistants
    - Use `info()` prompt for complete tool documentation
    - Use `check_unreal_connection()` for diagnostics
    - Always verify responses with 'success' field
    
    ### For MCP Clients
    - Server supports stdio transport only
    - All tools return JSON with consistent structure
    - Error handling includes troubleshooting guidance
    - Comprehensive logging available in vibe_ue.log
    
    ### Prerequisites
    1. Unreal Engine 5.6 running
    2. VibeUE plugin loaded and enabled
    3. Port 55557 available (configurable in plugin)
    4. Project with plugin in Plugins folder
    
    ## Error Patterns & Recovery
    
    ### Connection Errors
    - "Failed to connect" → Start Unreal Engine
    - "Plugin not responding" → Reload VibeUE plugin
    - "Port already in use" → Check port conflicts
    
    ### Tool Errors
    - "Widget not found" → Use search_items() first
    - "Blueprint compilation failed" → Check node connections
    - "Property not found" → Use list_widget_properties()
    
    This capability matrix ensures clients understand the full scope and proper usage of the VibeUE server.
    """

@mcp.prompt()
def server_status():
    """Current server status and operational information for monitoring."""
    import time
    from datetime import datetime
    
    try:
        # Get connection status
        unreal = get_unreal_connection()
        connection_status = "Connected" if unreal else "Disconnected"
        
        # Test basic communication if connected
        communication_status = "Unknown"
        if unreal:
            try:
                response = unreal.send_command("Ping", {})
                communication_status = "Responsive" if response and response.get("success") else "Not Responding"
            except:
                communication_status = "Error"
        
        return f"""
    # VibeUE Server Status Report
    
    ## Current Status
    - **Server Time:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
    - **Connection Status:** {connection_status}
    - **Communication Status:** {communication_status}
    - **Transport:** stdio
    - **Target:** Unreal Engine 5.6 on localhost:55557
    
    ## Server Health
    - **Configuration:** ✅ Properly configured
    - **Tool Registration:** ✅ All modules loaded
    - **Error Handling:** ✅ Comprehensive coverage
    - **Logging:** ✅ Active (vibe_ue.log)
    
    ## Available Tool Categories
    - **Core Engine Tools:** ✅ Registered (editor, blueprint, project)
    - **Basic UMG Tools:** ✅ Registered (legacy widget system)
    - **Enhanced UMG Tools:** ✅ Registered (advanced widget system)
    - **Diagnostic Tools:** ✅ Registered (system validation)
    
    ## Connection Diagnostics
    - **Host:** 127.0.0.1 (localhost)
    - **Port:** 55557 (VibeUE plugin default)
    - **Protocol:** TCP Socket
    - **Connection Pattern:** Per-command (Unreal requirement)
    
    ## Recommendations
    {"- ✅ System operational - ready for tool usage" if connection_status == "Connected" and communication_status == "Responsive" 
     else "- ⚠️ Start Unreal Engine 5.6 with VibeUE plugin loaded" if connection_status == "Disconnected"
     else "- ⚠️ Check VibeUE plugin status - communication issues detected"}
    
    Use `check_unreal_connection()` tool for detailed diagnostics.
    """
    except Exception as e:
        return f"""
    # VibeUE Server Status Report
    
    ## Error Getting Status
    - **Error:** {str(e)}
    - **Time:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
    
    ## Basic Server Info
    - **Server:** VibeUE v2.0.0
    - **Transport:** stdio
    - **Target:** Unreal Engine 5.6
    
    Use diagnostic tools for detailed troubleshooting.
    """

@mcp.prompt()
def client_integration_guide():
    """Complete MCP client integration guide with all necessary information."""
    return format_integration_guide_for_prompt()

@mcp.prompt()
def tool_schemas():
    """Tool parameter schemas and return value specifications for client developers."""
    return """
    # VibeUE Tool Schemas and Specifications
    
    ## Standard Response Format
    All tools return JSON responses with this structure:
    
    ```json
    {
        "success": boolean,
        "data": object | array | string,
        "error": string (only if success=false)
    }
    ```
    
    ## Parameter Type Specifications
    
    ### Basic Types
    - `string`: Text values, case-sensitive for names
    - `integer`: Whole numbers
    - `float`: Decimal numbers  
    - `boolean`: true/false values
    - `array`: Lists of values [item1, item2, ...]
    
    ### Unreal-Specific Types
    - `position`: [X, Y] pixel coordinates from top-left
    - `size`: [Width, Height] in pixels
    - `color`: [R, G, B, A] with 0.0-1.0 values
    - `rotation`: [Pitch, Yaw, Roll] in degrees
    - `transform`: Position, rotation, scale vectors
    
    ### Complex Object Types
    ```json
    {
        "Color": {"R": 0.5, "G": 0.8, "B": 1.0, "A": 1.0},
        "Font": {"Size": 16, "TypefaceFontName": "Bold"},
        "Padding": {"Left": 10, "Top": 5, "Right": 10, "Bottom": 5},
        "Anchors": {"Min": [0.0, 0.0], "Max": [1.0, 1.0]}
    }
    ```
    
    ## Tool Categories and Schemas
    
    ### Discovery Tools
    **search_items()**
    - Parameters: `search_term` (string), `asset_type` (string), `path` (string), `case_sensitive` (boolean)
    - Returns: Array of asset objects with name, path, type
    - Usage: Always use before modifying widgets or assets
    
    **get_widget_blueprint_info()**
    - Parameters: `widget_name` (string)
    - Returns: Complete widget structure with hierarchy, components, events
    - Usage: Inspect before making changes
    
    ### Creation Tools
    **create_umg_widget_blueprint()**
    - Parameters: `name` (string), `parent_class` (string), `path` (string)
    - Returns: Widget creation confirmation with path
    - Usage: Create new widget blueprints
    
    **add_button_to_widget()**
    - Parameters: `widget_name` (string), `button_name` (string), `text` (string), `position` (array), `size` (array), `font_size` (integer), `color` (array), `background_color` (array)
    - Returns: Component creation confirmation
    - Usage: Add interactive buttons
    
    ### Styling Tools  
    **set_widget_property()**
    - Parameters: `widget_name` (string), `component_name` (string), `property_name` (string), `property_value` (any), `property_type` (string)
    - Returns: Property update confirmation
    - Usage: Universal property setting
    
    **create_widget_style_set()**
    - Parameters: `style_set_name` (string), `style_properties` (object), `description` (string)
    - Returns: Style set creation confirmation
    - Usage: Create reusable themes
    
    ### Blueprint Tools
    **create_blueprint()**
    - Parameters: `name` (string), `parent_class` (string)
    - Returns: Blueprint creation confirmation
    - Usage: Create new Blueprint classes
    
    **compile_blueprint()**
    - Parameters: `blueprint_name` (string)
    - Returns: Compilation status and any errors
    - Usage: REQUIRED after Blueprint graph changes
    
    ### Diagnostic Tools
    **check_unreal_connection()**
    - Parameters: None
    - Returns: Connection status, plugin state, troubleshooting info
    - Usage: First tool to use when issues occur

    ## Error Response Patterns
    
    ### Connection Errors
    ```json
    {
        "success": false,
        "error": "Failed to connect to Unreal Engine"
    }
    ```
    
    ### Validation Errors
    ```json
    {
        "success": false,
        "error": "Widget 'WBP_NonExistent' not found. Use search_items() to find correct name."
    }
    ```
    
    ### Property Errors
    ```json
    {
        "success": false,
        "error": "Property 'InvalidProp' not found on Button component. Use list_widget_properties() to see available properties."
    }
    ```
    
    ## Client Implementation Guidelines
    
    ### Error Handling
    ```python
    response = tool_call(parameters)
    if response is None:
        # Connection failure - check Unreal Engine status
        handle_connection_error()
    elif not response.get('success', False):
        # Tool error - check error message for guidance
        handle_tool_error(response.get('error'))
    else:
        # Success - process data
        process_result(response.get('data'))
    ```
    
    ### Parameter Validation
    - Always validate widget/component names exist first
    - Use proper type formatting for complex properties
    - Check required vs optional parameters
    - Validate ranges for numeric values
    
    ### Best Practices
    - Cache search results to avoid redundant calls
    - Implement retry logic for connection failures
    - Provide user feedback for long operations
    - Log operations for debugging
    
    This schema guide ensures proper tool usage and robust client implementations.
    """

@mcp.prompt()
def info():
    """Comprehensive guide to available Unreal MCP tools and best practices for AI assistants."""
    return """
    # Unreal Engine MCP Server - Complete Tool Reference
    
    **Connection:** Connects to Unreal Engine via TCP socket on localhost:55557
    **Format:** All tools return JSON responses with 'success' field and relevant data
    **Requirement:** Unreal Engine must be running with VibeUE plugin loaded
    
    ## 📚 ESSENTIAL: GET WORKFLOW GUIDANCE FIRST
    **⚠️ CRITICAL FOR AI ASSISTANTS: Before using any tools, call `get_help(topic="umg-guide")` for:**
    - **Proper workflow patterns** and step-by-step processes
    - **Component nesting & hierarchy** best practices 
    - **Sizing & positioning** strategies for responsive layouts
    - **Background creation** methods for different container types
    - **SVG generation & import** capabilities for custom graphics
    - **Error recovery** patterns and troubleshooting guides

    **👉 Always start with: `get_help(topic="umg-guide")` - This prevents common mistakes and ensures professional results**

    ## �🎨 UMG Widget Discovery & Inspection
    ### Widget Blueprint Management
    - `search_items(search_term="", asset_type="Widget", path="/Game", case_sensitive=False)`
      Search for existing Widget Blueprints in the project
    - `get_widget_blueprint_info(widget_name)`
      Get comprehensive info about a widget: hierarchy, components, bindings, events
    - `list_widget_components(widget_name)`
      List all components in a widget with their properties and hierarchy
    - `validate_widget_hierarchy(widget_name)`
      Check widget structure for issues and optimization opportunities
    
    ### Component Property Inspection
    - `get_widget_component_properties(widget_name, component_name)`
      Get all properties of a specific widget component with current values
    - `list_widget_properties(widget_name, component_name, include_inherited=True, category_filter="")`
      List available properties for a component with metadata
    - `get_widget_property(widget_name, component_name, property_name)`
      Get current value of a specific widget property
    - `get_available_events(widget_name, component_name)`
      List all available events for a widget component
    
    ## 🏗️ UMG Widget Creation & Modification
    ### Basic Widget Creation
    - `create_umg_widget_blueprint(name, parent_class="UserWidget", path="/Game/UI")`
      Create a new UMG Widget Blueprint
    
    ### Component Addition (All support position, size, styling)
    - `add_text_block_to_widget(widget_name, text_block_name, text="", position=[0,0], size=[200,50], font_size=12, color=[1,1,1,1])`
    - `add_button_to_widget(widget_name, button_name, text="", position=[0,0], size=[200,50], font_size=12, color=[1,1,1,1], background_color=[0.1,0.1,0.1,1])`
    - `add_image(widget_name, image_name, image_path="", position=[0,0], size=[100,100], color_tint=[1,1,1,1])`
    - `add_editable_text(widget_name, editable_text_name, text="", position=[0,0], size=[200,30], font_size=12, hint_text="")`
    - `add_editable_text_box(widget_name, text_box_name, text="", position=[0,0], size=[300,100], is_multiline=True)`
    - `add_check_box(widget_name, check_box_name, is_checked=False, position=[0,0], size=[20,20], label_text="")`
    - `add_slider(widget_name, slider_name, value=0.5, min_value=0, max_value=1, position=[0,0], size=[200,20])`
    - `add_progress_bar(widget_name, progress_bar_name, percent=0, position=[0,0], size=[200,20], fill_color=[0,1,0,1])`
    - `add_rich_text_block(widget_name, rich_text_name, text="", position=[0,0], size=[400,100], auto_wrap_text=True)`
    - `add_spacer(widget_name, spacer_name, size=[50,50], position=[0,0])`
    
    ### Layout Panels
    - `add_canvas_panel(widget_name, panel_name, position=[0,0], size=[400,300])`
      Absolute positioning panel
    - `add_vertical_box(widget_name, box_name, position=[0,0], size=[200,300], padding=[0,0,0,0])`
      Vertical layout container
    - `add_horizontal_box(widget_name, box_name, position=[0,0], size=[400,50], padding=[0,0,0,0])`
      Horizontal layout container
    - `add_grid_panel(widget_name, grid_name, position=[0,0], size=[400,300], columns=2, rows=2)`
      Grid-based layout
    - `add_scroll_box(widget_name, scroll_box_name, position=[0,0], size=[300,200], orientation="Vertical")`
      Scrollable content container
    - `add_widget_switcher(widget_name, switcher_name, position=[0,0], size=[400,300], active_widget_index=0)`
      Tab-like switching container
    
  ### Data Visualization
  - (removed) List/Tile/Tree view helpers — use `add_widget_component` and `set_widget_property` instead
    
    ### Advanced Layout Management
    - `add_child_to_panel(widget_name, parent_panel_name, child_widget_name, slot_index=-1, position=None, size=None)`
      Add child widgets to parent panels with layout constraints
    - `remove_child_from_panel(widget_name, parent_panel_name, child_widget_name)`
      Remove child widgets from parent panels
    - `add_widget_switcher_slot(widget_name, switcher_name, child_widget_name, slot_index)`
      Add slots to widget switcher for tab functionality
    
    ## 🎛️ Enhanced UI Building System
    ### Hierarchical UI Construction
    - `create_widget_with_parent(widget_name, widget_type, widget_name, parent_name, properties={})`
      Create widgets with automatic parent assignment and properties
    - `create_nested_layout(widget_name, layout_definition)`
      Build complex nested layouts from JSON definitions
    - `build_complex_ui(widget_name, ui_definition)`
      Comprehensive UI construction with root containers and children
    
    **Example Layout Definition:**
    ```json
    [
      {
        "type": "CanvasPanel",
        "name": "MainContainer",
        "properties": {"size": [800, 600]},
        "children": [
          {
            "type": "VerticalBox", 
            "name": "ContentLayout",
            "children": [
              {"type": "TextBlock", "name": "Title", "properties": {"Text": "My UI"}}
            ]
          }
        ]
      }
    ]
    ```
    
    ## 🎨 Widget Styling & Theming
    ### Property Management
    - `set_widget_property(widget_name, component_name, property_name, property_value, property_type="auto")`
      Set any property on any widget component (supports complex types)
    - `set_widget_style(widget_name, component_name, style_properties)`
      Set multiple style properties at once
    - (removed) Transform/visibility/z-order helpers — set slot and render properties via `set_widget_property`
    - `set_widget_slot_properties(widget_name, widget_component_name, slot_properties)`
      Set slot-specific properties (padding, alignment, etc.)
    
    ### Style Set System (NEW Enhanced Feature)
    - `create_widget_style_set(style_set_name, style_properties, description="")`
      Create reusable style sets for consistent theming
    - `apply_widget_theme(widget_name, component_name, theme_name)`
      Apply stored style sets to specific components
    
    **Example Style Properties (Complex Types Supported):**
    ```json
    {
      "BackgroundColor": {"R": 0.08, "G": 0.15, "B": 0.4, "A": 0.95},
      "BorderBrush": {"R": 0, "G": 0.6, "B": 1, "A": 1},
      "Font": {"Size": 14, "TypefaceFontName": "Bold"},
      "Padding": {"Left": 10, "Top": 5, "Right": 10, "Bottom": 5},
      "RenderOpacity": 0.92
    }
    ```
    
    ## ⚡ Widget Events & Interactivity
    ### Event Binding
    - `bind_input_events(widget_name, component_name, input_events)`
      Bind multiple input events at once

    ## 🖥️ Viewport & Display
    
      Add widget instance to game viewport
    
    ## 📝 Editor Tools
    ### Screenshots and Asset Management
    - `take_screenshot(filename, show_ui, resolution)`
      Capture screenshots
    
    ### Actor Management
    - `get_actors_in_level()`
      List all actors in current level
    - `find_actors_by_name(pattern)`
      Find actors by name pattern
    - `spawn_actor(name, type, location=[0,0,0], rotation=[0,0,0], scale=[1,1,1])`
      Create new actors
    - `delete_actor(name)`
      Remove actors
    - `set_actor_transform(name, location=None, rotation=None, scale=None)`
      Modify actor transform
    - `get_actor_properties(name)`
      Get actor properties
    - `set_actor_property(name, property_name, property_value)`
      Set actor properties
    
    ## 🔧 Blueprint Management
    ### Blueprint Creation & Compilation
    - `create_blueprint(name, parent_class)`
      Create new Blueprint classes
    - `compile_blueprint(blueprint_name)`
      Compile Blueprint changes (ALWAYS do this after modifications)
    - `reparent_blueprint(blueprint_name, new_parent_class)`
      Change Blueprint parent class
    
    ### Component Management
    - `add_component_to_blueprint(blueprint_name, component_type, component_name, location=[], rotation=[], scale=[], component_properties={})`
      Add components to Blueprints
    - (removed) Static mesh/physics helpers — use `set_component_property` and reflection tools
    - `set_blueprint_property(blueprint_name, property_name, property_value)`
      Set Blueprint class default properties
    - `set_component_property(blueprint_name, component_name, property_name, property_value)`
      Set component properties
    
    ### Blueprint Actor Spawning
    - `spawn_blueprint_actor(blueprint_name, actor_name, location=[0,0,0], rotation=[0,0,0])`
      Spawn Blueprint actors in the level
    
    ## 🔗 Blueprint Graph & Node Management
    ### Unified Node Orchestration
    - `manage_blueprint_node(blueprint_name, action, **kwargs)`
      One command for listing, adding, deleting, connecting, moving, finding, and inspecting nodes.
      Supported actions include `list`, `add`, `delete`, `connect`, `move`, `details`, `available`, `find`, `set_property`, `get_property`, and `list_custom_events`.
    - `manage_blueprint_function(blueprint_name, action, **kwargs)`
      Single entry point for Blueprint function graphs: `list`, `get`, `create`, `delete`, `list_params`, `add_param`, `remove_param`, `update_param`, `update_properties`.
    - `manage_blueprint_variables(blueprint_name, action, **kwargs)`
      Unified Blueprint variable operations covering creation, deletion, info, property access, modification, listing, and type discovery via actions like `create`, `delete`, `get_info`, `get_property`, `set_property`, `list`, `modify`, and `search_types`.
    - `summarize_event_graph(blueprint_name, max_nodes=200)`
      Get a readable overview of event graph structure.
    

    ## ⚙️ Project Tools
    - `create_input_mapping(action_name, key, input_type="Action")`
      Create input mappings (keys: SpaceBar, LeftMouseButton, W, A, S, D, etc.)
    
    ## 📋 Critical Best Practices for AI Assistants
    
    ### 🚨 ALWAYS Follow These Rules:
    1. **Check Unreal Connection First:** If commands fail with "Failed to connect", Unreal Engine isn't running
    2. **Compile After Blueprint Changes:** Always call `compile_blueprint()` after modifying Blueprints
    3. **Use Exact Widget Names:** Widget names are case-sensitive, use `search_items()` to find exact names
    4. **Verify Before Modify:** Use inspection tools before making changes
    5. **Handle Errors Gracefully:** Check 'success' field in all responses
    
    ### 🎯 Workflow Patterns:
    
    **Creating Complex UIs:**
    1. `create_umg_widget_blueprint()` - Create widget
    2. `add_canvas_panel()` - Add root container
    3. Use enhanced layout tools for nested structures
    4. `create_widget_style_set()` for consistent theming
    5. `apply_widget_theme()` to components
    
    **Modifying Existing Widgets:**
    1. `search_items()` - Find widget
    2. `get_widget_blueprint_info()` - Inspect structure
    3. `list_widget_components()` - See components
    4. Make modifications with appropriate tools
    5. Test with `validate_widget_hierarchy()`
    
    **Blueprint Development:**
    1. `create_blueprint()` - Create Blueprint
    2. `add_component_to_blueprint()` - Add components
    3. `add_blueprint_event_node()` - Add events
    4. `connect_blueprint_nodes()` - Wire logic
    5. `compile_blueprint()` - ESSENTIAL final step
    6. `spawn_blueprint_actor()` - Test in level
    
    ### 🔍 Color Format (RGBA 0.0-1.0):
    - `[1,1,1,1]` = White, fully opaque
    - `[0,0,0,0.5]` = Black, 50% transparent
    - `[1,0,0,1]` = Red, `[0,1,0,1]` = Green, `[0,0,1,1]` = Blue
    
    ### 📐 Position/Size Format:
    - `position=[X,Y]` in pixels from top-left
    - `size=[Width,Height]` in pixels
    - Use responsive layouts (VerticalBox, HorizontalBox) when possible
    
    ### ⚠️ Common Pitfalls to Avoid:
    - Don't assume widgets exist - always search first
    - Don't skip Blueprint compilation
    - Don't use invalid widget/component names
    - Don't ignore error responses
    - Don't forget to clean up test actors/widgets
    
    ### 🎨 Container Background Best Practices:
    - **Canvas Panels**: Always use Overlay wrapper (Canvas -> Overlay -> Image)
    - **ScrollBox/Box Containers**: Add Image directly as child with Fill sizing
    - **Border Widgets**: Use native BrushColor property, not child Images
    - **Complex Layouts**: Use `get_help(topic="umg-guide")` for container-specific patterns
    - **Performance**: Avoid unnecessary Overlay nesting, use native properties when available

    ## 🚀 REMEMBER: START WITH UMG GUIDE
    **Before beginning any UI work, call `get_help(topic="umg-guide")` to get:**
    - Complete workflow patterns and step-by-step processes
    - Professional component nesting and hierarchy guidelines
    - Sizing, positioning, and responsive layout strategies  
    - Background creation methods for all container types
    - SVG generation and import capabilities
    - Error recovery and troubleshooting guidance
    
    **This ensures professional results and prevents common mistakes!**

    ## 🆘 WHEN STUCK: USE GET_HELP TOOL
    **Call `get_help()` immediately when you:**
    - Can't find the right tool for your task
    - Don't know which parameters a tool needs
    - Get errors and need troubleshooting guidance
    - Need examples of multi-action tools (manage_blueprint_function, manage_blueprint_node)
    - Want to understand tool workflows and usage patterns
    - Are unsure about best practices or performance tips
    
    **The help tool contains complete documentation for ALL 50+ VibeUE MCP tools with:**
    - Detailed parameter lists and examples
    - Multi-action tool documentation with all available actions
    - Common usage patterns and workflows
    - Error troubleshooting guides
    - Performance optimization tips
    
    **Always check help BEFORE asking users for clarification on tool usage!**
    """

# ============================================================================
# SERVER STARTUP - AI ASSISTANT OPERATIONAL NOTES
# ============================================================================
# Run the server
if __name__ == "__main__":
    """
    STARTUP CHECKLIST FOR AI ASSISTANTS:
    
    Before Using Tools:
    1. Verify Unreal Engine 5.6 is running
    2. Confirm VibeUE plugin is loaded (check plugin manager)
    3. Ensure port 55557 is available (default plugin setting)
    4. Check vibe_ue.log for connection status
    
    Tool Usage Patterns:
    - Always check response 'success' field before proceeding
    - Use search/inspect tools before making modifications
    - Compile Blueprints after graph changes
    - Handle connection failures gracefully (retry or guide user)
    - **CRITICAL: Call get_help() when stuck or need tool guidance**
    
    When You Need Help:
    - Can't find the right tool? → Call get_help()
    - Don't know tool parameters? → Call get_help()
    - Getting errors? → Call get_help() for troubleshooting
    - Need multi-action tool examples? → Call get_help()
    - Want workflow guidance? → Call get_help()
    
    Common Issues:
    - "Failed to connect" = Start Unreal Engine with plugin
    - Tools return None = Check log for specific error
    - Partial responses = Network timeout, retry operation
    - JSON errors = Plugin communication issue, restart Unreal
    - **Tool confusion = Call get_help() for complete documentation**
    
    Success Indicators:
    - Log shows "Connected to Unreal Engine on startup"
    - Tools return {"success": true} responses
    - Widget operations reflect in Unreal editor
    """
    
    logger.info("Starting Unreal MCP server with stdio transport")
    logger.info("AI Assistant Guide: Use get_help() for complete tool documentation and troubleshooting")
    logger.info("Prerequisites: Unreal Engine 5.6 running with VibeUE plugin loaded")
    
    mcp.run(transport='stdio') 
