# Smoke Test Prompts

## Setup
Create Blueprint "BP_QuickTest" with parent "Actor" in /Game/MCP_Test
Create Widget Blueprint "WBP_QuickTest" with parent "UserWidget" in /Game/MCP_Test

## Test 1: Basic Blueprint Tools
Create variable "Health" with type float in Blueprint "/Game/MCP_Test/BP_QuickTest"
Create function "Calculate" in Blueprint "/Game/MCP_Test/BP_QuickTest"
Add component "SpotLight" to Blueprint "/Game/MCP_Test/BP_QuickTest"

## Test 2: Function with Nodes
In Blueprint "/Game/MCP_Test/BP_QuickTest" create function "CalculateDamage" with input "BaseDamage" and output "FinalDamage"
Create "RandomFloatInRange" node and "Multiply" node
Connect nodes and compile

## Test 3: Complete Actor
Create Blueprint "BP_PlayerTest" with parent "Character" in /Game/MCP_Test
Add variables "MaxHealth" and "CurrentHealth" type float
Add component "SpotLight" named "PlayerLight" with Intensity 5000
Create function "TakeDamage" with damage calculation
Compile

## Test 4: UMG Widget
Create widget Blueprint "WBP_MenuTest" with parent "UserWidget" in /Game/MCP_Test
Add Image "Background" with dark gray color
Add VerticalBox "MenuContainer"
Add Button "PlayButton" with TextBlock "PlayText"
Add Button "QuitButton" with TextBlock "QuitText"

## Test 5: Utility
Search for widgets in /Game/MCP_Test
Check Unreal connection

## Cleanup
Delete /Game/MCP_Test/BP_QuickTest with force_delete=True and show_confirmation=False
Delete /Game/MCP_Test/WBP_QuickTest with force_delete=True and show_confirmation=False
Delete /Game/MCP_Test/BP_PlayerTest with force_delete=True and show_confirmation=False
Delete /Game/MCP_Test/WBP_MenuTest with force_delete=True and show_confirmation=False
