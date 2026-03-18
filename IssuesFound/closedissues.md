# VibeUE — Closed / Fixed Issues

## ~~#268 InEditorChat — Streaming on VibeUE model~~ — FIXED

**Fixed:** 2026-03-14, PR #336 (Kevin's commit `075c4bc`)
SSE streaming enabled for the VibeUE API client. Closed upstream — no longer in open issues list.

Archived from `issues.md`. See that file for open issues.

---

## 6. ~~`add_set_variable_node` / `add_get_variable_node` fail for object reference variables~~ — FIXED

**Fixed:** 2026-03-15, PR #335
Root cause: guard used `FindFProperty(GeneratedClass)` which returns null for uncompiled BPs. Added `NewVariables` fallback so both methods work before compile. Affects all variable types, not just object references.

---

## 16. ~~`discover_nodes` does not surface widget component functions (WBP context)~~ — FIXED

**Fixed:** 2026-03-15, PR #335
Added a widget tree walk for `UWidgetBlueprint`: `WidgetTree->ForEachWidget` collects unique widget classes and iterates their functions. Discovery now reflects the actual widgets in the WBP dynamically.

---

## 1. ~~`create_node_by_key` returns null GUID for most FUNC nodes~~ — FIXED

**Fixed:** 2026-03-15, session 3
**Root cause:** `CreateNewGuid()` and `PostPlacedNewNode()` were never called in either the `FUNC` or `NODE` branch. The correct initialization order (matching `AddFunctionCallNode`) is: `AddNode` → `CreateNewGuid` → `PostPlacedNewNode` → `AllocateDefaultPins`.

---

## 2. ~~Ghost `00000000` nodes cannot be deleted~~ — FIXED

**Fixed:** 2026-03-15, session 3 (same fix as #1)
Nodes created by `create_node_by_key` now receive valid GUIDs so `delete_node` and `set_node_position` work correctly.

---

## 14. ~~`add_function_call_node` fails for `UUserWidget::GetOwningActor`~~ — INVALID

**Verdict:** False issue — `UUserWidget::GetOwningActor` does not exist in UE 5.7.
Investigated 2026-03-15: confirmed `GetOwningActor` is not a UFUNCTION on `UUserWidget` or `UWidget`.

The functions people actually want are:
- `GetOwningPlayer` → `APlayerController*` — works via `add_function_call_node("UserWidget", "GetOwningPlayer")`
- `GetOwningPlayerPawn` → `APawn*` — works via `add_function_call_node("UserWidget", "GetOwningPlayerPawn")`

---

## 3. ~~`set_node_pin_value` silently fails on class reference pins~~ — FIXED

**Fixed:** 2026-03-15, PR #335
Class reference pins (`TSubclassOf<>`) now route through `TrySetDefaultObject` with U/A prefix fallbacks.

---

## 4. ~~`configure_node` fails on class reference properties~~ — FIXED

**Fixed:** 2026-03-15, PR #335
`FClassProperty` branch now uses `FindFirstObject` with U/A prefix chain.

---

## 10. ~~`check_unreal_connection` tool does not exist~~ — INVALID (already documented)

CLAUDE.md already correctly states this tool doesn't exist. Issue was stale — closed 2026-03-15.

---

## 11. ~~CLAUDE.md documents 14 tools~~ — INVALID (already fixed)

CLAUDE.md already correctly documents 9 tools. Issue was stale — closed 2026-03-15.

---

## 5. ~~`connect_nodes` fails when source is a default K2Node_Event~~ — FIXED

**Severity:** ~~High~~ — Fixed in commit `1973086` (Mar 13 2026), PR #326
**Method:** `BlueprintService.connect_nodes`

`AllocateDefaultPins()` is now called on any node with an empty Pins array before pin lookup. Default auto-placed K2Node_Event nodes (BeginPlay, Tick) now connect correctly.

~~**Workaround:** Use `add_event_node('ReceiveTick')` or `add_event_node('ReceiveBeginPlay')` to add user-created event override nodes. These return valid GUIDs and connect normally.~~

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

## 12. ~~No first-run nudge when MCP client connects directly (bypassing proxy)~~ — FIXED

**Severity:** ~~Low~~ — Fixed in commit `0dc70c8` (Mar 14 2026), PR #332 (closes GitHub #314)
**Location:** Module startup / MCP server initialisation (C++)

On first `initialize` request without `X-VibeUE-Proxy` header, UE now fires a one-time Slate toast pointing the user at the proxy with a "Got it, don't show again" dismiss button. Proxy adds `X-VibeUE-Proxy: true` to all forwarded requests so the two connection types are distinguishable.

---

## 13. ~~No security warning when MCP server runs without an API key~~ — FIXED

**Severity:** ~~High~~ — Fixed in commit `6740f93` (Mar 14 2026), PR #332 (closes GitHub #315)
**Location:** `FMCPServer::Start()` (`MCPServer.cpp`)

`FMCPServer::Start()` now logs at `Error` severity every session when `Config.ApiKey` is empty, making the unauthenticated exposure impossible to miss in the output log.

---

## 15. ~~Delegate bind nodes (`UK2Node_AddDelegate`) cannot be created~~ — FIXED

**Severity:** ~~High~~ — Fixed in commit `ad621ea` (Mar 14 2026), closes GitHub #15
**Method:** `BlueprintService.add_delegate_bind_node`

Added `BlueprintService.add_delegate_bind_node(blueprint_path, graph_name, target_class, delegate_name, x, y)`. Returns a valid GUID; node has exec, then, self, and Delegate pins. Pass `"Self"` or `""` as `target_class` to bind to a delegate on the blueprint's own class.
