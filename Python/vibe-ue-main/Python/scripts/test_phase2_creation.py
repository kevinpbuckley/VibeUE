"""
Test Phase 2 - Create Node from Exact Spawner Key
"""

import sys
import json

sys.path.insert(0, r"E:\az-dev-ops\Proteus\Plugins\VibeUE\Python\vibe-ue-main\Python")

from vibe_ue_server import get_unreal_connection

print("=" * 80)
print("TESTING NODE CREATION FROM EXACT SPAWNER KEY")
print("=" * 80)

unreal = get_unreal_connection()
if not unreal:
    print("❌ Failed to connect to Unreal Engine!")
    sys.exit(1)

print("✅ Connected to Unreal Engine\n")

# Test creating node with exact spawner_key
print("🔧 Creating GetPlayerController using EXACT spawner_key")
print("   Spawner Key: GameplayStatics::GetPlayerController")
print("-" * 80)

payload = {
    "blueprint_name": "/Game/Blueprints/BP_Player2",
    "action": "create",
    "graph_scope": "function",
    "function_name": "CastToMicrosubHUD",
    "node_params": {
        "spawner_key": "GameplayStatics::GetPlayerController"
    },
    "position": [-500, 50]
}

result = unreal.send_command("manage_blueprint_node", payload)

print(f"\nCreation Result:")
print(f"Success: {result.get('success')}")

if result.get('success'):
    print(f"✅ NODE CREATED SUCCESSFULLY!")
    print(f"\nNode Details:")
    print(f"  Node ID: {result.get('node_id')}")
    print(f"  Spawner Key: {result.get('spawner_key')}")
    print(f"  Creation Method: {result.get('creation_method')}")
    
    reflection = result.get('reflection_result', {})
    pin_count = reflection.get('pin_count', 0)
    print(f"  Pin Count: {pin_count}")
    
    if pin_count == 3:
        print(f"\n🎯 PERFECT! Created GameplayStatics variant with 3 pins!")
        print(f"   (WorldContextObject, PlayerIndex, ReturnValue)")
    else:
        print(f"\n⚠️ Warning: Expected 3 pins, got {pin_count}")
else:
    print(f"❌ Creation failed: {result.get('error')}")
    print(f"\nFull result:\n{json.dumps(result, indent=2)}")

# Now list all nodes to verify
print("\n" + "-" * 80)
print("📋 Listing all nodes in function graph to verify:")
print("-" * 80)

list_payload = {
    "blueprint_name": "/Game/Blueprints/BP_Player2",
    "action": "list",
    "graph_scope": "function",
    "function_name": "CastToMicrosubHUD"
}

list_result = unreal.send_command("manage_blueprint_node", list_payload)

if list_result:
    nodes = list_result.get('nodes', [])
    print(f"\nTotal nodes in graph: {len(nodes)}")
    
    # Find our newly created node
    for node in nodes:
        title = node.get('title', '')
        if 'Get Player Controller' in title:
            print(f"\n✓ Found: {title}")
            print(f"  Node Type: {node.get('node_type')}")
            print(f"  Pin Count: {len(node.get('pins', []))}")
            pins = node.get('pins', [])
            if pins:
                print(f"  Pins:")
                for pin in pins:
                    print(f"    - {pin.get('name')} ({pin.get('direction')}): {pin.get('type')}")

print("\n" + "=" * 80)
print("PHASE 2 INTEGRATION TEST COMPLETE!")
print("=" * 80)
