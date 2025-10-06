"""
Test script for function call node bug fix.

This script tests the fix for creating function call nodes using spawner search.
It will attempt to create a K2_GetActorLocation node which previously failed.

Expected Result: Node created with correct "Get Actor Location" function and proper pins
Previous Bug: Node created as "Set Material Function" with wrong pins
"""

from tools.node_tools import manage_blueprint_node
from tools.blueprint_tools import get_blueprint_info
from tools.search_tools import search_items

def test_function_call_node_creation():
    """Test creating a simple Actor function call node."""
    
    print("\n" + "="*80)
    print("TEST: Function Call Node Creation Fix Validation")
    print("="*80)
    
    # Step 1: Find the Blueprint
    print("\n[1] Searching for BP_Player2...")
    search_result = search_items(search_term="BP_Player2", asset_type="Blueprint")
    
    if not search_result.get("success"):
        print(f"❌ Search failed: {search_result.get('error')}")
        return False
    
    if not search_result.get("items"):
        print("❌ BP_Player2 not found in search results")
        return False
    
    bp_item = search_result["items"][0]
    blueprint_path = bp_item["package_path"]
    print(f"✅ Found Blueprint: {blueprint_path}")
    
    # Step 2: Get Blueprint info to confirm SpawnDeathEffects function exists
    print("\n[2] Getting Blueprint info...")
    bp_info = get_blueprint_info(blueprint_path)
    
    if not bp_info.get("success"):
        print(f"❌ Failed to get Blueprint info: {bp_info.get('error')}")
        return False
    
    functions = bp_info.get("blueprint_info", {}).get("functions", [])
    spawn_death_effects = None
    for func in functions:
        if func.get("name") == "SpawnDeathEffects":
            spawn_death_effects = func
            break
    
    if not spawn_death_effects:
        print("❌ SpawnDeathEffects function not found")
        return False
    
    print(f"✅ Found SpawnDeathEffects function")
    
    # Step 3: Create K2_GetActorLocation node using function_name in node_params
    print("\n[3] Creating K2_GetActorLocation node...")
    print("    Using: node_params={'function_name': 'K2_GetActorLocation'}")
    
    create_result = manage_blueprint_node(
        blueprint_name=blueprint_path,
        action="create",
        graph_scope="function",
        function_name="SpawnDeathEffects",
        node_type="K2_GetActorLocation",
        node_params={"function_name": "K2_GetActorLocation"},
        position=[100, 100]
    )
    
    if not create_result.get("success"):
        print(f"❌ Node creation failed: {create_result.get('error')}")
        return False
    
    node_id = create_result.get("node_id")
    print(f"✅ Node created with ID: {node_id}")
    
    # Step 4: Get node details to verify correct function
    print("\n[4] Verifying node configuration...")
    
    details_result = manage_blueprint_node(
        blueprint_name=blueprint_path,
        action="get_details",
        graph_scope="function",
        function_name="SpawnDeathEffects",
        node_id=node_id
    )
    
    if not details_result.get("success"):
        print(f"❌ Failed to get node details: {details_result.get('error')}")
        return False
    
    node_info = details_result.get("node")
    node_title = node_info.get("title", "Unknown")
    pins = node_info.get("pins", [])
    
    print(f"    Node Title: {node_title}")
    print(f"    Pin Count: {len(pins)}")
    print("    Pins:")
    for pin in pins:
        print(f"      - {pin.get('name')} ({pin.get('direction')}, {pin.get('type')})")
    
    # Validation checks
    validation_passed = True
    
    # Check 1: Title should contain "Get Actor Location" or "K2_GetActorLocation"
    if "Material" in node_title and "Function" in node_title:
        print("\n❌ VALIDATION FAILED: Node is still 'Set Material Function'!")
        validation_passed = False
    elif "Location" in node_title or "K2_GetActorLocation" in node_title:
        print(f"\n✅ VALIDATION PASSED: Node title is correct: '{node_title}'")
    else:
        print(f"\n⚠️  WARNING: Node title is unexpected: '{node_title}'")
    
    # Check 2: Should have execution pin and Return Value (Vector)
    has_exec_pin = any(pin.get("name") == "exec" or pin.get("name") == "then" for pin in pins)
    has_return_value = any("Return" in pin.get("name", "") for pin in pins)
    
    if has_exec_pin:
        print("✅ Has execution pin")
    else:
        print("❌ Missing execution pin")
        validation_passed = False
    
    if has_return_value:
        print("✅ Has Return Value pin")
    else:
        print("❌ Missing Return Value pin")
        validation_passed = False
    
    # Check 3: Should NOT have Material-related pins
    has_material_pins = any("Material" in pin.get("name", "") for pin in pins)
    if has_material_pins:
        print("❌ Has Material-related pins (bug not fixed!)")
        validation_passed = False
    else:
        print("✅ No Material-related pins")
    
    print("\n" + "="*80)
    if validation_passed:
        print("✅ TEST PASSED: Function call node creation fix is working!")
        print("="*80)
        return True
    else:
        print("❌ TEST FAILED: Function call node creation fix needs more work")
        print("="*80)
        return False

if __name__ == "__main__":
    import sys
    success = test_function_call_node_creation()
    sys.exit(0 if success else 1)
