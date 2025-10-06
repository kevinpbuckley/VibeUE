"""
Test Phase 2 - Descriptor Discovery Integration
"""

import sys
import os
import json

# Add the Python MCP path
sys.path.insert(0, r"E:\az-dev-ops\Proteus\Plugins\VibeUE\Python\vibe-ue-main\Python")

from vibe_ue_server import get_unreal_connection

print("=" * 80)
print("TESTING DESCRIPTOR-BASED DISCOVERY (Phase 2)")
print("=" * 80)

# Get Unreal connection
unreal = get_unreal_connection()
if not unreal:
    print("❌ Failed to connect to Unreal Engine!")
    sys.exit(1)

print("✅ Connected to Unreal Engine")

# Test 1: Discover GetPlayerController variants with complete metadata
print("\n🔍 Test 1: Discover GetPlayerController with Descriptors")
print("-" * 80)

payload = {
    "blueprint_name": "/Game/Blueprints/BP_Player2",
    "search_term": "GetPlayerController",
    "max_results": 10
}

result = unreal.send_command("discover_nodes_with_descriptors", payload)

print(f"\nRaw Result Keys: {result.keys() if result else 'None'}")

if result and result.get("success"):
    descriptors = result.get("descriptors", [])
    print(f"\n✅ Found {len(descriptors)} variants!\n")
    
    for i, desc in enumerate(descriptors, 1):
        print(f"\n--- Variant {i} ---")
        print(f"Spawner Key: {desc.get('spawner_key', 'N/A')}")
        print(f"Display Name: {desc.get('display_name', 'N/A')}")
        
        func_meta = desc.get('function_metadata', {})
        print(f"Function Class: {func_meta.get('function_class', 'N/A')}")
        print(f"Function Class Path: {func_meta.get('function_class_path', 'N/A')}")
        print(f"Is Static: {func_meta.get('is_static', 'N/A')}")
        print(f"Is Pure: {func_meta.get('is_pure', 'N/A')}")
        print(f"Expected Pins: {desc.get('expected_pin_count', 0)}")
        print(f"Category: {desc.get('category', 'N/A')}")
        print(f"Module: {func_meta.get('module', 'N/A')}")
        
        # Show pins
        pins = desc.get('pins', [])
        if pins:
            print(f"\nPins ({len(pins)}):")
            for pin in pins:
                direction = "→" if pin.get('direction') == 'output' else "←"
                pin_name = pin.get('name', 'Unknown')
                pin_type = pin.get('type', 'Unknown')
                type_path = pin.get('type_path', '')
                print(f"  {direction} {pin_name}: {pin_type}")
                if type_path:
                    print(f"      Path: {type_path}")
else:
    print(f"\n❌ Discovery failed: {result.get('error') if result else 'No response'}")
    if result:
        print(f"Full result: {json.dumps(result, indent=2)}")

print("\n" + "=" * 80)
print("TEST COMPLETE")
print("=" * 80)
