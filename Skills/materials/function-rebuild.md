---
name: materials/function-rebuild
description: Rebuilding a Material Function gives callers new pins; copied assets are not a valid before.
---

# Rebuilding a material function breaks its callers

Rebuilding a Material Function (deleting and re-adding its inputs and outputs) gives its call nodes **new pins**. Every
material that calls it then fails to compile with `Missing ... input` until its graph is rewired: after rebuilding a
function, recompile each caller (`MaterialToolset.Recompile`, it raises with the failing node) and reconnect the pins.
Prefer editing the function's existing inputs in place.

A copied `.uasset` of an old material is **not a valid "before"** for an A/B: the copy loses those links too, so it
renders as broken or default, not as it used to look. Keep a "before" as a capture, or as a duplicate made inside the
editor while the function was unchanged.
