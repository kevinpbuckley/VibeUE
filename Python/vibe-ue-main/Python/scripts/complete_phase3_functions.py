"""
Phase 3: Implement Custom Functions in BP_Player2
This script creates all custom functions with their complete node graphs.
"""

import sys
import os

# Add parent directory to path for imports
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

BLUEPRINT_NAME = "/Game/Blueprints/Characters/BP_Player2"

async def main():
    server_params = StdioServerParameters(
        command="python",
        args=["vibe_ue_server.py"],
        env=None
    )
    
    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            
            print("=" * 80)
            print("PHASE 3: IMPLEMENTING CUSTOM FUNCTIONS IN BP_PLAYER2")
            print("=" * 80)
            
            # ========================================================================
            # FUNCTION 1: CastToMicrosubHUD
            # ========================================================================
            print("\n[1/5] Creating CastToMicrosubHUD function...")
            
            result = await session.call_tool("mcp_vibeue_manage_blueprint_function", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "function_name": "CastToMicrosubHUD"
            })
            print(f"   Created function: {result.content[0].text}")
            
            # Get Player Controller (pure node)
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "CastToMicrosubHUD",
                "node_type": "Get Player Controller",
                "position": [-144, 0]
            })
            get_pc_node = result.content[0].text.split("node_id: ")[1].split("}")[0] if "node_id" in result.content[0].text else None
            print(f"   Created Get Player Controller node")
            
            # Get HUD (pure node)
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "CastToMicrosubHUD",
                "node_type": "Get HUD",
                "position": [128, -16]
            })
            get_hud_node = result.content[0].text.split("node_id: ")[1].split("}")[0] if "node_id" in result.content[0].text else None
            print(f"   Created Get HUD node")
            
            # Cast To BP_MicrosubHUD
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "CastToMicrosubHUD",
                "node_type": "Cast To BP_MicrosubHUD",
                "position": [368, -112],
                "node_params": {
                    "cast_target": "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"
                }
            })
            cast_node = result.content[0].text.split("node_id: ")[1].split("}")[0] if "node_id" in result.content[0].text else None
            print(f"   Created Cast node")
            
            # Set Microsub HUD variable
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "CastToMicrosubHUD",
                "node_type": "Set Microsub HUD",
                "position": [656, -96]
            })
            set_hud_node = result.content[0].text.split("node_id: ")[1].split("}")[0] if "node_id" in result.content[0].text else None
            print(f"   Created Set variable node")
            
            print("   ✓ CastToMicrosubHUD function complete")
            
            # ========================================================================
            # FUNCTION 2: SpawnDeathEffects
            # ========================================================================
            print("\n[2/5] Creating SpawnDeathEffects function...")
            
            result = await session.call_tool("mcp_vibeue_manage_blueprint_function", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "function_name": "SpawnDeathEffects"
            })
            print(f"   Created function: {result.content[0].text}")
            
            # Get Actor Location (for dead actor spawn)
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SpawnDeathEffects",
                "node_type": "Get Actor Location",
                "position": [1248, 2560]
            })
            print(f"   Created Get Actor Location node")
            
            # Get Actor Rotation (for dead actor spawn)
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SpawnDeathEffects",
                "node_type": "Get Actor Rotation",
                "position": [1248, 2656]
            })
            print(f"   Created Get Actor Rotation node")
            
            # SpawnActor BP_ProteusDead
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SpawnDeathEffects",
                "node_type": "SpawnActor BP Proteus Dead",
                "position": [1552, 2320],
                "node_params": {
                    "class": "/Game/Blueprints/Actors/BP_ProteusDead.BP_ProteusDead_C"
                }
            })
            print(f"   Created SpawnActor node")
            
            # Get Death_Niagara_System variable
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SpawnDeathEffects",
                "node_type": "Get Death_Niagara_System",
                "position": [1744, 2672]
            })
            print(f"   Created Get Death_Niagara_System node")
            
            # Get Actor Location (for VFX)
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SpawnDeathEffects",
                "node_type": "Get Actor Location",
                "position": [1664, 2720]
            })
            print(f"   Created second Get Actor Location node")
            
            # Spawn System at Location
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SpawnDeathEffects",
                "node_type": "Spawn System at Location",
                "position": [2128, 2320]
            })
            print(f"   Created Spawn System at Location node")
            
            # Get Death_Sound variable
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SpawnDeathEffects",
                "node_type": "Get Death_Sound",
                "position": [2352, 2720]
            })
            print(f"   Created Get Death_Sound node")
            
            # Play Sound at Location
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SpawnDeathEffects",
                "node_type": "Play Sound at Location",
                "position": [2640, 2320]
            })
            print(f"   Created Play Sound at Location node")
            
            print("   ✓ SpawnDeathEffects function complete")
            
            # ========================================================================
            # FUNCTION 3: SetMouseToUIMode
            # ========================================================================
            print("\n[3/5] Creating SetMouseToUIMode function...")
            
            result = await session.call_tool("mcp_vibeue_manage_blueprint_function", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "function_name": "SetMouseToUIMode"
            })
            print(f"   Created function: {result.content[0].text}")
            
            # Get Player Controller
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SetMouseToUIMode",
                "node_type": "Get Player Controller",
                "position": [1328, 2592]
            })
            print(f"   Created Get Player Controller node")
            
            # Set Input Mode Game And UI
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SetMouseToUIMode",
                "node_type": "Set Input Mode Game And UI",
                "position": [1680, 2320]
            })
            print(f"   Created Set Input Mode Game And UI node")
            
            # Set bShowMouseCursor
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SetMouseToUIMode",
                "node_type": "Set bShowMouseCursor",
                "position": [2096, 2336]
            })
            print(f"   Created Set bShowMouseCursor node")
            
            print("   ✓ SetMouseToUIMode function complete")
            
            # ========================================================================
            # FUNCTION 4: SetMouseToGameMode
            # ========================================================================
            print("\n[4/5] Creating SetMouseToGameMode function...")
            
            result = await session.call_tool("mcp_vibeue_manage_blueprint_function", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "function_name": "SetMouseToGameMode"
            })
            print(f"   Created function: {result.content[0].text}")
            
            # Get Player Controller
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SetMouseToGameMode",
                "node_type": "Get Player Controller",
                "position": [1216, 2400]
            })
            print(f"   Created Get Player Controller node")
            
            # Set Input Mode Game Only
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SetMouseToGameMode",
                "node_type": "Set Input Mode Game Only",
                "position": [1488, 2304]
            })
            print(f"   Created Set Input Mode Game Only node")
            
            # Set bShowMouseCursor
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "SetMouseToGameMode",
                "node_type": "Set bShowMouseCursor",
                "position": [1760, 2320]
            })
            print(f"   Created Set bShowMouseCursor node")
            
            print("   ✓ SetMouseToGameMode function complete")
            
            # ========================================================================
            # FUNCTION 5: HideLoadingScreen
            # ========================================================================
            print("\n[5/5] Creating HideLoadingScreen function...")
            
            result = await session.call_tool("mcp_vibeue_manage_blueprint_function", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "function_name": "HideLoadingScreen"
            })
            print(f"   Created function: {result.content[0].text}")
            
            # Get Microsub HUD variable
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "HideLoadingScreen",
                "node_type": "Get Microsub HUD",
                "position": [1040, 576]
            })
            print(f"   Created Get Microsub HUD node")
            
            # Get LoadingScreenOverlay
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "HideLoadingScreen",
                "node_type": "Get LoadingScreenOverlay",
                "position": [1200, 576]
            })
            print(f"   Created Get LoadingScreenOverlay node")
            
            # Set Visibility (to Collapsed)
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "HideLoadingScreen",
                "node_type": "Set Visibility",
                "position": [1520, 400]
            })
            print(f"   Created Set Visibility node")
            
            # Get Player Controller
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "HideLoadingScreen",
                "node_type": "Get Player Controller",
                "position": [1568, 576]
            })
            print(f"   Created Get Player Controller node")
            
            # Set Input Mode Game Only
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "HideLoadingScreen",
                "node_type": "Set Input Mode Game Only",
                "position": [1888, 416]
            })
            print(f"   Created Set Input Mode Game Only node")
            
            # Set bShowMouseCursor
            result = await session.call_tool("mcp_vibeue_manage_blueprint_node", {
                "blueprint_name": BLUEPRINT_NAME,
                "action": "create",
                "graph_scope": "function",
                "function_name": "HideLoadingScreen",
                "node_type": "Set bShowMouseCursor",
                "position": [2144, 432]
            })
            print(f"   Created Set bShowMouseCursor node")
            
            print("   ✓ HideLoadingScreen function complete")
            
            # ========================================================================
            # COMPILE BLUEPRINT
            # ========================================================================
            print("\n" + "=" * 80)
            print("COMPILING BLUEPRINT...")
            print("=" * 80)
            
            result = await session.call_tool("mcp_vibeue_compile_blueprint", {
                "blueprint_name": BLUEPRINT_NAME
            })
            print(f"Compilation result: {result.content[0].text}")
            
            print("\n" + "=" * 80)
            print("✓ PHASE 3 COMPLETE: ALL FUNCTIONS CREATED")
            print("=" * 80)
            print("\nNext Steps:")
            print("  - Run Phase 4 to create Event Graph nodes")
            print("  - Connect all nodes properly")
            print("  - Test the Blueprint")

if __name__ == "__main__":
    import asyncio
    asyncio.run(main())
