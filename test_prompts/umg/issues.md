# manage_umg_widget issues

1. **`bind_events` requires undocumented `input_mappings` and still errors**  
   - **Repro**: `manage_umg_widget(action="bind_events", widget_name="/Game/Blueprints/WBP_TestWidget", component_name="PlayButton", input_events={"OnClicked":"WBP_TestWidget_C::HandlePlayClicked"})`.  
   - **Result**: `Missing input_mappings parameter`. Supplying `input_mappings={}` still throws the same error.  
   - **Expected**: Per `umg-guide`, binding events should only need the `input_events` map.  
   - **Impact**: Test 6 (event binding) cannot pass; no workaround is documented.

2. **Canvas slot alignments listed in `umg-guide` are rejected**  
   - **Repro**: `manage_umg_widget(action="set_property", component_name="Background", property_name="Slot.HorizontalAlignment", property_value="HAlign_Fill")` where `Background` is a `CanvasPanel` child.  
   - **Result**: `Property 'HorizontalAlignment' not found`.  
   - **Expected**: `umg-guide` explicitly instructs using `Slot.HorizontalAlignment` / `Slot.VerticalAlignment` for Canvas backgrounds.  
   - **Impact**: Test 5 (slot alignment) cannot be satisfied through MCP; we must manually manipulate `LayoutData` instead, which contradicts the documented process.
