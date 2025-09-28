# GitHub Copilot Instructions for VibeUE

## Repository Overview

VibeUE is an **Unreal Engine 5.6+ plugin** that provides AI assistant integration through the **Model Context Protocol (MCP)**. This allows AI assistants (VS Code, Claude Desktop, Cursor, Windsurf) to control Unreal Engine directly using natural language commands.

**Core Components:**
- **C++ Plugin** (`Source/VibeUE/`) - Unreal Engine plugin source
- **Python MCP Server** (`Python/vibe-ue-main/Python/`) - AI assistant bridge with 60+ tools
- **Documentation** (`docs/`, `README.md`) - Comprehensive setup and usage guides

## Key Architecture Principles

### 🎯 Primary Purpose
Enable AI assistants to manipulate Unreal Engine assets through natural language:
- **Blueprint creation and modification**
- **UMG widget development and styling**
- **Asset management and discovery**
- **Real-time property manipulation**

### 🔧 Technical Stack
- **Unreal Engine 5.6+** (C++ plugin)
- **Python 3.12+** (MCP server with 60+ tools)
- **Model Context Protocol** (AI assistant communication)
- **TCP Socket Communication** (localhost:55557)

## Development Guidelines

### 🚨 Critical Success Patterns

**For AI Assistant Integration:**
1. **Always check Unreal connection first** - If tools fail with "Failed to connect", Unreal Engine isn't running
2. **Use exact widget names** - Names are case-sensitive, use `search_items()` to find exact matches
3. **Compile after Blueprint changes** - Always call `compile_blueprint()` after modifying Blueprint graphs
4. **Verify before modify** - Use inspection tools (`get_widget_blueprint_info()`, `list_widget_components()`) before changes
5. **Handle errors gracefully** - Check 'success' field in all tool responses

**Essential Workflow Pattern:**
```python
# 1. Discovery first
search_items(search_term='target_name', asset_type='Widget')

# 2. Inspect structure  
get_widget_blueprint_info('exact_widget_name')
list_widget_components('exact_widget_name')

# 3. Make systematic changes
set_widget_property('widget_name', 'component_name', 'property', value)

# 4. Validate results
validate_widget_hierarchy('widget_name')
```

### 🐍 Python MCP Tools Development

**When working with `@mcp.tool()` decorated functions:**

- **Never use**: `Any`, `object`, `Optional[T]`, `Union[T]` parameter types
- **For optional parameters**: Use `x: T = None` instead of `x: T | None = None`
- **Always include**: Comprehensive docstrings with valid input examples
- **Error handling**: Return structured JSON with `success` field
- **Type validation**: Handle defaults within method body

**Example Tool Pattern:**
```python
@mcp.tool()
def example_tool(widget_name: str, component_name: str = "DefaultComponent"):
    """
    Tool description with clear purpose.
    
    Args:
        widget_name: Exact widget blueprint name (case-sensitive)
        component_name: Target component name, defaults to "DefaultComponent"
    
    Returns:
        {"success": bool, "data": any, "error": str (if failed)}
    
    Example:
        example_tool("WBP_MainMenu", "Button_Start")
    """
    if not widget_name:
        return {"success": False, "error": "Widget name is required"}
    
    # Implementation...
    return {"success": True, "data": result}
```

### 🎨 UMG Widget Development Best Practices

**Component Hierarchy Rules:**
- **Canvas Panel**: Use for absolute positioning, backgrounds, complex layouts
- **Vertical/Horizontal Box**: Use for linear layouts with automatic sizing
- **Grid Panel**: Use for tabular data or grid-based layouts
- **Scroll Box**: Use for scrollable content areas

**Property Naming Conventions:**
- **Colors**: Use RGBA arrays `[R, G, B, A]` with values 0.0-1.0
- **Positions**: Use `[X, Y]` coordinate arrays
- **Sizes**: Use `[Width, Height]` arrays
- **Fonts**: Use objects like `{"Size": 16, "TypefaceFontName": "Bold"}`

**Modern UI Color Palettes:**
```python
MODERN_COLORS = {
    "primary_blue": [0.2, 0.6, 1.0, 1.0],
    "dark_background": [0.08, 0.08, 0.08, 1.0],
    "text_light": [0.95, 0.95, 0.95, 1.0],
    "success_green": [0.3, 0.69, 0.31, 1.0]
}
```

### 🔍 Asset Discovery & Management

**Always Use Full Paths:**
- Prefer `search_items()` results for exact asset paths
- Avoid partial names that trigger expensive Asset Registry searches
- Cache search results to minimize redundant calls

**Error Recovery Patterns:**
```python
# Connection Issues
"Failed to connect" → Check if Unreal Engine is running
"Plugin not responding" → Reload VibeUE plugin in Unreal
"Widget not found" → Use search_items() to find exact name

# Blueprint Issues  
"Compilation failed" → Check node connections and variable types
"Property not found" → Use list_widget_properties() to see available options
```

## File Structure Guidelines

### 📁 Directory Organization
```
VibeUE/
├── Source/VibeUE/           # C++ plugin source
│   ├── Private/             # Implementation files
│   ├── Public/              # Header files
│   └── VibeUE.Build.cs     # Build configuration
├── Python/vibe-ue-main/Python/  # MCP server
│   ├── tools/               # Tool modules (actor, editor, blueprint)
│   ├── scripts/             # Example scripts and demos
│   ├── vibe_ue_server.py   # Main MCP server
│   ├── ai_guidance.py      # AI assistant patterns
│   └── mcp_client_guide.py # Client integration guide
├── docs/                    # Documentation
└── README.md               # Main documentation
```

### 🧪 Testing Patterns

**For MCP Tools:**
- Test connection with `check_unreal_connection()`
- Use validation tools after making changes
- Test with realistic UE5 project scenarios
- Verify tool responses have proper `success` field structure

**For C++ Plugin:**
- Follow Unreal Engine coding standards
- Test in Development and Shipping builds
- Verify plugin loading in fresh UE5 projects

## Integration Guidelines

### 🔗 MCP Client Configuration

**VS Code (Recommended):**
```json
{
  "servers": {
    "VibeUE": {
      "type": "stdio",
      "command": "python",
      "args": ["Plugins\\VibeUE\\Python\\vibe-ue-main\\Python\\vibe_ue_server.py"],
      "env": {},
      "cwd": "${workspaceFolder}"
    }
  }
}
```

### 🚀 Performance Optimization

- **Batch property changes** using `set_widget_style()` for multiple modifications
- **Use style sets** for consistent theming across widgets  
- **Validate hierarchies** after complex structural changes
- **Monitor log files** (`vibe_ue.log`) for connection issues

### 🎯 AI Assistant Best Practices

**When helping users:**
- Start with `get_umg_guide()` for workflow guidance
- Use discovery tools before making modifications
- Provide clear error messages with troubleshooting steps
- Validate results and give user feedback
- Reference existing color palettes and UI patterns

**Common Workflows:**
1. **Widget Discovery**: `search_items()` → `get_widget_blueprint_info()` → `list_widget_components()`
2. **Modern Styling**: Apply consistent color schemes, typography, and spacing
3. **Layout Management**: Use appropriate panel types for responsive designs
4. **Error Recovery**: Provide clear diagnostics and recovery steps

## Contributing Guidelines

### 🔄 Code Changes
- **Minimal modifications** - Change as few lines as possible
- **Maintain existing patterns** - Follow established code style
- **Test thoroughly** - Verify changes don't break existing functionality
- **Document changes** - Update relevant documentation

### 📝 Documentation Updates
- Keep README.md current with major changes
- Update Python docstrings for tool modifications
- Maintain AI guidance patterns in `ai_guidance.py`
- Update MCP client configuration examples

### 🐛 Issue Resolution
- Reproduce issues in clean UE5 environment
- Check connection status first for tool failures
- Reference existing error patterns in `mcp_client_guide.py`
- Provide detailed troubleshooting steps

---

**Remember**: This is an **experimental** project focused on AI-assisted Unreal Engine development. Always prioritize clarity, error handling, and user guidance in AI assistant interactions.