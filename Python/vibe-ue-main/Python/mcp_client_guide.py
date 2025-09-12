"""
MCP Client Integration Guide

COMPREHENSIVE CLIENT INTEGRATION REFERENCE

This document provides everything MCP clients need to understand and integrate
with the UnrealMCP server effectively.
"""

# ============================================================================
# MCP CLIENT INTEGRATION GUIDE
# ============================================================================

UNREAL_MCP_INTEGRATION_GUIDE = {
    "server_info": {
        "name": "UnrealMCP",
        "version": "2.0.0",
        "description": "Advanced Unreal Engine 5.6 integration server",
        "protocol": "MCP (Model Context Protocol)",
        "transport": "stdio",
        "target_engine": "Unreal Engine 5.6",
        "required_plugin": "UnrealMCP"
    },
    
    "connection_requirements": {
        "host": "127.0.0.1",
        "port": 55557,
        "protocol": "TCP Socket (internal)",
        "transport": "stdio (MCP)",
        "prerequisites": [
            "Unreal Engine 5.6 running",
            "UnrealMCP plugin loaded and enabled",
            "Port 55557 available",
            "Project with UnrealMCP plugin"
        ]
    },
    
    "tool_categories": {
        "core_engine": {
            "module": "editor_tools, blueprint_tools, node_tools",
            "purpose": "Basic Unreal Engine interaction",
            "tools": ["actor management", "Blueprint lifecycle", "graph manipulation", "project settings"],
            "complexity": "intermediate",
            "use_cases": ["level design", "Blueprint development", "project configuration"]
        },
        "umg_basic": {
            "module": "umg_tools",
            "purpose": "Legacy widget system",
            "tools": ["basic widget creation", "simple events"],
            "complexity": "basic",
            "use_cases": ["simple UI creation", "basic interactivity"]
        },
        "umg_enhanced": {
            "modules": [
                "umg_discovery", "umg_components", "umg_layout", 
                "umg_styling", "umg_events", "umg_data_binding", "umg_animation"
            ],
            "purpose": "Advanced UI development system",
            "tools": ["hierarchical UI construction", "style sets", "data binding", "animations"],
            "complexity": "advanced",
            "use_cases": ["complex UI systems", "game menus", "HUD systems", "interactive interfaces"]
        },
        "diagnostics": {
            "module": "system_diagnostics",
            "purpose": "System monitoring and troubleshooting",
            "tools": ["connection testing", "validation", "quick reference"],
            "complexity": "utility",
            "use_cases": ["debugging", "system verification", "AI guidance"]
        }
    },
    
    "response_patterns": {
        "success": {
            "structure": {"success": True, "data": "..."},
            "handling": "Process data field for results"
        },
        "error": {
            "structure": {"success": False, "error": "message"} or {"status": "error", "error": "message"},
            "handling": "Check error field for troubleshooting"
        },
        "connection_failure": {
            "structure": None,
            "handling": "Server not connected - check prerequisites"
        }
    },
    
    "common_workflows": {
        "first_time_setup": [
            "Use server_status() prompt to check system",
            "Use check_unreal_connection() tool to verify",
            "Use check_unreal_connection() to test",
            "Use info() prompt for complete documentation"
        ],
        "widget_development": [
            "search_items() to find existing widgets",
            "get_widget_blueprint_info() to inspect structure",
            "Use appropriate creation/modification tools",
            "validate_widget_hierarchy() to verify integrity"
        ],
        "blueprint_development": [
            "create_blueprint() or find existing",
            "add_component_to_blueprint() for components",
            "add_blueprint_event_node() for events",
            "connect_blueprint_nodes() for logic",
            "compile_blueprint() - CRITICAL final step"
        ]
    },
    
    "error_recovery": {
        "connection_errors": {
            "Failed to connect": "Start Unreal Engine 5.6 with UnrealMCP plugin",
            "Plugin not responding": "Reload UnrealMCP plugin in Unreal",
            "Port already in use": "Check for port conflicts on 55557",
            "Timeout": "Verify Unreal Engine is not frozen"
        },
        "tool_errors": {
            "Widget not found": "Use search_items() to find exact name",
            "Blueprint compilation failed": "Check node connections and types",
            "Property not found": "Use list_widget_properties() to see available",
            "Component not found": "Use list_widget_components() to verify"
        }
    },
    
    "advanced_features": {
        "style_sets": {
            "description": "Persistent theming system",
            "usage": "create_widget_style_set() then apply_widget_theme()",
            "benefits": "Consistent styling across multiple components"
        },
        "hierarchical_ui": {
            "description": "JSON-defined complex UI structures",
            "usage": "build_complex_ui() with nested definitions",
            "benefits": "Rapid UI prototyping and consistent layouts"
        },
        "animation_system": {
            "description": "Widget animation and transitions",
            "usage": "set_widget_property() with proper styling",
            "benefits": "Professional UI interactions and feedback"
        }
    },
    
    "property_type_guide": {
        "simple": {
            "types": ["string", "integer", "float", "boolean"],
            "format": "Direct values",
            "example": "set_widget_property(..., 'Text', 'Hello World')"
        },
        "colors": {
            "types": ["RGBA arrays", "Color objects"],
            "format": "[R, G, B, A] with 0.0-1.0 values or {R: 0.5, G: 0.8, B: 1.0, A: 1.0}",
            "example": "set_widget_property(..., 'Color', [1, 0, 0, 1])"
        },
        "transforms": {
            "types": ["Position", "Size", "Scale"],
            "format": "[X, Y] or [X, Y, Z] arrays",
            "example": "set_widget_transform(..., position=[100, 50], size=[200, 30])"
        },
        "fonts": {
            "types": ["Font objects"],
            "format": "{Size: number, TypefaceFontName: string}",
            "example": "set_widget_property(..., 'Font', {Size: 16, TypefaceFontName: 'Bold'})"
        }
    },
    
    "best_practices": {
        "for_ai_assistants": [
            "Always check connection status first",
            "Use search/inspect tools before modifications",
            "Handle errors gracefully with troubleshooting",
            "Compile Blueprints after graph changes",
            "Verify widget names are case-sensitive exact matches"
        ],
        "for_mcp_clients": [
            "Implement retry logic for connection failures",
            "Cache search results to minimize redundant calls",
            "Provide user feedback for long-running operations",
            "Handle partial responses gracefully",
            "Log important operations for debugging"
        ],
        "performance": [
            "Batch property changes using set_widget_style()",
            "Use style sets for consistent theming",
            "Validate hierarchy after complex changes",
            "Clean up test actors and widgets",
            "Monitor log files for issues"
        ]
    }
}

def get_integration_guide():
    """Get the complete MCP client integration guide."""
    return UNREAL_MCP_INTEGRATION_GUIDE

def format_integration_guide_for_prompt():
    """Format the integration guide as a prompt response."""
    guide = get_integration_guide()
    
    output = "# UnrealMCP Client Integration Guide\n\n"
    
    for section_name, section_data in guide.items():
        output += f"## {section_name.replace('_', ' ').title()}\n\n"
        
        if isinstance(section_data, dict):
            for key, value in section_data.items():
                if isinstance(value, list):
                    output += f"### {key.replace('_', ' ').title()}\n"
                    for item in value:
                        output += f"- {item}\n"
                    output += "\n"
                elif isinstance(value, dict):
                    output += f"### {key.replace('_', ' ').title()}\n"
                    for subkey, subvalue in value.items():
                        if isinstance(subvalue, list):
                            output += f"**{subkey}:** {', '.join(subvalue)}\n"
                        else:
                            output += f"**{subkey}:** {subvalue}\n"
                    output += "\n"
                else:
                    output += f"**{key.replace('_', ' ').title()}:** {value}\n"
        else:
            output += f"{section_data}\n"
        
        output += "\n"
    
    return output
