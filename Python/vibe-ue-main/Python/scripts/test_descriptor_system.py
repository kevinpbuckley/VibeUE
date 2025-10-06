"""
Test New Node Descriptor System
Demonstrates discovery with complete metadata and creation from exact spawner keys
"""

import json
from mcp import send_command

def test_discover_with_descriptors():
    """Test the new discovery system that returns complete descriptors"""
    
    print("═" * 80)
    print("TEST 1: Discover GetPlayerController with Complete Descriptors")
    print("═" * 80)
    
    # This will call the enhanced discovery
    result = send_command({
        "type": "discover_nodes_with_descriptors",
        "params": {
            "blueprint_name": "/Game/Blueprints/BP_Player2",
            "search_term": "GetPlayerController",
            "max_results": 10
        }
    })
    
    print(f"\nDiscovery Result: {json.dumps(result, indent=2)}")
    
    if result.get("success"):
        descriptors = result.get("descriptors", [])
        print(f"\nFound {len(descriptors)} variants:")
        
        for desc in descriptors:
            spawner_key = desc.get("spawner_key", "N/A")
            function_class = desc.get("function_metadata", {}).get("function_class", "N/A")
            is_static = desc.get("function_metadata", {}).get("is_static", False)
            pin_count = desc.get("expected_pin_count", 0)
            
            print(f"\n  ✓ {spawner_key}")
            print(f"    Class: {function_class}")
            print(f"    Static: {is_static}")
            print(f"    Pins: {pin_count}")
        
        return descriptors
    else:
        print(f"❌ Discovery failed: {result.get('error')}")
        return []

def test_create_from_spawner_key(spawner_key):
    """Test creation using exact spawner key (NO SEARCHING)"""
    
    print("\n" + "═" * 80)
    print(f"TEST 2: Create Node from Spawner Key: {spawner_key}")
    print("═" * 80)
    
    result = send_command({
        "type": "manage_blueprint_node",
        "params": {
            "blueprint_name": "/Game/Blueprints/BP_Player2",
            "action": "create",
            "graph_scope": "function",
            "function_name": "CastToMicrosubHUD",
            "spawner_key": spawner_key,  # ✅ NEW: Exact spawner key
            "position": [-350, 0]
        }
    })
    
    print(f"\nCreation Result: {json.dumps(result, indent=2)}")
    
    if result.get("success"):
        pin_count = result.get("reflection_result", {}).get("pin_count", 0)
        node_id = result.get("node_id", "N/A")
        print(f"\n✅ SUCCESS! Created node with {pin_count} pins (ID: {node_id})")
    else:
        print(f"❌ Creation failed: {result.get('error')}")
    
    return result

def test_legacy_vs_new():
    """Compare legacy creation vs new descriptor-based creation"""
    
    print("\n" + "═" * 80)
    print("TEST 3: Legacy vs New Creation Comparison")
    print("═" * 80)
    
    # Legacy way (fuzzy search - can get wrong variant)
    print("\n--- Legacy Method (fuzzy search) ---")
    legacy_result = send_command({
        "type": "manage_blueprint_node",
        "params": {
            "blueprint_name": "/Game/Blueprints/BP_Player2",
            "action": "create",
            "graph_scope": "function",
            "function_name": "CastToMicrosubHUD",
            "node_type": "Get Player Controller",  # ❌ Ambiguous!
            "position": [-400, 100]
        }
    })
    
    legacy_pins = legacy_result.get("reflection_result", {}).get("pin_count", 0)
    print(f"Legacy creation: {legacy_pins} pins")
    
    # New way (exact spawner key)
    print("\n--- New Method (exact spawner key) ---")
    new_result = send_command({
        "type": "manage_blueprint_node",
        "params": {
            "blueprint_name": "/Game/Blueprints/BP_Player2",
            "action": "create",
            "graph_scope": "function",
            "function_name": "CastToMicrosubHUD",
            "spawner_key": "GameplayStatics::GetPlayerController",  # ✅ Exact!
            "position": [-400, 200]
        }
    })
    
    new_pins = new_result.get("reflection_result", {}).get("pin_count", 0)
    print(f"New creation: {new_pins} pins")
    
    print(f"\n📊 Comparison:")
    print(f"  Legacy pins: {legacy_pins} (may be wrong variant)")
    print(f"  New pins: {new_pins} (always correct)")

def main():
    """Run all tests"""
    
    print("\n" + "█" * 80)
    print("█" + " " * 78 + "█")
    print("█" + "  NEW NODE DESCRIPTOR SYSTEM - INTEGRATION TEST".center(78) + "█")
    print("█" + " " * 78 + "█")
    print("█" * 80)
    
    # Test 1: Discovery with descriptors
    descriptors = test_discover_with_descriptors()
    
    if descriptors:
        # Find GameplayStatics variant
        gameplay_variant = next(
            (d for d in descriptors if "GameplayStatics" in d.get("spawner_key", "")),
            None
        )
        
        if gameplay_variant:
            spawner_key = gameplay_variant["spawner_key"]
            
            # Test 2: Create from exact spawner key
            test_create_from_spawner_key(spawner_key)
        
        # Test 3: Compare legacy vs new
        test_legacy_vs_new()
    
    print("\n" + "█" * 80)
    print("█" + " " * 78 + "█")
    print("█" + "  TESTS COMPLETE".center(78) + "█")
    print("█" + " " * 78 + "█")
    print("█" * 80 + "\n")

if __name__ == "__main__":
    main()
