# VibeUE Issues Found
_Discovered during BP_ActiveUnitIndicator authoring — March 9, 2026_

---

## 1. `create_node_by_key` returns null GUID for most FUNC nodes

**Severity:** High
**Method:** `BlueprintService.create_node_by_key`
**Spawner key format:** `FUNC ClassName::FunctionName`

Nodes are created in the graph but assigned a GUID of `00000000`. This means:
- They cannot be referenced for subsequent `connect_nodes` calls
- They cannot be deleted via `delete_node`
- They accumulate as permanent ghost nodes in the graph

**Workaround:** Use `add_function_call_node(bp, graph, class_name, function_name, x, y)` instead — this consistently returns valid GUIDs.

---

## 2. Ghost `00000000` nodes cannot be deleted

**Severity:** High
**Method:** `BlueprintService.delete_node`

Nodes created with a null GUID (see issue #1) cannot be removed via the API. `delete_node` returns `False`. They also cannot be repositioned via `set_node_position`.

**Workaround:** In the Blueprint editor, right-click the graph → **Select All Disconnected Nodes → Delete**.

---

## 3. `set_node_pin_value` silently fails on class reference pins

**Severity:** High
**Method:** `BlueprintService.set_node_pin_value`
**Affected pin type:** `TSubclassOf<>` (e.g. the `Actor Class` pin on `GetActorOfClass`)

Returns `True` regardless of the value passed, but the pin value is never applied. Tried all formats:
- `"ATurnManager"`
- `"TurnManager"`
- `"/Script/InvasionTactical.ATurnManager"`
- `"Class /Script/InvasionTactical.ATurnManager"`

All return `True`, readback always shows `''`.

**Workaround:** Set class reference pins manually in the Blueprint editor dropdown.

---

## 4. `configure_node` fails on class reference properties

**Severity:** Medium
**Method:** `BlueprintService.configure_node`

Returns `False` for class reference properties. Same root cause as issue #3 — class reference types are not handled.

---

## 5. ~~`connect_nodes` fails when source is a default K2Node_Event~~ — FIXED

**Severity:** ~~High~~ — Fixed in commit `1973086` (Mar 13 2026), PR #326
**Method:** `BlueprintService.connect_nodes`

`AllocateDefaultPins()` is now called on any node with an empty Pins array before pin lookup. Default auto-placed K2Node_Event nodes (BeginPlay, Tick) now connect correctly.

~~**Workaround:** Use `add_event_node('ReceiveTick')` or `add_event_node('ReceiveBeginPlay')` to add user-created event override nodes. These return valid GUIDs and connect normally.~~

---

## 6. `add_set_variable_node` / `add_get_variable_node` fail for object reference variables

**Severity:** Medium
**Method:** `BlueprintService.add_set_variable_node`, `BlueprintService.add_get_variable_node`
**Affected type:** Object reference variables (e.g. `ATurnManager*`)

Both methods return an empty string (failure) for variables of object reference type, even when the variable exists and `list_variables` confirms it. ~~The variable type is reported as `object` by `list_variables` regardless of the actual class.~~ — Fixed by issue #9 fix (PR #330). `list_variables` now returns the correct class name.

**Note:** `add_get_variable_node` works correctly for **component** references (e.g. a `UStaticMeshComponent` added via `add_component`).

---

## 7. ~~`get_node_pins` returns empty for default K2Node_Event nodes~~ — FIXED

**Severity:** ~~Low~~ — Fixed in commit `1973086` (Mar 13 2026), PR #326
**Method:** `BlueprintService.get_node_pins`

Same `AllocateDefaultPins()` fix as issue #5. Pins are now correctly returned for default event nodes.

---

## 8. ~~Undocumented Branch exec pin names~~ — FIXED

**Severity:** ~~Low~~ — Fixed in commit `1973086` (Mar 13 2026), PR #326
**Method:** `BlueprintService.connect_nodes`

`connect_nodes` now normalises `True → then` and `False → else` (case-insensitive). Both the editor-visible names and internal names are accepted.

---

## 9. ~~`add_variable` always reports type as `object` or `int`~~ — FIXED

**Severity:** ~~Low~~ — Fixed in commit `013ee6c` (Mar 13 2026), PR #330
**Method:** `BlueprintService.list_variables`, `BlueprintService.get_variable_info`

`ListVariables` and `GetVariableInfo` now use `FBlueprintTypeParser::GetFriendlyTypeName` instead of raw `PinCategory.ToString()`. Types now report correctly (e.g. `float` not `real`, struct names instead of `struct`).

---

_Discovered during MCP connection audit — March 10, 2026_
_Note: "UE not running" diagnosis during audit was incorrect — the real cause was a stale wrong token in Claude's memory (`claudevibe` instead of `25220625`). Full stack confirmed working once correct token used._

---

## 10. `check_unreal_connection` tool does not exist in C++ source

**Severity:** Medium (documentation/diagnostic)

The VibeUE `CLAUDE.md` references `check_unreal_connection` as a diagnostic tool in Check 3 of the connection guide. However, this tool is **not registered anywhere in the C++ codebase** (`grep` finds no matches in `Source/`). Calling it returns the proxy's generic "UE not running" fallback — it is not a real tool.

**Impact:** The diagnostic guide in `CLAUDE.md` is misleading; the call appears to succeed (gets a response) but the response is from the proxy, not UE.

**Workaround:** Diagnose UE connectivity by attempting any Python tool call (e.g. `execute_python_code`) — if UE is running, it executes; if not, the proxy returns "Unreal Engine is not running".

---

## 11. CLAUDE.md documents 14 tools — current architecture has 9

**Severity:** Low (documentation staleness)

The VibeUE `CLAUDE.md` states the plugin exposes "14 multi-action tools with 177 total actions" and lists tools including `manage_blueprint`, `manage_asset`, `manage_umg_widget`, etc. The current implementation registers **9 tools** only (Python discovery/execution tools plus utilities). The manage_* tool layer was replaced by the Python-based approach.

**Impact:** Causes confusion when diagnosing tool count mismatches.

**Workaround:** Treat the 9-tool manifest as correct. The manage_* tools no longer exist.

---

## 12. No first-run nudge when MCP client connects directly (bypassing proxy)

**Severity:** Low (UX / discoverability)
**Location:** Module startup / MCP server initialisation (C++)

When a user connects directly to UE on port 8088, they get no indication that a proxy exists or why they might want it. The practical consequence: they work in Claude Code, launch UE mid-session, and find their AI tools are broken because Claude Code loaded with 0 tools at startup (before UE was running). Restarting Claude Code is the only fix in that scenario — but if they'd been pointed at the proxy, it would have worked transparently.

**Proposed fix:** On MCP server startup, if the first `initialize` request arrives on port 8088 directly (not forwarded from the proxy on 8089) and a local config flag `proxy_nudge_dismissed` is not set, emit a one-time dismissable notification:

> "Your AI client is connected directly to VibeUE (port 8088). If you launch your AI tool before opening Unreal, consider running the VibeUE proxy — it keeps your tools available even when the editor is closed. See the plugin docs for setup. [Dismiss]"

On dismiss, write `proxy_nudge_dismissed = true` to `Saved/Config/VibeUE.ini` so it never shows again.

**Implementation requires two changes:**

**Step 1 — proxy (`vibeue-proxy.py`):** Add `X-VibeUE-Proxy: true` to `forward_headers` in `forward_to_ue()`. One line. Without this, both proxy-forwarded and direct client requests arrive from `127.0.0.1` and are indistinguishable on the UE side.

**Step 2 — UE (`MCPServer.cpp`):** On receiving an `initialize` request, check for the `X-VibeUE-Proxy` header. If absent and `proxy_nudge_dismissed` is not set in `Saved/Config/VibeUE.ini`, fire the notification. On dismiss, write the flag. Client identity doesn't matter — trigger is purely the absence of the proxy header.

**Notes:**
- Flag lives in `Saved/Config/VibeUE.ini` (same pattern as disabled tools config in `ToolRegistry.cpp`)
- Notification can use `FNotificationManager::Get().AddNotification()` (Slate) or a simple `UE_LOG` if UI is out of scope
- Should fire once per machine, not once per session

---

## 13. No security warning when MCP server runs without an API key

**Severity:** High (security)
**Location:** `FMCPServer::ValidateApiKey` / `FMCPServer::Start()` (`MCPServer.cpp`)

When no API key is configured, `ValidateApiKey` silently returns `true` and all inbound connections are accepted with no warning.

**What IS protected (even without an API key):**
- External network access — server binds to `127.0.0.1` only
- Browser CSRF from external sites — `ValidateOrigin` rejects any `Origin` that isn't localhost/127.0.0.1/vscode/file
- DNS rebinding — attacker's domain origin is rejected by `ValidateOrigin`

**What is NOT protected without an API key:**
- Any local process on the machine — scripts, malware, browser extensions, other tools — can send requests with no `Origin` header. `ValidateOrigin` explicitly returns `true` for no-Origin requests (line 1231: `// likely a non-browser client like curl or an IDE`). Without an API key, these have full unauthenticated access to Python execution and asset manipulation inside UE.

This is a meaningful local attack surface. If the developer's machine has any compromised or untrusted software, their UE project can be freely manipulated or arbitrary Python executed inside the editor with no authentication required. External network and browser-based attacks are well-defended; it is the machine trust model that is the gap.

**Proposed fix:** In `FMCPServer::Start()`, after loading config, check if `Config.ApiKey` is empty and log at `Error` severity — every session, not just once:

```
SECURITY WARNING: VibeUE MCP server is running on port 8088 with NO API key set.
Any local process on this machine can connect and execute tools without authentication
— including file access, asset changes, and arbitrary Python code execution inside
Unreal Engine. Set an API key in Project Settings → Plugins → VibeUE → API Key
to restrict access.
```

**Notes:**
- Must be `UE_LOG(LogMCPServer, Error, ...)` — not Warning, not Log. Needs to be impossible to miss in the output log
- Must fire every session while the key is unset — not a one-time dismissable. The exposure is active every time UE runs
- Consider also surfacing as a red editor notification bar entry so it is visible without opening the output log
- `Access-Control-Allow-Origin: *` throughout the server is also worth revisiting — should be tightened to match the `ValidateOrigin` allowlist rather than wildcarded
