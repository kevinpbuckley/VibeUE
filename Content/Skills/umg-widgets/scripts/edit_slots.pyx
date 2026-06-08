# edit_slots.pyx — Edit slot layout: canvas z-order, box alignment/padding/size, and reparent.
#
# Sample script for the umg-widgets skill. Run via execute_python_code.
# All slot edits go through set_property with string values; reparent_widget moves a widget.
import unreal
ws = unreal.WidgetService

PATH = "/Game/UI/WBP_Menu"   # must already exist with the named widgets

# Canvas child: z-order (render order) + position
ws.set_property(PATH, "PlayButton", "ZOrder", "5")

# Box/Overlay child: alignment + padding + fill rule
ws.set_property(PATH, "HeaderRow", "Horizontal Alignment", "Fill")   # Fill/Left/Center/Right
ws.set_property(PATH, "HeaderRow", "Vertical Alignment", "Top")      # Fill/Top/Center/Bottom
ws.set_property(PATH, "HeaderRow", "Padding", "8")                   # one value, or "(Left=..,Top=..,Right=..,Bottom=..)"
ws.set_property(PATH, "HeaderRow", "Size Rule", "Fill")              # Fill/Automatic
ws.set_property(PATH, "HeaderRow", "Size Value", "1.0")

# Move a widget to a new parent panel (preserves the widget object)
ws.reparent_widget(PATH, "BackgroundImage", "MainContainer")

# Verify
for s in ws.get_widget_snapshot(PATH):
    if s.widget_name in ("PlayButton", "HeaderRow", "BackgroundImage"):
        print(s.widget_name, "parent:", s.parent_widget, "slot:", s.slot_info.slot_type)
unreal.EditorAssetLibrary.save_asset(PATH)
