"""
Complete all Blueprint Challenge Phase 4 functions.

This script systematically creates and connects all nodes for the remaining 4 functions:
1. SpawnDeathEffects (6 nodes - mostly done, just need connections)
2. SetMouseToUIMode (4 nodes)  
3. SetMouseToGameMode (4 nodes)
4. HideLoadingScreen (3 nodes)

Using the FIXED function call node creation system!
"""

from tools.node_tools import manage_blueprint_node
from tools.blueprint_tools import compile_blueprint

BP_PATH = "/Game/Blueprints/BP_Player2"

print("\n" + "="*80)
print("BLUEPRINT CHALLENGE - PHASE 4: FUNCTION IMPLEMENTATION")
print("="*80)

# ============================================================================
# FUNCTION 1: SpawnDeathEffects - CONNECT EXISTING NODES
# ============================================================================
print("\n[1/4] Completing SpawnDeathEffects function connections...")

spawn_death_connections = [
    # Entry -> SpawnEmitterAtLocation
    {
        "source_node_id": "6808D5D94B9EDFCD3ED873BCA436BF17",
        "source_pin_name": "then",
        "target_node_id": "BDC4AF5F45CCF2AB5260B1A72CFD03FF",
        "target_pin_name": "execute"
    },
    # GetActorLocation -> SpawnEmitterAtLocation.Location
    {
        "source_node_id": "CD9B599F4649EEA5FA877E8DF12133E6",
        "source_pin_name": "ReturnValue",
        "target_node_id": "BDC4AF5F45CCF2AB5260B1A72CFD03FF",
        "target_pin_name": "Location"
    },
    # Get Death_Niagara_System -> SpawnEmitterAtLocation.EmitterTemplate
    {
        "source_node_id": "1E0B7B0A444177B48572DFBA6800ECF5",
        "source_pin_name": "Death_Niagara_System",
        "target_node_id": "BDC4AF5F45CCF2AB5260B1A72CFD03FF",
        "target_pin_name": "EmitterTemplate"
    },
    # SpawnEmitterAtLocation -> SpawnSoundAtLocation
    {
        "source_node_id": "BDC4AF5F45CCF2AB5260B1A72CFD03FF",
        "source_pin_name": "then",
        "target_node_id": "4FB48545445FC4B0ECA6DCA1017CF2CE",
        "target_pin_name": "execute"
    },
    # GetActorLocation -> SpawnSoundAtLocation.Location
    {
        "source_node_id": "CD9B599F4649EEA5FA877E8DF12133E6",
        "source_pin_name": "ReturnValue",
        "target_node_id": "4FB48545445FC4B0ECA6DCA1017CF2CE",
        "target_pin_name": "Location"
    },
    # Get Death_Sound -> SpawnSoundAtLocation.Sound
    {
        "source_node_id": "C06017024AA109ABC9EE3184363148FB",
        "source_pin_name": "Death_Sound",
        "target_node_id": "4FB48545445FC4B0ECA6DCA1017CF2CE",
        "target_pin_name": "Sound"
    },
    # SpawnSoundAtLocation -> DestroyActor
    {
        "source_node_id": "4FB48545445FC4B0ECA6DCA1017CF2CE",
        "source_pin_name": "then",
        "target_node_id": "BD4058C244BCA26325D99F9D84BF03CE",
        "target_pin_name": "execute"
    }
]

result = manage_blueprint_node(
    blueprint_name=BP_PATH,
    action="connect_pins",
    graph_scope="function",
    function_name="SpawnDeathEffects",
    extra={"connections": spawn_death_connections}
)

if result.get("success"):
    succeeded = result.get("succeeded", 0)
    total = result.get("attempted", 0)
    print(f"✅ SpawnDeathEffects: Connected {succeeded}/{total} pins")
else:
    print(f"❌ SpawnDeathEffects connections failed: {result.get('error')}")

# ============================================================================
# FUNCTION 2: SetMouseToUIMode
# ============================================================================
print("\n[2/4] Creating SetMouseToUIMode function nodes...")

# Node 1: GetPlayerController
node1 = manage_blueprint_node(
    blueprint_name=BP_PATH,
    action="create",
    graph_scope="function",
    function_name="SetMouseToUIMode",
    node_type="GetPlayerController",
    node_params={"function_name": "GetPlayerController"},
    position=[100, 100]
)
print(f"   Node 1 (GetPlayerController): {'✅' if node1.get('success') else '❌'}")

# Node 2: SetShowMouseCursor  
node2 = manage_blueprint_node(
    blueprint_name=BP_PATH,
    action="create",
    graph_scope="function",
    function_name="SetMouseToUIMode",
    node_type="SetShowMouseCursor",
    node_params={"function_name": "SetShowMouseCursor"},
    position=[400, 50]
)
print(f"   Node 2 (SetShowMouseCursor): {'✅' if node2.get('success') else '❌'}")

# Node 3: SetInputMode_UIOnlyEx
node3 = manage_blueprint_node(
    blueprint_name=BP_PATH,
    action="create",
    graph_scope="function",
    function_name="SetMouseToUIMode",
    node_type="SetInputMode_UIOnlyEx",
    node_params={"function_name": "SetInputMode_UIOnlyEx"},
    position=[400, 250]
)
print(f"   Node 3 (SetInputMode_UIOnlyEx): {'✅' if node3.get('success') else '❌'}")

# Note: We'll connect these after getting their IDs from describe

# ============================================================================
# FUNCTION 3: SetMouseToGameMode
# ============================================================================
print("\n[3/4] Creating SetMouseToGameMode function nodes...")

# Node 1: GetPlayerController
node1 = manage_blueprint_node(
    blueprint_name=BP_PATH,
    action="create",
    graph_scope="function",
    function_name="SetMouseToGameMode",
    node_type="GetPlayerController",
    node_params={"function_name": "GetPlayerController"},
    position=[100, 100]
)
print(f"   Node 1 (GetPlayerController): {'✅' if node1.get('success') else '❌'}")

# Node 2: SetShowMouseCursor
node2 = manage_blueprint_node(
    blueprint_name=BP_PATH,
    action="create",
    graph_scope="function",
    function_name="SetMouseToGameMode",
    node_type="SetShowMouseCursor",
    node_params={"function_name": "SetShowMouseCursor"},
    position=[400, 50]
)
print(f"   Node 2 (SetShowMouseCursor): {'✅' if node2.get('success') else '❌'}")

# Node 3: SetInputMode_GameOnly
node3 = manage_blueprint_node(
    blueprint_name=BP_PATH,
    action="create",
    graph_scope="function",
    function_name="SetMouseToGameMode",
    node_type="SetInputMode_GameOnly",
    node_params={"function_name": "SetInputMode_GameOnly"},
    position=[400, 250]
)
print(f"   Node 3 (SetInputMode_GameOnly): {'✅' if node3.get('success') else '❌'}")

# ============================================================================
# FUNCTION 4: HideLoadingScreen
# ============================================================================
print("\n[4/4] Creating HideLoadingScreen function nodes...")

# Node 1: Variable Get - Microsub HUD
node1 = manage_blueprint_node(
    blueprint_name=BP_PATH,
    action="create",
    graph_scope="function",
    function_name="HideLoadingScreen",
    node_type="VariableGet",
    node_params={"variable_name": "Microsub HUD"},
    position=[100, 100]
)
print(f"   Node 1 (Get Microsub HUD): {'✅' if node1.get('success') else '❌'}")

# Node 2: HideLoadingScreen (call function on HUD)
node2 = manage_blueprint_node(
    blueprint_name=BP_PATH,
    action="create",
    graph_scope="function",
    function_name="HideLoadingScreen",
    node_type="HideLoadingScreen",
    node_params={"function_name": "HideLoadingScreen"},
    position=[400, 100]
)
print(f"   Node 2 (HideLoadingScreen call): {'✅' if node2.get('success') else '❌'}")

# ============================================================================
# COMPILE BLUEPRINT
# ============================================================================
print("\n[5/5] Compiling Blueprint...")
compile_result = compile_blueprint(BP_PATH)
if compile_result.get("success"):
    print("✅ Blueprint compiled successfully!")
else:
    print(f"❌ Compilation failed: {compile_result.get('error')}")

print("\n" + "="*80)
print("PHASE 4 NODE CREATION COMPLETE")
print("Next: Use describe action to get node IDs and complete connections")
print("="*80)
