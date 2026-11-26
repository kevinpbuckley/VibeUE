# Testing VibeUE (5-8 line)

Two layers test this plugin. Neither replaces the other.

| Layer | What it is | How to run | What it can see |
|---|---|---|---|
| Automation suite | C++ `VibeUE.*` tests (257 on 2026-11-25), in `Source/VibeUE/Private/**/*Tests.cpp` | in the editor console `Automation RunTests VibeUE`, or headless (below) | everything callable in-process: services, the `execute_python_code` reply path (`UPythonTools::ExecutePythonCode`), the Python bindings of every service (tests run real Python through that path) |
| Live MCP checks | an agent or script driving a running editor over its MCP port | see "Live testing" | the MCP transport and tool schemas, PIE, real project maps, rendering, focus, anything that needs a second process or real time |

## Running the suite headless

Build the plugin into a package, copy the package into a blank 5.8 host project's `Plugins/VibeUE`, then:

```
UnrealEditor-Cmd.exe <Host>.uproject -ExecCmds="Automation RunTests VibeUE; Quit" ^
  -testexit="Automation Test Queue Empty" -unattended -nullrhi -nosound -nosplash ^
  -ModelContextProtocolPort=<a free port> -abslog=<log>
```

- Count the `Test Completed. Result={...}` lines in the log; every one must be `Success`. On 2026-11-25 the suite was
  257/257 under `-nullrhi` (252 before the #614 tests); with `-RenderOffScreen` one more capture test runs.
- Give the host its own MCP port: a second editor on port 8000 fights the GUI editor for it.
- The host's `Content` folder must be empty afterwards: tests make their fixtures under `/Game/__VibeUETest/` in
  memory and must never save them. A file left there is a test bug.
- A test that logs an unexpected `Error` fails, so a refusal path a test drives must declare it with
  `AddExpectedError` (match the message prefix the service logs, e.g. `CreateFunctionGraph:`).

## Mutation run (issue #614, 2026-11-25)

The suite always passes, and two bugs shipped while it was green (#610, #613). To find out whether it can fail at
all, ten known-fixed bugs (and plausible variants of them) were put back one at a time on a scratch copy of the
code, the plugin rebuilt, and the full suite run against each mutant. Each mutant is a change of one to three lines;
the patches are not kept in the repo.

| # | Mutant (fix it reverts) | Where | Old suite: tests that failed | With the new tests |
|---|---|---|---|---|
| M01 | #603: an empty or garbage expression id falls back to expression index 0 | `UMaterialNodeService::FindExpressionById` | `MaterialNodeService.ConnectExpressionToOutput` | same |
| M02 | #604: a free port at grace expiry decides `Silent` instead of logging the error | `FVibeUEMcpStatus::DecideAction` | `McpStatus.DecideAction` | same |
| M03 | #613: `GetResidentMapWorlds` always returns empty | `UPythonTools::GetResidentMapWorlds` | `Python.ResidentMapDetection`, `Python.GetResidentMapsIsCallable`, then the editor crashed in `Python.SafeLoadLevelRefuses`: told nothing was resident, `safe_load_level` really loaded the resident probe map, the failure the feature exists to prevent; 44 tests never ran | + `Python.ResidentMapsInEveryReply` |
| M04 | #608 as named in #614: `auto_save=false` no longer reports `opted_out` | `BeginPythonRun` | `Python.AutoSaveReport` | same |
| M05 | #607: `create_function_graph` accepts a name an existing graph already has | `UBlueprintService::CreateFunctionGraph` | **none (252/252)** | `BlueprintService.CreateFunctionGraphRefusals`, `BlueprintService.InterfaceAuthoredFromPython` |
| M06 | #610: the qualified key matches a bool by its raw name only (`Get Can Be Damaged` misses) | `PropertyMatchesSearchName` | **none (252/252)** | `BlueprintService.CreateNodeByKeyQualifiedKey` |
| M07 | #609: no fallback for a variable the action database has no spawner for yet | `UBlueprintService::CreateNodeByKey` | **none (252/252)** | `BlueprintService.CreateNodeByKeyBindsFreshVariable` |
| M08 | #613's first implementation: the reply of a run that RAISED carries no `resident_maps` | `FinishPythonRun` | **none (252/252)** | `Python.ResidentMapsInEveryReply` |
| M09 | #608 as fixed: an ordinary Python exception latches "previous run crashed" | `FinishPythonRun` | `Python.ExceptionIsNotACrash` | same |
| M10 | #607: `create_function_graph` creates a graph that shadows a parent function | `UBlueprintService::CreateFunctionGraph` | **none (252/252)** | `BlueprintService.CreateFunctionGraphRefusals` |

Real code with the new tests: 257/257 (`-nullrhi`). Every mutant is now caught by at least one test.

What the run showed:

- **Five of ten mutants passed the whole suite**, among them exactly the two escapes #614 describes: M06 is the
  #610 bool lookup, M08 is #613's success-path-only reply. The suite was not vacuous (it caught M01-M04 and M09, each by
  the test written for that fix), but every survivor sat on a path that had only been checked live: `create_function_graph`
  had no test at all, and nothing read `resident_maps` out of a reply or drove `create_node_by_key` with a qualified or
  fresh-variable key.
- **Testing the helper is not testing the reply.** `ResidentMapDetection` called `GetResidentMapWorlds` directly and was
  right about it, while the field the agent actually reads was missing on the error path. The new test reads the field from
  real success and failure replies.
- **An id coming back is not the node being right.** The new key tests check the node's class, its bound member and whether
  it is self or external, so a fix that "returns something" for the wrong property still fails.
- **Two timing tests are load-sensitive** and failed in runs unrelated to their code while other lanes were compiling on the
  same machine: `Input.HoldInPIE` (2 of 22 runs: "throttling delegate is gone after the release grace", expected 1, got 2) and
  `TerrainData.CancelWakesWorker` (1 of 22: the cancel took 3.32 s). Both passed in every other run. They measure wall-clock
  time; read a failure of either on a loaded machine as noise until it repeats on an idle one.

The tests added for the survivors call the service the way an agent does and assert the result an agent relies on
(the node is bound to `bCanBeDamaged`, the failing reply lists the resident map, the Blueprint's graph list is
unchanged after a refusal), not only that a call returned something. Three of them also make the same call through
Python (`unreal.BlueprintService...`) inside the test, so the binding an agent uses is covered, not only the C++.

## What the suite covers, and what it does not

Covered well: pure decision functions (`McpStatus.DecideAction`, the performance verdicts, the EQS / Behavior Tree
layout and validation passes), the `execute_python_code` reply contract (`auto_save`, `auto_save_note`,
`dirtied_packages`, `resident_maps`, the crash latch), refusal paths of the asset, Blueprint, material, Behavior Tree,
Blackboard, EQS, StateTree and Mutable services, and round trips that build something and read it back.

Reflection-shape assertions (a `UFunction` exists, has `AICallable`, returns a struct rather than a bool) appear inside
about ten behavioural tests (for example `Assets.DeleteAssetUnattendedResultStruct`, `Animation.Capture.ErrorReturnsStruct`,
`MaterialNodeService.ConnectExpressionToOutput`, `Python.GetResidentMapsIsCallable`). They are kept: they fail when a tool
is unregistered or its signature regresses to a bool, which a behavioural assertion in C++ would not notice. They are
not behavioural coverage, and no test consists of them alone. The JSON-contract tests (`Performance.FrameTiming.JsonShape`,
`Bridge.SchemaTypes`, `Readiness.SignalPath`) are kept for the same reason: callers outside the plugin parse those fields
and paths, and a rename fails them.

No test was deleted: every test reviewed for #614 fails against at least one plausible defect in the code it names.

Known blind spots:

- **Fixtures are synthetic.** Resident-map detection uses a bare `UWorld` in a scratch package, not a real map: loading a
  project map pulls in World Partition and releasing it asserts. A real map loaded as an asset is `EWorldType::Inactive`
  like the fixture, but only a live editor proves the two agree.
- **No PIE.** PIE behaviour is tested through its refusals when PIE is not running and through pure validators; no headless
  test starts PIE (a queued automation run tears a PIE session down).
- **The action database is warm.** Blueprint node tests run after the editor has built its node-spawner database; a
  freshly started editor, or a database that has not seen a new variable yet, behaves differently. `CreateNodeByKeyBindsFreshVariable`
  forces the fallback path with a spelling no spawner can match, so it does not depend on that state.
- **No transport.** Tests call `UPythonTools::ExecutePythonCode`, the function behind the MCP tool, not the MCP endpoint;
  schema generation is covered by `Bridge.SchemaTypes` and registration by `Registry.RefreshKeepsTools`, but an actual
  `tools/call` over HTTP is not.

## Live testing

Both escapes behind #614 were caught by an agent driving a live editor over MCP, and that remains the only check of the
blind spots above. It is a manual step today. The recipe:

1. Start a host editor with MCP on its own port: `UnrealEditor-Cmd.exe <Host>.uproject -unattended -nosound
   -ModelContextProtocolPort=<port>` (add `-RenderOffScreen` for captures). Wait for `Starting MCP server on port` in its log.
2. Speak MCP over HTTP to `http://127.0.0.1:<port>/mcp`: `initialize`, keep the `Mcp-Session-Id` header, then `tools/call`
   VibeUE's `execute_python_code` with `{"code": "<python>"}` (stdlib `urllib` is enough).
3. Run the scenario and read the reply JSON, not only `success`: `resident_maps`, `dirtied_packages`, `auto_save_note`,
   the id a service call returned and what it points at.
4. Leave the host's `Content` empty and stop the editor you started.

`scripts/tests/*_regression.py` are such live scenarios for fixtures the suite cannot build (a World Partition landscape,
a Niagara scratch pad); each says in its docstring what it needs. When a live check finds a bug, add a suite test that
fails without the fix before closing it, and mutate the fix once to prove the test can fail.
