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
import time
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

# Load bearer token and optional proxy_port from vibeue-proxy.json.
# This token is injected into every outbound request to UE, so the MCP client
# does not need to send auth — UE is still protected.
_PROXY_CONFIG_PATH = pathlib.Path(__file__).parent.parent.parent / "vibeue-proxy.json"
try:
    with open(_PROXY_CONFIG_PATH) as _f:
        _proxy_cfg = json.load(_f)
        _UE_BEARER_TOKEN = _proxy_cfg.get("bearer_token", "")
        PROXY_PORT = int(_proxy_cfg.get("proxy_port", PROXY_PORT))
except Exception:
    _UE_BEARER_TOKEN = ""

# ---------------------------------------------------------------------------
# Synthetic tools — always present in tools/list regardless of manifest state.
# Handled entirely by the proxy; never forwarded to UE.
# Keep descriptions SHORT — they are paid as tokens on every Claude request.
# ---------------------------------------------------------------------------

SYNTHETIC_TOOLS: list[dict] = [
    {
        "name": "vibeue_status",
        "description": (
            "Check VibeUE setup status and get next steps. "
            "Call this first if tools are not responding or UE seems unreachable. "
            "For first-time setup, read Plugins/VibeUE/VIBEUE_MCP_SETUP.md."
        ),
        "inputSchema": {"type": "object", "properties": {}, "required": []},
    }
]


def handle_vibeue_status(req_id) -> dict:
    """Synthetic tool: check UE reachability, token config, and manifest state."""
    ue_running = False
    try:
        conn = socket.create_connection(("127.0.0.1", UE_PORT), timeout=2)
        conn.close()
        ue_running = True
    except OSError:
        pass

    token_set = bool(_UE_BEARER_TOKEN)
    manifest_found = MANIFEST_PATH.exists()
    manifest_tools = len(load_manifest()) if manifest_found else 0

    if ue_running and token_set and manifest_found:
        text = (
            f"VibeUE is ready.\n"
            f"  UE running:     yes\n"
            f"  Token set:      yes\n"
            f"  Manifest:       {manifest_tools} tools loaded\n"
        )
    else:
        steps = []
        n = 1
        if not token_set:
            steps.append(
                f"{n}. Set a bearer token:\n"
                f"     a) Open Plugins/VibeUE/vibeue-proxy.json and set \"bearer_token\": \"<your-token>\"\n"
                f"     b) In UE: Project Settings -> Plugins -> VibeUE -> API Key — set the same value.\n"
                f"     c) Restart the proxy after editing vibeue-proxy.json."
            )
            n += 1
        if not ue_running:
            steps.append(f"{n}. Launch Unreal Engine with the VibeUE plugin enabled.")
            n += 1
        if not manifest_found:
            steps.append(
                f"{n}. Launch UE once with VibeUE enabled to generate the tool manifest at:\n"
                f"     {MANIFEST_PATH}"
            )
        text = (
            "VibeUE setup incomplete. For guided setup follow: Plugins/VibeUE/Content/Setup/Claude_MCP_Setup.md\n\n"
            "Remaining steps:\n"
            + "\n".join(steps)
        )

    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "result": {"content": [{"type": "text", "text": text}]},
    }


# ---------------------------------------------------------------------------
# Tool-description hints
#
# Appended to the manifest descriptions before serving tools/list.
# Keeps guidance close to the tool without requiring C++ or UE restarts.
#
# TOKEN COST: every hint is injected into every Claude context window.
# Keep hints focused — move deep reference material to skills instead.
# ---------------------------------------------------------------------------

# Maps tool name -> text appended to the existing description.
TOOL_HINTS: dict[str, str] = {

    "execute_python_code": """

IMMEDIATE ACTIONS — go straight to these, no skill or discovery step needed:

  User asks to list / show / describe components in a Widget Blueprint (WBP):
      import unreal
      components = unreal.WidgetService.list_components("/Game/path/WBP_X")
      print(components)
      Returns Array[WidgetInfo]: hierarchy + parent/child relationships, lightweight.
      USE THIS as the default for "what's in this widget" questions.
      DO NOT use get_hierarchy (names only) or get_widget_snapshot (full props, token-heavy) for hierarchy queries.

  User needs widget PROPERTIES (bindings, slot anchors, visibility, is_variable etc.):
      import unreal
      snapshot = unreal.WidgetService.get_widget_snapshot("/Game/path/WBP_X")
      print(snapshot)
      Returns full hierarchy + slot info + ALL properties — use only when property data is needed.
      Snapshot format: flat list of widget dicts. children are NAME STRINGS, not nested objects.
      Build a lookup dict to traverse:
          lookup = {w["name"]: w for w in snapshot}
          def children_of(name): return [lookup[c] for c in lookup[name]["children"] if c in lookup]
      Property access: snapshot[i]["properties"] → list of {name, value} dicts
      is_variable=True means the widget is a BindWidget target in C++.

  User asks to compile a Blueprint:
      import unreal
      unreal.BlueprintService.compile_blueprint("/Game/path/BP_X")

  User asks to list graphs in a Blueprint:
      import unreal
      unreal.BlueprintService.list_graphs("/Game/path/BP_X")

KEY RULES:
  - Assets (search/find/open/save/move/delete) → use the manage_asset TOOL, not Python.
  - Logs → use the read_logs TOOL, not Python file I/O.
  - Subsystems → unreal.get_editor_subsystem(unreal.SubsystemName)
    NOT unreal.EditorLevelLibrary (removed in UE 5.7+).
  - For complex graph authoring or unfamiliar services → load the relevant skill first.""",

    "manage_skills": """

SKILL INDEX — load BEFORE executing tasks in that domain:

UI / Widgets
  umg-widgets          → Widget Blueprint creation and authoring (get_widget_snapshot needs no skill)

Blueprints
  blueprints           → Blueprint assets, variables, functions, components
  blueprint-graphs     → Complex graph authoring, build_graph patterns, node wiring
  pie-testing          → Play-In-Editor testing, runtime state checks

Animation
  animation-blueprint  → AnimBP state machines, states, transitions
  animation-editing    → Bone rotation edits, constraints, retarget safety
  animation-montage    → Montage sections, slots, blending
  animsequence         → Animation sequence editing and baking
  skeleton             → Bones, sockets, retargeting, blend profiles

Landscape / Terrain
  landscape            → Sculpt, paint, heightmap import/export, procedural features
  landscape-materials  → Layer blending, texture setup, layer info objects
  landscape-auto-material → Master material + RVT + auto-layering biome system

Assets & Content
  asset-management     → Search, find, move, duplicate, save, import, export
  data-assets          → Primary Data Assets
  data-tables          → Data Tables, row management
  enum-struct          → UserDefinedEnums, UserDefinedStructs

Materials & VFX
  materials            → Material and material instance creation/editing
  uv-mapping           → UV channels on StaticMesh (inspect, generate, transform)
  niagara-systems      → Niagara system lifecycle, emitters, user parameters
  niagara-emitters     → Niagara emitter internals, modules, renderers, scratch-pad
  metasounds           → MetaSound Source assets, node graph authoring
  sound-cues           → SoundCue nodes and connections

AI / Logic
  state-trees          → StateTree tasks, transitions, considerations, event bindings
  enhanced-input       → Input Actions, Mapping Contexts, triggers

Environment
  foliage              → Foliage instances on landscapes
  pcg                  → Procedural Content Generation graphs
  level-actors         → Actor manipulation in the current level
  viewport             → Camera, view mode, FOV, layout, rendering settings

Project Config
  engine-settings      → Rendering, physics, audio, cvars, scalability
  project-settings     → Project settings + editor preferences
  gameplay-tags        → Gameplay Tag create/list/remove/rename

Capture / Research
  screenshots          → Editor screenshots for AI vision analysis
  terrain-data         → Real-world heightmaps, water features (rivers, lakes)
  vibeue               → General VibeUE Python API reference""",

}


def apply_hints(tools: list) -> list:
    """Append proxy-side guidance to tool descriptions before serving tools/list."""
    for tool in tools:
        hint = TOOL_HINTS.get(tool.get("name", ""))
        if hint:
            tool = dict(tool)  # shallow copy — don't mutate the cached manifest
            tool["description"] = tool.get("description", "") + hint
        yield tool


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

    success=True  -- UE returned a 2xx; response_bytes is valid JSON-RPC to forward as-is.
    success=False -- UE unreachable OR returned an error; response_bytes is a plain-text
                    description of the problem (may be empty if connection failed outright).
                    Caller must wrap this in a proper JSON-RPC error envelope.
    """
    forward_headers = {
        "Content-Type": "application/json",
        "Accept": "application/json",
        "X-VibeUE-Proxy": "true",  # Identifies this request as proxy-forwarded (issue #314)
        # Tell Python and UE not to reuse the connection.  Without this, Python's
        # urllib reuses the keep-alive socket from the previous call; UE's server
        # closes it on its end, so the next request hits a stale socket and raises
        # OSError / RemoteDisconnected, which was reported as "UE is not running".
        "Connection": "close",
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

    last_exc = None
    for attempt in range(2):          # retry once on stale-connection errors
        try:
            req = urllib.request.Request(
                UE_URL,
                data=body_bytes,
                headers=forward_headers,
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=120) as resp:
                return True, resp.read()
        except urllib.error.HTTPError as e:
            # UE responded but rejected the request (e.g. 401 bad token, 404 unknown session).
            # Return failure with the UE error text so the caller can surface it clearly.
            body = e.read()
            log(f"UE returned HTTP {e.code}: {body[:200]}")
            return False, body
        except (urllib.error.URLError, socket.timeout, OSError) as exc:
            last_exc = exc
            if attempt == 0:
                log(f"Connection error (attempt {attempt + 1}), retrying: {exc}")
            continue
    log(f"UE unreachable after 2 attempts: {last_exc}")
    return False, b""


def ue_error_response(req_id, tool_name: str, ue_message: str = "") -> dict:
    if ue_message:
        text = (
            f"Unreal Engine rejected the request: {ue_message}\n"
            f"Check that the bearer token in Plugins/VibeUE/vibeue-proxy.json matches "
            f"UE Project Settings -> Plugins -> VibeUE -> API Key.\n"
            f"For full setup instructions read: Plugins/VibeUE/Content/Setup/Claude_MCP_Setup.md"
        )
    else:
        text = (
            f"Unreal Engine is not running.\n"
            f"Please launch UE with the VibeUE plugin enabled, then retry '{tool_name}'.\n"
            f"If this is a first-time setup, follow: Plugins/VibeUE/Content/Setup/Claude_MCP_Setup.md"
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
        accept = self.headers.get("Accept", "")
        if "text/event-stream" not in accept:
            # Plain health check
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self._send_cors()
            self.end_headers()
            self.wfile.write(b"VibeUE proxy running")
            return

        # SSE stream — hold the connection open with heartbeats so the client
        # doesn't reconnect in a loop. Tool call responses still come back inline
        # on POST; this stream exists to stop the reconnect flood (issue #327).
        log("SSE stream opened")
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self._send_cors()
        self.end_headers()

        try:
            while True:
                self.wfile.write(b": heartbeat\n\n")
                self.wfile.flush()
                time.sleep(15)
        except (BrokenPipeError, ConnectionResetError, OSError):
            log("SSE stream closed")

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
            # Echo back the client's requested protocol version so Claude Code
            # doesn't reject the handshake due to a version it doesn't recognise.
            client_version = (rpc.get("params") or {}).get("protocolVersion", "2024-11-05")
            log(f"initialize (offline-capable, protocol {client_version})")
            self._jsonrpc(req_id, {
                "protocolVersion": client_version,
                "capabilities": {"tools": {}},
                "serverInfo": {
                    "name": "VibeUE-Proxy",
                    "version": "1.0.0",
                    "instructions": (
                        "VibeUE gives Claude direct access to Unreal Engine. "
                        "Quick-start rules: "
                        "(1) To inspect a Widget Blueprint, call execute_python_code immediately with unreal.WidgetService.get_widget_snapshot(path) — no skill or discovery step needed. "
                        "(2) To find/open/move assets, use manage_asset, not Python. "
                        "(3) If this is a first-time setup, read Plugins/VibeUE/Content/Setup/Claude_MCP_Setup.md and follow it. "
                        "If tools are not working after setup, call vibeue_status to diagnose."
                    ),
                },
            })
            return

        # --- notifications (no response needed) ---
        if method in ("notifications/initialized",):
            self.send_response(202)
            self.end_headers()
            return

        # --- tools/list: manifest tools + synthetic tools ---
        if method == "tools/list":
            tools = list(apply_hints(load_manifest())) + SYNTHETIC_TOOLS
            log(f"tools/list -> {len(tools)} tools ({len(tools) - len(SYNTHETIC_TOOLS)} manifest + {len(SYNTHETIC_TOOLS)} synthetic)")
            self._jsonrpc(req_id, {"tools": tools})
            return

        # --- synthetic tool calls: handled entirely by the proxy ---
        if method == "tools/call":
            tool_name = (rpc.get("params") or {}).get("name", "")
            if tool_name == "vibeue_status":
                log("vibeue_status -> handled by proxy")
                self._raw_json(handle_vibeue_status(req_id))
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

class QuietThreadingHTTPServer(ThreadingHTTPServer):
    """Suppress noisy tracebacks from normal client disconnects."""
    def handle_error(self, request, client_address):
        exc_type = sys.exc_info()[0]
        if exc_type in (BrokenPipeError, ConnectionAbortedError, ConnectionResetError):
            return
        super().handle_error(request, client_address)


if __name__ == "__main__":
    if not _PROXY_CONFIG_PATH.exists():
        log(f"WARNING: vibeue-proxy.json not found at {_PROXY_CONFIG_PATH}")
        log("Create it with {\"bearer_token\": \"<your-token>\"} to match UE Project Settings -> VibeUE -> API Key.")
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

    server = QuietThreadingHTTPServer(("127.0.0.1", PROXY_PORT), ProxyHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        log("Proxy stopped.")
