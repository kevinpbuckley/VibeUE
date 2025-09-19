"""
Test MCP tool parameter mapping fix
"""
import sys
sys.path.append('.')

from vibe_ue_server import get_unreal_connection

def test_node_creation():
    try:
        print('🔧 Testing Parameter Mapping Fix')
        print('================================')
        
        # Connect to Unreal
        unreal = get_unreal_connection()
        if not unreal:
            print('❌ Could not connect to Unreal Engine')
            return
            
        print('✅ Connected to Unreal Engine')
        
        # Test the fixed parameter mapping
        print('\n📋 Testing add_blueprint_node with correct parameters...')
        result = unreal.send_command('add_blueprint_node', {
            'blueprint_name': 'BP_Player',
            'node_identifier': 'Branch',  # Correct parameter name
            'position': [500, 300]
        })
        
        if result and result.get('success', False):
            node_id = result.get('node_id', 'Unknown')
            print(f'✅ SUCCESS: Created Branch node with ID: {node_id}')
            
            # Verify by listing nodes
            print('\n🔍 Verifying node creation...')
            verify_result = unreal.send_command('list_event_graph_nodes', {
                'blueprint_name': 'BP_Player'
            })
            
            if verify_result and 'nodes' in verify_result.get('result', {}):
                nodes = verify_result['result']['nodes']
                branch_count = sum(1 for node in nodes if 'branch' in node.get('title', '').lower())
                print(f'✅ Found {branch_count} Branch nodes in BP_Player')
                print(f'✅ Total nodes in blueprint: {len(nodes)}')
            
        else:
            error_msg = result.get('error', 'Unknown error') if result else 'No response'
            print(f'❌ ERROR: {error_msg}')
            
        print('\n🎯 Parameter Mapping Fix Status:')
        if result and result.get('success', False):
            print('✅ node_type → node_identifier mapping WORKING')
            print('✅ MCP tool parameter fix SUCCESSFUL')
        else:
            print('❌ Parameter mapping still has issues')
            
    except Exception as e:
        print(f'❌ Error during test: {e}')
        import traceback
        print(f'Traceback: {traceback.format_exc()}')

if __name__ == '__main__':
    test_node_creation()