# VibeUE Help Topics

## Available Topics

Use `get_help(topic="topic_name")` to access specific help content.

### Quick Start & Overview

**overview** - VibeUE MCP overview and quick reference
- What is VibeUE
- Core capabilities
- Connection architecture
- Essential first steps
- Quick reference with common commands

### Workflows & Best Practices

**blueprint-workflow** - Complete Blueprint development workflow
- Critical dependency order (Variables → Functions → Nodes → Event Graph)
- 4-phase Blueprint development process
- Common mistakes and how to avoid them
- Best practices checklist

**umg-guide** - UMG widget development guide
- Widget structure best practices
- DO's and DON'Ts
- Container-specific patterns (Canvas, ScrollBox, Border, VBox/HBox)
- Component discovery and styling
- Compilation requirements
- Modern UI color examples

### Tool References

**node-tools** - Comprehensive node tools reference
- Node discovery with get_available_blueprint_nodes()
- Spawner key vs fuzzy search creation
- CRITICAL node_params requirements (Variable Set/Get, Cast nodes)
- Pin connection format and patterns
- Common pin names by node type
- Node inspection and troubleshooting

**node-positioning** - Blueprint node positioning best practices
- Left-to-right flow principles (CRITICAL)
- Consistent spacing standards (250-400 units)
- Branch layout patterns (±100 Y offset)
- Execution pin alignment
- Position calculation helpers
- Common anti-patterns to avoid
- Real-world examples from challenge documentation

**properties** - Component property setting guide (⭐ NEW!)
- Color property formats (FColor, FLinearColor)
- Common color values reference (yellow, red, blue, cyan, etc.)
- Light-specific properties (LightColor, Intensity, AttenuationRadius)
- Vector/Rotator property formats
- Common mistakes and solutions
- Complete workflow examples
- Troubleshooting property setting failures
- Property type quick reference

**multi-action-tools** - Multi-action tool reference
- manage_blueprint_function (functions, parameters, locals)
- manage_blueprint_variables (type search, creation with type_path)
- manage_blueprint_components (component management, properties)
- manage_blueprint_node (node operations, connections)
- Action-specific parameters and patterns
- Complete workflow examples

**asset-discovery** - Asset finding and management
- Universal search_items() usage
- Package path vs object path (CRITICAL difference)
- Supported asset types
- Texture import/export
- Opening assets in editor
- Performance tips and error recovery

### Problem Solving

**troubleshooting** - Comprehensive troubleshooting guide
- Connection issues (failed to connect, timeout)
- Blueprint issues (not found, compilation failed, dependencies)
- Node issues (type not found, wrong pins, invalid direction)
- UMG widget issues (widget/component/property not found)
- Common error patterns table
- Diagnostic commands
- Escalation steps

## Topic Organization

Topics are organized by use case:

1. **Getting Started**: overview
2. **Development Workflows**: blueprint-workflow, umg-guide, node-positioning
3. **Tool References**: node-tools, multi-action-tools, asset-discovery
4. **Problem Solving**: troubleshooting

## Usage Examples

```python
# Get overview
get_help(topic="overview")

# Learn Blueprint workflow
get_help(topic="blueprint-workflow")

# Understand node creation
get_help(topic="node-tools")

# Learn proper node positioning
get_help(topic="node-positioning")

# Reference multi-action tools
get_help(topic="multi-action-tools")

# Style UMG widgets
get_help(topic="umg-guide")

# Find and manage assets
get_help(topic="asset-discovery")

# Troubleshoot issues
get_help(topic="troubleshooting")

# See all available topics
get_help(topic="topics")
```

## Default Topic

If no topic specified, `get_help()` returns **overview** by default.

## Creating New Topics

Topics are markdown files in `resources/topics/` directory:
- One topic per file
- Clear headings and organization
- Code examples for complex patterns
- Tables for reference information
- Cross-references using get_help(topic="...")
- 50-200 lines per topic (AI-optimized size)

## Topic Best Practices

1. **Focused Content**: Each topic covers single area
2. **Executable Examples**: Include working Python code
3. **Clear Structure**: Use headings, lists, tables
4. **Cross-References**: Link related topics
5. **Search Keywords**: Include common search terms
6. **Error Guidance**: Show common mistakes and solutions
