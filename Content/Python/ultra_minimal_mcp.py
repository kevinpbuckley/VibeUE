#!/usr/bin/env python3
"""Ultra-minimal MCP server for testing"""
import sys

# Fix for Windows: MCP protocol expects LF (\n) only, not CRLF (\r\n)
if sys.platform == "win32":
    sys.stdout.reconfigure(newline="\n")

import json

# Ensure unbuffered output
sys.stdout.reconfigure(line_buffering=True)
sys.stderr.reconfigure(line_buffering=True)

def send_response(response):
    """Send JSON-RPC response"""
    print(json.dumps(response), flush=True)

def main():
    """Main loop"""
    for line in sys.stdin:
        try:
            request = json.loads(line.strip())
            
            if request.get("method") == "initialize":
                send_response({
                    "jsonrpc": "2.0",
                    "id": request["id"],
                    "result": {
                        "protocolVersion": "2024-11-05",
                        "capabilities": {},
                        "serverInfo": {
                            "name": "TestMCP",
                            "version": "1.0.0"
                        }
                    }
                })
            elif request.get("method") == "tools/list":
                send_response({
                    "jsonrpc": "2.0",
                    "id": request["id"],
                    "result": {
                        "tools": []
                    }
                })
            else:
                send_response({
                    "jsonrpc": "2.0",
                    "id": request.get("id"),
                    "error": {
                        "code": -32601,
                        "message": "Method not found"
                    }
                })
        except Exception as e:
            print(f"ERROR: {e}", file=sys.stderr)

if __name__ == "__main__":
    main()
