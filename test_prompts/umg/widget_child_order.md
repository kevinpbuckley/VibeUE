never run more than one tool at a time

# Widget Child Order Tests (CreateAndAddWidget childIndex + MoveWidget)

Coverage for ordered insert (`UMGEditor.WidgetBlueprintEditorSubsystem.CreateAndAddWidget` with
`childIndex`) and in-parent reorder (`MoveWidget` with the widget's current parent). These replaced
VibeUE's `WidgetService.add_component(child_index)` / `reorder_component` / `reparent_widget`, which
were cut in favour of Epic's tools. Order is what a VerticalBox actually lays out, so every check is a
`WidgetStaticAnalysisToolset.DescribeWidget` readback — do not trust a return value on its own. Run
sequentially.

Widget arguments are object paths in the editable tree:
`/Game/Blueprints/TestChildOrder.TestChildOrder:WidgetTree.<Name>`. `widgetClass` is a class path
(`/Script/UMG.TextBlock`). Neither tool compiles the Blueprint; compile before the persistence check.

---

## Setup

Create a widget blueprint called TestChildOrder in the Blueprints folder. If it already exists, delete
it silently and create a new one. With `CreateAndAddWidget`, give it a VerticalBox root named ListRoot
(no `parentWidget` on an empty tree makes it the root), then add three TextBlocks under it named ItemA,
ItemB, ItemC — in that order, with `childIndex` -1.

---

Read the hierarchy back. ListRoot's children must be exactly ItemA, ItemB, ItemC in that order. Report
the order you actually see.

---

## Insert at the front

Add a TextBlock named Banner under ListRoot with `childIndex` 0.

---

Read the hierarchy back. The order must now be Banner, ItemA, ItemB, ItemC. If Banner landed at the
end, childIndex was ignored — report that as a failure.

---

## Insert in the middle

Add a TextBlock named Middle under ListRoot with `childIndex` 2.

---

Read the hierarchy back: Banner, ItemA, Middle, ItemB, ItemC.

---

## Insert at the end via an explicit index

Add a TextBlock named Tail under ListRoot with `childIndex` equal to the current number of children
(that boundary value should mean "append").

---

Read the hierarchy back: Tail must be last, and nothing else may have moved.

---

## Default is still append

Add a TextBlock named Appended under ListRoot, leaving `childIndex` at its default (-1).

---

Read the hierarchy back. Appended must be last.

---

## Out-of-range insert

Try to add a TextBlock named TooFar under ListRoot with `childIndex` 99.

---

Report exactly what Epic's tool does (error, clamp to append, or something else). It must not corrupt
the tree. Read the hierarchy back and report whether TooFar exists and where.

---

Try to add a TextBlock named Negative under ListRoot with `childIndex` -5. Same: report the observed
behaviour and read the hierarchy back. (Only -1 is documented as append.)

---

## Omitted parent means "root", but only on an empty tree

Add a TextBlock named IntoRoot with no `parentWidget` while the tree already has a root.

---

Report what happens. The documented contract is that a null parent makes the widget the root only
when the tree is empty; do not assume it falls back to the existing root panel (VibeUE's removed
`add_component` did). Read the hierarchy back and confirm there is still exactly one root, ListRoot.

---

## Reorder an existing widget

Move ItemC to the front with `MoveWidget(newParent=<ListRoot>, childIndex=0)`.

---

Read the hierarchy back and confirm ItemC is first and every other widget kept its relative order.

---

Move ItemC back to the last position. Because `MoveWidget` uses drop-position semantics, moving
forward within the same parent to final position N needs `childIndex` N+1, or -1 to append. Use -1.

---

Read the hierarchy back and confirm it is last.

---

## Reorder edge cases

Move a widget to the position it already occupies. This should succeed and change nothing (mind the
N+1 rule for forward moves).

---

Try `MoveWidget` on a widget name that does not exist. It must return an error, not crash.

---

Try `MoveWidget` on ListRoot itself (the root, which has no panel parent). It must be rejected, not
crash, and the hierarchy must be unchanged.

---

## Reparent

Add a HorizontalBox named SideBox under ListRoot, and a TextBlock named Nested under SideBox. Now call
`MoveWidget` on Nested with `newParent` ListRoot and `childIndex` 0.

---

Read the hierarchy back. Nested must now be ListRoot's first child and SideBox must be empty. Then
try `MoveWidget` on SideBox with `newParent` Nested (a non-panel) and with `newParent` SideBox itself;
both must be rejected and leave the tree unchanged.

---

## Rename and remove

Rename Banner to BannerRenamed with `RenameWidget`. Read the hierarchy back and confirm the new name
and its position. Then `RemoveWidget` SideBox and confirm SideBox is gone along with any children.

---

## Persistence

Compile and save the widget blueprint, reload it, then read the hierarchy one more time. The child
order must survive the round trip.

---

## Cleanup

Delete the TestChildOrder widget blueprint.
