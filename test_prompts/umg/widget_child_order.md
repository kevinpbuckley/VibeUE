never run more than one tool at a time

# Widget Child Order Tests (add_component child_index + reorder_component)

Coverage for ordered insert (`add_component(..., child_index=N)`) and `reorder_component`, added in
PR #526. Order is what a VerticalBox actually lays out, so every check is a hierarchy readback — do
not trust a `True` return on its own. Run sequentially.

---

## Setup

Create a widget blueprint called TestChildOrder in the Blueprints folder. If it already exists, delete
it silently and create a new one. Give it a VerticalBox root named ListRoot, then add three TextBlocks
under it named ItemA, ItemB, ItemC — in that order, with no child_index.

---

Read the hierarchy back. ListRoot's children must be exactly ItemA, ItemB, ItemC in that order. Report
the order you actually see.

---

## Insert at the front

Add a TextBlock named Banner under ListRoot at child_index 0.

---

Read the hierarchy back. The order must now be Banner, ItemA, ItemB, ItemC. If Banner landed at the
end, child_index was ignored — report that as a failure.

---

## Insert in the middle

Add a TextBlock named Middle under ListRoot at child_index 2.

---

Read the hierarchy back: Banner, ItemA, Middle, ItemB, ItemC.

---

## Insert at the end via an explicit index

Add a TextBlock named Tail under ListRoot at a child_index equal to the current number of children
(that boundary value is legal and means "append").

---

Read the hierarchy back: Tail must be last, and nothing else may have moved.

---

## Default is still append

Add a TextBlock named Appended under ListRoot with no child_index argument at all.

---

Read the hierarchy back. Appended must be last — the default behaviour must not have changed.

---

## Out-of-range insert must fail loudly

Try to add a TextBlock named TooFar under ListRoot at child_index 99.

---

This must fail with an error message naming the valid range — it must NOT silently append. Confirm
TooFar does not exist in the hierarchy afterwards.

---

Try to add a TextBlock named Negative under ListRoot at child_index -5. Same expectation: an explicit
error, and no widget created. (Only -1 means append.)

---

## Empty parent name means "the root panel", not "become the root"

Add a TextBlock named IntoRoot with an empty parent name and child_index 0.

---

This must SUCCEED and land at index 0 of ListRoot — an empty parent name resolves to the existing root
panel. It must not become a second root.

---

## child_index with genuinely no panel parent

Create a second, completely empty widget blueprint (no root widget at all), then try to add a TextBlock
to it with child_index 0. This must fail with an error explaining there is no panel parent to insert
into, and must not create the widget. Delete that blueprint afterwards.

---

## Reorder an existing widget

Move ItemC to index 0 with reorder_component.

---

Read the hierarchy back and confirm ItemC is first and every other widget kept its relative order.

---

Move ItemC back to the last index.

---

Read the hierarchy back and confirm it is last.

---

## Reorder edge cases

Reorder a widget to the index it already occupies. This should succeed and change nothing.

---

Try reorder_component with an index equal to the child count (one past the end). It must return False,
and the hierarchy must be unchanged.

---

Try reorder_component with index -1. It must return False — unlike add_component, -1 is not special
here.

---

Try reorder_component on a widget name that does not exist. It must return False, not crash.

---

Try reorder_component on ListRoot itself (the root widget, which has no panel parent). It must return
False, not crash, and the hierarchy must be unchanged.

---

## Reorder does not reparent

Add a HorizontalBox named SideBox under ListRoot, and a TextBlock named Nested under SideBox. Now call
reorder_component on Nested with index 0.

---

Read the hierarchy back. Nested must still be a child of SideBox — reorder_component only moves a
widget within its current parent. Moving between parents is reparent_widget's job.

---

## Persistence

Save and reload the widget blueprint, then read the hierarchy one more time. The child order must
survive the round trip.

---

## Cleanup

Delete the TestChildOrder widget blueprint.
