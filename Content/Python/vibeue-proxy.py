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
import socket
from http.server import BaseHTTPRequestHandler, HTTPServer, ThreadingHTTPServer
from datetime import datetime, timezone

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

PROXY_PORT = 8089
UE_PORT    = 8088
UE_URL     = f"http://127.0.0.1:{UE_PORT}/mcp"

APPDATA = os.environ.get("APPDATA", str(pathlib.Path.home()))
MANIFEST_PATH = pathlib.Path(APPDATA) / "VibeUE" / "tools-manifest.json"

# Load bearer token from vibeue-proxy.json (same directory as this script).
# This token is injected into every outbound request to UE, so the MCP client
# does not need to send auth — UE is still protected.
_PROXY_CONFIG_PATH = pathlib.Path(__file__).parent.parent.parent / "vibeue-proxy.json"
try:
    with open(_PROXY_CONFIG_PATH) as _f:
        _UE_BEARER_TOKEN = json.load(_f).get("bearer_token", "")
except Exception:
    _UE_BEARER_TOKEN = ""

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
    """Try to forward a raw request body to UE. Returns (success, response_bytes).

    success=True  → UE returned a 2xx; response_bytes is valid JSON-RPC to forward as-is.
    success=False → UE unreachable OR returned an error; response_bytes is a plain-text
                    description of the problem (may be empty if connection failed outright).
                    Caller must wrap this in a proper JSON-RPC error envelope.
    """
    forward_headers = {
        "Content-Type": "application/json",
        "Accept": "application/json",
    }
    # Inject the UE bearer token directly from vibeue-proxy.json — do not rely on
    # the MCP client forwarding it, as some clients (e.g. Claude Code) omit auth headers.
    if _UE_BEARER_TOKEN:
        forward_headers["Authorization"] = f"Bearer {_UE_BEARER_TOKEN}"
    # Pass through MCP protocol version from the incoming request.
    # Do NOT forward mcp-session-id — the proxy answers initialize itself and has no
    # session with UE, so a client session ID forwarded verbatim would be rejected by UE.
    for key in ("mcp-protocol-version",):
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
    except urllib.error.HTTPError as e:
        # UE responded but rejected the request (e.g. 401 bad token, 404 unknown session).
        # Return failure with the UE error text so the caller can surface it clearly.
        body = e.read()
        log(f"UE returned HTTP {e.code}: {body[:200]}")
        return False, body
    except (urllib.error.URLError, socket.timeout, OSError):
        return False, b""


def ue_error_response(req_id, tool_name: str, ue_message: str = "") -> dict:
    if ue_message:
        text = (
            f"Unreal Engine rejected the request: {ue_message}\n"
            f"Check that the API token in ~/.claude/mcp.json matches "
            f"Project Settings → Plugins → VibeUE → API Key."
        )
    else:
        text = (
            f"Unreal Engine is not running.\n"
            f"Please launch UE with the VibeUE plugin enabled, "
            f"then retry '{tool_name}'."
        )
    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "result": {
            "content": [{"type": "text", "text": text}],
            "isError": True,
        },
    }

# ---------------------------------------------------------------------------
# HTTP handler
# ---------------------------------------------------------------------------

class ProxyHandler(BaseHTTPRequestHandler):

    protocol_version = "HTTP/1.1"

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
        # Health check only — UE speaks JSON-RPC POST, not GET/SSE
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self._send_cors()
        self.end_headers()
        self.wfile.write(b"VibeUE proxy running")

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
            ue_msg = response_bytes.decode(errors="replace").strip() if response_bytes else ""
            log(f"{method} -> UE error: {ue_msg or '(no response — UE not running or unreachable)'}")
            if method == "tools/call":
                tool_name = (rpc.get("params") or {}).get("name", "unknown")
                self._raw_json(ue_error_response(req_id, tool_name, ue_msg))
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
    if not _PROXY_CONFIG_PATH.exists():
        log(f"WARNING: vibeue-proxy.json not found at {_PROXY_CONFIG_PATH}")
        log("Create it with {\"bearer_token\": \"<your-token>\"} to match UE Project Settings → VibeUE → API Key.")
        log("Without it, requests to UE will be sent without auth and will fail if an API Key is set.")
    elif not _UE_BEARER_TOKEN:
        log(f"WARNING: vibeue-proxy.json exists but 'bearer_token' is empty — UE requests will be unauthenticated.")
    else:
        log(f"Bearer token loaded from vibeue-proxy.json")

    if not MANIFEST_PATH.exists():
        log(f"Note: manifest not found at {MANIFEST_PATH}")
        log("Launch Unreal Engine with VibeUE once to generate it.")
    else:
        tools = load_manifest()
        log(f"Loaded {len(tools)} tools from manifest")

    log(f"VibeUE MCP Proxy listening on http://127.0.0.1:{PROXY_PORT}/mcp")
    log(f"Forwarding tool calls to UE at http://127.0.0.1:{UE_PORT}/mcp")

    server = ThreadingHTTPServer(("127.0.0.1", PROXY_PORT), ProxyHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        log("Proxy stopped.")
