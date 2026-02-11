never run more than one tool at a time

# UMG Widget Tests

Tests for creating and configuring UI widgets. Run sequentially.

---

## Setup

Create a widget blueprint called TestWidget in the Blueprints folder. if it already exsits delete it silently and create a new one.

---

## Initial State

What's in the widget right now?

---

Is there a root element?

---

## Finding Widget Types

What button types are available in the common category?

---

What panel containers can I use?

---

Show me the text block options.

---

## Adding UI Elements

Add a play button.

---

Add a title text element.

---

Add a background image.

---

Add a vertical box container for menu layout.

---

Show me all the widgets now.

---

Save the widget blueprint.

---

Open the widget blueprint in the editor to verify it compiles without errors.

---

## Text Configuration

What properties does the title text have?

---

Set the text to "Main Menu".

---

Make it white.

---

Bump up the font size.

---

Verify those changes.

---

## Layout Properties

The background should fill the whole screen horizontally.

---

And vertically too.

---

Center the play button.

---

Check those layout settings.

---

## Event Bindings

When the play button is clicked, it should call a function. Set that up.

---

Add a hover event too.

---

Verify the events are bound.

---

## Modifying Widgets

Remove the title text.

---

Show what's left in the widget.

---

## Building a Menu

Add some more buttons and text for a proper menu layout.

---

Configure the layout to look like a vertical menu.

---

## Complex Hierarchy Test

Create a nested widget structure to test GUID registration:
- Add a VerticalBox called "MainContainer"
- Inside MainContainer, add a HorizontalBox called "HeaderRow"
- Inside HeaderRow, add a TextBlock called "HeaderTitle"
- Add a ScrollBox called "ContentScroll" to MainContainer
- Inside ContentScroll, add a VerticalBox called "ItemList"

---

Save and verify the complex hierarchy compiles without GUID errors.

---

## Widget Property Validation

Verify that HeaderTitle text is actually set to what we configured earlier.

---

Check the HeaderRow's padding and alignment properties.

---

## Deep Nesting Test

Inside ItemList, add 3 HorizontalBoxes called "Item1", "Item2", "Item3".

---

Inside each Item box, add a TextBlock called "ItemLabel" and a Button called "ItemButton".

---

Save the widget and verify all 6 nested children compile correctly.

---

Show me the complete hierarchy structure now.

---

## Widget Renaming Test

Rename "ItemButton" in Item1 to "Item1_ActionButton".

---

Verify the rename worked and the GUID is preserved.

---

## Widget Reparenting Test

Move the background image to be a child of MainContainer instead of root.

---

Verify the widget moved correctly and hierarchy updated.

---

## Slot Properties Test

Set the vertical alignment of HeaderRow in MainContainer to Top.

---

Set the horizontal alignment to Fill.

---

Verify those slot properties are set correctly.

---

## Multiple Widget Test

Create a second widget blueprint called "TestWidget2" in the Blueprints folder.

---

Add a Canvas Panel to TestWidget2 as the root.

---

Add a Button at position (100, 100) with size (200, 50).

---

Save TestWidget2 and verify it compiles.

---

Switch back to TestWidget and verify it still has all our previous widgets.

---

## Widget Visibility Test

Set the visibility of ContentScroll to Hidden.

---

Verify the visibility property is set correctly.

---

Set it back to Visible.

---

## Z-Order Test

In TestWidget, show me the Z-order of all root-level widgets.

---

Move the play button to be rendered above the vertical box container.

---

Verify the Z-order changed.

---

## Widget Deletion Test

Remove Item3 from ItemList.

---

Verify Item3 is gone but Item1 and Item2 remain.

---

Remove all children from HeaderRow.

---

Verify HeaderRow is now empty.

---

## Event Handler Verification

Show me all the event bindings we've set up.

---

Verify that the play button's OnClicked event is still bound.

---

## Style Properties Test

Set the HeaderTitle font size to 24.

---

Set the font style to bold.

---

Set the text color to yellow.

---

Verify all three style changes are applied.

---

## Anchor Test

In TestWidget2, set the button's anchors to stretch both horizontally and vertically.

---

Verify the anchor settings are correct.

---

## Cleanup Test

Delete TestWidget2 completely.

---

Verify TestWidget2 no longer exists.

---

Save TestWidget one final time.

---

Open TestWidget in the editor and verify everything compiles without errors.

---

## Final Validation

Count the total number of widgets in TestWidget's hierarchy.

---

List all widgets by name to confirm the complete structure.

---

