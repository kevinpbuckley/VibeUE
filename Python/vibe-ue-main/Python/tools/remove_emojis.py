import os
import re

# List of tool files to process
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

# Comprehensive emoji pattern
emoji_pattern = re.compile(
    '[\U0001F300-\U0001F9FF'  # Misc symbols & pictographs
    '\U00002600-\U000027BF'   # Misc symbols
    '\U0001F1E0-\U0001F1FF'   # Flags
    '\U0001F900-\U0001F9FF'   # Supplemental symbols
    '\U00002300-\U000023FF'   # Misc technical
    '\U0001F600-\U0001F64F'   # Emoticons
    '\U0001F680-\U0001F6FF'   # Transport & map
    '\U0001F700-\U0001F77F'   # Alchemical
    '\U0001F780-\U0001F7FF'   # Geometric shapes
    '\U0001F800-\U0001F8FF'   # Supplemental arrows
    '\U0001F3A0-\U0001F3FF'   # Misc symbols & pictographs
    '\U0001FA70-\U0001FAFF'   # Symbols and pictographs extended
    ']+',
    flags=re.UNICODE
)

count = 0
for filename in files:
    if os.path.exists(filename):
        with open(filename, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Remove emojis
        cleaned = emoji_pattern.sub('', content)
        
        # Write back
        with open(filename, 'w', encoding='utf-8') as f:
            f.write(cleaned)
        
        count += 1
        print(f"Processed: {filename}")

print(f"\nRemoved emojis from {count} files")
