"""
AI Assistant Guidance for UnrealMCP Tools

COMPREHENSIVE GUIDE FOR AI ASSISTANTS USING UNREALMCP

This module provides quick reference patterns, common workflows, and best practices
specifically designed for AI assistants working with Unreal Engine UMG widgets.

CRITICAL SUCCESS PATTERNS:
1. Always search_items() before any widget operation
2. Use get_widget_blueprint_info() to understand structure
3. Use list_widget_components() to get exact component names
4. Apply changes systematically with proper error checking
5. Validate results and provide user feedback

COMMON WORKFLOWS:
- Widget Discovery & Inspection
- Generic UI Styling Application  
- Component Property Modification
- Error Recovery and Troubleshooting
"""

# Quick Reference Patterns for AI Assistants

MODERN_COLOR_PALETTE = {
    "primary_blue": [0.2, 0.6, 1.0, 1.0],
    "secondary_gray": [0.5, 0.5, 0.5, 1.0], 
    "success_green": [0.3, 0.69, 0.31, 1.0],
    "warning_orange": [1.0, 0.6, 0.0, 1.0],
    "light_background": [0.95, 0.95, 0.95, 1.0],
    "white_surface": [1.0, 1.0, 1.0, 1.0],
    "text_dark": [0.13, 0.13, 0.13, 1.0],
    "accent_red": [0.96, 0.26, 0.21, 1.0]
}

DARK_MODE_PALETTE = {
    "primary_blue": [0.38, 0.69, 1.0, 1.0],
    "secondary_gray": [0.6, 0.6, 0.6, 1.0],
    "success_green": [0.4, 0.87, 0.4, 1.0],
    "warning_orange": [1.0, 0.74, 0.0, 1.0],
    "dark_background": [0.08, 0.08, 0.08, 1.0],
    "dark_surface": [0.15, 0.15, 0.15, 1.0],
    "text_light": [0.95, 0.95, 0.95, 1.0],
    "text_secondary": [0.7, 0.7, 0.7, 1.0]
}

GAMING_UI_PALETTE = {
    "neon_green": [0.0, 1.0, 0.5, 1.0],
    "electric_blue": [0.0, 0.8, 1.0, 1.0],
    "hot_orange": [1.0, 0.65, 0.0, 1.0],
    "bright_red": [1.0, 0.0, 0.0, 1.0],
    "dark_background": [0.05, 0.05, 0.1, 0.95],
    "medium_dark": [0.1, 0.1, 0.2, 0.9],
    "bright_text": [1.0, 1.0, 1.0, 1.0],
    "glow_cyan": [0.0, 1.0, 1.0, 0.8]
}

GENERIC_STYLE_SETS = {
    "modern_ui_complete": {
        "TextBlock": {
            "ColorAndOpacity": MODERN_COLOR_PALETTE["text_dark"],
            "Font": {"Size": 14, "TypefaceFontName": "Regular"},
            "Justification": "Left"
        },
        "Button": {
            "ColorAndOpacity": MODERN_COLOR_PALETTE["primary_blue"],
            "BackgroundColor": MODERN_COLOR_PALETTE["white_surface"],
            "BorderColor": MODERN_COLOR_PALETTE["primary_blue"]
        },
        "Panel": {
            "BackgroundColor": MODERN_COLOR_PALETTE["white_surface"],
            "BorderColor": MODERN_COLOR_PALETTE["secondary_gray"],
            "Padding": {"Left": 12, "Top": 12, "Right": 12, "Bottom": 12}
        },
        "ProgressBar": {
            "FillColor": MODERN_COLOR_PALETTE["success_green"],
            "BackgroundColor": MODERN_COLOR_PALETTE["light_background"]
        }
    },
    
    "dark_mode_complete": {
        "TextBlock": {
            "ColorAndOpacity": DARK_MODE_PALETTE["text_light"],
            "Font": {"Size": 14, "TypefaceFontName": "Regular"},
            "Justification": "Left"
        },
        "Button": {
            "ColorAndOpacity": DARK_MODE_PALETTE["primary_blue"],
            "BackgroundColor": DARK_MODE_PALETTE["dark_surface"],
            "BorderColor": DARK_MODE_PALETTE["primary_blue"]
        },
        "Panel": {
            "BackgroundColor": DARK_MODE_PALETTE["dark_surface"],
            "BorderColor": DARK_MODE_PALETTE["secondary_gray"],
            "Padding": {"Left": 12, "Top": 12, "Right": 12, "Bottom": 12}
        },
        "ProgressBar": {
            "FillColor": DARK_MODE_PALETTE["success_green"],
            "BackgroundColor": DARK_MODE_PALETTE["dark_background"]
        }
    },
    
    "gaming_ui": {
        "TextBlock": {
            "ColorAndOpacity": GAMING_UI_PALETTE["bright_text"],
            "Font": {"Size": 16, "TypefaceFontName": "Bold"},
            "Justification": "Center"
        },
        "Button": {
            "ColorAndOpacity": GAMING_UI_PALETTE["neon_green"],
            "BackgroundColor": GAMING_UI_PALETTE["medium_dark"],
            "BorderColor": GAMING_UI_PALETTE["electric_blue"]
        }
    }
}

WORKFLOW_PATTERNS = {
    "widget_discovery": [
        "search_items(search_term='target_name', asset_type='Widget')",
        "get_widget_blueprint_info(exact_widget_name)",
        "list_widget_components(exact_widget_name)"
    ],
    
    "modern_inventory_styling": [
        "# Step 1: Find the inventory widget",
        "search_items(search_term='Inventory', asset_type='Widget')",
        "# Step 2: Get widget structure", 
        "get_widget_blueprint_info('WBP_Inventory')",
        "# Step 3: List all components to style",
        "list_widget_components('WBP_Inventory')",
        "# Step 4: Create modern style set",
        "create_widget_style_set('ModernInventory', modern_style)",
        "# Step 5: Apply to each component systematically",
        "set_widget_property('WBP_Inventory', 'component_name', 'property', value)"
    ],
    
    "dark_mode_conversion": [
        "# Step 1: Identify all components",
        "list_widget_components('WBP_TargetWidget')",
        "# Step 2: Apply dark background",
        "set_widget_property('WBP_TargetWidget', 'Background', 'ColorAndOpacity', [0.08, 0.08, 0.08, 1.0])",
        "# Step 3: Update text colors for dark mode",
        "set_widget_property('WBP_TargetWidget', 'Title_Text', 'ColorAndOpacity', [0.95, 0.95, 0.95, 1.0])"
    ],
    
    "error_recovery": [
        "# If widget not found:",
        "search_items(search_term='broader_search', asset_type='Widget')",
        "# If component not found:",
        "list_widget_components(widget_name)",
        "# If property fails:",
        "list_widget_properties(widget_name, component_name)"
    ]
}

COMMON_PROPERTY_MAPPINGS = {
    "TextBlock": {
        "text_content": "Text",
        "text_color": "ColorAndOpacity", 
        "font_settings": "Font",
        "text_alignment": "Justification",
        "visibility": "Visibility"
    },
    "Button": {
        "text_content": "Text",
        "background_color": "BackgroundColor",
        "text_color": "ColorAndOpacity",
        "border_color": "BorderBrushColor",
        "hover_color": "HoveredColor",
        "pressed_color": "PressedColor"
    },
    "Image": {
        "image_color": "ColorAndOpacity",
        "background_color": "BrushColor", 
        "image_asset": "Brush",
        "tint_color": "ColorAndOpacity"
    },
    "CanvasPanel": {
        "background_color": "BrushColor",
        "border_color": "BorderBrushColor",
        "padding": "Padding"
    },
    "ProgressBar": {
        "fill_color": "FillColorAndOpacity",
        "background_color": "BackgroundColor",
        "progress_value": "Percent"
    }
}

ERROR_PATTERNS_AND_SOLUTIONS = {
    "Widget not found": {
        "likely_causes": [
            "Widget name is incorrect or case-sensitive",
            "Widget is in different path than expected", 
            "Widget Blueprint doesn't exist"
        ],
        "solutions": [
            "Use search_items() with partial name",
            "Try search_items() with empty search_term to see all widgets",
            "Check if widget exists in Content Browser"
        ]
    },
    
    "Component not found": {
        "likely_causes": [
            "Component name is incorrect",
            "Component doesn't exist in widget hierarchy",
            "Component name changed"
        ],
        "solutions": [
            "Use list_widget_components() to get exact names",
            "Check component hierarchy in get_widget_blueprint_info()",
            "Verify component exists in widget structure"
        ]
    },
    
    "Property setting failed": {
        "likely_causes": [
            "Property name is incorrect",
            "Property value type is wrong",
            "Property doesn't exist for component type"
        ],
        "solutions": [
            "Use list_widget_properties() to see available properties",
            "Check property value format (colors as [R,G,B,A])",
            "Use get_widget_property() to see current value format"
        ]
    }
}

STYLING_BEST_PRACTICES = [
    "✅ Use consistent color palettes across all components",
    "✅ Ensure sufficient contrast for accessibility (4.5:1 minimum)",
    "✅ Test styling with different screen sizes",
    "✅ Use appropriate font sizes for readability", 
    "✅ Apply padding and margins for proper spacing",
    "✅ Test interactive elements (hover states, etc.)",
    "✅ Validate widget hierarchy before major changes",
    "✅ Create style sets for reusable themes"
]

# get_ai_quick_reference function removed - use search_items() instead of search_widgets()
