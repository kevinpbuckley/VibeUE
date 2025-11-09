#!/usr/bin/env python3
"""Validate server.json against MCP schema"""

import json
import sys
from pathlib import Path

try:
    import requests
    from jsonschema import validate, ValidationError, SchemaError
except ImportError:
    print("Error: Required packages not installed. Run: pip install jsonschema requests")
    sys.exit(1)

def validate_server_json(server_json_path: Path):
    """Validate server.json against the MCP schema"""
    
    # Read server.json
    try:
        with open(server_json_path, 'r') as f:
            server_data = json.load(f)
    except FileNotFoundError:
        print(f"❌ Error: File not found: {server_json_path}")
        return False
    except json.JSONDecodeError as e:
        print(f"❌ Error: Invalid JSON in {server_json_path}: {e}")
        return False
    
    # Get schema URL from server.json
    schema_url = server_data.get('$schema')
    if not schema_url:
        print("❌ Error: No $schema field found in server.json")
        return False
    
    print(f"📥 Downloading schema from: {schema_url}")
    
    # Download schema
    try:
        response = requests.get(schema_url, timeout=10)
        response.raise_for_status()
        schema = response.json()
    except requests.RequestException as e:
        print(f"❌ Error downloading schema: {e}")
        return False
    except json.JSONDecodeError:
        print(f"❌ Error: Schema at {schema_url} is not valid JSON")
        return False
    
    # Validate
    try:
        validate(instance=server_data, schema=schema)
        print("✅ server.json is valid!")
        print(f"\n📋 Server Details:")
        print(f"   Name: {server_data.get('name')}")
        print(f"   Title: {server_data.get('title')}")
        print(f"   Version: {server_data.get('version')}")
        print(f"   Description: {server_data.get('description')}")
        
        if 'packages' in server_data:
            print(f"\n📦 Packages:")
            for pkg in server_data['packages']:
                print(f"   - {pkg.get('registryType')}: {pkg.get('identifier')} v{pkg.get('version')}")
        
        return True
        
    except ValidationError as e:
        print(f"❌ Validation Error: {e.message}")
        print(f"   Path: {' -> '.join(str(p) for p in e.path)}")
        if e.schema_path:
            print(f"   Schema Path: {' -> '.join(str(p) for p in e.schema_path)}")
        return False
        
    except SchemaError as e:
        print(f"❌ Schema Error: {e.message}")
        return False

if __name__ == "__main__":
    script_dir = Path(__file__).parent
    server_json = script_dir / "server.json"
    
    print(f"🔍 Validating {server_json}")
    print("=" * 60)
    
    success = validate_server_json(server_json)
    sys.exit(0 if success else 1)
