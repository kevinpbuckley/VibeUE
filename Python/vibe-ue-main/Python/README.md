# VibeUE Python MCP Server

<!-- mcp-name: io.github.kevinpbuckley/vibeue -->

Python bridge for interacting with Unreal Engine 5.6+ using the Model Context Protocol (MCP). This is the backend server for the VibeUE plugin, providing 60+ tools for Blueprint and UMG manipulation through AI assistants.

## 🚀 Quick Start

### Prerequisites
- **Python**: 3.10 or higher
- **Unreal Engine**: 5.6+ with VibeUE plugin enabled
- **uv** (recommended) or **pip**

### Option 1: Using uv (Recommended)

1. Install `uv` if you haven't already:
   ```bash
   # Windows PowerShell
   powershell -c "irm https://astral.sh/uv/install.ps1 | iex"
   
   # macOS/Linux
   curl -LsSf https://astral.sh/uv/install.sh | sh
   ```

2. Set up the environment:
   ```bash
   # Navigate to Python directory
   cd Python/vibe-ue-main/Python/
   
   # Create and activate virtual environment
   uv venv
   
   # Windows:
   .venv\Scripts\activate
   # macOS/Linux:
   source .venv/bin/activate
   
   # Install dependencies
   uv pip install -e .
   ```

### Option 2: Using pip

```bash
# Navigate to Python directory
cd Python/vibe-ue-main/Python/

# Create and activate virtual environment
python -m venv venv

# Windows:
venv\Scripts\activate
# macOS/Linux:
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt
```

### Optional: SVG Conversion Support

The `svg_to_png` action in `manage_asset` tool requires additional packages for converting SVG files to optimized PNG textures for UMG.

**Using uv:**
```bash
uv pip install .[svg]
```

**Using pip:**
```bash
pip install cairosvg pillow numpy
```

**Required for:**
- `manage_asset(action="svg_to_png", ...)` - Convert SVG vector graphics to PNG textures
- Automatic UMG optimizations (premultiplied alpha, transparent cleanup)
- Custom UI texture workflows from SVG design files

If you don't need SVG conversion, these packages are optional and don't affect other functionality.

## 🎯 Features

The MCP server provides comprehensive tools for:

- **Blueprint Management**: Create, modify, compile Blueprints
- **UMG Widget System**: Add buttons, text, images, complex layouts  
- **Component Properties**: Colors, positions, sizes, styling
- **Asset Management**: Import textures, search assets, open editors
- **Event Binding**: Input events and widget interactions
- **Advanced Layouts**: Canvas panels, overlays, scroll boxes
- **Data Binding**: MVVM patterns and list templates

## 🔧 MCP Client Configuration

### Claude Desktop
Add to your `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "vibe-ue": {
      "command": "uv",
      "args": [
        "run", 
        "python", 
        "unreal_mcp_server.py"
      ],
      "cwd": "path/to/VibeUE/Python/vibe-ue-main/Python"
    }
  }
}
```

### MCPJam Inspector
Use the `MCP-Inspector.bat` in the plugin root for testing and development.

## 🧪 Testing Scripts

Several test scripts are available in the `scripts/` folder for direct tool testing without running the full MCP server:

```bash
# Make sure you're in the virtual environment
python scripts/test_blueprint_tools.py
python scripts/test_widget_tools.py
```

## 🔍 Troubleshooting

### Common Issues

1. **"Module not found" errors**
   ```bash
   # Ensure you're in the correct directory
   cd Python/vibe-ue-main/Python/
   
   # Reinstall dependencies
   uv pip install -e .  # or pip install -r requirements.txt
   ```

2. **Unreal Engine Connection Issues**
   - **CRITICAL**: Unreal Engine editor must be running before starting the MCP server
   - Ensure the VibeUE plugin is enabled in your project
   - Check that UnrealMCP plugin is also enabled
   - Verify HTTP REST API is enabled in Unreal Editor Preferences

3. **Virtual Environment Problems**
   ```bash
   # Remove and recreate
   # Windows:
   rmdir /s .venv
   # macOS/Linux:
   rm -rf .venv
   
   # Recreate
   uv venv  # or python -m venv venv
   ```

4. **Port/Connection Issues**
   - Check `unreal_mcp.log` for detailed error information
   - Verify Unreal's HTTP REST API port (usually 30010)
   - Ensure no firewall blocking localhost connections

### Debug Mode

Run with verbose logging:
```bash
python unreal_mcp_server.py --debug
```

## 📋 System Requirements

- **Python**: 3.10+
- **Unreal Engine**: 5.6+ (5.5 minimum)
- **Operating System**: Windows 10/11, macOS 10.15+, Ubuntu 18.04+
- **Memory**: 4GB+ RAM recommended
- **Network**: Localhost HTTP communication required

## � Publishing to PyPI and MCP Registry

This package is automatically published to both PyPI and the MCP Registry when a new version tag is pushed.

### Automated Publishing (Recommended)

1. **Update version** in `pyproject.toml` and commit changes
2. **Create and push a version tag**:
   ```bash
   git tag v0.1.0
   git push origin v0.1.0
   ```
3. **GitHub Actions automatically**:
   - Builds the Python package
   - Publishes to PyPI (requires `PYPI_API_TOKEN` secret)
   - Publishes to MCP Registry using GitHub OIDC
   - Creates a GitHub Release

### Setup Requirements

**First-time setup** (maintainers only):
1. **PyPI API Token**: Add `PYPI_API_TOKEN` to GitHub repository secrets
   - Create token at https://pypi.org/manage/account/token/
   - Add to Settings → Secrets and variables → Actions

2. **GitHub OIDC**: Already configured in the workflow (no additional setup needed)

### Manual Publishing

If needed, you can publish manually:

```bash
# Install build tools
pip install build twine

# Build package
python -m build

# Publish to PyPI
python -m twine upload dist/*

# Install MCP Publisher
curl -L "https://github.com/modelcontextprotocol/registry/releases/latest/download/mcp-publisher_$(uname -s | tr '[:upper:]' '[:lower:]')_$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/').tar.gz" | tar xz mcp-publisher

# Login and publish to MCP Registry
./mcp-publisher login github-oidc
./mcp-publisher publish
```

### Validation

Validate `server.json` before publishing:
```bash
python validate_server.py
```

## �🔗 Related Documentation

- [Main VibeUE README](../../README.md) - Complete plugin overview and 60+ tools reference
- [UMG-Guide.md](../../UMG-Guide.md) - Widget styling best practices and patterns
- [MCP Protocol](https://modelcontextprotocol.io/) - Official MCP documentation 