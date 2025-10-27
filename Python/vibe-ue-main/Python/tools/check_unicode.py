import os
import sys

files = [
    'manage_asset.py',
    'manage_blueprint.py', 
    'manage_blueprint_component.py',
    'manage_blueprint_function.py',
    'manage_blueprint_node.py',
    'manage_blueprint_variable.py',
    'manage_umg_widget.py',
    'system.py'
]

for filename in files:
    if os.path.exists(filename):
        print(f"\nChecking {filename}:")
        with open(filename, 'rb') as f:
            content = f.read()
        
        # Try to decode and check for issues
        try:
            decoded = content.decode('utf-8')
            
            # Check for surrogate pairs
            for i, char in enumerate(decoded):
                if 0xD800 <= ord(char) <= 0xDFFF:
                    print(f"  SURROGATE at position {i}: U+{ord(char):04X}")
                    context_start = max(0, i - 20)
                    context_end = min(len(decoded), i + 20)
                    print(f"  Context: {repr(decoded[context_start:context_end])}")
            
            print(f"  OK - {len(decoded)} characters, no surrogates found")
        except Exception as e:
            print(f"  ERROR: {e}")
