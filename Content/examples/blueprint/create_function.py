# Title: Create Blueprint Function
# Category: blueprint
# Tags: blueprint, function, create, add_function_graph
# Description: Create a function in a blueprint and check if it exists first to avoid duplicates

import unreal

# Load the blueprint
bp = unreal.load_asset('/Game/Blueprints/BP_Player')

# ============================================================================
# Check if function already exists (IMPORTANT: prevents duplicates!)
# ============================================================================
function_name = 'TakeDamage'
existing_functions = []

# Get list of existing function graphs
# Note: function_graphs might not be exposed, so we check by trying to find the graph
try:
    graphs = bp.function_graphs if hasattr(bp, 'function_graphs') else []
    existing_functions = [g.get_name() for g in graphs]
except:
    # If we can't get the list, we'll just try to add it
    pass

print(f'Existing functions: {existing_functions}')

# ============================================================================
# Create function only if it doesn't exist
# ============================================================================
if function_name not in existing_functions:
    graph = unreal.BlueprintEditorLibrary.add_function_graph(bp, function_name)
    print(f'Created function: {function_name}')
    print(f'Function graph: {graph.get_name() if graph else "None"}')
else:
    print(f'Function {function_name} already exists, skipping creation')

# ============================================================================
# Compile blueprint
# ============================================================================
unreal.BlueprintEditorLibrary.compile_blueprint(bp)

# Save
unreal.EditorAssetLibrary.save_asset(bp.get_path_name())

print(f'Blueprint {bp.get_name()} saved and compiled')
