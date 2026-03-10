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

## 5. `connect_nodes` fails when source is a default K2Node_Event

**Severity:** High
**Method:** `BlueprintService.connect_nodes`
**Affected nodes:** `Event BeginPlay`, `Event Tick` (default auto-generated event nodes)

Every exec output pin name tried returns `False`: `then`, `Exec`, `execute`, `Then`, `""`, etc. The source node ID resolves correctly but the pin cannot be connected from.

**Workaround:** Use `add_event_node('ReceiveTick')` or `add_event_node('ReceiveBeginPlay')` to add user-created event override nodes. These return valid GUIDs and connect normally.

---

## 6. `add_set_variable_node` / `add_get_variable_node` fail for object reference variables

**Severity:** Medium
**Method:** `BlueprintService.add_set_variable_node`, `BlueprintService.add_get_variable_node`
**Affected type:** Object reference variables (e.g. `ATurnManager*`)

Both methods return an empty string (failure) for variables of object reference type, even when the variable exists and `list_variables` confirms it. The variable type is reported as `object` by `list_variables` regardless of the actual class.

**Note:** `add_get_variable_node` works correctly for **component** references (e.g. a `UStaticMeshComponent` added via `add_component`).

---

## 7. `get_node_pins` returns empty for default K2Node_Event nodes

**Severity:** Low
**Method:** `BlueprintService.get_node_pins`
**Affected nodes:** `Event BeginPlay`, `Event Tick`

Returns 0 pins despite `get_nodes_in_graph` reporting e.g. 3 pins on the same node. Purely a read issue — `connect_nodes` attempts on these nodes (issue #5) are the real blocker.

---

## 8. Undocumented Branch exec pin names

**Severity:** Low (documentation gap)
**Method:** `BlueprintService.connect_nodes`

The Branch node's exec output pins are named `then` (True path) and `else` (False path) internally — not `True` and `False` as displayed in the editor. Using `True`/`False` silently fails.

---

## 9. `add_variable` always reports type as `object` or `int`

**Severity:** Low (cosmetic/diagnostic)
**Method:** `BlueprintService.add_variable`, `BlueprintService.list_variables`

Variables added with an actor class type (e.g. `ATurnManager`) are created but `list_variables` reports their type as `object` or `int`. The actual underlying type may be correct but the display is misleading.

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
