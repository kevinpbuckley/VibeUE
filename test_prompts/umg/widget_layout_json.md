never run more than one tool at a time

# UMG Layout JSON & Verified Writes

Tests for build_layout, export_layout, set_property_verified and set_color. Run sequentially.

---

## Setup

Create a widget blueprint called WBP_LayoutTest in the Blueprints folder. If it already exists delete it silently and create a new one.

---

## Build in one call

Using a single build_layout call, give WBP_LayoutTest a CanvasPanel root with a VerticalBox at position 100, 200 that contains a Play button, an Options button and a Quit button, each with a text label at font size 28.

---

Do a dry run first: what would build_layout do if I added a second widget called PlayButton under the VerticalBox?

---

## Verified writes

Make the Play label red using the hex colour #E53935 and confirm the stored value.

---

Set the Play button's slot padding using the value "(Lft=9)" with set_property_verified. Did it actually change anything?

---

## Round trip

Export the layout as JSON, swap the order of the Options and Quit buttons in the JSON, and rebuild the widget from it with replace mode.

---

Show me the hierarchy now and confirm the buttons are in the new order and the labels kept their colour and size.
