# apply_font.pyx — Apply a full font configuration to a text widget and verify it.
#
# Sample script for the umg-widgets skill. Run via execute_python_code.
# Uses the dedicated set_font API (preferred over many set_property calls for full edits).
import unreal

WIDGET_PATH = "/Game/Blueprints/TestWidget"
TEXT_WIDGET = "HeaderTitle"   # must be a TextBlock that already exists

font_info = unreal.WidgetFontInfo()
font_info.font_family = "/Engine/EngineFonts/Roboto"  # optional; engine font as a safe default
font_info.typeface = "Bold"
font_info.size = 30
font_info.letter_spacing = 20
font_info.color = "(R=1.0,G=0.9,B=0.25,A=1.0)"        # yellow
font_info.shadow_offset = "(X=2.0,Y=2.0)"
font_info.shadow_color = "(R=0.0,G=0.0,B=0.0,A=0.75)" # mostly opaque black
unreal.WidgetService.set_font(WIDGET_PATH, TEXT_WIDGET, font_info)

unreal.EditorAssetLibrary.save_asset(WIDGET_PATH)

# Read back to verify every field that was set.
applied = unreal.WidgetService.get_font(WIDGET_PATH, TEXT_WIDGET)
print("typeface:", applied.typeface)
print("size:", applied.size)
print("letter_spacing:", applied.letter_spacing)
print("color:", applied.color)
print("shadow_offset:", applied.shadow_offset)
print("shadow_color:", applied.shadow_color)
