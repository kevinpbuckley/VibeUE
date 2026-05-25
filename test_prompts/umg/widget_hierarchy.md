# Widget Hierarchy Tests (get_hierarchy crash regression)

Regression coverage for the `get_hierarchy` use-after-free that crashed the Python interpreter
(access violation 0xC0000005) and left the session unrecoverable. Run sequentially.

---

## Read a widget's hierarchy

Show me the full widget hierarchy of one of the HUD widgets (e.g. WBP_FastTravel or WBP_SlashOverlay).
List each widget with its class and its parent.

---

Confirm the result is in depth-first order: the root widget first, then its children nested under it.

---

## Stress / regression

Read the hierarchy of several different widget blueprints in a row — pick the most deeply nested ones
in the project. Do this for at least 8 widgets.

---

Now read the same nested widget's hierarchy 20 times in a loop. It must complete every time without
crashing the editor or the Python interpreter.

---

## Session is still alive

After all those hierarchy reads, run a trivial Python command (e.g. print the engine version) to
confirm the Python interpreter is still responsive — a crash here would mean the session died.

---

## Empty / edge widget

Read the hierarchy of a widget that has no root widget or is otherwise empty. It should return an
empty result gracefully (no crash), not an error.

---
