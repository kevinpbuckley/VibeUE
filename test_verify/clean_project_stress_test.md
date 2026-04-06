# VibeUE — Clean Project Stress Test

**Purpose:** Validate the plugin in a brand new UE 5.6 project before pushing to production. Simulates the exact scenario that caused the last user-reported failures.

**When to run:** After any fix that touches C++ before a production push.

**Branch to test:** `feat/metasound-service` (or whatever is about to be pushed)

**All test assets go in `/Game/StressTest/`** — delete the folder when done.

---

## Test Modes

Every step in this document must be run **twice** — once via Claude Code (MCP), once via the in-editor AI Chat window. Both paths hit the same tools but through different surfaces and different conversation state. A failure in one that doesn't appear in the other narrows the root cause immediately.

| Mode | How to run |
|---|---|
| **Claude Code (MCP)** | Paste prompts into Claude Code CLI / IDE extension |
| **In-editor AI Chat** | Open Window → VibeUE AI Chat (or Ctrl+Shift+V), paste same prompt |

Mark results separately: e.g. `Step 1a — MCP: ✅ / In-editor: ✅`

---

## Pre-Flight Checklist

Before running any prompts, confirm:

- [ ] Fresh UE 5.6 project — no prior VibeUE content
- [ ] Plugin copied in and project compiled cleanly (0 errors)
- [ ] VibeUE proxy running on port 8089
- [ ] AI Chat window opens (Window → VibeUE AI Chat, or Ctrl+Shift+V)

If the project didn't compile, stop here — fix the build first.

---

## Step 0 — Connection Smoke Test

Paste this first. If it fails, nothing else matters.

```
Print the current Unreal Engine version, list all editor subsystems, and read the last 20 lines of the main log.
```

**Expected:** Engine version string (5.6.x), subsystem list, recent log lines. No errors.

---

## Step 1 — Blueprint Core

### 1a. Actor lifecycle

```
In /Game/StressTest:
1. Create Actor Blueprint "BP_StressActor"
2. Add float variable "Health" with default 100
3. Add bool variable "bIsAlive" with default true
4. Add int variable "Score" with default 0
5. Add a SpotLight component named "ActorLight"
6. Set ActorLight Intensity to 3000 and LightColor to orange [1.0, 0.4, 0.0, 1.0]
7. Compile the Blueprint and report its state
```

**Expected:** Compiles successfully. Variables and component visible.

---

### 1b. Function with nodes

```
In Blueprint /Game/StressTest/BP_StressActor:
1. Create function "CalculateDamage" with input float "BaseDamage" and output float "FinalDamage"
2. In the function graph:
   - Add a RandomFloatInRange node with Min=0.8, Max=1.2
   - Add a Multiply node
   - Connect function entry exec to RandomFloatInRange
   - Connect BaseDamage to Multiply input A
   - Connect RandomFloatInRange ReturnValue to Multiply input B
   - Connect Multiply output to FinalDamage
3. Compile and verify
```

**Expected:** Function exists, compiles. Regression check: node GUIDs are non-null (Issue #1).

---

### 1c. Event graph

```
In Blueprint /Game/StressTest/BP_StressActor:
1. In the EventGraph, add a Branch node connected to the BeginPlay event
2. Connect the bIsAlive variable GET to the Branch condition
3. Add a PrintString node on the True pin with message "Actor is alive"
4. Add a PrintString node on the False pin with message "Actor is dead"
5. Compile
```

**Expected:** Compiles. Regression check: Branch True/False aliases work (Issue #8), BeginPlay K2Node_Event connects correctly (Issue #5).

---

### 1d. Object reference variable (known open issue)

```
In Blueprint /Game/StressTest/BP_StressActor, add an object reference variable "TargetActor" of type Actor.
```

**Expected:** Returns false or logs a known failure. Do NOT mark as a bug — Issue #6 is still open. Record the actual error message in the results.

---

### 1e. Character blueprint + component + reparent

```
In /Game/StressTest:
1. Create Blueprint "BP_StressCharacter" with parent Character
2. Add a CapsuleComponent named "HitBox"
3. Reparent BP_StressCharacter to Pawn
4. Compile
```

**Expected:** Compiles after reparent.

---

## Step 2 — Asset Management

```
1. Search for all Blueprints in /Game/StressTest
2. Duplicate BP_StressActor to /Game/StressTest as BP_StressActor_Copy
3. Move BP_StressActor_Copy to /Game/StressTest/Sub as BP_StressActor_Moved
4. List all assets in /Game/StressTest
5. Delete BP_StressActor_Moved (force)
```

**Expected:** Each step returns success. Deleted asset gone from final list.

---

## Step 3 — UMG Widget

### 3a. Widget hierarchy + compile

```
In /Game/StressTest:
1. Create Widget Blueprint "WBP_StressMenu" with parent UserWidget
2. Add Image component "Background", set color to [0.05, 0.05, 0.05, 1.0]
3. Add VerticalBox "Layout" to root
4. Add Button "PlayBtn" to Layout
5. Add TextBlock "PlayLabel" to PlayBtn with text "PLAY", color white, font size 28
6. Add Button "QuitBtn" to Layout
7. Add TextBlock "QuitLabel" to QuitBtn with text "QUIT", color white, font size 28
8. Compile
```

**Expected:** Compiles. Hierarchy intact.

---

### 3b. Widget v2 APIs

```
In Widget /Game/StressTest/WBP_StressMenu:
1. Set font on PlayLabel: family "Roboto", size 32, bold
2. Get the font back from PlayLabel and confirm size is 32
3. Set brush on Background to use a solid color [0.1, 0.1, 0.2, 1.0]
4. Rename PlayBtn to "StartButton"
5. Bind OnClicked event on StartButton to a function named "HandlePlay"
6. List all functions in WBP_StressMenu
```

**Expected:** All six operations return success. HandlePlay appears in function list.

---

### 3c. MVVM ViewModel

```
In /Game/StressTest:
1. Create MVVM ViewModel "VM_StressData"
2. Add a float field "PlayerHP" to the viewmodel
3. In WBP_StressMenu, bind PlayLabel text to VM_StressData PlayerHP
```

**Expected:** ViewModel created, binding established.

---

## Step 4 — Materials

```
In /Game/StressTest:
1. Create Material "M_StressTest" with BlendMode Opaque and ShadingModel DefaultLit
2. Add scalar parameter "Roughness" default 0.5
3. Add vector parameter "BaseColor" default [0.8, 0.2, 0.2, 1.0]
4. Add a Multiply node, connect BaseColor and Roughness into it, connect output to Base Color
5. Save the material
```

**Expected:** Material created and saved. Parameters visible.

---

## Step 5 — Landscape + Layer Operations

This tier specifically covers the fix from 2026-03-28 (`GetLayerName()` removal in UE 5.6).

### 5a. Create and query

```
1. Create a landscape in the current level named "StressLandscape" at location [0,0,0]
2. Get landscape info for StressLandscape
3. List all landscapes in the level
```

**Expected:** Landscape created. `list_landscapes` returns `actor_label` field (not `landscape_name`).

---

### 5b. Sculpt

```
1. Sculpt StressLandscape at location [25000, 25000] with brush_radius 2000 and strength 0.6
2. Smooth at location [25500, 25500] with brush_radius 1500 and strength 0.3
3. Create a mountain at StressLandscape center [25000, 25000] with radius 3000 and height 800
```

**Expected:** All three operations return success.

---

### 5c. Layers (null LayerInfoObj path — regression for today's fix)

```
1. List layers on StressLandscape
2. Attempt to add a layer named "Grass" to StressLandscape
```

**Expected:** `list_layers` returns empty (no material assigned — expected). `add_layer` returns false or logs that no landscape material is set. The key check: **no crash**, no assertion failure. These are the callsites fixed today.

---

### 5d. Splines

```
1. Add a spline point to StressLandscape at world location [0, 0, 200]
2. Add a second spline point at [2000, 0, 200]
3. Connect the two spline points
4. Get spline info for StressLandscape
```

**Expected:** 2 control points, 1 segment. `get_spline_info` returns structure.

---

## Step 6 — Foliage

```
In /Game/StressTest:
1. Create foliage type "FT_StressTree" using mesh /Engine/BasicShapes/Cone with save path /Game/StressTest
2. Scatter FT_StressTree on StressLandscape: center [0,0], radius 5000, count 30
3. Get instance count for FT_StressTree
4. Get foliage in radius 2000 around [0, 0]
5. Remove foliage in radius 1000 around [0, 0]
6. Get instance count again to confirm removal
7. Clear all foliage
8. Confirm final count is 0
```

**Expected:** Scatter places roughly 25–30 instances. Remove and clear work. Final count 0.

---

## Step 7 — State Tree

```
In /Game/StressTest:
1. Create StateTree asset at /Game/StressTest/ST_StressEnemy
2. Add state "Patrol" (parent: "")
3. Add state "Attack" (parent: "")
4. Add child state "ChaseTarget" under Patrol
5. Add transition from Patrol: on event "EnemySpotted" → go to Attack
6. Add transition from Attack: on completion → go to Patrol
7. Compile the StateTree
```

**Expected:** All states added. Transitions set. Compiles. Remember: top-level parent is `""` not `"Root"`. The `EnemySpotted` gameplay tag is auto-registered by the service — no pre-step needed.

---

## Step 8 — Data Assets

### 8a. Enum + Struct

```
In /Game/StressTest:
1. Create enum "EStressState" with values: Idle, Patrol, Attack, Dead
2. Create struct "FStressEnemyData" with fields: Name (string), HP (float), State (EStressState)
```

**Expected:** Both created. Note: `create_enum` takes `(directory_path, enum_name)`.

---

### 8b. Data Table

```
In /Game/StressTest:
1. Create data table "DT_StressEnemies" with row struct TableRowBase, asset name DT_StressEnemies, save to /Game/StressTest
2. Add row "Goblin" with fields: Name="Goblin", HP=50
3. Add row "Troll" with fields: Name="Troll", HP=200
4. Get row "Goblin"
5. Update row "Goblin": HP=60
6. List all rows
7. Remove row "Troll"
8. List rows again to confirm removal
```

**Expected:** CRUD all succeeds. Note: `create_data_table` arg order is `(row_struct, asset_path, asset_name)`.

---

## Step 9 — Enhanced Input

```
In /Game/StressTest:
1. Create Input Action "IA_StressJump" with value type Boolean, save to /Game/StressTest
2. Create Input Action "IA_StressMove" with value type Axis2D, save to /Game/StressTest
3. Create Input Mapping Context "IMC_StressDefault", save to /Game/StressTest
4. Add key mapping: SpaceBar → IA_StressJump to IMC_StressDefault
5. Add key mapping: W → IA_StressMove to IMC_StressDefault
6. Add Held trigger to the SpaceBar mapping (mapping index 0)
7. Get mappings from IMC_StressDefault
8. List all input actions in the project
```

**Expected:** 2 actions, 1 context, 2 mappings. Held trigger on jump. Note: `list_input_actions` takes no arguments.

---

## Step 10 — Niagara

```
In /Game/StressTest:
1. Create Niagara system "NS_StressFX", save to /Game/StressTest
2. List available emitter templates
3. Add a sprite emitter to NS_StressFX
4. List emitters in NS_StressFX
5. List parameters on NS_StressFX
6. Compile NS_StressFX
```

**Expected:** System created, emitter added, compiles. Note: `create_system` arg order is `(name, path)`.

---

## Step 11 — Sound Cues

```
In /Game/StressTest:
1. Load the sound-cues skill
2. Create Sound Cue at /Game/StressTest/SC_StressTest
3. Add a Wave Player node
4. Add a Modulator node
5. Add a Random node
6. Connect Wave Player (child) → Random (parent), input slot 0
7. Set root node to the Random node
8. Set volume multiplier to 0.8
9. Save the cue
10. Get cue info
```

**Expected:** All operations succeed. Note: path includes asset name (`/Game/StressTest/SC_StressTest`).

---

## Step 12 — MetaSound

```
In /Game/StressTest:
1. Load the metasounds skill
2. Create MetaSound Source at package_path=/Game/StressTest, asset_name=MS_StressTone, output_format=Mono
3. Add graph input "Frequency" of type float, default 440.0
4. Add graph output "MonoOutput" of type Audio (Mono)
5. List all nodes in MS_StressTone
6. Connect the Frequency input to a Sine node, connect Sine output to MonoOutput
7. Set default value for Frequency input to 880.0
8. Build the MetaSound
9. Verify node list shows correct structure
```

**Expected:** Full chain. Pin auto-strip of `:Audio` suffix works. Note: must instantiate `ms = unreal.MetaSoundService()`.

---

## Step 13 — Gameplay Tags

```
1. Add gameplay tag "StressTest.Enemy.Patrol"
2. Add gameplay tag "StressTest.Enemy.Attack"
3. Add gameplay tag "StressTest.Player.Base"
4. List tags with prefix "StressTest"
5. Check has_tag for "StressTest.Enemy.Patrol"
6. Get children of "StressTest.Enemy"
7. Get tag info for "StressTest.Player.Base"
```

**Expected:** All 7 operations succeed. Hierarchy correct.

---

## Step 14 — Terrain Data + Deep Research

```
1. Geocode "Yosemite Valley, California" using deep_research
2. Use the returned coordinates to preview elevation data with terrain_data (radius 15km)
3. Fetch water features for the same area
4. Do a web search for "UE 5.6 landscape layer blend documentation"
```

**Expected:** Geocode returns lat/lng. Elevation returns min/max heights and suggested scales. Water features return rivers/lakes or empty (both fine). Search returns result.

---

## Step 15 — Logs & Diagnostics

```
1. Read the main log, filter for any "Error" entries
2. Read the chat log (alias: chat), last 30 lines
3. Search the main log for "StressTest" to confirm our asset operations were logged
```

**Expected:** Logs readable. Alias resolution works. Chat log shows today's tool calls.

---

## Regression Markers Summary

Run these inline during the steps above, but double-check each explicitly at the end:

| Check | Where | Pass Condition |
|---|---|---|
| Node GUID non-null after `add_function_call_node` | Step 1b | GUID string returned, not null |
| Branch True/False aliases (not then/else) | Step 1c | Pins connected without error |
| BeginPlay K2Node_Event connectable | Step 1c | No "default pins" error |
| Variable type names correct | Step 1a | float/bool/int reported correctly |
| Object ref variable → expected failure | Step 1d | Returns false, no crash |
| CDO `get_default_object` read-only succeeds | Any BP | No error on read |
| MetaSound pin `:DataType` suffix auto-stripped | Step 12 | Connect succeeds |
| Landscape layer list with no material → no crash | Step 5c | Returns empty, no assertion |
| `GetLayerName` null path → no crash | Step 5c | Clean false/empty return |

---

## Cleanup

```
Delete all assets in /Game/StressTest using manage_asset force delete.
Also delete the StressLandscape from the level.
```

Verify `/Game/StressTest/` no longer appears in the Content Browser.

---

## Results Template

Fill in during the run:

```
Date:
Branch:
UE version:
Clean project: yes/no
Build result: (errors / succeeded)

                              MCP        In-editor
Step 0  — Connection:         [ ]        [ ]
Step 1a — Actor vars/comps:   [ ]        [ ]
Step 1b — Function nodes:     [ ]        [ ]
Step 1c — Event graph/Branch: [ ]        [ ]
Step 1d — Object ref (fail):  [ ]        [ ]
Step 1e — Reparent:           [ ]        [ ]
Step 2  — Asset management:   [ ]        [ ]
Step 3a — Widget hierarchy:   [ ]        [ ]
Step 3b — Widget v2 APIs:     [ ]        [ ]
Step 3c — MVVM:               [ ]        [ ]
Step 4  — Materials:          [ ]        [ ]
Step 5a — Landscape create:   [ ]        [ ]
Step 5b — Sculpt:             [ ]        [ ]
Step 5c — Layer null path:    [ ]        [ ]
Step 5d — Splines:            [ ]        [ ]
Step 6  — Foliage:            [ ]        [ ]
Step 7  — State Tree:         [ ]        [ ]
Step 8a — Enum/Struct:        [ ]        [ ]
Step 8b — DataTable:          [ ]        [ ]
Step 9  — Enhanced Input:     [ ]        [ ]
Step 10 — Niagara:            [ ]        [ ]
Step 11 — Sound Cues:         [ ]        [ ]
Step 12 — MetaSound:          [ ]        [ ]
Step 13 — Gameplay Tags:      [ ]        [ ]
Step 14 — Terrain/Research:   [ ]        [ ]
Step 15 — Logs:               [ ]        [ ]

Regressions: all clear / [list any failures]

Overall: PASS / FAIL
Notes:
```
