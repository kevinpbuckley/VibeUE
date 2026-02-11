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

