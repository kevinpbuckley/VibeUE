# UMG Widget Styling Guide for AI Assistants

## Overview
This guide provides DO's and DON'Ts for AI assistants when styling UMG widgets in Unreal Engine. Following these guidelines ensures proper widget hierarchy, performance, and visual consistency across all widget types and themes.

**IMPORTANT**: This guide focuses on **layout structure and component organization rules**, not specific color schemes. Color choices should be determined by user requirements or project-specific themes.

## Compilation Requirements
- **ALWAYS** compile the widget after making styling changes using `compile_blueprint(blueprint_name)`
- **REQUIRED** for changes to take effect in the editor and runtime
- **BEST PRACTICE** - Compile immediately after completing styling modifications

## ⚠️ CRITICAL: Property Setting Syntax

### Using set_widget_property for Slot Properties
**IMPORTANT**: The `set_widget_slot_properties` function is NOT available as an MCP tool. Instead, use `set_widget_property` with the "Slot." prefix:

```python
# ✅ CORRECT: Use set_widget_property with Slot. prefix
set_widget_property(
    widget_name="MyWidget",
    component_name="Background",
    property_name="Slot.HorizontalAlignment",
    property_value="HAlign_Fill"  # Use UE enum format!
)

# ❌ WRONG: This function doesn't exist as MCP tool
set_widget_slot_properties(  # This will fail!
    widget_name="MyWidget",
    widget_component_name="Background",
    slot_properties={"HorizontalAlignment": "Fill"}
)
```

### Unreal Engine Enum Values for Alignments
**CRITICAL**: Alignment properties require specific Unreal Engine enum values:

```python
# ✅ CORRECT enum values for HorizontalAlignment:
"HAlign_Fill"    # Fill entire horizontal space
"HAlign_Left"    # Align to left
"HAlign_Center"  # Center horizontally
"HAlign_Right"   # Align to right

# ✅ CORRECT enum values for VerticalAlignment:
"VAlign_Fill"    # Fill entire vertical space
"VAlign_Top"     # Align to top
"VAlign_Center"  # Center vertically
"VAlign_Bottom"  # Align to bottom

# ❌ WRONG: These will fail with "Invalid enum value" error:
"Fill"           # Missing prefix!
"Center"         # Missing prefix!
"Top"            # Missing prefix!
```

### SizeRule Property Special Case
```python
# ✅ CORRECT: SizeRule accepts string value without enum prefix
set_widget_property(
    widget_name="MyWidget",
    component_name="Background",
    property_name="Slot.Size.SizeRule",
    property_value="Fill"  # This is correct for SizeRule
)

# Other SizeRule values: "Auto", "Fill"
```

### Quick Reference: Common Slot Properties
```python
# For Overlay slots (backgrounds in Canvas panels):
set_widget_property(widget_name, component_name, "Slot.HorizontalAlignment", "HAlign_Fill")
set_widget_property(widget_name, component_name, "Slot.VerticalAlignment", "VAlign_Fill")

# For ScrollBox/VBox/HBox slots (backgrounds in box containers):
set_widget_property(widget_name, component_name, "Slot.HorizontalAlignment", "HAlign_Fill")
set_widget_property(widget_name, component_name, "Slot.VerticalAlignment", "VAlign_Fill")
set_widget_property(widget_name, component_name, "Slot.Size.SizeRule", "Fill")  # CRITICAL for ScrollBox!
```

## MCP Tool Invocation Updates (Dec 2025)

All widget automation now routes through `manage_umg_widget`. Earlier revisions of this guide referenced helper aliases such as `set_widget_property` or `add_overlay`; treat those as shorthand for the `action`-based API shown below.

```python
# General pattern
manage_umg_widget(
    action="set_property",  # or list_components, add_component, etc.
    widget_name="/Game/Blueprints/WBP_MyWidget",
    component_name="TitleText",
    property_name="ColorAndOpacity",
    property_value={"R": 0.1, "G": 1.0, "B": 1.0, "A": 1.0}
)
```

| Legacy helper | Updated call | Notes |
|---|---|---|
| `set_widget_property` | `manage_umg_widget(action="set_property", ...)` | Supports nested names like `Slot.LayoutData.Anchors.Minimum`. |
| `add_overlay`, `add_image`, `add_button`, etc. | `manage_umg_widget(action="add_component", component_type="Overlay"/"Image"/"Button", parent_name=...)` | Provide `component_name` when you need deterministic names. |
| `list_widget_components` | `manage_umg_widget(action="list_components", widget_name=...)` | Returns the hierarchy plus slot info. |
| `get_widget_properties`, `get_widget_property` | `manage_umg_widget(action="get_component_properties"|"get_property", ...)` | Pass `include_inherited=True` to see defaults. |
| `bind_widget_events` | `manage_umg_widget(action="bind_events", component_name=..., input_events={...}, input_mappings={...})` | Backend currently insists on `input_mappings`; see *Known MCP Limitations*. |
| `search_widget_types` | `manage_umg_widget(action="search_types", ...)` | Returns the MCP-backed palette via `get_available_widget_types`. |

When you see helper-style snippets later in this file, translate them using the table above before calling the MCP tool.

## Widget Structure Best Practices

### DO's ✅

#### **Work with Existing Components First**
- **DO** discover existing widget structure using `search_items()` → `list_widget_components()` first
- **DO** style existing components before adding new ones
- **DO** use `set_widget_property()` to modify colors, fonts, visibility of existing elements
- **DO** understand the current widget hierarchy before making changes
- **DO** check if functionality already exists before duplicating it

#### **Component Addition Guidelines**
- **DO** add new components ONLY when necessary for functionality or essential styling
- **DO** justify each new component - ask "Does this serve a specific purpose?"
- **DO** prefer modifying existing Text Blocks over creating new ones
- **DO** prefer using existing layout containers over creating new panels
- **DO** add backgrounds only when none exist and they're essential for the theme

#### **Proper Background Implementation**
- **DO** use Overlay widgets for layered backgrounds in Canvas panels
- **DO** add background images as direct children to ScrollBox/Border containers when supported
- **DO** set image sizes to "Fill" when creating full container backgrounds
- **DO** use proper parent-child relationships (backgrounds inside containers, not root)
- **DO** set proper Z-ordering for layering (-10 for backgrounds, -5 for borders, 0+ for content)

#### **Container Organization**
- **DO** place background elements according to container type:
  ```
  SizeBox_Root (recommended for Canvas-based layouts)
  └── CanvasPanel_Main
      └── Overlay_Background (required for Canvas)
          ├── MainBackground_Image (alignment: Fill, Z: -10)
          └── Content (Z: 0+)
  
  ScrollBox_ContentA (supports direct background children)
  ├── SectionBackground_Image (size: Fill, Z: -5)
  └── Content...
  
  Border_ContentB (use native background properties)
  └── Content (set Border.BrushColor directly)
  ```

#### **Size and Positioning**
- **DO** set background images to Fill their parent containers when possible
- **DO** use "Fill" alignment for Canvas+Overlay backgrounds when inside Size Box
- **DO** use "Top Left" alignment + manual sizing when Fill doesn't work properly  
- **DO** use anchors for responsive positioning
- **DO** set proper margins and padding for content separation

### DON'Ts ❌

#### **Unnecessary Component Creation**
- **DON'T** add phantom text blocks, progress bars, or info panels without specific functionality requirements
- **DON'T** create duplicate components when existing ones can be restyled
- **DON'T** add decorative elements just to implement the style - work with existing structure
- **DON'T** assume you need new components - always check existing widget structure first
- **DON'T** add status displays, resource meters, or informational panels unless specifically requested
- **DON'T** create multiple text blocks when one can be restyled appropriately

#### **Avoid Fake Visual Effects**
- **DON'T** create fake "glow" effects using multiple colored image widgets
- **DON'T** add "accent lines" or "border effects" using multiple image components
- **DON'T** try to simulate lighting effects with layered transparent images
- **DON'T** create "neon" effects by stacking colored widgets
- **DON'T** add unnecessary visual flourishes - focus on clean, functional styling
- **DON'T** use multiple images to create effects that should be handled by native widget properties

#### **Background Implementation Mistakes**
- **DON'T** place background images directly in the root CanvasPanel
- **DON'T** create backgrounds without proper containment
- **DON'T** use fixed pixel sizes for backgrounds that should fill containers
- **DON'T** forget to set Z-ordering for proper layering

#### **Structure Violations**
- **DON'T** add decorative elements as siblings to functional content
- **DON'T** create floating images without proper parent containers
- **DON'T** ignore existing widget hierarchy when adding new elements
- **DON'T** add backgrounds directly to Canvas panels (use Overlay wrapper)
- **DON'T** use Overlays for containers that support direct background children

#### **Performance Issues**
- **DON'T** create unnecessary overlapping transparent images
- **DON'T** use multiple background layers when one would suffice
- **DON'T** forget to set appropriate visibility states
- **DON'T** create unnecessary nested containers

#### **Fake Visual Effects - CRITICAL AVOIDANCE**
- **DON'T** create fake "glow" effects using multiple colored Image widgets - these look unprofessional
- **DON'T** add multiple Image widgets to simulate borders, outlines, or accent lines
- **DON'T** layer colored Images to create visual effects - use proper UMG styling instead
- **DON'T** add "accent line" images, "border" images, or "glow" images for decoration
- **DON'T** create multiple overlapping Images for visual flair - this creates visual confusion
- **DON'T** use Image widgets for effects that should be achieved through proper styling properties

## AI Assistant Workflow for Widget Restyling

### Required Workflow Steps
1. **DISCOVER**: Use `search_items()` to find the target widget
2. **ANALYZE**: Use `list_widget_components()` to understand existing structure
3. **PLAN**: Identify which existing components can be restyled vs. what's actually missing
4. **RESTYLE**: Use `set_widget_property()` to modify existing components first
5. **ADD MINIMALLY**: Only add new components if essential for functionality or absolutely required styling
6. **AVOID EFFECTS**: Focus on clean styling - no fake glows, borders, or decorative elements
7. **COMPILE**: Always use `compile_blueprint(widget_name)` after completing styling changes

### Example Restyling Approach
```python
# Step 1: Find and analyze existing structure
widgets = search_items(search_term="Inventory", asset_type="Widget")
components = list_widget_components("WBP_Inventory")

# Step 2: Apply user-specified colors/styling to existing elements
set_widget_property("WBP_Inventory", "Title_Text", "ColorAndOpacity", user_specified_color)
set_widget_property("WBP_Inventory", "ButtonClose", "ColorAndOpacity", user_theme_color)

# Step 3: Add background ONLY if none exists and essential for theme (rare)
if no_background_exists_and_essential:
    add_overlay(...)  # Background container
    add_image(...)    # Single background image - NO decorative effects

# Step 4: NEVER add fake effects, glows, borders, or accent lines

# Step 5: ALWAYS compile after styling changes
compile_blueprint("WBP_Inventory")
```

### Questions to Ask Before Adding Components
- "Is there an existing component I can restyle instead?"
- "Does this serve a functional purpose or is it purely decorative?"
- "Can I achieve this visual effect by modifying existing properties?"
- "Is this component specifically requested by the user?"
- "Am I trying to create fake effects that should be avoided?"
- "Will this component actually improve functionality or just add visual clutter?"

## Clean Styling Principles

### **Focus on Essentials Only**
- **PRIORITY 1**: Modify existing component colors, fonts, and basic properties
- **PRIORITY 2**: Add background ONLY if none exists and specifically needed
- **PRIORITY 3**: Ensure functionality is maintained
- **AVOID**: Decorative elements, fake effects, visual flourishes

### **Professional UI Guidelines**
- **CLEAN**: Use native widget properties for styling
- **SIMPLE**: One background per container maximum
- **FUNCTIONAL**: Every component should serve a purpose
- **ACCESSIBLE**: High contrast, readable text
- **MAINTAINABLE**: Avoid complex layering and multiple decorative elements

### **User-Driven Styling Strategy**
1. **Text Colors**: Apply user-specified colors to existing text components
2. **Button Colors**: Style existing buttons with user-requested theme colors
3. **Background**: Add single background if essential and requested
4. **COMPILE**: Always compile the blueprint after styling changes
5. **STOP**: Do not add borders, glows, accents, or decorative elements

## Available Tools

### Background and Layout Tools
- **add_overlay()** - Creates proper Overlay widgets for layering (essential for Canvas backgrounds)
- **add_canvas_panel()** - Creates Canvas panels for absolute positioning  
- **add_image()** - Adds image widgets for backgrounds and decorations
- **set_widget_transform()** - Sets position, size, anchors properly
- **set_widget_z_order()** - Controls layering order

### Styling Tools
- **set_widget_property()** - Sets individual widget properties
- **set_widget_visibility()** - Controls widget visibility states
- **compile_blueprint()** - REQUIRED: Compiles widget after styling changes
- **add_child_to_panel()** - Properly parents widgets to containers

## Professional Visual Effects - DO's and DON'Ts

### ✅ **PROPER Visual Effects**
- **DO** use `set_widget_property()` to set Border.BrushColor for actual borders
- **DO** use ColorAndOpacity properties to create color themes and accents
- **DO** use native UMG styling properties (Padding, Margin, BackgroundColor)
- **DO** rely on Brush properties and Slate styling for professional effects
- **DO** use existing widget styling capabilities before adding new components
- **DO** achieve themed appearance through color schemes, not fake image effects

### ❌ **AVOID Fake Effects**
- **NEVER** create multiple Image widgets to simulate glow, borders, or accent lines
- **NEVER** layer colored Images for visual effects - this looks amateur
- **NEVER** add "decoration" Images that serve no functional purpose
- **NEVER** use Image widgets when styling properties would achieve the same result

### 🎨 **Professional Theming Done Right**
```python
# ✅ CORRECT: Use proper color theming
set_widget_property("Widget", "TextBlock", "ColorAndOpacity", {"R": 0.1, "G": 1, "B": 1, "A": 1})  # Cyan text
set_widget_property("Widget", "Background", "ColorAndOpacity", {"R": 0.05, "G": 0.1, "B": 0.25, "A": 0.9})  # Dark blue bg

# ❌ WRONG: Don't create fake accent lines
# add_image(widget_name="Widget", image_name="AccentLine", color_tint=[0, 1, 1, 0.8])  # DON'T DO THIS
```

## Container-Specific Background Guidelines

### CanvasPanel
- **❌ DON'T**: Add images directly as children
- **✅ DO**: Use `add_overlay()` first, then add images to overlay
- **📋 PROCEDURE**: Canvas Panel background implementation follows this sequence:

#### **Canvas Panel Background Implementation**

**Step 1**: Create Overlay as child of Canvas Panel
```python
add_overlay(widget_name="MyWidget", overlay_name="Overlay_MainBackground", parent_name="CanvasPanel_Body")
```

**Step 2**: Size Overlay to fill entire Canvas Panel
```python
set_widget_transform(
    widget_name="MyWidget", 
    component_name="Overlay_MainBackground",
    anchor_min=[0, 0], 
    anchor_max=[1, 1], 
    position=[0, 0], 
    size=[0, 0]
)
```

**Step 3**: Set Overlay Z-order behind content
```python
set_widget_z_order(widget_name="MyWidget", component_name="Overlay_MainBackground", z_order=-15)
```

**Step 4**: Add background image to Overlay
```python
add_image(widget_name="MyWidget", image_name="MainBackground", parent_name="Overlay_MainBackground", ...)
```

**Step 5**: Set background image transform with large size
```python
set_widget_transform(
    widget_name="MyWidget",
    component_name="MainBackground",
    anchor_min=[0, 0], 
    anchor_max=[1, 1], 
    position=[0, 0], 
    size=[10000, 10000]  # Large size ensures complete fill
)
```

**Step 6**: **CRITICAL** - Configure Canvas slot `LayoutData` so the image actually fills the panel
```python
# CanvasPanelSlot exposes LayoutData.* fields instead of Horizontal/Vertical enums
manage_umg_widget(
    action="set_property",
    widget_name="MyWidget",
    component_name="MainBackground",
    property_name="Slot.LayoutData.Anchors.Minimum",
    property_value={"x": 0.0, "y": 0.0}
)
manage_umg_widget(
    action="set_property",
    widget_name="MyWidget",
    component_name="MainBackground",
    property_name="Slot.LayoutData.Anchors.Maximum",
    property_value={"x": 1.0, "y": 1.0}
)
manage_umg_widget(
    action="set_property",
    widget_name="MyWidget",
    component_name="MainBackground",
    property_name="Slot.LayoutData.Alignment",
    property_value={"x": 0.0, "y": 0.0}
)
manage_umg_widget(
    action="set_property",
    widget_name="MyWidget",
    component_name="MainBackground",
    property_name="Slot.LayoutData.Offsets",
    property_value={"left": 0, "top": 0, "right": 0, "bottom": 0}
)
```
These settings mirror the old `HAlign_Fill` / `VAlign_Fill` workflow. For Overlay/ScrollBox/VBox slots you can still set `Slot.HorizontalAlignment` / `Slot.VerticalAlignment` directly, but Canvas children require the `LayoutData` field names shown above.

**Step 7**: Set background image Z-order
```python
set_widget_z_order(widget_name="MyWidget", component_name="MainBackground", z_order=-10)
```

- **Note**: Canvas panels require Overlay wrapper for proper background layering
- **CRITICAL**: Always set slot properties (Fill alignment) for background images in overlays
- **CRITICAL**: Use "HAlign_Fill" and "VAlign_Fill" enum values, NOT "Fill"

### ScrollBox, VerticalBox, HorizontalBox
- **✅ DO**: Add images directly as children with proper Fill configuration
- **❌ DON'T**: Create unnecessary overlay wrappers
- **❌ DON'T**: Use fixed pixel sizes for backgrounds that should fill
- **CRITICAL**: Always set SizeRule, HorizontalAlignment, and VerticalAlignment for proper filling

#### **Complete ScrollBox Background Implementation**

**Step 1**: Add background image as direct child
```python
add_image(widget_name="MyWidget", image_name="ScrollBackground", parent_name="MyScrollBox")
```

**Step 2**: Set transform with large size for guaranteed coverage
```python
set_widget_transform(
    widget_name="MyWidget",
    component_name="ScrollBackground",
    anchor_min=[0, 0],
    anchor_max=[1, 1],
    position=[0, 0],
    size=[10000, 10000]  # Large size ensures complete fill
)
```

**Step 3**: Set slot properties for Fill behavior
```python
# Use set_widget_property with "Slot." prefix to configure slot properties
# CRITICAL: Use exact enum values for Unreal Engine
set_widget_property(
    widget_name="MyWidget",
    component_name="ScrollBackground",
    property_name="Slot.HorizontalAlignment",
    property_value="HAlign_Fill"  # NOT "Fill" - use UE enum format!
)
set_widget_property(
    widget_name="MyWidget",
    component_name="ScrollBackground",
    property_name="Slot.VerticalAlignment",
    property_value="VAlign_Fill"  # NOT "Fill" - use UE enum format!
)
set_widget_property(
    widget_name="MyWidget",
    component_name="ScrollBackground",
    property_name="Slot.Size.SizeRule",
    property_value="Fill"  # CRITICAL: Override default "Auto"
)
```

**Step 4**: Set proper Z-order behind content
```python
set_widget_z_order(widget_name="MyWidget", component_name="ScrollBackground", z_order=-6)
```

**Step 5**: Apply theme color
```python
set_widget_property(
    widget_name="MyWidget",
    component_name="ScrollBackground",
    property_name="ColorAndOpacity",
    property_value={"R": 0.02, "G": 0.02, "B": 0.1, "A": 0.8}
)
```

- **Reason**: These containers natively support child layout
- **⚠️ WARNING**: Never use fixed sizes like [400, 300] - always use Fill configuration

### Border
- **✅ DO**: Use `set_widget_property()` to set BrushColor/Background
- **❌ DON'T**: Add image children
- **Example**:
  ```python
  set_widget_property(widget_name="MyWidget", component_name="MyBorder", 
                      property_name="BrushColor", property_value=[0.2, 0.2, 0.2, 0.9])
  ```
- **Reason**: Border has native background properties

## SVG Background Assets - Usage Guidelines 📐

**VibeUE MCP Server includes SVG to PNG conversion capabilities for creating custom background textures.**

### ✅ **APPROPRIATE SVG Usage**
- **Complex UI Patterns**: Circuit boards, futuristic panels, technical diagrams
- **Custom Geometric Backgrounds**: When existing textures don't match required aesthetic  
- **Theme-Specific Elements**: Logo integration, brand-specific graphics
- **Scalable UI Components**: Icons, symbols that need crisp rendering at multiple sizes

### ⚠️ **MODERATION GUIDELINES - Use SVGs Judiciously**
- **DEFAULT FIRST**: Always check existing project textures before creating new SVG assets
- **SIMPLE OVER COMPLEX**: Prefer simple solid colors and basic gradients when possible
- **AVOID OVERENGINEERING**: Don't create SVG for effects achievable with UMG properties
- **PERFORMANCE CONSCIOUS**: Complex SVGs with many filters/effects can impact conversion time

### 🚫 **AVOID SVG When**
- **Simple solid colors** can achieve the desired effect
- **Basic gradients** using UMG properties would suffice  
- **Existing project textures** already provide suitable backgrounds
- **Users request minimal/clean designs** without complex graphics
- **Project emphasizes performance** over visual complexity

### SVG Asset Integration Workflow

#### **Step 1: Evaluate Necessity**
```python
# Ask these questions before creating SVG:
# - Can I achieve this with existing textures in the project?
# - Would a simple color gradient work instead?
# - Does this complexity match the user's request?
# - Is this SVG essential for the requested theme?
```

#### **Step 2: Create and Import SVG** (When Justified)
```python
# IMPORTANT: Use temporary directory for processing before Unreal import
# This keeps the Content directory clean and organized

# Convert SVG to PNG in temp directory first
convert_svg_to_png(
    svg_path="C:/path/to/custom_background.svg",
    output_path=None,  # Auto-generates temp file path
    size=[2048, 2048],  # High quality for UI backgrounds
    background="#00000000"  # Transparent background for UI elements
)

# Then import the processed PNG to Unreal
import_texture_asset(
    file_path="C:/temp/converted/custom_background.png",  # Use temp file path
    destination_path="/Game/Textures/Generated",
    texture_name="CustomBackground",
    validate_format=True,
    auto_optimize=True
)
```

#### **🗂️ Temporary Directory Best Practices**
- **ALWAYS** use temp directories for SVG conversion before Unreal import
- **NEVER** convert SVGs directly into Content directory 
- **CLEAN UP** temp files after successful import to conserve disk space
- **ORGANIZE** temp processing in dedicated folders (e.g., `C:/temp/unreal_assets/`)
- **SEPARATE** different asset types in temp subfolders (textures, audio, etc.)

#### **Step 3: Apply Using Standard Background Process**
```python
# Use converted texture by setting widget properties directly
set_widget_property(
    widget_name="WBP_MyWidget",
    component_name="MainBackground",
    property_name="Brush",
    property_value={
        "ResourcePath": "/Game/Textures/Generated/CustomBackground",
        "DrawAs": "Image"  # or "Border", "Box" for tiling
    }
)
```

### SVG Creation Best Practices

#### **✅ SVG Content Guidelines**
- **Vector Elements**: Use paths, circles, rectangles for crisp scaling
- **Limited Color Palette**: 3-5 colors maximum for clean aesthetics
- **Simple Filters**: Basic shadows, glows - avoid complex multi-layer effects
- **Readable Text**: If including text, use web-safe fonts
- **Consistent Style**: Match project's overall visual language

#### **⚠️ Performance Considerations**
- **Reasonable Complexity**: Aim for < 200 SVG elements
- **Optimize Paths**: Combine similar elements, reduce anchor points
- **Standard Size**: Target 1024x768 or 2048x1536 for UI backgrounds
- **Test Conversion**: Verify PNG output quality matches expectations

#### **❌ Avoid in SVG**
- **Raster Images**: Embedded JPG/PNG within SVG (defeats vector purpose)
- **Complex Animations**: SVG animations don't translate to static PNG
- **External References**: External stylesheets, fonts, or linked resources
- **Excessive Filters**: Multiple blur/shadow effects that impact conversion

### 🎨 Grayscale Gradient Strategy - Color Efficiency

**PREFERRED APPROACH: Create grayscale gradients and tint them with Unreal brush properties for maximum flexibility.**

#### **✅ Grayscale Gradient Benefits**
- **Single Asset, Multiple Colors**: One grayscale texture can serve many color themes
- **Runtime Flexibility**: Change colors instantly without new texture imports
- **Performance Optimized**: Smaller file sizes, reduced texture memory
- **Theme Consistency**: Ensures gradient shapes remain consistent across color schemes
- **Easy Maintenance**: Update colors via properties instead of recreating assets

#### **🏗️ Grayscale Gradient Implementation Workflow**

##### **Step 1: Create Grayscale Gradient Asset**
```python
# Create SVG with grayscale gradient (when complex gradients needed)
# OR use existing grayscale gradient textures from project assets
convert_svg_to_png(
    svg_path="C:/temp/gradients/grayscale_radial.svg",  # Grayscale values only
    output_path="C:/temp/converted/grayscale_gradient.png",
    size=[1024, 1024],
    background="#00000000"  # Transparent background
)

# Import grayscale gradient
import_texture_asset(
    file_path="C:/temp/converted/grayscale_gradient.png",
    destination_path="/Game/Textures/Gradients",
    texture_name="GradientRadial_Grayscale"
)
```

##### **Step 2: Apply Gradient with Theme Color Tinting**
```python
# Apply grayscale texture as background using widget properties
set_widget_property(
    widget_name="WBP_MyWidget",
    component_name="MainBackground",
    property_name="Brush",
    property_value={
        "ResourcePath": "/Game/Textures/Gradients/GradientRadial_Grayscale",
        "DrawAs": "Image"
    }
)

# Tint with theme color using brush properties
set_widget_property(
    widget_name="WBP_MyWidget",
    component_name="MainBackground", 
    property_name="ColorAndOpacity",
    property_value=[0.2, 0.8, 1.0, 0.7]  # Cyan tint with transparency
)
```

##### **Step 3: Dynamic Color Switching (Same Gradient, Different Themes)**
```python
# Neon theme - bright cyan
set_widget_property(widget_name, component_name, "ColorAndOpacity", [0.0, 1.0, 1.0, 0.8])

# Corporate theme - blue
set_widget_property(widget_name, component_name, "ColorAndOpacity", [0.2, 0.4, 1.0, 0.9])

# Dark mode - purple
set_widget_property(widget_name, component_name, "ColorAndOpacity", [0.6, 0.2, 1.0, 0.6])

# Military theme - green
set_widget_property(widget_name, component_name, "ColorAndOpacity", [0.2, 1.0, 0.3, 0.7])
```

#### **💡 Grayscale Design Guidelines**

##### **✅ Effective Grayscale Patterns**
- **Radial Gradients**: Center-to-edge fades for UI panels and buttons
- **Linear Gradients**: Top-to-bottom or left-to-right for headers and sections
- **Vignette Effects**: Dark edges with bright centers for focus areas
- **Noise Textures**: Subtle grayscale noise for texture variation
- **Geometric Patterns**: Grayscale shapes that can be tinted to any color

##### **🎯 Color Tinting Best Practices**
- **Alpha Channel**: Use alpha values (0.6-0.9) for background transparency
- **Saturation**: High saturation ([0.0, 1.0, 1.0]) for vibrant themes, low for subtle
- **Contrast**: Ensure sufficient contrast between tinted backgrounds and text
- **Consistency**: Use same tinting approach across all UI elements for cohesive theme

##### **📋 Grayscale vs Colored Asset Decision Matrix**
```
Gradient Complexity Assessment:
├── Simple linear/radial gradient?
│   ├── YES → Create grayscale asset + tint with brush color
│   └── NO → Evaluate if complex SVG is justified
├── Multiple color transitions needed?
│   ├── YES → Consider grayscale base + multiple tinting layers
│   └── NO → Single grayscale asset with brush tinting
└── User requires specific brand colors?
    ├── YES → Grayscale base allows easy brand color application
    └── NO → Standard grayscale approach with theme tinting
```

#### **⚠️ When to Use Colored Assets Instead**
- **Complex Multi-Color Gradients**: When gradient requires 3+ distinct colors
- **Brand-Specific Graphics**: Logos or graphics with mandatory specific colors
- **Photorealistic Elements**: When realistic textures require specific color relationships
- **Performance Critical**: When brush tinting adds unnecessary shader complexity (rare)

### Decision Tree for Background Implementation

```
User Requests Background Styling
├── Check existing project textures first
│   ├── Found suitable texture? → Use existing asset + brush tinting if needed
│   └── No suitable texture? → Evaluate complexity needs
│       ├── Simple solid color? → Use UMG ColorAndOpacity properties
│       ├── Simple gradient needed?
│       │   ├── Existing grayscale gradient available? → Use + brush color tinting
│       │   ├── Need basic linear/radial gradient? → Create grayscale + brush tinting
│       │   └── Complex multi-color gradient? → Evaluate if SVG justified
│       ├── Basic pattern available? → Use existing + brush tinting
│       └── Complex custom graphics needed?
│           ├── User specifically requested elaborate visuals? → Create SVG (via temp)
│           ├── Theme requires custom branding/technical elements? → Create SVG (via temp)
│           ├── Simple gradient but themed? → Create grayscale + brush tinting
│           └── User wants clean/minimal design? → Avoid SVG, use grayscale + tinting
```

### 📁 Asset Processing Workflow - Temp Directory Management

#### **✅ CORRECT Asset Processing Pipeline**
```python
# Step 1: Process in temp directory
temp_asset_path = convert_svg_to_png(
    svg_path="C:/source/my_background.svg",
    output_path="C:/temp/unreal_processing/textures/my_background.png",
    size=[2048, 2048]
)

# Step 2: Import processed asset to Unreal
final_asset = import_texture_asset(
    file_path=temp_asset_path,  # Use temp processed file
    destination_path="/Game/Textures/UI",  # Clean organized Content path
    texture_name="ProcessedBackground"
)

# Step 3: Clean up temp files (optional but recommended)
# Temp files can be deleted after successful import
```

#### **❌ INCORRECT: Direct Processing in Content Directory**
```python
# DON'T DO THIS - Clutters Content directory with temp files
convert_svg_to_png(
    svg_path="C:/source/my_background.svg", 
    output_path="E:/az-dev-ops/Proteus/Content/Textures/temp_converted.png"  # BAD
)
```

#### **🗂️ Recommended Temp Directory Structure**
```
C:/temp/unreal_processing/
├── textures/          # Converted images, SVG outputs
├── audio/             # Audio file conversions
├── models/            # 3D model processing
└── cache/             # Temporary cache files
```

#### **📋 Temp Directory Best Practices**
- **SEPARATE**: Keep temp processing completely outside of project Content directory
- **ORGANIZE**: Use logical subfolder structure for different asset types  
- **CLEAN**: Remove temp files after successful import (optional but good practice)
- **CONSISTENT**: Use predictable temp paths for better debugging
- **ISOLATED**: Temp processing failures don't affect project directory structure

### SVG Usage Examples

#### **✅ JUSTIFIED: Sci-Fi UI Theme**
```python
# Complex technical background with circuits, HUD elements
# SVG provides: Custom geometric patterns, precise technical aesthetics, scalable details
```

#### **✅ JUSTIFIED: Brand Integration**  
```python
# Company logo background, specific brand graphics
# SVG provides: Vector precision, scalable branding, custom design requirements
```

#### **❌ UNNECESSARY: Simple Color Theme**
```python
# User wants "dark blue background with light borders"
# Better approach: Use solid colors and UMG border properties
set_widget_property(widget_name, component_name, "ColorAndOpacity", [0.1, 0.1, 0.3, 0.9])
```

#### **❌ OVERENGINEERING: Basic Gradient**
```python
# User wants "gradient background" 
# Better approach: Use grayscale gradient + brush color tinting
# 1. Use existing grayscale gradient texture
# 2. Apply theme color via ColorAndOpacity property
set_widget_property(widget_name, component_name, "ColorAndOpacity", [0.2, 0.6, 1.0, 0.8])
```

#### **✅ PREFERRED: Grayscale + Tinting Approach**
```python
# User wants "blue gradient background for sci-fi theme"
# Step 1: Apply grayscale gradient texture
set_widget_property(
    widget_name="WBP_SciFiWidget",
    component_name="BackgroundImage", 
    property_name="Brush",
    property_value={
        "ResourcePath": "/Game/Textures/Gradients/GradientRadial_Grayscale",
        "DrawAs": "Image"
    }
)
    widget_name="WBP_Menu",
    texture_path="/Game/Textures/Gradients/RadialGradient_Grayscale",
    target_component="Background"
)

# Step 2: Tint with sci-fi blue
set_widget_property(
    widget_name="WBP_Menu", 
    component_name="Background",
    property_name="ColorAndOpacity", 
    property_value=[0.1, 0.7, 1.0, 0.8]  # Cyan-blue with transparency
)

# Same gradient, different theme - just change the tint!
# Military green: [0.2, 1.0, 0.3, 0.7]
# Corporate purple: [0.6, 0.2, 1.0, 0.6] 
# Dark mode: [0.3, 0.3, 0.3, 0.9]
```

#### **✅ EFFICIENT: Multi-Theme Support**
```python
# One grayscale asset serves multiple themes efficiently
def apply_theme_colors(widget_name, theme_name):
    if theme_name == "neon":
        color = [0.0, 1.0, 1.0, 0.8]  # Neon cyan
    elif theme_name == "military": 
        color = [0.2, 1.0, 0.3, 0.7]  # Military green
    elif theme_name == "corporate":
        color = [0.2, 0.4, 1.0, 0.9]  # Corporate blue
    
    # Same grayscale texture, different color tinting
    set_widget_property(widget_name, "GradientBackground", "ColorAndOpacity", color)
```

### SVG Workflow Integration with Container Guidelines

**SVG backgrounds follow the same container-specific implementation rules:**
- **Canvas Panel**: Add SVG texture via Overlay → Image structure
- **ScrollBox/VBox/HBox**: Add SVG texture as direct child with Fill properties
- **Border**: Apply SVG texture via BrushColor property if supported

**The SVG conversion process is transparent to the background implementation workflow.**

## Layout Structure Guidelines (Universal)

**NOTE**: The following color examples are for demonstration only. Always use colors specified by the user or project requirements.

### Color Format Examples (For Reference Only)
```json
{
  "example_theme_format": {
    "backgrounds": {
      "main": [R, G, B, A],
      "header": [R, G, B, A],
      "panels": [R, G, B, A]
    },
    "accents": {
      "primary": [R, G, B, A],
      "secondary": [R, G, B, A],
      "success": [R, G, B, A],
      "warning": [R, G, B, A]
    }
  }
}
    }
  },
  "modern": {
    "backgrounds": {
      "main": [0.95, 0.95, 0.95, 1],
      "header": [0.2, 0.2, 0.2, 1],
      "panels": [1, 1, 1, 0.9]
    },
    "accents": {
      "primary": [0.2, 0.6, 1, 1],
      "secondary": [0.5, 0.5, 0.5, 1],
      "success": [0.2, 0.8, 0.2, 1],
      "warning": [1, 0.6, 0, 1]
    }
  }
}
```

### Implementation Steps for Any Theme
1. **Analyze container types** in existing widget
2. **Create appropriate structure** (Overlay for Canvas, direct children for ScrollBox, etc.)
3. **Add background elements** with proper parent-child relationships
4. **Apply theme colors** to backgrounds and accents
5. **Set text colors** to match theme requirements
6. **Configure Z-ordering** for proper layering
7. **Set sizes to Fill** for background elements

## Common Widget Patterns

### Universal Widget Structure with Backgrounds
```
CanvasPanel_Root
├── Overlay_Main (required for Canvas backgrounds)
│   └── MainBackground (Fill, Z: -10)
├── HeaderSection (any container type)
│   └── HeaderBackground (Fill, Z: -8)
├── ContentSection_A (ScrollBox/VBox/HBox)
│   ├── SectionBackground (Fill, Z: -6)
│   ├── SectionGlow (Fill, Z: -5)
│   └── Content_Container (Z: 0)
└── ActionPanel
    └── Buttons (Z: 1)
```

### Button Styling
- Background colors should contrast with theme
- Use accent colors for important actions
- Apply hover states with appropriate theme colors

### Text Styling
- High contrast against backgrounds for readability
- Use theme accent colors for headers and important text
- Ensure accessibility with proper font sizes and contrast ratios

## Validation Checklist

Before completing any styling task:
- [ ] Are background images in proper containers based on container type?
- [ ] Are sizes set to Fill for background elements?
- [ ] Is Z-ordering configured correctly?
- [ ] Do colors match the requested theme?
- [ ] Is the widget hierarchy logical and maintainable?
- [ ] Are there no orphaned elements in the root panel?
- [ ] Have I used the correct background implementation for each container type?
- [ ] **NEW**: Are SizeRule, HorizontalAlignment, and VerticalAlignment set to "Fill" for background images?
- [ ] **NEW**: Are transform sizes set to large values (10000x10000) for guaranteed fill coverage?
- [ ] **NEW**: Are slot properties properly configured for all background images?

## Error Prevention

### Always Check
1. Container type before adding background elements
2. Whether container supports direct children vs needs Overlay wrapper
3. Parent-child relationships for new components
4. Size settings (Fill vs Fixed) for backgrounds
5. Z-order values for proper layering
6. **NEW**: SizeRule, HorizontalAlignment, and VerticalAlignment for background images
7. **NEW**: Transform size set to large values (10000x10000) for guaranteed fill coverage

### Never Assume
- That adding to root panel is correct
- That all containers work the same way for backgrounds
- That default sizes are appropriate for backgrounds
- That Z-ordering doesn't matter
- That any container can hold any widget type without consideration
- **NEW**: That setting anchors alone is sufficient for background filling
- **NEW**: That default SizeRule="Auto" will work for background images

### Common ScrollBox Background Mistakes & Fixes

#### **❌ MISTAKE: Using Fixed Pixel Sizes**
```python
# WRONG - Fixed size won't adapt to container changes
add_child_to_panel(widget_name="Widget", parent_panel_name="ScrollBox", 
                   child_widget_name="Background", size=[400, 300])
```

#### **✅ CORRECT: Fill Configuration**
```python
# RIGHT - Use Fill properties for responsive backgrounds
set_widget_slot_properties(widget_name="Widget", widget_component_name="Background",
                          slot_properties={"SizeRule": "Fill", 
                                         "HorizontalAlignment": "Fill", 
                                         "VerticalAlignment": "Fill"})
set_widget_transform(widget_name="Widget", component_name="Background",
                    anchor_min=[0,0], anchor_max=[1,1], size=[10000,10000])
```

#### **❌ MISTAKE: Missing SizeRule Property**
- **Problem**: Background images use SizeRule="Auto" by default, maintaining natural size
- **Fix**: Always set SizeRule="Fill" for background images that should fill containers

#### **❌ MISTAKE: Small Transform Sizes**
- **Problem**: Using container size [400,300] limits background coverage
- **Fix**: Use large transform size [10000,10000] to guarantee complete fill

## Implementation Standards

### Color Property Format
- **✅ ALWAYS**: Use structured FLinearColor format
- **✅ CORRECT**: `{"R": 0.05, "G": 0.1, "B": 0.25, "A": 0.95}`
- **❌ NEVER**: Use array format `[0.05, 0.1, 0.25, 0.95]`

### Background Image Sizing
- **✅ ALWAYS**: Set background images to fill their containers using transform anchors
- **✅ ALWAYS**: Set SizeRule="Fill" for background images (never use "Auto")
- **✅ ALWAYS**: Set HorizontalAlignment="Fill" and VerticalAlignment="Fill"
- **✅ CORRECT**: `anchor_min=[0, 0], anchor_max=[1, 1]`
- **✅ CORRECT**: Transform size=[10000, 10000] for guaranteed coverage
- **❌ NEVER**: Use fixed pixel sizes for backgrounds
- **❌ NEVER**: Rely on default SizeRule="Auto" for background images

### Styling vs Content
- **✅ MODIFY**: Color, font, size, transform properties
- **❌ NEVER MODIFY**: Text content, widget names, or labels when asked to "style"

### Canvas Panel Background Protocol
- **STEP 1**: Create Overlay in Canvas Panel
- **STEP 2**: Set Overlay anchors to [0,0] → [1,1]
- **STEP 3**: Add background image to Overlay
- **STEP 4**: Set background image anchors to [0,0] → [1,1]
- **STEP 5**: Set proper Z-ordering (Overlay: -15, Background: -10)
- **Investigation Needed**: Determine correct brush/slate property names
- **Text Styling**: May require `Font.ColorAndOpacity` or similar nested properties
- **Image Styling**: May require `Brush.TintColor` or similar brush properties

#### **5. CRITICAL: Overlay Container Sizing Issues**
- **WRONG**: Creating Overlay but not sizing it to fill the parent Canvas Panel
- **RIGHT**: ALWAYS set Overlay anchors to [0,0] → [1,1] to fill Canvas Panel
- **WRONG**: Forgetting to set Overlay Z-order behind content (use -15 or lower)
- **CRITICAL ISSUE DISCOVERED**: MCP OverlaySlot alignment properties were not implemented!

#### **6. CRITICAL FIX: OverlaySlot Alignment Properties**
- **ROOT CAUSE**: `set_widget_slot_properties` function was missing UOverlaySlot support
- **FIXED IN C++**: Added proper OverlaySlot alignment handling in HandleSetWidgetSlotProperties
- **PROPER ALIGNMENT VALUES**:
  - `"HorizontalAlignment": "Fill"` or `"HorizontalAlignment": "HAlign_Fill"`
  - `"VerticalAlignment": "Fill"` or `"VerticalAlignment": "VAlign_Fill"`
- **MANUAL VERIFICATION REQUIRED**: If MCP tools fail, manually set alignment in Details panel

#### **7. CRITICAL STEPS FOR CANVAS PANEL BACKGROUNDS**:
  ```python
  # Step 1: Add Overlay to Canvas Panel
  add_overlay(widget_name="MyWidget", overlay_name="Overlay_MainBackground", parent_name="CanvasPanel_Body")
  
  # Step 2: CRITICAL - Size the Overlay to fill the Canvas Panel
  set_widget_transform(
      widget_name="MyWidget", 
      component_name="Overlay_MainBackground",
      anchor_min=[0, 0], 
      anchor_max=[1, 1], 
      position=[0, 0], 
      size=[0, 0]  # Size is ignored when anchors are set to fill
  )
  
  # Step 3: Set Overlay behind content
  set_widget_z_order(widget_name="MyWidget", component_name="Overlay_MainBackground", z_order=-15)
  
  # Step 4: Add background image to Overlay
  add_image(widget_name="MyWidget", image_name="MainBackground", parent_name="Overlay_MainBackground", ...)
  
  # Step 5: CRITICAL - Set background image to Fill the Overlay (FIXED IN C++)
  set_widget_slot_properties(
      widget_name="MyWidget", 
      widget_component_name="MainBackground", 
      slot_properties={"HorizontalAlignment": "Fill", "VerticalAlignment": "Fill"}
  )
  
  # Step 6: Set background image Z-order
  set_widget_z_order(widget_name="MyWidget", component_name="MainBackground", z_order=-10)
  
  # Step 7: VERIFY MANUALLY - Check Details panel shows "Fill" alignment
  # If alignment is not "Fill", manually set it in Unreal Editor Details panel
  ```

#### **8. Canvas Panel Background Implementation Sequence**
- **MANDATORY ORDER**: Follow this exact sequence for Canvas Panel backgrounds:
  1. ✅ Create Overlay as child of Canvas Panel
  2. ✅ Set Overlay anchors to fill Canvas Panel [0,0] → [1,1]
  3. ✅ Set Overlay Z-order to -15 (behind all content)
  4. ✅ Add background image as child of Overlay
  5. ✅ Set background image alignment to Fill within Overlay (C++ FIX APPLIED)
  6. ✅ Set background image Z-order to -10
  7. ✅ **VERIFY MANUALLY** - Check Details panel alignment shows "Fill"
- **FAILURE TO FOLLOW THIS ORDER RESULTS IN**: Tiny background images that don't fill containers
- **C++ FIX STATUS**: OverlaySlot alignment properties now properly implemented

### ✅ CORRECTED IMPLEMENTATION RULES

#### **Size Setting Protocol**
```python
# For ALL background images - CRITICAL: Always set to Fill
add_child_to_panel(
    widget_name="MyWidget", 
    parent_panel_name="MyContainer",
    child_widget_name="Background",
    size="Fill"  # NOT fixed pixel values
)

# THEN immediately set slot properties to Fill
set_widget_slot_properties(
    widget_name="MyWidget",
    widget_component_name="Background", 
    slot_properties={"SizeRule": "Fill"}
)

# WRONG: Using fixed pixel sizes for backgrounds
size=[400, 300]  # This will NOT fill containers properly
```

#### **Glow Effects - Use Sparingly**
```python
# Glow effects in ScrollBoxes become list items instead of backgrounds
# AVOID: Adding "glow" images to ScrollBox containers
# PREFER: Use border accents or overlay effects instead
# If needed: Add glow effects to Canvas panels with proper positioning
```

#### **Color Setting Protocol**
```python
# CORRECT: Use structured FLinearColor format for TEXT
set_widget_property(
    widget_name="MyWidget",
    component_name="MyTextBlock", 
    property_name="ColorAndOpacity",
    property_type="FLinearColor",
    property_value={"R": 0.2, "G": 1, "B": 1, "A": 1}  # Bright Cyan (visible)
)

# CORRECT: Use structured FLinearColor format for IMAGES  
set_widget_property(
    widget_name="MyWidget",
    component_name="MyImage",
    property_name="ColorAndOpacity",
    property_type="FLinearColor", 
    property_value={"R": 0.05, "G": 0.1, "B": 0.25, "A": 0.95}  # Dark blue
)

# CORRECT: For button backgrounds
set_widget_property(
    widget_name="MyWidget",
    component_name="MyButton",
    property_name="BackgroundColor", 
    property_type="FLinearColor",
    property_value={"R": 0.8, "G": 0.1, "B": 0.1, "A": 0.8}  # Red
)

# WRONG: Using array format
property_value=[0, 1, 1, 1]  # This will NOT work!

# CRITICAL: Text colors need higher RGB values to be visible against dark backgrounds
# Use R: 0.2+ for cyan text instead of R: 0 (which is invisible)
```

#### **Styling vs Content Rule**
- **NEVER** modify these when asked to "style":
  - `Text` property
  - `Content` property
  - Widget names or labels
- **ONLY** modify these for styling:
  - Color properties

## Complete ScrollBox Background Workflow

### **TESTED WORKING EXAMPLE - Copy This Pattern**

Here's a complete, tested example for adding a background to a ScrollBox that you can copy and adapt:

```python
# Step 1: Add background image as direct child of ScrollBox
add_image(
    widget_name="/Game/Blueprints/UI/WBP_MyWidget",
    image_name="MyScrollBackground",
    parent_name="MyScrollBox"
)

# Step 2: Set horizontal alignment to Fill (use HAlign_Fill enum!)
set_widget_property(
    widget_name="/Game/Blueprints/UI/WBP_MyWidget",
    component_name="MyScrollBackground",
    property_name="Slot.HorizontalAlignment",
    property_value="HAlign_Fill"  # CRITICAL: Use UE enum format!
)

# Step 3: Set vertical alignment to Fill (use VAlign_Fill enum!)
set_widget_property(
    widget_name="/Game/Blueprints/UI/WBP_MyWidget",
    component_name="MyScrollBackground",
    property_name="Slot.VerticalAlignment",
    property_value="VAlign_Fill"  # CRITICAL: Use UE enum format!
)

# Step 4: Set SizeRule to Fill (CRITICAL - prevents 32x32 default size!)
set_widget_property(
    widget_name="/Game/Blueprints/UI/WBP_MyWidget",
    component_name="MyScrollBackground",
    property_name="Slot.Size.SizeRule",
    property_value="Fill"  # Note: SizeRule uses "Fill" not "ESizeRule::Fill"
)

# Step 5: Apply background color
set_widget_property(
    widget_name="/Game/Blueprints/UI/WBP_MyWidget",
    component_name="MyScrollBackground",
    property_name="ColorAndOpacity",
    property_value={"R": 0.1, "G": 0.05, "B": 0.3, "A": 0.8}
)

# Step 6: Compile to apply changes
compile_blueprint("/Game/Blueprints/UI/WBP_MyWidget")
```

### **REQUIRED Steps for Proper Background Implementation**

**1. Add Background Image**
```python
add_image(widget_name="MyWidget", image_name="Background", parent_name="ScrollBox")
```

**2. Configure Transform (Large Size for Guaranteed Fill)**
```python
set_widget_transform(
    widget_name="MyWidget",
    component_name="Background",
    anchor_min=[0, 0],
    anchor_max=[1, 1],
    position=[0, 0],
    size=[10000, 10000]  # CRITICAL: Large size ensures complete coverage
)
```

**3. Set Slot Properties (Fill Configuration)**
```python
# Use set_widget_property with "Slot." prefix to configure slot properties
# CRITICAL: Use exact enum values for Unreal Engine
set_widget_property(
    widget_name="MyWidget",
    component_name="Background",
    property_name="Slot.HorizontalAlignment",
    property_value="HAlign_Fill"  # NOT "Fill" - use UE enum format!
)
set_widget_property(
    widget_name="MyWidget",
    component_name="Background",
    property_name="Slot.VerticalAlignment",
    property_value="VAlign_Fill"  # NOT "Fill" - use UE enum format!
)
set_widget_property(
    widget_name="MyWidget",
    component_name="Background",
    property_name="Slot.Size.SizeRule",
    property_value="Fill"  # CRITICAL: Override default "Auto" - SizeRule accepts "Fill" string
)
```

**4. Set Z-Order (Behind Content)**
```python
set_widget_z_order(widget_name="MyWidget", component_name="Background", z_order=-6)
```

**5. Apply Color**
```python
set_widget_property(
    widget_name="MyWidget",
    component_name="Background",
    property_name="ColorAndOpacity",
    property_value={"R": 0.02, "G": 0.02, "B": 0.1, "A": 0.8}
)
```

### **Why This Works**
- **SizeRule="Fill"**: Forces image to fill available space instead of using natural size
- **Large Transform Size**: Ensures coverage even with dynamic container sizing
- **Fill Alignments**: Makes image responsive to container size changes
- **Proper Z-Order**: Keeps background behind scrollable content

### **Common Failure Points**
- ❌ **CRITICAL**: Missing slot properties (HorizontalAlignment, VerticalAlignment, SizeRule="Fill")
- ❌ Using size [0, 0] instead of large size [10000, 10000] for guaranteed coverage
- ❌ Missing SizeRule="Fill" (images stay at natural 32x32 size)
- ❌ Small transform sizes (incomplete coverage)
- ❌ Missing alignment properties (improper positioning)
- ❌ Wrong Z-order (background covers content)
- ❌ **Using "Fill" instead of "HAlign_Fill" or "VAlign_Fill"** (enum value error)
- ❌ **Using non-existent set_widget_slot_properties function** (use set_widget_property instead)

### **Troubleshooting Background Fill Issues**

#### Problem: Background showing as tiny 32x32 image
**Symptoms**: Background appears as small square in corner instead of filling container

**Solution Checklist**:
1. ✅ Set `Slot.Size.SizeRule` to `"Fill"` (for ScrollBox/VBox/HBox)
   ```python
   set_widget_property(widget_name, component_name, "Slot.Size.SizeRule", "Fill")
   ```

2. ✅ Set alignments to Fill using correct enum values:
   ```python
   set_widget_property(widget_name, component_name, "Slot.HorizontalAlignment", "HAlign_Fill")
   set_widget_property(widget_name, component_name, "Slot.VerticalAlignment", "VAlign_Fill")
   ```

3. ✅ Compile the blueprint after changes:
   ```python
   compile_blueprint(widget_name)
   ```

#### Problem: "Invalid enum value 'Fill'" error
**Symptoms**: Error message when trying to set alignment

**Solution**: Use proper Unreal Engine enum format:
```python
# ❌ WRONG:
set_widget_property(widget_name, component_name, "Slot.HorizontalAlignment", "Fill")

# ✅ CORRECT:
set_widget_property(widget_name, component_name, "Slot.HorizontalAlignment", "HAlign_Fill")
set_widget_property(widget_name, component_name, "Slot.VerticalAlignment", "VAlign_Fill")
```

#### Problem: Background not visible at all
**Symptoms**: Background component exists but doesn't show

**Solution Checklist**:
1. ✅ Check Z-order is negative (behind content)
2. ✅ Check ColorAndOpacity alpha channel > 0
3. ✅ Verify component is child of correct parent
4. ✅ Compile the blueprint

  - Font properties
  - Brush properties
  - Size and transform properties

## Known MCP Limitations (Dec 2025)

These issues are tracked in `Plugins/VibeUE/test_prompts/umg/issues.md` and reflected in current MCP builds. Plan for the workarounds below until the backend is updated.

- **`bind_events` demands `input_mappings` but still rejects the request.** Even when both `input_events={"OnClicked": "MyBP_C::HandleClick"}` and `input_mappings={}` are supplied, the tool returns `Missing input_mappings parameter`. For now, bind button delegates manually inside the Blueprint Editor after MCP creates the handler functions.
- **Canvas children expose `Slot.LayoutData.*` instead of `Slot.HorizontalAlignment`.** Attempting to set `Slot.HorizontalAlignment` on a Canvas child raises `Property 'HorizontalAlignment' not found`. Use the LayoutData-based calls outlined in the Canvas Panel workflow earlier in this guide (anchors min/max to 0/1, zero offsets, explicit alignment vector) to achieve Fill behavior.

## 🔍 MANDATORY PRE-IMPLEMENTATION VALIDATION

### Before Adding ANY Canvas Panel Background:
- [ ] **Step 1 Check**: Will I add an Overlay to the Canvas Panel? (Required: YES)
- [ ] **Step 2 Check**: Will I set Overlay anchors to [0,0] → [1,1]? (Required: YES)  
- [ ] **Step 3 Check**: Will I set Overlay Z-order to -15? (Required: YES)
- [ ] **Step 4 Check**: Will I add background image to the Overlay (not Canvas Panel)? (Required: YES)
- [ ] **Step 5 Check**: Will I set background image transform with large size [10000, 10000]? (Required: YES)
- [ ] **Step 6 Check**: Will I set slot properties with Fill alignment and SizeRule? (Required: YES)
- [ ] **Step 7 Check**: Will I set background image Z-order to -10? (Required: YES)
- [ ] **Step 6 Check**: Will I set background image Z-order to -10? (Required: YES)

### Before Setting Any Background Size:
- [ ] **Container Type**: Is this a ScrollBox, VBox, or HBox? (Use direct child with Fill)
- [ ] **Container Type**: Is this a Canvas Panel? (Use Overlay wrapper with 6-step process)
- [ ] **Container Type**: Is this a Border? (Use native BrushColor property)
- [ ] **Size Rule**: Am I setting size to "Fill" for all background images? (Required: YES)
- [ ] **Alignment**: Am I setting alignment to "Fill" for all background images? (Required: YES)

### Before Applying Colors:
- [ ] **User Colors**: Am I using the colors specified by the user, not predefined examples? (Required: YES)
- [ ] **Format Check**: Am I using appropriate color format for the tool? (Required: YES)
- [ ] **Visibility**: For text on dark backgrounds, am I ensuring sufficient contrast? (Required: YES)
- [ ] **Content Safety**: Am I only modifying color/font properties, not Text content? (Required: YES)

### After Completing Styling:
- [ ] **Compilation**: Did I compile the blueprint using `compile_blueprint(widget_name)`? (Required: YES)
- [ ] **Verification**: Are all styling changes applied and visible? (Required: YES)

### Final Implementation Validation:
- [ ] **Hierarchy**: Are backgrounds inside proper containers (not floating)?
- [ ] **Z-Order**: Are backgrounds behind content (negative Z-order)?
- [ ] **Fill Coverage**: Do backgrounds fill their intended containers?
- [ ] **Theme Consistency**: Do colors match the requested theme palette?
- [ ] **No Breakage**: Did I follow container-specific guidelines for each widget type?

**🚨 IF ANY CHECKBOX IS UNCHECKED**: Stop and revise the implementation plan before proceeding.

---

*This guide should be referenced before making any UMG styling changes to ensure consistency and proper implementation.*