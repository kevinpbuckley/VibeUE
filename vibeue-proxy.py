#!/usr/bin/env python3
"""
VibeUE MCP Proxy
================
Always-running proxy that lets Claude Code (and other MCP clients) see
VibeUE tool definitions even when Unreal Engine is not running.

How it works:
  - Listens on PROXY_PORT (default 8089)
  - tools/list  -> served from %APPDATA%/VibeUE/tools-manifest.json (written by UE on startup)
  - tools/call  -> forwarded to UE on UE_PORT (default 8088); returns friendly error if UE is down
  - initialize  -> answered directly (no UE needed)
  - everything else -> forwarded to UE, or empty success if UE is down

Setup:
  1. Run this script once to start the proxy:
       pythonw vibeue-proxy.py      (Windows, no console window)
       python  vibeue-proxy.py      (any platform, with console)
  2. Point your MCP client at http://127.0.0.1:8089/mcp instead of 8088.
  3. Optionally add start-vibeue-proxy.bat to Windows startup so it runs automatically.
"""

import json
import os
import pathlib
import sys
import urllib.request
import urllib.error
from http.server import BaseHTTPRequestHandler, HTTPServer
from datetime import datetime, timezone

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

PROXY_PORT = 8089
UE_PORT    = 8088
UE_URL     = f"http://127.0.0.1:{UE_PORT}/mcp"

APPDATA = os.environ.get("APPDATA", str(pathlib.Path.home()))
MANIFEST_PATH = pathlib.Path(APPDATA) / "VibeUE" / "tools-manifest.json"

# Optional bearer token for authenticating to the UE MCP server.
# Read from vibeue-proxy.json ({"bearer_token": "..."}) or VIBEUE_BEARER_TOKEN env var.
def _load_bearer_token() -> str:
    config_path = pathlib.Path(__file__).parent / "vibeue-proxy.json"
    if config_path.exists():
        try:
            with open(config_path, encoding="utf-8") as f:
                return json.load(f).get("bearer_token", "")
        except Exception:
            pass
    return os.environ.get("VIBEUE_BEARER_TOKEN", "")

BEARER_TOKEN = _load_bearer_token()

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def log(msg: str) -> None:
    ts = datetime.now(timezone.utc).strftime("%H:%M:%S")
    print(f"[{ts}] {msg}", flush=True)


def load_manifest() -> list:
    """Load tool definitions from the manifest file written by UE.
    Handles both UTF-8 and UTF-16 (UE writes UTF-16 by default)."""
    if MANIFEST_PATH.exists():
        for encoding in ("utf-8-sig", "utf-16", "utf-8"):
            try:
                with open(MANIFEST_PATH, encoding=encoding) as f:
                    return json.load(f)
            except (UnicodeDecodeError, UnicodeError):
                continue
            except Exception as e:
                log(f"Warning: could not read manifest ({encoding}): {e}")
                break
    return []


def forward_to_ue(body_bytes: bytes, headers: dict) -> tuple[bool, bytes]:
    """Try to forward a raw request body to UE. Returns (success, response_bytes)."""
    forward_headers = {
        "Content-Type": "application/json",
        "Accept": "application/json",
    }
    if BEARER_TOKEN:
        forward_headers["Authorization"] = f"Bearer {BEARER_TOKEN}"
    # Pass through relevant MCP headers
    for key in ("mcp-protocol-version", "mcp-session-id"):
        if key in headers:
            forward_headers[key] = headers[key]

    try:
        req = urllib.request.Request(
            UE_URL,
            data=body_bytes,
            headers=forward_headers,
            method="POST",
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            return True, resp.read()
    except Exception:
        return False, b""


def ue_error_response(req_id, tool_name: str) -> dict:
    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "result": {
            "content": [
                {
                    "type": "text",
                    "text": (
                        f"Unreal Engine is not running.\n"
                        f"Please launch UE with the VibeUE plugin enabled, "
                        f"then retry '{tool_name}'."
                    ),
                }
            ],
            "isError": True,
        },
    }

# ---------------------------------------------------------------------------
# HTTP handler
# ---------------------------------------------------------------------------

class ProxyHandler(BaseHTTPRequestHandler):

    def log_message(self, fmt, *args):
        # Suppress default access log; we do our own
        pass

    def _send_cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS")
        self.send_header(
            "Access-Control-Allow-Headers",
            "Content-Type, MCP-Protocol-Version, mcp-session-id",
        )

    def do_OPTIONS(self):
        self.send_response(200)
        self._send_cors()
        self.end_headers()

    def do_GET(self):
        # SSE / health — try UE, otherwise 503
        success, body = forward_to_ue(b"", self._lower_headers())
        if success:
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self._send_cors()
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_response(503)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"Unreal Engine is not running")

    def do_POST(self):
        if self.path != "/mcp":
            self.send_response(404)
            self.end_headers()
            return

        length = int(self.headers.get("Content-Length", 0))
        raw_body = self.rfile.read(length)
        lower_headers = self._lower_headers()

        try:
            rpc = json.loads(raw_body)
        except json.JSONDecodeError:
            self._json(400, {"error": "Invalid JSON"})
            return

        method = rpc.get("method", "")
        req_id = rpc.get("id")

        # --- initialize: answer directly, no UE needed ---
        if method == "initialize":
            log(f"initialize (offline-capable)")
            self._jsonrpc(req_id, {
                "protocolVersion": "2025-11-25",
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "VibeUE-Proxy", "version": "1.0.0"},
            })
            return

        # --- notifications (no response needed) ---
        if method in ("notifications/initialized",):
            self.send_response(202)
            self.end_headers()
            return

        # --- tools/list: always serve from manifest ---
        if method == "tools/list":
            tools = load_manifest()
            log(f"tools/list -> {len(tools)} tools from manifest (UE may or may not be running)")
            self._jsonrpc(req_id, {"tools": tools})
            return

        # --- everything else: try UE ---
        success, response_bytes = forward_to_ue(raw_body, lower_headers)
        if success:
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self._send_cors()
            self.end_headers()
            self.wfile.write(response_bytes)
            log(f"{method} -> forwarded to UE")
        else:
            log(f"{method} -> UE not running")
            if method == "tools/call":
                tool_name = (rpc.get("params") or {}).get("name", "unknown")
                self._raw_json(ue_error_response(req_id, tool_name))
            else:
                # ping, resources/list, etc. — return empty success
                self._jsonrpc(req_id, {})

    # -----------------------------------------------------------------------
    # Helpers
    # -----------------------------------------------------------------------

    def _lower_headers(self) -> dict:
        return {k.lower(): v for k, v in self.headers.items()}

    def _jsonrpc(self, req_id, result: dict):
        self._raw_json({"jsonrpc": "2.0", "id": req_id, "result": result})

    def _raw_json(self, data: dict):
        body = json.dumps(data).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self._send_cors()
        self.end_headers()
        self.wfile.write(body)

    def _json(self, status: int, data: dict):
        body = json.dumps(data).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    if not MANIFEST_PATH.exists():
        log(f"Note: manifest not found at {MANIFEST_PATH}")
        log("Launch Unreal Engine with VibeUE once to generate it.")
    else:
        tools = load_manifest()
        log(f"Loaded {len(tools)} tools from manifest")

    log(f"VibeUE MCP Proxy listening on http://127.0.0.1:{PROXY_PORT}/mcp")
    log(f"Forwarding tool calls to UE at http://127.0.0.1:{UE_PORT}/mcp")

    server = HTTPServer(("127.0.0.1", PROXY_PORT), ProxyHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        log("Proxy stopped.")
