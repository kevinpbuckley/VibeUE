# VibeUE MCP Tools - Quick Start Validation

## Setup: Create Test Folder

**Start by creating test assets in a dedicated folder:**

```
Create Blueprint "BP_QuickTest" with parent "Actor" in /Game/MCP_Test
Create Widget Blueprint "WBP_QuickTest" with parent "UserWidget" in /Game/MCP_Test
```

## One-Liner Tests for Each Tool

Copy and paste these into AI chat to quickly verify tools work:

### Blueprint Tools

```
Create variable "Health" with type float in Blueprint "/Game/MCP_Test/BP_QuickTest"
```

```
Create function "Calculate" in Blueprint "/Game/MCP_Test/BP_QuickTest"
```

```
Add component "SpotLight" to Blueprint "/Game/MCP_Test/BP_QuickTest"
```

```
Create node in function "Calculate" in Blueprint "/Game/MCP_Test/BP_QuickTest"
```

### 🎯 End-to-End Function Creation

**Complete function with nodes in one prompt:**

```
In Blueprint "/Game/MCP_Test/BP_QuickTest":
1. Create variable "PlayerHealth" type float
2. Create function "CalculateDamage"
3. Add input parameter "BaseDamage" type float to the function
4. Add output parameter "FinalDamage" type float to the function
5. In the function graph:
   - Create a "RandomFloatInRange" node with Min=0.8 and Max=1.2
   - Create a "Multiply" node at position 600, 100
   - Connect the function entry "then" to RandomFloatInRange "execute"
   - Connect "BaseDamage" parameter to Multiply input A
   - Connect RandomFloatInRange "ReturnValue" to Multiply input B
   - Connect Multiply output to "FinalDamage" parameter
6. Compile the Blueprint
```

**Expected**: Complete working function that multiplies base damage by random multiplier (0.8-1.2)

### 🚀 Complete Blueprint with Components and Events

**Full Actor Blueprint in one prompt:**

```
Create a complete player actor in /Game/MCP_Test:
1. Create Blueprint "BP_PlayerTest" with parent "Character" in /Game/MCP_Test
2. Add variable "MaxHealth" type float, set default value to 100
3. Add variable "CurrentHealth" type float, set default value to 100
4. Add component "SpotLight" named "PlayerLight"
5. Set PlayerLight Intensity to 5000, LightColor to cyan [0.0, 1.0, 1.0, 1.0]
6. Create function "TakeDamage" with input parameter "DamageAmount" type float
7. In TakeDamage function:
   - Create "GET CurrentHealth" node at [100, 100]
   - Create "Subtract" node at [400, 100]
   - Create "SET CurrentHealth" node at [700, 100]
   - Connect function entry "then" to Subtract "execute"
   - Connect GET CurrentHealth output to Subtract A
   - Connect DamageAmount parameter to Subtract B
   - Connect Subtract output to SET CurrentHealth input
   - Connect SET CurrentHealth "then" to function exit
8. Compile the Blueprint
```

**Expected**: Complete player character with health system and lighting

### UMG Tools

```
Add Button "TestButton" to widget "/Game/MCP_Test/WBP_QuickTest", set its color to blue
```

### 🎯 End-to-End UMG Widget Creation

**Complete styled menu widget in one prompt:**

```
Create a main menu widget in /Game/MCP_Test:
1. Create widget Blueprint "WBP_MenuTest" with parent "UserWidget" in /Game/MCP_Test
2. Add Image component "Background" to root, set color to dark gray [0.08, 0.08, 0.08, 1.0]
3. Set Background slot properties: HorizontalAlignment to HAlign_Fill, VerticalAlignment to VAlign_Fill
4. Add VerticalBox "MenuContainer" to root
5. Add Button "PlayButton" to MenuContainer
6. Add TextBlock "PlayText" to PlayButton with text "PLAY GAME", color white [1.0, 1.0, 1.0, 1.0], font size 24
7. Add Button "QuitButton" to MenuContainer
8. Add TextBlock "QuitText" to QuitButton with text "QUIT", color white [1.0, 1.0, 1.0, 1.0], font size 24
```

**Expected**: Professional-looking menu with dark background and two styled buttons

### Asset Tools

```
Search for widgets in /Game/MCP_Test folder
```

### Utility Tools

```
Check Unreal connection
```

```
Get help for blueprint-workflow
```

---

## Cleanup: Delete Test Assets

**After testing, delete the test folder (manual deletion required until delete action is added):**

```
Delete all assets in /Game/MCP_Test folder:
- BP_QuickTest
- WBP_QuickTest  
- BP_PlayerTest
- WBP_MenuTest
```

**Note**: Asset deletion via MCP is not yet implemented. Manually delete from Content Browser or wait for Phase 6 asset deletion feature.

---

## If Any Test Fails

1. Check Unreal Engine is running
2. Verify VibeUE plugin loaded
3. Check MCP server logs
4. Use `check_unreal_connection` tool

---

**All tools validated!** ✅
