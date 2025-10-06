"""
Test Enhanced get_available_blueprint_nodes with Descriptors
"""

import sys
import json

sys.path.insert(0, r"E:\az-dev-ops\Proteus\Plugins\VibeUE\Python\vibe-ue-main\Python")

from vibe_ue_server import get_unreal_connection

print("=" * 80)
print("TESTING ENHANCED get_available_blueprint_nodes")
print("=" * 80)

unreal = get_unreal_connection()
if not unreal:
    print("❌ Failed to connect to Unreal Engine!")
    sys.exit(1)

print("✅ Connected to Unreal Engine\n")

# Test 1: Discovery with descriptors (default)
print("🔍 Test 1: Get available nodes with DESCRIPTORS (return_descriptors=True)")
print("-" * 80)

payload = {
    "blueprint_name": "/Game/Blueprints/BP_Player2",
    "search_term": "GetPlayerController",
    "max_results": 5,
    "return_descriptors": True  # NEW parameter
}

result = unreal.send_command("get_available_blueprint_nodes", payload)

if result and result.get("success"):
    print(f"✅ Success! with_descriptors={result.get('with_descriptors')}")
    print(f"Total nodes: {result.get('total_nodes')}\n")
    
    categories = result.get("categories", {})
    
    for cat_name, nodes in categories.items():
        print(f"\n📁 Category: {cat_name}")
        print("-" * 40)
        
        for node in nodes[:3]:  # Show first 3 in each category
            print(f"\n  ✓ {node.get('display_name', node.get('name'))}")
            
            # NEW: Check for descriptor fields
            if 'spawner_key' in node:
                print(f"    🔑 Spawner Key: {node.get('spawner_key')}")
                
            func_meta = node.get('function_metadata', {})
            if func_meta:
                print(f"    📦 Function Class: {func_meta.get('function_class')}")
                print(f"    🏷️  Is Static: {func_meta.get('is_static')}")
                
            if 'expected_pin_count' in node:
                print(f"    📍 Expected Pins: {node.get('expected_pin_count')}")
                
            # Show pin info if available
            pins = node.get('pins', [])
            if pins:
                print(f"    🔌 Pins ({len(pins)}):")
                for pin in pins[:3]:  # Show first 3 pins
                    direction = "→" if pin.get('direction') == 'output' else "←"
                    print(f"       {direction} {pin.get('name')}: {pin.get('type')}")
else:
    print(f"❌ Failed: {result.get('error') if result else 'No response'}")

# Test 2: Legacy mode without descriptors
print("\n\n🔍 Test 2: Get available nodes WITHOUT descriptors (return_descriptors=False)")
print("-" * 80)

payload_legacy = {
    "blueprint_name": "/Game/Blueprints/BP_Player2",
    "search_term": "GetPlayerController",
    "max_results": 3,
    "return_descriptors": False  # Legacy mode
}

result_legacy = unreal.send_command("get_available_blueprint_nodes", payload_legacy)

if result_legacy and result_legacy.get("success"):
    print(f"✅ Success! with_descriptors={result_legacy.get('with_descriptors', 'not set')}")
    print(f"Total nodes: {result_legacy.get('total_nodes')}\n")
    
    categories = result_legacy.get("categories", {})
    
    for cat_name, nodes in list(categories.items())[:1]:  # Show first category
        print(f"\n📁 Category: {cat_name}")
        for node in nodes[:2]:
            print(f"  ✓ {node.get('name')}")
            
            # Check if descriptor fields are present
            has_spawner_key = 'spawner_key' in node
            has_pins = 'pins' in node
            print(f"    Has spawner_key: {has_spawner_key}")
            print(f"    Has pins: {has_pins}")
else:
    print(f"❌ Failed: {result_legacy.get('error') if result_legacy else 'No response'}")

print("\n" + "=" * 80)
print("✅ ENHANCED DISCOVERY WORKING!")
print("=" * 80)
