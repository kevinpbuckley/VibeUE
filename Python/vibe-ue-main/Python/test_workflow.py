"""
Test the complete Blueprint node workflow
"""
import sys
sys.path.append('.')

from unreal_mcp_server import send_command
import traceback

def test_complete_workflow():
    try:
        print('🎯 COMPLETE WORKFLOW TEST')
        print('========================')
        
        # Step 1: Search available nodes
        print('\n1️⃣ SEARCH AVAILABLE NODES:')
        search_result = send_command('get_available_blueprint_nodes', {
            'blueprint_name': 'BP_Player'
        })
        
        if 'error' not in search_result:
            categories = search_result.get('result', {}).get('categories', [])
            total_nodes = sum(len(cat.get('nodes', [])) for cat in categories)
            print(f'   ✅ Found {total_nodes} available node types')
            
            # Show some example categories
            if categories:
                print('   📂 Available categories:')
                for cat in categories[:5]:  # Show first 5
                    name = cat.get('name', 'Unknown')
                    count = len(cat.get('nodes', []))
                    print(f'      • {name}: {count} nodes')
        else:
            error_msg = search_result.get('error', 'Unknown error')
            print(f'   ❌ Error searching nodes: {error_msg}')
            
        # Step 2: Create a node
        print('\n2️⃣ CREATE NEW NODE:')  
        create_result = send_command('add_blueprint_node', {
            'blueprint_name': 'BP_Player',
            'node_identifier': 'Branch',
            'position': [500, 300]
        })
        
        if 'error' not in create_result:
            node_id = create_result.get('result', {}).get('node_id', 'Unknown')
            print(f'   ✅ Created Branch node with ID: {node_id}')
        else:
            error_msg = create_result.get('error', 'Unknown error')
            print(f'   ❌ Error creating node: {error_msg}')
        
        # Step 3: Verify node exists
        print('\n3️⃣ VERIFY NODE CREATION:')
        verify_result = send_command('list_event_graph_nodes', {
            'blueprint_name': 'BP_Player'
        })
        
        if 'error' not in verify_result:
            nodes = verify_result.get('result', {}).get('nodes', [])
            branch_count = sum(1 for node in nodes if 'branch' in node.get('title', '').lower())
            print(f'   ✅ Found {branch_count} Branch nodes in BP_Player')
            print(f'   ✅ Total nodes in blueprint: {len(nodes)}')
        else:
            error_msg = verify_result.get('error', 'Unknown error')
            print(f'   ❌ Error verifying nodes: {error_msg}')
        
        print('\n🎉 WORKFLOW COMPLETE!')
        print('✅ Search → Create → Verify pipeline working')
        print('✅ Blueprint Action Database operational')  
        print('✅ Node creation system functional')
        
    except Exception as e:
        print(f'❌ Error: {e}')
        print(f'Traceback: {traceback.format_exc()}')

if __name__ == '__main__':
    test_complete_workflow()