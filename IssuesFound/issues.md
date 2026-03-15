# VibeUE — Open Issues

_Last updated: 2026-03-15_

Fixed/closed issues are archived in [closedissues.md](closedissues.md).

---

## Local Bug Tracker

### 1. `create_node_by_key` returns null GUID for most FUNC nodes

**Severity:** High
**Method:** `BlueprintService.create_node_by_key`
**Spawner key format:** `FUNC ClassName::FunctionName`

Nodes are created in the graph but assigned a GUID of `00000000`. This means:
- They cannot be referenced for subsequent `connect_nodes` calls
- They cannot be deleted via `delete_node`
- They accumulate as permanent ghost nodes in the graph

**Workaround:** Use `add_function_call_node(bp, graph, class_name, function_name, x, y)` instead — this consistently returns valid GUIDs.

---

### 2. Ghost `00000000` nodes cannot be deleted

**Severity:** High
**Method:** `BlueprintService.delete_node`

Nodes created with a null GUID (see issue #1) cannot be removed via the API. `delete_node` returns `False`. They also cannot be repositioned via `set_node_position`.

**Workaround:** In the Blueprint editor, right-click the graph → **Select All Disconnected Nodes → Delete**.

---

### 3. `set_node_pin_value` silently fails on class reference pins

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

### 4. `configure_node` fails on class reference properties

**Severity:** Medium
**Method:** `BlueprintService.configure_node`

Returns `False` for class reference properties. Same root cause as issue #3 — class reference types are not handled.

---

### 6. `add_set_variable_node` / `add_get_variable_node` fail for object reference variables

**Severity:** Medium
**Method:** `BlueprintService.add_set_variable_node`, `BlueprintService.add_get_variable_node`
**Affected type:** Object reference variables (e.g. `ATurnManager*`)

Both methods return an empty string (failure) for variables of object reference type, even when the variable exists and `list_variables` confirms it.

**Note:** `add_get_variable_node` works correctly for **component** references (e.g. a `UStaticMeshComponent` added via `add_component`).

---

### 10. `check_unreal_connection` tool does not exist in C++ source

**Severity:** Medium (documentation/diagnostic)

The VibeUE `CLAUDE.md` references `check_unreal_connection` as a diagnostic tool. It is **not registered anywhere in the C++ codebase**. Calling it returns the proxy's generic "UE not running" fallback — it is not a real tool.

**Workaround:** Diagnose UE connectivity by attempting any Python tool call (e.g. `execute_python_code`).

---

### 11. CLAUDE.md documents 14 tools — current architecture has 9

**Severity:** Low (documentation staleness)

The upstream README states the plugin exposes "14 multi-action tools". The current implementation registers **9 tools** only. The manage_* tool layer was replaced by the Python-based approach.

---

### 14. `add_function_call_node` fails for `UUserWidget::GetOwningActor`

**Severity:** High
**Method:** `BlueprintService.add_function_call_node`, `BlueprintService.create_node_by_key`
**Context:** Widget Blueprint (WBP) graphs

`UUserWidget::GetOwningActor` is a `UFUNCTION(BlueprintCallable)` but cannot be placed via the API. All attempted forms return empty string / null GUID:

- `add_function_call_node("UserWidget", "GetOwningActor")`
- `add_function_call_node("Widget", "GetOwningActor")`
- `add_function_call_node("UUserWidget", "GetOwningActor")`
- `add_function_call_node("Actor", "GetOwningActor")`
- `create_node_by_key("FUNC UserWidget::GetOwningActor")`
- `discover_nodes("GetOwningActor")` / `discover_nodes("Owning Actor")` — no results

**Impact:** Cannot retrieve the owning actor from within a WBP graph via MCP — a very common widget pattern.

**Workaround:** Place the node manually in the Blueprint editor.

---

### 16. `discover_nodes` does not surface widget component functions (WBP context)

**Severity:** Medium
**Method:** `BlueprintService.discover_nodes`
**Context:** Widget Blueprint (WBP) graphs

Functions on widget components (e.g. `SetPercent` on a `ProgressBar`) are not returned by `discover_nodes` in WBP context, even though they are valid Blueprint-callable functions.

`add_function_call_node("ProgressBar", "SetPercent")` works correctly once the class name is known — but cannot be found via discovery first.

**Note:** The component name in the asset may differ from the class name (e.g. `ProgressBar_40` vs `ProgressBar`). `add_function_call_node` accepts the class name, not the instance name.

**Workaround:** Use `add_function_call_node(class_name, function_name)` directly if the class name is known.

---

## GitHub Issues Snapshot

_Cached 2026-03-15. Refresh by checking [kevinpbuckley/VibeUE/issues](https://github.com/kevinpbuckley/VibeUE/issues)._

| # | Title | Labels | Quick Win? |
|---|-------|--------|-----------|
| [#334](https://github.com/kevinpbuckley/VibeUE/issues/334) | Investigate feasibility of using mcp2cli instead of direct MCP | — | Yes — research only, no code |
| [#311](https://github.com/kevinpbuckley/VibeUE/issues/311) | Self-Hosted LLM / OpenAI-Compatible Endpoint + Tabbed Settings UI | enhancement | No — large, well-specced |
| [#310](https://github.com/kevinpbuckley/VibeUE/issues/310) | Sound Cue & MetaSound Service Support | enhancement | No — large scope |
| [#281](https://github.com/kevinpbuckley/VibeUE/issues/281) | C++ Agent Chat Framework with Tool Support | enhancement | No — major architecture |
| [#268](https://github.com/kevinpbuckley/VibeUE/issues/268) | InEditorChat — Streaming on VibeUE model | — | Yes — well-scoped C++ client change |
| [#267](https://github.com/kevinpbuckley/VibeUE/issues/267) | Auto-Router and Enhanced Model Selection | enhancement | No — needs API worker too |
| [#258](https://github.com/kevinpbuckley/VibeUE/issues/258) | Context Knowledge (internal vector indexing) | — | No — exploratory |
| [#257](https://github.com/kevinpbuckley/VibeUE/issues/257) | Domain Knowledge — External RAG | — | No — exploratory |
| [#254](https://github.com/kevinpbuckley/VibeUE/issues/254) | InEditorChat — Sub Agents | — | No — no spec yet |
| [#253](https://github.com/kevinpbuckley/VibeUE/issues/253) | InEditorChat — Tasks for multi-task AI | — | No — no spec yet |
