# manage_umg_widget Test Prompts

## Setup
Create Blueprint "WBP_TestWidget" with parent "UserWidget"

## Test 1: Creation
Create WBP_TestWidget
List components
Verify root

## Test 2: Type Discovery
Search for Button types category="Common"
Search for panels
Search for TextBlock

## Test 3: Add Components
Add Button "PlayButton"
Add TextBlock "TitleText"
Add Image "Background"
Add VerticalBox "MenuContainer"
List

## Test 4: Widget Properties
Get properties of TitleText
Set Text to "Main Menu"
Set ColorAndOpacity to [1.0, 1.0, 1.0, 1.0]
Set font size
Verify

## Test 5: Slot Properties
Set Background Slot.HorizontalAlignment to "HAlign_Fill"
Set Slot.VerticalAlignment to "VAlign_Fill"
Set PlayButton slot center
Verify

## Test 6: Event Binding
Bind OnClicked for PlayButton
Bind OnHovered
Verify

## Test 7: Component Removal
Remove TitleText
List to verify

## Test 8: Complex Widget
Add nested components
Set multiple properties
Configure layout

## Cleanup
Delete /Game/Blueprints/WBP_TestWidget with force_delete=True and show_confirmation=False
